#pragma once

#include <windows.h>
#include "models/Snowflake.hpp"

class MessageList;

class NotificationViewer
{
public:
	static void Show(int x, int y, bool rightJustify = true);
	static bool IsActive();

private:
	static void Initialize(HWND hWnd);
	static void OnResize(HWND hWnd, int newWidth, int newHeight);
	static void OnClickMessage(Snowflake sf);
	static void MarkAllAsRead();
	static INT_PTR CALLBACK DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	static POINT m_appearXY;
	static bool m_bRightJustify;
	static bool m_bActive;
	static MessageList* m_pMessageList;
};

