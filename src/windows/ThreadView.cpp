#include "ThreadView.hpp"

WNDCLASS ThreadView::g_ThreadViewClass;

ThreadView::~ThreadView()
{
	if (m_listHwnd)
	{
		BOOL b = DestroyWindow(m_listHwnd);
		assert(b && "Was the window destroyed?");
		m_listHwnd = NULL;
	}
}

void ThreadView::StartUpdate()
{
	SendMessage(m_listHwnd, WM_SETREDRAW, false, 0);
}

void ThreadView::StopUpdate()
{
	SendMessage(m_listHwnd, WM_SETREDRAW, true, 0);
}

void ThreadView::SetGuild(Snowflake sf)
{
	if (m_guild == sf)
		return;

	m_guild = sf;
	ClearThreads();
}

void ThreadView::SetChannel(Snowflake sf)
{
	if (m_channel == sf)
		return;

	m_channel = sf;
	ClearThreads();
}

void ThreadView::ClearThreads()
{
	m_nextItem = 0;
	m_nextGroup = 0;
	m_threads.clear();
	m_threadToThreadIdx.clear();
	m_hotItem = -1;

	ListView_DeleteAllItems(m_listHwnd);
	ListView_RemoveAllGroups(m_listHwnd);
}

static UINT g_columnIndices[] = { 1 };

void ThreadView::SetThreads(const std::vector<Channel>& channels)
{
	StartUpdate();
	ClearThreads();

	for (auto& chan : channels)
	{
		LVITEM lvi{};
		TCHAR testStr[] = TEXT("");

		lvi.mask = LVIF_TEXT | LVIF_STATE | LVIF_COLUMNS;
		lvi.stateMask = LVIS_OVERLAYMASK;
		lvi.pszText = testStr;
		lvi.iItem = m_nextItem;
		lvi.iSubItem = 0;
		lvi.iImage = 0;
		lvi.state = 0;
		lvi.cColumns = _countof(g_columnIndices);
		lvi.puColumns = g_columnIndices;

		m_threadToThreadIdx[chan.m_snowflake] = m_nextItem;
		m_threads.push_back(Simplify(chan));
		m_nextItem++;

		ListView_InsertItem(m_listHwnd, &lvi);
	}

	StopUpdate();
}

HWND ThreadView::GetHWND()
{
	return m_mainHwnd;
}

LRESULT ThreadView::ListWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	ThreadView* pView = (ThreadView*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
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
		case WM_SIZE:
		{
			RECT rc;
			GetClientRect(hWnd, &rc);
			ListView_SetColumnWidth(hWnd, 0, rc.right - rc.left);
			break;
		}
	}

	return CallWindowProc(pView->m_origListWndProc, hWnd, uMsg, wParam, lParam);
}

bool ThreadView::OnNotify(LRESULT& out, WPARAM wParam, LPARAM lParam)
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
				//Snowflake sf = m_items[itemID];

				RECT rcItem{};
				ListView_GetItemRect(m_listHwnd, itemID, &rcItem, LVIR_BOUNDS);

				POINT pt{ rcItem.left, rcItem.top };
				ClientToScreen(m_listHwnd, &pt);

				//ProfilePopout::Show(sf, m_guild, pt.x, pt.y, true);
			}
			break;
		}
	}

	return false;
}

LRESULT ThreadView::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	ThreadView* pView = (ThreadView*) GetWindowLongPtr(hWnd, GWLP_USERDATA);

	switch (uMsg)
	{
		case WM_NCCREATE: {
			CREATESTRUCT* strct = (CREATESTRUCT*)lParam;
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)strct->lpCreateParams);
			break;
		}

		case WM_DESTROY: {
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR) NULL);
			pView->m_mainHwnd = NULL;
			break;
		}

		case WM_SIZE: {
			assert(pView);
			WORD wWidth  = LOWORD(lParam);
			WORD wHeight = HIWORD(lParam);
			ListView_SetColumnWidth(pView->m_listHwnd, 0, wWidth);
			MoveWindow(pView->m_listHwnd, 0, 0, wWidth, wHeight, TRUE);
			break;
		}

		case WM_NOTIFY: {
			assert(pView);
			LPNMHDR nmhdr = (LPNMHDR)lParam;

			if (nmhdr->hwndFrom == pView->m_listHwnd)
			{
				LRESULT lres = 0;
				if (pView->OnNotify(lres, wParam, lParam))
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
			assert(pView);
			LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lParam;
			HDC hdc = lpdis->hDC;
			RECT rcItem = lpdis->rcItem;

			int itemID = lpdis->itemID;

			COLORREF backgdColor = GetSysColor(COLOR_WINDOW);
			COLORREF nameTextColor = 0;
			COLORREF statusTextColor = GetSysColor(COLOR_GRAYTEXT);
			if (pView->m_hotItem == lpdis->itemID)
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

			ThreadListItem& tli = pView->m_threads[itemID];

			LPTSTR tstr = ConvertCppStringToTString(tli.m_name);

			DrawText(hdc, tstr, -1, &rcItem, DT_NOPREFIX);

			free(tstr);

			break;
		}
	}

	return DefWindowProc( hWnd, uMsg, wParam, lParam );
}

void ThreadView::Initialize()
{
#ifdef UNICODE
	m_origListWndProc = (WNDPROC)GetWindowLongPtr(m_listHwnd, GWLP_WNDPROC);
	SetWindowLongPtr(m_listHwnd, GWLP_USERDATA, (LONG_PTR)this);
	SetWindowLongPtr(m_listHwnd, GWLP_WNDPROC, (LONG_PTR)ListWndProc);
#endif

#ifdef NEW_WINDOWS
	ListView_SetExtendedListViewStyleEx(m_listHwnd, LVS_EX_DOUBLEBUFFER, LVS_EX_DOUBLEBUFFER);
#endif

	RECT rc;
	GetClientRect(m_listHwnd, &rc);
	TCHAR nameStr[] = TEXT("Name");
	LVCOLUMN col{};
	col.mask = LVCF_SUBITEM | LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
	col.pszText = nameStr;
	col.iSubItem = 0;
	col.fmt = LVCFMT_LEFT;
	col.cx = rc.right - rc.left;
	ListView_InsertColumn(m_listHwnd, 0, &col);

	SetWindowFont(m_listHwnd, g_MessageTextFont, TRUE);
}

ThreadView* ThreadView::Create(HWND hWnd, LPRECT lpRect)
{
	ThreadView* pView = new ThreadView;

	pView->m_hwndParent = hWnd;

	pView->m_mainHwnd = CreateWindowEx(
		WS_EX_CLIENTEDGE,
		T_THREAD_VIEW_CLASS,
		NULL,
		WS_CHILD | WS_VISIBLE,
		lpRect->left,
		lpRect->top,
		lpRect->right - lpRect->left,
		lpRect->bottom - lpRect->top,
		hWnd,
		(HMENU)CID_THREADVIEW,
		g_hInstance,
		pView
	);

	//assert(list->m_mainHwnd);
	if (!pView->m_mainHwnd) {
		DbgPrintW("Couldn't create Thread View main window!");
		delete pView;
		return nullptr;
	}

	pView->m_listHwnd = CreateWindowEx(
		0,
		WC_LISTVIEW,
		NULL,
		WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_OWNERDRAWFIXED | LVS_NOCOLUMNHEADER,
		0,
		0,
		lpRect->right - lpRect->left,
		lpRect->bottom - lpRect->top,
		pView->m_mainHwnd,
		(HMENU)1,
		g_hInstance,
		NULL
	);

	if (!pView->m_listHwnd) {
		DbgPrintW("Couldn't create Thread View list window!");
		delete pView;
		return nullptr;
	}

	pView->Initialize();
	return pView;
}

void ThreadView::InitializeClass()
{
#ifndef MINGW_SPECIFIC_HACKS
	INITCOMMONCONTROLSEX icc;
	icc.dwSize = sizeof icc;
	icc.dwICC = ICC_LISTVIEW_CLASSES;
	InitCommonControlsEx(&icc);
#endif

	WNDCLASS& wc = g_ThreadViewClass;

	wc.lpszClassName = T_THREAD_VIEW_CLASS;
	wc.hbrBackground = ri::GetSysColorBrush(COLOR_3DFACE);
	wc.style = 0;
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.lpfnWndProc = &ThreadView::WndProc;
	wc.hInstance = g_hInstance;

	RegisterClass(&wc);
}

void ThreadView::PopulateWithDummyData()
{
	SetGuild(1);
	SetChannel(1);

	std::vector<Channel> channels;
	channels.push_back(Channel(1, "test1", Channel::PUBTHREAD));
	channels.push_back(Channel(2, "test2", Channel::PUBTHREAD));
	channels.push_back(Channel(3, "test3", Channel::PUBTHREAD));
	channels.push_back(Channel(4, "test4", Channel::PUBTHREAD));
	channels.push_back(Channel(5, "test5", Channel::PUBTHREAD));

	SetThreads(channels);
}
