#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include "network/HTTPClient.hpp"

namespace httplib {
	class Result;
}

#define C_AMT_NETWORKER_THREADS (4)
#define C_INTERACTIVE_NETWORKER_THREADS (2)

// One HTTP worker: a queue of requests served by httplib on its own thread.
// The response functions run on this thread; they hand their results to the
// UI thread themselves (the frontend's OnRequestDone).
class NetworkerThread
{
public:
	NetworkerThread();
	~NetworkerThread();

	void AddRequest(
		NetRequest::eType type,
		const std::string& url,
		int itype,
		uint64_t requestKey,
		std::string params = "",
		std::string authorization = "",
		std::string additional_data = "",
		NetRequest::NetworkResponseFunc pRespFunc = nullptr,
		uint8_t* stream_bytes = nullptr,
		size_t stream_size = 0
	);

	void StopAllRequests();
	void PrepareQuit();
	void Join();

	bool ProgressFunction(NetRequest* pRequest, uint64_t offset, uint64_t length);

private:
	void Run();
	void FulfillRequest(NetRequest& request);
	bool ProcessResult(NetRequest& req, const httplib::Result& res, int& attempt);

	std::priority_queue<NetRequest> m_requests;
	std::mutex m_requestLock;
	std::condition_variable m_requestCond;
	std::thread m_thread;
};

class NetworkerThreadManager : public HTTPClient
{
public:
	~NetworkerThreadManager();

	void Init() override;
	void StopAllRequests() override;
	void PrepareQuit() override;
	void Kill() override;

	// Adds a request to one of the networker threads.  If interactive, it
	// goes to a separate set of threads, so it never waits behind images.
	void PerformRequest(
		bool interactive,
		NetRequest::eType type,
		const std::string& url,
		int itype,
		uint64_t requestKey,
		std::string params = "",
		std::string authorization = "",
		std::string additional_data = "",
		NetRequest::NetworkResponseFunc pRespFunc = nullptr,
		uint8_t* stream_bytes = nullptr,
		size_t stream_size = 0
	) override;

	std::string ErrorMessage(int errorCode) const override;

private:
	NetworkerThread* m_pNetworkThreads[C_AMT_NETWORKER_THREADS] = { nullptr };

	int m_nextInteractiveId = 0;
	int m_nextBackgroundId  = 0;
	bool m_bKilled = true;
};

// The CA bundle used to verify servers: DM_CA_FILE, the one installed with
// the program, or the system's.  Empty if none was found.
std::string GetCACertFile();
