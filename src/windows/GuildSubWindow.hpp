#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "WinUtils.hpp"
#include "ChatWindow.hpp"

class MessageList;
class MessageEditor;
class IMemberList;
class IChannelView;
class StatusBar;

extern HINSTANCE g_hInstance;

class GuildSubWindow : public ChatWindow
{
public:
	static bool InitializeClass();

public:
	GuildSubWindow();
	~GuildSubWindow();
	bool InitFailed() const { return m_bInitFailed; }

	HWND GetHWND() const override { return m_hwnd; }
	void SetCurrentGuildID(Snowflake sf) override;
	void SetCurrentChannelID(Snowflake sf) override;

	void Close() override;
	void ProperlySizeControls();
	void UpdateWindowTitle();

	void OnTyping(Snowflake guildID, Snowflake channelID, Snowflake userID, time_t timeStamp) override;
	void OnStopTyping(Snowflake channelID, Snowflake userID) override;

private:
	LRESULT WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
	static WNDCLASS m_wndClass;

private:
	HWND m_hwnd;
	bool m_bInitFailed = false;

	MessageList*   m_pMessageList = nullptr;
	MessageEditor* m_pMessageEditor = nullptr;
	IMemberList*   m_pMemberList = nullptr;
	IChannelView*  m_pChannelView = nullptr;
	StatusBar*     m_pStatusBar = nullptr;

	bool m_bMemberListVisible = false;
	bool m_bChannelListVisible = false;

	Snowflake m_lastGuildID = 0;
	Snowflake m_lastChannelID = 0;

protected:
	friend class StatusBar;

	// Proportions
	int m_ChannelViewListWidth = 0;
	int m_ChannelViewListWidth2 = 0;
	int m_BottomBarHeight = 0;
	int m_BottomInputHeight = 0;
	int m_SendButtonWidth = 0;
	int m_SendButtonHeight = 0;
	int m_GuildHeaderHeight = 0;
	int m_MemberListWidth = 0;
	int m_MessageEditorHeight = 0;
};
