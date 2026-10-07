#include "NetworkerThread.hpp"
#include "network/DiscordRequest.hpp"
#include "config/LocalSettings.hpp"
#include "config/DiscordClientConfig.hpp"
#include "Frontend.hpp"
#include "utils/Util.hpp"

#include <cassert>
#include <cstdlib>
#include <functional>
#include <sys/stat.h>
#include <unistd.h>

#include <httplib/httplib.h>

#ifndef DM_DATADIR
#define DM_DATADIR "/usr/local/share/discord-messenger"
#endif

constexpr size_t REPORT_PROGRESS_EVERY_BYTES = 15360; // arbitrary

// Connection failures are retried this many times, a second apart and then
// two, before the request fails (the Win32 client asks the user instead).
constexpr int MAX_ATTEMPTS = 3;

int g_latestSSLError = 0; // used by httplib.h

static bool FileExists(const std::string& path)
{
	struct stat st;
	return !path.empty() && stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

std::string GetCACertFile()
{
	static std::string s_file;
	static bool s_looked = false;
	if (s_looked)
		return s_file;
	s_looked = true;

	const char* env = getenv("DM_CA_FILE");
	const char* candidates[] = {
		env,
		DM_DATADIR "/cacert.pem",
		"/etc/ssl/certs/ca-certificates.crt",
		"/etc/ssl/cert.pem",
		"/etc/pki/tls/certs/ca-bundle.crt",
	};
	for (const char* c : candidates) {
		if (c && FileExists(c)) {
			s_file = c;
			break;
		}
	}
	return s_file;
}

// Used by the gateway's TLS context (core/network/WebsocketClient.cpp).
void LoadSystemCertsOnPosix(SSL_CTX* ctx)
{
	std::string file = GetCACertFile();
	if (!file.empty())
		SSL_CTX_load_verify_locations(ctx, file.c_str(), nullptr);
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
		case PUT_OCTETS:
		case PUT_OCTETS_PROGRESS:
		case PUT_JSON:
		case DELETE_:
			prio = 100;
			break;
		case GET:
		case GET_PROGRESS:
			prio = 90;
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

bool NetworkerThread::ProcessResult(NetRequest& req, const httplib::Result& res, int& attempt)
{
	using namespace httplib;

	if (!res || res.error() == Error::SSLServerVerification)
	{
		bool isSSLError = res.error() == Error::SSLServerVerification;
		std::string errorstr = to_string(res.error());

		if (!isSSLError && ++attempt < MAX_ATTEMPTS) {
			DbgPrintF("Request to %s failed (%s), retrying", req.url.c_str(), errorstr.c_str());
			sleep(attempt);
			return true;
		}

		req.result = -1;
		req.response = errorstr;
		if (isSSLError)
			GetFrontend()->OnGenericError("Could not verify the identity of " + req.url +
				".\n\nThe server's certificate was not accepted (" + errorstr + ").");
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

	// N.B.  Don't return unless you're absolutely done with the request!
	req.pFunc(&req);
	return false;
}

std::string NetworkerThreadManager::ErrorMessage(int code) const
{
	if (code < 0) return "Client Error";
	return std::string(httplib::detail::status_message(code));
}

// Custom content provider to track upload progress
class ProgressContentProvider {
public:
	typedef std::function<bool(uint64_t, uint64_t)> ProgressFunction;

	ProgressContentProvider(const uint8_t* bytes, size_t size, ProgressFunction prog)
		: data_(bytes), data_size_(size), progfunc(prog) {}

	bool operator()(size_t offset, httplib::DataSink& sink) {
		size_t data_to_send = std::min(data_size_ - offset, REPORT_PROGRESS_EVERY_BYTES);
		if (data_to_send > 0) {
			sink.write((const char*) &data_[offset], data_to_send);
			if (!progfunc(offset, data_size_))
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
	ProgressFunction progfunc;
};

void NetworkerThread::FulfillRequest(NetRequest& req)
{
	std::string& url = req.url;
	DbgPrintF("Accessing URL: %s", url.c_str());

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

	client.enable_server_certificate_verification(GetLocalSettings()->EnableTLSVerification());
	std::string caFile = GetCACertFile();
	if (!caFile.empty())
		client.set_ca_cert_path(caFile.c_str());

	// Follow redirects (CDN links)
	client.set_follow_location(true);

	Headers headers;
	headers.insert(std::make_pair("User-Agent", GetClientConfig()->GetUserAgent()));

	if (GetLocalSettings()->AddExtraHeaders())
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
		assert(req.url.find("images") == std::string::npos);
		assert(req.url.find("cdn") == std::string::npos);
		assert(req.url.find("discord") != std::string::npos);

		headers.insert(std::make_pair("Authorization", req.authorization));
	}

	using namespace std::placeholders;
	int attempt = 0;
	bool retry = false;
	do
	{
		switch (req.type)
		{
			case NetRequest::POST:
				retry = ProcessResult(req, client.Post(path, headers, req.params, "application/x-www-form-urlencoded"), attempt);
				break;
			case NetRequest::POST_JSON:
				retry = ProcessResult(req, client.Post(path, headers, req.params, "application/json"), attempt);
				break;
			case NetRequest::PUT:
				retry = ProcessResult(req, client.Put(path, headers, req.params, "application/x-www-form-urlencoded"), attempt);
				break;
			case NetRequest::PUT_JSON:
				retry = ProcessResult(req, client.Put(path, headers, req.params, "application/json"), attempt);
				break;
			case NetRequest::PUT_OCTETS:
				retry = ProcessResult(req, client.Put(path, headers, (const char*) req.params_bytes.data(), req.params_bytes.size(), "application/octet-stream"), attempt);
				break;
			case NetRequest::PUT_OCTETS_PROGRESS:
			{
				ProgressContentProvider provider(req.params_bytes.data(), req.params_bytes.size(), std::bind(&NetworkerThread::ProgressFunction, this, &req, _1, _2));
				req.result = HTTP_PROGRESS;
				retry = ProcessResult(req, client.Put(path, headers, provider, "application/octet-stream"), attempt);
				break;
			}
			case NetRequest::GET:
				retry = ProcessResult(req, client.Get(path, headers), attempt);
				break;
			case NetRequest::GET_PROGRESS:
				retry = ProcessResult(req, client.Get(path, headers, std::bind(&NetworkerThread::ProgressFunction, this, &req, _1, _2)), attempt);
				break;
			case NetRequest::PATCH:
				retry = ProcessResult(req, client.Patch(path, headers, req.params, "application/json"), attempt);
				break;
			case NetRequest::DELETE_:
				retry = ProcessResult(req, client.Delete(path, headers, req.params, "application/json"), attempt);
				break;
			default:
				assert(!"Don't know how to handle that type of request!");
				break;
		}
	}
	while (retry);
}

void NetworkerThread::Run()
{
	for (;;)
	{
		NetRequest request;
		{
			std::unique_lock<std::mutex> lk(m_requestLock);
			m_requestCond.wait(lk, [this] { return !m_requests.empty(); });
			request = m_requests.top();
			m_requests.pop();
		}

		if (request.type == NetRequest::QUIT)
			break;

		FulfillRequest(request);
	}
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

	std::lock_guard<std::mutex> lk(m_requestLock);
	m_requests.push(rq);
	m_requestCond.notify_one();
}

void NetworkerThread::StopAllRequests()
{
	std::lock_guard<std::mutex> lk(m_requestLock);
	while (!m_requests.empty())
		m_requests.pop();
}

void NetworkerThread::PrepareQuit()
{
	std::lock_guard<std::mutex> lk(m_requestLock);
	while (!m_requests.empty())
		m_requests.pop();
	m_requests.push(NetRequest(0, 0, 0, NetRequest::QUIT));
	m_requestCond.notify_one();
}

void NetworkerThread::Join()
{
	if (m_thread.joinable())
		m_thread.join();
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
	m_thread = std::thread(&NetworkerThread::Run, this);
}

NetworkerThread::~NetworkerThread()
{
	PrepareQuit();
	Join();
}

NetworkerThreadManager::~NetworkerThreadManager()
{
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
	if (m_bKilled)
		return;

	PrepareQuit();
	for (int i = 0; i < C_AMT_NETWORKER_THREADS; i++)
	{
		delete m_pNetworkThreads[i];
		m_pNetworkThreads[i] = nullptr;
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

	if (m_pNetworkThreads[idx])
		m_pNetworkThreads[idx]->AddRequest(type, url, itype, requestKey, params, authorization, additional_data, pRespFunc, stream_bytes, stream_size);
}
