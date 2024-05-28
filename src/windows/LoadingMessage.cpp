#include "LoadingMessage.hpp"
#define C_NUM_LOADING_DOTS (10)
#define C_LOADING_ELAPSE   (50)

WNDCLASS LoadingMessage::g_LoadingMessageClass;

LoadingMessage::LoadingMessage()
{
}

LoadingMessage::~LoadingMessage()
{
	Hide();
}

void LoadingMessage::CreateWnd()
{
	RECT rcLoading = { 0, 0, ScaleByDPI(300), ScaleByDPI(200) };
	RECT rect = {};
	POINT pt{ 0, 0 };
	GetClientRect(parHwnd, &rect);
	ClientToScreen(parHwnd, &pt);
	OffsetRect(&rcLoading, rect.right / 2 - rcLoading.right / 2, rect.bottom / 2 - rcLoading.bottom / 2);
	OffsetRect(&rcLoading, pt.x, pt.y);
	AdjustWindowRect(&rcLoading, WS_OVERLAPPED, false);
	int width = rcLoading.right - rcLoading.left, height = rcLoading.bottom - rcLoading.top;

	assert(!m_hwnd);
	m_hwnd = CreateWindowEx(
		0,
		T_LOADING_MESSAGE_CLASS,
		NULL,
		WS_OVERLAPPED,
		rcLoading.left,
		rcLoading.top,
		width, height,
		g_Hwnd,
		NULL,
		g_hInstance,
		this
	);
}

void LoadingMessage::Show()
{
	if (!m_hwnd)
	{
		CreateWnd();
		ShowWindow(m_hwnd, SW_SHOWNORMAL);
		SetFocus(m_hwnd);
	}
}

void LoadingMessage::Hide()
{
	if (m_hwnd) {
		BOOL b = DestroyWindow(m_hwnd);
		assert(b && "was window already destroyed?");
		m_hwnd = NULL;
	}
}

void LoadingMessage::DrawLoading(HDC hdc)
{
	RECT rect;
	GetClientRect(m_hwnd, &rect);
	rect.top += MulDiv(rect.bottom - rect.top, 3, 4);
	int iconSizeSm = GetSystemMetrics(SM_CXSMICON);

	// Draw loading bar
	int dotWidth = iconSizeSm - ScaleByDPI(8);
	int yLoad = rect.top - ScaleByDPI(12) - iconSizeSm / 2;
	int xLoad = (rect.right - rect.left - dotWidth * C_NUM_LOADING_DOTS) / 2;

	for (int i = 0; i < C_NUM_LOADING_DOTS; i++) {
		// XXX: crappy solution, done to allow smooth looparound
		int st1 = m_icon_stage;
		int st2 = (m_icon_stage + 1) % C_NUM_LOADING_DOTS;
		int st3 = (m_icon_stage + 2) % C_NUM_LOADING_DOTS;
		HICON ic = m_dot_0;
		if (i == st1 || i == st2 || i == st3)
			ic = m_dot_1;

		DrawIconEx(hdc, xLoad + i * dotWidth - (iconSizeSm - dotWidth) / 2, yLoad, ic, iconSizeSm, iconSizeSm, 0, NULL, DI_COMPAT | DI_NORMAL);
	}

	m_load_rect = { xLoad, yLoad, xLoad + dotWidth * C_NUM_LOADING_DOTS,yLoad + iconSizeSm };
}

LRESULT CALLBACK LoadingMessage::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LoadingMessage* pThis = (LoadingMessage*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	switch (uMsg)
	{
		case WM_NCCREATE:
		{
			CREATESTRUCT* strct = (CREATESTRUCT*)lParam;
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)strct->lpCreateParams);
			break;
		}
		case WM_CREATE: {
			int iconSize = GetSystemMetrics(SM_CXICON) * 2;
			int iconSizeSm = GetSystemMetrics(SM_CXSMICON);
			pThis->m_icon  = (HICON)LoadImage(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_ICON)), IMAGE_ICON, iconSize, iconSize, LR_CREATEDIBSECTION | LR_SHARED);
			pThis->m_dot_0 = (HICON)LoadImage(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_OFFLINE_SM)), IMAGE_ICON, iconSizeSm, iconSizeSm, LR_CREATEDIBSECTION | LR_SHARED);
			pThis->m_dot_1 = (HICON)LoadImage(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_ONLINE_SM)),  IMAGE_ICON, iconSizeSm, iconSizeSm, LR_CREATEDIBSECTION | LR_SHARED);
			pThis->m_timer_id = SetTimer(hWnd, 0, C_LOADING_ELAPSE, NULL);
			break;
		}
		case WM_TIMER: {
			pThis->m_icon_stage = (pThis->m_icon_stage + 1) % C_NUM_LOADING_DOTS;
			HDC hdc = GetDC(hWnd);
			pThis->DrawLoading(hdc);
			ReleaseDC(hWnd, hdc);
			break;
		}
		case WM_DESTROY: {
			pThis->m_hwnd = NULL;
			if (pThis->m_timer_id)
				KillTimer(hWnd, pThis->m_timer_id);
			pThis->m_timer_id = 0;
			break;
		}
		case WM_PAINT: {
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
			RECT rect{};
			GetClientRect(hWnd, &rect);

			RECT textRect = rect;
			textRect.top += MulDiv(textRect.bottom - textRect.top, 3, 4);
			HGDIOBJ old = SelectObject(hdc, g_AuthorTextFont);
			COLORREF oldClr = SetBkColor(hdc, GetSysColor(COLOR_3DFACE));
			
			DrawText(hdc, TEXT("Please wait, connecting to Discord..."), -1, &textRect, DT_CENTER);

			int iconSize = GetSystemMetrics(SM_CXICON) * 2;
			SelectObject(hdc, old);
			SetBkColor(hdc, oldClr);

			int x = (rect.right - rect.left - iconSize) / 2;
			int y = (textRect.top - rect.top - iconSize) / 2;
			DrawIconEx(hdc, x, y, pThis->m_icon, iconSize, iconSize, 0, NULL, DI_COMPAT | DI_NORMAL);

			pThis->DrawLoading(hdc);

			EndPaint(hWnd, &ps);
			break;
		}
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void LoadingMessage::InitializeClass()
{
	WNDCLASS& wc = g_LoadingMessageClass;
	if (wc.lpfnWndProc)
		return;

	wc.lpszClassName = T_LOADING_MESSAGE_CLASS;
	wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
	wc.style = CS_HREDRAW;
	wc.hCursor = LoadCursor(0, IDC_WAIT);
	wc.lpfnWndProc = LoadingMessage::WndProc;
	wc.hInstance = g_hInstance;

	RegisterClass(&wc);
}

LoadingMessage* LoadingMessage::Create(HWND hwnd, LPRECT pRect, int comboId)
{
	LoadingMessage* newThis = new LoadingMessage;
	newThis->parHwnd = hwnd;
	return newThis;
}
