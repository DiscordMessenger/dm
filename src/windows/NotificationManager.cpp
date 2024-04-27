#include "NotificationManager.hpp"
#include "Main.hpp"

#define C_NOTIF_ID (100)

/*

void NotificationManager::CreateTestNotification()
{
	Notification n{};
	NOTIFYICONDATA& d = n.m_data;

	d.cbSize = NOTIFYICONDATA_V2_SIZE;
	d.hWnd = g_Hwnd;
	d.uID = m_nextNotifID++;
	d.uFlags = NIF_ICON | NIF_MESSAGE | NIF_INFO | NIF_TIP | NIF_SHOWTIP;
	d.uCallbackMessage = WM_NOTIFMANAGERCALLBACK;
	d.hIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_ICON)));
	d.dwInfoFlags = NIIF_USER;
	d.uTimeout = 10000;

	_tcscpy(d.szInfo, TEXT("szInfo"));
	_tcscpy(d.szInfoTitle, TEXT("szInfoTitle"));
	_tcscpy(d.szTip, TmGetTString(""));

	if (!Shell_NotifyIcon(NIM_ADD, &d)) {
		TCHAR tch[4096];
		_snwprintf(tch, _countof(tch), TEXT("Cannot add icon: %x"), GetLastError());
		MessageBox(g_Hwnd, tch, TEXT("yo"), 0);
		return;
	}

	m_notifications.push_back(std::move(n));
}

*/

void NotificationManager::CreateNotification()
{
	NOTIFYICONDATA d;
	ZeroMemory(&d, sizeof d);

	d.cbSize = NOTIFYICONDATA_V2_SIZE;
	d.hWnd = g_Hwnd;
	d.uID = C_NOTIF_ID;
	d.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
	d.uCallbackMessage = WM_NOTIFMANAGERCALLBACK;
	d.hIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_ICON)));
	d.uTimeout = 10000;

	_tcscpy(d.szTip, TmGetTString(IDS_PROGRAM_NAME));

	Shell_NotifyIcon(NIM_ADD, &d);
}

void NotificationManager::DeleteNotification()
{
	NOTIFYICONDATA d;
	ZeroMemory(&d, sizeof d);
	d.cbSize = NOTIFYICONDATA_V2_SIZE;
	d.uID = C_NOTIF_ID;
	d.hWnd = g_Hwnd;
	Shell_NotifyIcon(NIM_DELETE, &d);
}

NotificationManager::~NotificationManager()
{
	DeleteNotification();
}

void NotificationManager::Callback(WPARAM wParam, LPARAM lParam)
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

			ShowPopupMenu(pt, LOWORD(wParam) == WM_RBUTTONUP);
			break;
		}
	}


	DbgPrintW("Callback! Wparam: %d, Lparam: %p", wParam, lParam);
}

void NotificationManager::ShowPopupMenu(POINT pt, bool bRightButton)
{
	HMENU menu = GetSubMenu(LoadMenu(g_hInstance, MAKEINTRESOURCE(IDR_NOTIFICATION_CONTEXT)), 0);

	// Adjust the menu here

	SetForegroundWindow(g_Hwnd);
	SetFocus(g_Hwnd);

	TrackPopupMenu(
		menu,
		bRightButton ? TPM_RIGHTBUTTON : TPM_LEFTBUTTON,
		pt.x, pt.y,
		0,
		g_Hwnd,
		NULL
	);
}
