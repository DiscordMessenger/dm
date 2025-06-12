#include "NetworkerThread.hpp"
#include "WinUtils.hpp"
#include "WindowMessages.hpp"
#include "../discord/DiscordRequest.hpp"
#include "../discord/LocalSettings.hpp"
#include "../discord/Frontend.hpp"
#include "../discord/DiscordClientConfig.hpp"

#define CPPHTTPLIB_OPENSSL_SUPPORT

#ifndef __MINGW32__
#define __MINGW32__ // so that it doesn't use inet_pton
#endif

#define CPPHTTPLIB_NO_EXCEPTIONS
#include <httplib/httplib.h>

constexpr size_t REPORT_PROGRESS_EVERY_BYTES = 15360; // arbitrary

void LoadSystemCertsOnWindows(SSL_CTX* ctx)
{
	X509_STORE* store = X509_STORE_new();
	httplib::detail::load_system_certs_on_windows(store);
	SSL_CTX_set_cert_store(ctx, store);
}

extern HWND g_Hwnd;

static NetworkerThread::nmutex g_sslErrorMutex;
static bool g_bQuittingFromSSLError;

int g_latestSSLError = 0; // HACK - used by httplib.h, to debug some weird issue

bool AddExtraHeaders()
{
	return GetLocalSettings()->AddExtraHeaders();
}

int NetRequest::Priority() const
{
	int prio = 0;

	switch (type)
	{
		case QUIT:
			prio = 200;
			break;
		case PUT:
		case POST:
		case POST_JSON:
		case PATCH:
			prio = 100;
			break;
		case GET:
			prio =  90;
			break;
		default:
			assert(!"huh?");
	}

	switch (itype) {
		using namespace DiscordRequest;
		default:
			prio += 9;
			break;

		case IMAGE_ATTACHMENT:
		case MESSAGES:
		case GUILD:
			prio += 8;
			break;

		case IMAGE:
			prio += 1;
			break;
	}

	return prio;
}

bool NetworkerThread::ProcessResult(NetRequest& req, const httplib::Result& res)
{
	using namespace httplib;

	if (!res || res.error() == Error::SSLServerVerification)
	{
		bool isSSLError = res.error() == Error::SSLServerVerification;

		g_sslErrorMutex.lock();
		if (g_bQuittingFromSSLError) {
			// we're actually quitting. Ignore
			g_sslErrorMutex.unlock();
			return false;
		}

		std::string errorstr = to_string(res.error());

		const char* strarr[2];
		strarr[0] = req.url.c_str();
		strarr[1] = errorstr.c_str();
		int result = (int) SendMessage(g_Hwnd, WM_HTTPERROR, (WPARAM) isSSLError, (LPARAM) strarr);

		if (result == IDCANCEL || result == IDABORT)
		{
			// Declare it a failure
			req.result = -1;
			req.response = to_string(res.error());

			g_sslErrorMutex.unlock();
		}
		else if (isSSLError && (result == IDCONTINUE || result == IDIGNORE))
		{
			GetLocalSettings()->SetEnableTLSVerification(false);
			PrepareQuit();

			g_bQuittingFromSSLError = true;
			SendMessage(g_Hwnd, WM_FORCERESTART, 0, 0);
			g_sslErrorMutex.unlock();
			return false;
		}
		// return true to retry, IDTRYAGAIN or IDRETRY
		else
		{
			g_sslErrorMutex.unlock();
			return true;
		}
	}
	else if (res.error() == Error::Canceled)
	{
		req.result = HTTP_CANCELED;
		req.response = "Operation cancelled by user";
	}
	else
	{
		req.result = res->status;
		req.response = res->body;
	}

	// Call the handler function.
	// N.B.  Don't return unless you're absolutely done with the request!
	req.pFunc(&req);

	// Return false to let the runner know that it shouldn't retry.
	return false;
}

std::string NetworkerThreadManager::ErrorMessage(int code) const
{
	if (code < 0) return "Client Error";
	return std::string(httplib::detail::status_message(code));
}

void NetworkerThread::IdleWait()
{
	Sleep(100);
}

// Custom Content Provider to track progress
class ProgressContentProvider {
public:
	typedef std::function<bool(uint64_t, uint64_t)> ProgressFunction;

    ProgressContentProvider(const uint8_t* bytes, size_t size, ProgressFunction prog)
        : data_(bytes), data_size_(size), offset_(0), progfunc(prog) {}

    bool operator()(size_t offset, httplib::DataSink& sink) {
        size_t data_to_send = std::min(data_size_ - offset, REPORT_PROGRESS_EVERY_BYTES);
        if (data_to_send > 0) {
            sink.write((const char*) &data_[offset], data_to_send);
            offset_ = offset;
			if (!progfunc(offset_, data_size_))
				return false;
        }
		else {
			sink.done();
		}
		return true;
    }

private:
	const uint8_t* data_;
    size_t data_size_;
    size_t offset_;
	ProgressFunction progfunc;
};

void NetworkerThread::FulfillRequest(NetRequest& req)
{
	std::string& url = req.url;

	// split the URL into its host name and path
	std::string hostName = "", path = "";
	auto pos = url.find("://"), pos2 = pos;
	if (pos != std::string::npos)
		pos2 = url.find("/", pos + 4);
	else
		pos2 = url.find("/");

	if (pos2 != std::string::npos)
	{
		hostName = url.substr(0, pos2);
		path = url.substr(pos2);
	}

	using namespace httplib;
	Client client(hostName);

	// on Windows XP, enabling this doesn't actually work for some reason.
	// Probably outdated certs. I mean, this would allow attackers to host
	// a self-instance of Discord to intercept packets, but this is fine
	// for now.....
	client.enable_server_certificate_verification(GetLocalSettings()->EnableTLSVerification());

	// Follow redirects.  Used by GitHub auto-update service
	client.set_follow_location(true);

	Headers headers;
	headers.insert(std::make_pair("User-Agent", GetClientConfig()->GetUserAgent()));

	if (AddExtraHeaders())
	{
		headers.insert(std::make_pair("X-Super-Properties", GetClientConfig()->GetSerializedBase64Blob()));
		headers.insert(std::make_pair("X-Discord-Timezone", GetClientConfig()->GetTimezone()));
		headers.insert(std::make_pair("X-Discord-Locale", GetClientConfig()->GetLocale()));
		headers.insert(std::make_pair("Sec-Ch-Ua", GetClientConfig()->GetSecChUa()));
		headers.insert(std::make_pair("Sec-Ch-Ua-Mobile", "?0"));
		headers.insert(std::make_pair("Sec-Ch-Ua-Platform", GetClientConfig()->GetOS()));
	}

	if (req.authorization.size())
	{
		headers.insert(std::make_pair("Authorization", req.authorization));
	}

	bool retry = false;
	do
	{
		switch (req.type)
		{
			// no default constructor for httplib::Result?? this SUCKS!
			case NetRequest::POST:
			{
				const Result res = client.Post(path, headers, req.params, "application/x-www-form-urlencoded");
				retry = ProcessResult(req, res);
				break;
			}
			case NetRequest::POST_JSON:
			{
				const Result res = client.Post(path, headers, req.params, "application/json");
				retry = ProcessResult(req, res);
				break;
			}
			case NetRequest::PUT:
			{
				const Result res = client.Put(path, headers, req.params, "application/x-www-form-urlencoded");
				retry = ProcessResult(req, res);
				break;
			}
			case NetRequest::PUT_JSON:
			{
				const Result res = client.Put(path, headers, req.params, "application/json");
				retry = ProcessResult(req, res);
				break;
			}
			case NetRequest::PUT_OCTETS:
			{
				const Result res = client.Put(path, headers, (const char*) req.params_bytes.data(), req.params_bytes.size(), "application/octet-stream");
				retry = ProcessResult(req, res);
				break;
			}
			case NetRequest::PUT_OCTETS_PROGRESS:
			{
				using namespace std::placeholders;
				ProgressContentProvider provider(req.params_bytes.data(), req.params_bytes.size(), std::bind(&NetworkerThread::ProgressFunction, this, &req, _1, _2));
				req.result = HTTP_PROGRESS;
				const Result res = client.Put(path, headers, provider, "application/octet-stream");
				retry = ProcessResult(req, res);
				break;
			}
			case NetRequest::GET:
			{
				const Result res = client.Get(path, headers);
				retry = ProcessResult(req, res);
				break;
			}
			case NetRequest::GET_PROGRESS:
			{
				using namespace std::placeholders;
				const Result res = client.Get(path, headers, std::bind(&NetworkerThread::ProgressFunction, this, &req, _1, _2));
				retry = ProcessResult(req, res);
				break;
			}
			case NetRequest::PATCH:
			{
				const Result res = client.Patch(path, headers, req.params, "application/json");
				retry = ProcessResult(req, res);
				break;
			}
			case NetRequest::DELETE_:
			{
				const Result res = client.Delete(path, headers, req.params, "application/json");
				retry = ProcessResult(req, res);
				break;
			}
			default:
				assert(!"Don't know how to handle that type of request!");
				break;
		}
	}
	while (retry);
}

void NetworkerThread::Run()
{
	while (true)
	{
		// lock the mutex
		m_requestLock.lock();
		if (m_requests.empty())
		{
			m_requestLock.unlock();
			IdleWait();
			continue;
		}

		NetRequest request = std::move(m_requests.top());
		m_requests.pop();
		m_requestLock.unlock();

		// Service the request.
		if (request.type == NetRequest::QUIT)
			break;

		FulfillRequest(request);
	}

	m_ThreadHandle = NULL;
	m_ThreadID = 0;
}

DWORD WINAPI NetworkerThread::Init(LPVOID that)
{
	NetworkerThread* pThrd = (NetworkerThread*)that;
	pThrd->Run();
	return 0;
}

void NetworkerThread::AddRequest(
	NetRequest::eType type,
	const std::string& url,
	int itype,
	uint64_t requestKey,
	std::string params,
	std::string authorization,
	std::string additional_data,
	NetRequest::NetworkResponseFunc pRespFunc,
	uint8_t* stream_bytes,
	size_t stream_size)
{
	NetRequest rq(0, itype, requestKey, type, url, "", params, authorization, additional_data, pRespFunc, stream_bytes, stream_size);

	m_requestLock.lock();
	m_requests.push(rq);
	m_requestLock.unlock();
}

void NetworkerThread::StopAllRequests()
{
	m_requestLock.lock();
	while (!m_requests.empty())
		m_requests.pop();
	m_requestLock.unlock();
}

void NetworkerThread::PrepareQuit()
{
	m_requestLock.lock();

	while (!m_requests.empty())
		m_requests.pop();

	for (int i = 0; i < C_AMT_NETWORKER_THREADS; i++)
		m_requests.push(NetRequest(0, 0, 0, NetRequest::QUIT));

	m_requestLock.unlock();
}

bool NetworkerThread::ProgressFunction(NetRequest* pRequest, uint64_t offset, uint64_t length)
{
	if (pRequest->type == NetRequest::PUT_OCTETS_PROGRESS)
		assert(length == pRequest->params_bytes.size());

	pRequest->m_bCancelOp = false;
	pRequest->m_offset = offset;
	pRequest->m_length = length;
	pRequest->result = HTTP_PROGRESS;
	pRequest->pFunc(pRequest);

	// Return false if the operation must be cancelled.
	return !pRequest->m_bCancelOp;
}

NetworkerThread::NetworkerThread()
{
	m_ThreadHandle = CreateThread(
		NULL,
		0,
		Init,
		this,
		0,
		&m_ThreadID
	);

	if (!m_ThreadHandle)
	{
		HRESULT hr = GetLastError();
		std::string str = "Could not start NetworkerThread. Discord Messenger will now close.\n\n(" + std::to_string(hr) + ") " + GetStringFromHResult(hr);
		LPCTSTR ctstr = ConvertCppStringToTString(str);
		MessageBox(g_Hwnd, ctstr, TEXT("Discord Messenger - Fatal Error"), MB_ICONERROR | MB_OK);
		free((void*)ctstr);
		exit(1);
	}
}

NetworkerThread::~NetworkerThread()
{
	PrepareQuit();

	// wait for the thread to go away
	WaitForSingleObject(m_ThreadHandle, INFINITE);
}

NetworkerThreadManager::~NetworkerThreadManager()
{
	assert(m_bKilled && "Ideally you wouldn't kill now");
	Kill();
}

void NetworkerThreadManager::Init()
{
	m_bKilled = false;
	for (int i = 0; i < C_AMT_NETWORKER_THREADS; i++)
		m_pNetworkThreads[i] = new NetworkerThread();
}

void NetworkerThreadManager::StopAllRequests()
{
	for (int i = 0; i < C_AMT_NETWORKER_THREADS; i++) {
		if (m_pNetworkThreads[i])
			m_pNetworkThreads[i]->StopAllRequests();
	}
}

void NetworkerThreadManager::PrepareQuit()
{
	for (int i = 0; i < C_AMT_NETWORKER_THREADS; i++) {
		if (m_pNetworkThreads[i])
			m_pNetworkThreads[i]->PrepareQuit();
	}
}

void NetworkerThreadManager::Kill()
{
	PrepareQuit();

	// Wait for all networker threads to quit
	HANDLE threads[C_AMT_NETWORKER_THREADS];
	int nthreads = 0;

	for (int i = 0; i < C_AMT_NETWORKER_THREADS; i++)
	{
		if (!m_pNetworkThreads[i])
			continue;

		HANDLE h = m_pNetworkThreads[i]->GetThreadHandle();
		if (h)
			threads[nthreads++] = h;
	}

	if (nthreads > 0)
		WaitForMultipleObjects(nthreads, threads, true, INFINITE);

	for (int i = 0; i < C_AMT_NETWORKER_THREADS; i++)
	{
		if (m_pNetworkThreads[i])
			delete m_pNetworkThreads[i];

		m_pNetworkThreads[i] = NULL;
	}

	m_bKilled = true;
}

void NetworkerThreadManager::PerformRequest(
	bool interactive,
	NetRequest::eType type,
	const std::string& url,
	int itype,
	uint64_t requestKey,
	std::string params,
	std::string authorization,
	std::string additional_data,
	NetRequest::NetworkResponseFunc pRespFunc,
	uint8_t* stream_bytes,
	size_t stream_size)
{
	int idx;
	if (interactive) {
		m_nextInteractiveId = (m_nextInteractiveId + 1) % C_INTERACTIVE_NETWORKER_THREADS;
		idx = m_nextInteractiveId;
	}
	else {
		m_nextBackgroundId = C_INTERACTIVE_NETWORKER_THREADS + (m_nextBackgroundId + 1) % (C_AMT_NETWORKER_THREADS - C_INTERACTIVE_NETWORKER_THREADS);
		idx = m_nextBackgroundId;
	}

	m_pNetworkThreads[idx]->AddRequest(type, url, itype, requestKey, params, authorization, additional_data, pRespFunc, stream_bytes, stream_size);
}
