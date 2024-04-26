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
}

void ProfileView::Update()
{
	InvalidateRect(m_hwnd, NULL, false);
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
			RECT rect = {};
			GetClientRect(hWnd, &rect);

			HDC hdc =
			BeginPaint(hWnd, &ps);

			Profile* pf = GetDiscordInstance()->GetProfile();
			HBITMAP hbm = GetAvatarCache()->GetBitmap(pf->m_avatarlnk);
			int pfpSize = GetProfilePictureSize();
			int pfpBorderSize = ScaleByDPI(PROFILE_PICTURE_SIZE_DEF + 12);
			int pfpBorderSizeDrawn = GetProfileBorderSize();
			int pfpX = rect.left + ScaleByDPI(8);
			int pfpY = rect.top + ScaleByDPI(6) + (rect.bottom - rect.top - pfpBorderSize) / 2;

			RECT rcProfile = rect;
			rcProfile.right = pfpX + pfpBorderSize - ScaleByDPI(6);
			FillRect(hdc, &rcProfile, GetSysColorBrush(PROFILE_VIEW_COLOR));
			DrawIconEx(hdc, pfpX - ScaleByDPI(6), pfpY - ScaleByDPI(4), g_ProfileBorderIcon, pfpBorderSizeDrawn, pfpBorderSizeDrawn, 0, NULL, DI_NORMAL | DI_COMPAT);
			DrawBitmap(hdc, hbm, pfpX, pfpY, NULL, CLR_NONE, pfpSize);

			DrawActivityStatus(hdc, pfpX, pfpY, pf->m_activeStatus);

			COLORREF clrOld = SetBkColor(hdc, GetSysColor(PROFILE_VIEW_COLOR));
			COLORREF cltOld = SetTextColor(hdc, GetSysColor(COLOR_MENUTEXT));
			HGDIOBJ objOld = SelectObject(hdc, g_AccountInfoFont);

			std::string nameText = pf->m_discrim ? pf->m_name : pf->m_globalName;
			LPTSTR ctstr = ConvertCppStringToTString(nameText);
			RECT rcText = rect;
			rcText.left = pfpX + pfpBorderSize - ScaleByDPI(6);
			rcText.bottom = (rect.top + rect.bottom) / 2;
			DrawText(hdc, ctstr, -1, &rcText, DT_BOTTOM | DT_SINGLELINE | DT_WORD_ELLIPSIS);

			free(ctstr);

			SelectObject(hdc, g_AccountTagFont);
			std::string str;
			if (pf->m_status.size())
				str = pf->m_status;
			else if (pf->m_discrim)
				str = "#" + FormatDiscrim(pf->m_discrim);
			else
				str = pf->m_name;

			ctstr = ConvertCppStringToTString(str);

			rcText = rect;
			rcText.left = pfpX + pfpBorderSize - ScaleByDPI(6);
			rcText.top = (rect.top + rect.bottom) / 2;
			DrawText(hdc, ctstr, -1, &rcText, DT_TOP | DT_SINGLELINE | DT_WORD_ELLIPSIS);

			free(ctstr);
			SelectObject(hdc, objOld);
			SetTextColor(hdc, cltOld);
			SetBkColor(hdc, clrOld);
			EndPaint(hWnd, &ps);
			break;
		}
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

ProfileView* ProfileView::Create(HWND hwnd, LPRECT pRect)
{
	ProfileView* newThis = new ProfileView;

	int width = pRect->right - pRect->left, height = pRect->bottom - pRect->top;

	newThis->m_hwnd = CreateWindowEx(
		0,
		T_PROFILE_VIEW_CLASS,
		NULL,
		WS_CHILD | WS_VISIBLE,
		pRect->left, pRect->top,
		width, height,
		hwnd,
		(HMENU)CID_MESSAGELIST,
		g_hInstance,
		newThis
	);

	return newThis;
}

