#include "MemberList.hpp"
#include "ProfilePopout.hpp"
#include "../discord/ProfileCache.hpp"
#include "../discord/LocalSettings.hpp"
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

MemberList::~MemberList()
{
	if (m_listHwnd)
	{
		BOOL b = DestroyWindow(m_listHwnd);
		assert(b && "was window destroyed?");
		m_listHwnd = NULL;
	}
	if (m_mainHwnd)
	{
		BOOL b = DestroyWindow(m_mainHwnd);
		assert(b && "was window destroyed?");
		m_mainHwnd = NULL;
	}
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
	ListView_DeleteAllItems(m_listHwnd);
	ListView_RemoveAllGroups(m_listHwnd);
#ifdef UNICODE
	ListView_EnableGroupView(m_listHwnd, true);
#endif
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

	// Preserve scroll position
	SCROLLINFO si{};
	si.cbSize = sizeof si;
	si.fMask = SIF_POS;
	ri::GetScrollInfo(m_listHwnd, SB_VERT, &si);

	// Add each group
	for (auto& mem : pGuild->m_members)
	{
		GuildMember* pGroup = pGuild->GetGuildMember(mem);
		if (!pGroup->m_bIsGroup)
			continue;

		if (pGroup->m_groupCount == 0)
			// not worth it
			continue;

#ifdef UNICODE
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
#endif
	}

	// Now add each non-group member
	for (auto& mem : pGuild->m_members)
	{
		GuildMember* pMember = pGuild->GetGuildMember(mem);
		if (pMember->m_bIsGroup)
			continue;

		LVITEM lvi{};
		int groupId = 0;
#ifdef UNICODE
		groupId = LVIF_GROUPID;
#endif

		TCHAR testStr[] = TEXT("");

		lvi.mask = LVIF_TEXT | LVIF_STATE | LVIF_COLUMNS | groupId;
		lvi.stateMask = LVIS_OVERLAYMASK;
		lvi.pszText = testStr;
		lvi.iItem = m_nextItem;
		lvi.iSubItem = 0;
		lvi.iImage = 0;
		lvi.state = 0;
		lvi.cColumns = _countof(g_columnIndices);
		lvi.puColumns = g_columnIndices;
#ifdef UNICODE
		lvi.iGroupId = m_grpToGrpIdx[pMember->m_groupId];
#endif

		m_usrToUsrIdx[pMember->m_user] = m_nextItem;
		m_items.push_back(pMember->m_user);
		m_nextItem++;

		ListView_InsertItem(m_listHwnd, &lvi);
	}

	// Now restore the position
	//ListView_Scroll(m_listHwnd, 0, si.nPos);

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
			ri::TrackMouseEvent(&tme);

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

				ProfilePopout::Show(sf, m_guild, pt.x, pt.y, true);
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
	if (idx < 0 || idx >= int(m_items.size()))
		return;

	// request refresh of the image
	TCHAR tchr[] = TEXT("");
	LVITEM lv;
	lv.mask = LVIF_IMAGE;
	lv.iImage = 0;
	lv.iItem = idx;

	if (bAlsoUpdateText) {
		lv.mask |= LVIF_TEXT;
		lv.pszText = tchr;
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
#ifdef UNICODE
	m_origListWndProc = (WNDPROC) GetWindowLongPtr(m_listHwnd, GWLP_WNDPROC);
	SetWindowLongPtr(m_listHwnd, GWLP_USERDATA, (LONG_PTR) this);
	SetWindowLongPtr(m_listHwnd, GWLP_WNDPROC,  (LONG_PTR) ListWndProc);
#endif

#ifdef NEW_WINDOWS
	ListView_SetExtendedListViewStyleEx(m_listHwnd, LVS_EX_DOUBLEBUFFER, LVS_EX_DOUBLEBUFFER);
#endif

	TCHAR nameStr[] = TEXT("Name");
	LVCOLUMN col{};
	col.mask = LVCF_SUBITEM | LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
	col.pszText = nameStr;
	col.cx = ScaleByDPI(MEMBER_LIST_WIDTH - 25);
	col.iSubItem = 0;
	col.fmt = LVCFMT_LEFT;
	ListView_InsertColumn(m_listHwnd, 0, &col);

	SetWindowFont(m_listHwnd, g_MessageTextFont, TRUE);
}

LRESULT MemberList::ListWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	MemberList* pList = (MemberList*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	assert(pList->m_listHwnd == hWnd);

	switch (uMsg)
	{
		case WM_DESTROY:
		{
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR) NULL);
			SetWindowLongPtr(hWnd, GWLP_WNDPROC,  (LONG_PTR) pList->m_origListWndProc);
			pList->m_listHwnd = NULL;
			break;
		}
		case WM_MOUSELEAVE:
		{
			int oldItem = pList->m_hotItem;
			pList->m_hotItem = -1;
			ListView_RedrawItems(hWnd, oldItem, oldItem);
			break;
		}
	}

	return CallWindowProc(pList->m_origListWndProc, hWnd, uMsg, wParam, lParam);
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

		case WM_DESTROY: {
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR) NULL);
			pList->m_mainHwnd = NULL;
			break;
		}

		case WM_SIZE: {
			assert(pList);
			WORD wWidth  = LOWORD(lParam);
			WORD wHeight = HIWORD(lParam);
			MoveWindow(pList->m_listHwnd, 0, 0, wWidth, wHeight, TRUE);
			break;
		}

		case WM_NOTIFY: {
			assert(pList);
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
			assert(pList);
			LPMEASUREITEMSTRUCT lpmis = (LPMEASUREITEMSTRUCT)lParam;

			if (GetLocalSettings()->GetCompactMemberList()) {
				// adjust the default to be even
				if (lpmis->itemHeight & 1)
					lpmis->itemHeight++;
				break;
			}

			lpmis->itemHeight = ScaleByDPI(PROFILE_PICTURE_SIZE_DEF + 12);
			break;
		}

		case WM_DRAWITEM: {
			assert(pList);
			LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lParam;
			HDC hdc = lpdis->hDC;
			RECT rcItem = lpdis->rcItem;

			bool compact = GetLocalSettings()->GetCompactMemberList();

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
				FillRect(lpdis->hDC, &lpdis->rcItem, ri::GetSysColorBrush(COLOR_MENUBAR));
				backgdColor = GetSysColor(COLOR_MENUBAR);
			}
			else
			{
				// Windows 2000 doesn't do automatic clearing
				FillRect(lpdis->hDC, &lpdis->rcItem, ri::GetSysColorBrush(COLOR_WINDOW));
			}
			
			if (lpdis->itemState & ODS_SELECTED)
			{
				FillRect(lpdis->hDC, &lpdis->rcItem, ri::GetSysColorBrush(COLOR_HIGHLIGHT));
				backgdColor     = GetSysColor(COLOR_HIGHLIGHT);
				nameTextColor   = GetSysColor(COLOR_HIGHLIGHTTEXT);
				statusTextColor = GetSysColor(COLOR_HIGHLIGHTTEXT);
			}

			int textOffset = 0;
			if (!compact)
			{
				// draw profile picture frame
				int sz = ScaleByDPI(PROFILE_PICTURE_SIZE_DEF + 12);
				int szDraw = GetProfileBorderSize();
				ri::DrawIconEx(hdc, rcItem.left, rcItem.top, g_ProfileBorderIcon, szDraw, szDraw, 0, NULL, DI_NORMAL | DI_COMPAT);

				// draw profile picture
				bool hasAlpha = false;
				HBITMAP hbm = GetAvatarCache()->GetImage(pf->m_avatarlnk, hasAlpha)->GetFirstFrame(), hbmmask = NULL;
				DrawBitmap(hdc, hbm, rcItem.left + ScaleByDPI(6), rcItem.top + ScaleByDPI(4), NULL, CLR_NONE, GetProfilePictureSize(), 0, hasAlpha);

				// draw status indicator
				DrawActivityStatus(hdc, rcItem.left + ScaleByDPI(6), rcItem.top + ScaleByDPI(4), pf->m_activeStatus);
				textOffset = sz;
			}
			else
			{
				// draw status indicator, shifted up by 1 pixel because the graphic is actually off-center
				int statusIconSize = ScaleByDPI(16);
				int profileSize = ScaleByDPI(PROFILE_PICTURE_SIZE_DEF);
				DrawActivityStatus(
					hdc,
					rcItem.left - profileSize + statusIconSize,
					rcItem.top - profileSize + statusIconSize + (rcItem.bottom - rcItem.top - statusIconSize) / 2 - ScaleByDPI(1),
					pf->m_activeStatus
				);
				textOffset = statusIconSize + ScaleByDPI(5);
			}
			// compute data necessary to draw the text
			RECT rcText = rcItem;
			rcText.left += textOffset;
			// note, off center from the rectangle because the profile picture is off-center too
			rcText.bottom = rcText.top + textOffset - 4;

			LPTSTR nameTstr = NULL, statTstr = NULL;

			std::string nameText   = pf->GetName(pList->m_guild);
			nameTstr = ConvertCppStringToTString(nameText);

			if (!compact) {
				std::string statusText = pf->GetStatus(pList->m_guild);
				statTstr = ConvertCppStringToTString(statusText);
			}

			COLORREF oldTextColor = SetTextColor(hdc, nameTextColor);
			COLORREF oldBkColor   = SetBkColor  (hdc, backgdColor);

			HGDIOBJ oldObj = SelectObject(hdc, g_AuthorTextFont);

			if (compact || !*statTstr) {
				rcText.top+=2;
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
			if (statTstr)
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
	wc.hbrBackground = ri::GetSysColorBrush(COLOR_3DFACE);
	wc.style = 0;
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.lpfnWndProc = &MemberList::WndProc;
	wc.hInstance = g_hInstance;

	RegisterClass(&wc);
}
