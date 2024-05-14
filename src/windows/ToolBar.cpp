#include "ToolBar.hpp"
#include "Main.hpp"

ToolBar* ToolBar::Create(HWND hParent)
{
    ToolBar* pBar = new ToolBar;
    pBar->m_hwnd = CreateWindowEx(
        0,
        TOOLBARCLASSNAME,
        NULL,
        WS_CHILD | TBSTYLE_WRAPABLE | TBSTYLE_LIST | TBSTYLE_FLAT,
        0, 0, 0, 0,
        hParent,
        (HMENU)CID_TOOLBAR,
        g_hInstance,
        NULL
    );

    int smcxicon = GetSystemMetrics(SM_CXICON);
    pBar->m_imageList = ImageList_Create(
        smcxicon, smcxicon,
        ILC_COLOR32 | ILC_MASK,
        16, 16
    );

    SendMessage(pBar->m_hwnd, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_MIXEDBUTTONS);

    pBar->m_iconPinned  = ImageList_AddIcon(pBar->m_imageList, (HICON)LoadImage(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_PIN)),     IMAGE_ICON, smcxicon, smcxicon, LR_CREATEDIBSECTION | LR_SHARED));
    pBar->m_iconMembers = ImageList_AddIcon(pBar->m_imageList, (HICON)LoadImage(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_MEMBERS)), IMAGE_ICON, smcxicon, smcxicon, LR_CREATEDIBSECTION | LR_SHARED));

    SendMessage(pBar->m_hwnd, TB_SETIMAGELIST, 0, (LPARAM) pBar->m_imageList);
    
    // Initialize button info.
    // IDM_NEW, IDM_OPEN, and IDM_SAVE are application-defined command constants.

    const int IDM_NEW  = 42;
    const int IDM_OPEN = 43;
    const int IDM_SAVE = 44;
    const int numButtons = 6;
    const int buttonStyles = BTNS_SHOWTEXT | TBSTYLE_AUTOSIZE;

    TBBUTTON tbButtons[numButtons] =
    {
        { MAKELONG(pBar->m_iconPinned,  0), IDM_NEW,  TBSTATE_ENABLED, buttonStyles, {0}, 0, (INT_PTR) TEXT("Pinned") },

        { MAKELONG(pBar->m_iconPinned,  0), IDM_NEW,  TBSTATE_ENABLED, buttonStyles, {0}, 0, (INT_PTR) TEXT("Pinned") },
        { MAKELONG(pBar->m_iconMembers, 0), IDM_OPEN, TBSTATE_ENABLED, buttonStyles, {0}, 0, (INT_PTR) TEXT("Show Members") },
        { 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, {0}, 0, (INT_PTR) 0 },
        { 100, 0, TBSTATE_ENABLED, TBSTYLE_SEP, {0}, 0, (INT_PTR) -1 },
        { 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, {0}, 0, (INT_PTR) 0 },
    };

    // Add buttons.
    SendMessage(pBar->m_hwnd, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
    SendMessage(pBar->m_hwnd, TB_ADDBUTTONS,       (WPARAM)numButtons,       (LPARAM)&tbButtons);

    SendMessage(pBar->m_hwnd, TB_AUTOSIZE, 0, 0);
    ShowWindow(pBar->m_hwnd, TRUE);

    if (!pBar->m_hwnd) {
        delete pBar;
        pBar = nullptr;
    }

	SetWindowLongPtr(pBar->m_hwnd, GWLP_USERDATA, (LONG_PTR) pBar);
    return pBar;
}


ToolBar::~ToolBar()
{
	if (m_hwnd) {
		BOOL b = DestroyWindow(m_hwnd);
		assert(b && "ToolBar was already destroyed");
		m_hwnd = NULL;
	}
}
