#pragma once

// If windows.h isn't already included
#ifndef _WINDOWS_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winuser.h>
#undef  WIN32_LEAN_AND_MEAN

#endif

#include <queue>
#include <cassert>

#ifdef MINGW_SPECIFIC_HACKS
#include <iprog/mutex.hpp>
#else
#include <mutex>
#endif

#include "../discord/HTTPClient.hpp"

struct NetworkResponse
{
	int m_code; // 200 = OK, 404 = Not Found, 403 = Forbidden, 401 = Unauthorized
};

#define C_AMT_NETWORKER_THREADS (16)
#define C_INTERACTIVE_NETWORKER_THREADS (8)

class NetworkerThread
{
public:
#ifdef MINGW_SPECIFIC_HACKS
	using nmutex = iprog::mutex;
#else
	using nmutex = std::mutex;
#endif

private:
	std::priority_queue<NetRequest> m_requests;
	nmutex m_requestLock;

	HANDLE m_ThreadHandle;
	DWORD  m_ThreadID;

	bool ProcessResult(NetRequest& req, const httplib::Result& res);

	void IdleWait();
	
protected:
	friend class NetworkerThreadManager;

	HANDLE GetThreadHandle() const {
		return m_ThreadHandle;
	}

public:
	static DWORD WINAPI Init(LPVOID that);

	void FulfillRequest(NetRequest& request);
	void Run();

	NetworkerThread();
	~NetworkerThread();

	// Safely adds a request to the network worker thread's queue.
	// * The requestKey is an identifier for what type of request was made. It can be a pointer, enum etc.
	// * Note: The response function will be run within the context of the networker thread, so
	//   that's where you send messages back to the main thread.
	// * Note: The stream_bytes array is owned by the HTTP client after being passed in.
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
};

class NetworkerThreadManager : public HTTPClient
{
public:
	~NetworkerThreadManager();

	void Init() override;
	void StopAllRequests() override;
	void PrepareQuit() override;
	void Kill() override;

	// Adds a request to one of the networker threads.  If interactive, is
	// prioritized by sending to a different set of networker threads.
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

private:
	NetworkerThread* m_pNetworkThreads[C_AMT_NETWORKER_THREADS] = { nullptr };

	int m_nextInteractiveId = 0;
	int m_nextBackgroundId  = 0;
	bool m_bKilled = true;
};

//NetworkerThreadManager* GetNetworkerThreadManager();

