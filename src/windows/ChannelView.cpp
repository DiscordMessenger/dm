#include "ChannelView.hpp"
#include <tchar.h>

#define CX_BITMAP (16)
#define CY_BITMAP (16)
#define NUM_BITMAPS (6)

WNDCLASS ChannelView::g_ChannelViewClass;

static int GetProfileBorderSize()
{
	return ScaleByDPI(Supports32BitIcons() ? (PROFILE_PICTURE_SIZE_DEF + 12) : 64);
}

enum {
	CID_CHANNELVIEWTREE = 1,
	CID_CHANNELVIEWLIST,
};

ChannelView::~ChannelView()
{
	if (m_treeHwnd)
	{
		BOOL b = DestroyWindow(m_treeHwnd);
		assert(b && "window was already destroyed??");
		m_treeHwnd = NULL;
	}
	if (m_listHwnd)
	{
		BOOL b = DestroyWindow(m_listHwnd);
		assert(b && "window was already destroyed??");
		m_listHwnd = NULL;
	}
	if (m_hwnd)
	{
		BOOL b = DestroyWindow(m_hwnd);
		assert(b && "window was already destroyed??");
		m_hwnd = NULL;
	}
}

bool ChannelView::InitTreeView()
{
	HWND hwndTV = m_treeHwnd;

	HIMAGELIST himl;  // handle to image list

	// Create the image list.
	int flag = 0;
	if (Supports32BitIcons())
		flag = ILC_COLOR32 | ILC_MASK;
	else
		flag = ILC_COLOR4 | ILC_MASK;

	if ((himl = ImageList_Create(ScaleByDPI(CX_BITMAP),
		                         ScaleByDPI(CY_BITMAP),
		                         flag,
		                         NUM_BITMAPS, 0)) == NULL) {
		DbgPrintW("Cannot create image list!");
		return FALSE;
	}

	// Add the open file, closed file, and document bitmaps.
	m_nCategoryExpandIcon   = ImageList_AddIcon(himl, LoadIcon(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_CATEGORY_EXPAND))));
	m_nCategoryCollapseIcon = ImageList_AddIcon(himl, LoadIcon(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_CATEGORY_COLLAPSE))));
	m_nChannelIcon    = ImageList_AddIcon(himl, LoadIcon(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_CHANNEL))));
	m_nForumIcon      = ImageList_AddIcon(himl, LoadIcon(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_GROUPDM))));
	m_nVoiceIcon      = ImageList_AddIcon(himl, LoadIcon(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_VOICE))));
	m_nDmIcon         = ImageList_AddIcon(himl, LoadIcon(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_DM))));
	m_nGroupDmIcon    = ImageList_AddIcon(himl, LoadIcon(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_GROUPDM))));
	m_nChannelDotIcon = ImageList_AddIcon(himl, LoadIcon(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_CHANNEL_UNREAD))));
	m_nChannelRedIcon = ImageList_AddIcon(himl, LoadIcon(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_CHANNEL_MENTIONED))));

	// Fail if not all of the images were added.
	int ic = ImageList_GetImageCount(himl);
	if (ic < 8) {
		DbgPrintW("Cannot add all icons!");
		ImageList_Destroy(himl);
		return FALSE;
	}

	// Associate the image list with the tree-view control.
	// It will be destroyed when the window is destroyed.
	TreeView_SetImageList(hwndTV, himl, TVSIL_NORMAL);

	SetWindowFont(hwndTV, g_MessageTextFont, TRUE);

	return TRUE;
}

bool ChannelView::InitListView()
{
	m_origListWndProc = (WNDPROC) GetWindowLongPtr(m_listHwnd, GWLP_WNDPROC);
	SetWindowLongPtr(m_listHwnd, GWLP_USERDATA, (LONG_PTR) this);
	SetWindowLongPtr(m_listHwnd, GWLP_WNDPROC,  (LONG_PTR) ListWndProc);

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

	return TRUE;
}

HTREEITEM ChannelView::GetPrevious(int parentIndex)
{
	assert(TreeMode());

	auto iter = m_hPrev.find(parentIndex);
	if (iter == m_hPrev.end()) {
		m_hPrev[parentIndex] = TVI_FIRST;
		return TVI_FIRST;
	}
	return iter->second;
}

void ChannelView::SetPrevious(int parentIndex, HTREEITEM hPrev)
{
	assert(TreeMode());

	m_hPrev[parentIndex] = hPrev;
}

void ChannelView::ResetTree()
{
	assert(TreeMode());

	m_hPrev.clear();
}

void ChannelView::SetItemIcon(HTREEITEM hItem, int icon)
{
	if (!TreeMode())
		return;

	TVITEM item{};
	ZeroMemory(&item, sizeof(TVITEM)); // Ensure the structure is zero-initialized

	item.hItem = hItem;
	item.mask = TVIF_HANDLE | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
	item.iImage = icon;
	item.iSelectedImage = icon;
	int res = TreeView_SetItem(m_treeHwnd, &item);
}

HTREEITEM ChannelView::AddItemToTree(HWND hwndTV, LPTSTR lpszItem, HTREEITEM hParent, int nIndex, int nParentIndex, int nIcon)
{
	assert(TreeMode());

	TVITEM tvi;
	TVINSERTSTRUCT tvins;
	HTREEITEM hti;

	tvi.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;

	// Set the text of the item.
	tvi.pszText = lpszItem;
	tvi.cchTextMax = sizeof(tvi.pszText) / sizeof(tvi.pszText[0]);

	// Assume the item is not a parent item, so give it a
	// document image.
	tvi.iImage = nIcon;
	tvi.iSelectedImage = nIcon;

	// Save the heading level in the item's application-defined
	// data area.
	tvi.lParam = (LPARAM)nIndex;
	tvins.item = tvi;
	tvins.hInsertAfter = GetPrevious(nParentIndex);

	// Set the parent item based on the specified level.
	tvins.hParent = hParent;

	// Add the item to the tree-view control.
	HTREEITEM prev = (HTREEITEM)SendMessage(hwndTV, TVM_INSERTITEM, 0, (LPARAM)(LPTVINSERTSTRUCT)&tvins);

	if (prev == NULL)
		return NULL;

	SetPrevious(nParentIndex, prev);

	// The new item is a child item. Give the parent item a
	// closed folder bitmap to indicate it now has child items.
	hti = TreeView_GetParent(hwndTV, prev);
	TreeView_Expand(hwndTV, hti, TVM_EXPAND);

	return prev;
}

void ChannelView::OnUpdateSelectedChannel(Snowflake newCh)
{
	if (m_currentChannel == newCh)
		return;

	m_currentChannel = newCh;

	if (m_channels.empty())
		return;

	ChannelMember& member = m_channels[m_idToIdx[newCh]];
	if (member.m_snowflake != newCh)
		return;

	if (TreeMode()) {
		if (member.m_hItem)
			TreeView_SelectItem(m_treeHwnd, member.m_hItem);
	}
	else {
		ListView_SetItemState(m_listHwnd, m_idToIdx[newCh], LVIS_SELECTED, LVIS_SELECTED);
	}
}

void ChannelView::SetMode(bool listMode)
{
	m_bListMode = listMode;

	int swL = listMode ? SW_SHOWNORMAL : SW_HIDE;
	int swT = listMode ? SW_HIDE : SW_SHOWNORMAL;

	ShowWindow(m_listHwnd, swL);
	ShowWindow(m_treeHwnd, swT);
}

void ChannelView::OnUpdateAvatar(Snowflake user)
{
	if (m_idToIdx.find(user) == m_idToIdx.end())
		return;

	int idx = m_idToIdx[user];
	if (idx < 0 || idx >= int(m_channels.size()))
		return;

	// request refresh of the image
	if (m_bListMode) {
		LVITEM lv;
		lv.mask = LVIF_IMAGE;
		lv.iImage = 0;
		lv.iItem = idx;
		ListView_SetItem(m_listHwnd, &lv);
		ListView_Update(m_listHwnd, idx);
	}
}

void ChannelView::ClearChannels()
{
	m_channels.clear();

	if (TreeMode()) {
		TreeView_DeleteAllItems(m_treeHwnd);
		ResetTree();
	}
	else {
		ListView_DeleteAllItems(m_listHwnd);
	}
}

int ChannelView::GetIcon(const Channel& ch, bool bIsExpanded)
{
	int nIcon;
	bool mentioned = ch.WasMentioned();
	bool unread = ch.HasUnreadMessages();

	switch (ch.m_channelType) {
		default: 
			if (mentioned)
				nIcon = m_nChannelRedIcon;
			else if (unread)
				nIcon = m_nChannelDotIcon;
			else
				nIcon = m_nChannelIcon;
			break;

		case Channel::CATEGORY: nIcon = bIsExpanded ? m_nCategoryCollapseIcon : m_nCategoryExpandIcon; break;
		case Channel::VOICE:    nIcon = m_nVoiceIcon;    break;
		case Channel::FORUM:    nIcon = m_nForumIcon;    break;
		case Channel::GROUPDM:  nIcon = m_nGroupDmIcon;  break;
		case Channel::DM:       nIcon = m_nDmIcon;       break;
	}

	return nIcon;
}

// why do I gotta do this bull crap
static HTREEITEM GetItemHandleByIndex(HWND hwndTree, int index)
{
	HTREEITEM hItem = TreeView_GetNextItem(hwndTree, NULL, TVGN_ROOT); // Get the root item

	for (int i = 0; i < index && hItem; ++i)
		hItem = TreeView_GetNextItem(hwndTree, hItem, TVGN_NEXT); // Get the next item

	return hItem;
}

void ChannelView::UpdateAcknowledgement(Snowflake sf)
{
	int index = -1;
	for (int i = 0; i < int(m_channels.size()); i++)
	{
		if (m_channels[i].m_snowflake == sf)
			index = i;
	}

	if (index < 0) {
		DbgPrintW("ChannelView::UpdateAcknowledgement : Could not find channel %lld", sf);
		return;
	}

	Channel* pChan = GetDiscordInstance()->GetChannel(sf);

	if (TreeMode()) {
		HTREEITEM item = m_channels[index].m_hItem;
		SetItemIcon(item, GetIcon(*pChan, false)); // note: assume it's false because, like, categories don't get acknowledged
	}
	else {
		// TODO
	}
}

bool IsChannelASubThread(Channel::eChannelType x)
{
	return x >= Channel::NEWSTHREAD && x < Channel::FORUM;
}

static UINT g_columnIndices[] = { 1 };

void ChannelView::AddChannel(const Channel & ch)
{
	if (!ch.HasPermissionConst(PERM_VIEW_CHANNEL))
	{
		// If not a category, return.  We'll remove invisible categories without elements after the fact.
		if (ch.m_channelType != Channel::CATEGORY)
			return;
	}

	ChannelMember cmem;
	cmem.m_hItem = NULL;
	cmem.m_childCount = 0;
	cmem.m_snowflake = ch.m_snowflake;

	const TCHAR* ptr = (const TCHAR*)ConvertCppStringToTString(ch.m_name);
	_tcsncpy(cmem.str, ptr, _countof(cmem.str));
	cmem.str[_countof(cmem.str) - 1] = 0; // Ensure that it is null terminated. tcsncpy doesn't.
	free((void*)ptr);

	int index = int(m_channels.size());
	int old_level = -1;

	if (TreeMode())
	{
		int parentIndex = -1;
		HTREEITEM hParent = TVI_ROOT;
		if (ch.m_channelType == Channel::CATEGORY) {
			parentIndex = -2;
		}
		else if (ch.m_parentCateg != 0) {
			parentIndex = m_idToIdx[ch.m_parentCateg];
			hParent = m_channels[parentIndex].m_hItem;
			m_channels[parentIndex].m_childCount++;
		}

		old_level = m_level;
		if (ch.m_channelType == Channel::CATEGORY) m_level = 1;
		if (IsChannelASubThread(ch.m_channelType)) {
			old_level = m_level;
			m_level = 3;
		}

		cmem.m_hItem = AddItemToTree(m_treeHwnd, cmem.str, hParent, index, parentIndex, GetIcon(ch, true)); // categories are expanded by default
	}
	else
	{
		LVITEM lvi{};
		lvi.mask = LVIF_COLUMNS;
		lvi.stateMask = LVIS_OVERLAYMASK;
		lvi.iItem = index;
		lvi.iSubItem = 0;
		lvi.cColumns = _countof(g_columnIndices);
		lvi.puColumns = g_columnIndices;
		ListView_InsertItem(m_listHwnd, &lvi);
	}

	m_channels.push_back(cmem);
	m_idToIdx[cmem.m_snowflake] = index;

	if (ch.m_channelType == Channel::DM)
		m_idToIdx[ch.GetDMRecipient()] = index;

	if (TreeMode())
	{
		if (ch.m_channelType == Channel::CATEGORY)
			m_level = 2;
		if (IsChannelASubThread(ch.m_channelType)) m_level = old_level;
	}
}

void ChannelView::RemoveCategoryIfNeeded(const Channel& ch)
{
	if (!TreeMode())
		return;

	if (ch.m_channelType != Channel::CATEGORY)
		return;

	int parentIndex = m_idToIdx[ch.m_parentCateg];
	ChannelMember& mem = m_channels[parentIndex];
	if (mem.m_childCount > 0)
		return;

	TreeView_DeleteItem(m_treeHwnd, mem.m_hItem);
	mem.m_hItem = NULL;
}

ChannelView* ChannelView::Create(HWND hwnd, LPRECT rect)
{
	ChannelView* view = new ChannelView;
	view->m_hwndParent = hwnd;

	view->m_hwnd = CreateWindowEx(
		WS_EX_CLIENTEDGE,
		T_CHANNEL_VIEW_CONTAINER_CLASS,
		NULL,
		WS_CHILD | WS_VISIBLE,
		rect->left,
		rect->top,
		rect->right - rect->left,
		rect->bottom - rect->top,
		hwnd,
		(HMENU)(CID_CHANNELVIEW),
		g_hInstance,
		view
	);

	assert(view->m_hwnd);
	if (!view->m_hwnd) {
		DbgPrintW("ChannelView::Create: Could not create main window!");
		delete view;
		return NULL;
	}

	view->m_treeHwnd = CreateWindowEx(
		0,
		WC_TREEVIEW,
		NULL,
		WS_CHILD | TVS_HASBUTTONS | TVS_TRACKSELECT | TVS_SHOWSELALWAYS,
		rect->left,
		rect->top,
		rect->right - rect->left,
		rect->bottom - rect->top,
		view->m_hwnd,
		(HMENU)(CID_CHANNELVIEWTREE),
		g_hInstance,
		view
	);
	if (!view->m_treeHwnd) {
		DbgPrintW("ChannelView::Create: Could not create tree sub-window!");
		delete view;
		return NULL;
	}
	
	view->m_listHwnd = CreateWindowEx(
		0,
		WC_LISTVIEW,
		NULL,
		WS_CHILD | LVS_REPORT | LVS_SINGLESEL | LVS_OWNERDRAWFIXED | LVS_NOCOLUMNHEADER,
		rect->left,
		rect->top,
		rect->right - rect->left,
		rect->bottom - rect->top,
		view->m_hwnd,
		(HMENU)(CID_CHANNELVIEWLIST),
		g_hInstance,
		view
	);
	if (!view->m_listHwnd) {
		DbgPrintW("ChannelView::Create: Could not create tree sub-window!");
		delete view;
		return NULL;
	}

	if (!view->InitTreeView() || !view->InitListView())
	{
		DbgPrintW("ChannelView::Create: InitTreeView or InitListView FAILED!");

		delete view;
		view = NULL;
	}

	return view;
}

void ChannelView::InitializeClass()
{
#ifndef MINGW_SPECIFIC_HACKS
	INITCOMMONCONTROLSEX icc;
	icc.dwSize = sizeof icc;
	icc.dwICC = ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES;
	InitCommonControlsEx(&icc);
#endif

	WNDCLASS& wc = g_ChannelViewClass;

	wc.lpszClassName = T_CHANNEL_VIEW_CONTAINER_CLASS;
	wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
	wc.style = 0;
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.lpfnWndProc = &ChannelView::WndProc;

	RegisterClass(&wc);
}

bool ChannelView::OnNotifyTree(LRESULT& out, WPARAM wParam, LPARAM lParam)
{
	LPNMHDR nmhdr = (LPNMHDR)lParam;

	switch (nmhdr->code)
	{
		case TVN_ITEMEXPANDED:
		{
			NMTREEVIEW* nmtv = (NMTREEVIEW*)lParam;

			HTREEITEM hItem = nmtv->itemNew.hItem;
			if (!hItem) break;

			SetItemIcon(hItem, nmtv->action == TVE_EXPAND ? m_nCategoryCollapseIcon : m_nCategoryExpandIcon);
			break;
		}
		case TVN_SELCHANGED:
		{
			NMTREEVIEW* nmtv = (NMTREEVIEW*) lParam;

			HTREEITEM hSelectedItem = nmtv->itemNew.hItem;
			if (!hSelectedItem) break;

			TVITEM item;
			item.hItem = hSelectedItem;
			item.mask = TVIF_PARAM;
			if (!TreeView_GetItem(m_treeHwnd, &item)) break;

			// look for a channel with that name
			size_t idx = size_t(item.lParam);
			if (idx >= m_channels.size())
				break;

			Snowflake sf = m_channels[item.lParam].m_snowflake;

			if (sf) {
				m_currentChannel = sf;
				GetDiscordInstance()->OnSelectChannel(sf);
			}

			break;
		}
	}

	return false;
}

bool ChannelView::OnNotifyList(LRESULT& out, WPARAM wParam, LPARAM lParam)
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
			if (lplv->uNewState & LVIS_SELECTED)
			{
				int itemID = lplv->iItem;

				// TODO: Select the channel
				//DbgPrintW("Selected item ID %d", itemID);
				if (itemID < 0 || itemID >= int(m_channels.size()))
					return false;

				ChannelMember* pMember = &m_channels[itemID];
				GetDiscordInstance()->OnSelectChannel(pMember->m_snowflake);
			}
			break;
		}
	}

	return false;
}

LRESULT ChannelView::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	ChannelView* pView = (ChannelView*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	switch (uMsg)
	{
		case WM_NCCREATE: {
			CREATESTRUCT* strct = (CREATESTRUCT*)lParam;
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)strct->lpCreateParams);
			break;
		}

		case WM_DESTROY: {
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR) NULL);
			pView->m_hwnd = NULL;
			break;
		}

		case WM_SIZE: {
			assert(pView);
			WORD wWidth  = LOWORD(lParam);
			WORD wHeight = HIWORD(lParam);
			MoveWindow(pView->m_treeHwnd, 0, 0, wWidth, wHeight, TRUE);
			MoveWindow(pView->m_listHwnd, 0, 0, wWidth, wHeight, TRUE);
			break;
		}

		case WM_NOTIFY: {
			assert(pView);
			LPNMHDR nmhdr = (LPNMHDR)lParam;

			if (nmhdr->hwndFrom == pView->m_treeHwnd)
			{
				LRESULT lres = 0;
				if (pView->OnNotifyTree(lres, wParam, lParam))
					return lres;
			}
			else if (nmhdr->hwndFrom == pView->m_listHwnd)
			{
				LRESULT lres = 0;
				if (pView->OnNotifyList(lres, wParam, lParam))
					return lres;
			}

			break;
		}

		case WM_MEASUREITEM: {
			assert(pView);
			LPMEASUREITEMSTRUCT lpmis = (LPMEASUREITEMSTRUCT)lParam;
			lpmis->itemHeight = ScaleByDPI(PROFILE_PICTURE_SIZE_DEF + 12);
			break;
		}

		case WM_DRAWITEM: {
			LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lParam;
			assert(pView);
			assert(lpdis->hwndItem == pView->m_listHwnd);

			HDC hdc = lpdis->hDC;
			RECT rcItem = lpdis->rcItem;

			if (lpdis->itemID < 0 || lpdis->itemID >= int(pView->m_channels.size()))
				break;

			ChannelMember* pMember = &pView->m_channels[lpdis->itemID];
			Channel* pChan = GetDiscordInstance()->GetChannel(pMember->m_snowflake);

			COLORREF nameTextColor = GetSysColor(COLOR_WINDOWTEXT);
			COLORREF statusTextColor = GetSysColor(COLOR_GRAYTEXT);
			COLORREF backgdColor = GetSysColor(COLOR_WINDOW);
			
			if (pView->m_hotItem == lpdis->itemID)
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
			bool hasAlpha = false;
			HBITMAP hbm = GetAvatarCache()->GetImage(pChan->m_avatarLnk, hasAlpha)->GetFirstFrame(), hbmmask = NULL;
			DrawBitmap(hdc, hbm, rcItem.left + ScaleByDPI(6), rcItem.top + ScaleByDPI(4), NULL, CLR_NONE, GetProfilePictureSize(), 0, hasAlpha);

			// draw status indicator
			std::string statusText = "";
			if (pChan->m_channelType == Channel::DM)
			{
				Profile* pf = GetProfileCache()->LookupProfile(pChan->GetDMRecipient(), "", "", "", false);
				DrawActivityStatus(hdc, rcItem.left + ScaleByDPI(6), rcItem.top + ScaleByDPI(4), pf->m_activeStatus);
				statusText = pf->m_status;

				// ^^^^^ NOTE: Status and activestatus are inaccurate right now because I don't support certain status updates.
			}
			else
			{
				assert(pChan->m_channelType == Channel::GROUPDM);

				statusText = std::to_string(pChan->GetRecipientCount() + 1) + " members";
			}
			
			// compute data necessary to draw the text
			RECT rcText = rcItem;
			rcText.left += sz;
			// note, off center from the rectangle because the profile picture is off-center too
			rcText.bottom = rcText.top + sz - 4;

			std::string nameText = pChan->m_name;	

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

LRESULT ChannelView::ListWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	ChannelView* pView = (ChannelView*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	assert(pView->m_listHwnd == hWnd);

	switch (uMsg)
	{
	case WM_DESTROY:
	{
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)NULL);
		SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)pView->m_origListWndProc);
		pView->m_listHwnd = NULL;
		break;
	}
	case WM_MOUSELEAVE:
	{
		int oldItem = pView->m_hotItem;
		pView->m_hotItem = -1;
		ListView_RedrawItems(hWnd, oldItem, oldItem);
		break;
	}
	}

	return CallWindowProc(pView->m_origListWndProc, hWnd, uMsg, wParam, lParam);
}
