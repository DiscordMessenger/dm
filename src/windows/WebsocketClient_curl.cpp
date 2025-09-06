#include "WebsocketClient_curl.h"
#include "HTTPClient_curl.h"
#include "Frontend_Win32.hpp"
#include "../discord/Util.hpp"

WebsocketClient_curl::WebsocketClient_curl()
{
}

WebsocketClient_curl::~WebsocketClient_curl()
{
}

void WebsocketClient_curl::Init()
{
}

void WebsocketClient_curl::Kill()
{
	for (auto& connection : m_connections)
	{
		if (connection.second)
			connection.second->Disconnect(CloseCode::NORMAL);
	}

	// TODO: wait for these connections to truly die off?
}

int WebsocketClient_curl::Connect(const std::string& uri)
{
	CURL* easy = curl_easy_init();
	if (!easy)
	{
		DbgPrintF("Error, curl_easy_init failed.");
		return -1;
	}

	int id = m_nextId++;
	WSConnection* connection = new WSConnection(this, id, easy, uri);
	m_connections[id] = connection;

	connection->Connect();
	return id;
}

void WebsocketClient_curl::Close(int id, int code)
{
	WSConnection* connection;
	std::lock_guard<std::mutex> guard(m_mutex);

	connection = m_connections[id];
	if (!connection)
		return;

	connection->Disconnect(code);
}

void WebsocketClient_curl::SendMsg(int id, const std::string& msg)
{
	WSConnection* connection;
	std::lock_guard<std::mutex> guard(m_mutex);

	connection = m_connections[id];
	if (!connection)
		return;

	connection->Send(msg);
}

void WebsocketClient_curl::ConnectionClosed(WSConnection* connection)
{
	std::lock_guard<std::mutex> guard(m_mutex);

	auto it = m_connections.find(connection->GetID());
	if (it == m_connections.end())
		return;

	m_connections.erase(it);
	delete connection;
}

void WebsocketClient_curl::WSConnection::Connect()
{
	std::thread thrd(&WSConnection::ConnectInternal, this);
	thrd.detach();
}

void WebsocketClient_curl::WSConnection::Disconnect(int code)
{
	m_mutex.lock();
	short payload = (short) code;

	size_t sent = 0;
	CURLcode rc = curl_ws_send(m_easy, &payload, sizeof payload, &sent, 1024, CURLWS_CLOSE);

	m_bIsOpen = false;
	m_mutex.unlock();
}

void WebsocketClient_curl::WSConnection::Send(const std::string& payload)
{
	std::lock_guard<std::mutex> guard(m_mutex);

	size_t sent = 0;
	CURLcode rc = curl_ws_send(m_easy, payload.data(), payload.size(), &sent, (curl_off_t) payload.size(), CURLWS_TEXT);

	if (sent < payload.size())
		DbgPrintF("WARNING: Partial send (sent %zu bytes but wanted to send %zu bytes) not supported yet.", sent, payload.size());

	if (rc != CURLE_OK)
		GetFrontend()->OnWebsocketFail(m_id, rc, std::string("Cannot send packet: ") + curl_easy_strerror(rc), false, false);
}

void WebsocketClient_curl::WSConnection::ConnectInternal()
{
	L_lock_guard<L_mutex> guard(m_mutex);

	m_userAgent = GetClientConfig()->GetUserAgent();
	
	// Runs in a separate thread.
	curl_easy_setopt(m_easy, CURLOPT_URL, m_uri.c_str());
	curl_easy_setopt(m_easy, CURLOPT_USERAGENT, m_userAgent.c_str());

	if (GetLocalSettings()->EnableTLSVerification())
	{
		curl_easy_setopt(m_easy, CURLOPT_CAINFO_BLOB, HTTPClient_curl::GetCABlob());
	}
	else
	{
		curl_easy_setopt(m_easy, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(m_easy, CURLOPT_SSL_VERIFYHOST, 0L);
	}

	curl_easy_setopt(m_easy, CURLOPT_CONNECT_ONLY, 2L);

	CURLcode res = curl_easy_perform(m_easy);
	if (res != CURLE_OK)
	{
		m_errorReason = std::string(curl_easy_strerror(res));
		GetFrontend()->OnWebsocketFail(m_id, res, "Could not connect: " + m_errorReason, false, true);
		return;
	}

	// Connected
	m_recvThread = std::thread(ReceiveThread, this);
}

void DumpCurrentPayload(std::vector<uint8_t>& payload) {
	
	std::string stuff = "Payload after this: ";

	for (size_t i = 0; i < payload.size(); i++) {
		if (!payload[i]) {
			stuff += "\\0";
		}
		else if (payload[i] < 0x20 || payload[i] >= 0x7F) {
			char buffer[8];
			snprintf(buffer, sizeof buffer, "\\x%02X", payload[i]);
			stuff += std::string(buffer);
		}
		else {
			stuff += (char)payload[i];
		}
	}

	stuff += "\n";
	OutputDebugStringA(stuff.c_str());
}

void WebsocketClient_curl::WSConnection::ReceiveThread2()
{
	std::vector<uint8_t> currentMessage;

	while (m_bIsOpen)
	{
		const struct curl_ws_frame* meta = NULL;
		size_t bytesRead = 0;
		uint8_t* buffer = m_buffer;
		CURLcode rc = CURLE_OK;

		// Read data
		{
			std::lock_guard<std::mutex> guard(m_mutex);
			rc = curl_ws_recv(m_easy, m_buffer, m_bufferSize, &bytesRead, &meta);
		}

		if (rc == CURLE_AGAIN)
		{
			fd_set rfds;
			FD_ZERO(&rfds);
			curl_socket_t sockfd;
			curl_easy_getinfo(m_easy, CURLINFO_ACTIVESOCKET, &sockfd);
			FD_SET(sockfd, &rfds);

			struct timeval tv { 1, 0 }; // 1 second timeout
			select(sockfd + 1, &rfds, nullptr, nullptr, &tv);
			continue;
		}

		if (rc != CURLE_OK)
		{
			DbgPrintF("Websocket receive error: %s", curl_easy_strerror(rc));
			m_bIsOpen = false;
			GetFrontend()->OnWebsocketFail(m_id, rc, "Connection has been closed", false, false);
			break;
		}

		if (bytesRead > 0)
		{
			size_t newSize = meta->offset + bytesRead;
			if (newSize > currentMessage.size())
				currentMessage.resize(newSize);

			memcpy(currentMessage.data() + meta->offset, buffer, bytesRead);
		}

		if (meta->bytesleft == 0)
		{
			std::string message((const char*) currentMessage.data(), currentMessage.size());
			GetFrontend()->OnWebsocketMessage(m_id, message);
			currentMessage.clear();
		}
	}
}

void WebsocketClient_curl::WSConnection::ReceiveThread(WSConnection* connection)
{
	connection->m_bIsOpen = true;
	connection->ReceiveThread2();

	// since we are about to delete the connection, we have
	// to detach this thread from the connection class otherwise
	// we are in for a world of pain
	connection->m_recvThread.detach();
	connection->m_parent->ConnectionClosed(connection);
}

static WebsocketClient_curl g_WSCSingleton;
WebsocketClient* GetWebsocketClient() { return &g_WSCSingleton; }
