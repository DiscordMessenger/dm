#include "AutoComplete.hpp"
#include "Main.hpp"

#define T_AUTOCOMPLETE_CLASS TEXT("DMAutoComplete")

AutoComplete::~AutoComplete()
{
	if (m_hwnd)
	{
		BOOL b = DestroyWindow(m_hwnd);
		assert(b && "Was the window already destroyed?");

		m_hwnd = NULL;
	}

	assert(!m_listHwnd);
}

void AutoComplete::ShowOrMove(POINT pt)
{
	bool oldReachedMaxX = m_bReachedMaxSizeX;
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
	const int maxHeight = ScaleByDPI(200);
	const int maxWidth = ScaleByDPI(200);
	m_bReachedMaxSizeX = false;
	m_bReachedMaxSizeY = false;
	for (auto& entry : m_matches)
	{
		RECT rcMeasure{};
		LPTSTR tstr = ConvertCppStringToTString(entry.str);
		DrawText(hdc, tstr, -1, &rcMeasure, DT_SINGLELINE | DT_CALCRECT);
		free(tstr);
		height += rcMeasure.bottom - rcMeasure.top + ScaleByDPI(4);

		int newWidth = rcMeasure.right - rcMeasure.left + ScaleByDPI(4);
		width = std::max(width, newWidth);

		if (width > maxWidth) {
			m_bReachedMaxSizeX = true;
			width = maxWidth;
		}

		if (height > maxHeight) {
			m_bReachedMaxSizeY = true;
			height = maxHeight;
		}
	}
	SelectObject(hdc, old);
	ReleaseDC(m_hwnd, hdc);

	// If reachedMaxSize is different, dismiss the old autocomplete window first and recreate:
	if (m_bReachedMaxSizeX != oldReachedMaxX || m_bReachedMaxSizeY != oldReachedMaxY)
		Hide();

	constexpr int style = WS_POPUP | WS_BORDER;
	width += ScaleByDPI(8);

	width += m_bReachedMaxSizeY ? GetSystemMetrics(SM_CXVSCROLL) : 0;
	height += m_bReachedMaxSizeX ? GetSystemMetrics(SM_CYHSCROLL) : 0;

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

	m_matches.clear();
	m_startAt = 0;
	m_bHadFirstArrowPress = false;
	m_bReachedMaxSizeX = false;
	m_bReachedMaxSizeY = false;
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
	m_lookup(stuff, query, m_matches);

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

bool AutoComplete::InitializeClass()
{
	WNDCLASS wc;
	ZeroMemory(&wc, sizeof wc);
	wc.lpszClassName = T_AUTOCOMPLETE_CLASS;
	wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
	wc.style         = 0;
	wc.hCursor       = LoadCursor (0, IDC_ARROW);
	wc.lpfnWndProc   = AutoComplete::WndProc;

	return RegisterClass(&wc);
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
				WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_NOCOLUMNHEADER | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
				0, 0, 1, 1,
				hWnd,
				(HMENU)idList,
				g_hInstance,
				NULL
			);

			pThis->m_listHwnd = hList;
			if (!hList)
				break;

			SetWindowFont(hList, pThis->m_hFont, FALSE);
			break;
		}
		case WM_SIZE:
		{
			HWND hList = GetDlgItem(hWnd, idList);
			if (!hList)
				break;

			MoveWindow(hList, 0, 0, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), TRUE);
			ListView_DeleteColumn(hList, 0);

			TCHAR colText[] = TEXT("NAME");
			LVCOLUMN lvc;
			ZeroMemory(&lvc, sizeof lvc);
			lvc.mask = LVCF_TEXT | LVCF_WIDTH;
			lvc.pszText = colText;
			lvc.cx = GET_X_LPARAM(lParam);
			ListView_InsertColumn(hList, 0, &lvc);
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
			for (auto& entry : pThis->m_matches)
			{
				LPTSTR str = ConvertCppStringToTString(entry.str);

				LVITEM lvi;
				ZeroMemory(&lvi, sizeof lvi);
				lvi.mask = LVIF_TEXT;
				lvi.pszText = str;
				lvi.iItem = i++;

				ListView_InsertItem(hList, &lvi);
				free(str);
			}

			ListView_SetItemState(hList, 0, LVIS_SELECTED, LVIS_SELECTED);
			break;
		}
		case WM_DESTROY:
		{
			BOOL b = DestroyWindow(pThis->m_listHwnd);
			assert(b && "Was window already destroyed?");
			pThis->m_listHwnd = NULL;
			break;
		}
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
