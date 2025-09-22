#include "GuildHeader.hpp"
#include "PinList.hpp"
#include "NotificationViewer.hpp"

#define GUILD_HEADER_COLOR   COLOR_ACTIVECAPTION
#define GUILD_HEADER_COLOR_2 GetGradientActiveCaptionColor()
#define CHANNEL_ICON_SIZE 16

#define IDTB_MEMBERS  (1000) // Show list of members
#define IDTB_PINS     (1001) // Show pinned messages
#define IDTB_CHANNELS (1002) // Hide channel list
#define IDTB_NOTIFS   (1003) // Show notifications
#define IDTB_FLOAT    (1004) // Float channel out

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

	m_buttons.push_back(Button(IDTB_MEMBERS,  DMIC(IDI_MEMBERS),      BUTTON_RIGHT));
	m_buttons.push_back(Button(IDTB_PINS,     DMIC(IDI_PIN),          BUTTON_RIGHT));
	m_buttons.push_back(Button(IDTB_NOTIFS,   DMIC(IDI_NOTIFICATION), BUTTON_RIGHT));
	m_buttons.push_back(Button(IDTB_CHANNELS, DMIC(IDI_SHIFT_LEFT),   BUTTON_GUILD_RIGHT));
}

std::string GuildHeader::GetGuildName()
{
	Guild* pGuild = GetCurrentGuild();
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
	Channel *pChannel = GetCurrentChannel();
	if (!pChannel)
		return "";
	return pChannel->m_name;
}

std::string GuildHeader::GetChannelInfo()
{
	Channel* pChannel = GetCurrentChannel();
	if (!pChannel)
		return "";
	return pChannel->m_topic;
}

Guild* GuildHeader::GetCurrentGuild()
{
	return GetDiscordInstance()->GetGuild(m_guildID);
}

Channel* GuildHeader::GetCurrentChannel()
{
	auto guild = GetCurrentGuild();
	if (!guild)
		return nullptr;

	return guild->GetChannel(m_channelID);
}

void GuildHeader::Update()
{
	InvalidateRect(m_hwnd, NULL, false);
}

void GuildHeader::LayoutButton(Button& button, RECT& chanNameRect, RECT& guildNameRect)
{
	int tbrh = 0;
	if (IsPlacementGuildType(button.m_placement)) {
		button.m_rect = guildNameRect;
		tbrh = guildNameRect.bottom - chanNameRect.top;
	}
	else {
		button.m_rect = chanNameRect;
		tbrh = chanNameRect.bottom - chanNameRect.top;
	}

	switch (button.m_placement)
	{
		case BUTTON_RIGHT:
		{
			button.m_rect.left = button.m_rect.right - tbrh;
			chanNameRect.right = button.m_rect.left;
			break;
		}
		case BUTTON_LEFT:
		{
			button.m_rect.right = button.m_rect.left + tbrh;
			chanNameRect.left = button.m_rect.right;
			break;
		}
		case BUTTON_GUILD_RIGHT:
		{
			button.m_rect.left = button.m_rect.right - tbrh;
			guildNameRect.right = button.m_rect.left;
			break;
		}
		case BUTTON_GUILD_LEFT:
		{
			button.m_rect.right = button.m_rect.left + tbrh;
			guildNameRect.left = button.m_rect.right;
			break;
		}
	}
}

void GuildHeader::Layout()
{
	RECT rect = {};
	GetClientRect(m_hwnd, &rect);

	m_fullRect = rect;

	RECT &rrect1 = m_rectLeftFull, &rrect2 = m_rectMidFull, &rrect3 = m_rectRightFull;
		
	rrect1 = rect, rrect2 = rect, rrect3 = rect;
	rrect1.right = rrect1.left + m_pParent->IsChannelListVisible() ? ScaleByDPI(CHANNEL_VIEW_LIST_WIDTH) : (rect.bottom - rect.top);
	rrect2.left  = rrect1.right;
	rrect3.left  = rrect3.right - ScaleByDPI(MEMBER_LIST_WIDTH);
	rrect2.right = rrect3.left;

	m_rectLeft = m_rectLeftFull;
	m_rectRight = m_rectRightFull;
	m_rectMid = m_rectMidFull;
	m_rectMid.left  += ScaleByDPI(10);
	m_rectMid.right -= ScaleByDPI(10);
	m_minRightToolbarX = m_rectMid.right;

	for (auto& btn : m_buttons)
	{
		LayoutButton(btn, m_rectMid, m_rectLeft);

		if (btn.m_placement == BUTTON_RIGHT)
			m_minRightToolbarX = std::min(btn.m_rect.left, m_minRightToolbarX);
	}
}

HICON GuildHeader::GetIcon(int iconID, int iconSize)
{
	HICON hicon = m_hIcons[iconID];

	if (!hicon)
	{
		// Note, not shared but we do delete them in the destructor
		hicon = (HICON)ri::LoadImage(g_hInstance, MAKEINTRESOURCE(iconID), IMAGE_ICON, iconSize, iconSize, LR_CREATEDIBSECTION);

		if (!hicon)
			hicon = LoadIcon(g_hInstance, MAKEINTRESOURCE(iconID));

		m_hIcons[iconID] = hicon;
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
	HGDIOBJ  oldObj = SelectObject(hdc, ri::GetDCPen());
	MoveToEx(hdc, 0, 0, &oldPt);

	const int clr = GUILD_HEADER_COLOR_2;
	COLORREF clrA = LerpColor(RGB(255, 255, 255), GetSysColor(clr), 1, 2);
	COLORREF clrB = LerpColor(RGB(0, 0, 0), GetSysColor(clr), 1, 2);

	RECT exp = { iconX - 4, iconY - 4, iconX + 4 + iconSize, iconY + 4 + iconSize };
	exp.left   = std::max(exp.left,   button.m_rect.left);
	exp.top    = std::max(exp.top,    button.m_rect.top);
	exp.right  = std::min(exp.right,  button.m_rect.right);
	exp.bottom = std::min(exp.bottom, button.m_rect.bottom);

	if (IsPlacementGuildType(button.m_placement)) {
		HRGN rgn = CreateRectRgnIndirect(&exp);
		SelectClipRgn(hdc, rgn);
		FillGradient(hdc, &m_rectLeftFull, GUILD_HEADER_COLOR, GUILD_HEADER_COLOR_2, false);
		SelectClipRgn(hdc, NULL);
		DeleteRgn(rgn);
	}
	else {
		FillRect(hdc, &exp, ri::GetSysColorBrush(clr));
	}

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

	HICON hicon = GetIcon(button.m_iconID, iconSize);
	DrawIconInvert(
		hdc,
		hicon,
		iconX,
		iconY,
		iconSize,
		iconSize,
		IsIconMostlyBlack(hicon) && IsTextColorLight()
	);

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

void GuildHeader::InvalidateEmote(void* context, const Rect& rc)
{
	HRGN invRgn = CreateRectRgn(W32RECT(rc));
	UnionRgn((HRGN)context, (HRGN)context, invRgn);
	DeleteRgn(invRgn);
}

void GuildHeader::OnUpdateEmoji(Snowflake sf)
{
	HRGN rgn = CreateRectRgn(0, 0, 0, 0);

	// TODO: Only invalidate the emoji that were updated
	if (m_channelDescription.IsFormatted())
		m_channelDescription.RunForEachCustomEmote(&InvalidateEmote, (void*)rgn);

	InvalidateRgn(m_hwnd, rgn, FALSE);
	DeleteRgn(rgn);
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
				ri::TrackMouseEvent(&tme);

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
					MessageBox(GetParent(pThis->m_hwnd), ctstr1, ctstr2, MB_OK | MB_ICONINFORMATION);
					free((void*)ctstr1);
					free((void*)ctstr2);
				}
			}

			HDC hdc = GetDC(hWnd);
			for (size_t i = 0; i < pThis->m_buttons.size(); i++)
			{
				Button& button = pThis->m_buttons[i];
				pThis->CheckReleaseButton(hdc, hWnd, button, int(i), pt);
				pThis->HitTestButton(hdc, button, pt);
			}
			ReleaseDC(hWnd, hdc);

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

			pThis->Layout();

			//draw the background
#if (WINVER>=0x0500)
			FillGradient(hdc, &pThis->m_rectLeftFull,  GUILD_HEADER_COLOR, GUILD_HEADER_COLOR_2, false);
			FillGradient(hdc, &pThis->m_rectRightFull, GUILD_HEADER_COLOR_2, GUILD_HEADER_COLOR, false);
			FillRect(hdc, &pThis->m_rectMidFull, ri::GetSysColorBrush(GUILD_HEADER_COLOR_2));
#else
			FillRect(hdc, &pThis->m_rectFull, ri::GetSysColorBrush(GUILD_HEADER_COLOR));
#endif

			int old_mode = SetBkMode(hdc, TRANSPARENT);

			std::string gname = pThis->GetGuildName();
			std::string ginfo = pThis->GetSubtitleText();
			std::string cname = pThis->GetChannelName();
			std::string cinfo = pThis->GetChannelInfo();

			if (cinfo != pThis->m_currentChannelDescription)
			{
				pThis->m_currentChannelDescription = cinfo;
				pThis->m_channelDescription.Clear();
				pThis->m_channelDescription.SetDefaultStyle(WORD_ITALIC);
				pThis->m_channelDescription.SetAllowBiggerText(false);
				pThis->m_channelDescription.SetMessage(cinfo);

				// N.B. Interactables currently unused for now.
				std::vector<InteractableItem> interactables;
				GetDiscordInstance()->ResolveLinks(&pThis->m_channelDescription, interactables, pThis->GetCurrentGuildID());
			}

			LPCTSTR ctstrName = ConvertCppStringToTString(gname);
			LPCTSTR ctstrSubt = ConvertCppStringToTString(ginfo);
			LPCTSTR ctstrChNm = ConvertCppStringToTString(cname);

			HGDIOBJ objOld = SelectObject(hdc, g_GuildCaptionFont);
			RECT nameRect;
			int nameHeight;

			bool channelListVisible = pThis->m_pParent->IsChannelListVisible();
			if (channelListVisible)
			{
				nameRect = pThis->m_rectLeft;
				nameRect.left += ScaleByDPI(8);
				DrawText(hdc, ctstrName, (int) _tcslen(ctstrName), &nameRect, DT_SINGLELINE | DT_CALCRECT | DT_NOPREFIX | ri::GetWordEllipsisFlag());
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
				DrawText(hdc, ctstrName, (int) _tcslen(ctstrName), &nameRect, DT_SINGLELINE | DT_NOPREFIX | ri::GetWordEllipsisFlag());
				// also draw the subtitle text
				SelectObject(hdc, g_GuildSubtitleFont);
				nameRect.top += nameHeight;
				nameRect.bottom += nameHeight;
				DrawText(hdc, ctstrSubt, -1, &nameRect, DT_SINGLELINE | DT_NOPREFIX | ri::GetWordEllipsisFlag());

				SelectObject(hdc, g_GuildCaptionFont);
			}

			// Measure the channel name.
			nameRect = pThis->m_rectMid;
			DrawText(hdc, ctstrChNm, -1, &nameRect, DT_SINGLELINE | DT_NOPREFIX | ri::GetWordEllipsisFlag() | DT_CALCRECT);
			nameHeight = nameRect.bottom - nameRect.top;
			int nameWidth = nameRect.right - nameRect.left;

			nameRect = pThis->m_rectMid;

			// Draw the channel icon.
			Channel* pChannel = pThis->GetCurrentChannel();
			if (pChannel)
			{
				HICON icon = pThis->GetIconFromType(pChannel->m_channelType);
				if (icon)
				{
					DrawIconInvert(
						hdc,
						icon,
						nameRect.left,
						nameRect.top + (nameRect.bottom - nameRect.top) / 2 - ScaleByDPI(CHANNEL_ICON_SIZE) / 2,
						ScaleByDPI(CHANNEL_ICON_SIZE),
						ScaleByDPI(CHANNEL_ICON_SIZE),
						IsIconMostlyBlack(icon) && IsTextColorLight()
					);

					nameRect.left += ScaleByDPI(CHANNEL_ICON_SIZE) + 2;
				}
			}

			nameRect.top += (rectHeight - nameHeight) / 2;
			DrawText(hdc, ctstrChNm, -1, &nameRect, DT_SINGLELINE | DT_NOPREFIX | ri::GetWordEllipsisFlag());

			SelectObject(hdc, g_GuildInfoFont);

			if (!cinfo.empty())
			{
				nameRect = pThis->m_rectMid;
				nameRect.left += ScaleByDPI(30) + nameWidth;
				nameRect.top += (rectHeight - nameHeight) / 2;
				nameRect.bottom = nameRect.top + nameHeight;

				DrawingContext dc(hdc);
				pThis->m_channelDescription.Layout(&dc, Rect(W32RECT(nameRect)));
				pThis->m_channelDescription.DrawConfined(&dc, Rect(W32RECT(nameRect)));
			}

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
			break;
		}
		case WM_USERCOMMAND:
		{
			assert(pThis);
			switch (wParam)
			{
				case IDTB_PINS: {
					Guild* pGuild = pThis->GetCurrentGuild();
					Channel* pChan = pThis->GetCurrentChannel();
					if (!pGuild || !pChan) break;

					POINT pt = {
						pThis->m_buttons[lParam].m_rect.right,
						pThis->m_buttons[lParam].m_rect.bottom
					};
					ClientToScreen(hWnd, &pt);
					PinList::Show(pChan->m_snowflake, pGuild->m_snowflake, pt.x, pt.y, true);
					break;
				}
				case IDTB_NOTIFS: {
					POINT pt = {
						pThis->m_buttons[lParam].m_rect.right,
						pThis->m_buttons[lParam].m_rect.bottom
					};
					ClientToScreen(hWnd, &pt);
					NotificationViewer::Show(pt.x, pt.y, true);
					break;
				}
				case IDTB_MEMBERS: {
					SendMessage(GetParent(pThis->m_hwnd), WM_TOGGLEMEMBERS, 0, 0);
					break;
				}
				case IDTB_CHANNELS: {
					SendMessage(GetParent(pThis->m_hwnd), WM_TOGGLECHANNELS, 0, 0);
					bool channelListVisible = pThis->m_pParent->IsChannelListVisible();

					pThis->m_buttons[lParam].m_iconID = DMIC(channelListVisible ? IDI_SHIFT_LEFT : IDI_SHIFT_RIGHT);
					pThis->m_buttons[lParam].m_placement = channelListVisible ? BUTTON_GUILD_RIGHT : BUTTON_GUILD_LEFT;
					pThis->Layout();
					InvalidateRect(hWnd, &pThis->m_rectLeftFull, FALSE);
					InvalidateRect(hWnd, &pThis->m_rectMidFull, FALSE);
					break;
				}
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
	wc.hInstance     = g_hInstance;

	RegisterClass(&wc);
}

GuildHeader* GuildHeader::Create(ChatWindow* parent, LPRECT pRect)
{
	GuildHeader* newThis = new GuildHeader;
	newThis->m_pParent = parent;

	int width = pRect->right - pRect->left, height = pRect->bottom - pRect->top;

	newThis->m_hwnd = CreateWindowEx(
		0,
		T_GUILD_HEADER_CLASS,
		NULL,
		WS_CHILD | WS_VISIBLE,
		pRect->left, pRect->top,
		width, height,
		parent->GetHWND(),
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

