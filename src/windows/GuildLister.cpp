#include "GuildLister.hpp"
#include "../discord/LocalSettings.hpp"

#define C_GUILD_ICON_WIDTH (PROFILE_PICTURE_SIZE + 4)
#define C_GUILD_GAP_HEIGHT 5

#if GUILD_LISTER_BORDER
# define GUILD_LISTER_COLOR COLOR_SCROLLBAR
#else
# define GUILD_LISTER_COLOR COLOR_3DFACE
#endif

#define BORDER_SIZE GUILD_LISTER_BORDER_SIZE

WNDCLASS GuildLister::g_GuildListerClass;
WNDCLASS GuildLister::g_GuildListerParentClass;
GuildLister::GuildLister() {}

extern int g_GuildListerWidth;

enum {
	GCMD_MORE,
	GCMD_BAR,
};

GuildLister::~GuildLister()
{
	if (m_hwnd)
	{
		BOOL b = DestroyWindow(m_hwnd);
		assert(b && "was window already destroyed?");
		m_hwnd = NULL;
	}

	assert(!m_bar_btn_hwnd);
	assert(!m_tooltip_hwnd);
	assert(!m_scrollable_hwnd);
}

static int GetProfileBorderRenderSize()
{
	return ScaleByDPI(Supports32BitIcons() ? (PROFILE_PICTURE_SIZE_DEF + 12) : 64);
}

void GuildLister::ProperlyResizeSubWindows()
{
	RECT rect, rect2;
	GetClientRect(m_hwnd, &rect);
	rect2 = rect;

	int buttonHeight = ScaleByDPI(24);

	bool wasScrollBarVisible = m_bIsScrollBarVisible;
	m_bIsScrollBarVisible = rect.right - rect.left > g_GuildListerWidth;

	if (wasScrollBarVisible != m_bIsScrollBarVisible) {
		if (wasScrollBarVisible) {
			SaveScrollInfo();
			ShowScrollBar(m_scrollable_hwnd, SB_VERT, FALSE);
			MoveWindow(m_scrollable_hwnd, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top - buttonHeight, true);
		}
		else {
			MoveWindow(m_scrollable_hwnd, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top - buttonHeight, true);
			ShowScrollBar(m_scrollable_hwnd, SB_VERT, TRUE);
			RestoreScrollInfo();
		}
	}
	else {
		MoveWindow(m_scrollable_hwnd, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top - buttonHeight, true);
	}

	rect = rect2;

	int buttonWidth = (rect.right - rect.left) / 2;

	if (m_more_btn_hwnd) {
		MoveWindow(m_more_btn_hwnd, rect.left, rect.bottom - buttonHeight, buttonWidth, buttonHeight, true);
		MoveWindow(m_bar_btn_hwnd,  rect.right - buttonWidth, rect.bottom - buttonHeight, buttonWidth, buttonHeight, true);
	}
	else {
		MoveWindow(m_bar_btn_hwnd, rect.left, rect.bottom - buttonHeight, rect.right - rect.left, buttonHeight, true);
	}

	UpdateTooltips();
}

void GuildLister::ClearTooltips()
{
	// Remove all tools from the tooltip control
	TOOLINFO toolInfo = { 0 };
	toolInfo.cbSize = sizeof(TOOLINFO);
	toolInfo.hwnd = m_tooltip_hwnd;

	// Iterate through each tool and remove it
	while (SendMessage(m_tooltip_hwnd, TTM_ENUMTOOLS, 0, (LPARAM)&toolInfo))
		SendMessage(m_tooltip_hwnd, TTM_DELTOOL, 0, (LPARAM)&toolInfo);
}

void GuildLister::UpdateTooltips()
{
	ClearTooltips();

	RECT rect = {};
	GetClientRect(m_scrollable_hwnd, &rect);

	SCROLLINFO si{};
	si.cbSize = sizeof(si);
	si.fMask = SIF_POS;
	GetScrollInfo(&si);

	int y = -si.nPos;
	int height = ScaleByDPI(PROFILE_PICTURE_SIZE_DEF + 12);

	TRACKMOUSEEVENT tme;
	tme.cbSize = sizeof tme;
	tme.dwFlags = TME_LEAVE | TME_HOVER;
	tme.hwndTrack = m_scrollable_hwnd;
	tme.dwHoverTime = HOVER_DEFAULT;
	ri::TrackMouseEvent(&tme);

	Snowflake currentFolder = 0;
	std::vector<Snowflake> sfs;
	GetDiscordInstance()->GetGuildIDsOrdered(sfs, true);
	int idx = 1;
	for (auto sf : sfs)
	{
		if (sf == 1)
		{
			y += C_GUILD_GAP_HEIGHT;
			continue;
		}

		std::string name = "";
		if (sf & BIT_FOLDER)
		{
			name = GetDiscordInstance()->GetGuildFolderName(sf & ~BIT_FOLDER);
			currentFolder = sf & ~BIT_FOLDER;

			if (sf == BIT_FOLDER) {
				// skip
				continue;
			}
		}
		else if (currentFolder && !m_openFolders[currentFolder]) {
			// skip items in closed folders.
			continue;
		}

		RECT r = {
			rect.left + ScaleByDPI(6),
			rect.top + y + ScaleByDPI(4),
			rect.right - ScaleByDPI(6),
			rect.top + y + ScaleByDPI(4) + GetProfilePictureSize()
		};

		Guild* pGld = GetDiscordInstance()->GetGuild(sf);
		if (pGld) {
			name = pGld->m_name;
		}

		LPTSTR tstr = ConvertCppStringToTString(name);
		TOOLINFO ti{};
		ti.cbSize = sizeof(ti);
		ti.hwnd = m_scrollable_hwnd;
		ti.hinst = g_hInstance;
		ti.lpszText = tstr;
		ti.uFlags = TTF_CENTERTIP | TTF_SUBCLASS;
		ti.uId = idx;
		ti.rect = r;
		idx++;

		(void) SendMessage(m_tooltip_hwnd, TTM_ADDTOOL, 0, (LPARAM)&ti);
		free(tstr);

		y += height;
	}
}

void GuildLister::Update()
{
	InvalidateRect(m_hwnd, NULL, false);
	UpdateTooltips();
	UpdateScrollBar(false);
}

void GuildLister::UpdateSelected()
{
	UpdateScrollBar(false);

	auto it = m_iconRects.find(m_selectedGuild);
	if (it != m_iconRects.end()) {
		InvalidateRect(m_hwnd, &it->second, FALSE);
	}

	m_selectedGuild = GetDiscordInstance()->GetCurrentGuildID();
	it = m_iconRects.find(m_selectedGuild);
	if (it != m_iconRects.end()) {
		InvalidateRect(m_hwnd, &it->second, FALSE);
	}
}

int GuildLister::UpdateScrollBar(bool setOldPos)
{
	RECT rcClient{};
	GetClientRect(m_scrollable_hwnd, &rcClient);

	SCROLLINFO si{};
	si.cbSize = sizeof si;
	si.fMask = SIF_POS;
	GetScrollInfo(&si);
	int oldPos = si.nPos;
	si.fMask |= SIF_RANGE | SIF_PAGE;
	si.nMin = 0;
	si.nMax = GetScrollableHeight();
	si.nPage = rcClient.bottom - rcClient.top;
	SetScrollInfo(&si);
	GetScrollInfo(&si);
	if (setOldPos) m_oldPos = si.nPos;
	return si.nPos - oldPos;
}

void GuildLister::GetScrollInfo(SCROLLINFO* pInfo)
{
	if (m_bIsScrollBarVisible) {
		ri::GetScrollInfo(m_scrollable_hwnd, SB_VERT, pInfo);
		return;
	}

	if (pInfo->fMask & SIF_RANGE) {
		pInfo->nMin = m_simulatedScrollInfo.nMin;
		pInfo->nMax = m_simulatedScrollInfo.nMax;
	}

	if (pInfo->fMask & SIF_PAGE) {
		pInfo->nPage = m_simulatedScrollInfo.nPage;
	}

	if (pInfo->fMask & SIF_POS) {
		pInfo->nPos = m_simulatedScrollInfo.nPos;
	}

	if (pInfo->fMask & SIF_TRACKPOS) {
		pInfo->nTrackPos = m_simulatedScrollInfo.nTrackPos;
	}
}

void GuildLister::SetScrollInfo(SCROLLINFO* pInfo, bool redraw)
{
	if (m_bIsScrollBarVisible) {
		ri::SetScrollInfo(m_scrollable_hwnd, SB_VERT, pInfo, redraw);
		::ShowScrollBar(m_scrollable_hwnd, SB_VERT, TRUE);
		return;
	}

	SCROLLINFO& si = m_simulatedScrollInfo;

	if (pInfo->fMask & SIF_RANGE) {
		si.nMin = pInfo->nMin;
		si.nMax = pInfo->nMax;
	}

	if (pInfo->fMask & SIF_PAGE) {
		si.nPage = pInfo->nPage;
		if (si.nPage < 0) si.nPage = 0;
		if (si.nPage > (UINT)(si.nMax - si.nMin + 1)) si.nPage = (UINT)(si.nMax - si.nMin + 1);
	}

	if (pInfo->fMask & SIF_POS) {
		int& pos = m_simulatedScrollInfo.nPos;
		pos = pInfo->nPos;

		if (pos < si.nMin)
			pos = si.nMin;
		if (pos > si.nMax - (int) std::max(si.nPage - 1, 0U))
			pos = si.nMax - (int) std::max(si.nPage - 1, 0U);
	}
}

void GuildLister::SaveScrollInfo()
{
	SCROLLINFO& si = m_simulatedScrollInfo;
	si.cbSize = sizeof m_simulatedScrollInfo;
	si.fMask = SIF_ALL;
	ri::GetScrollInfo(m_scrollable_hwnd, SB_VERT, &si);
}

void GuildLister::RestoreScrollInfo()
{
	RECT rcClient{};
	GetClientRect(m_scrollable_hwnd, &rcClient);
	SCROLLINFO& si = m_simulatedScrollInfo;
	si.cbSize = sizeof si;
	si.fMask = SIF_ALL;
	si.nMin = 0;
	si.nMax = GetScrollableHeight();
	si.nPage = rcClient.bottom - rcClient.top;
	ri::SetScrollInfo(m_scrollable_hwnd, SB_VERT, &m_simulatedScrollInfo, TRUE);
}

void GuildLister::ShowGuildChooserMenu()
{
	DialogBox(g_hInstance, MAKEINTRESOURCE(DMDI(IDD_DIALOG_GUILD_CHOOSER)), g_Hwnd, &ChooserDlgProc);
}

void GuildLister::OnScroll()
{
	UpdateTooltips();
}

void GuildLister::ShowMenu(Snowflake guild, POINT pt)
{
	Guild* pGuild = GetDiscordInstance()->GetGuild(guild);
	Profile* pf = GetDiscordInstance()->GetProfile();
	if (!pGuild) return;
	if (!pf) return;

	m_rightClickedGuild = guild;
	HMENU menu = GetSubMenu(LoadMenu(g_hInstance, MAKEINTRESOURCE(IDR_GUILD_CONTEXT)), 0);

	// Check if all channels are read
	bool bHasAllRead = true;
	bool bHasInvitableChannel = false;
	bool bHasServerSettings = false;
	bool bHasChangeNickname = false;
	bool bHasManageChannels = false;
	for (auto& chn : pGuild->m_channels)
	{
		if (chn.HasPermission(PERM_VIEW_CHANNEL) && chn.HasUnreadMessages())
			bHasAllRead = false;

		if (chn.HasPermission(PERM_CREATE_INSTANT_INVITE))
			bHasInvitableChannel = true;

		// If no more point in scanning, break.
		if (!bHasAllRead && bHasInvitableChannel)
			break;
	}

	// TODO: Add server settings dialog, permissions
	// TODO: Is this right?
	constexpr uint64_t PermsForServerSettings =
		PERM_MANAGE_GUILD |
		PERM_MANAGE_GUILD_EXPRESSIONS |
		PERM_MANAGE_CHANNELS |
		PERM_MANAGE_ROLES;
	for (auto& rolid : pf->m_guildMembers[guild].m_roles)
	{
		GuildRole& rol = pGuild->m_roles[rolid];

		if (rol.m_permissions & PermsForServerSettings)
			bHasServerSettings = true;

		if (rol.m_permissions & PERM_CHANGE_NICKNAME)
			bHasChangeNickname = true;

		if (rol.m_permissions & PERM_MANAGE_CHANNELS)
			bHasManageChannels = true;

		if (bHasServerSettings && bHasChangeNickname && bHasManageChannels)
			break;
	}

	// Disable unimplemented items
	EnableMenuItem(menu, IDM_INVITEPEOPLE,    MF_GRAYED);
	EnableMenuItem(menu, IDM_MUTESERVER,      MF_GRAYED); // TODO: Not sure how to fetch that.
	EnableMenuItem(menu, IDM_GUILDSETTINGS,   MF_GRAYED);
	EnableMenuItem(menu, IDM_PRIVACYSETTINGS, MF_GRAYED);
	EnableMenuItem(menu, IDM_EDITNICKNAME,    MF_GRAYED);
	EnableMenuItem(menu, IDM_CREATECHAN,      MF_GRAYED);
	EnableMenuItem(menu, IDM_CREATECATEG,     MF_GRAYED);

	EnableMenuItem(menu, IDM_NOTIFYALL,       MF_GRAYED); // Same here
	EnableMenuItem(menu, IDM_NOTIFYMENTS,     MF_GRAYED);
	EnableMenuItem(menu, IDM_NOTIFYNONE,      MF_GRAYED);
	EnableMenuItem(menu, IDM_NOTIFYSUPPRGLOB, MF_GRAYED);
	EnableMenuItem(menu, IDM_NOTIFYSUPPRROLE, MF_GRAYED);

	// TODO: ri::RemoveMenu impl
	int separator_invitepeople  = 1;
	int separator_muteserver    = 3;
	int separator_guildsettings = 6;
	int separator_createchannel = 10;
	int separator_leaveserver   = 13;
	int separator_copyserverid  = 15;

	// Remove separators first...
	if (pf->m_snowflake == pGuild->m_ownerId) {
		RemoveMenu(menu, separator_leaveserver, MF_BYPOSITION);
		separator_copyserverid--;
	}
	if (!bHasManageChannels) {
		RemoveMenu(menu, separator_createchannel, MF_BYPOSITION);
		separator_leaveserver--;
		separator_copyserverid--;
	}

	// ... And now the actual items - this makes management easier I think
	if (pf->m_snowflake == pGuild->m_ownerId) {
		RemoveMenu(menu, IDM_LEAVEGUILD, MF_BYCOMMAND);
	}

	if (!bHasManageChannels) {
		RemoveMenu(menu, IDM_CREATECHAN,    MF_BYCOMMAND);
		RemoveMenu(menu, IDM_CREATECATEG,   MF_BYCOMMAND);
	}

	if (!bHasServerSettings)
		RemoveMenu(menu, IDM_GUILDSETTINGS, MF_BYCOMMAND);
	
	if (!bHasChangeNickname)
		RemoveMenu(menu, IDM_EDITNICKNAME,  MF_BYCOMMAND);

	EnableMenuItem(menu, IDM_MARKASREAD,    !bHasAllRead         ? MF_ENABLED : MF_GRAYED);
	//EnableMenuItem(menu, IDM_INVITEPEOPLE,  bHasInvitableChannel ? MF_ENABLED : MF_GRAYED);

	TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_scrollable_hwnd, NULL);
}

void GuildLister::AskLeave(Snowflake guild)
{
	Guild* pGuild = GetDiscordInstance()->GetGuild(guild);
	if (!pGuild)
		return;

	TCHAR buff[4096];
	WAsnprintf(buff, _countof(buff), TmGetTString(IDS_CONFIRM_LEAVE_GUILD), pGuild->m_name.c_str());

	if (MessageBox(g_Hwnd, buff, TmGetTString(IDS_PROGRAM_NAME), MB_YESNO | MB_ICONQUESTION) == IDYES)
	{
		// Leave Server
		GetDiscordInstance()->RequestLeaveGuild(guild);
	}
}

void GuildLister::RedrawIconForGuild(Snowflake guild)
{
	InvalidateRect(m_hwnd, &m_iconRects[guild], TRUE);
}

LRESULT CALLBACK GuildLister::ParentWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	GuildLister* pThis = (GuildLister*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	switch (uMsg)
	{
		case WM_SIZE:
		{
			pThis->ProperlyResizeSubWindows();
			break;
		}
		case WM_NCCREATE:
		{
			CREATESTRUCT* strct = (CREATESTRUCT*)lParam;

			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)strct->lpCreateParams);

			break;
		}
		case WM_COMMAND:
		{
			switch (LOWORD(wParam))
			{
				case GCMD_MORE:
					pThis->ShowGuildChooserMenu();
					break;

				case GCMD_BAR:
					GetLocalSettings()->SetShowScrollBarOnGuildList(
						!GetLocalSettings()->ShowScrollBarOnGuildList()
					);
					SendMessage(g_Hwnd, WM_REPOSITIONEVERYTHING, 0, 0);
					break;
			}
		}
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void GuildLister::DrawServerIcon(HDC hdc, HBITMAP hicon, int& y, RECT& rect, Snowflake id, const std::string& textOver, bool hasAlpha, Snowflake currentFolder, bool isLastItem)
{
	int height = 0;
	int pfpSize = GetProfilePictureSize();
	int pfpBorderSize = ScaleByDPI(PROFILE_PICTURE_SIZE_DEF + 12);
	bool isCurrent = GetDiscordInstance()->GetCurrentGuildID() == id;

	SetRectEmpty(&m_iconRects[id]);

	bool isFolderIcon = currentFolder && (id & ~BIT_FOLDER) == currentFolder;

	m_selectedGuild = GetDiscordInstance()->GetCurrentGuildID();
	if (hdc && hicon)
	{
		HICON hborder = NULL;

		if (!NT31SimplifiedInterface()) {
			if (isCurrent)
				hborder = g_ProfileBorderIconGold;
			else
				hborder = g_ProfileBorderIcon;
		}

		RECT rcProfile = {
			rect.left + BORDER_SIZE,
			rect.top  + y,
			rect.left + pfpBorderSize + BORDER_SIZE,
			rect.top  + y + pfpBorderSize + BORDER_SIZE * 2
		};

		m_iconRects[id] = rcProfile;

		int pfpBorderSize2 = GetProfileBorderRenderSize();

		FillRect(hdc, &rcProfile, ri::GetSysColorBrush(GUILD_LISTER_COLOR));

		if (currentFolder) {
			HRGN hrgn = CreateRectRgnIndirect(&rcProfile);
			SelectClipRgn(hdc, hrgn);
			int amount = 0;

			if (m_openFolders[currentFolder]) {
				amount = 15;
			}

			HGDIOBJ oldBrush, oldPen;
			oldBrush = SelectObject(hdc, ri::GetSysColorBrush(COLOR_BTNSHADOW));
			oldPen = SelectObject(hdc, GetStockObject(BLACK_PEN));

			if (currentFolder == (id & ~BIT_FOLDER)) {
				RoundRect(hdc, rcProfile.left + 1, rcProfile.top + 1,  rcProfile.right - 1, rcProfile.top - 1 + pfpBorderSize + amount, 10, 10);
			}
			else if (isLastItem) {
				RoundRect(hdc, rcProfile.left + 1, rcProfile.top - 14, rcProfile.right - 1, rcProfile.top - 1 + pfpBorderSize, 10, 10);
			}
			else {
				RoundRect(hdc, rcProfile.left + 1, rcProfile.top - 14, rcProfile.right - 1, rcProfile.bottom - 1 + 15, 10, 10);
			}

			SelectObject(hdc, oldPen);
			SelectObject(hdc, oldBrush);

			SelectClipRgn(hdc, NULL);
			DeleteRgn(hrgn);
		}

		if (!isFolderIcon) {
			if (hborder)
				ri::DrawIconEx(hdc, rect.left + BORDER_SIZE, rect.top + BORDER_SIZE + y, hborder, pfpBorderSize2, pfpBorderSize2, 0, NULL, DI_COMPAT | DI_NORMAL);
			DrawBitmap(hdc, hicon, rect.left + BORDER_SIZE + ScaleByDPI(6), rect.top + BORDER_SIZE + y + ScaleByDPI(4), NULL, CLR_NONE, GetProfilePictureSize(), GetProfilePictureSize(), hasAlpha);
		}

		height = pfpBorderSize;
	}
	else if (hicon)
	{
		height = pfpBorderSize;

		RECT rcProfile = {
			rect.left + BORDER_SIZE,
			rect.top + y,
			rect.left + pfpBorderSize + BORDER_SIZE * 2,
			rect.top + y + pfpBorderSize + BORDER_SIZE * 2
		};
	}
	
	RECT rcProfile = {
		rect.left + BORDER_SIZE + ScaleByDPI(6),
		rect.top  + y + BORDER_SIZE + ScaleByDPI(4),
		rect.left + BORDER_SIZE + pfpBorderSize - ScaleByDPI(6),
		rect.top  + y + pfpBorderSize + BORDER_SIZE * 2 - ScaleByDPI(10)
	};

	if (hdc && isFolderIcon)
	{
		int smIconSz = GetSystemMetrics(SM_CXICON);
		HICON hic;
		if (m_openFolders[currentFolder])
			hic = g_folderOpenIcon;
		else
			hic = g_folderClosedIcon;

		DrawIcon(hdc, rcProfile.left + (rcProfile.right - rcProfile.left - smIconSz) / 2, rcProfile.top + (rcProfile.bottom - rcProfile.top - smIconSz) / 2, hic);
	}
	else if (hdc && !textOver.empty())
	{
		int mod = SetBkMode(hdc, TRANSPARENT);
		COLORREF clr = SetTextColor(hdc, RGB(255, 255, 255));
		HGDIOBJ go = SelectObject(hdc, g_AuthorTextFont);
		LPTSTR tstr = ConvertCppStringToTString(textOver);
		DrawText(hdc, tstr, -1, &rcProfile, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | ri::GetWordEllipsisFlag());
		free(tstr);
		SelectObject(hdc, go);
		SetTextColor(hdc, clr);
		SetBkMode(hdc, mod);
	}

	y += height;
}

int GuildLister::GetScrollableHeight()
{
	Snowflake currentFolder = 0;
	std::vector<Snowflake> sfs;
	GetDiscordInstance()->GetGuildIDsOrdered(sfs, true);
	int pfpSize = ScaleByDPI(PROFILE_PICTURE_SIZE_DEF + 12);
	int height = 0;
	for (auto sf : sfs)
	{
		if (sf == 1) {
			height += C_GUILD_GAP_HEIGHT;
			continue;
		}

		if (sf & BIT_FOLDER) {
			currentFolder = sf & ~BIT_FOLDER;
			if (sf == BIT_FOLDER) {
				// Skip the folder terminator
				continue;
			}
		}
		else if (currentFolder && !m_openFolders[currentFolder])
		{
			// Skip items in closed folders.
			continue;
		}

		height += pfpSize;
	}

	//take out gap
	return height;
}

std::string MakeIconStringFromName(const std::string& name)
{
	std::string result = "";
	bool addAlNums = true;

	for (char chr : name)
	{
		if (isalnum(chr))
		{
			if (!addAlNums)
				continue;
			addAlNums = false;
		}
		else addAlNums = true;

		if (chr == ' ') {
			// Discord seems to have an exception for just "'s". Strip off the 's if needed.
			if (result.size() >= 2 && strcmp("'s", result.c_str() + result.size() - 2) == 0)
				result.resize(result.size() - 2);
			continue;
		}

		result += chr;
	}

	return result;
}

LRESULT CALLBACK GuildLister::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	SCROLLINFO si{};
	GuildLister* pThis = (GuildLister*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	switch (uMsg)
	{
		case WM_NCCREATE:
		{
			CREATESTRUCT* strct = (CREATESTRUCT*)lParam;

			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)strct->lpCreateParams);

			break;
		}
		case WM_CREATE:
		{
			si.cbSize = sizeof si;
			si.nPos = 0;
			si.nMin = 0;
			si.nMax = 0;

			break;
		}
		case WM_DESTROY:
		{
			//DestroyWindow(pThis->m_scrollable_hwnd);
			DestroyWindow(pThis->m_tooltip_hwnd);
			DestroyWindow(pThis->m_more_btn_hwnd);
			DestroyWindow(pThis->m_bar_btn_hwnd);
			pThis->m_scrollable_hwnd = NULL;
			pThis->m_tooltip_hwnd    = NULL;
			pThis->m_more_btn_hwnd   = NULL;
			pThis->m_bar_btn_hwnd    = NULL;
			break;
		}
		case WM_LBUTTONDOWN:
		{
			SetFocus(hWnd);
			break;
		}
		case WM_MOUSEMOVE:
		{
			RECT rect = {};
			GetClientRect(hWnd, &rect);
			POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			Snowflake selected = 2;

			si.cbSize = sizeof(si);
			si.fMask = SIF_POS;
			pThis->GetScrollInfo(&si);

			int y = -si.nPos;
			int height = ScaleByDPI(PROFILE_PICTURE_SIZE_DEF + 12);

			TRACKMOUSEEVENT tme;
			tme.cbSize = sizeof tme;
			tme.dwFlags = TME_LEAVE | TME_HOVER;
			tme.hwndTrack = hWnd;
			tme.dwHoverTime = HOVER_DEFAULT;
			ri::TrackMouseEvent(&tme);

			Snowflake currentFolder = 0;
			std::vector<Snowflake> sfs;
			GetDiscordInstance()->GetGuildIDsOrdered(sfs, true);
			for (auto sf : sfs)
			{
				if (sf == 1)
				{
					y += C_GUILD_GAP_HEIGHT;
					continue;
				}

				if (sf & BIT_FOLDER)
				{
					currentFolder = sf & ~BIT_FOLDER;

					if (sf == BIT_FOLDER) {
						// Skip the folder terminator
						continue;
					}
				}
				else if (currentFolder && !pThis->m_openFolders[currentFolder])
				{
					// Skip items in closed folders.
					continue;
				}

				RECT r = {
					rect.left + ScaleByDPI(6),
					rect.top + y + ScaleByDPI(4),
					rect.right - ScaleByDPI(6),
					rect.top + y + ScaleByDPI(4) + GetProfilePictureSize()
				};

				if (PtInRect(&r, pt))
				{
					selected = sf;
					break;
				}

				y += height;
			}

			if (selected != 2) {
				SetCursor(LoadCursor(NULL, IDC_HAND));
				return TRUE;
			}

			break;
		}
		case WM_MOUSEWHEEL:
		{
			RECT rect = {};
			GetClientRect(hWnd, &rect);
			short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);

			si.cbSize = sizeof(si);
			si.fMask = SIF_TRACKPOS | SIF_POS | SIF_RANGE;
			pThis->GetScrollInfo(&si);
			si.nTrackPos = si.nPos - (zDelta / 3);
			if (si.nTrackPos < si.nMin) si.nTrackPos = si.nMin;
			if (si.nTrackPos > si.nMax) si.nTrackPos = si.nMax;
			pThis->SetScrollInfo(&si, false);

			wParam = SB_THUMBTRACK;
			goto VscrollLabel;
		}
		case WM_VSCROLL:
		{
			si.cbSize = sizeof(si);
			si.fMask = SIF_ALL;
			pThis->GetScrollInfo(&si);

		VscrollLabel:;
			int diffUpDown = 0;

			RECT rect;
			GetClientRect(hWnd, &rect);

			int scrollBarWidth = GetSystemMetrics(SM_CXVSCROLL);

			TEXTMETRIC tm;
			HDC hdc = GetDC(hWnd);
			GetTextMetrics(hdc, &tm);
			ReleaseDC(hWnd, hdc);

			int yChar = tm.tmHeight + tm.tmExternalLeading;

			switch (LOWORD(wParam))
			{
				case SB_ENDSCROLL:
					// nPos is correct
					break;
				case SB_THUMBTRACK:
					si.nPos = si.nTrackPos;
					break;
				case SB_LINEUP:
					si.nPos -= yChar;
					break;
				case SB_LINEDOWN:
					si.nPos += yChar;
					break;
				case SB_PAGEUP:
					si.nPos -= si.nPage;
					break;
				case SB_PAGEDOWN:
					si.nPos += si.nPage;
					break;
				case SB_TOP:
					si.nPos = si.nMin;
					break;
				case SB_BOTTOM:
					si.nPos = si.nMax;
					break;
			}

			pThis->SetScrollInfo(&si);
			pThis->GetScrollInfo(&si);

			diffUpDown = si.nPos - pThis->m_oldPos;
			pThis->m_oldPos = si.nPos;

			WindowScroll(hWnd, diffUpDown);
			pThis->UpdateTooltips();
			break;
		}
		case WM_LBUTTONUP:
		case WM_CONTEXTMENU:
		{
			POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			RECT rect = {};
			GetClientRect(hWnd, &rect);

			si.cbSize = sizeof(si);
			si.fMask = SIF_POS;
			pThis->GetScrollInfo(&si);

			int y = -si.nPos;
			int height = ScaleByDPI(PROFILE_PICTURE_SIZE_DEF + 12);
			POINT oript = pt;

			if (uMsg == WM_CONTEXTMENU) {
				ScreenToClient(hWnd, &pt);
			}

			RECT selectedRect;
			Snowflake selected = 2, currentFolder = 0;
			std::vector<Snowflake> sfs;
			GetDiscordInstance()->GetGuildIDsOrdered(sfs, true);

			for (auto sf : sfs)
			{
				if (sf == 1)
				{
					y += C_GUILD_GAP_HEIGHT;
					continue;
				}
				
				if (sf & BIT_FOLDER)
				{
					currentFolder = sf & ~BIT_FOLDER;

					if (sf == BIT_FOLDER) {
						// Skip the folder terminator
						continue;
					}
				}
				else if (currentFolder && !pThis->m_openFolders[currentFolder])
				{
					// Skip items in closed folders.
					continue;
				}

				RECT r = {
					rect.left + ScaleByDPI(6),
					rect.top + y + ScaleByDPI(4),
					rect.right - ScaleByDPI(6),
					rect.top + y + ScaleByDPI(4) + GetProfilePictureSize()
				};

				if (PtInRect(&r, pt))
				{
					selected = sf;
					selectedRect = r;
					break;
				}

				y += height;
			}

			if (selected != 2)
			{
				if (uMsg == WM_CONTEXTMENU) {
					if (selected != 0)
						pThis->ShowMenu(selected, oript);
				}
				else if (selected & BIT_FOLDER) {
					Snowflake folder = selected & ~BIT_FOLDER;
					pThis->m_openFolders[folder] ^= 1;

					selectedRect.left = rect.left;
					selectedRect.right = rect.right;
					selectedRect.bottom = rect.bottom;

					// Get the old position
					SCROLLINFO si{};
					si.cbSize = sizeof si;
					si.fMask = SIF_POS | SIF_RANGE | SIF_PAGE;
					pThis->GetScrollInfo(&si);

					int diff = pThis->UpdateScrollBar(true);

					if (pThis->m_openFolders[folder])
					{
						// Check if the bottom of this folder is outide of the current
						// scroll boundaries. If yes, then scroll down to reveal it.
						int bottomPosition = 0;

						// NOTE: I hate that I duplicate this exact loop like 5 times
						// throughout this file.
						pThis->GetScrollInfo(&si);
						int yBot = -si.nPos, yTop = -si.nPos;

						for (auto sf : sfs)
						{
							if (sf == 1) {
								yBot += C_GUILD_GAP_HEIGHT;
								continue;
							}

							if (sf & BIT_FOLDER) {
								if (sf == BIT_FOLDER) {
									// Skip the folder terminator. Except, if the terminated folder
									// is the one we just opened.
									if (currentFolder == folder) {
										break;
									}

									continue;
								}

								currentFolder = sf & ~BIT_FOLDER;
								if (currentFolder == folder)
									yTop = yBot;
							}
							else if (currentFolder && !pThis->m_openFolders[currentFolder]) {
								// Skip items in closed folders.
								continue;
							}

							yBot += height;
						}

						// Which Y should we reveal?
						// If the folder, when opened, is more than a page, reveal
						// the top, else reveal the bottom.
						int yRev = yBot;
						if (yBot > yTop + (int) si.nPage)
							yRev = yTop + (int) si.nPage;

						if (yRev - si.nPos > (int) si.nPage) {
							diff += yRev - si.nPos - si.nPage;
							si.nPos += diff;
							selectedRect.top = std::max(selectedRect.top - diff, rect.top);
							si.fMask = SIF_POS;
							pThis->SetScrollInfo(&si, true);
							pThis->m_oldPos = si.nPos;
						}
					}
					else {
						// TODO HACK
						if (pThis->m_bIsScrollBarVisible)
							diff = 0;
					}

					WindowScroll(hWnd, diff);
					pThis->UpdateTooltips();

					pThis->GetScrollInfo(&si);
					pThis->m_oldPos = si.nPos;

					InvalidateRect(hWnd, &selectedRect, FALSE);
				}
				else {
					Snowflake sf1 = 0;
					if (GetDiscordInstance()->GetCurrentGuild())
						sf1 = GetDiscordInstance()->GetCurrentGuild()->m_snowflake;

					if (sf1 != selected)
						GetDiscordInstance()->OnSelectGuild(selected);
				}
			}

			break;
		}
		case WM_COMMAND:
		{
			Snowflake guildID = pThis->m_rightClickedGuild;
			Guild* pGuild = GetDiscordInstance()->GetGuild(guildID);
			if (!pGuild)
				break;

			switch (wParam)
			{
				case IDM_COPYGUILDID:
					CopyStringToClipboard(std::to_string(guildID));
					break;

				case IDM_LEAVEGUILD:
					pThis->AskLeave(guildID);
					break;

				case IDM_MARKASREAD:
					GetDiscordInstance()->RequestAcknowledgeGuild(guildID);
					break;
			}
			break;
		}
		case WM_PAINT:
		{
			PAINTSTRUCT ps = {};
			HDC hdc = BeginPaint(hWnd, &ps);

			RECT rect = {};
			GetClientRect(hWnd, &rect);

			si.cbSize = sizeof(si);
			si.fMask = SIF_POS;
			pThis->GetScrollInfo(&si);

			int y = -si.nPos;
			RECT initialRect = rect;
			initialRect.bottom = y;

			std::vector<Snowflake> sfs;
			GetDiscordInstance()->GetGuildIDsOrdered(sfs, true);

			POINT pt;
			MoveToEx(hdc, 0, 0, &pt);

			Snowflake currentFolder = 0;

			for (size_t i = 0; i < sfs.size(); i++)
			{
				Snowflake sf = sfs[i];
				if (sf == 1)
				{
					RECT rc = rect;
					rc.top += y;
					rc.bottom = rc.top + C_GUILD_GAP_HEIGHT;
					FillRect(hdc, &rc, ri::GetSysColorBrush(GUILD_LISTER_COLOR));
					COLORREF oldPenColor = ri::SetDCPenColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
					HGDIOBJ oldobj = SelectObject(hdc, ri::GetDCPen());
					POINT pt2;
					MoveToEx(hdc, rc.left + BORDER_SIZE, rc.top + BORDER_SIZE, &pt2);
					LineTo(hdc, rc.right - BORDER_SIZE, rc.top + BORDER_SIZE);
					SelectObject(hdc, oldobj);
					ri::SetDCPenColor(hdc, oldPenColor);
					y += C_GUILD_GAP_HEIGHT;
					continue;
				}

				HBITMAP hbm;
				std::string textOver = "", name = "", avatarlnk = "";
				bool loadedByLoadBitmap = false;
				bool hasAlpha = false;
				int mentionCount = 0;
				
				if (sf & BIT_FOLDER)
				{
					name = "";
					avatarlnk = "";

					currentFolder = sf & ~BIT_FOLDER;

					if (sf == BIT_FOLDER) {
						// Skip the folder terminator
						continue;
					}
				}
				else if (currentFolder && !pThis->m_openFolders[currentFolder])
				{
					// Skip items in closed folders.
					continue;
				}

				bool isLastItem = false;
				if (i == sfs.size() - 1 || (sfs[i + 1] & BIT_FOLDER))
					isLastItem = true;

				Guild* pGuild = GetDiscordInstance()->GetGuild(sf);
				if (pGuild)
				{
					for (auto &channel : pGuild->m_channels) {
						mentionCount += channel.m_mentionCount;
					}

					name = pGuild->m_name;
					avatarlnk = pGuild->m_avatarlnk;
				}
				
				if (sf == 0)
				{
					int id = NT31SimplifiedInterface() ? IDB_DEFAULT_4BPP : IDB_DEFAULT;
					loadedByLoadBitmap = true;
					hbm = LoadBitmap(g_hInstance, MAKEINTRESOURCE(id));
				}
				else if (avatarlnk.empty())
				{
					int id = NT31SimplifiedInterface() ? IDB_EMPTY_4BPP : IDB_EMPTY;
					loadedByLoadBitmap = true;
					hbm = LoadBitmap(g_hInstance, MAKEINTRESOURCE(id));
					textOver = MakeIconStringFromName(name);
				}
				else
				{
					hbm = GetAvatarCache()->GetImage(avatarlnk, hasAlpha)->GetFirstFrame();
				}

				int oldY = y;
				pThis->DrawServerIcon(hdc, hbm, y, rect, sf, textOver, hasAlpha, currentFolder, isLastItem);
				DrawMentionStatus(hdc, rect.left + BORDER_SIZE + ScaleByDPI(6), rect.top + BORDER_SIZE + ScaleByDPI(4) + oldY, mentionCount);

				if (loadedByLoadBitmap)
					DeleteObject(hbm);
			}

			RECT finalRect = rect;
			finalRect.top = y;

			if (initialRect.top < initialRect.bottom)
				FillRect(hdc, &initialRect, ri::GetSysColorBrush(GUILD_LISTER_COLOR));
			if (finalRect.top < finalRect.bottom)
				FillRect(hdc, &finalRect, ri::GetSysColorBrush(GUILD_LISTER_COLOR));

			MoveToEx(hdc, pt.x, pt.y, NULL);

			EndPaint(hWnd, &ps);
			break;
		}
		case WM_SIZE:
		{
			WindowScroll(hWnd, pThis->UpdateScrollBar(true));
			pThis->UpdateTooltips();
			break;
		}
		case WM_GESTURE:
		{
			HandleGestureMessage(hWnd, uMsg, wParam, lParam, 3.0f);
			break;
		}
		case WM_RBUTTONUP:
		{
			if (!NT31SimplifiedInterface())
				break;
			
			// NT 3.1 doesn't implement WM_CONTEXTMENU, so send it ourselves
			POINT pt;
			pt.x = GET_X_LPARAM(lParam);
			pt.y = GET_Y_LPARAM(lParam);
			ClientToScreen(hWnd, &pt);

			WndProc(hWnd, WM_CONTEXTMENU, (WPARAM) hWnd, MAKELPARAM(pt.x, pt.y));
			break;
		}
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

struct GuildChooserData
{
	HIMAGELIST m_imageList;
	std::vector<Snowflake> m_guildIDs;
};

INT_PTR GuildLister::ChooserDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	GuildChooserData* pData = (GuildChooserData*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
	switch (uMsg)
	{
		case WM_NOTIFY:
		{
			LPNMHDR nmhdr = (LPNMHDR)lParam;
			if (nmhdr->idFrom != IDC_GUILD_LIST)
				break;

			switch (nmhdr->code)
			{
				case NM_DBLCLK:
				{
					LPNMITEMACTIVATE pAct = (LPNMITEMACTIVATE)nmhdr;
					int index = pAct->iItem;
					if (index < 0 || size_t(index) >= pData->m_guildIDs.size())
						break;

					Snowflake guild = pData->m_guildIDs[index];
					GetDiscordInstance()->OnSelectGuild(guild);
					EndDialog(hWnd, TRUE);
					break;
				}
				case LVN_ITEMCHANGED:
				{
					HWND lst = GetDlgItem(hWnd, IDC_GUILD_LIST);
					int item = ListView_GetNextItem(lst, -1, LVNI_SELECTED);
					EnableWindow(GetDlgItem(hWnd, IDOK), item >= 0 && size_t(item) < pData->m_guildIDs.size());
					break;
				}
			}

			break;
		}
		case WM_COMMAND:
		{
			switch (LOWORD(wParam))
			{
				case IDCANCEL:
				{
					EndDialog(hWnd, FALSE);
					break;
				}
				case IDOK:
				{
					int index = ListView_GetNextItem(GetDlgItem(hWnd, IDC_GUILD_LIST), -1, LVNI_SELECTED);
					if (index < 0 || size_t(index) >= pData->m_guildIDs.size())
						break;

					Snowflake guild = pData->m_guildIDs[index];
					GetDiscordInstance()->OnSelectGuild(guild);
					EndDialog(hWnd, TRUE);
					break;
				}
			}
			break;
		}
		case WM_INITDIALOG:
		{
			pData = new GuildChooserData;
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR) pData);

			HWND hWndList = GetDlgItem(hWnd, IDC_GUILD_LIST);

			HIMAGELIST hImgList = ri::ImageList_Create(
				GetProfilePictureSize(),
				GetProfilePictureSize(),
				ILC_MASK | ILC_COLOR32,
				32,
				32
			);

			pData->m_imageList = hImgList;

			if (!hImgList) {
				DbgPrintW("hImgList could not be created!");
				break;
			}

			ListView_SetImageList(hWndList, hImgList, LVSIL_NORMAL);
			ListView_SetImageList(hWndList, hImgList, LVSIL_SMALL);

			// add DM guild
			pData->m_guildIDs.push_back(0);

			int im = ri::ImageList_Add(hImgList, GetDefaultBitmap(), NULL);
			int index = 0;

			LPTSTR tstr = ConvertCppStringToTString("Direct Messages");
			LVITEM lvi{};
			lvi.mask = LVIF_IMAGE | LVIF_TEXT;
			lvi.pszText = tstr;
			lvi.iImage = im;
			lvi.iItem = index++;
			ListView_InsertItem(hWndList, &lvi);
			free(tstr);

			const auto& guildList = GetDiscordInstance()->m_guilds;
			for (const auto& guild : guildList)
			{
				HBITMAP hbm = GetDefaultBitmap();

				if (!guild.m_avatarlnk.empty()) {
					bool unusedHasAlpha = false;
					hbm = GetAvatarCache()->GetImage(guild.m_avatarlnk, unusedHasAlpha)->GetFirstFrame();
				}

				int im = ri::ImageList_Add(hImgList, hbm, NULL);

				LPTSTR tstr = ConvertCppStringToTString(guild.m_name);
				lvi.pszText = tstr;
				lvi.iImage = im;
				lvi.iItem = index++;
				ListView_InsertItem(hWndList, &lvi);

				pData->m_guildIDs.push_back(guild.m_snowflake);
			}

			break;
		}
		case WM_DESTROY:
		{
			ri::ImageList_Destroy(pData->m_imageList);
			delete pData;
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR) NULL);
			break;
		}
	}

	return FALSE;
}

void GuildLister::InitializeClass()
{
	WNDCLASS& wc1 = g_GuildListerParentClass;

	wc1.lpszClassName = T_GUILD_LISTER_PARENT_CLASS;
	wc1.hbrBackground = g_backgroundBrush;
	wc1.style         = 0;
	wc1.hCursor       = LoadCursor(0, IDC_ARROW);
	wc1.lpfnWndProc   = GuildLister::ParentWndProc;
	wc1.hInstance     = g_hInstance;

	RegisterClass(&wc1);

	WNDCLASS& wc = g_GuildListerClass;

	wc.lpszClassName = T_GUILD_LISTER_CLASS;
	wc.hbrBackground = ri::GetSysColorBrush(GUILD_LISTER_COLOR);
	wc.style         = 0;
	wc.hCursor       = LoadCursor(0, IDC_ARROW);
	wc.lpfnWndProc   = GuildLister::WndProc;
	wc.hInstance     = g_hInstance;

	RegisterClass(&wc);
}

GuildLister* GuildLister::Create(HWND hwnd, LPRECT pRect)
{
	GuildLister* newThis = new GuildLister;

	int width = pRect->right - pRect->left, height = pRect->bottom - pRect->top;

	int flagsex = 0;
#if GUILD_LISTER_BORDER
	flagsex |= WS_EX_CLIENTEDGE;
#endif

	newThis->m_bIsScrollBarVisible = false;

	newThis->m_hwnd = CreateWindowEx(
		flagsex, T_GUILD_LISTER_PARENT_CLASS, NULL, WS_CHILD | WS_VISIBLE,
		pRect->left, pRect->top, width, height, hwnd, (HMENU)CID_GUILDLISTER, g_hInstance, newThis
	);

	newThis->m_scrollable_hwnd = CreateWindowEx(
		0, T_GUILD_LISTER_CLASS, NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL,
		0, 0, width - 4, height - 4, newThis->m_hwnd, (HMENU)1, g_hInstance, newThis
	);

	newThis->m_tooltip_hwnd = CreateWindowEx(
		WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, newThis->m_scrollable_hwnd, NULL, g_hInstance, NULL
	);

	if (!NT31SimplifiedInterface()) {
		newThis->m_more_btn_hwnd = CreateWindow(
			WC_BUTTON, TEXT(""), BS_PUSHBUTTON | BS_ICON | WS_CHILD | WS_VISIBLE,
			0, 0, 1, 1, newThis->m_hwnd, (HMENU)GCMD_MORE, g_hInstance, NULL
		);
	}
	else {
		newThis->m_more_btn_hwnd = NULL;
	}

	LPCTSTR barText = NT31SimplifiedInterface() ? TEXT("Scroll") : TEXT("");
	int ic = NT31SimplifiedInterface() ? 0 : BS_ICON;

	newThis->m_bar_btn_hwnd = CreateWindow(
		WC_BUTTON, barText, BS_PUSHBUTTON | ic | WS_CHILD | WS_VISIBLE,
		0, 0, 1, 1, newThis->m_hwnd, (HMENU) GCMD_BAR, g_hInstance, NULL
	);

	ShowScrollBar(newThis->m_scrollable_hwnd, SB_VERT, false);

	// TODO: other icons
	int smcx = GetSystemMetrics(SM_CXSMICON);

	if (!NT31SimplifiedInterface()) {
		SendMessage(newThis->m_more_btn_hwnd, BM_SETIMAGE, (WPARAM) IMAGE_ICON, (LPARAM) ri::LoadImage(g_hInstance, MAKEINTRESOURCE(IDI_GUILDS), IMAGE_ICON, smcx, smcx, LR_SHARED | LR_CREATEDIBSECTION));
		SendMessage(newThis->m_bar_btn_hwnd,  BM_SETIMAGE, (WPARAM) IMAGE_ICON, (LPARAM) ri::LoadImage(g_hInstance, MAKEINTRESOURCE(IDI_SCROLL), IMAGE_ICON, smcx, smcx, LR_SHARED | LR_CREATEDIBSECTION));
	}
	else {
		SendMessage(newThis->m_bar_btn_hwnd, WM_SETFONT, (WPARAM)g_SendButtonFont, TRUE);
	}

	SetWindowPos(newThis->m_tooltip_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	
	return newThis;
}

