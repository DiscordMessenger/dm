#pragma once
#include "Main.hpp"

#define T_ROLE_LIST_CLASS TEXT("RoleList")

struct RoleItem
{
	GuildRole m_role;
	RECT m_rect;
	LPTSTR m_text = NULL;
	
	LPTSTR GetText() {
		if (!m_text)
			m_text = ConvertCppStringToTString(m_role.m_name);
		return m_text;
	}

	~RoleItem() {
		if (m_text)
			free(m_text);
	}
};

class RoleList
{
public:
	HWND m_hwnd = NULL;
	int m_scrollY = 0;
	int m_scrollHeight = 0;
	bool m_bHasBorder = false;

	std::vector<RoleItem> m_items;

public:
	RoleList();
	~RoleList();

	void Update();
	void AddRole(GuildRole& role);
	void ClearRoles();
	void LayoutRoles();
	int GetRolesHeight();

public:
	static WNDCLASS g_RoleListClass;

	static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static void InitializeClass();

	static RoleList* Create(HWND hwnd, LPRECT pRect, int comboId = 0, bool border = false, bool visible = true);

private:
	void DrawRole(HDC hdc, RoleItem* role);
	void DrawShinyRoleColor(HDC hdc, const LPRECT pRect, COLORREF base);
};
