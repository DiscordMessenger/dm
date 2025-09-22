#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "WinUtils.hpp"
#include "ChatWindow.hpp"

extern HINSTANCE g_hInstance;

class GuildSubWindow : public ChatWindow
{
public:
	static bool InitializeClass();
	bool InitFailed() const { return m_bInitFailed; }

	GuildSubWindow();
	~GuildSubWindow();

	HWND GetHWND() const override { return m_hwnd; }

	void Close() override;

private:
	LRESULT WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
	static WNDCLASS m_wndClass;

private:
	HWND m_hwnd;
	bool m_bInitFailed = false;
};
