#include "LoadingMessage.hpp"

WNDCLASS LoadingMessage::g_LoadingMessageClass;

LoadingMessage::LoadingMessage()
{
}
LoadingMessage::~LoadingMessage()
{
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
		CreateWnd();

	ShowWindow(m_hwnd, SW_SHOWNORMAL);
	SetFocus(m_hwnd);
}

void LoadingMessage::Hide()
{
	DestroyWindow(m_hwnd);
	m_hwnd = NULL;
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
			break;
		}
		case WM_PAINT: {
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
			RECT rect{};
			GetClientRect(hWnd, &rect);

			RECT textRect = rect;
			rect.top += MulDiv(textRect.bottom - textRect.top, 3, 4);
			HGDIOBJ old = SelectObject(hdc, g_AuthorTextFont);
			COLORREF oldClr = SetBkColor(hdc, GetSysColor(COLOR_3DFACE));
			
			DrawText(hdc, TEXT("Connecting to Discord servers..."), -1, &rect, DT_CENTER);

			SelectObject(hdc, old);
			SetBkColor(hdc, oldClr);

			EndPaint(hWnd, &ps);
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
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.lpfnWndProc = LoadingMessage::WndProc;
	wc.hInstance = g_hInstance;

	RegisterClass(&wc);
}

LoadingMessage* LoadingMessage::Create(HWND hwnd, LPRECT pRect, int comboId)
{
	LoadingMessage* newThis = new LoadingMessage;

	newThis->parHwnd = hwnd;
	newThis->CreateWnd();

	return newThis;
}
