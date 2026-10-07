// dm-cli: a text client that drives the Discord Messenger core without a
// GUI.  It is a test tool for ports: it fetches the gateway address over
// HTTPS, connects the gateway websocket over TLS, and prints what arrives.
//
//   dm-cli            log in with the token in the settings file or DM_TOKEN
//   dm-cli --probe    connect without a token: Discord greets the client,
//                     then refuses it (close code 4004), which proves HTTPS,
//                     TLS and the websocket work.

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <map>
#include <string>
#include <sys/select.h>
#include <sys/time.h>

#include "DiscordInstance.hpp"
#include "config/LocalSettings.hpp"
#include "network/WebsocketClient.hpp"
#include "text/TextInterface.hpp"
#include "posix/Frontend_Posix.hpp"
#include "posix/MainQueue.hpp"
#include "posix/NetworkerThread.hpp"

static DiscordInstance* g_pDiscordInstance;
static bool g_bQuit;
static bool g_bProbe;

DiscordInstance* GetDiscordInstance()
{
	return g_pDiscordInstance;
}

// ---- timers --------------------------------------------------------------

static long long NowMs()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (long long) tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

struct Timer
{
	long long due;
	int interval; // 0: once
	std::function<void()> fn;
};

static std::map<int, Timer> g_timers;
static int g_nextTimerId = 1;

static int AddTimer(int ms, bool repeat, std::function<void()> fn)
{
	int id = g_nextTimerId++;
	g_timers[id] = Timer{ NowMs() + ms, repeat ? ms : 0, fn };
	return id;
}

static void RunDueTimers()
{
	long long now = NowMs();
	for (auto it = g_timers.begin(); it != g_timers.end(); )
	{
		if (it->second.due > now) {
			++it;
			continue;
		}
		std::function<void()> fn = it->second.fn;
		if (it->second.interval) {
			it->second.due = now + it->second.interval;
			++it;
		}
		else {
			it = g_timers.erase(it);
		}
		fn();
		now = NowMs();
	}
}

static int MsToNextTimer()
{
	long long now = NowMs(), next = -1;
	for (auto& t : g_timers)
		if (next < 0 || t.second.due < next)
			next = t.second.due;
	if (next < 0)
		return 1000;
	return next <= now ? 0 : (int) (next - now);
}

// ---- frontend --------------------------------------------------------------

class Frontend_CLI : public Frontend_Posix
{
public:
	void OnConnecting() override {
		printf("* connecting to the gateway\n");
	}
	void OnConnected() override {
		m_retryDelayMs = 1000;
		printf("* connected as user %llu\n", (unsigned long long) GetDiscordInstance()->GetUserID());
	}
	void OnSessionClosed(int errorCode) override {
		printf("* session closed (%d)\n", errorCode);
	}
	void OnLoggedOut() override {
		printf("* logged out\n");
		RequestQuit();
	}
	void OnAddMessage(Snowflake channelID, const Message& msg) override {
		Frontend_Posix::OnAddMessage(channelID, msg);
		Channel* pChan = GetDiscordInstance()->GetChannelGlobally(channelID);
		printf("[#%s] %s: %s\n", pChan ? pChan->m_name.c_str() : "?",
			msg.m_author.c_str(), msg.m_message.c_str());
	}
	void OnWebsocketMessage(int gatewayID, const std::string& payload) override {
		if (g_bProbe)
			printf("* gateway says: %.200s\n", payload.c_str());
		Frontend_Posix::OnWebsocketMessage(gatewayID, payload);
	}
	void OnWebsocketClose(int gatewayID, int errorCode, const std::string& message) override {
		printf("* gateway closed the connection: %d %s\n", errorCode, message.c_str());
		if (g_bProbe) {
			MainQueue::Post([] { g_bQuit = true; });
			return;
		}
		Frontend_Posix::OnWebsocketClose(gatewayID, errorCode, message);
	}
	void SetHeartbeatInterval(int timeMs) override {
		printf("* heartbeat every %d ms\n", timeMs);
		if (m_heartbeatTimer)
			g_timers.erase(m_heartbeatTimer);
		m_heartbeatTimer = timeMs > 0 ?
			AddTimer(timeMs, true, [] { GetDiscordInstance()->SendHeartbeat(); }) : 0;
	}
	void RequestQuit() override {
		g_bQuit = true;
	}

protected:
	void ShowError(const std::string& message) override {
		fprintf(stderr, "error: %s\n", message.c_str());
	}
	void ScheduleReconnect(int ms) override {
		AddTimer(ms, false, [this] { StartSession(); });
	}

private:
	int m_heartbeatTimer = 0;
};

static Frontend_CLI* g_pFrontend;
static NetworkerThreadManager* g_pHTTPClient;

Frontend* GetFrontend()
{
	return g_pFrontend;
}

HTTPClient* GetHTTPClient()
{
	return g_pHTTPClient;
}

// The formatted-text renderer measures and draws words; a text client has no
// such thing, so it measures one unit per character.
struct DrawingContext {};
Point MdMeasureString(DrawingContext*, const String& word, int, bool& outWasWordWrapped, int)
{
	outWasWordWrapped = false;
	return Point((int) word.GetWrapped().size(), 1);
}
int MdLineHeight(DrawingContext*, int) { return 1; }
int MdSpaceWidth(DrawingContext*, int) { return 1; }
void MdDrawString(DrawingContext*, const Rect&, const String&, int) {}
void MdDrawCodeBackground(DrawingContext*, const Rect&) {}
void MdDrawForwardBackground(DrawingContext*, const Rect&) {}
int MdGetQuoteIndentSize() { return 2; }
void MdSetClippingRect(DrawingContext*, const Rect&) {}
void MdClearClippingRect(DrawingContext*) {}

int main(int argc, char** argv)
{
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--probe"))
			g_bProbe = true;
		else {
			fprintf(stderr, "usage: %s [--probe]\n", argv[0]);
			return 2;
		}
	}

	setvbuf(stdout, NULL, _IOLBF, 0);
	srand((unsigned) time(NULL));
	MainQueue::Init();
	SetupPosixPaths();

	g_pFrontend = new Frontend_CLI;
	g_pHTTPClient = new NetworkerThreadManager;
	GetLocalSettings()->Load();

	std::string token = g_bProbe ? "" : GetLocalSettings()->GetToken();
	const char* envToken = getenv("DM_TOKEN");
	if (!g_bProbe && envToken && *envToken)
		token = envToken;
	if (!g_bProbe && token.empty()) {
		fprintf(stderr, "No token: set DM_TOKEN, or run with --probe to test the connection.\n");
		return 1;
	}

	printf("* CA bundle: %s\n", GetCACertFile().empty() ? "(OpenSSL default)" : GetCACertFile().c_str());

	g_pHTTPClient->Init();
	GetWebsocketClient()->Init();
	g_pDiscordInstance = new DiscordInstance(token);
	g_pFrontend->StartSession();

	int fd = MainQueue::WakeFd();
	while (!g_bQuit)
	{
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
		int ms = MsToNextTimer();
		struct timeval tv = { ms / 1000, (ms % 1000) * 1000 };
		int n = select(fd + 1, &rfds, NULL, NULL, &tv);
		if (n < 0 && errno != EINTR) {
			perror("select");
			break;
		}
		if (n > 0 && FD_ISSET(fd, &rfds))
			MainQueue::Drain();
		RunDueTimers();
	}

	printf("* quitting\n");
	g_pDiscordInstance->CloseGatewaySession();
	GetWebsocketClient()->Kill();
	MainQueue::Shutdown();
	g_pHTTPClient->Kill();
	return 0;
}
