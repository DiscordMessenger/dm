#include "Frontend_Posix.hpp"
#include "MainQueue.hpp"

#include "DiscordInstance.hpp"
#include "config/LocalSettings.hpp"
#include "network/DiscordAPI.hpp"
#include "network/DiscordRequest.hpp"
#include "state/MessageCache.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

DiscordInstance* GetDiscordInstance();

static const char* const g_monthNames[] = {
	"January", "February", "March", "April", "May", "June", "July",
	"August", "September", "October", "November", "December"
};

void SetupPosixPaths()
{
	std::string base;
	const char* dmHome = getenv("DM_HOME");
	if (dmHome && *dmHome) {
		base = dmHome;
		SetBasePath(base);
		SetProgramNamePath("");
	}
	else {
		const char* home = getenv("HOME");
		base = std::string(home && *home ? home : ".");
		SetBasePath(base);
		SetProgramNamePath(".discordmessenger");
	}

	mkdir(GetBasePath().c_str(), 0700);
	mkdir(GetCachePath().c_str(), 0700);
}

void Frontend_Posix::StartSession()
{
	DiscordInstance* pInst = GetDiscordInstance();
	if (pInst->HasGatewayURL()) {
		pInst->StartGatewaySession();
	}
	else {
		GetHTTPClient()->PerformRequest(
			false,
			NetRequest::GET,
			GetDiscordAPI() + "gateway",
			DiscordRequest::GATEWAY,
			0
		);
	}
}

void Frontend_Posix::OnLoginAgain()
{
	MainQueue::Post([this] { StartSession(); });
}

void Frontend_Posix::OnAddMessage(Snowflake channelID, const Message& msg)
{
	GetMessageCache()->AddMessage(channelID, msg);
}

void Frontend_Posix::OnUpdateMessage(Snowflake channelID, const Message& msg)
{
	GetMessageCache()->EditMessage(channelID, msg);
}

void Frontend_Posix::OnRequestDone(NetRequest* pRequest)
{
	// Called on a network thread; the request lives on its stack.
	NetRequest copy = *pRequest;
	MainQueue::Post([copy]() mutable {
		GetDiscordInstance()->HandleRequest(&copy);
	});
}

void Frontend_Posix::OnWebsocketMessage(int gatewayID, const std::string& payload)
{
	MainQueue::Post([gatewayID, payload] {
		DiscordInstance* pInst = GetDiscordInstance();
		if (pInst->GetGatewayID() == gatewayID)
			pInst->HandleGatewayMessage(payload);
	});
}

void Frontend_Posix::OnWebsocketClose(int gatewayID, int errorCode, const std::string& message)
{
	MainQueue::Post([gatewayID, errorCode] {
		DiscordInstance* pInst = GetDiscordInstance();
		if (pInst->GetGatewayID() == gatewayID)
			pInst->GatewayClosed(errorCode);
		else
			DbgPrintF("Unknown gateway connection %d closed: %d", gatewayID, errorCode);
	});
}

void Frontend_Posix::OnWebsocketFail(int gatewayID, int errorCode, const std::string& message, bool isTLSError, bool mayRetry)
{
	std::string text = "Could not connect to the websocket gateway.\nConnection was closed with code: " +
		std::to_string(errorCode) + "\n\nMessage: " + message;
	if (isTLSError)
		text += "\n\nYour connection may not be private: Discord Messenger could not verify "
			"that it is connecting to Discord's real-time service.";

	MainQueue::Post([this, text, isTLSError, mayRetry] {
		OnConnectFailed(text, isTLSError, mayRetry);
	});
}

void Frontend_Posix::OnConnectFailed(const std::string& message, bool isTLSError, bool mayRetry)
{
	if (!mayRetry) {
		ShowError(message);
		return;
	}

	DbgPrintF("%s\nTrying to connect again in %d ms", message.c_str(), m_retryDelayMs);
	ScheduleReconnect(m_retryDelayMs);
	m_retryDelayMs = m_retryDelayMs * 115 / 100;
	if (m_retryDelayMs > 10000)
		m_retryDelayMs = 10000;
}

void Frontend_Posix::OnFailedToUploadFile(const std::string& file, int error)
{
	if (error == HTTP_CANCELED)
		return;

	std::string text = "Failed to upload " + file + " (error " + std::to_string(error) + ").";
	MainQueue::Post([this, text] { ShowError(text); });
}

void Frontend_Posix::OnGenericError(const std::string& message)
{
	MainQueue::Post([this, message] { ShowError(message); });
}

void Frontend_Posix::OnJsonException(const std::string& message)
{
	OnGenericError("A bug has occurred and Discord Messenger failed to parse a piece of JSON received from the server.\n\nMessage:" + message);
}

void Frontend_Posix::OnCantViewChannel(const std::string& channelName)
{
	OnGenericError("You do not have permission to view the channel #" + channelName + ".");
}

void Frontend_Posix::OnGatewayConnectFailure()
{
	OnGenericError("Could not connect to Discord servers.\n\nThis could be because you aren't connected to the Internet, or because Discord servers are down.");
}

void Frontend_Posix::OnProtobufError(Protobuf::ErrorCode code)
{
	OnGenericError("Cannot load settings information from Discord. Got error code " + std::to_string((int) code) + " from protobuf.");
}

void Frontend_Posix::LaunchURL(const std::string& url)
{
	// The browser to open links with: DM_BROWSER, then BROWSER.
	const char* browser = getenv("DM_BROWSER");
	if (!browser || !*browser)
		browser = getenv("BROWSER");
	if (!browser || !*browser) {
		OnGenericError("No web browser is set (DM_BROWSER or BROWSER) to open:\n\n" + url);
		return;
	}

	pid_t pid = fork();
	if (pid == 0) {
		execlp(browser, browser, url.c_str(), (char*) NULL);
		_exit(127);
	}
}

static std::string ReadFile(const std::string& path)
{
	std::ifstream in(path.c_str(), std::ios::binary);
	if (!in)
		return "";
	std::ostringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

std::string Frontend_Posix::LoadConfig()
{
	return ReadFile(GetBasePath() + "/settings.json");
}

bool Frontend_Posix::SaveConfig(const std::string& configJson)
{
	// Write a new file and rename it over the old one, so a crash never
	// leaves half a settings file.
	std::string path = GetBasePath() + "/settings.json";
	std::string tmp = path + ".new";
	FILE* f = fopen(tmp.c_str(), "wb");
	if (!f)
		return false;
	chmod(tmp.c_str(), 0600); // it holds the login token
	bool ok = fwrite(configJson.data(), 1, configJson.size(), f) == configJson.size();
	ok = (fclose(f) == 0) && ok;
	if (!ok || rename(tmp.c_str(), path.c_str()) != 0) {
		unlink(tmp.c_str());
		return false;
	}
	return true;
}

std::string Frontend_Posix::GetDirectMessagesText() { return "Direct Messages"; }
std::string Frontend_Posix::GetPleaseWaitText() { return "Please wait..."; }

std::string Frontend_Posix::GetMonthName(int index)
{
	if (index < 0 || index > 11)
		return "";
	return g_monthNames[index];
}

std::string Frontend_Posix::GetTodayAtText() { return "Today at " + GetFormatTimestampTimeShort(); }
std::string Frontend_Posix::GetYesterdayAtText() { return "Yesterday at " + GetFormatTimestampTimeShort(); }
std::string Frontend_Posix::GetFormatDateOnlyText() { return "%s %d%s, %d"; }
std::string Frontend_Posix::GetFormatTimeLongText() { return "%d-%m-%Y at " + GetFormatTimestampTimeShort(); }
std::string Frontend_Posix::GetFormatTimeShortText() { return "%d/%m " + GetFormatTimestampTimeShort(); }
std::string Frontend_Posix::GetFormatTimeShorterText() { return GetFormatTimestampTimeShort(); }

std::string Frontend_Posix::GetFormatTimestampTimeShort()
{
	return GetLocalSettings()->Use12HourTime() ? "%I:%M %p" : "%H:%M";
}

std::string Frontend_Posix::GetFormatTimestampTimeLong() { return "%H:%M:%S"; }
std::string Frontend_Posix::GetFormatTimestampDateShort() { return "%d/%m/%Y"; }
std::string Frontend_Posix::GetFormatTimestampDateLong() { return "%e %B %Y"; }

std::string Frontend_Posix::GetFormatTimestampDateLongTimeShort()
{
	return GetFormatTimestampDateLong() + " " + GetFormatTimestampTimeShort();
}

std::string Frontend_Posix::GetFormatTimestampDateLongTimeLong()
{
	return "%A, " + GetFormatTimestampDateLong() + " " + GetFormatTimestampTimeShort();
}

#ifdef USE_DEBUG_PRINTS
void Frontend_Posix::DebugPrint(const char* fmt, va_list vl)
{
	// DM_DEBUG=1 prints the core's debug messages on stderr.
	static int s_enabled = -1;
	if (s_enabled < 0) {
		const char* e = getenv("DM_DEBUG");
		s_enabled = e && *e && *e != '0';
	}
	if (!s_enabled)
		return;

	vfprintf(stderr, fmt, vl);
	fputc('\n', stderr);
}
#endif
