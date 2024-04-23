#pragma once

#include <map>
#include <cassert>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "../discord/Snowflake.hpp"

class MessageView
{
public:
	// Initializes and creates the main message view.
	static void Initialize();
	static bool CreateView(HWND hParent, Snowflake channel);
	static void OnClosed(Snowflake channel);

public:
	MessageView(Snowflake chan);
	~MessageView();

	void Activate();

private:
	LRESULT OnEvent(UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static WNDCLASS g_MessageViewClass;

private:
	HWND m_hwnd = NULL;
	Snowflake m_currentChannel = 0;
};

MessageView* GetMainMessageView();
MessageView* GetMessageViewForChannel(Snowflake channel);
