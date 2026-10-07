// Discord Messenger for X11/Motif (IRIX first).

#include "Xm.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <X11/Xutil.h>
#include <Xm/Protocols.h>

#include "DiscordInstance.hpp"
#include "config/LocalSettings.hpp"
#include "network/WebsocketClient.hpp"
#include "state/MessageCache.hpp"
#include "posix/Frontend_Posix.hpp"
#include "posix/MainQueue.hpp"
#include "posix/NetworkerThread.hpp"

#include "Canvas.hpp"
#include "Fonts.hpp"
#include "ImageCache.hpp"
#include "LogonDialog.hpp"
#include "MainWindow.hpp"
#include "MessageView.hpp"
#include "Theme.hpp"

static XtAppContext g_app;
static Widget g_toplevel;
static PixelFormat g_pixelFormat;
static DiscordInstance* g_pDiscordInstance;
static bool g_bQuit;

static Visual* g_visual;
static int g_depth;
static Colormap g_colormap;

DiscordInstance* GetDiscordInstance()
{
	return g_pDiscordInstance;
}

// Shells must be told the visual the application runs on, or they get the
// screen's default one (and X refuses them a colormap of another depth).
int AddVisualArgs(Arg* args, int n)
{
	XtSetArg(args[n], XmNvisual, g_visual); n++;
	XtSetArg(args[n], XmNdepth, g_depth); n++;
	XtSetArg(args[n], XmNcolormap, g_colormap); n++;
	return n;
}

static void ShowLogon(const std::string& why);

class Frontend_Motif : public Frontend_Posix
{
public:
	void OnConnecting() override {
		GetMainWindow()->SetStatus("Connecting to Discord...");
	}
	void OnConnected() override {
		m_retryDelayMs = 1000;
		GetMainWindow()->SetStatus("");
		GetMainWindow()->UpdateGuildList();
	}
	void OnSessionClosed(int errorCode) override {
		GetMainWindow()->SetStatus("Disconnected (" + std::to_string(errorCode) + ").  File > Reconnect to try again.");
	}
	void OnLoggedOut() override {
		ShowLogon("Discord did not accept the token.  Log in again.");
	}
	void OnAddMessage(Snowflake channelID, const Message& msg) override {
		Frontend_Posix::OnAddMessage(channelID, msg);
		MainWindow* mw = GetMainWindow();
		if (mw->GetMessageView()->GetChannel() == channelID) {
			mw->GetMessageView()->Refresh();
			mw->OnStopTyping(channelID, msg.m_author_snowflake);
		}
		Channel* pChan = GetDiscordInstance()->GetChannelGlobally(channelID);
		if (pChan && pChan->IsDM() && GetDiscordInstance()->ResortChannels(pChan->m_parentGuild))
			UpdateChannelList();
	}
	void OnUpdateMessage(Snowflake channelID, const Message& msg) override {
		Frontend_Posix::OnUpdateMessage(channelID, msg);
		if (GetMainWindow()->GetMessageView()->GetChannel() == channelID)
			GetMainWindow()->GetMessageView()->Refresh();
	}
	void OnDeleteMessage(Snowflake messageInCurrentChannel) override {
		GetMainWindow()->GetMessageView()->Refresh();
	}
	void OnStartTyping(Snowflake userID, Snowflake guildID, Snowflake channelID, time_t startTime) override {
		GetMainWindow()->OnTyping(userID, guildID, channelID, startTime);
	}
	void OnFailedToSendMessage(Snowflake channel, Snowflake message) override {
		if (GetMainWindow()->GetMessageView()->GetChannel() == channel)
			GetMainWindow()->GetMessageView()->Refresh();
	}
	void UpdateSelectedGuild() override { GetMainWindow()->UpdateSelectedGuild(); }
	void UpdateSelectedChannel() override { GetMainWindow()->UpdateSelectedChannel(); }
	void UpdateChannelList() override { GetMainWindow()->UpdateChannelList(); }
	void UpdateMemberList() override { GetMainWindow()->UpdateMemberList(); }
	void UpdateChannelAcknowledge(Snowflake channelID, Snowflake messageID) override {
		GetMainWindow()->UpdateChannelList();
	}
	void RepaintGuildList() override { GetMainWindow()->UpdateGuildList(); }
	void RefreshMessages(ScrollDir::eScrollDir sd, Snowflake gapCulprit) override {
		GetMainWindow()->GetMessageView()->Refresh();
	}
	void RefreshMembers(const std::set<Snowflake>& members) override {
		GetMainWindow()->UpdateMemberList();
	}
	void UpdateUserData(Snowflake userID) override {
		GetMainWindow()->UpdateMemberList();
	}
	void UpdateProfileAvatar(Snowflake userID, const std::string& resid) override {
		GetMainWindow()->UpdateMemberList();
	}
	void OnAttachmentDownloaded(bool bIsProfilePicture, const uint8_t* pData, size_t nSize, const std::string& additData) override {
		ImageCache::Downloaded(additData, pData, nSize);
	}
	void OnAttachmentFailed(bool bIsProfilePicture, const std::string& additData) override {
		ImageCache::DownloadFailed(additData);
	}
	void SetHeartbeatInterval(int timeMs) override {
		if (m_heartbeat)
			XtRemoveTimeOut(m_heartbeat);
		m_heartbeat = 0;
		m_heartbeatMs = timeMs;
		if (timeMs > 0)
			m_heartbeat = XtAppAddTimeOut(g_app, timeMs, HeartbeatCB, this);
	}
	void RequestQuit() override {
		g_bQuit = true;
	}
	bool IsWindowMinimized() override {
		return GetMainWindow()->IsIconic();
	}

protected:
	void ShowError(const std::string& message) override {
		GetMainWindow()->ShowError(message);
	}
	void ScheduleReconnect(int ms) override {
		GetMainWindow()->SetStatus("Could not connect; trying again...");
		XtAppAddTimeOut(g_app, ms, ReconnectCB, this);
	}

private:
	static void HeartbeatCB(XtPointer client, XtIntervalId*) {
		Frontend_Motif* self = (Frontend_Motif*) client;
		self->m_heartbeat = XtAppAddTimeOut(g_app, self->m_heartbeatMs, HeartbeatCB, self);
		GetDiscordInstance()->SendHeartbeat();
	}
	static void ReconnectCB(XtPointer client, XtIntervalId*) {
		((Frontend_Motif*) client)->StartSession();
	}

	XtIntervalId m_heartbeat = 0;
	int m_heartbeatMs = 0;
};

static Frontend_Motif* g_pFrontend;
static NetworkerThreadManager* g_pHTTPClient;

Frontend* GetFrontend()
{
	return g_pFrontend;
}

HTTPClient* GetHTTPClient()
{
	return g_pHTTPClient;
}

// Starts over with the token in the settings: a new DiscordInstance, a new
// gateway session.
static void StartWithToken()
{
	GetHTTPClient()->StopAllRequests();
	if (g_pDiscordInstance) {
		g_pDiscordInstance->CloseGatewaySession();
		delete g_pDiscordInstance;
	}
	g_pDiscordInstance = new DiscordInstance(GetLocalSettings()->GetToken());
	GetMainWindow()->UpdateGuildList();
	GetMainWindow()->UpdateSelectedChannel();
	g_pFrontend->StartSession();
}

static void ShowLogon(const std::string& why)
{
	static bool s_showing = false;
	if (s_showing)
		return;
	s_showing = true;
	ShowLogonDialog(g_toplevel, why, [](const std::string& token) {
		s_showing = false;
		if (token.empty()) {
			g_bQuit = true;
			return;
		}
		GetLocalSettings()->SetToken(token);
		GetLocalSettings()->Save();
		StartWithToken();
	});
}

void RequestLogout()
{
	if (g_pDiscordInstance)
		g_pDiscordInstance->CloseGatewaySession();
	GetLocalSettings()->SetToken("");
	GetLocalSettings()->Save();
	ShowLogon("");
}

void RequestReconnect()
{
	if (g_pDiscordInstance) {
		g_pDiscordInstance->CloseGatewaySession();
		g_pFrontend->StartSession();
	}
}

// --demo: sample messages in the message view, without logging in (to see
// how messages are drawn).
static void LoadDemo()
{
	const Snowflake chan = 4242;
	time_t now = time(NULL);
	struct Sample { Snowflake author; const char* name; int minutesAgo; const char* text; MessageType::eType type; };
	const Sample samples[] = {
		{ 1001, "Ada", 26 * 60, "Good morning! Has anyone got the **Indigo2** booting from the new disk yet?", MessageType::DEFAULT },
		{ 1002, "Grace", 26 * 60 - 3, "Not yet, the PROM says `Unable to load bootp()` and stops.", MessageType::DEFAULT },
		{ 1002, "Grace", 26 * 60 - 2, "I'll try `setenv netaddr` again after lunch.", MessageType::DEFAULT },
		{ 1003, "Linus", 90, "", MessageType::USER_JOIN },
		{ 1001, "Ada", 45, "Here is what fixed it for me:\n```\nsetenv netaddr 192.168.1.20\nboot -f bootp()/unix\n```\nThen it went straight to the miniroot.", MessageType::DEFAULT },
		{ 1004, "Bjarne", 30, "> it went straight to the miniroot\nNice. *Italic*, __underlined__, ~~struck~~ and a link: https://www.sgi.com/ and caf\xc3\xa9 na\xc3\xafve \xe2\x80\x94 \xe2\x9c\x93 \xe2\x98\x85 \xf0\x9f\x98\x80", MessageType::DEFAULT },
		{ 1002, "Grace", 12, "# Release notes\n- MIPS IV build\n- FreeType text\n- Motif UI\n-# small print: tested on an emulated R10000", MessageType::DEFAULT },
		{ 1005, "Dennis", 2, "A long line to see the wrapping: Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris.", MessageType::DEFAULT },
	};
	Snowflake id = 1000000;
	for (auto& sm : samples)
	{
		Message m;
		m.m_snowflake = ++id;
		m.m_author_snowflake = sm.author << 22;
		m.m_author = sm.name;
		m.m_message = sm.text;
		m.m_type = sm.type;
		m.SetTime(now - sm.minutesAgo * 60);
		if (sm.author == 1001 && sm.minutesAgo == 45) {
			Attachment a;
			a.m_fileName = "bootp-setup.txt";
			a.m_size = 1834;
			a.m_actualUrl = "https://example.com/bootp-setup.txt";
			m.m_attachments.push_back(a);
		}
		if (sm.author == 1002 && sm.minutesAgo == 12) {
			Attachment a;
			a.m_fileName = "transparency.png";
			a.m_size = 226933;
			a.m_width = 800;
			a.m_height = 600;
			a.m_contentType = ContentType::PNG;
			a.m_proxyUrl = a.m_actualUrl = "https://upload.wikimedia.org/wikipedia/commons/4/47/PNG_transparency_demonstration_1.png";
			a.UpdatePreviewSize();
			m.m_attachments.push_back(a);
		}
		if (sm.author == 1004) {
			m.m_pReferencedMessage = std::make_shared<ReferenceMessage>();
			m.m_pReferencedMessage->m_author = "Ada";
			m.m_pReferencedMessage->m_message = "Then it went straight to the miniroot.";
			RichEmbed e;
			e.m_color = 0x2d8f9e;
			e.m_providerName = "sgi.com";
			e.m_title = "Silicon Graphics";
			e.m_url = "https://www.sgi.com/";
			e.m_description = "High-performance computing and *visualization* since 1982.";
			e.m_footerText = "Embed footer";
			e.m_bHasImage = true;
			e.m_imageUrl = e.m_imageProxiedUrl = "https://www.gstatic.com/webp/gallery/1.webp";
			e.m_imageWidth = 550;
			e.m_imageHeight = 368;
			m.m_embeds.push_back(e);
		}
		GetMessageCache()->AddMessage(chan, m);
	}
	GetMainWindow()->GetMessageView()->SetChannel(0, chan);
	GetMainWindow()->ShowDemoLists();
	GetMainWindow()->SetStatus("Demo: sample messages, not connected.");
}

static void WakeCB(XtPointer, int*, XtInputId*)
{
	MainQueue::Drain();
}

static void WmDeleteCB(Widget, XtPointer, XtPointer)
{
	g_bQuit = true;
}

// The visual to draw on: the default one when it is TrueColor, else a
// 24-bit (or deeper) TrueColor one when the screen has it, else the default
// (8-bit colour is dithered).  DM_VISUAL=default keeps the default visual.
static void PickVisual(Display* dpy)
{
	int scr = DefaultScreen(dpy);
	g_visual = DefaultVisual(dpy, scr);
	g_depth = DefaultDepth(dpy, scr);
	g_colormap = DefaultColormap(dpy, scr);

	const char* pref = getenv("DM_VISUAL");
	if ((pref && !strcmp(pref, "default")) || g_visual->c_class == TrueColor)
		return;

	XVisualInfo vi;
	if (XMatchVisualInfo(dpy, scr, 24, TrueColor, &vi) || XMatchVisualInfo(dpy, scr, 32, TrueColor, &vi)) {
		g_visual = vi.visual;
		g_depth = vi.depth;
		g_colormap = XCreateColormap(dpy, RootWindow(dpy, scr), g_visual, AllocNone);
	}
}

static XtString g_fallbackResources[] = {
	(XtString) "*sgiMode: True",
	(XtString) "*useSchemes: all",
	(XtString) "DiscordMessenger*guilds.visibleItemCount: 20",
	(XtString) "DiscordMessenger*channels.visibleItemCount: 20",
	(XtString) "DiscordMessenger*members.visibleItemCount: 20",
	(XtString) "DiscordMessenger*header.fontList: -*-helvetica-bold-r-normal--14-*-*-*-*-*-iso8859-1",
	NULL
};

int main(int argc, char** argv)
{
	srand((unsigned) time(NULL));
	MainQueue::Init();
	SetupPosixPaths();

	XtToolkitInitialize();
	g_app = XtCreateApplicationContext();
	XtAppSetFallbackResources(g_app, g_fallbackResources);
	Display* dpy = XtOpenDisplay(g_app, NULL, "dm", "DiscordMessenger", NULL, 0, &argc, argv);
	if (!dpy) {
		fprintf(stderr, "dm: cannot open the display (is DISPLAY set?)\n");
		return 1;
	}

	LoadMotifConfig();
	ApplyThemeResources(dpy);
	PickVisual(dpy);
	g_pixelFormat.Init(dpy, g_visual, g_depth, g_colormap);

	Arg args[8];
	int n = 0;
	n = AddVisualArgs(args, n);
	XtSetArg(args[n], XmNtitle, "Discord Messenger"); n++;
	XtSetArg(args[n], XmNiconName, "Discord"); n++;
	g_toplevel = XtAppCreateShell("dm", "DiscordMessenger", applicationShellWidgetClass, dpy, args, n);

	std::string fontErr;
	if (!Fonts::Init(fontErr)) {
		fprintf(stderr, "dm: %s\n", fontErr.c_str());
		return 1;
	}

	g_pFrontend = new Frontend_Motif;
	g_pHTTPClient = new NetworkerThreadManager;
	GetLocalSettings()->Load();

	new MainWindow(g_toplevel, g_pixelFormat);
	ImageCache::SetChangedCallback([] { GetMainWindow()->OnImagesChanged(); });

	Atom wmDelete = XmInternAtom(dpy, (char*) "WM_DELETE_WINDOW", False);
	XtVaSetValues(g_toplevel, XmNdeleteResponse, XmDO_NOTHING, NULL);
	XmAddWMProtocolCallback(g_toplevel, wmDelete, WmDeleteCB, NULL);

	XtRealizeWidget(g_toplevel);
	XtAppAddInput(g_app, MainQueue::WakeFd(), (XtPointer) XtInputReadMask, WakeCB, NULL);

	g_pHTTPClient->Init();
	GetWebsocketClient()->Init();

	// DM_TOKEN logs in for this run only; it is not saved.
	std::string token = GetLocalSettings()->GetToken();
	const char* envToken = getenv("DM_TOKEN");
	if (envToken && *envToken)
		token = envToken;

	bool demo = argc > 1 && !strcmp(argv[1], "--demo");
	g_pDiscordInstance = new DiscordInstance(demo ? "" : token);
	if (demo)
		LoadDemo();
	else if (token.empty())
		ShowLogon("");
	else
		g_pFrontend->StartSession();

	while (!g_bQuit)
		XtAppProcessEvent(g_app, XtIMAll);

	GetLocalSettings()->Save();
	XtUnrealizeWidget(g_toplevel);
	XFlush(dpy);

	g_pDiscordInstance->CloseGatewaySession();
	GetWebsocketClient()->Kill();
	MainQueue::Shutdown();
	g_pHTTPClient->Kill();
	return 0;
}
