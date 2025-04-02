#include "AutoComplete.hpp"
#include "Main.hpp"

std::set<AutoComplete*> AutoComplete::m_activeAutocompletes;

#define T_AUTOCOMPLETE_CLASS TEXT("DMAutoComplete")

enum {
	COL_NAME,
	COL_SUBNAME,
};

AutoComplete::~AutoComplete()
{
	if (m_hwnd)
	{
		BOOL b = DestroyWindow(m_hwnd);
		assert(b && "Was the window already destroyed?");

		m_hwnd = NULL;
	}

	assert(!m_listHwnd);

	// Erase ourselves from the list of active autocomplete windows, if needed.
	auto it = m_activeAutocompletes.find(this);
	if (it != m_activeAutocompletes.end())
		m_activeAutocompletes.erase(it);
}

void AutoComplete::ShowOrMove(POINT pt)
{
	bool oldReachedMaxY = m_bReachedMaxSizeY;

	if (m_matches.empty())
	{
		Hide();
		return;
	}

	// Calculate the height of all the entries.
	HDC hdc = GetDC(m_hwnd);
	HGDIOBJ old = SelectObject(hdc, g_MessageTextFont);
	int height = 0;
	int width = 0;
	int widthCol1 = 0;
	int widthCol2 = 0;
	const int maxHeight = ScaleByDPI(200);
	const int maxWidth = ScaleByDPI(200);
	m_bReachedMaxSizeY = false;
	for (auto& entry : m_matches)
	{
		RECT rcMeasure{};
		RECT rcMeasureSub{};

		// measure string
		LPTSTR tstr = ConvertCppStringToTString(entry.str);
		DrawText(hdc, tstr, -1, &rcMeasure, DT_SINGLELINE | DT_CALCRECT);
		free(tstr);

		// measure substring if not blank
		if (!entry.substr.empty())
		{
			tstr = ConvertCppStringToTString(entry.substr);
			DrawText(hdc, tstr, -1, &rcMeasureSub, DT_SINGLELINE | DT_CALCRECT);
			free(tstr);
		}

		height += rcMeasure.bottom - rcMeasure.top + ScaleByDPI(4);

		int newC1Width = rcMeasure.right - rcMeasure.left + ScaleByDPI(12);
		int newC2Width = entry.substr.empty() ? 0 : (rcMeasureSub.right - rcMeasureSub.left + ScaleByDPI(12));

		widthCol1 = std::max(widthCol1, std::min(maxWidth, newC1Width));
		widthCol2 = std::max(widthCol2, std::min(maxWidth, newC2Width));

		if (height > maxHeight) {
			m_bReachedMaxSizeY = true;
			height = maxHeight;
		}
	}
	SelectObject(hdc, old);
	ReleaseDC(m_hwnd, hdc);

	width = widthCol1 + widthCol2;

	m_widthColumn1 = widthCol1;
	m_widthColumn2 = widthCol2;

	// If reachedMaxSize is different, dismiss the old autocomplete window first and recreate:
	if (m_bReachedMaxSizeY != oldReachedMaxY)
		Hide();

	constexpr int style = WS_POPUP | WS_BORDER;

	width += m_bReachedMaxSizeY ? GetSystemMetrics(SM_CXVSCROLL) : 0;

	RECT rc{ pt.x, pt.y - height, pt.x + width, pt.y };
	AdjustWindowRect(&rc, style, FALSE);

	if (!m_hwnd)
	{
		// Create the window, the list window will be created by it
		m_hwnd = CreateWindowEx(
			WS_EX_NOACTIVATE,
			T_AUTOCOMPLETE_CLASS,
			TEXT(""),
			style,
			rc.left,
			rc.top,
			rc.right - rc.left,
			rc.bottom - rc.top,
			GetParent(m_editHwnd),
			NULL,
			g_hInstance,
			this
		);

		ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
		SendMessage(m_hwnd, WM_UPDATECOMPITEMS, 0, 0);

		m_bHadFirstArrowPress = false;

		// Add ourselves to the list of active autocomplete windows.
		m_activeAutocompletes.insert(this);
	}
	else
	{
		MoveWindow(m_hwnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
		SendMessage(m_hwnd, WM_UPDATECOMPITEMS, 0, 0);
	}
}

void AutoComplete::Hide()
{
	if (m_hwnd)
	{
		BOOL b = DestroyWindow(m_hwnd);
		assert(b && "This window was already destroyed?");
		m_hwnd = NULL;
	}

	assert(!m_listHwnd);

	m_bHadFirstArrowPress = false;

	// Erase ourselves from the list of active autocomplete windows.
	auto it = m_activeAutocompletes.find(this);
	if (it != m_activeAutocompletes.end())
		m_activeAutocompletes.erase(it);
}

void AutoComplete::Commit()
{
	if (!IsActive())
		return;
	
	int index = GetSelectionIndex();
	if (index < 0 || index >= int(m_matches.size()))
		return;

	const std::string& match = m_matches[index].str;

	// Append this string to the text.
	LPTSTR tstr = ConvertCppStringToTString(match);

	// Stolen from https://cboard.cprogramming.com/windows-programming/55742-appending-text-edit-control-post389162.html#post389162 cheers!
	int length = GetWindowTextLength(m_editHwnd);
	SendMessage(m_editHwnd, EM_SETSEL, m_startAt, length);
	SendMessage(m_editHwnd, EM_REPLACESEL, 0, (LPARAM)tstr);
	SendMessage(m_editHwnd, WM_VSCROLL, SB_BOTTOM, (LPARAM)NULL);

	free(tstr);

	Hide();
}

int AutoComplete::GetSelectionIndex()
{
	if (!IsActive()) return -1;
	return (int)SendMessage(m_hwnd, WM_GETCOMPINDEX, 0, 0);
}

void AutoComplete::_Update(LPCTSTR message, int length)
{
	LPCTSTR initialMessage = message;
	if (GetFocus() != m_editHwnd)
		return;

	if (length <= 0) {
		Hide();
		return;
	}
	
	int autoStart = -1;

	for (int i = length - 1; i >= 0; )
	{
		// If a space, break.  We've searched the entirety of this word.
		TCHAR c = message[i];
		if (c == (TCHAR) ' ')
			break;

		if ((c == (TCHAR) '@' || c == (TCHAR) '#' || c == (TCHAR) ':') &&
			(i == 0 || message[i - 1] == ' '))
		{
			// Start auto-completion here.
			autoStart = i;
			break;
		}

		if (i == 0)
			break;
		--i;
	}
	
	if (autoStart == -1) {
		Hide();
		return;
	}

	message += autoStart;

	// take the query
	char query = (char)*message; message++;

	// and the actual name
	std::string stuff = MakeStringFromTString(message);

	m_matches.clear();
	m_lookup(m_pLookupContext, stuff, query, m_matches);

	std::sort(m_matches.begin(), m_matches.end());

	POINT pt {};
	if (!GetCaretPos(&pt))
		return;

	ClientToScreen(m_editHwnd, &pt);

	m_startAt = int(message - initialMessage);
	ShowOrMove(pt);
}

void AutoComplete::Update(LPCTSTR textInEditControl, int length)
{
	LPTSTR freed = NULL;

	if (!textInEditControl)
	{
		length = Edit_GetTextLength(m_editHwnd);

		freed = new TCHAR[length + 1];
		Edit_GetText(m_editHwnd, freed, length + 1);

		textInEditControl = freed;
	}

	if (!length)
		length = int(_tcslen(textInEditControl));

	_Update(textInEditControl, length);

	if (freed)
		delete[] freed;
}

bool AutoComplete::HandleKeyMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (!m_listHwnd)
		return false;
	
	if (wParam != VK_UP && wParam != VK_DOWN)
		return false;

	SendMessage(m_listHwnd, uMsg, wParam, lParam);

	// HACK: Need to re-do this key press because the list view control drops
	// the first one for some reason.
	if (!m_bHadFirstArrowPress)
	{
		m_bHadFirstArrowPress = true;

		if (uMsg == WM_KEYDOWN) {
			SendMessage(m_listHwnd, WM_KEYUP, wParam, 3 << 30);
			SendMessage(m_listHwnd, uMsg, wParam, lParam);
		}
		else {
			SendMessage(m_listHwnd, WM_KEYDOWN, wParam, 0);
			SendMessage(m_listHwnd, uMsg, wParam, lParam);
		}
	}

	return true;
}

bool AutoComplete::HandleCharMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (!m_listHwnd)
		return false;

	if (wParam != '\r' && wParam != '\t')
		return false;

	Commit();
	return true;
}

void AutoComplete::SetEdit(HWND hWnd)
{
	m_editHwnd = hWnd;
}

void AutoComplete::SetFont(HFONT hFont)
{
	m_hFont = hFont;
}

void AutoComplete::SetLookup(LookUpFunction function)
{
	m_lookup = function;
}

void AutoComplete::SetLookupContext(void* context)
{
	m_pLookupContext = context;
}

bool AutoComplete::InitializeClass()
{
	WNDCLASS wc;
	ZeroMemory(&wc, sizeof wc);
	wc.lpszClassName = T_AUTOCOMPLETE_CLASS;
	wc.hbrBackground = ri::GetSysColorBrush(COLOR_3DFACE);
	wc.style         = 0;
	wc.hCursor       = LoadCursor (0, IDC_ARROW);
	wc.lpfnWndProc   = AutoComplete::WndProc;
	wc.hInstance     = g_hInstance;

	return RegisterClass(&wc);
}

void AutoComplete::DismissAutoCompleteWindowsIfNeeded(HWND updated)
{
	const auto& active = AutoComplete::GetActive();
	bool isFine = false;

	for (const auto& ac : active)
	{
		if (updated == ac->GetHWND() || IsChildOf(updated, ac->GetHWND())) {
			isFine = true;
			break;
		}
	}

	if (isFine)
		return;

	// Create a copy to prevent the set from being mutated while worked on
	std::set<AutoComplete*> activeCopy = active;
	for (const auto& acc : activeCopy)
		acc->Hide();
}

LRESULT CALLBACK AutoComplete::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	constexpr int idList = 1;

	AutoComplete* pThis = (AutoComplete*) GetWindowLongPtr(hWnd, GWLP_USERDATA);

	switch (uMsg)
	{
		case WM_NCCREATE:
		{
			CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
			break;
		}
		case WM_CREATE:
		{
			HWND hList = CreateWindow(
				WC_LISTVIEW,
				NULL,
				WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_NOCOLUMNHEADER | LVS_SINGLESEL,
				0, 0, 1, 1,
				hWnd,
				(HMENU) (uintptr_t) idList,
				g_hInstance,
				NULL
			);

			pThis->m_listHwnd = hList;
			if (!hList)
				break;

			SetWindowFont(hList, pThis->m_hFont, FALSE);
			ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT);
			break;
		}
		case WM_SIZE:
		{
			HWND hList = GetDlgItem(hWnd, idList);
			if (!hList)
				break;

			MoveWindow(hList, 0, 0, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), TRUE);
			ListView_DeleteColumn(hList, 0);
			ListView_DeleteColumn(hList, 0);

			TCHAR colText[] = TEXT("NAME");
			LVCOLUMN lvc;
			ZeroMemory(&lvc, sizeof lvc);
			lvc.mask = LVCF_TEXT | LVCF_WIDTH;
			lvc.pszText = colText;
			lvc.cx = pThis->m_widthColumn1;
			ListView_InsertColumn(hList, COL_NAME, &lvc);

			if (pThis->m_widthColumn2)
			{
				TCHAR colTextSub[] = TEXT("SUBNAME");
				LVCOLUMN lvc;
				ZeroMemory(&lvc, sizeof lvc);
				lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
				lvc.pszText = colTextSub;
				lvc.cx = pThis->m_widthColumn2;
				lvc.fmt = LVCFMT_RIGHT;
				ListView_InsertColumn(hList, COL_SUBNAME, &lvc);
			}
			break;
		}
		case WM_GETCOMPINDEX:
		{
			HWND hList = GetDlgItem(hWnd, idList);
			if (!hList)
				return -1;
			return ListView_GetNextItem(hList, -1, LVNI_SELECTED);
		}
		case WM_UPDATECOMPITEMS:
		{
			HWND hList = GetDlgItem(hWnd, idList);
			if (!hList)
				break;

			ListView_DeleteAllItems(hList);

			int i = 0;
			for (const auto& entry : pThis->m_matches)
			{
				LVITEM lvi;
				ZeroMemory(&lvi, sizeof lvi);
				lvi.mask = LVIF_TEXT;
				lvi.pszText  = LPSTR_TEXTCALLBACK;
				lvi.iItem    = i;
				ListView_InsertItem(hList, &lvi);
				i++;
			}

			ListView_SetItemState(hList, 0, LVIS_SELECTED, LVIS_SELECTED);
			break;
		}
		case WM_NOTIFY:
		{
			NMHDR* phdr = (NMHDR*)lParam;

			switch (phdr->code)
			{
				case NM_CUSTOMDRAW:
					return pThis->HandleCustomDraw(hWnd, (NMLVCUSTOMDRAW*)lParam);

				case LVN_GETDISPINFO:
					return pThis->HandleGetDispInfo(hWnd, (NMLVDISPINFO*) lParam);
			}
			break;
		}
		case WM_DESTROY:
		{
			if (pThis->m_listHwnd)
			{
				BOOL b = DestroyWindow(pThis->m_listHwnd);
				assert(b && "Was window already destroyed?");
				pThis->m_listHwnd = NULL;
			}

			pThis->m_hwnd = NULL;
			break;
		}
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

LRESULT AutoComplete::HandleCustomDraw(HWND hWnd, NMLVCUSTOMDRAW* pInfo)
{
	switch (pInfo->nmcd.dwDrawStage)
	{
		case CDDS_PREPAINT:
			return CDRF_NOTIFYITEMDRAW;

		case CDDS_ITEMPREPAINT:
			return CDRF_NOTIFYSUBITEMDRAW;

		case CDDS_SUBITEM | CDDS_ITEMPREPAINT: {
			HWND hList = m_listHwnd;

			int idx = (int) pInfo->nmcd.dwItemSpec;
			int sta = ListView_GetItemState(hList, idx, LVIS_SELECTED);

			if (sta & LVIS_SELECTED) {
				pInfo->clrTextBk = GetSysColor(COLOR_HIGHLIGHT);
				pInfo->clrText = GetSysColor(COLOR_HIGHLIGHTTEXT);
			}
			else if (pInfo->iSubItem == COL_SUBNAME) {
				pInfo->clrTextBk = GetSysColor(COLOR_WINDOW);
				pInfo->clrText = GetSysColor(COLOR_GRAYTEXT);
			}
			else {
				pInfo->clrTextBk = GetSysColor(COLOR_WINDOW);
				pInfo->clrText = GetSysColor(COLOR_MENUTEXT);
			}

			return CDRF_NEWFONT;
		}

		default:DbgPrintW("Unhandled draw stage %d", pInfo->nmcd.dwDrawStage);
	}

	return CDRF_DODEFAULT;
}

LRESULT AutoComplete::HandleGetDispInfo(HWND hWnd, NMLVDISPINFO* pInfo)
{
	if (pInfo->item.iItem < 0 || pInfo->item.iItem >= int(m_matches.size()))
		return S_OK;

	AutoCompleteMatch& match = m_matches[pInfo->item.iItem];

	switch (pInfo->item.iSubItem)
	{
		case COL_NAME:
		{
			ConvertCppStringToTCharArray(match.GetDisplayString(), m_dispInfoBuffer1, _countof(m_dispInfoBuffer1));
			pInfo->item.pszText = m_dispInfoBuffer1;
			break;
		}
		case COL_SUBNAME:
		{
			ConvertCppStringToTCharArray(match.substr, m_dispInfoBuffer2, _countof(m_dispInfoBuffer2));
			pInfo->item.pszText = m_dispInfoBuffer2;
			break;
		}
	}

	return S_OK;
}
