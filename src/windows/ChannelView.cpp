#include "ChannelView.hpp"
#include <tchar.h>

#define CX_BITMAP (16)
#define CY_BITMAP (16)
#define NUM_BITMAPS (6)

ChannelView::~ChannelView()
{
	if (m_hwnd)
	{
		BOOL b = DestroyWindow(m_hwnd);
		assert(b && "window was already destroyed??");
		m_hwnd = NULL;
	}
}

// InitTreeViewImageLists - creates an image list, adds three bitmaps
// to it, and associates the image list with a tree-view control.
// Returns TRUE if successful, or FALSE otherwise.
// hwndTV - handle to the tree-view control.
//
// Global variables and constants:
// g_hInst - the global instance handle.
// g_nOpen, g_nClosed, and g_nDocument - global indexes of the images.
// CX_BITMAP and CY_BITMAP - width and height of an icon.
// NUM_BITMAPS - number of bitmaps to add to the image list.
// IDB_OPEN_FILE, IDB_CLOSED_FILE, IDB_DOCUMENT -
//     resource identifiers of the bitmaps.

bool ChannelView::InitTreeViewImageLists()
{
	HWND hwndTV = m_hwnd;

	HIMAGELIST himl;  // handle to image list
	HBITMAP hbmp;     // handle to bitmap

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

	return TRUE;
}

HTREEITEM ChannelView::GetPrevious(int parentIndex)
{
	auto iter = m_hPrev.find(parentIndex);
	if (iter == m_hPrev.end()) {
		m_hPrev[parentIndex] = TVI_FIRST;
		return TVI_FIRST;
	}
	return iter->second;
}

void ChannelView::SetPrevious(int parentIndex, HTREEITEM hPrev)
{
	m_hPrev[parentIndex] = hPrev;
}

void ChannelView::ResetTree()
{
	m_hPrev.clear();
}

void ChannelView::SetItemIcon(HTREEITEM hItem, int icon)
{
	TVITEM item{};
	ZeroMemory(&item, sizeof(TVITEM)); // Ensure the structure is zero-initialized

	item.hItem = hItem;
	item.mask = TVIF_HANDLE | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
	item.iImage = icon;
	item.iSelectedImage = icon;
	int res = TreeView_SetItem(m_hwnd, &item);
}

HTREEITEM ChannelView::AddItemToTree(HWND hwndTV, LPTSTR lpszItem, HTREEITEM hParent, int nIndex, int nParentIndex, int nIcon)
{
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

void ChannelView::ClearChannels()
{
	m_channels.clear();
	TreeView_DeleteAllItems(m_hwnd);
	ResetTree();
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
	HTREEITEM item = m_channels[index].m_hItem;
	SetItemIcon(item, GetIcon(*pChan, false)); // note: assume it's false because, like, categories don't get acknowledged
}

bool IsChannelASubThread(Channel::eChannelType x)
{
	return x >= Channel::NEWSTHREAD && x < Channel::FORUM;
}

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

	int old_level = m_level;
	if (ch.m_channelType == Channel::CATEGORY) m_level = 1;
	if (IsChannelASubThread(ch.m_channelType)) {
		old_level = m_level;
		m_level = 3;
	}

	cmem.m_hItem = AddItemToTree(m_hwnd, cmem.str, hParent, index, parentIndex, GetIcon(ch, true)); // categories are expanded by default
	m_channels.push_back(cmem);
	m_idToIdx[cmem.m_snowflake] = index;

	if (ch.m_channelType == Channel::CATEGORY)
		m_level = 2;
	if (IsChannelASubThread(ch.m_channelType)) m_level = old_level;
}

void ChannelView::RemoveCategoryIfNeeded(const Channel& ch)
{
	if (ch.m_channelType != Channel::CATEGORY)
		return;

	if (ch.HasPermissionConst(PERM_VIEW_CHANNEL))
		// We can see this category, so it's fine if it's empty
		return;

	int parentIndex = m_idToIdx[ch.m_parentCateg];
	ChannelMember& mem = m_channels[parentIndex];
	if (mem.m_childCount > 0)
		return;

	TreeView_DeleteItem(m_hwnd, mem.m_hItem);
	mem.m_hItem = NULL;
}

ChannelView* ChannelView::Create(HWND hwnd, LPRECT rect)
{
	ChannelView* view = new ChannelView;
	view->m_hwndParent = hwnd;

	view->m_hwnd = CreateWindowEx(
		0,
		WC_TREEVIEW,
		TEXT("Tree View"),
		WS_VISIBLE | WS_CHILD | WS_BORDER | TVS_HASBUTTONS | TVS_TRACKSELECT,
		rect->left,
		rect->top,
		rect->right - rect->left,
		rect->bottom - rect->top,
		hwnd,
		(HMENU)(CID_CHANNELVIEW),
		g_hInstance,
		view
	);

	if (!view->InitTreeViewImageLists())
	{
		DbgPrintW("InitTreeViewImageLists FAILED!");

		delete view;
		view = NULL;
	}

	return view;
}

void ChannelView::OnNotify(WPARAM wParam, LPARAM lParam)
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

			TCHAR buffer[128];
			TVITEM item;
			item.hItem = hSelectedItem;
			item.mask = TVIF_PARAM;
			if (!TreeView_GetItem(m_hwnd, &item)) break;

			// look for a channel with that name
			int idx = int(item.lParam);
			if (idx < 0 || idx >= m_channels.size())
				break;

			Snowflake sf = m_channels[item.lParam].m_snowflake;

			if (sf)
				GetDiscordInstance()->OnSelectChannel(sf);

			break;
		}
	}
}
