#include "MessageView.hpp"
#include "Main.hpp"

#define T_MESSAGE_VIEW_CLASS TEXT("DMMessageViewClass")
WNDCLASS MessageView::g_MessageViewClass;

static MessageView* g_pMainMessageView;
static std::map<Snowflake, MessageView*> g_pViewsByChannel;

MessageView* GetMainMessageView()
{
	return g_pMainMessageView;
}

MessageView* GetMessageViewForChannel(Snowflake channel)
{
	auto iter = g_pViewsByChannel.find(channel);
	if (iter == g_pViewsByChannel.end())
		return nullptr;

	return iter->second;
}

MessageView::MessageView(Snowflake chan)
{
	m_currentChannel = chan;
}

MessageView::~MessageView()
{
	assert(!m_hwnd);
}

void MessageView::Activate()
{
	SetWindowPos(m_hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
}

void MessageView::Initialize()
{
	// Initialize the main class.
	WNDCLASS& wc = g_MessageViewClass;

	wc.lpszClassName = T_MESSAGE_VIEW_CLASS;
	wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
	wc.style = 0;
	wc.lpfnWndProc = MessageView::WndProc;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);

	if (!RegisterClass(&wc)) {
		DbgPrintW("Error, cannot register MessageView class");
		std::terminate();
	}
}

bool MessageView::CreateView(HWND hParent, Snowflake channel)
{
	MessageView* pView;
	// Check if a view already exists for this channel.
	pView = GetMessageViewForChannel(channel);
	if (pView) {
		pView->Activate();
		return false;
	}

	pView = new MessageView(channel);

	Channel* pChan = GetDiscordInstance()->GetChannel(channel);
	if (!pChan) {
		DbgPrintW("Cannot find channel with ID %lld", channel);
		return false;
	}

	LPTSTR name = ConvertCppStringToTString("#" + pChan->m_name);

	pView->m_hwnd = CreateWindow(
		T_MESSAGE_VIEW_CLASS,
		name,
		WS_OVERLAPPEDWINDOW | WS_POPUP,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		400,
		400,
		hParent,
		NULL,
		g_hInstance,
		pView
	);

	free(name);

	if (!pView->m_hwnd) {
		DbgPrintW("Cannot create message view window.");
		delete pView;
		return false;
	}

	ShowWindow(pView->m_hwnd, SW_SHOW);
	UpdateWindow(pView->m_hwnd);

	g_pViewsByChannel[channel] = pView;
	return true;
}

void MessageView::OnClosed(Snowflake channel)
{
	auto iter = g_pViewsByChannel.find(channel);
	if (iter == g_pViewsByChannel.end()) {
		assert(!"This shouldn't happen!");
		return;
	}

	MessageView* pView = iter->second;
	assert(pView);
	delete pView;
	
	iter->second = nullptr;
	g_pViewsByChannel.erase(iter);
}

LRESULT MessageView::OnEvent(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	HWND hWnd = m_hwnd;

	switch (uMsg)
	{
		case WM_DESTROY:
			assert(!"This shouldn't happen");

		default:
			return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
}

LRESULT MessageView::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	MessageView* pView = (MessageView*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
	
	if (uMsg == WM_NCCREATE)
	{
		pView = (MessageView*)((LPCREATESTRUCT)lParam)->lpCreateParams;
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pView);
		pView->m_hwnd = hWnd;
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	if (uMsg == WM_DESTROY)
	{
		assert(pView);

		// Clear the user data for this window.  This will cause it to be disconnected
		// from this MessageView instance.
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)0);

		// Disassociate the pView instance from a window.
		pView->m_hwnd = NULL;

		// Let the parent window know that the view is closed.  This will destroy
		// the MessageView instance.
		Snowflake sf = pView->m_currentChannel;
		HWND hPar = GetParent(hWnd);
		SendMessage(hPar, WM_MESSAGEVIEWCLOSED, 0, (LPARAM)&sf);
		pView = NULL;

		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	if (!pView)
	{
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	return pView->OnEvent(uMsg, wParam, lParam);
}
