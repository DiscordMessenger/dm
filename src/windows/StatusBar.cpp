#include "StatusBar.hpp"
#include "Main.hpp"

enum {
	IDP_NOTIFS,
	IDP_TYPING,
	IDP_CHRCNT,
	IDP_CNTRLS
};

StatusBar* StatusBar::Create(HWND hParent)
{
	StatusBar* pBar = new StatusBar;
	pBar->m_hwnd = CreateWindowEx(
		0,
		STATUSCLASSNAME,
		NULL,
		WS_CHILD | WS_VISIBLE,
		0, 0, 0, 0,
		hParent,
		(HMENU)CID_STATUSBAR,
		g_hInstance,
		NULL
	);

	if (!pBar->m_hwnd)
	{
		// try it with CreateStatusWindow
		pBar->m_hwnd = ri::CreateStatusWindowANSI(
			WS_CHILD | WS_VISIBLE,
			"",
			hParent,
			CID_STATUSBAR
		);
	}

	if (!pBar->m_hwnd) {
		delete pBar;
		pBar = nullptr;
	}

	SetWindowFont(pBar->m_hwnd, g_MessageTextFont, TRUE);
	SetWindowLongPtr(pBar->m_hwnd, GWLP_USERDATA, (LONG_PTR) pBar);
	return pBar;
}

void StatusBar::TimerProc(HWND hWnd, UINT uMsg, UINT_PTR uTimerId, DWORD dwTime)
{
	StatusBar* pBar = (StatusBar*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
	assert(pBar);
	assert(uMsg == WM_TIMER);

	switch (uTimerId)
	{
		case IDT_EXPIRY:
			pBar->OnExpiryTick();
			break;
		case IDT_ANIMATION:
			pBar->OnAnimationTick();
			break;
	}
}

void StatusBar::AddTypingName(Snowflake sf, time_t startTime, const std::string& name)
{
	uint64_t currentTime = time(NULL) * 1000ULL;
	uint64_t startTimeMS = (uint64_t)startTime * 1000ULL;
	if (startTimeMS + TYPING_INTERVAL <= currentTime)
		return;

	RemoveTypingNameInternal(sf);
	m_typingUsers.push_back({ sf, name, startTimeMS });
	SetExpiryTimerIn(GetNextExpiryTime());
	StartAnimation();
	InvalidateRect(m_hwnd, &m_typing_status_rect, TRUE);
}

void StatusBar::RemoveTypingName(Snowflake sf)
{
	RemoveTypingNameInternal(sf);
	if (!m_typingUsers.empty())
	{
		StartAnimation();
		SetExpiryTimerIn(GetNextExpiryTime());
	}
	else
	{
		StopAnimation();
	}

	InvalidateRect(m_hwnd, &m_typing_status_rect, TRUE);
}

void StatusBar::ClearTypers()
{
	m_typingUsers.clear();

	StopAnimation();
	if (m_timerExpireEvent)
		KillTimer(m_hwnd, m_timerExpireEvent);
	m_timerExpireEvent = 0;

	InvalidateRect(m_hwnd, &m_typing_status_rect, TRUE);
	InvalidateRect(m_hwnd, &m_typing_animation_rect, TRUE);
}

void StatusBar::RemoveTypingNameInternal(Snowflake sf)
{
	for (auto iter = m_typingUsers.begin(); iter != m_typingUsers.end(); ++iter) {
		if (iter->m_key == sf) {
			m_typingUsers.erase(iter);
			break;
		}
	}
}

void StatusBar::SetExpiryTimerIn(int ms)
{
	if (m_timerExpireEvent)
		KillTimer(m_hwnd, m_timerExpireEvent);
	m_timerExpireEvent = SetTimer(m_hwnd, IDT_EXPIRY, ms, TimerProc);
}

void StatusBar::StartAnimation()
{
	StopAnimation();
	m_timerAnimEvent = SetTimer(m_hwnd, IDT_ANIMATION, ANIMATION_MS, TimerProc);
	InvalidateRect(m_hwnd, &m_typing_animation_rect, TRUE);
}

void StatusBar::StopAnimation()
{
	if (m_timerAnimEvent)
		KillTimer(m_hwnd, m_timerAnimEvent);
	InvalidateRect(m_hwnd, &m_typing_animation_rect, TRUE);
}

int StatusBar::GetNextExpiryTime()
{
	time_t t = time(NULL) * 1000LL;
	uint64_t minExpiry = UINT64_MAX;

	for (auto& tu : m_typingUsers)
	{
		uint64_t expiry = tu.m_startTimeMS + TYPING_INTERVAL - t;
		if (minExpiry > expiry)
			minExpiry = expiry;
	}

	return int(minExpiry);
}

void StatusBar::OnExpiryTick()
{
	uint64_t timeMS = time(NULL) * 1000LL;
	bool bNeedUpdate = false;

	for (auto iter = m_typingUsers.begin(); iter != m_typingUsers.end(); )
	{
		if (iter->m_startTimeMS - 100 < timeMS) {
			size_t dist = std::distance(m_typingUsers.begin(), iter);
			m_typingUsers.erase(iter);
			iter = m_typingUsers.begin() + dist;
			bNeedUpdate = true;
		}
		else ++iter;
	}

	if (bNeedUpdate)
		InvalidateRect(m_hwnd, &m_typing_status_rect, TRUE);

	KillTimer(m_hwnd, m_timerExpireEvent);
	m_timerExpireEvent = 0;
	if (!m_typingUsers.empty()) {
		StartAnimation();
		SetExpiryTimerIn(GetNextExpiryTime());
	}
	else {
		StopAnimation();
	}
}

void StatusBar::OnAnimationTick()
{
	m_anim_frame_number = (m_anim_frame_number + 1) % ANIMATION_FRAMES;
	InvalidateRect(m_hwnd, &m_typing_animation_rect, TRUE);
}

void StatusBar::UpdateCharacterCounter(int nChars, int nCharsMax)
{
	if (!nChars) {
		SendMessage(m_hwnd, SB_SETTEXT, IDP_CHRCNT, (LPARAM) TEXT(""));
		return;
	}

	std::string text = std::to_string(nChars) + "/" + std::to_string(nCharsMax);
	LPTSTR tstr = ConvertCppStringToTString(text);
	SendMessage(m_hwnd, SB_SETTEXT, IDP_CHRCNT, (LPARAM) tstr);
	free(tstr);
}

void StatusBar::DrawItem(LPDRAWITEMSTRUCT lpDIS)
{
	// this is the only owner drawn item
	assert(lpDIS->itemID == IDP_TYPING);
	HDC hdc = lpDIS->hDC;
	RECT rc = lpDIS->rcItem;

	RECT rc2 = rc;
	int smcxiconsm = GetSystemMetrics(SM_CXSMICON);
	rc2.right = rc2.left + smcxiconsm + ScaleByDPI(4);
	rc.left = rc2.right;

	HICON icon = LoadIcon(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_TYPING_FRAME1 + m_anim_frame_number)));
	m_typing_animation_rect = rc2;

	size_t sz = m_typingUsers.size();

	if (sz)
		ri::DrawIconEx(hdc, rc2.left, rc2.top + (rc2.bottom - rc2.top - smcxiconsm) / 2, icon, smcxiconsm, smcxiconsm, 0, NULL, DI_NORMAL | DI_COMPAT);

	int mode = SetBkMode(hdc, TRANSPARENT);
	HGDIOBJ gdiObjOld = SelectObject(hdc, g_TypingBoldFont);
	COLORREF textOld = SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));

	m_typing_status_rect = rc;

	for (size_t i = 0; i < sz; i++)
	{
		TypingUser& tu = m_typingUsers[i];
		if (i != 0) {
			LPCTSTR separator;
			if (i == sz - 1)
				separator = TmGetTString(IDS_AND);
			else
				separator = TEXT(", ");

			HGDIOBJ gdiObj = SelectObject(hdc, g_TypingRegFont);
			RECT rcMeasure{};
			rcMeasure.right = rc.right - rc.left;
			DrawText(hdc, separator, -1, &rcMeasure, ri::GetWordEllipsisFlag() | DT_NOPREFIX | DT_VCENTER | DT_SINGLELINE | DT_CALCRECT);
			DrawText(hdc, separator, -1, &rc,        ri::GetWordEllipsisFlag() | DT_NOPREFIX | DT_VCENTER | DT_SINGLELINE);
			SelectObject(hdc, gdiObj);
			rc.left += rcMeasure.right - rcMeasure.left;

			// If the width is bigger or equal to the alloted space, quit right now
			if (rc.left >= rc.right)
				break;
		}

		LPTSTR typerText = ConvertCppStringToTString(tu.m_name);
		RECT rcMeasure{};
		rcMeasure.right = rc.right - rc.left;
		DrawText(hdc, typerText, -1, &rcMeasure, ri::GetWordEllipsisFlag() | DT_NOPREFIX | DT_VCENTER | DT_SINGLELINE | DT_CALCRECT);
		DrawText(hdc, typerText, -1, &rc,        ri::GetWordEllipsisFlag() | DT_NOPREFIX | DT_VCENTER | DT_SINGLELINE);
		rc.left += rcMeasure.right - rcMeasure.left;

		if (rc.left >= rc.right)
			break;
	}

	if (sz != 0)
	{
		SelectObject(hdc, g_TypingRegFont);
		LPCTSTR typingEnd = TmGetTString(sz == 1 ? IDS_IS_TYPING : IDS_ARE_TYPING);
		DrawText(hdc, typingEnd, -1, &rc, ri::GetWordEllipsisFlag() | DT_NOPREFIX | DT_VCENTER | DT_SINGLELINE);
	}

	SetTextColor(hdc, textOld);
	SelectObject(hdc, gdiObjOld);
	SetBkMode(lpDIS->hDC, mode);
}

extern int g_GuildListerWidth; // all in main.cpp
extern int g_ChannelViewListWidth;
extern int g_MemberListWidth;
extern int g_SendButtonWidth;

StatusBar::~StatusBar()
{
	if (m_hwnd) {
		BOOL b = DestroyWindow(m_hwnd);
		assert(b && "StatusBar was already destroyed");
		m_hwnd = NULL;
	}
}

void StatusBar::UpdateParts(int width)
{
	INT Widths[4];
	// Part 0 - Under the guild list and channel view
	// Part 1 - Under the message editor widget, list of typing users
	// Part 2 - Under the send button, character count
	// Part 3 - Under the member list, some controls, I don't know
	
	int scaled10 = ScaleByDPI(10);
	Widths[IDP_CNTRLS] = width;
	Widths[IDP_CHRCNT] = width - g_MemberListWidth - scaled10 * 3 / 2;
	Widths[IDP_TYPING] = Widths[IDP_CHRCNT] - g_SendButtonWidth - scaled10;
	Widths[IDP_NOTIFS] = g_GuildListerWidth + g_ChannelViewListWidth + scaled10 * 3;

	SendMessage(m_hwnd, SB_SETPARTS, _countof(Widths), (LPARAM) Widths);

	SendMessage(m_hwnd, SB_SETTEXT, 1 | SBT_OWNERDRAW, 0);
}
