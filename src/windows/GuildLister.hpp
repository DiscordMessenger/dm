#pragma once

#include "Main.hpp"

#define T_GUILD_LISTER_PARENT_CLASS TEXT("GuildListerParent")
#define T_GUILD_LISTER_CLASS TEXT("GuildLister")

// config
#define GUILD_LISTER_BORDER 1

#if GUILD_LISTER_BORDER
# define GUILD_LISTER_BORDER_SIZE 2
#else
# define GUILD_LISTER_BORDER_SIZE 0
#endif

class GuildLister
{
public:
	HWND m_hwnd            = NULL;
	HWND m_scrollable_hwnd = NULL;
	HWND m_tooltip_hwnd    = NULL;

	int m_scroll_pos = 0;

	Snowflake m_rightClickedGuild = 0;
	Snowflake m_selectedGuild = 0; // kept in store to ensure that things don't flicker too much

	std::map<Snowflake, RECT> m_iconRects;

public:
	GuildLister();
	~GuildLister();

public:
	void ProperlyResizeSubWindows();
	void ClearTooltips();
	void UpdateTooltips();
	void Update();
	void UpdateSelected();
	void OnScroll();
	void ShowMenu(Snowflake guild, POINT pt);
	void AskLeave(Snowflake guild);
	void RedrawIconForGuild(Snowflake guild);

private:
	void DrawServerIcon(HDC hdc, HBITMAP hicon, int& y, RECT& rect, Snowflake id, const std::string& textOver, bool hasAlpha);

public:
	static WNDCLASS g_GuildListerParentClass;
	static WNDCLASS g_GuildListerClass;

	static LRESULT CALLBACK ParentWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static void InitializeClass();

	static GuildLister* Create(HWND hwnd, LPRECT pRect);

private:
	static int GetScrollableHeight();
};

