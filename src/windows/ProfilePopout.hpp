#pragma once

#include "Main.hpp"

class RoleList;
class ProfileView;

struct ShowProfilePopoutParams
{
	POINT m_pt;
	Snowflake m_user;
	Snowflake m_guild;
	bool m_bRightJustify;
};

class ProfilePopout
{
public:
	static void Show(Snowflake user, Snowflake guild, int x, int y, bool rightJustify = false);
	static void Dismiss();
	static void Update();
	static Snowflake GetUser();

	// only Main.cpp should use these!
	static HWND GetHWND();
	static void DeferredShow(const ShowProfilePopoutParams& params);

private:
	static void FlushNote();
	static bool Layout(HWND hWnd, SIZE& fullSize);
	static void Paint(HWND hWnd, HDC hDC);
	static INT_PTR CALLBACK Proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	static Snowflake m_user, m_guild;
	static RoleList* m_pRoleList;
	static ProfileView* m_pProfileView;
	static HWND m_hwnd;
	static HBITMAP m_hBitmap;
	static SIZE m_size;
};
