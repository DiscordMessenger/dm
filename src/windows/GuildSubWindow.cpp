#include "GuildSubWindow.hpp"
#include "Main.hpp"

#define GUILD_SUB_WINDOW_CLASS TEXT("DiscordMessengerGuildSubWindowClass")

WNDCLASS GuildSubWindow::m_wndClass;
bool GuildSubWindow::InitializeClass()
{
	WNDCLASS& wc = m_wndClass;

	wc.lpfnWndProc   = WndProc;
	wc.hInstance     = g_hInstance;
	wc.lpszClassName = GUILD_SUB_WINDOW_CLASS;
	wc.hbrBackground = g_backgroundBrush;
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon         = g_Icon;
	wc.lpszMenuName  = MAKEINTRESOURCE(IDR_MAINMENU);

	return RegisterClass(&wc) != 0;
}

GuildSubWindow::GuildSubWindow()
{
	m_hwnd = CreateWindow(
		GUILD_SUB_WINDOW_CLASS,
		TEXT("Discord Messenger (sub-window)"),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		ScaleByDPI(700), // TODO: Specify non-hardcoded size
		ScaleByDPI(600),
		NULL,
		NULL,
		g_hInstance,
		this
	);

	if (!m_hwnd) {
		m_bInitFailed = true;
		return;
	}

	ShowWindow(m_hwnd, SW_SHOWNORMAL);
}

GuildSubWindow::~GuildSubWindow()
{
	DbgPrintW("GuildSubWindow destructor!");
}

void GuildSubWindow::Close()
{
	DestroyWindow(m_hwnd);
}

LRESULT GuildSubWindow::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_CREATE:
		{
			break;
		}
		case WM_CLOSE:
		{
			DestroyWindow(hWnd);
			break;
		}
	}

	return 0;
}

LRESULT GuildSubWindow::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	GuildSubWindow* pThis = (GuildSubWindow*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if (!pThis && uMsg != WM_NCCREATE)
		return DefWindowProc(hWnd, uMsg, wParam, lParam);

	if (uMsg == WM_NCCREATE)
	{
		CREATESTRUCT* strct = (CREATESTRUCT*) lParam;
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)strct->lpCreateParams);
	}

	if (pThis)
	{
		LRESULT lres = pThis->WindowProc(hWnd, uMsg, wParam, lParam);
		if (lres != 0)
			return lres;
	}

	if (uMsg == WM_DESTROY)
	{
		GetMainWindow()->OnClosedWindow(pThis);
		SetWindowLongPtr(hWnd, GWLP_USERDATA, NULL);
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
