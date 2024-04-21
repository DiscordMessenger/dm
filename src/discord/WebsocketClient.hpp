#pragma once
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>

namespace CloseCode
{
	enum {
		UNKNOWN_ERROR = 4000,
		UNKNOWN_OPCODE,
		DECODE_ERROR,
		NOT_AUTHENTICATED,
		AUTHENTICATION_FAILED,
		ALREADY_AUTHENTICATED,
		INVALID_SEQ = 4007,
		RATE_LIMITED,
		SESSION_TIMED_OUT,
		INVALID_SHARD,
		SHARDING_REQUIRED,
		INVALID_API_VERSION,
		INVALID_INTENT,
		DISALLOWED_INTENT,

		LOG_ON_AGAIN = 5000,
	};
}

typedef websocketpp::client<websocketpp::config::asio_tls_client> WSClient;
typedef websocketpp::lib::shared_ptr<websocketpp::lib::thread> WSThreadSharedPtr;
typedef websocketpp::lib::asio::ssl::context AsioSslContext;
typedef websocketpp::lib::shared_ptr<AsioSslContext> AsioSslContextSharedPtr;

class WSConnectionMetadata
{
public:
	enum eStatus
	{
		CONNECTING,
		OPEN,
		FAILED,
		CLOSED,
	};

	typedef websocketpp::lib::shared_ptr<WSConnectionMetadata> Pointer;
 
	WSConnectionMetadata(int id, websocketpp::connection_hdl hdl, std::string uri)
	  : m_id(id)
	  , m_hdl(hdl)
	  , m_status(CONNECTING)
	  , m_uri(uri)
	  , m_server("N/A")
	{}

	void OnOpen(WSClient* c, websocketpp::connection_hdl hdl);
	void OnFail(WSClient* c, websocketpp::connection_hdl hdl);
	void OnClose(WSClient* c, websocketpp::connection_hdl hdl);
	void OnMessage(websocketpp::connection_hdl hdl, WSClient::message_ptr msg);
 
	websocketpp::connection_hdl GetHDL() const
	{
		return m_hdl;
	}

	int GetID() const
	{
		return m_id;
	}

	eStatus GetStatus() const
	{
		return m_status;
	}

private:
	int m_id;
	websocketpp::connection_hdl m_hdl;
	eStatus m_status;
	std::string m_uri;
	std::string m_server;
	std::string m_errorReason;
};

struct WebsocketMessageParm
{
	int m_gatewayId;
	std::string m_payload;
};

class WebsocketClient
{
public:
	WebsocketClient();
	~WebsocketClient();

	void Init();

	void Kill();

	// Returns a connection ID.
	int Connect(const std::string& uri);

	// Gets metadata about a connection.
	WSConnectionMetadata::Pointer GetMetadata(int ID);

	// Closes a connection by ID.
	void Close(int ID, websocketpp::close::status::value code);

	// Send a message to a connection.
	void SendMsg(int id, const std::string& msg);

private:
	typedef std::map<int, WSConnectionMetadata::Pointer> WSConnList;

	WSClient m_endpoint;
	WSThreadSharedPtr m_thread;
	WSConnList m_connList;
	int m_nextId = 0;
	bool m_bKilled = true;

	// Handle TLS initialization.
	AsioSslContextSharedPtr HandleTLSInit(websocketpp::connection_hdl hdl);
};

WebsocketClient* GetWebsocketClient();

