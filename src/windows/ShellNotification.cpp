#include "ShellNotification.hpp"
#include "Main.hpp"
#include "../discord/LocalSettings.hpp"

constexpr int NOTIFICATION_ID = 1000;

// Shell Notification Singleton
static ShellNotification g_ShellNotification;
ShellNotification* GetShellNotification() {
	return &g_ShellNotification;
}

ShellNotification::~ShellNotification()
{
	assert(!m_bInitialized);
	Deinitialize();
}

void ShellNotification::Initialize()
{
	NOTIFYICONDATA d;
	ZeroMemory(&d, sizeof d);

	d.cbSize = NOTIFYICONDATA_V2_SIZE;
	d.hWnd   = g_Hwnd;
	d.uID    = NOTIFICATION_ID;
	d.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
	d.hIcon  = LoadIcon(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_ICON)));
	d.uCallbackMessage = WM_NOTIFMANAGERCALLBACK;
	
	_tcscpy(d.szTip, TmGetTString(IDS_PROGRAM_NAME));

	Shell_NotifyIcon(NIM_ADD, &d);

	m_bInitialized = true;
}

void ShellNotification::Deinitialize()
{
	if (!m_bInitialized)
		return;

	NOTIFYICONDATA d;
	ZeroMemory(&d, sizeof d);
	d.cbSize = NOTIFYICONDATA_V2_SIZE;
	d.uID    = NOTIFICATION_ID;
	d.hWnd   = g_Hwnd;
	Shell_NotifyIcon(NIM_DELETE, &d);

	m_bInitialized = false;
}

void ShellNotification::ShowBalloon(const std::string& titleString, const std::string& contents)
{
	if (!GetLocalSettings()->EnableNotifications())
		return;

	// If user is in 'Do Not Disturb' mode, no taskbar notification should be fired.
	// However, other methods of checking notifications will still work, such as the menu.
	Profile* pf = GetDiscordInstance()->GetProfile();
	if (pf)
	{
		if (pf->m_activeStatus == STATUS_DND)
			return;
	}

	if (GetLocalSettings()->FlashOnNotification() && IsIconic(g_Hwnd))
	{
		FlashWindow(g_Hwnd, FALSE);
	}

	NOTIFYICONDATA d;
	ZeroMemory(&d, sizeof d);
	d.cbSize = NOTIFYICONDATA_V2_SIZE;
	d.uID    = NOTIFICATION_ID;
	d.hWnd   = g_Hwnd;
	d.uFlags = NIF_INFO | NIF_REALTIME;
	d.dwInfoFlags = NIIF_USER;
	d.uTimeout = 5000;

	if (m_bBalloonActive)
	{
		_tcscpy(d.szInfo, TEXT(""));
		_tcscpy(d.szInfoTitle, TEXT(""));
		Shell_NotifyIcon(NIM_MODIFY, &d);
		m_bBalloonActive = false;
	}

	LPTSTR tstr;
	tstr = ConvertCppStringToTString(titleString);
	_tcsncpy(d.szInfoTitle, tstr, _countof(d.szInfoTitle));
	_tcscpy(d.szInfoTitle + _countof(d.szInfoTitle) - 4, TEXT("..."));
	free(tstr);

	tstr = ConvertCppStringToTString(contents);
	_tcsncpy(d.szInfo, tstr, _countof(d.szInfo));
	_tcscpy(d.szInfo + _countof(d.szInfo) - 4, TEXT("..."));
	free(tstr);

	Shell_NotifyIcon(NIM_MODIFY, &d);
}

void ShellNotification::ShowBalloonForOneNotification(Notification* pNotif)
{
	if (!pNotif) {
		DbgPrintW("False alarm, no notification available");
		return;
	}

	if (pNotif->m_bRead) {
		DbgPrintW("False alarm, latest notification was already read?");
		assert(!"pay attention!");
		return;
	}

	// Find the channel's name
	std::string channelName = "Unknown";
	Channel* pChan = GetDiscordInstance()->GetChannelGlobally(pNotif->m_sourceChannel);
	if (pChan) {
		if (pChan->m_channelType == Channel::DM)
			channelName = "";
		else
			channelName = pChan->GetTypeSymbol() + pChan->m_name;
	}

	std::string titleString = pNotif->m_author + (pNotif->m_bIsReply ? " replied" : " wrote");
	if (!channelName.empty())
		titleString += " in " + channelName;
	titleString += ":";

	std::string contents = GetDiscordInstance()->ReverseMentions(pNotif->m_contents, pNotif->m_sourceGuild), contents2;
	contents2.reserve(contents.size() * 2);

	for (char c : contents) {
		if (c == '\n')
			contents2 += "\r";
		contents2 += c;
	}

	ShowBalloon(titleString, contents2);
}

void ShellNotification::ShowBalloonForNotifications(const std::vector<Notification*>& pNotifs)
{
	if (pNotifs.empty())
		return;

	if (pNotifs.size() == 1) {
		ShowBalloonForOneNotification(pNotifs[0]);
		return;
	}

	std::string title = "You have " + std::to_string(pNotifs.size()) + " new notifications!";

	// Include an excerpt from the first ~5
	std::string content = "";

	for (size_t i = 0; i < 5 && i < pNotifs.size(); i++)
	{
		Notification* pNotif = pNotifs[i];
		std::string line = pNotif->m_author + ": " + GetDiscordInstance()->ReverseMentions(pNotif->m_contents, pNotif->m_sourceGuild);

		// remove new lines here
		for (char& c : line) {
			if (c == '\n') c = ' ';
		}
		if (!content.empty())
			content += "\r\n";

		content += line;
	}

	ShowBalloon(title, content);
}

void ShellNotification::OnNotification()
{
	// If a balloon is already active, then it'll be bundled in later
	//if (m_bBalloonActive)
	//{
	//	m_bAnyNotificationsSinceLastTime = true;
	//	return;
	//}

	std::vector<Notification*> notifs;

	auto& notifList = GetNotificationManager()->GetNotifications();
	for (auto it = notifList.begin(); it != notifList.end(); ++it)
	{
		if (!it->m_bRead)
			notifs.push_back(&*it);
	}

	ShowBalloonForNotifications(notifs);
	m_bAnyNotificationsSinceLastTime = false;
}

void ShellNotification::ShowContextMenu()
{
	POINT cursor;
	if (!GetCursorPos(&cursor)) {
		DbgPrintW("Could not acquire cursor position.");
		return;
	}

	const HMENU popupMenu = GetSubMenu(LoadMenu(g_hInstance, MAKEINTRESOURCE(IDR_NOTIFICATION_CONTEXT)), 0);
	SetForegroundWindow(g_Hwnd);
	TrackPopupMenu(popupMenu, TPM_LEFTBUTTON, cursor.x, cursor.y, 0, g_Hwnd, NULL);
}

void ShellNotification::Callback(WPARAM wParam, LPARAM lParam)
{
	switch (LOWORD(lParam))
	{
		case NIN_BALLOONSHOW:
		{
			m_bBalloonActive = true;
			break;
		}
		case NIN_BALLOONHIDE:
		{
			m_bBalloonActive = false;
			break;
		}
		case NIN_BALLOONTIMEOUT:
		{
			m_bBalloonActive = false;

			// If there are any new notifications, update the user about them as well.
			if (m_bAnyNotificationsSinceLastTime)
				OnNotification();

			break;
		}

		case NIN_BALLOONUSERCLICK:
		{
			// TODO: Don't know how to know which one we clicked
			DbgPrintW("Acknowledge Notification");

			ShowWindow(g_Hwnd, SW_SHOW);
			SetActiveWindow(g_Hwnd);

			break;
		}

		case WM_LBUTTONUP:
			SendMessage(g_Hwnd, WM_RESTOREAPP, 0, 0);
			break;

		case WM_RBUTTONUP:
			ShowContextMenu();
			break;
	}


	DbgPrintW("Callback! Wparam: %d, Lparam: %p", wParam, lParam);
}
