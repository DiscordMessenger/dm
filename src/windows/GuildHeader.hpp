#pragma once

#include "Main.hpp"
#include "../discord/Channel.hpp"

#define T_GUILD_HEADER_CLASS TEXT("GuildHeader")

class GuildHeader
{
public:
	HWND m_hwnd = NULL;

	std::string GetGuildName();// = "Operating System Development";
	std::string GetSubtitleText();// = "Boost Level 3 - 15 Boosts";
	std::string GetChannelName();// = "#fictional-channel";
	std::string GetChannelInfo();// = "This is an entirely fictional channel that I made up!";

public:
	GuildHeader();
	~GuildHeader();

	void Update();

public:
	static WNDCLASS g_GuildHeaderClass;

	static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static void InitializeClass();

	static GuildHeader* Create(HWND hwnd, LPRECT pRect);

private:
	struct Button {
		RECT m_rect;
		bool m_hot = false;
		bool m_held = false;
		int m_iconID;
		int m_buttonID;
		bool m_bRightJustify;

		Button(int bid, int iid, bool rj = true) : m_iconID(iid), m_buttonID(bid), m_bRightJustify(rj) {}
	};

	HICON m_channelIcon;
	HICON m_voiceIcon;
	HICON m_dmIcon;
	HICON m_groupDmIcon;

	// layout
	RECT m_fullRect;
	RECT m_rectLeft;
	RECT m_rectMidFull;
	RECT m_rectMid;
	RECT m_rectRight;

	std::vector<Button> m_buttons;
	bool m_bLClickHeld = false;

	LONG m_oldWidth = 0;
	LONG m_minRightToolbarX = 0;

	void Layout();

	HICON GetIconFromType(Channel::eChannelType);
	static void LayoutButton(Button& button, RECT& toolbarRect);
	static void DrawButton(HDC hdc, Button& button);
	static void HitTestButton(HDC hdc, Button& button, POINT& pt);
	static void CheckClickButton(HDC hdc, Button& button, POINT& pt);
	static void CheckReleaseButton(HDC hdc, HWND hWnd, Button& button, int buttonIndex, POINT& pt);
};

