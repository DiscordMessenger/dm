#include "ChannelView.hpp"
#include <tchar.h>

int g_nCategoryIcon, g_nChannelIcon, g_nForumIcon, g_nVoiceIcon, g_nDmIcon, g_nGroupDmIcon, g_nChannelDotIcon, g_nChannelRedIcon;

/*
#define MAX_HEADING_LEN (256)

struct Heading
{
	TCHAR tchHeading[MAX_HEADING_LEN];
	int tchLevel;
};
*/

//Heading g_rgDocHeadings[27];

#define CX_BITMAP (16)
#define CY_BITMAP (16)
#define NUM_BITMAPS (6)

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

BOOL InitTreeViewImageLists(HWND hwndTV)
{
	HIMAGELIST himl;  // handle to image list
	HBITMAP hbmp;     // handle to bitmap

	// Create the image list.
	int flag = 0;
	if (Supports32BitIcons())
		flag = ILC_COLOR32;
	else
		flag = ILC_COLOR4;

	if ((himl = ImageList_Create(ScaleByDPI(CX_BITMAP),
		                         ScaleByDPI(CY_BITMAP),
		                         flag,
		                         NUM_BITMAPS, 0)) == NULL) {
		DbgPrintW("Cannot create image list!");
		return FALSE;
	}

	// Add the open file, closed file, and document bitmaps.
	g_nCategoryIcon   = ImageList_AddIcon(himl, LoadIcon(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_CATEGORY))));
	g_nChannelIcon    = ImageList_AddIcon(himl, LoadIcon(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_CHANNEL))));
	g_nForumIcon      = ImageList_AddIcon(himl, LoadIcon(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_GROUPDM))));
	g_nVoiceIcon      = ImageList_AddIcon(himl, LoadIcon(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_VOICE))));
	g_nDmIcon         = ImageList_AddIcon(himl, LoadIcon(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_DM))));
	g_nGroupDmIcon    = ImageList_AddIcon(himl, LoadIcon(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_GROUPDM))));
	g_nChannelDotIcon = ImageList_AddIcon(himl, LoadIcon(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_CHANNEL_UNREAD))));
	g_nChannelRedIcon = ImageList_AddIcon(himl, LoadIcon(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_CHANNEL_MENTIONED))));

	// Fail if not all of the images were added.
	int ic = ImageList_GetImageCount(himl);
	if (ic < 4) {
		DbgPrintW("Cannot add all icons!");
		return FALSE;
	}

	// Associate the image list with the tree-view control.
	TreeView_SetImageList(hwndTV, himl, TVSIL_NORMAL);

	return TRUE;
}

// Adds items to a tree-view control.
// Returns the handle to the newly added item.
// hwndTV - handle to the tree-view control.
// lpszItem - text of the item to add.
// nLevel - level at which to add the item.
//
// g_nClosed, and g_nDocument - global indexes of the images.
std::map<int, HTREEITEM> g_hPrev;
HTREEITEM g_hPrevRootItem = NULL;
HTREEITEM g_hPrevLev2Item = NULL;

static HTREEITEM GetPrevious(int parentIndex)
{
	auto iter = g_hPrev.find(parentIndex);
	if (iter == g_hPrev.end()) {
		g_hPrev[parentIndex] = TVI_FIRST;
		return TVI_FIRST;
	}
	return iter->second;
}

static void SetPrevious(int parentIndex, HTREEITEM hPrev)
{
	g_hPrev[parentIndex] = hPrev;
}

void TreeReset()
{
	g_hPrev.clear();
	g_hPrevRootItem = NULL;
	g_hPrevLev2Item = NULL;
}

HTREEITEM AddItemToTree(HWND hwndTV, LPTSTR lpszItem, HTREEITEM hParent, int nIndex, int nParentIndex, int nIcon)
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
	TreeReset();
}

static int GetIcon(const Channel& ch)
{
	int nIcon;
	bool mentioned = ch.WasMentioned();
	bool unread = ch.HasUnreadMessages();

	switch (ch.m_channelType) {
		default: 
			if (mentioned)
				nIcon = g_nChannelRedIcon;
			else if (unread)
				nIcon = g_nChannelDotIcon;
			else
				nIcon = g_nChannelIcon;
			break;

		case Channel::CATEGORY: nIcon = g_nCategoryIcon; break;
		case Channel::VOICE:    nIcon = g_nVoiceIcon;    break;
		case Channel::FORUM:    nIcon = g_nForumIcon;    break;
		case Channel::GROUPDM:  nIcon = g_nGroupDmIcon;  break;
		case Channel::DM:       nIcon = g_nDmIcon;       break;
	}

	return nIcon;
}

// why do I gotta do this bull crap
HTREEITEM GetItemHandleByIndex(HWND hwndTree, int index)
{
	HTREEITEM hItem = TreeView_GetNextItem(hwndTree, NULL, TVGN_ROOT); // Get the root item

	for (int i = 0; i < index && hItem; ++i)
	{
		hItem = TreeView_GetNextItem(hwndTree, hItem, TVGN_NEXT); // Get the next item
	}

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

	TVITEM item{};
	ZeroMemory(&item, sizeof(TVITEM)); // Ensure the structure is zero-initialized

	item.hItem = (HTREEITEM) m_channels[index].m_hItem;
	item.mask = TVIF_IMAGE;
	item.iImage = 0;
	TreeView_GetItem(m_hwnd, &item);

	Channel* pChan = GetDiscordInstance()->GetChannel(sf);
	item.iImage = GetIcon(*pChan);
	TreeView_SetItem(m_hwnd, &item);
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
	_tcscpy(cmem.str, ptr);
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

	cmem.m_hItem = AddItemToTree(m_hwnd, cmem.str, hParent, index, parentIndex, GetIcon(ch));
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
		WS_VISIBLE | WS_CHILD | WS_BORDER | TVS_HASLINES | TVS_HASBUTTONS,
		rect->left,
		rect->top,
		rect->right - rect->left,
		rect->bottom - rect->top,
		hwnd,
		(HMENU)(CID_CHANNELVIEW),
		g_hInstance,
		view
	);

	if (!InitTreeViewImageLists(view->m_hwnd))
	{
		DbgPrintW("InitTreeViewImageLists FAILED!");
		DestroyWindow(view->m_hwnd);

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
		case NM_DBLCLK:
		{
			HTREEITEM hSelectedItem = TreeView_GetSelection(m_hwnd);
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
