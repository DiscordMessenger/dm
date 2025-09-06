#pragma once
#include <cassert>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <list>
#include <condition_variable>
#include <curl/curl.h>

// why couldn't I just use C++ lists? they didn't fulfill everything I needed
#include "boronlist.h"

#include "../discord/HTTPClient.hpp"
#include "../discord/DiscordClientConfig.hpp"
#include "../discord/LocalSettings.hpp"

#ifdef MINGW_SPECIFIC_HACKS
#include "iprog/thread.hpp"
#include "iprog/mutex.hpp"
#include "iprog/condition_variable.hpp"
typedef iprog::thread L_thread;
typedef iprog::mutex L_mutex;
typedef iprog::condition_variable L_condition_variable;
#else
typedef std::thread L_thread;
typedef std::mutex L_mutex;
typedef std::condition_variable L_condition_variable;
#endif

class HTTPClient_curl : public HTTPClient
{
public:
	HTTPClient_curl();
	virtual ~HTTPClient_curl() {}
	
	void Init() override;
	void Kill() override;
	void StopAllRequests() override;
	void PrepareQuit() override;
	std::string ErrorMessage(int code) const override;
	void PerformRequest(
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
	) override;
	
public:
	static void InitializeCABlob();

private:
	struct HTTPRequest
	{
		BLIST_ENTRY entry;
		NetRequest* netRequest = nullptr;
		CURL* easyHandle = nullptr;
		curl_slist* headers = nullptr;

		std::string url;
		std::string params;
		std::string userAgent;
		std::string xSuperProperties;
		std::string xDiscordTimeZone;
		std::string xDiscordLocale;
		std::string secChUa;
		std::string secChUaPlatform;
		std::string authorization;
		
		~HTTPRequest()
		{
			if (netRequest)
				delete netRequest;
			assert(!easyHandle);
		}
		
		HTTPRequest() {}
		HTTPRequest(const HTTPRequest&) = delete;
		HTTPRequest(HTTPRequest&& other) = delete;
	};
	
private:
	L_thread m_workerThread;
	L_mutex m_workerThreadMutex;
	L_condition_variable m_signalCV;
	std::atomic<bool> m_bRunning { false };
	
	BLIST_ENTRY m_netRequests;
	
	CURLM* m_multi = nullptr;

	bool ProcessMultiEvent();
	void MultiPerform();
	void WorkerThreadRun();
	void WaitForNewRequests();

	static void WorkerThreadInit(HTTPClient_curl* instance);
	static size_t ReadCallback(void* contents, size_t size, size_t nmemb, void* userp);
	static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
	static int GetProgress(void* userp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);
	static int PutProgress(void* userp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);
	static void DefaultHandler(NetRequest* pRequest);
};
