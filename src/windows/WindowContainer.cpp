#include "WindowContainer.hpp"
#include "WinUtils.hpp"
#include "Main.hpp"

#define T_WINDOW_CONTAINER_CLASS TEXT("DMWindowContainerClass")

void WindowContainer::InitializeClass()
{
	WNDCLASS wc;
	ZeroMemory(&wc, sizeof wc);
	wc.lpszClassName = T_WINDOW_CONTAINER_CLASS;
	wc.hbrBackground = GetSysColorBrushV2(COLOR_3DFACE);
	wc.style = 0;
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.lpfnWndProc = WindowContainer::WndProc;
	wc.hInstance = g_hInstance;

	RegisterClass(&wc);
}

HWND WindowContainer::InitWindow(DWORD dwExStyle, LPCTSTR className, LPCTSTR windowName, DWORD dwStyle, int X, int Y, int width, int height, HWND hwndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
	m_exStyle = dwExStyle;
	m_style = dwStyle;

	int orgX = 0, orgY = 0;

	// force some things on which this won't work without.
	dwStyle |= WS_CHILD | WS_VISIBLE;

	// Create the container window.
	m_parentHwnd = CreateWindow(T_WINDOW_CONTAINER_CLASS, TEXT(""), WS_CHILD | WS_VISIBLE, X, Y, width, height, hwndParent, hMenu, hInstance, this);
	if (!m_parentHwnd) {
		DbgPrintW("Could not create container HWND!");
		return NULL;
	}

	// Make sure that client edge is handled now.
	if (dwExStyle & WS_EX_CLIENTEDGE) {
		dwExStyle &= ~WS_EX_CLIENTEDGE;

		int edgeWidth = GetSystemMetrics(SM_CXEDGE), edgeHeight = GetSystemMetrics(SM_CYEDGE);
		DbgPrintW("Edge Width: %d, Height: %d", edgeWidth, edgeHeight);

		if (!edgeWidth) edgeWidth = 2;
		if (!edgeHeight) edgeHeight = 2;

		orgX += edgeWidth;
		orgY += edgeHeight;
		width -= edgeWidth * 2;
		height -= edgeHeight * 2;
	}

	// Create the scroll bars now.
	if (dwStyle & WS_VSCROLL) {
		int scrollBarWidth = GetSystemMetrics(SM_CXVSCROLL);
		int scrollBarHeight = height;
		if (dwStyle & WS_HSCROLL) {
			scrollBarHeight -= GetSystemMetrics(SM_CYHSCROLL);
		}

		m_vscrollHwnd = CreateWindow(TEXT("SCROLLBAR"), NULL, WS_CHILD | WS_VISIBLE | SBS_VERT, orgX + width - scrollBarWidth, orgY, scrollBarWidth, scrollBarHeight, m_parentHwnd, NULL, hInstance, NULL);
		if (!m_vscrollHwnd) {
			KillWindow();
			return NULL;
		}
	}
	if (dwStyle & WS_HSCROLL) {
		int scrollBarWidth = width;
		int scrollBarHeight = GetSystemMetrics(SM_CYHSCROLL);
		if (dwStyle & WS_VSCROLL) {
			scrollBarHeight -= GetSystemMetrics(SM_CXVSCROLL);
		}

		m_hscrollHwnd = CreateWindow(TEXT("SCROLLBAR"), NULL, WS_CHILD | WS_VISIBLE | SBS_HORZ, orgX, orgY + height - scrollBarHeight, scrollBarWidth, scrollBarHeight, m_parentHwnd, NULL, hInstance, NULL);
		if (!m_hscrollHwnd) {
			KillWindow();
			return NULL;
		}
	}

	if (dwStyle & WS_VSCROLL)
		width -= GetSystemMetrics(SM_CXVSCROLL);
	if (dwStyle & WS_HSCROLL)
		height -= GetSystemMetrics(SM_CYHSCROLL);

	dwStyle &= ~(WS_HSCROLL | WS_VSCROLL);

	// Finally, create the actual window.
	m_hwnd = CreateWindowEx(dwExStyle, className, windowName, dwStyle, orgX, orgY, width, height, m_parentHwnd, hMenu, hInstance, lpParam);
	if (!m_hwnd) {
		KillWindow();
		return NULL;
	}

	return m_hwnd;
}

WindowContainer::WindowContainer()
{
}

WindowContainer::~WindowContainer()
{
	KillWindow();
}

void WindowContainer::KillWindow()
{
	if (m_hwnd) {
		DestroyWindow(m_hwnd);
		m_hwnd = NULL;
	}
	if (m_hscrollHwnd) {
		DestroyWindow(m_hscrollHwnd);
		m_hscrollHwnd = NULL;
	}
	if (m_vscrollHwnd) {
		DestroyWindow(m_vscrollHwnd);
		m_vscrollHwnd = NULL;
	}
	if (m_parentHwnd) {
		DestroyWindow(m_parentHwnd);
		m_parentHwnd = NULL;
	}
}

HWND WindowContainer::GetHWND()
{
	return m_hwnd;
}

HWND WindowContainer::GetVerticalScrollbarHWND()
{
	return m_vscrollHwnd;
}

HWND WindowContainer::GetHorizontalScrollbarHWND()
{
	return m_hscrollHwnd;
}

HWND WindowContainer::GetContainerHWND()
{
	return m_parentHwnd;
}

void WindowContainer::GetScrollInfo(int nBar, SCROLLINFO* outScrollInfo)
{
	if (nBar == SB_VERT || nBar == SB_BOTH) {
		if (m_vscrollHwnd) {
			ri::GetScrollInfo(m_vscrollHwnd, SB_CTL, outScrollInfo);
		}
	}
	if (nBar == SB_HORZ || nBar == SB_BOTH) {
		if (m_hscrollHwnd) {
			ri::GetScrollInfo(m_hscrollHwnd, SB_CTL, outScrollInfo);
		}
	}
	if (nBar != SB_HORZ && nBar != SB_VERT && nBar != SB_BOTH) {
		ri::GetScrollInfo(m_hwnd, nBar, outScrollInfo);
	}
}

void WindowContainer::SetScrollInfo(int nBar, SCROLLINFO* inScrollInfo, bool redraw)
{
	if (nBar == SB_VERT || nBar == SB_BOTH) {
		if (m_vscrollHwnd) {
			ri::SetScrollInfo(m_vscrollHwnd, SB_CTL, inScrollInfo, redraw);
		}
	}
	if (nBar == SB_HORZ || nBar == SB_BOTH) {
		if (m_hscrollHwnd) {
			ri::SetScrollInfo(m_hscrollHwnd, SB_CTL, inScrollInfo, redraw);
		}
	}
	if (nBar != SB_HORZ && nBar != SB_VERT && nBar != SB_BOTH) {
		ri::SetScrollInfo(m_hwnd, nBar, inScrollInfo, redraw);
	}
}

LRESULT WindowContainer::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	WindowContainer* pThis = (WindowContainer*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if (!pThis && uMsg != WM_NCCREATE) {
		DbgPrintW("WindowContainer::WndProc: called with no pThis?");
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	switch (uMsg)
	{
		case WM_NCCREATE:
		{
			CREATESTRUCT* pStruct = (CREATESTRUCT*) lParam;
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR) pStruct->lpCreateParams);
			break;
		}
		case WM_PAINT:
		{
			if (!(pThis->m_exStyle & WS_EX_CLIENTEDGE))
				break;

			PAINTSTRUCT ps = {};
			HDC hdc = BeginPaint(hWnd, &ps);

			RECT rect{};
			GetWindowRect(hWnd, &rect);
			OffsetRect(&rect, -rect.left, -rect.top);

			DrawEdgeV2(hdc, &rect, BDR_SUNKENINNER, BF_RECT);

			EndPaint(hWnd, &ps);
			return 0;
		}
		case WM_SIZE:
		{
			RECT rc;
			GetWindowRect(hWnd, &rc);

			int width = rc.right - rc.left;
			int height = rc.bottom - rc.top;

			// Handle client edge
			int orgX = 0, orgY = 0;
			if (pThis->m_exStyle & WS_EX_CLIENTEDGE)
			{
				int edgeWidth = GetSystemMetrics(SM_CXEDGE), edgeHeight = GetSystemMetrics(SM_CYEDGE);

				if (!edgeWidth) edgeWidth = 2;
				if (!edgeHeight) edgeHeight = 2;

				orgX += edgeWidth;
				orgY += edgeHeight;
				width -= edgeWidth * 2;
				height -= edgeHeight * 2;
			}

			int awidth = width, aheight = height;
			int sbwidth = 0, sbheight = 0;
			if (pThis->m_vscrollHwnd) {
				sbwidth = GetSystemMetrics(SM_CXVSCROLL);
				width -= sbwidth;
			}
			if (pThis->m_hscrollHwnd) {
				sbheight = GetSystemMetrics(SM_CYHSCROLL);
				height -= sbheight;
			}

			if (pThis->m_vscrollHwnd) {
				MoveWindow(pThis->m_vscrollHwnd, orgX + awidth - sbwidth, orgY, sbwidth, height, TRUE);
			}
			if (pThis->m_hscrollHwnd) {
				MoveWindow(pThis->m_hscrollHwnd, orgX, orgY + aheight - sbheight, width, sbheight, TRUE);
			}
			if (pThis->m_hwnd) {
				MoveWindow(pThis->m_hwnd, orgX, orgY, width, height, TRUE);
			}
			return 0;
		}
		case WM_VSCROLL:
		case WM_HSCROLL:
		{
			// Forward these to the inner window, they'll know how to handle it.
			SendMessage(pThis->m_hwnd, uMsg, wParam, lParam);
			break;
		}
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

WindowContainer* GetParentContainer(HWND hWnd)
{
	HWND parentHwnd = GetParent(hWnd);

	TCHAR className[64];
	if (!GetClassName(parentHwnd, className, 64))
	{
		// can't possibly be parented by a window container since our class name is <64 chars wide
		DbgPrintW("WindowContainer::GetParentContainer: Not parented by a WindowContainer (2)");
		return NULL;
	}

	if (_tcscmp(className, T_WINDOW_CONTAINER_CLASS) != 0)
	{
		// not the same name
		DbgPrintW("WindowContainer::GetParentContainer: Not parented by a WindowContainer");
		return NULL;
	}

	WindowContainer* pContainer = (WindowContainer*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
	return pContainer;
}