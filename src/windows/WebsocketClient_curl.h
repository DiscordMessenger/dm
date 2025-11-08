#pragma once

#include <string>
#include <map>
#include "network/WebsocketClient.hpp"

#include <curl/curl.h>
#include <curl/easy.h>
#include <curl/websockets.h>

#include <atomic>

#ifdef MINGW_SPECIFIC_HACKS

#include "iprog/thread.hpp"
#include "iprog/mutex.hpp"
#include "iprog/lock_guard.hpp"
using L_thread = iprog::thread;
using L_mutex = iprog::mutex;
using L_this_thread = iprog::this_thread;
template<typename M>
using L_lock_guard = typename iprog::lock_guard<M>;

#else

#include <thread>
#include <mutex>
using L_thread = std::thread;
using L_mutex = std::mutex;

namespace L_this_thread = std::this_thread;

template<typename M>
using L_lock_guard = typename std::lock_guard<M>;

#endif

class WebsocketClient_curl : public WebsocketClient
{
public:
	WebsocketClient_curl();
	~WebsocketClient_curl() override;
	void Init() override;
	void Kill() override;
	int Connect(const std::string& uri) override;
	void Close(int id, int code) override;
	void SendMsg(int id, const std::string& msg) override;

private:
	class WSConnection
	{
	public:
		WSConnection(WebsocketClient_curl* parent, int id, CURL* easy, const std::string& uri) :
			m_parent(parent),
			m_id(id),
			m_easy(easy),
			m_uri(uri)
		{
			m_buffer = new uint8_t[65536];
			m_bufferSize = 65536;
		}

		~WSConnection()
		{
			curl_easy_cleanup(m_easy);
			delete[] m_buffer;
		}

		int GetID() const { return m_id; }
		void Connect();
		void Disconnect(int code);
		void Send(const std::string& payload);
		void SetNonBlockingHack();

	private:
		void ConnectInternal();
		void ReceiveThread2();
		static void ReceiveThread(WSConnection* connection);

	private:
		WebsocketClient_curl* m_parent;
		L_mutex m_mutex;
		int m_id;
		CURL* m_easy;
		uint8_t* m_buffer;
		size_t m_bufferSize;

		std::string m_uri;
		std::string m_userAgent;

		std::atomic<bool> m_bIsOpen = false;
		std::string m_errorReason;

		std::thread m_recvThread;
	};

protected:
	friend class WSConnection;

	void ConnectionClosed(WSConnection* connection);

private:
	L_mutex m_mutex;
	std::map<int, WSConnection*> m_connections;
	int m_nextId = 0;
};
