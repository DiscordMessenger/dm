#pragma once
#include <string>

namespace CloseCode
{
	enum {
		// RFC 6455, S.7.4.1
		NORMAL = 1000,
		GOING_AWAY = 1001,
		PROTOCOL_ERROR = 1002,
		UNACCEPTABLE_DATA = 1003,
		ABNORMAL_CLOSE = 1006, // illegal on the wire
		INVALID_PAYLOAD = 1007,
		POLICY_VIOLATION = 1008,
		MESSAGE_TOO_BIG = 1009,
		EXTENSION_MISSING = 1010,
		INTERNAL_ERROR = 1011,
		SERVICE_RESTART = 1012,

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

class WebsocketClient
{
public:
	WebsocketClient() {};
	virtual ~WebsocketClient() {};

	// Initializes the client.
	virtual void Init() = 0;

	// Kills the client.
	virtual void Kill() = 0;

	// Returns a connection ID.
	virtual int Connect(const std::string& uri) = 0;

	// Closes a connection by ID.
	virtual void Close(int ID, int code) = 0;

	// Send a message to a connection.
	virtual void SendMsg(int id, const std::string& msg) = 0;
};

WebsocketClient* GetWebsocketClient();
