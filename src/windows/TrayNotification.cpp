#include "TrayNotification.hpp"
#include "Main.hpp"

constexpr int NOTIFICATION_ID = 1000;

// Tray Notification Singleton
static TrayNotification g_trayNotification;
TrayNotification* GetTrayNotification() {
    return &g_trayNotification;
}

TrayNotification::~TrayNotification()
{
    assert(!m_bInitialized);
    Deinitialize();
}

void TrayNotification::Initialize()
{
    NOTIFYICONDATA d;
    ZeroMemory(&d, sizeof d);

    d.cbSize = NOTIFYICONDATA_V2_SIZE;
    d.hWnd   = g_Hwnd;
    d.uID    = NOTIFICATION_ID;
	d.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    d.hIcon  = LoadIcon(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_ICON)));
	d.uTimeout = 10000;
    d.uCallbackMessage = WM_NOTIFMANAGERCALLBACK;
	
	_tcscpy(d.szTip, TmGetTString(IDS_PROGRAM_NAME));

    Shell_NotifyIcon(NIM_ADD, &d);

    m_bInitialized = true;
}

void TrayNotification::Deinitialize()
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

void TrayNotification::OnNotification()
{
	Notification* pNotif = GetNotificationManager()->GetLatestNotification();
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

	NOTIFYICONDATA d;
	ZeroMemory(&d, sizeof d);
	d.cbSize = NOTIFYICONDATA_V2_SIZE;
	d.uID    = NOTIFICATION_ID;
	d.hWnd   = g_Hwnd;
	d.uFlags = NIF_INFO;
	d.dwInfoFlags = NIIF_USER;

	std::string titleString = pNotif->m_author + " wrote";
	if (!channelName.empty())
		titleString += " in " + channelName;
	titleString += ":";

	LPTSTR tstr;
	tstr = ConvertCppStringToTString(titleString);
	_tcsncpy(d.szInfoTitle, tstr, _countof(d.szInfoTitle));
	_tcscpy(d.szInfoTitle + _countof(d.szInfoTitle) - 4, TEXT("..."));
	free(tstr);

	tstr = ConvertCppStringToTString(pNotif->m_contents);
	_tcsncpy(d.szInfo, tstr, _countof(d.szInfo));
	_tcscpy(d.szInfo + _countof(d.szInfo) - 4, TEXT("..."));
	free(tstr);

	Shell_NotifyIcon(NIM_MODIFY, &d);
}

void TrayNotification::Callback(WPARAM wParam, LPARAM lParam)
{
	switch (LOWORD(lParam))
	{
		case NIN_BALLOONUSERCLICK:
		{
			DbgPrintW("Acknowledge Notification");
			break;
		}

		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
		{
			POINT pt;
			if (!GetCursorPos(&pt)) {
				DbgPrintW("Could not acquire cursor position.");
				break;
			}

			//ShowPopupMenu(pt, LOWORD(wParam) == WM_RBUTTONUP);
			break;
		}
	}


	DbgPrintW("Callback! Wparam: %d, Lparam: %p", wParam, lParam);
}
