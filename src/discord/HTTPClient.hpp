#pragma once

#include <string>
#include <cstring>
#include <vector>

enum eHttpResponseCodes
{
	HTTP_OOPS = -1,
	HTTP_OK           = 200,
	HTTP_CREATED      = 201,
	HTTP_ACCEPTED     = 202,
	HTTP_NONAUTHINFO  = 203,
	HTTP_NOCONTENT    = 204,
	HTTP_RESETCONTENT = 204,
	HTTP_PARTIALCNTNT = 204,

	HTTP_BADREQUEST   = 400,
	HTTP_UNAUTHORIZED = 401,
	HTTP_FORBIDDEN    = 403,
	HTTP_NOTFOUND     = 404,
	HTTP_UNSUPPMEDIA  = 415,
	HTTP_TOOMANYREQS  = 429,

	HTTP_BADGATEWAY   = 502,

	HTTP_CANCELED     = 998,
	HTTP_PROGRESS     = 999,
};

namespace httplib {
	class Result;
}

//
// ! - The PUT_OCTETS_PROGRESS HTTP request type is a special one.
// Basically, the pFunc is called for every time that httplib wants
// to report progress, with a result of HTTP_PROGRESS (999), and then
// with the actual code.
//

struct NetRequest
{
	typedef void(*NetworkResponseFunc)(NetRequest* pReq);
	enum eType
	{
		NOTHING_MAN,
		QUIT,
		GET,
		POST,
		PUT,
		PATCH,
		POST_JSON,
		DELETE_, // WinNT defines "DELETE" as a macro... bruh
		PUT_OCTETS,
		PUT_OCTETS_PROGRESS, // (!)
	};
	int result = 0;
	int itype  = 0;
	uint64_t key = 0;
	eType type;
	NetworkResponseFunc pFunc;
	std::string url;
	std::string response = "";
	std::string params = "";
	std::string authorization = "";
	std::string additional_data = "";
	std::vector<uint8_t> params_bytes; // used only for PUT_OCTETS and PUT_OCTETS_PROGRESS
	size_t m_offset; // used only for PUT_OCTETS_PROGRESS
	bool m_bCancelOp = false; // used only for PUT_OCTETS_PROGRESS

	size_t GetOffset() const {
		return m_offset;
	}
	size_t GetTotalBytes() const {
		return params_bytes.size();
	}

	int Priority() const;

	bool operator<(const NetRequest& other) const {
		return Priority() < other.Priority();
	}

	// NOTE: NetRequest takes ownership of the bytes array we pass!
	NetRequest(
		int _result = 0,
		int _itype = 0,
		uint64_t _key = 0,
		eType _type = NOTHING_MAN,
		const std::string& _url = "",
		const std::string& _response = "",
		const std::string& _params = "",
		const std::string& _authorization = "",
		const std::string& _additional_data = "",
		NetworkResponseFunc _func = nullptr,
		uint8_t* _bytes = nullptr,
		size_t _size = 0
	);
};

class HTTPClient
{
public:
	HTTPClient() {}
	virtual ~HTTPClient() {}

	virtual void Init() = 0;
	virtual void Kill() = 0;
	virtual void StopAllRequests() = 0;
	virtual void PrepareQuit() = 0;

	// Sends a request via this HTTP client.  If interactive, is prioritized.
	// Data from stream_bytes is copied if needed.
	virtual void PerformRequest(
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
	) = 0;

	static void DefaultRequestHandler(NetRequest* pRequest);
};

HTTPClient* GetHTTPClient();
