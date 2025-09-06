#include "HTTPClient_curl.h"
#include "../discord/Util.hpp"

static std::vector<uint8_t> s_pemData;
static curl_blob s_pemDataBlob;

bool AddExtraHeaders()
{
	return GetLocalSettings()->AddExtraHeaders();
}

HTTPClient_curl::HTTPClient_curl()
{
	BInitializeListHead(&m_netRequests);
}

void HTTPClient_curl::Init()
{
	m_multi = curl_multi_init();
	m_bRunning.store(true);
	m_workerThread = L_thread(WorkerThreadInit, this);
}

void HTTPClient_curl::Kill() 
{
	m_bRunning.store(false);
	m_signalCV.notify_all();
	m_workerThread.join();
	
	curl_multi_cleanup(m_multi);
	m_multi = nullptr;
}

void HTTPClient_curl::StopAllRequests() 
{
	// TODO
}

void HTTPClient_curl::PrepareQuit() 
{
	// TODO
}

std::string HTTPClient_curl::ErrorMessage(int code) const
{
	// TODO
	return "TODO";
}

int HTTPClient_curl::GetProgress(void* userp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
	HTTPRequest* request = (HTTPRequest*) userp;
	NetRequest* netRequest = request->netRequest;
	
	netRequest->m_bCancelOp = false;
	netRequest->m_offset = dlnow;
	netRequest->m_length = dltotal;
	netRequest->result = HTTP_PROGRESS;
	netRequest->pFunc(netRequest);
	
	return netRequest->m_bCancelOp;
}

int HTTPClient_curl::PutProgress(void* userp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
	HTTPRequest* request = (HTTPRequest*) userp;
	NetRequest* netRequest = request->netRequest;
	
	netRequest->m_bCancelOp = false;
	netRequest->m_offset = ulnow;
	netRequest->m_length = ultotal;
	netRequest->result = HTTP_PROGRESS;
	netRequest->pFunc(netRequest);
	
	return netRequest->m_bCancelOp;
}

size_t HTTPClient_curl::WriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
	HTTPRequest* request = (HTTPRequest*) userp;
	NetRequest* netRequest = request->netRequest;
	netRequest->response.append((char*) contents, size * nmemb);
	return size * nmemb;
}

size_t HTTPClient_curl::ReadCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
	HTTPRequest* request = (HTTPRequest*) userp;
	NetRequest* netRequest = request->netRequest;
	
	size_t byteCount = size * nmemb;
	size_t startOffset = netRequest->m_offset;
	size_t endOffset = netRequest->m_offset + byteCount;
	
	if (endOffset >= netRequest->params_bytes.size()) {
		endOffset = netRequest->params_bytes.size();
		if (endOffset <= startOffset)
			return 0;
		
		byteCount = endOffset - startOffset;
	}
	
	memcpy(contents, netRequest->params_bytes.data(), byteCount);
	return byteCount;
}

void HTTPClient_curl::PerformRequest(
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
	size_t stream_size
)
{
	HTTPRequest* pRequest = new HTTPRequest;
	pRequest->netRequest = new NetRequest(0, itype, requestKey, type, url, "", params, authorization, additional_data, pRespFunc, stream_bytes, stream_size);
	pRequest->easyHandle = curl_easy_init();

	pRequest->url = url;
	pRequest->params = params;
	
	// Configure handle
	curl_easy_setopt(pRequest->easyHandle, CURLOPT_URL, pRequest->url.c_str());
	curl_easy_setopt(pRequest->easyHandle, CURLOPT_WRITEDATA, pRequest);
	curl_easy_setopt(pRequest->easyHandle, CURLOPT_READDATA, pRequest);
	curl_easy_setopt(pRequest->easyHandle, CURLOPT_PRIVATE, pRequest);
	curl_easy_setopt(pRequest->easyHandle, CURLOPT_FOLLOWLOCATION, 1L);

	curl_easy_setopt(pRequest->easyHandle, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(pRequest->easyHandle, CURLOPT_READFUNCTION, ReadCallback);
	
	if (GetLocalSettings()->EnableTLSVerification())
	{
		curl_easy_setopt(pRequest->easyHandle, CURLOPT_CAINFO_BLOB, &s_pemDataBlob);
	}
	else
	{
		curl_easy_setopt(pRequest->easyHandle, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(pRequest->easyHandle, CURLOPT_SSL_VERIFYHOST, 0L);
	}
	
	switch (type)
	{
		case NetRequest::GET_PROGRESS:
			curl_easy_setopt(pRequest->easyHandle, CURLOPT_NOPROGRESS, 0L);
			curl_easy_setopt(pRequest->easyHandle, CURLOPT_XFERINFOFUNCTION, GetProgress);
		case NetRequest::GET:
			curl_easy_setopt(pRequest->easyHandle, CURLOPT_HTTPGET, 1L);
			break;
		
		case NetRequest::DELETE_:
			curl_easy_setopt(pRequest->easyHandle, CURLOPT_CUSTOMREQUEST, "DELETE");
			break;
		
		case NetRequest::POST:
		case NetRequest::POST_JSON:
			curl_easy_setopt(pRequest->easyHandle, CURLOPT_POST, 1L);
			curl_easy_setopt(pRequest->easyHandle, CURLOPT_POSTFIELDS, pRequest->params.c_str());
			break;
		
		case NetRequest::PUT:
		case NetRequest::PUT_JSON:
			curl_easy_setopt(pRequest->easyHandle, CURLOPT_CUSTOMREQUEST, "PUT");
			curl_easy_setopt(pRequest->easyHandle, CURLOPT_POSTFIELDS, pRequest->params.c_str());
			break;
		
		case NetRequest::PUT_OCTETS_PROGRESS:
			curl_easy_setopt(pRequest->easyHandle, CURLOPT_NOPROGRESS, 0L);
			curl_easy_setopt(pRequest->easyHandle, CURLOPT_XFERINFOFUNCTION, PutProgress);
		case NetRequest::PUT_OCTETS:
			curl_easy_setopt(pRequest->easyHandle, CURLOPT_CUSTOMREQUEST, "PUT");
			curl_easy_setopt(pRequest->easyHandle, CURLOPT_UPLOAD, 1L);
			curl_easy_setopt(pRequest->easyHandle, CURLOPT_INFILESIZE_LARGE, (curl_off_t) stream_size);
			break;
		
		case NetRequest::PATCH:
			curl_easy_setopt(pRequest->easyHandle, CURLOPT_CUSTOMREQUEST, "PATCH");
			curl_easy_setopt(pRequest->easyHandle, CURLOPT_POSTFIELDS, pRequest->params.c_str());
			break;
	}
	
	struct curl_slist* headers = nullptr;
	
	// Set proper content type
	switch (type)
	{
		case NetRequest::PUT_JSON:
		case NetRequest::POST_JSON:
		case NetRequest::PATCH:
		case NetRequest::DELETE_:
		{
			headers = curl_slist_append(headers, "Content-Type: application/json");
			break;
		}
		case NetRequest::POST:
		case NetRequest::PUT:
		{
			headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
			break;
		}
		case NetRequest::PUT_OCTETS:
		case NetRequest::PUT_OCTETS_PROGRESS:
		{
			headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
			break;
		}
	}

	pRequest->userAgent = "User-Agent: " + GetClientConfig()->GetUserAgent();
	headers = curl_slist_append(headers, pRequest->userAgent.c_str());
	
	if (AddExtraHeaders())
	{
		pRequest->xSuperProperties = "X-Super-Properties: " + GetClientConfig()->GetSerializedBase64Blob();
		pRequest->xDiscordTimeZone = "X-Discord-Timezone: " + GetClientConfig()->GetTimezone();
		pRequest->xDiscordLocale = "X-Discord-Locale: " + GetClientConfig()->GetLocale();
		pRequest->secChUa = "Sec-Ch-Ua: " + GetClientConfig()->GetSecChUa();
		pRequest->secChUaPlatform = "Sec-Ch-Ua-Platform: " + GetClientConfig()->GetOS();

		headers = curl_slist_append(headers, pRequest->xSuperProperties.c_str());
		headers = curl_slist_append(headers, pRequest->xDiscordTimeZone.c_str());
		headers = curl_slist_append(headers, pRequest->xDiscordLocale.c_str());
		headers = curl_slist_append(headers, pRequest->secChUa.c_str());
		headers = curl_slist_append(headers, pRequest->secChUaPlatform.c_str());
		headers = curl_slist_append(headers, "Sec-Ch-Ua-Mobile: ?0");
	}
	
	if (!authorization.empty())
	{
		pRequest->authorization = "Authorization: " + authorization;
		headers = curl_slist_append(headers, pRequest->authorization.c_str());
	}

	pRequest->headers = headers;
	
	curl_easy_setopt(pRequest->easyHandle, CURLOPT_HTTPHEADER, pRequest->headers);
	
	m_workerThreadMutex.lock();
	
	curl_multi_add_handle(m_multi, pRequest->easyHandle);
	BInsertTailList(&m_netRequests, &pRequest->entry);
	
	m_workerThreadMutex.unlock();
	m_signalCV.notify_all();
}

void HTTPClient_curl::MultiPerform()
{
	int runningHandles = 0;
	std::unique_lock<std::mutex> lock(m_workerThreadMutex);
	curl_multi_perform(m_multi, &runningHandles);
}

bool HTTPClient_curl::ProcessMultiEvent()
{
	HTTPRequest* request = nullptr;
	CURLcode result = CURLE_OK;
	long httpStatus = 0;
	CURLMsg* msg;
	int msgsInQueue;

	{
		std::unique_lock<std::mutex> lock(m_workerThreadMutex);

		msg = curl_multi_info_read(m_multi, &msgsInQueue);
		if (!msg)
			return false;

		if (msg->msg != CURLMSG_DONE)
			return true;

		result = msg->data.result;

		CURL* easy = msg->easy_handle;
		curl_easy_getinfo(easy, CURLINFO_PRIVATE, &request);
		curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &httpStatus);

		// Clean up the easy handle.
		BRemoveEntryList(&request->entry);
		curl_multi_remove_handle(m_multi, easy);
		curl_easy_cleanup(easy);
		request->easyHandle = nullptr;
	}

	NetRequest* netRequest = request->netRequest;

	if (result != CURLE_OK)
	{
		netRequest->result = -1;
		netRequest->response = std::string("CURL error: ") + curl_easy_strerror(result);

		// TODO: Allow trying again.
	}
	else
	{
		netRequest->result = httpStatus;
	}

	// invoke callback now
	auto func = netRequest->pFunc;
	func(netRequest);

	// and then cleanup
	delete request;

	return true;
}


void HTTPClient_curl::WaitForNewRequests()
{
	std::unique_lock<std::mutex> lock(m_workerThreadMutex);
	m_signalCV.wait_for(lock, std::chrono::milliseconds(10));
}

void HTTPClient_curl::WorkerThreadRun()
{
	while (m_bRunning)
	{
		MultiPerform();
		while (ProcessMultiEvent());
		WaitForNewRequests();
	}
}

void HTTPClient_curl::WorkerThreadInit(HTTPClient_curl* instance)
{
	return instance->WorkerThreadRun();
}

#include "../discord/Frontend.hpp"

void HTTPClient_curl::DefaultHandler(NetRequest* pRequest)
{
	GetFrontend()->OnRequestDone(pRequest);
}

curl_blob* HTTPClient_curl::GetCABlob()
{
	return &s_pemDataBlob;
}

#include <windows.h>
#include "../resource.h"

void HTTPClient_curl::InitializeCABlob()
{
	HRSRC hRes = FindResource(NULL, MAKEINTRESOURCE(IDR_CACERT_PEM), TEXT("PEM"));
	HGLOBAL hMem = LoadResource(NULL, hRes);
	DWORD size = SizeofResource(NULL, hRes);
	const uint8_t* pemData = (const uint8_t*)LockResource(hMem);

	s_pemData.resize(size);
	memcpy(s_pemData.data(), pemData, size);

	curl_blob blob;
	blob.data = s_pemData.data();
	blob.len = s_pemData.size();
	blob.flags = CURL_BLOB_NOCOPY;
	s_pemDataBlob = blob;

	UnlockResource(pemData);
}
