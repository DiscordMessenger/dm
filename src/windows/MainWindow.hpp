#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <map>
#include "WinUtils.hpp"
#include "StatusBar.hpp"
#include "TextManager.hpp"
#include "ChatWindow.hpp"

extern HINSTANCE g_hInstance;

class MessageList;
class GuildLister;
class GuildHeader;
class ProfileView;
class IMemberList;
class IChannelView;
class MessageEditor;
class LoadingMessage;
class DiscordInstance;

struct Guild;
struct Channel;

typedef std::shared_ptr<ChatWindow> ChatWindowPtr;

struct TypingInfo
{
	std::map<Snowflake, TypingUser> m_typingUsers;
};

class MainWindow : public ChatWindow
{
public:
	MainWindow(LPCTSTR pClassName, int nShowCmd);
	~MainWindow();

	// cannot copy the main window.
	MainWindow(const MainWindow& mw) = delete;

	HWND GetHWND() const override {
		return m_hwnd;
	}

	void SetCurrentGuildID(Snowflake sf) override;
	void SetCurrentChannelID(Snowflake sf) override;

	bool InitFailed() const { return m_bInitFailed; }

	bool IsPartOfMainWindow(HWND hWnd);

	MessageEditor* GetMessageEditor() {
		return m_pMessageEditor;
	}

	bool IsChannelListVisible() const override { return m_bChannelListVisible; }
	int GetGuildListerWidth() const override { return m_GuildListerWidth; }
	bool IsMemberListVisible() const { return m_bMemberListVisible; }

	void SetHeartbeatInterval(int timeMs);
	void TryConnectAgainIn(int time);
	void ResetTryAgainInTime();
	void UpdateMainWindowTitle();
	void OnTyping(Snowflake guildID, Snowflake channelID, Snowflake userID, time_t timeStamp);
	void OnStopTyping(Snowflake channelID, Snowflake userID);
	void ProperlySizeControls();
	void AddOrRemoveAppFromStartup();
	void CloseCleanup();
	void OnUpdateAvatar(const std::string& resid);
	int  OnHTTPError(const std::string& url, const std::string& reasonString, bool isSSL);
	void CreateGuildSubWindow(Snowflake guildId, Snowflake channelId);
	void CloseSubWindowByViewID(int viewID);
	void Close();

protected:
	friend class GuildSubWindow;
	friend class Frontend_Win32;

	TypingInfo& GetTypingInfo(Snowflake sf);
	void OnClosedWindow(ChatWindow* ptr);
	std::vector<ChatWindowPtr>& GetSubWindows() { return m_subWindows; }

private:
	void MirrorMessageToSubViewByChannelID(Snowflake channelId, UINT uMsg, WPARAM wParam, LPARAM lParam);
	void UpdateMemberListVisibility();

	LRESULT WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	void TryAgainTimer(HWND hWnd, UINT uMsg, UINT_PTR uTimerID, DWORD dwParam);
	LRESULT HandleCommand(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static void CALLBACK OnHeartbeatTimer(HWND hWnd, UINT uInt, UINT_PTR uIntPtr, DWORD dWord);
	static void CALLBACK TryAgainTimerStatic(HWND hWnd, UINT uMsg, UINT_PTR uTimerID, DWORD dwParam);

private:
	HWND m_hwnd;
	WNDCLASS m_wndClass {};

	bool m_bInitFailed = false;

	StatusBar* m_pStatusBar = nullptr;
	MessageList* m_pMessageList = nullptr;
	ProfileView* m_pProfileView = nullptr;
	GuildHeader* m_pGuildHeader = nullptr;
	GuildLister* m_pGuildLister = nullptr;
	IMemberList* m_pMemberList = nullptr;
	IChannelView* m_pChannelView = nullptr;
	MessageEditor* m_pMessageEditor = nullptr;
	LoadingMessage* m_pLoadingMessage = nullptr;

	Snowflake m_lastGuildID = 0;
	Snowflake m_lastChannelID = 0;

	int m_agerCounter = 0;
	int m_HeartbeatTimerInterval = 0;
	int m_TryAgainTimerInterval = 100;

	UINT_PTR m_HeartbeatTimer = 0;
	UINT_PTR m_TryAgainTimer = 0;

	const UINT_PTR m_TryAgainTimerId = 123456;

	bool m_bMemberListVisible = false;
	bool m_bMemberListVisibleBackup = true;
	bool m_bChannelListVisible = false;

	std::map<Snowflake, TypingInfo> m_typingInfo;

	std::vector<ChatWindowPtr> m_subWindows;

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
	int m_GuildListerWidth = 0;
	int m_GuildListerWidth2 = 0;
	int m_MemberListWidth = 0;
	int m_MessageEditorHeight = 0;
};

MainWindow* GetMainWindow();
HWND GetMainHWND();
DiscordInstance* GetDiscordInstance();
std::string GetDiscordToken();
