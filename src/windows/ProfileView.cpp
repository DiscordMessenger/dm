#include "ProfileView.hpp"

#define PROFILE_VIEW_COLOR  COLOR_3DFACE

WNDCLASS ProfileView::g_ProfileViewClass;

static int GetProfileBorderSize()
{
	return ScaleByDPI(Supports32BitIcons() ? (PROFILE_PICTURE_SIZE_DEF + 12) : 64);
}

ProfileView::ProfileView()
{
}

ProfileView::~ProfileView()
{
	if (m_hwnd)
	{
		BOOL b = DestroyWindow(m_hwnd);
		assert(b && "was window deleted?");
		m_hwnd = NULL;
	}

	if (m_name) {
		free(m_name);
		m_name = NULL;
	}

	if (m_username) {
		free(m_username);
		m_username = NULL;
	}
}

void ProfileView::Update()
{
	InvalidateRect(m_hwnd, NULL, false);
}

void ProfileView::SetData(LPTSTR name, LPTSTR username, eActiveStatus astatus, const std::string& avlnk)
{
	assert(!m_bAutonomous);

	if (m_name) free(m_name);
	if (m_username) free(m_username);

	m_name = name;
	m_username = username;
	m_avatarLnk = avlnk;
	m_activeStatus = astatus;

	Update();
}

void ProfileView::Paint(HDC hdc)
{
	RECT rect = {};
	GetClientRect(m_hwnd, &rect);

	LPTSTR sName, sUserName;
	bool freeNameAndUserName = false;
	std::string avatarLink;
	eActiveStatus activeStatus;

	if (m_bAutonomous)
	{
		Profile* pf = GetDiscordInstance()->GetProfile();

		std::string nameText = pf->m_discrim ? pf->m_name : pf->m_globalName;
		sName = ConvertCppStringToTString(nameText);

		std::string str;
		if (pf->m_status.size())
			str = pf->m_status;
		else if (pf->m_discrim)
			str = "#" + FormatDiscrim(pf->m_discrim);
		else
			str = pf->m_name;

		sUserName = ConvertCppStringToTString(str);

		activeStatus = pf->m_activeStatus;
		avatarLink = pf->m_avatarlnk;
		freeNameAndUserName = true;
	}
	else
	{
		sName = m_name;
		sUserName = m_username;
		activeStatus = m_activeStatus;
		avatarLink = m_avatarLnk;
		freeNameAndUserName = false;
	}

	bool hasAlpha = false;
	HBITMAP hbm = GetAvatarCache()->GetBitmap(avatarLink, hasAlpha);
	int pfpSize = GetProfilePictureSize();
	int pfpBorderSize = ScaleByDPI(PROFILE_PICTURE_SIZE_DEF + 12);
	int pfpBorderSizeDrawn = GetProfileBorderSize();
	int pfpX = rect.left + ScaleByDPI(6);
	int pfpY = rect.top + ScaleByDPI(6) + (rect.bottom - rect.top - pfpBorderSize) / 2;

	RECT rcProfile = rect;
	rcProfile.right = pfpX + pfpBorderSize - ScaleByDPI(6);
	FillRect(hdc, &rcProfile, GetSysColorBrush(PROFILE_VIEW_COLOR));
	DrawIconEx(hdc, pfpX - ScaleByDPI(6), pfpY - ScaleByDPI(4), g_ProfileBorderIcon, pfpBorderSizeDrawn, pfpBorderSizeDrawn, 0, NULL, DI_NORMAL | DI_COMPAT);
	DrawBitmap(hdc, hbm, pfpX, pfpY, NULL, CLR_NONE, pfpSize, 0, hasAlpha);

	DrawActivityStatus(hdc, pfpX, pfpY, activeStatus);

	COLORREF clrOld = SetBkColor(hdc, GetSysColor(PROFILE_VIEW_COLOR));
	COLORREF cltOld = SetTextColor(hdc, GetSysColor(COLOR_MENUTEXT));
	HGDIOBJ objOld = SelectObject(hdc, g_AccountInfoFont);

	RECT rcText = rect;
	rcText.left = pfpX + pfpBorderSize - ScaleByDPI(6);
	rcText.bottom = (rect.top + rect.bottom) / 2;
	DrawText(hdc, sName, -1, &rcText, DT_BOTTOM | DT_SINGLELINE | DT_WORD_ELLIPSIS);

	SelectObject(hdc, g_AccountTagFont);

	rcText = rect;
	rcText.left = pfpX + pfpBorderSize - ScaleByDPI(6);
	rcText.top = (rect.top + rect.bottom) / 2;
	DrawText(hdc, sUserName, -1, &rcText, DT_TOP | DT_SINGLELINE | DT_WORD_ELLIPSIS);

	if (freeNameAndUserName) {
		free(sName);
		free(sUserName);
	}

	SelectObject(hdc, objOld);
	SetTextColor(hdc, cltOld);
	SetBkColor(hdc, clrOld);
}

LRESULT CALLBACK ProfileView::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	ProfileView* pThis = (ProfileView*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	switch (uMsg)
	{
		case WM_NCCREATE:
		{
			CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
			break;
		}
		case WM_DESTROY:
		{
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR) NULL);
			pThis->m_hwnd = NULL;
			break;
		}
		case WM_PAINT:
		{
			PAINTSTRUCT ps = {};
			HDC hdc = BeginPaint(hWnd, &ps);
			pThis->Paint(hdc);
			EndPaint(hWnd, &ps);
			break;
		}
		case WM_PRINT:
		case WM_PRINTCLIENT:
			pThis->Paint((HDC) wParam);
			break;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void ProfileView::InitializeClass()
{
	WNDCLASS& wc = g_ProfileViewClass;

	wc.lpszClassName = T_PROFILE_VIEW_CLASS;
	wc.hbrBackground = GetSysColorBrush(PROFILE_VIEW_COLOR);
	wc.style         = 0;
	wc.hCursor       = LoadCursor(0, IDC_ARROW);
	wc.lpfnWndProc   = ProfileView::WndProc;

	RegisterClass(&wc);
}

ProfileView* ProfileView::Create(HWND hwnd, LPRECT pRect, int id, bool autonomous)
{
	ProfileView* newThis = new ProfileView;
	newThis->m_bAutonomous = autonomous;

	int width = pRect->right - pRect->left, height = pRect->bottom - pRect->top;

	newThis->m_hwnd = CreateWindowEx(
		0,
		T_PROFILE_VIEW_CLASS,
		NULL,
		WS_CHILD | WS_VISIBLE,
		pRect->left, pRect->top,
		width, height,
		hwnd,
		(HMENU) id,
		g_hInstance,
		newThis
	);

	return newThis;
}

