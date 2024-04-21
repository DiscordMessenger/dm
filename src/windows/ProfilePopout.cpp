#include "ProfilePopout.hpp"
#include "RoleList.hpp"

HWND g_ProfilePopoutHwnd;
Snowflake g_ProfilePopoutUser;
Snowflake g_ProfilePopoutGuild;

RoleList* g_pRoleList;

Snowflake GetProfilePopoutUser()
{
	return g_ProfilePopoutUser;
}

bool ProfileViewerSetProfileData(HWND hWnd)
{
	Profile* pProf = GetProfileCache()->LookupProfile(g_ProfilePopoutUser, "...", "...", "");
	if (!pProf) {
		EndDialog(hWnd, 0);
		return TRUE;
	}

	LPCTSTR Str;

	GuildMember* gm = nullptr;
	
	if (pProf->HasGuildMemberProfile(g_ProfilePopoutGuild))
		gm = &pProf->m_guildMembers[g_ProfilePopoutGuild];

	Str = ConvertCppStringToTString(pProf->GetName(g_ProfilePopoutGuild));
	HWND item = GetDlgItem(hWnd, IDC_GLOBAL_NAME);
	SetWindowText(item, Str);

	free((void*)Str);
	SendMessage(item, WM_SETFONT, (WPARAM)g_AccountInfoFont, 0);

	Str = ConvertCppStringToTString(pProf->m_name);
	item = GetDlgItem(hWnd, IDC_USERNAME);
	SetWindowText(item, Str);
	
	if (pProf->m_bIsBot)
	{
		RECT rc{};
		HDC hdc = GetDC(hWnd);
		HGDIOBJ old = SelectObject(hdc, g_AccountTagFont);
		DrawText(hdc, Str, -1, &rc, DT_CALCRECT | DT_NOPREFIX);
		SelectObject(hdc, old);
		ReleaseDC(hWnd, hdc);

		int width = rc.right - rc.left + ScaleByDPI(5);
		RECT rcWnd{};
		GetWindowRect(item, &rcWnd);
		ScreenToClientRect(hWnd, &rcWnd);
		MoveWindow(item, rcWnd.left, rcWnd.top, width, rcWnd.bottom - rcWnd.top, FALSE);

		int iconSize = ScaleByDPI(16);
		int y = rcWnd.top + (rcWnd.bottom - rcWnd.top) / 2 - iconSize / 2;

		HWND botIcon = CreateWindow(
			TEXT("STATIC"),
			NULL,
			WS_CHILD | WS_VISIBLE | SS_ICON | SS_REALSIZECONTROL,
			rcWnd.left + width,
			y,
			iconSize,
			iconSize,
			hWnd,
			(HMENU)0,
			g_hInstance,
			NULL
		);

		Static_SetIcon(botIcon, g_BotIcon);
	}

	free((void*)Str);
	SendMessage(item, WM_SETFONT, (WPARAM)g_AccountTagFont, 0);

	Str = ConvertCppStringToTString(pProf->GetStatus(g_ProfilePopoutGuild));
	item = GetDlgItem(hWnd, IDC_PROFILE_STATUS);
	SetWindowText(item, Str);
	free((void*)Str);
	SendMessage(item, WM_SETFONT, (WPARAM)g_DateTextFont, 0);

	HWND roleWnd = GetDlgItem(hWnd, IDC_ROLE_STATIC);
	RECT rect{};
	GetWindowRect(roleWnd, &rect);
	ScreenToClientRect(hWnd, &rect);
	DestroyWindow(roleWnd);

	RoleList::InitializeClass();
	g_pRoleList = RoleList::Create(hWnd, &rect, IDC_ROLE_STATIC);
	
	Guild* gld = GetDiscordInstance()->GetGuild(g_ProfilePopoutGuild);
	if (gld && gm) {
		std::vector<GuildRole> grs;
		for (Snowflake role : gm->m_roles) {
			grs.push_back(gld->m_roles[role]);
		}
		std::sort(grs.begin(), grs.end());
		for (auto& gr : grs) {
			g_pRoleList->AddRole(gr);
		}
	}
	g_pRoleList->Update();

	item = GetDlgItem(hWnd, IDC_STATIC_PROFILE_IMAGE);
	SendMessage(item, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)GetAvatarCache()->GetBitmap(pProf->m_avatarlnk));

	item = GetDlgItem(hWnd, IDC_GUILD_JOIN_DATE);
	if (gm) {
		Str = ConvertCppStringToTString(FormatDate(gm->m_joinedAt));
		SetWindowText(item, Str);
	}
	else {
		SetWindowText(item, TEXT(""));
		item = GetDlgItem(hWnd, IDC_ICON_GUILD);
		ShowWindow(item, SW_HIDE);
	}
	item = GetDlgItem(hWnd, IDC_DISCORD_JOIN_DATE);
	Str = ConvertCppStringToTString(FormatDate(ExtractTimestamp(pProf->m_snowflake) / 1000));
	SetWindowText(item, Str);
	return FALSE;
}

BOOL CALLBACK ProfileViewerProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_PAINT:
		{
			PAINTSTRUCT ps = {};
			RECT rect = {};
			HDC hdc = BeginPaint(hWnd, &ps);
			GetClientRect(hWnd, &rect);

			DrawEdge(hdc, &rect, EDGE_RAISED, BF_RECT);

			EndPaint(hWnd, &ps);
			break;
		}

		case WM_INITDIALOG:
		case WM_UPDATEPROFILEPOPOUT:
			return ProfileViewerSetProfileData(hWnd);

		case WM_DESTROY:
			g_ProfilePopoutHwnd = NULL;
			delete g_pRoleList;
			g_pRoleList = NULL;
			return TRUE;
	}

	return FALSE;
}

void DismissProfilePopout()
{
	if (g_ProfilePopoutHwnd)
	{
		DestroyWindow(g_ProfilePopoutHwnd);
		g_ProfilePopoutHwnd = NULL;
	}
}

void DeferredShowProfilePopout(const ShowProfilePopoutParams& params)
{
	Snowflake user = params.m_user;
	Snowflake guild = params.m_guild;
	int x = params.m_pt.x;
	int y = params.m_pt.y;
	bool bRightJustify = params.m_bRightJustify;

	Profile* pf = GetProfileCache()->LookupProfile(user, "", "", "", false);
	if (!pf)
		return; // that profile is nonexistent!

	DismissProfilePopout();
	g_ProfilePopoutUser = user;
	g_ProfilePopoutGuild = guild;
	
	g_ProfilePopoutHwnd = CreateDialog(g_hInstance, MAKEINTRESOURCE(IDD_DIALOG_PROFILE_POPOUT), g_Hwnd, ProfileViewerProc);

	RECT rcClient{};
	GetClientRect(g_ProfilePopoutHwnd, &rcClient);
	int wndWidth = rcClient.right - rcClient.left;
	int wndHeight = rcClient.bottom - rcClient.top;

	if (bRightJustify) {
		x -= wndWidth;
	}

	MoveWindow(g_ProfilePopoutHwnd, x, y, wndWidth, wndHeight, false);
	ShowWindow(g_ProfilePopoutHwnd, SW_SHOWNOACTIVATE);
}

void ShowProfilePopout(Snowflake user, Snowflake guild, int x, int y, bool bRightJustify)
{
	ShowProfilePopoutParams* parms = new ShowProfilePopoutParams;
	parms->m_user = user;
	parms->m_guild = guild;
	parms->m_pt = { x, y };
	parms->m_bRightJustify = bRightJustify;

	// N.B. The parms struct is now owned by the main window.
	// Don't send immediately (the message gets enqueued instead). It'd be useless to do
	// it like that as it ends up just calling the deferred version directly
	if (!PostMessage(g_Hwnd, WM_SHOWPROFILEPOPOUT, 0, (LPARAM) parms))
		delete parms;
}

void UpdateProfilePopout()
{
	SendMessage(g_ProfilePopoutHwnd, WM_UPDATEPROFILEPOPOUT, 0, 0);
}
