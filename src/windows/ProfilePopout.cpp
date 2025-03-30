#include "ProfilePopout.hpp"
#include "ProfileView.hpp"
#include "RoleList.hpp"

#define ADD_NOTES
//#define ADD_MESSAGE

// Unlike DrawText, this sets the rect to an area of zero if the text is empty
static int DrawText2(HDC hdc, LPCTSTR lpchText, int cchText, RECT* lprc, UINT format)
{
	int result = 0;
	if (cchText != 0 && *lpchText) {
		result = DrawText(hdc, lpchText, cchText, lprc, format);
	}
	else if (format & DT_CALCRECT) {
		lprc->right = lprc->left;
		lprc->bottom = lprc->top;
	}

	return result;
}

Snowflake ProfilePopout::m_user;
Snowflake ProfilePopout::m_guild;
RoleList* ProfilePopout::m_pRoleList;
ProfileView* ProfilePopout::m_pProfileView;
HWND ProfilePopout::m_hwnd;
SIZE ProfilePopout::m_size;
HBITMAP ProfilePopout::m_hBitmap; // This bitmap will be disposed of when the popout is dismissed.

Snowflake ProfilePopout::GetUser()
{
	return m_user;
}

bool ProfilePopout::Layout(HWND hWnd, SIZE& fullSize)
{
	Profile* pProf = GetProfileCache()->LookupProfile(m_user, "...", "...", "");
	if (!pProf) {
		EndDialog(hWnd, 0);
		return TRUE;
	}

	if (!pProf->m_bExtraDataFetched) {
		GetProfileCache()->RequestExtraData(m_user, m_guild);
		pProf->m_bExtraDataFetched = true;
	}

	bool bShouldNoteBeEditable = false;
	if (!pProf->m_bNoteFetched) {
		GetProfileCache()->RequestNote(m_user);
		pProf->m_bNoteFetched = true;
	}
	else {
		bShouldNoteBeEditable = true;
	}

	Guild* gld = GetDiscordInstance()->GetGuild(m_guild);
	GuildMember* gm = nullptr;
	if (pProf->HasGuildMemberProfile(m_guild))
		gm = &pProf->m_guildMembers[m_guild];

	// Gather data about the user profile.
	LPTSTR name        = ConvertCppStringToTString(pProf->GetName(m_guild));
	LPTSTR status      = ConvertCppStringToTString(pProf->GetStatus(m_guild));
	LPTSTR userName    = ConvertCppStringToTString(pProf->m_name);
	LPTSTR pronouns    = ConvertCppStringToTString(pProf->m_pronouns);
	LPTSTR bio         = ConvertToTStringAddCR(pProf->m_bio);
	LPTSTR note        = ConvertToTStringAddCR(pProf->m_note);
	LPTSTR dscJoinedAt = ConvertCppStringToTString(pProf->m_snowflake ? FormatDate(ExtractTimestamp(pProf->m_snowflake) / 1000) : "");
	LPTSTR gldJoinedAt = ConvertCppStringToTString((gm && gm->m_joinedAt) ? FormatDate(gm->m_joinedAt) : "");

	// Get a HDC to measure the user information.
	HDC hdc = GetDC(hWnd);

	const int windowBorder     = ScaleByDPI(10);
	const int maxWidth         = ScaleByDPI(300);
	const int groupBoxBorder   = ScaleByDPI(10);
	const int pronounStatusGap = ScaleByDPI(10);
	 int joinedAtIconSize = ScaleByDPI(16);
	const int separatorSize    = ScaleByDPI(6);
	const int interItemGap     = ScaleByDPI(4);
	const int profileImageSize = GetProfilePictureSize() + ScaleByDPI(12);
	const int singleLineFlags  = DT_CALCRECT | DT_SINGLELINE | ri::GetWordEllipsisFlag() | DT_WORDBREAK;
	const int editControlFlags = DT_CALCRECT | DT_EDITCONTROL | DT_WORDBREAK;
	const RECT rcMeasureTemplate = { 0, 0, maxWidth, 0 }; // Used with DT_CALCRECT and wordbreak templates

	RECT rcProfileView{}, rcName, rcUserName, rcPronouns, rcStatus, rcAboutMe, rcMemberSinceFull{}, rcMemberSinceGuild, rcMemberSinceDiscord, rcRoles, rcNote, rcMessage, rcGroupBox;

	HGDIOBJ oldFont = SelectObject(hdc, g_MessageTextFont);
	rcGroupBox = rcMeasureTemplate;
	DrawText(hdc, TEXT("Group Box"), -1, &rcGroupBox, singleLineFlags);
	rcGroupBox.bottom += ScaleByDPI(3) + groupBoxBorder;
	rcGroupBox.right = groupBoxBorder * 2;

	SelectObject(hdc, g_AuthorTextFont);
	rcName = rcMeasureTemplate;
	DrawText(hdc, name, -1, &rcName, singleLineFlags);

	SelectObject(hdc, g_MessageTextFont);
	rcUserName = rcMeasureTemplate;
	DrawText(hdc, userName, -1, &rcUserName, singleLineFlags);

	rcPronouns = rcMeasureTemplate;
	rcPronouns.right = ScaleByDPI(70); // Max width for pronouns, if exists
	DrawText2(hdc, pronouns, -1, &rcPronouns, singleLineFlags);

	rcStatus = rcMeasureTemplate;
	rcStatus.right -= groupBoxBorder * 2;
	rcStatus.right -= rcPronouns.right + pronounStatusGap;
	HGDIOBJ o2 = SelectObject(hdc, g_DateTextFont);
	DrawText2(hdc, status, -1, &rcStatus, singleLineFlags);
	SelectObject(hdc, o2);
	
	rcAboutMe = rcMeasureTemplate;
	rcAboutMe.right -= groupBoxBorder * 2 + ScaleByDPI(20);
	DrawText2(hdc, bio, -1, &rcAboutMe, editControlFlags);

	rcMemberSinceGuild = rcMeasureTemplate;
	rcMemberSinceGuild.right -= groupBoxBorder * 2 + joinedAtIconSize;
	rcMemberSinceGuild.right /= 2;
	DrawText2(hdc, gldJoinedAt, -1, &rcMemberSinceGuild, singleLineFlags);

	rcMemberSinceDiscord = rcMeasureTemplate;
	rcMemberSinceDiscord.right -= groupBoxBorder * 2 + joinedAtIconSize;
	rcMemberSinceDiscord.right /= 2;
	DrawText2(hdc, dscJoinedAt, -1, &rcMemberSinceDiscord, singleLineFlags);

	rcMemberSinceFull.right  = rcMemberSinceGuild.right + rcMemberSinceDiscord.right + joinedAtIconSize * 2 + groupBoxBorder * 2 + rcGroupBox.right;
	rcMemberSinceFull.bottom = std::max({ rcMemberSinceDiscord.bottom, rcMemberSinceGuild.bottom, (LONG) joinedAtIconSize }) + rcGroupBox.bottom;
	
	SetRectEmpty(&rcRoles);

	int temp;
#ifdef ADD_NOTES
	rcNote = rcMeasureTemplate;
	rcNote.right -= groupBoxBorder * 2;
	DrawText(hdc, TEXT("Wp"), -1, &rcNote, editControlFlags);
	temp = rcNote.right - rcNote.left;
	rcNote = rcMeasureTemplate;
	rcNote.right -= groupBoxBorder * 2;
	DrawText(hdc, note, -1, &rcNote, editControlFlags);
	rcNote.bottom = std::max(rcNote.bottom, LONG(temp));
#endif

	rcMessage = rcMeasureTemplate;
	rcMessage.right -= groupBoxBorder * 2;
	DrawText(hdc, TEXT("Message this user"), -1, &rcMessage, editControlFlags | singleLineFlags);
	rcMessage.bottom += ScaleByDPI(8);

	rcProfileView.right   = profileImageSize + ScaleByDPI(6) + std::max(rcName.right, rcUserName.right);
	rcProfileView.bottom  = std::max((LONG) profileImageSize, rcName.bottom + rcUserName.bottom);
	rcProfileView.right  -= ScaleByDPI(4);
	rcProfileView.bottom -= ScaleByDPI(6);

	SelectObject(hdc, oldFont);
	ReleaseDC(hWnd, hdc);

	const LONG pad = LONG(groupBoxBorder) * 2;

	fullSize.cx = std::min(LONG(maxWidth), std::max({
		rcProfileView.right,
		rcStatus.right + rcPronouns.right + pronounStatusGap,
		rcAboutMe.right + pad,
		rcMemberSinceFull.right + pad,
#ifdef ADD_NOTES
		rcNote.right + pad,
#endif
#ifdef ADD_MESSAGES
		rcMessage.right + pad,
#endif
		(LONG) 0
	}));
	const int fullWidth = fullSize.cx;
	const int inGroupBoxWidth = fullWidth - groupBoxBorder * 2;
	fullSize.cx += windowBorder * 2;

	// Now that we know the final width of the window, calculate the role stuff
	if (gm && !gm->m_roles.empty() && gld)
	{
		RoleList::InitializeClass();
		RECT rcRolePos{};
		rcRolePos.right = inGroupBoxWidth;
		rcRolePos.bottom = ScaleByDPI(100);
		m_pRoleList = RoleList::Create(hWnd, &rcRolePos, IDC_ROLE_STATIC, false, false);

		std::vector<GuildRole> grs;
		for (Snowflake role : gm->m_roles) {
			grs.push_back(gld->m_roles[role]);
		}
		std::sort(grs.begin(), grs.end());
		for (auto& gr : grs) {
			m_pRoleList->AddRole(gr);
		}

		m_pRoleList->Update();

		// get the total outstanding size and update it:
		SetRectEmpty(&rcRoles);
		rcRoles.bottom = std::min(m_pRoleList->GetRolesHeight(), ScaleByDPI(200));
		rcRoles.right  = inGroupBoxWidth;
	}

	// Calculate the full size of the window
	fullSize.cy = windowBorder * 2;
	fullSize.cy += rcProfileView.bottom + interItemGap;
	temp = std::max(rcStatus.bottom, rcPronouns.bottom);
	fullSize.cy += temp + ((temp != 0) ? interItemGap : 0);
	fullSize.cy += separatorSize;
	rcGroupBox.bottom += interItemGap;
	if (rcAboutMe.bottom)         fullSize.cy += rcGroupBox.bottom + rcAboutMe.bottom;
	if (rcMemberSinceFull.bottom) fullSize.cy += rcMemberSinceFull.bottom;
	if (rcRoles.bottom)           fullSize.cy += rcGroupBox.bottom + rcRoles.bottom;
#ifdef ADD_NOTES
	if (rcNote.bottom)            fullSize.cy += rcGroupBox.bottom + rcNote.bottom;
#endif
#ifdef ADD_MESSAGE
	if (rcMessage.bottom)         fullSize.cy += rcGroupBox.bottom + rcMessage.bottom;
#endif
	rcGroupBox.bottom -= interItemGap;

	POINT pos{ };
	RECT place{};
	HWND hChild;

	pos.x = windowBorder;
	pos.y = windowBorder;

	// Create a profile view widget sized with rcProfileView
	place = rcProfileView;
	OffsetRect(&place, pos.x, pos.y);
	pos.y += rcProfileView.bottom + interItemGap;
	place.left -= ScaleByDPI(4);
	place.top  -= ScaleByDPI(2);

	if (pProf && pProf->m_bIsBot) {
		place.right += ScaleByDPI(4) + GetSystemMetrics(SM_CXSMICON);
	}

	m_pProfileView = ProfileView::Create(hWnd, &place, IDC_PROFILE_VIEW, false);
	if (!m_pProfileView) {
		free(name);
		free(userName);
	}
	else {
		m_pProfileView->SetData(name, userName, pProf->m_activeStatus, pProf->m_avatarlnk, pProf->m_bIsBot);
	}

	// Add pronouns and status if needed
	hChild = GetDlgItem(hWnd, IDC_PROFILE_PRONOUNS);
	if (rcPronouns.bottom) {
		MoveWindow(hChild, pos.x, pos.y, rcPronouns.right, rcPronouns.bottom, TRUE);
		ShowWindow(hChild, SW_SHOW);
		SetWindowText(hChild, pronouns);
		SetWindowFont(hChild, g_MessageTextFont, TRUE);
		pos.x += rcPronouns.right + pronounStatusGap;
	}
	else {
		ShowWindow(hChild, SW_HIDE);
	}
	
	hChild = GetDlgItem(hWnd, IDC_PROFILE_STATUS);
	if (rcStatus.bottom) {
		MoveWindow(hChild, pos.x, pos.y, rcStatus.right, rcStatus.bottom, TRUE);
		ShowWindow(hChild, SW_SHOW);
		SetWindowFont(hChild, g_DateTextFont, TRUE);
		SetWindowText(hChild, status);
	}
	else {
		ShowWindow(hChild, SW_HIDE);
	}

	pos.x = windowBorder;
	if (rcStatus.bottom || rcPronouns.bottom) {
		pos.y += std::max(rcStatus.bottom, rcPronouns.bottom) + interItemGap;
	}

	// Add a gap.
	pos.y += separatorSize;
	
	// Add about me text if needed.
	hChild = GetDlgItem(hWnd, IDC_ABOUTME_GROUP);
	if (rcAboutMe.bottom)
	{
		MoveWindow(hChild, pos.x, pos.y, fullWidth, rcAboutMe.bottom + rcGroupBox.bottom, TRUE);
		ShowWindow(hChild, SW_SHOW);
		SetWindowFont(hChild, g_MessageTextFont, TRUE);

		hChild = GetDlgItem(hWnd, IDC_EDIT_ABOUTME);
		MoveWindow(hChild, pos.x + groupBoxBorder, pos.y + rcGroupBox.bottom - groupBoxBorder, inGroupBoxWidth, rcAboutMe.bottom, TRUE);
		ShowWindow(hChild, SW_SHOW);
		SetWindowFont(hChild, g_MessageTextFont, TRUE);
		SetWindowText(hChild, bio);

		pos.y += rcAboutMe.bottom + rcGroupBox.bottom + interItemGap;
	}
	else
	{
		ShowWindow(hChild, SW_HIDE);
		ShowWindow(GetDlgItem(hWnd, IDC_EDIT_ABOUTME), SW_HIDE);
	}

	// Add member since text.
	hChild = GetDlgItem(hWnd, IDC_MEMBERSINCEGROUP);
	if (rcMemberSinceFull.bottom) // eh - probably always true
	{
		SetWindowFont(hChild, g_MessageTextFont, TRUE);
		ShowWindow(hChild, SW_SHOW);
		MoveWindow(hChild, pos.x, pos.y, fullWidth, rcMemberSinceFull.bottom, TRUE);
		LONG oldPosY = pos.y;
		pos.y += rcGroupBox.bottom - groupBoxBorder;
		pos.x += groupBoxBorder;

		// Add guild join, if applicable.
		if (rcMemberSinceGuild.bottom) {
			assert(gld);
			hChild = GetDlgItem(hWnd, IDC_ICON_GUILD);
			ShowWindow(hChild, SW_SHOW);
			MoveWindow(hChild, pos.x, pos.y, joinedAtIconSize, joinedAtIconSize, TRUE);
			bool hasAlpha = false;
			HBITMAP hbm = GetAvatarCache()->GetImage(gld->m_avatarlnk, hasAlpha)->GetFirstFrame();
			if (hbm) {
				hdc = GetDC(hWnd);
				int pps = GetProfilePictureSize();
				HBITMAP hnewbm = ResizeWithBackgroundColor(
					hdc,
					hbm,
					ri::GetSysColorBrush(COLOR_3DFACE),
					hasAlpha,
					joinedAtIconSize,
					joinedAtIconSize,
					pps,
					pps
				);
				ReleaseDC(hWnd, hdc);
				ShowWindow(hChild, SW_SHOW);
				HBITMAP holdbm = (HBITMAP) SendMessage(hChild, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM) hnewbm);
				if (holdbm) DeleteBitmap(holdbm);
			}
			hChild = GetDlgItem(hWnd, IDC_GUILD_JOIN_DATE);
			ShowWindow(hChild, SW_SHOW);
			SetWindowText(hChild, gldJoinedAt);
			SetWindowFont(hChild, g_MessageTextFont, TRUE);
			int wid = rcMemberSinceGuild.right, xpos = pos.x + joinedAtIconSize + ScaleByDPI(4);
			MoveWindow(hChild, xpos, pos.y, wid, rcMemberSinceGuild.bottom, TRUE);
			pos.x = std::max(wid + xpos + ScaleByDPI(6), int(pos.x) + inGroupBoxWidth / 2);
		}
		else {
			ShowWindow(GetDlgItem(hWnd, IDC_ICON_GUILD), SW_HIDE);
			ShowWindow(GetDlgItem(hWnd, IDC_GUILD_JOIN_DATE), SW_HIDE);
		}

		if (rcMemberSinceDiscord.bottom) {
			int icsz = GetSystemMetrics(SM_CXSMICON);
			hChild = GetDlgItem(hWnd, IDC_ICON_DISCORD);
			ShowWindow(hChild, SW_SHOW);
			MoveWindow(hChild, pos.x, pos.y, joinedAtIconSize, joinedAtIconSize, TRUE);

			// note: the old icon may have been set by the dialog itself.
			HICON hic = (HICON) SendMessage(hChild, STM_SETIMAGE, IMAGE_ICON, (LPARAM)ri::LoadImage(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_ICON)), IMAGE_ICON, icsz, icsz, LR_CREATEDIBSECTION));
			if (hic) DestroyIcon(hic);

			hChild = GetDlgItem(hWnd, IDC_DISCORD_JOIN_DATE);
			ShowWindow(hChild, SW_SHOW);
			SetWindowText(hChild, dscJoinedAt);
			SetWindowFont(hChild, g_MessageTextFont, TRUE);
			MoveWindow(hChild, pos.x + joinedAtIconSize + ScaleByDPI(4), pos.y, rcMemberSinceDiscord.right, rcMemberSinceDiscord.bottom, TRUE);
		}
		else {
			ShowWindow(GetDlgItem(hWnd, IDC_ICON_DISCORD), SW_HIDE);
			ShowWindow(GetDlgItem(hWnd, IDC_DISCORD_JOIN_DATE), SW_HIDE);
		}

		pos.y = oldPosY + rcMemberSinceFull.bottom + interItemGap;
		pos.x = windowBorder;
	}
	else
	{
		ShowWindow(hChild, SW_HIDE);
		ShowWindow(GetDlgItem(hWnd, IDC_ICON_GUILD), SW_HIDE);
		ShowWindow(GetDlgItem(hWnd, IDC_ICON_DISCORD), SW_HIDE);
		ShowWindow(GetDlgItem(hWnd, IDC_GUILD_JOIN_DATE), SW_HIDE);
		ShowWindow(GetDlgItem(hWnd, IDC_DISCORD_JOIN_DATE), SW_HIDE);
	}

	// Add role list.
	if (gm && !gm->m_roles.empty() && gld)
	{
		hChild = GetDlgItem(hWnd, IDC_ROLE_GROUP);
		ShowWindow(hChild, SW_SHOW);
		SetWindowFont(hChild, g_MessageTextFont, TRUE);
		MoveWindow(hChild, pos.x, pos.y, fullWidth, rcRoles.bottom + rcGroupBox.bottom, TRUE);
		pos.y += rcGroupBox.bottom - groupBoxBorder;
		hChild = GetDlgItem(hWnd, IDC_ROLE_STATIC);
		MoveWindow(hChild, pos.x + groupBoxBorder, pos.y, inGroupBoxWidth, rcRoles.bottom, SWP_NOSIZE | SWP_NOZORDER);
		pos.y += rcRoles.bottom + groupBoxBorder + interItemGap;
		ShowWindow(hChild, SW_SHOW);
	}
	else {
		ShowWindow(GetDlgItem(hWnd, IDC_ROLE_GROUP), SW_HIDE);
	}

	// Add note text box.
#ifdef ADD_NOTES
	hChild = GetDlgItem(hWnd, IDC_NOTE_GROUP);
	ShowWindow(hChild, SW_SHOW);
	SetWindowFont(hChild, g_MessageTextFont, TRUE);
	MoveWindow(hChild, pos.x, pos.y, fullWidth, rcNote.bottom + rcGroupBox.bottom, TRUE);

	hChild = GetDlgItem(hWnd, IDC_NOTE_TEXT);
	ShowWindow(hChild, SW_SHOW);
	SetWindowFont(hChild, g_MessageTextFont, TRUE);
	SetWindowText(hChild, note);
	MoveWindow(hChild, pos.x + groupBoxBorder, pos.y + rcGroupBox.bottom - groupBoxBorder, inGroupBoxWidth, rcNote.bottom, TRUE);
	Edit_SetReadOnly(hChild, !bShouldNoteBeEditable);

	pos.y += rcNote.bottom + rcGroupBox.bottom + interItemGap;
#else
	ShowWindow(GetDlgItem(hWnd, IDC_NOTE_GROUP), SW_HIDE);
	ShowWindow(GetDlgItem(hWnd, IDC_NOTE_TEXT), SW_HIDE);
#endif

#ifdef ADD_MESSAGE
	hChild = GetDlgItem(hWnd, IDC_MESSAGE_GROUP);
	ShowWindow(hChild, SW_SHOW);
	SetWindowFont(hChild, g_MessageTextFont, TRUE);
	MoveWindow(hChild, pos.x, pos.y, fullWidth, rcMessage.bottom + rcGroupBox.bottom, TRUE);

	hChild = GetDlgItem(hWnd, IDC_MESSAGE_TEXT);
	ShowWindow(hChild, SW_SHOW);
	SetWindowFont(hChild, g_MessageTextFont, TRUE);
	MoveWindow(hChild, pos.x + groupBoxBorder, pos.y + rcGroupBox.bottom - groupBoxBorder, inGroupBoxWidth, rcMessage.bottom, TRUE);

	pos.y += rcMessage.bottom + rcGroupBox.bottom + interItemGap;
#else
	ShowWindow(GetDlgItem(hWnd, IDC_MESSAGE_GROUP), SW_HIDE);
	ShowWindow(GetDlgItem(hWnd, IDC_MESSAGE_TEXT), SW_HIDE);
#endif

	// note: not freeing name and username because they're being used in and managed by the profile view we create below
	free(status);
	free(bio);
	free(note);
	free(pronouns);
	free(dscJoinedAt);
	free(gldJoinedAt);

	return FALSE;
}

void ProfilePopout::Paint(HWND hWnd, HDC hdc)
{
	RECT rect = {};
	GetClientRect(hWnd, &rect);

	ri::DrawEdge(hdc, &rect, EDGE_RAISED, BF_RECT | BF_MIDDLE);
}

void ProfilePopout::FlushNote()
{
	Profile* pProf = GetProfileCache()->LookupProfile(m_user, "...", "...", "");
	if (!pProf)
		return;
	if (!pProf->m_bNoteFetched) // note: might be redundant
		return;

	HWND editHwnd = GetDlgItem(m_hwnd, IDC_NOTE_TEXT);
	int length = GetWindowTextLength(editHwnd);
	TCHAR* data = new TCHAR[length + 1];
	GetWindowText(editHwnd, data, length + 1);
	std::string noteStr = MakeStringFromEditData(data);
	delete[] data;

	if (noteStr != pProf->m_note)
	{
		// Set the note!
		pProf->m_note = noteStr;

		// Flush it
		pProf->PutNote();
	}
}

BOOL CALLBACK ProfilePopout::Proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_PAINT: {
			PAINTSTRUCT ps = {};
			HDC hdc = BeginPaint(hWnd, &ps);
			Paint(hWnd, hdc);
			EndPaint(hWnd, &ps);
			break;
		}

		case WM_PRINT:
		case WM_PRINTCLIENT: {
			Paint(hWnd, (HDC) wParam);
			break;
		}

		case WM_INITDIALOG:
		case WM_UPDATEPROFILEPOPOUT: {
			m_size = {};
			BOOL res = Layout(hWnd, m_size);
			if (!res) {
				SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, m_size.cx, m_size.cy, SWP_NOACTIVATE | SWP_NOREPOSITION | SWP_NOZORDER | SWP_NOMOVE);
				InvalidateRect(hWnd, NULL, TRUE);
			}
			return res;
		}

		case WM_DESTROY: {
			FlushNote();

			HWND hMSDIcon = GetDlgItem(hWnd, IDC_ICON_DISCORD);
			HWND hMSGIcon = GetDlgItem(hWnd, IDC_ICON_GUILD);
			HICON hic = (HICON) SendMessage(hMSDIcon, STM_SETIMAGE, IMAGE_ICON, (LPARAM) NULL);
			if (hic)
				DestroyIcon(hic); // the icon was loaded without LR_SHARED
			HBITMAP hbm = (HBITMAP) SendMessage(hMSGIcon, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM) NULL);
			if (hbm)
				DeleteBitmap(hbm);

			m_hwnd = NULL;
			SAFE_DELETE(m_pRoleList);
			SAFE_DELETE(m_pProfileView);

			if (m_hBitmap) {
				//DeleteBitmap(m_hBitmap);
				m_hBitmap = NULL;
			}
			return TRUE;
		}
	}

	return FALSE;
}

void ProfilePopout::Dismiss()
{
	if (m_hwnd)
	{
		DestroyWindow(m_hwnd);
		m_hwnd = NULL;
	}
}

HWND ProfilePopout::GetHWND()
{
	return m_hwnd;
}

void ProfilePopout::DeferredShow(const ShowProfilePopoutParams& params)
{
	Snowflake user = params.m_user;
	Snowflake guild = params.m_guild;
	int x = params.m_pt.x;
	int y = params.m_pt.y;
	bool bRightJustify = params.m_bRightJustify;

	Profile* pf = GetProfileCache()->LookupProfile(user, "", "", "", false);
	if (!pf)
		return; // that profile is nonexistent!

	Dismiss();
	m_user = user;
	m_guild = guild;
	m_size = { 10, 10 };
	m_hwnd = CreateDialog(g_hInstance, MAKEINTRESOURCE(DMDI(IDD_DIALOG_PROFILE_POPOUT)), g_Hwnd, &Proc);

	// calculated in WM_CREATE
	int wndWidth = m_size.cx;
	int wndHeight = m_size.cy;

	if (bRightJustify) {
		x -= wndWidth;
	}

	POINT pt { x + wndWidth / 2, y + wndHeight / 2 };
	HMONITOR mon = ri::MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);

	if (mon) {
		MONITORINFO mi{};
		mi.cbSize = sizeof mi;
		ri::GetMonitorInfo(mon, &mi);

		if (x  < mi.rcWork.left)
			x  = mi.rcWork.left;
		if (x >= mi.rcWork.right - wndWidth)
			x  = mi.rcWork.right - wndWidth;
		if (y  < mi.rcWork.top)
			y  = mi.rcWork.top;
		if (y >= mi.rcWork.bottom - wndHeight)
			y  = mi.rcWork.bottom - wndHeight;
	}

	MoveWindow(m_hwnd, x, y, wndWidth, wndHeight, TRUE);
	//ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
	
	// NOTE: You need to disable the above ShowWindow to use this. Also, it's
	// kind of broken, in that the message text box just doesn't render.
	ri::AnimateWindow(m_hwnd, 200, AW_BLEND);
}

void ProfilePopout::Show(Snowflake user, Snowflake guild, int x, int y, bool bRightJustify)
{
	ShowProfilePopoutParams* parms = new ShowProfilePopoutParams;
	parms->m_user = user;
	parms->m_guild = guild;
	parms->m_pt = { x, y };
	parms->m_bRightJustify = bRightJustify;

	// N.B. The parms struct is now owned by the main window.
	// Don't send immediately (the message gets enqueued instead). It'd be useless to do
	// it like that as it ends up just calling the deferred version directly
	if (!PostMessage(g_Hwnd, WM_SHOWPROFILEPOPOUT, 0, (LPARAM) parms))
		delete parms;
}

void ProfilePopout::Update()
{
	SendMessage(m_hwnd, WM_UPDATEPROFILEPOPOUT, 0, 0);
}
