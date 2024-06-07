#include "RoleList.hpp"
#include "../discord/LocalSettings.hpp"

#define ROLE_VIEWER_COLOR COLOR_WINDOW
#define GAP_BETWEEN_EDGE_AND_COLOR (5)

WNDCLASS RoleList::g_RoleListClass;

RoleList::RoleList()
{
}

RoleList::~RoleList()
{
	if (m_hwnd) {
		BOOL b = DestroyWindow(m_hwnd);
		assert(b && "was window destroyed?");
		m_hwnd = NULL;
	}
}

void RoleList::Update()
{
	LayoutRoles();
	InvalidateRect(m_hwnd, NULL, false);
}

void RoleList::AddRole(GuildRole& role)
{
	RoleItem item;
	item.m_role = role;
	item.m_rect = RECT();
	item.m_text = NULL;
	m_items.push_back(item);
}

void RoleList::ClearRoles()
{
	m_items.clear();
}

void RoleList::LayoutRoles()
{
	HDC hdc = GetDC(m_hwnd);
	RECT rect = {};
	GetClientRect(m_hwnd, &rect);

	// position of next role item
	const int initX = m_bHasBorder ? 2 : 0;
	POINT point = { initX, initX };

	const int vertGap = 6;
	int rowHeight = -vertGap;
	for (auto& item : m_items)
	{
		RECT rcMeasure = {};
		HGDIOBJ obj = SelectObject(hdc, g_MessageTextFont);
		DrawText(hdc, item.GetText(), -1, &rcMeasure, DT_CALCRECT | DT_NOPREFIX);
		SelectObject(hdc, obj);

		int colorSize = rcMeasure.bottom - rcMeasure.top + 4 - GAP_BETWEEN_EDGE_AND_COLOR * 2;
		int width = rcMeasure.right - rcMeasure.left + GAP_BETWEEN_EDGE_AND_COLOR * 3 + colorSize;

		RECT realRect{};
		if (point.x + width > rect.right)
		{
			point.x = initX;
			point.y += rowHeight + vertGap;
			rowHeight = 0;
		}

		realRect.left   = point.x;
		realRect.top    = point.y;
		realRect.right  = point.x + width;
		realRect.bottom = point.y + rcMeasure.bottom - rcMeasure.top + 4;

		if (rowHeight < rcMeasure.bottom - rcMeasure.top)
			rowHeight = rcMeasure.bottom - rcMeasure.top;

		item.m_rect = realRect;

		point.x = realRect.right + 2;
	}

	m_scrollHeight = point.y + rowHeight + (m_bHasBorder ? 4 : 0);

	ReleaseDC(m_hwnd, hdc);

	RECT rcClient = {};
	GetClientRect(m_hwnd, &rcClient);
	// Adjust the scroll range and page size when the window is resized
	SCROLLINFO si = { sizeof(SCROLLINFO) };
	si.fMask = SIF_RANGE | SIF_PAGE;
	si.nMin = 0;
	si.nMax = m_scrollHeight; // Adjust this value depending on the total height of child controls
	si.nPage = rcClient.bottom - rcClient.top; // Set to the height of the client area
	SetScrollInfo(m_hwnd, SB_VERT, &si, TRUE);
}

int RoleList::GetRolesHeight()
{
	int top = 1000000000, btm = -1000000000;
	for (auto& item : m_items) {
		top = std::min(top, (int) item.m_rect.top);
		btm = std::max(btm, (int) item.m_rect.bottom);
	}

	return btm - top;
}

void RoleList::DrawRole(HDC hdc, RoleItem* role)
{
	eMessageStyle style = GetLocalSettings()->GetMessageStyle();

	RECT rect = role->m_rect;
	rect.top    -= m_scrollY;
	rect.bottom -= m_scrollY;
	int oldBkMode = 0; bool oldBkModeSet = false;
	COLORREF oldBkColor = CLR_NONE;
	switch (style) {
		case MS_3DFACE:
			DrawEdge(hdc, &rect, BDR_RAISEDINNER, BF_RECT | BF_MIDDLE | BF_ADJUST);
		case MS_FLAT:
		case MS_IMAGE:
			oldBkColor = SetBkColor(hdc, GetSysColor(COLOR_3DFACE));
			break;
		case MS_FLATBR:
			oldBkColor = SetBkColor(hdc, GetSysColor(COLOR_WINDOW));
			break;
		case MS_GRADIENT:
			oldBkMode = SetBkMode(hdc, TRANSPARENT);
			oldBkModeSet = true;
			FillGradient(hdc, &rect, COLOR_WINDOW, COLOR_3DFACE, true);
			break;
	}
	HGDIOBJ obj = SelectObject(hdc, g_MessageTextFont);

	RECT rcColor{};
	rcColor.top    = rect.top    + GAP_BETWEEN_EDGE_AND_COLOR;
	rcColor.bottom = rect.bottom - GAP_BETWEEN_EDGE_AND_COLOR;
	int size = rcColor.bottom - rcColor.top;
	rcColor.left   = rect.left   + GAP_BETWEEN_EDGE_AND_COLOR;
	rcColor.right  = rect.left   + GAP_BETWEEN_EDGE_AND_COLOR + size;

	RECT rcText = rect;
	rcText.left = rcColor.right + GAP_BETWEEN_EDGE_AND_COLOR;
	
	int col = role->m_role.m_colorOriginal;
	COLORREF cref = RGB((col >> 16) & 0xFF, (col >> 8) & 0xFF, (col) & 0xFF);

	DrawShinyRoleColor(hdc, &rcColor, cref);

	COLORREF col2 = SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
	DrawText(hdc, role->GetText(), -1, &rcText, DT_NOPREFIX | DT_VCENTER | DT_SINGLELINE | DT_CENTER);
	SetTextColor(hdc, col2);
	SelectObject(hdc, obj);

	if (oldBkColor != CLR_NONE)
		SetBkColor(hdc, oldBkColor);
	if (oldBkModeSet)
		SetBkMode(hdc, oldBkMode);
}

void RoleList::DrawShinyRoleColor(HDC hdc, const LPRECT pRect, COLORREF base)
{
	COLORREF old = ri::SetDCBrushColor(hdc, base);
	FillRect(hdc, pRect, GetStockBrush(DC_BRUSH));
	ri::SetDCBrushColor(hdc, old);

	int left = pRect->left;
	int top  = pRect->top;
	int width = pRect->right - pRect->left;
	int height = pRect->bottom - pRect->top;

	HGDIOBJ obj = SelectObject(hdc, GetStockPen(DC_PEN));
	POINT oldPt;
	MoveToEx(hdc, 0, 0, &oldPt);

	COLORREF highlight = LerpColor(base, RGB(255, 255, 255), 1, 2); // halfway between
	COLORREF light = LerpColor(base, RGB(255, 255, 255), 1, 4);
	COLORREF dark = LerpColor(base, RGB(0, 0, 0), 1, 4);
	ri::SetDCPenColor(hdc, light);
	MoveToEx(hdc, left, top, NULL);
	LineTo(hdc, left + width - 1, top);
	MoveToEx(hdc, left, top, NULL);
	LineTo(hdc, left, top + height - 1);
	ri::SetDCPenColor(hdc, dark);
	MoveToEx(hdc, left + width - 1, top + height - 1, NULL);
	LineTo(hdc, left + width - 1, top - 1);
	MoveToEx(hdc, left + width - 1, top + height - 1, NULL);
	LineTo(hdc, left - 1, top + height - 1);

	top++; width -= 2;
	left++; height -= 2;
	int mn = std::min(width, height);

	for (int i = 0; i < mn; i++)
	{
		ri::SetDCPenColor(hdc, LerpColor(highlight, base, i, mn));
		POINT p1, p2;
		p1.x = left + i;
		p1.y = top;
		p2.x = left - 1;
		p2.y = top + i + 1;
		// TODO: don't know why I have to offset p2 by (-1,1)
		MoveToEx(hdc, p1.x, p1.y, NULL);
		LineTo(hdc, p2.x, p2.y);
	}

	MoveToEx(hdc, oldPt.x, oldPt.y, NULL);
	SelectObject(hdc, obj);
}

LRESULT CALLBACK RoleList::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	RoleList* pThis = (RoleList*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	int yPos = 0;
	bool useYPos = false;

	switch (uMsg)
	{
		case WM_NCCREATE:
		{
			CREATESTRUCT* strct = (CREATESTRUCT*)lParam;
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)strct->lpCreateParams);
			break;
		}
		case WM_DESTROY:
		{
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR) NULL);
			pThis->m_hwnd = NULL;
			break;
		}
		case WM_CREATE: {
			eMessageStyle style = GetLocalSettings()->GetMessageStyle();
			switch (style) {
				default:
					SetClassLong(hWnd, GCLP_HBRBACKGROUND, (LONG_PTR)GetSysColorBrush(COLOR_3DFACE));
					break;
				case MS_FLATBR:
					SetClassLong(hWnd, GCLP_HBRBACKGROUND, (LONG_PTR)GetSysColorBrush(COLOR_WINDOW));
					break;
			}
			break;
		}
		case WM_REPOSITIONROLES:
		{
			pThis->LayoutRoles();
			InvalidateRect(hWnd, NULL, true);
			return 0;
		}
		case WM_SIZE: {
			pThis->LayoutRoles();
			SCROLLINFO si = { sizeof(SCROLLINFO) };
			si.cbSize = sizeof(SCROLLINFO);
			si.fMask = SIF_POS;
			GetScrollInfo(hWnd, SB_VERT, &si);
			if (pThis->m_scrollY != si.nPos) {
				pThis->m_scrollY = si.nPos;
				InvalidateRect(hWnd, NULL, TRUE);
			} else {
				InvalidateRect(hWnd, NULL, FALSE);
			}
			break;
		}
		case WM_MOUSEWHEEL: {
			SCROLLINFO si{};
			si.cbSize = sizeof(SCROLLINFO);
			si.fMask = SIF_ALL;
			GetScrollInfo(hWnd, SB_VERT, &si);
			short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
			yPos = si.nTrackPos = si.nPos - zDelta;
			SetScrollInfo(hWnd, SB_VERT, &si, false);
			useYPos = true;
			goto label_scroll;
		}
		case WM_VSCROLL: {
		label_scroll:
			SCROLLINFO si{};
			si.cbSize = sizeof(SCROLLINFO);
			si.fMask = SIF_ALL;
			GetScrollInfo(hWnd, SB_VERT, &si);
			RECT rcClient = {};
			GetClientRect(hWnd, &rcClient);

			if (!useYPos) {
				yPos = si.nPos;

				switch (LOWORD(wParam)) {
					case SB_LINEUP:
						yPos -= 10;
						break;
					case SB_LINEDOWN:
						yPos += 10;
						break;
					case SB_PAGEUP:
						yPos -= si.nPage;
						break;
					case SB_PAGEDOWN:
						yPos += si.nPage;
						break;
					case SB_THUMBTRACK:
						yPos = HIWORD(wParam);
						break;
				}
			}

			// Ensure yPos is within the scroll range
			yPos = std::max(0, yPos);
			yPos = std::min(si.nMax - (int)si.nPage + 1, yPos);

			// Adjust the delta position for smooth scrolling
			int nDeltaPos = yPos - si.nPos;

			// Set the new scroll position
			si.fMask = SIF_POS;
			si.nPos = yPos;
			SetScrollInfo(hWnd, SB_VERT, &si, TRUE);

			pThis->m_scrollY = si.nPos;

			// Scroll the child window
			ScrollWindow(hWnd, 0, -nDeltaPos, NULL, NULL);

			RECT rcInvalidate = rcClient;
			if (nDeltaPos < 0) {
				rcInvalidate.top = rcInvalidate.bottom - nDeltaPos;
			}
			else {
				rcInvalidate.bottom = rcInvalidate.top + nDeltaPos;
			}
			InvalidateRect(hWnd, &rcInvalidate, false);
			break;
		}
		case WM_LBUTTONUP:
		{
			SetFocus(hWnd);
			break;
		}
		case WM_PAINT:
		{
			PAINTSTRUCT ps = {};
			RECT rect = {};
			GetClientRect(hWnd, &rect);

			HDC hdc = BeginPaint(hWnd, &ps);

			for (auto& item : pThis->m_items) {
				pThis->DrawRole(hdc, &item);
			}

			EndPaint(hWnd, &ps);
			break;
		}
		case WM_PRINT:
		case WM_PRINTCLIENT:
		{
			HDC hdc = (HDC) wParam;
			for (auto& item : pThis->m_items) {
				pThis->DrawRole(hdc, &item);
			}
			break;
		}
		case WM_GESTURE:
		{
			HandleGestureMessage(hWnd, uMsg, wParam, lParam);
			break;
		}
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void RoleList::InitializeClass()
{
	WNDCLASS& wc = g_RoleListClass;
	if (wc.lpfnWndProc)
		return;

	wc.lpszClassName = T_ROLE_LIST_CLASS;
	wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
	wc.style = 0;
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.lpfnWndProc = RoleList::WndProc;
	wc.hInstance = g_hInstance;

	RegisterClass(&wc);
}

RoleList* RoleList::Create(HWND hwnd, LPRECT pRect, int comboId, bool border, bool visible)
{
	RoleList* newThis = new RoleList;

	int width = pRect->right - pRect->left, height = pRect->bottom - pRect->top;

	newThis->m_bHasBorder = border;
	newThis->m_hwnd = CreateWindowEx(
		border ? WS_EX_CLIENTEDGE : 0,
		T_ROLE_LIST_CLASS,
		NULL,
		WS_CHILD | WS_VSCROLL | (visible ? WS_VISIBLE : 0),
		pRect->left, pRect->top,
		width, height,
		hwnd,
		(HMENU)comboId,
		g_hInstance,
		newThis
	);

	return newThis;
}
