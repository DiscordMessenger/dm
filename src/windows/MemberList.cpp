#include "MemberList.hpp"
#include "ProfilePopout.hpp"
#include "../discord/ProfileCache.hpp"
WNDCLASS MemberList::g_MemberListClass;

#define GRPID_ONLINE  (1)
#define GRPID_OFFLINE (2)

#define OVERLAY_OFFLINE 4
#define OVERLAY_DND     3
#define OVERLAY_IDLE    2
#define OVERLAY_ONLINE  1

static int GetProfileBorderSize()
{
	return ScaleByDPI(Supports32BitIcons() ? (PROFILE_PICTURE_SIZE_DEF + 12) : 64);
}

void MemberList::StartUpdate()
{
	SendMessage(m_listHwnd, WM_SETREDRAW, false, 0);
}

void MemberList::StopUpdate()
{
	SendMessage(m_listHwnd, WM_SETREDRAW, true, 0);
}

void MemberList::ClearMembers()
{
	m_nextItem = 0;
	m_nextGroup = 0;
	m_items.clear();
	m_groups.clear();
	m_usrToUsrIdx.clear();
	m_grpToGrpIdx.clear();
	m_imageIDs.clear();
	InitializeImageList();
	ListView_DeleteAllItems(m_listHwnd);
	ListView_RemoveAllGroups(m_listHwnd);
	ListView_EnableGroupView(m_listHwnd, true);
}

void MemberList::SetGuild(Snowflake g)
{
	if (m_guild == g)
		return;

	m_guild = g;
	ClearMembers();
}

static UINT g_columnIndices[] = { 1 };

std::map<Snowflake, std::string> mockNames, mockStatuses;
std::map<Snowflake, bool> mockOnlineStat;

static TCHAR buff1[4096];
static TCHAR buff2[4096];

void MemberList::Update()
{
	StartUpdate();
	ClearMembers();

	Guild* pGuild = GetDiscordInstance()->GetGuild(m_guild);
	assert(pGuild);

	// Add each group
	for (auto& mem : pGuild->m_members)
	{
		GuildMember* pGroup = pGuild->GetGuildMember(mem);
		if (!pGroup->m_bIsGroup)
			continue;

		if (pGroup->m_groupCount == 0)
			// not worth it
			continue;

		LPTSTR strName = ConvertCppStringToTString(pGuild->GetGroupName(pGroup->m_groupId) + " - " + std::to_string(pGroup->m_groupCount));
		LVGROUP grpz{};
		grpz.cbSize = sizeof(LVGROUP);
		grpz.mask = LVGF_HEADER | LVGF_GROUPID;
		grpz.pszHeader = strName;
		grpz.iGroupId = m_nextGroup;
		ListView_InsertGroup(m_listHwnd, -1, &grpz);
		free(strName);

		m_groups.push_back(pGroup->m_groupId);
		m_grpToGrpIdx[pGroup->m_groupId] = m_nextGroup;
		m_nextGroup++;
	}

	// Now add each non-group member
	for (auto& mem : pGuild->m_members)
	{
		GuildMember* pMember = pGuild->GetGuildMember(mem);
		if (pMember->m_bIsGroup)
			continue;

		LVITEM lvi{};
		lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_STATE | LVIF_COLUMNS | LVIF_GROUPID;
		lvi.stateMask = LVIS_OVERLAYMASK;
		lvi.pszText = TEXT("");
		lvi.iItem = m_nextItem;
		lvi.iSubItem = 0;
		lvi.iImage = 0;
		lvi.state = 0;
		lvi.cColumns = _countof(g_columnIndices);
		lvi.puColumns = g_columnIndices;
		lvi.iGroupId = m_grpToGrpIdx[pMember->m_groupId];

		m_usrToUsrIdx[pMember->m_user] = m_nextItem;
		m_items.push_back(pMember->m_user);
		m_nextItem++;

		ListView_InsertItem(m_listHwnd, &lvi);
	}

	StopUpdate();
}

bool MemberList::OnNotify(LRESULT& out, WPARAM wParam, LPARAM lParam)
{
	NMHDR* hdr = (NMHDR*)lParam;

	switch (hdr->code)
	{
		// TODO: WTF IS THIS?!  Looks like the lParam structure is a hittestinfo after the hdr.
		// This ain't defined anywhere.  Only modern Windows seems to emit it.  I don't know why
		case ((UINT) -165):
		{
			LPLVHITTESTINFO hti = (LPLVHITTESTINFO)(hdr + 1);

			// hack for now, store the hot item ourself
			int oldItem = m_hotItem;
			m_hotItem = hti->iItem;
			//ListView_SetHotItem(m_listHwnd, m_hotItem);
			if (oldItem != m_hotItem) {
				ListView_RedrawItems(m_listHwnd, oldItem,   oldItem);
				ListView_RedrawItems(m_listHwnd, m_hotItem, m_hotItem);
			}

			TRACKMOUSEEVENT tme;
			tme.cbSize = sizeof tme;
			tme.dwFlags = TME_LEAVE;
			tme.hwndTrack = m_listHwnd;
			tme.dwHoverTime = 1;
			_TrackMouseEvent(&tme);

			out = TRUE;
			return true;
		}

		case LVN_HOTTRACK:
			out = TRUE;
			return true;

		case LVN_ITEMCHANGED:
		{
			LPNMLISTVIEW lplv = (LPNMLISTVIEW)lParam;
			if (((lplv->uOldState ^ lplv->uNewState) & LVIS_SELECTED) && (lplv->uNewState & LVIS_SELECTED))
			{
				int itemID = lplv->iItem;
				Snowflake sf = m_items[itemID];

				RECT rcItem{};
				ListView_GetItemRect(m_listHwnd, itemID, &rcItem, LVIR_BOUNDS);

				POINT pt{ rcItem.left, rcItem.top };
				ClientToScreen(m_listHwnd, &pt);

				ShowProfilePopout(sf, m_guild, pt.x, pt.y, true);
			}
			break;
		}
	}

	return false;
}

void MemberList::OnUpdateAvatar(Snowflake user, bool bAlsoUpdateText)
{
	if (m_usrToUsrIdx.find(user) == m_usrToUsrIdx.end())
		return;

	int idx = m_usrToUsrIdx[user];

	// request refresh of the image
	LVITEM lv;
	lv.mask = LVIF_IMAGE;
	lv.iImage = 0;
	lv.iItem = idx;

	if (bAlsoUpdateText) {
		lv.mask |= LVIF_TEXT;
		lv.pszText = TEXT("");
	}

	ListView_SetItem(m_listHwnd, &lv);
	ListView_Update(m_listHwnd, idx);
}

void MemberList::UpdateMembers(std::set<Snowflake>& mems)
{
	for (auto mem : mems) {
		OnUpdateAvatar(mem, true);
	}
}

void MemberList::Initialize()
{
	m_origListWndProc = (WNDPROC) GetWindowLongPtr(m_listHwnd, GWLP_WNDPROC);
	SetWindowLongPtr(m_listHwnd, GWLP_USERDATA, (LONG_PTR) this);
	SetWindowLongPtr(m_listHwnd, GWLP_WNDPROC,  (LONG_PTR) ListWndProc);

#ifdef NEW_WINDOWS
	ListView_SetExtendedListViewStyleEx(m_listHwnd, LVS_EX_DOUBLEBUFFER, LVS_EX_DOUBLEBUFFER);
#endif

	LVCOLUMN col{};
	col.mask = LVCF_SUBITEM | LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
	col.pszText = TEXT("Name");
	col.cx = ScaleByDPI(MEMBER_LIST_WIDTH - 10);
	col.iSubItem = 0;
	col.fmt = LVCFMT_LEFT;
	ListView_InsertColumn(m_listHwnd, 0, &col);

	InitializeImageList();
}

void MemberList::InitializeImageList()
{
	m_imageOffline = (HBITMAP) LoadBitmap(g_hInstance, MAKEINTRESOURCE(IDB_STATUS_OFFLINE));
	m_imageDnd     = (HBITMAP) LoadBitmap(g_hInstance, MAKEINTRESOURCE(IDB_STATUS_DND));
	m_imageIdle    = (HBITMAP) LoadBitmap(g_hInstance, MAKEINTRESOURCE(IDB_STATUS_IDLE));
	m_imageOnline  = (HBITMAP) LoadBitmap(g_hInstance, MAKEINTRESOURCE(IDB_STATUS_ONLINE));
}

LRESULT MemberList::ListWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	MemberList* pList = (MemberList*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	assert(pList && pList->m_listHwnd == hWnd);

	switch (uMsg)
	{
		case WM_MOUSELEAVE:
		{
			int oldItem = pList->m_hotItem;
			pList->m_hotItem = -1;
			ListView_RedrawItems(hWnd, oldItem, oldItem);
			break;
		}
	}

	return pList->m_origListWndProc(hWnd, uMsg, wParam, lParam);
}

LRESULT MemberList::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	MemberList* pList = (MemberList*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	switch (uMsg)
	{
		case WM_NCCREATE: {
			CREATESTRUCT* strct = (CREATESTRUCT*)lParam;
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)strct->lpCreateParams);
			break;
		}

		case WM_SIZE: {
			WORD wWidth  = LOWORD(lParam);
			WORD wHeight = HIWORD(lParam);
			MoveWindow(pList->m_listHwnd, 0, 0, wWidth, wHeight, TRUE);
			break;
		}

		case WM_NOTIFY: {
			LPNMHDR nmhdr = (LPNMHDR)lParam;

			if (nmhdr->hwndFrom == pList->m_listHwnd)
			{
				LRESULT lres = 0;
				if (pList->OnNotify(lres, wParam, lParam))
					return lres;
			}

			break;
		}

		case WM_MEASUREITEM: {
			LPMEASUREITEMSTRUCT lpmis = (LPMEASUREITEMSTRUCT)lParam;
			lpmis->itemHeight = ScaleByDPI(PROFILE_PICTURE_SIZE_DEF + 12);
			break;
		}

		case WM_DRAWITEM: {
			LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lParam;
			HDC hdc = lpdis->hDC;
			RECT rcItem = lpdis->rcItem;

			Snowflake user = pList->m_items[lpdis->itemID];
			Profile* pf = GetProfileCache()->LookupProfile(user, "", "", "", false);

			COLORREF nameTextColor = 0;
			COLORREF statusTextColor = GetSysColor(COLOR_GRAYTEXT);
			COLORREF backgdColor = GetSysColor(COLOR_WINDOW);

			nameTextColor = GetNameColor(pf, pList->m_guild);
			if (nameTextColor == CLR_NONE)
				nameTextColor = GetSysColor(COLOR_WINDOWTEXT);
			
			if (pList->m_hotItem == lpdis->itemID)
			{
				FillRect(lpdis->hDC, &lpdis->rcItem, GetSysColorBrush(COLOR_MENUBAR));
				backgdColor = GetSysColor(COLOR_MENUBAR);
			}
			else
			{
				// Windows 2000 doesn't do automatic clearing
				FillRect(lpdis->hDC, &lpdis->rcItem, GetSysColorBrush(COLOR_WINDOW));
			}
			
			if (lpdis->itemState & ODS_SELECTED)
			{
				FillRect(lpdis->hDC, &lpdis->rcItem, GetSysColorBrush(COLOR_HIGHLIGHT));
				backgdColor     = GetSysColor(COLOR_HIGHLIGHT);
				nameTextColor   = GetSysColor(COLOR_HIGHLIGHTTEXT);
				statusTextColor = GetSysColor(COLOR_HIGHLIGHTTEXT);
			}

			// draw profile picture frame
			int sz = ScaleByDPI(PROFILE_PICTURE_SIZE_DEF + 12);
			int szDraw = GetProfileBorderSize();
			DrawIconEx(hdc, rcItem.left, rcItem.top, g_ProfileBorderIcon, szDraw, szDraw, 0, NULL, DI_NORMAL | DI_COMPAT);
			
			// draw profile picture
			HBITMAP hbm = GetAvatarCache()->GetBitmap(pf->m_avatarlnk), hbmmask = NULL;
			DrawBitmap(hdc, hbm, rcItem.left + ScaleByDPI(6), rcItem.top + ScaleByDPI(4), NULL, CLR_NONE, GetProfilePictureSize());

			// draw status indicator
			DrawActivityStatus(hdc, rcItem.left + ScaleByDPI(6), rcItem.top + ScaleByDPI(4), pf->m_activeStatus);
			
			// compute data necessary to draw the text
			RECT rcText = rcItem;
			rcText.left += sz;
			// note, off center from the rectangle because the profile picture is off-center too
			rcText.bottom = rcText.top + sz - 4;

			std::string nameText   = pf->GetName(pList->m_guild);
			std::string statusText = pf->GetStatus(pList->m_guild);

			LPTSTR nameTstr = ConvertCppStringToTString(nameText);
			LPTSTR statTstr = ConvertCppStringToTString(statusText);

			COLORREF oldTextColor = SetTextColor(hdc, nameTextColor);
			COLORREF oldBkColor   = SetBkColor  (hdc, backgdColor);

			HGDIOBJ oldObj = SelectObject(hdc, g_AuthorTextFont);

			if (statusText.empty()) {
				DrawText(hdc, nameTstr, -1, &rcText, DT_NOPREFIX | DT_VCENTER | DT_SINGLELINE);
			}
			else {
				int top = rcText.top, bottom = rcText.bottom;
				rcText.bottom = (top + bottom) / 2;
				DrawText(hdc, nameTstr, -1, &rcText, DT_NOPREFIX | DT_BOTTOM | DT_SINGLELINE);
				SetTextColor(hdc, statusTextColor);

				SelectObject(hdc, g_DateTextFont);
				rcText.top = (top + bottom) / 2;
				rcText.bottom = bottom;
				DrawText(hdc, statTstr, -1, &rcText, DT_NOPREFIX | DT_TOP | DT_SINGLELINE);
			}

			SelectObject(hdc, oldObj);

			SetTextColor(hdc, oldTextColor);
			SetBkColor(hdc, oldBkColor);

			free(nameTstr);
			free(statTstr);
			break;
		}
	}

	return DefWindowProc( hWnd, uMsg, wParam, lParam );
}

MemberList* MemberList::Create(HWND hWnd, LPRECT rect)
{
	MemberList* list = new MemberList;

	list->m_hwndParent = hWnd;

	list->m_mainHwnd = CreateWindowEx(
		WS_EX_CLIENTEDGE,
		T_MEMBER_LIST_CLASS,
		NULL,
		WS_CHILD | WS_VISIBLE,
		rect->left,
		rect->top,
		rect->right - rect->left,
		rect->bottom - rect->top,
		hWnd,
		(HMENU)CID_MEMBERLIST,
		g_hInstance,
		list
	);

	assert(list->m_mainHwnd);

	list->m_listHwnd = CreateWindowEx(
		0,
		WC_LISTVIEW,
		NULL,
		WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_OWNERDRAWFIXED | LVS_NOCOLUMNHEADER,
		0,
		0,
		rect->right - rect->left,
		rect->bottom - rect->top,
		list->m_mainHwnd,
		(HMENU)1,
		g_hInstance,
		NULL
	);

	assert(list->m_listHwnd);

	list->Initialize();

	return list;
}

void MemberList::InitializeClass()
{
#ifndef MINGW_SPECIFIC_HACKS
	INITCOMMONCONTROLSEX icc;
	icc.dwSize = sizeof icc;
	icc.dwICC = ICC_LISTVIEW_CLASSES;
	InitCommonControlsEx(&icc);
#endif

	WNDCLASS& wc = g_MemberListClass;

	wc.lpszClassName = T_MEMBER_LIST_CLASS;
	wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
	wc.style = 0;
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.lpfnWndProc = &MemberList::WndProc;

	RegisterClass(&wc);
}
