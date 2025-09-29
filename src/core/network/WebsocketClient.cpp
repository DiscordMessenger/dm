#include "WebsocketClient.hpp"
#include "../config/DiscordClientConfig.hpp"
#include "../../discord/Frontend.hpp"
#include "../utils/Util.hpp"
#include "../config/SettingsManager.hpp"
#include "../config/LocalSettings.hpp"

#include <asio/ssl/context.hpp>

static WebsocketClient g_WSCSingleton;

#ifdef _WIN32
void LoadSystemCertsOnWindows(SSL_CTX* ctx);
#endif

WebsocketClient* GetWebsocketClient()
{
	return &g_WSCSingleton;
}

void WSConnectionMetadata::OnOpen(WSClient* c, websocketpp::connection_hdl hdl)
{
	m_status = OPEN;

	WSClient::connection_ptr pConn = c->get_con_from_hdl(hdl);
	m_server = pConn->get_response_header("Server");
}

void WSConnectionMetadata::OnFail(WSClient* c, websocketpp::connection_hdl hdl)
{
	m_status = FAILED;

	WSClient::connection_ptr pConn = c->get_con_from_hdl(hdl);
	m_server = pConn->get_response_header("Server");
	m_errorReason = pConn->get_ec().message();

	auto xportEc = pConn->get_transport_ec();

	DbgPrintF("Failed to connect: %s (server '%s').  Transport error code 0x%x, message %s\n",
		m_errorReason.c_str(), m_server.c_str(), xportEc.value(), xportEc.message().c_str());

	std::string guiMessage = m_errorReason;
	if (!m_server.empty())
		guiMessage += " (server: " + m_server + ")";
	if (xportEc)
		guiMessage += " (transport error " + xportEc.message() + ")";

	namespace SocketErrors = websocketpp::transport::asio::socket::error;
	using WebsocketErrors = websocketpp::error::value;

	bool isTLSError = false;
	switch (pConn->get_ec().value()) {
		case SocketErrors::missing_tls_init_handler:
		case SocketErrors::tls_failed_sni_hostname:
		case SocketErrors::invalid_tls_context:
		case SocketErrors::security:
		//case SocketErrors::tls_handshake_timeout:
		//case SocketErrors::tls_handshake_failed:
			isTLSError = true;
	}

	bool mayRetry = false;
	switch (pConn->get_ec().value()) {
		case WSAHOST_NOT_FOUND:
		case WSATRY_AGAIN:
		case WSAEDISCON:
		case WSAETIMEDOUT:
		case SocketErrors::tls_handshake_timeout:
		case SocketErrors::tls_handshake_failed:
		case WebsocketErrors::bad_connection:
		case WebsocketErrors::open_handshake_timeout:
		case WebsocketErrors::close_handshake_timeout:
		case WebsocketErrors::extension_neg_failed:
			mayRetry = true;
	}

	GetFrontend()->OnWebsocketFail(m_id, pConn->get_ec().value(), guiMessage, isTLSError, mayRetry);
}

void WSConnectionMetadata::OnClose(WSClient* c, websocketpp::connection_hdl hdl)
{
	m_status = CLOSED;
	WSClient::connection_ptr pConn = c->get_con_from_hdl(hdl);

	std::stringstream s;
	s << "Close code: " << pConn->get_remote_close_code() << " ("
		<< websocketpp::close::status::get_string(pConn->get_remote_close_code())
		<< "), Close reason: " << pConn->get_remote_close_reason();

	DbgPrintF("Connection ID %d closed by gateway! %s", m_id, s.str().c_str());

	m_errorReason = s.str();
	
	GetFrontend()->OnWebsocketClose(m_id, pConn->get_remote_close_code(), s.str());
}

void WSConnectionMetadata::OnMessage(websocketpp::connection_hdl hdl, WSClient::message_ptr msg)
{
	if (msg->get_opcode() != websocketpp::frame::opcode::text)
	{
		DbgPrintF("ERROR: Got unhandled opcode %d", msg->get_opcode());
		return;
	}

	GetFrontend()->OnWebsocketMessage(m_id, msg->get_payload());
}

WebsocketClient::WebsocketClient()
{
}

WebsocketClient::~WebsocketClient()
{
	if (this == &g_WSCSingleton && !m_bKilled)
		assert(!"Probably shouldn't happen after exiting from main");

	Kill();
}

AsioSslContextSharedPtr WebsocketClient::HandleTLSInit(websocketpp::connection_hdl hdl)
{
	// establishes a SSL connection
	AsioSslContextSharedPtr ctx = std::make_shared<AsioSslContext>(AsioSslContext::tls);

	try
	{
		ctx->set_options(
			websocketpp::lib::asio::ssl::context::default_workarounds |
			websocketpp::lib::asio::ssl::context::single_dh_use
		);

		if (GetLocalSettings()->EnableTLSVerification())
		{
			ctx->set_default_verify_paths();
#ifdef _WIN32
			LoadSystemCertsOnWindows(ctx->native_handle());
#endif
		}
	}
	catch (std::exception& e)
	{
		DbgPrintF("HandleTLSInit: Error in context pointer: %s", e.what());
	}

	return ctx;
}

void WebsocketClient::HandleSocketInit(websocketpp::connection_hdl hdl, AsioSocketType& socket)
{
	WSClient::connection_ptr pConn = m_endpoint.get_con_from_hdl(hdl);

	if (GetLocalSettings()->EnableTLSVerification())
	{
		socket.set_verify_mode(websocketpp::lib::asio::ssl::verify_peer);

		if (!SSL_set_tlsext_host_name(reinterpret_cast<SSL*>(socket.native_handle()), "gateway.discord.gg")) {
			DbgPrintF("Failed to set SNI host name... this might go awry");
		}
	}
}

void WebsocketClient::Init()
{
	m_endpoint.clear_access_channels(websocketpp::log::alevel::all);
	m_endpoint.clear_error_channels(websocketpp::log::elevel::all);
	m_endpoint.init_asio();
	m_endpoint.start_perpetual();
	m_endpoint.set_tls_init_handler(websocketpp::lib::bind(
		&WebsocketClient::HandleTLSInit,
		this,
		websocketpp::lib::placeholders::_1
	));
	m_endpoint.set_socket_init_handler(websocketpp::lib::bind(
		&WebsocketClient::HandleSocketInit,
		this,
		websocketpp::lib::placeholders::_1,
		websocketpp::lib::placeholders::_2
	));
	m_thread.reset(new websocketpp::lib::thread(&WSClient::run, &m_endpoint));
	m_bKilled = false;
}

void WebsocketClient::Kill()
{
	if (m_bKilled)
		return;

	m_bKilled = true;
	m_endpoint.stop_perpetual();

	for (WSConnList::const_iterator it = m_connList.begin(); it != m_connList.end(); ++it)
	{
		if (it->second->GetStatus() != WSConnectionMetadata::OPEN)
			// Only close open connections
			continue;

		DbgPrintF("Closing connection %d...", it->second->GetID());

		websocketpp::lib::error_code ec;
		m_endpoint.close(it->second->GetHDL(), websocketpp::close::status::going_away, "", ec);

		if (ec)
			DbgPrintF("Error closing connection %d: %s", it->second->GetID(), ec.message().c_str());
	}

	m_connList.clear();
	m_thread->join();
}

int WebsocketClient::Connect(const std::string& uri)
{
	websocketpp::lib::error_code ec;
	DbgPrintF("WebsocketClient: Connecting to %s", uri.c_str());
	WSClient::connection_ptr con = m_endpoint.get_connection(uri, ec);

	if (ec) {
		DbgPrintF("ERROR: Websocket client could not initialize: %s", ec.message().c_str());
		return -1;
	}

	con->append_header("User-Agent", GetClientConfig()->GetUserAgent());
	con->append_header("Origin", "https://discord.com");

	int newID = m_nextId++;
	WSConnectionMetadata::Pointer pMetadata(new WSConnectionMetadata(newID, con->get_handle(), uri));
	m_connList[newID] = pMetadata;

	con->set_open_handler(websocketpp::lib::bind(
		&WSConnectionMetadata::OnOpen,
		pMetadata,
		&m_endpoint,
		websocketpp::lib::placeholders::_1
	));
	con->set_fail_handler(websocketpp::lib::bind(
		&WSConnectionMetadata::OnFail,
		pMetadata,
		&m_endpoint,
		websocketpp::lib::placeholders::_1
	));
	con->set_close_handler(websocketpp::lib::bind(
		&WSConnectionMetadata::OnClose,
		pMetadata,
		&m_endpoint,
		websocketpp::lib::placeholders::_1
	));
	con->set_message_handler(websocketpp::lib::bind(
		&WSConnectionMetadata::OnMessage,
		pMetadata,
		websocketpp::lib::placeholders::_1,
		websocketpp::lib::placeholders::_2
	));

	m_endpoint.connect(con);
	return newID;
}

WSConnectionMetadata::Pointer WebsocketClient::GetMetadata(int id)
{
	WSConnList::const_iterator metadata_it = m_connList.find(id);
	if (metadata_it == m_connList.end())
		return WSConnectionMetadata::Pointer();
	else
		return metadata_it->second;
}

void WebsocketClient::Close(int id, websocketpp::close::status::value code)
{
	websocketpp::lib::error_code ec;

	WSConnList::iterator metadata_it = m_connList.find(id);
	if (metadata_it == m_connList.end()) {
		DbgPrintF("Error, no connection with id %d", id);
		return;
	}

	DbgPrintF("Closing connection with id %d", id);
	m_endpoint.close(metadata_it->second->GetHDL(), code, "", ec);
	if (ec)
		DbgPrintF("Error initiating close: %s", ec.message().c_str());

	m_connList.erase(metadata_it);
}

void WebsocketClient::SendMsg(int id, const std::string& msg)
{
	websocketpp::lib::error_code ec;
	
	WSConnList::iterator metadata_it = m_connList.find(id);
	if (metadata_it == m_connList.end())
	{
		DbgPrintF("Error in SendMsg, no connection with id %d", id);
		return;
	}

	DbgPrintF("Sending message %s", msg.c_str());
	
	m_endpoint.send(metadata_it->second->GetHDL(), msg, websocketpp::frame::opcode::text, ec);
	if (ec)
	{
		DbgPrintF("Error in SendMsg, failed to send message: %s", ec.message().c_str());
		return;
	}
}
