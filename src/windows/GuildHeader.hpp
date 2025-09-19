#pragma once

#include <map>
#include "Main.hpp"
#include "../discord/Channel.hpp"
#include "../discord/FormattedText.hpp"

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
	enum eButtonPlacement {
		BUTTON_RIGHT,       // To the right of the channel name section
		BUTTON_LEFT,        // To the left of the channel name section
		BUTTON_GUILD_RIGHT, // To the right of the guild name section
		BUTTON_GUILD_LEFT,  // To the left of the guild name section
	};

	static bool IsPlacementGuildType(eButtonPlacement pl) {
		return pl == BUTTON_GUILD_LEFT || pl == BUTTON_GUILD_RIGHT;
	}

	static bool IsPlacementChannelType(eButtonPlacement pl) {
		return pl == BUTTON_RIGHT || pl == BUTTON_LEFT;
	}

	struct Button {
		RECT m_rect;
		bool m_hot = false;
		bool m_held = false;
		int m_iconID;
		int m_buttonID;
		eButtonPlacement m_placement = BUTTON_RIGHT;

		Button(int bid, int iid, eButtonPlacement pl = BUTTON_RIGHT):
			m_iconID(iid), m_buttonID(bid), m_placement(pl) {}
	};

	HICON m_channelIcon;
	HICON m_voiceIcon;
	HICON m_dmIcon;
	HICON m_groupDmIcon;

	// layout
	RECT m_fullRect;
	RECT m_rectLeft;
	RECT m_rectLeftFull;
	RECT m_rectMid;
	RECT m_rectMidFull;
	RECT m_rectRight;
	RECT m_rectRightFull;

	std::vector<Button> m_buttons;
	bool m_bLClickHeld = false;

	LONG m_oldWidth = 0;
	LONG m_guildTextWidth = 0;
	LONG m_minRightToolbarX = 0;

	std::map<int, HICON> m_hIcons;
	
	std::string m_currentChannelDescription;
	FormattedText m_channelDescription;

	void Layout();

	HICON GetIconFromType(Channel::eChannelType);
	HICON GetIcon(int iconID, int iconSize);
	void LayoutButton(Button& button, RECT& chanNameRect, RECT& guildNameRect);
	void DrawButton(HDC hdc, Button& button);
	void HitTestButton(HDC hdc, Button& button, POINT& pt);
	void CheckClickButton(HDC hdc, Button& button, POINT& pt);
	void CheckReleaseButton(HDC hdc, HWND hWnd, Button& button, int buttonIndex, POINT& pt);
};

