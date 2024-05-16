#include "GuildHeader.hpp"
#include "PinnedMessageViewer.hpp"

#define GUILD_HEADER_COLOR   COLOR_ACTIVECAPTION
#define GUILD_HEADER_COLOR_2 COLOR_GRADIENTACTIVECAPTION
#define CHANNEL_ICON_SIZE 16

#define IDTB_MEMBERS (1000)
#define IDTB_PINS    (1001)

WNDCLASS GuildHeader::g_GuildHeaderClass;

GuildHeader::~GuildHeader()
{
	if (m_hwnd)
	{
		BOOL b = DestroyWindow(m_hwnd);
		assert(b && "window was already destroyed??");
		m_hwnd = NULL;
	}

	for (auto& icon : m_hIcons) {
		DestroyIcon(icon.second);
	}

	m_hIcons.clear();
}

GuildHeader::GuildHeader()
{
	m_channelIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_CHANNEL)));
	m_voiceIcon   = LoadIcon(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_VOICE)));
	m_dmIcon      = LoadIcon(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_DM)));
	m_groupDmIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_GROUPDM)));

	m_buttons.push_back(Button(IDTB_MEMBERS, DMIC(IDI_MEMBERS)));
	m_buttons.push_back(Button(IDTB_PINS,    DMIC(IDI_PIN)));
}

std::string GuildHeader::GetGuildName()
{
	Guild* pGuild = GetDiscordInstance()->GetCurrentGuild();
	if (!pGuild)
		return "";
	return pGuild->m_name;
}

std::string GuildHeader::GetSubtitleText()
{
	return "";
}

std::string GuildHeader::GetChannelName()
{
	Channel *pChannel = GetDiscordInstance()->GetCurrentChannel();
	if (!pChannel)
		return "";
	return pChannel->m_name;
}

std::string GuildHeader::GetChannelInfo()
{
	Channel* pChannel = GetDiscordInstance()->GetCurrentChannel();
	if (!pChannel)
		return "";
	return pChannel->m_topic;
}

void GuildHeader::Update()
{
	InvalidateRect(m_hwnd, NULL, false);
}

void GuildHeader::LayoutButton(Button& button, RECT& toolbarRect)
{
	button.m_rect = toolbarRect;
	int tbrh = toolbarRect.bottom - toolbarRect.top;

	if (button.m_bRightJustify) {
		button.m_rect.left = button.m_rect.right - tbrh;
		toolbarRect.right = button.m_rect.left;
	}
	else {
		button.m_rect.right = button.m_rect.left + tbrh;
		toolbarRect.left = button.m_rect.right;
	}
}

void GuildHeader::Layout()
{
	RECT rect = {};
	GetClientRect(m_hwnd, &rect);

	m_fullRect = rect;

	RECT& rrect1 = m_rectLeft, & rrect2 = m_rectMidFull, & rrect3 = m_rectRight;
		
	rrect1 = rect, rrect2 = rect, rrect3 = rect;
	rrect1.right = rrect1.left + ScaleByDPI(CHANNEL_VIEW_LIST_WIDTH);
	rrect2.left  = rrect1.right;
	rrect3.left  = rrect3.right - ScaleByDPI(MEMBER_LIST_WIDTH);
	rrect2.right = rrect3.left;

	m_rectMid = m_rectMidFull;
	m_rectMid.left  += ScaleByDPI(10);
	m_rectMid.right -= ScaleByDPI(10);
	m_minRightToolbarX = m_rectMid.right;

	for (auto& btn : m_buttons)
	{
		LayoutButton(btn, m_rectMid);

		if (btn.m_bRightJustify)
			m_minRightToolbarX = std::min(btn.m_rect.left, m_minRightToolbarX);
	}
}

HICON GuildHeader::GetIcon(int iconID, int iconSize)
{
	HICON hicon = m_hIcons[iconID];

	if (!hicon)
	{
		// Note, not shared but we do delete them in the destructor
		m_hIcons[iconID] = hicon =
			(HICON)LoadImage(g_hInstance, MAKEINTRESOURCE(iconID), IMAGE_ICON, iconSize, iconSize, LR_CREATEDIBSECTION);
	}

	return hicon;
}

void GuildHeader::DrawButton(HDC hdc, Button& button)
{
	int iconSize = ScaleByDPI(16);
	int sizeX = button.m_rect.right  - button.m_rect.left;
	int sizeY = button.m_rect.bottom - button.m_rect.top;
	int iconX = button.m_rect.left + sizeX / 2 - iconSize / 2;
	int iconY = button.m_rect.top  + sizeY / 2 - iconSize / 2;

	POINT    oldPt;
	COLORREF oldPen = ri::SetDCPenColor(hdc, 0);
	HGDIOBJ  oldObj = SelectObject(hdc, GetStockPen(DC_PEN));
	MoveToEx(hdc, 0, 0, &oldPt);

	const int clr = GUILD_HEADER_COLOR_2;
	COLORREF clrA = LerpColor(RGB(255, 255, 255), GetSysColor(clr), 1, 2);
	COLORREF clrB = LerpColor(RGB(0, 0, 0), GetSysColor(clr), 1, 2);

	RECT exp = { iconX - 4, iconY - 4, iconX + 4 + iconSize, iconY + 4 + iconSize };
	exp.left   = std::max(exp.left,   button.m_rect.left);
	exp.top    = std::max(exp.top,    button.m_rect.top);
	exp.right  = std::min(exp.right,  button.m_rect.right);
	exp.bottom = std::min(exp.bottom, button.m_rect.bottom);

	FillRect(hdc, &exp, GetSysColorBrush(clr));

	if (button.m_held) {
		iconX++, iconY++;
		std::swap(clrA, clrB);
	}

	if (button.m_hot || button.m_held) {
		ri::SetDCPenColor(hdc, clrA);
		MoveToEx(hdc, exp.left, exp.top, NULL);
		LineTo  (hdc, exp.right, exp.top);
		MoveToEx(hdc, exp.left, exp.top, NULL);
		LineTo  (hdc, exp.left, exp.bottom);
		ri::SetDCPenColor(hdc, clrB);
		MoveToEx(hdc, exp.right - 1, exp.bottom - 1, NULL);
		LineTo  (hdc, exp.right - 1, exp.top - 1);
		MoveToEx(hdc, exp.right - 1, exp.bottom - 1, NULL);
		LineTo  (hdc, exp.left - 1, exp.bottom - 1);
	}

	DrawIconEx(hdc, iconX, iconY, GetIcon(button.m_iconID, iconSize), iconSize, iconSize, 0, NULL, DI_NORMAL | DI_COMPAT);

	ri::SetDCPenColor(hdc, oldPen);
	MoveToEx(hdc, oldPt.x, oldPt.y, NULL);
}

void GuildHeader::HitTestButton(HDC hdc, Button& button, POINT& pt)
{
	bool hit = PtInRect(&button.m_rect, pt);

	if (button.m_hot != hit) {
		button.m_hot = hit;
		DrawButton(hdc, button);
	}
}

void GuildHeader::CheckClickButton(HDC hdc, Button& button, POINT& pt)
{
	bool hit = PtInRect(&button.m_rect, pt);

	if (button.m_held != hit || button.m_hot) {
		button.m_held = hit;
		button.m_hot = false;
		DrawButton(hdc, button);
	}
}

void GuildHeader::CheckReleaseButton(HDC hdc, HWND hWnd, Button& button, int buttonIndex, POINT& pt)
{
	if (button.m_held) {
		button.m_held = false;
		DrawButton(hdc, button);
		SendMessage(hWnd, WM_USERCOMMAND, (WPARAM) button.m_buttonID, (LPARAM) buttonIndex);
	}
}

LRESULT CALLBACK GuildHeader::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	GuildHeader* pThis = (GuildHeader*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
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
			assert(pThis);
			pThis->m_hwnd = NULL;
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR) NULL);
			break;
		}
		case WM_MOUSEMOVE:
		case WM_MOUSELEAVE:
		{
			assert(pThis);
			HDC hdc = GetDC(hWnd);
			if (uMsg == WM_MOUSEMOVE)
			{
				TRACKMOUSEEVENT tme;
				tme.cbSize = sizeof tme;
				tme.dwFlags = TME_LEAVE;
				tme.dwHoverTime = 1;
				tme.hwndTrack = hWnd;
				_TrackMouseEvent(&tme);

				POINT pt = { GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam) };
				for (auto& button : pThis->m_buttons) {
					if (pThis->m_bLClickHeld)
						pThis->CheckClickButton(hdc, button, pt);
					else
						pThis->HitTestButton(hdc, button, pt);
				}
			}
			else
			{
				POINT pt = { -1, -1 };
				HDC hdc = GetDC(hWnd);
				for (auto& button : pThis->m_buttons)
				{
					pThis->HitTestButton(hdc, button, pt);
					pThis->CheckClickButton(hdc, button, pt);
				}

				pThis->m_bLClickHeld = false;
			}

			ReleaseDC(hWnd, hdc);
			break;
		}
		case WM_LBUTTONDOWN:
		{
			assert(pThis);
			pThis->m_bLClickHeld = true;
			POINT pt = { GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam) };
			HDC hdc = GetDC(hWnd);
			for (auto& button : pThis->m_buttons)
				pThis->CheckClickButton(hdc, button, pt);
			ReleaseDC(hWnd, hdc);
			break;
		}
		case WM_LBUTTONUP:
		{
			assert(pThis);
			pThis->m_bLClickHeld = false;
			POINT pt = { GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam) };
			HDC hdc = GetDC(hWnd);
			for (size_t i = 0; i < pThis->m_buttons.size(); i++)
			{
				Button& button = pThis->m_buttons[i];
				pThis->CheckReleaseButton(hdc, hWnd, button, int(i), pt);
				pThis->HitTestButton(hdc, button, pt);
			}
			ReleaseDC(hWnd, hdc);
			
			RECT& inforect = pThis->m_rectMid;
			if (PtInRect(&inforect, pt))
			{
				std::string cinfo = pThis->GetChannelInfo();
				if (cinfo.size())
				{
					std::string cname = pThis->GetChannelName();
					// Show the info in a full textbox.
					LPCTSTR ctstr1 = ConvertCppStringToTString(cinfo);
					LPCTSTR ctstr2 = ConvertCppStringToTString("Discord Messenger - " + cname);
					MessageBox(g_Hwnd, ctstr1, ctstr2, MB_OK | MB_ICONINFORMATION);
					free((void*)ctstr1);
					free((void*)ctstr2);
				}
			}

			break;
		}
		case WM_SIZE:
		{
			assert(pThis);
			pThis->Layout();

			int diff = GET_X_LPARAM(lParam) - pThis->m_oldWidth;
			if (diff == 0)
				break;

			if (diff < 0)
				diff = 0;

			pThis->m_oldWidth = GET_X_LPARAM(lParam);

			RECT rcUpdate{};
			GetClientRect(hWnd, &rcUpdate);
			rcUpdate.left = pThis->m_minRightToolbarX - diff - ScaleByDPI(60);

			InvalidateRect(hWnd, &rcUpdate, FALSE);
			break;
		}
		case WM_PAINT:
		{
			assert(pThis);
			PAINTSTRUCT ps = {};
			RECT rect1 = {};
			GetClientRect(hWnd, &rect1);

			int rectHeight = rect1.bottom - rect1.top;

			HDC hdc =
			BeginPaint(hWnd, &ps);
			COLORREF old = SetBkColor(hdc, GetSysColor(GUILD_HEADER_COLOR));
			COLORREF oldText = SetTextColor(hdc, GetSysColor(COLOR_CAPTIONTEXT));

			//draw the background
			pThis->Layout();

#if (WINVER>=0x0500)
			FillGradient(hdc, &pThis->m_rectLeft,  GUILD_HEADER_COLOR, GUILD_HEADER_COLOR_2, false);
			FillGradient(hdc, &pThis->m_rectRight, GUILD_HEADER_COLOR_2, GUILD_HEADER_COLOR, false);
			FillRect(hdc, &pThis->m_rectMidFull, GetSysColorBrush(GUILD_HEADER_COLOR_2));
#else
			FillRect(hdc, &pThis->m_rectFull, GetSysColorBrush(GUILD_HEADER_COLOR));
#endif

			int old_mode = SetBkMode(hdc, TRANSPARENT);

			std::string gname = pThis->GetGuildName();
			std::string ginfo = pThis->GetSubtitleText();
			std::string cname = pThis->GetChannelName();
			std::string cinfo = pThis->GetChannelInfo();

			LPCTSTR ctstrName = ConvertCppStringToTString(gname);
			LPCTSTR ctstrSubt = ConvertCppStringToTString(ginfo);
			LPCTSTR ctstrChNm = ConvertCppStringToTString(cname);
			LPCTSTR ctstrChIn = ConvertCppStringToTString(cinfo);

			HGDIOBJ objOld = SelectObject(hdc, g_GuildCaptionFont);

			RECT nameRect = pThis->m_rectLeft;
			nameRect.left += ScaleByDPI(8);
			DrawText(hdc, ctstrName, -1, &nameRect, DT_SINGLELINE | DT_CALCRECT | DT_NOPREFIX | DT_WORD_ELLIPSIS);
			int nameHeight = nameRect.bottom - nameRect.top;

			nameRect = pThis->m_rectLeft;
			nameRect.right = nameRect.left + ScaleByDPI(CHANNEL_VIEW_LIST_WIDTH);
			nameRect.left += ScaleByDPI(8);
			
			// if we have a subtitle...
			if (ginfo.size())
				nameRect.top += (rectHeight - nameHeight * 2) / 2;
			else
				nameRect.top += (rectHeight - nameHeight) / 2;
			nameRect.bottom = nameRect.top + nameHeight;

			// draw the name text.
			DrawText(hdc, ctstrName, -1, &nameRect, DT_SINGLELINE | DT_NOPREFIX | DT_WORD_ELLIPSIS);
			// also draw the subtitle text
			SelectObject(hdc, g_GuildSubtitleFont);
			nameRect.top += nameHeight;
			nameRect.bottom += nameHeight;
			DrawText(hdc, ctstrSubt, -1, &nameRect, DT_SINGLELINE | DT_NOPREFIX | DT_WORD_ELLIPSIS);

			// Measure the channel name.
			SelectObject(hdc, g_GuildCaptionFont);

			nameRect = pThis->m_rectMid;
			DrawText(hdc, ctstrChNm, -1, &nameRect, DT_SINGLELINE | DT_NOPREFIX | DT_WORD_ELLIPSIS | DT_CALCRECT);
			nameHeight = nameRect.bottom - nameRect.top;
			int nameWidth = nameRect.right - nameRect.left;

			nameRect = pThis->m_rectMid;

			// Draw the channel icon.
			Channel* pChannel = GetDiscordInstance()->GetCurrentChannel();
			if (pChannel)
			{
				HICON icon = pThis->GetIconFromType(pChannel->m_channelType);
				if (icon)
				{
					DrawIconEx(
						hdc,
						nameRect.left,
						nameRect.top + (nameRect.bottom - nameRect.top) / 2 - ScaleByDPI(CHANNEL_ICON_SIZE) / 2,
						icon,
						ScaleByDPI(CHANNEL_ICON_SIZE),
						ScaleByDPI(CHANNEL_ICON_SIZE),
						0,
						NULL,
						DI_NORMAL
					);
					nameRect.left += ScaleByDPI(CHANNEL_ICON_SIZE) + 2;
				}
			}

			nameRect.top += (rectHeight - nameHeight) / 2;
			DrawText(hdc, ctstrChNm, -1, &nameRect, DT_SINGLELINE | DT_NOPREFIX | DT_WORD_ELLIPSIS);

			SelectObject(hdc, g_GuildInfoFont);

			nameRect = pThis->m_rectMid;
			nameRect.left  += ScaleByDPI(20) + nameWidth;
			nameRect.top   += (rectHeight - nameHeight) / 2;
			DrawText(hdc, ctstrChIn, -1, &nameRect, DT_SINGLELINE | DT_NOPREFIX | DT_WORD_ELLIPSIS);

			SelectObject(hdc, objOld);

			SetBkColor(hdc, old);
			SetTextColor(hdc, oldText);
			SetBkMode(hdc, old_mode);

			// Draw the pin button
			for (auto& btn : pThis->m_buttons)
				pThis->DrawButton(hdc, btn);

			EndPaint(hWnd, &ps);

			free((void*)ctstrName);
			free((void*)ctstrSubt);
			free((void*)ctstrChNm);
			free((void*)ctstrChIn);
			break;
		}
		case WM_USERCOMMAND:
		{
			assert(pThis);
			switch (wParam)
			{
				case IDTB_PINS: {
					Guild* pGuild = GetDiscordInstance()->GetCurrentGuild();
					Channel* pChan = GetDiscordInstance()->GetCurrentChannel();
					if (!pGuild || !pChan) break;

					POINT pt = {
						pThis->m_buttons[lParam].m_rect.right,
						pThis->m_buttons[lParam].m_rect.bottom
					};
					ClientToScreen(hWnd, &pt);
					PmvCreate(pChan->m_snowflake, pGuild->m_snowflake, pt.x, pt.y, true);
					break;
				}
				case IDTB_MEMBERS:
					SendMessage(g_Hwnd, WM_TOGGLEMEMBERS, 0, 0);
					break;
			}
			break;
		}
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void GuildHeader::InitializeClass()
{
	WNDCLASS& wc = g_GuildHeaderClass;

	wc.lpszClassName = T_GUILD_HEADER_CLASS;
	wc.style         = 0;
	wc.hCursor       = LoadCursor(0, IDC_ARROW);
	wc.lpfnWndProc   = GuildHeader::WndProc;

	RegisterClass(&wc);
}

GuildHeader* GuildHeader::Create(HWND hwnd, LPRECT pRect)
{
	GuildHeader* newThis = new GuildHeader;

	int width = pRect->right - pRect->left, height = pRect->bottom - pRect->top;

	newThis->m_hwnd = CreateWindowEx(
		0,
		T_GUILD_HEADER_CLASS,
		NULL,
		WS_CHILD | WS_VISIBLE,
		pRect->left, pRect->top,
		width, height,
		hwnd,
		(HMENU)CID_GUILDHEADER,
		g_hInstance,
		newThis
	);

	return newThis;
}

HICON GuildHeader::GetIconFromType(Channel::eChannelType t)
{
	int icon = DMIC(IDI_CHANNEL);
	switch (t) {
		case Channel::DM:      icon = DMIC(IDI_DM);      break;
		case Channel::GROUPDM: icon = DMIC(IDI_GROUPDM); break;
		case Channel::FORUM:   icon = DMIC(IDI_GROUPDM); break;
		case Channel::VOICE:   icon = DMIC(IDI_VOICE);   break;
	}

	return GetIcon(icon, GetSystemMetrics(SM_CXSMICON));
}

