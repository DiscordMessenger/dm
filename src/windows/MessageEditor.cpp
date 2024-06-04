#include <climits>
#include "Config.hpp"
#include "MessageEditor.hpp"
#include "MessageList.hpp"
#include "UploadDialog.hpp"
#include "../discord/ProfileCache.hpp"
#include "../discord/MessageCache.hpp"
#include "../discord/LocalSettings.hpp"

#define T_MESSAGE_EDITOR_CLASS TEXT("DMMessageEditorClass")
#define T_MESSAGE_EDITOR_AUTOCOMPLETE_CLASS TEXT("DMMessageEditorAutoCompleteClass")

// allow a query every 50 ms, to not saturate Discord's servers
constexpr uint64_t QUERY_RATE = 50;

WNDPROC MessageEditor::m_editWndProc;
bool MessageEditor::m_shiftHeld;

static BOOL IsJustWhiteSpace(TCHAR* str)
{
	while (*str)
	{
		if (*str != ' ' && *str != '\r' && *str != '\n' && *str != '\t')
			// Not going to add exceptions for ALL of the characters that Discord thinks are white space.
			// Just assume good faith from the user.  They'll get an "invalid request" response if they
			// try :(
			return FALSE;

		str++;
	}

	return TRUE;
}

static BOOL ContainsAuthenticationToken(TCHAR* str)
{
	const DWORD LengthToken = 58;
	const DWORD PlaceFirstSep = 23;
	const DWORD PlaceSecondSep = 30;
	DWORD Length = 0;
	for (; str[Length] != 0; Length++);

	if (Length < LengthToken) return FALSE;

	for (DWORD i = 0; i <= Length - LengthToken; i++)
	{
		if (str[i + PlaceFirstSep] != L'.' || str[i + PlaceSecondSep] != L'.')
			continue;

		bool fake = false;
		for (DWORD j = 0; j < LengthToken; j++)
		{
			if (j == PlaceFirstSep || j == PlaceSecondSep)
				continue;

			if (strchr("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-_", (char)str[i + j]) == NULL) {
				fake = true;
				break;
			}
		}

		if (!fake)
			return TRUE;
	}

	return FALSE;
}

MessageEditor::MessageEditor()
{
}

MessageEditor::~MessageEditor()
{
	if (m_autoComplete_hwnd)
	{
		DestroyWindow(m_autoComplete_hwnd);
		// assert(b && "Was window destroyed?"); <-- apparently it is being destroyed by the main BEFORE child controls?
		// what the hell.
		m_autoComplete_hwnd = NULL;
		m_autoCompleteList_hwnd = NULL;
	}
	if (m_hwnd)
	{
		BOOL b = DestroyWindow(m_hwnd);
		assert(b && "Was window destroyed?");
		m_send_hwnd = NULL;
		m_btnUpload_hwnd = NULL;
		m_mentionText_hwnd = NULL;
		m_mentionName_hwnd = NULL;
		m_mentionCheck_hwnd = NULL;
		m_mentionCancel_hwnd = NULL;
		m_mentionJump_hwnd = NULL;
		m_editingMessage_hwnd = NULL;
	}

	assert(!m_autoCompleteList_hwnd);
	assert(!m_send_hwnd);
	assert(!m_btnUpload_hwnd);
	assert(!m_mentionText_hwnd);
	assert(!m_mentionName_hwnd);
	assert(!m_mentionCheck_hwnd);
	assert(!m_mentionCancel_hwnd);
	assert(!m_mentionJump_hwnd);
	assert(!m_editingMessage_hwnd);
}

void MessageEditor::UpdateTextBox()
{
	bool mayType = GetDiscordInstance()->GetCurrentChannel() && GetDiscordInstance()->GetCurrentChannel()->HasPermission(PERM_SEND_MESSAGES);
	bool wasDisabled = EnableWindow(m_edit_hwnd, mayType);
	EnableWindow(m_send_hwnd, mayType);

	if (mayType && wasDisabled) {
		SetWindowText(m_edit_hwnd, TEXT(""));
	}
	if (!mayType && !wasDisabled) {
		SetWindowText(m_edit_hwnd, TmGetTString(IDS_CANT_SEND_MESSAGES));
	}
}

void MessageEditor::ShowOrHideReply(bool shown)
{
	int nCmdShow = shown ? SW_SHOW : SW_HIDE;
	ShowWindow(m_mentionText_hwnd,   nCmdShow);
	ShowWindow(m_mentionName_hwnd,   nCmdShow);
	ShowWindow(m_mentionCheck_hwnd,  nCmdShow);
	ShowWindow(m_mentionJump_hwnd,   nCmdShow);
}

void MessageEditor::ShowOrHideEdit(bool shown)
{
	int nCmdShow = shown ? SW_SHOW : SW_HIDE;
	ShowWindow(m_editingMessage_hwnd, nCmdShow);
}

void MessageEditor::UpdateCommonButtonsShown()
{
	bool shown = m_bReplying || m_bEditing;
	int nCmdShow = shown ? SW_SHOW : SW_HIDE;
	ShowWindow(m_mentionCancel_hwnd, nCmdShow);
}

bool MessageEditor::IsUploadingAllowed()
{
	Channel* pChan = GetDiscordInstance()->GetCurrentChannel();
	if (!pChan)
		return true;

	return pChan->HasPermission(PERM_ATTACH_FILES);
}

void MessageEditor::Expand(int Amount)
{
	if (Amount == 0)
		return;

	extern MessageList* g_pMessageList; // main.cpp - This sucks, should ideally have a function for that
	m_expandedBy += Amount;

	ShowOrHideReply(m_bReplying);

	RECT rcSend{};
	GetChildRect(m_hwnd, m_send_hwnd, &rcSend);

	RECT rect{}, rectM{};
	GetChildRect(g_Hwnd, g_pMessageList->m_hwnd, &rect);
	int oldBottom = rect.bottom;
	rect.bottom -= Amount;
	MoveWindow(g_pMessageList->m_hwnd, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, FALSE);
	rectM = rect;

	GetChildRect(g_Hwnd, m_hwnd, &rect);
	rect.top -= Amount;
	MoveWindow(m_hwnd, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, FALSE);

	// Invalidate the gap
	RECT gap = rectM;
	gap.top = std::min(int(rectM.bottom) - 2, oldBottom - 2);
	gap.bottom = rect.top;
	InvalidateRect(g_Hwnd, &gap, TRUE);

	RECT rcClient{};
	GetClientRect(m_hwnd, &rcClient);
	RECT rcEdit{};
	GetChildRect(m_hwnd, m_edit_hwnd, &rcEdit);
	int scaled4 = ScaleByDPI(4);
	rcEdit.left   += scaled4;
	rcEdit.top    += scaled4;
	rcEdit.right  -= scaled4;
	rcEdit.bottom -= scaled4;

	// Invalidate the send button's old position
	HRGN rgnInv = CreateRectRgn(W32RECT(rcClient));
	HRGN rgnEdit = CreateRectRgn(W32RECT(rcEdit));
	int res = CombineRgn(rgnInv, rgnInv, rgnEdit, RGN_DIFF);// take out the edit widget

	// Invalidate it now
	InvalidateRgn(m_hwnd, rgnInv,  TRUE);
	InvalidateRgn(m_hwnd, rgnEdit, FALSE);

	DeleteRgn(rgnEdit);
	DeleteRgn(rgnInv);
}

bool MessageEditor::MentionRepliedUser()
{
	return Button_GetCheck(m_mentionCheck_hwnd) == BST_CHECKED;
}

void MessageEditor::TryToSendMessage()
{
	int length = GetWindowTextLength(m_edit_hwnd);
	TCHAR* data = new TCHAR[length + 1];

	GetWindowText(m_edit_hwnd, data, length + 1);

	if (IsJustWhiteSpace(data))
	{
		delete[] data;
		return;
	}

	if (ContainsAuthenticationToken(data))
	{
		if (MessageBox(g_Hwnd, TmGetTString(IDS_MAY_CONTAIN_TOKEN), TmGetTString(IDS_HOLD_UP_CONFIRM), MB_YESNO | MB_ICONQUESTION) != IDYES) {
			delete[] data;
			return;
		}
	}

	SendMessageParams parms;
	parms.m_rawMessage = data;
	parms.m_replyTo = ReplyingTo();
	parms.m_bEdit = m_bEditing;
	parms.m_bReply = m_bReplying;
	parms.m_bMention = MentionRepliedUser();

	// send it as a message to the main window
	if (!SendMessage(g_Hwnd, WM_SENDMESSAGE, 0, (LPARAM) &parms))
	{
		// Message was sent, so clear the box
		SetWindowText(m_edit_hwnd, TEXT(""));
		m_bReplying = false;
		m_bEditing = false;
		m_replyMessage = 0;
		m_replyName = "";
		Expand(-m_expandedBy);
	}
	delete[] data;
}

void MessageEditor::StartReply(Snowflake messageID, Snowflake authorID)
{
	StopEdit();

	bool wasReplying = m_bReplying;

	Profile* pf = GetProfileCache()->LookupProfile(authorID, "", "", "", false);
	std::string userName = "<UNKNOWN>";
	if (pf)
	{
		Snowflake guildID = GetDiscordInstance()->GetCurrentGuildID();
		userName = pf->GetName(guildID);

		COLORREF clr = CLR_NONE;
		if (guildID) {
			clr = GetNameColor(pf, guildID);
		}

		if (clr == CLR_NONE)
			clr = GetSysColor(COLOR_WINDOWTEXT);

		m_userNameColor = clr;
	}

	m_bReplying = true;
	m_replyName = userName;
	m_replyMessage = messageID;

	if (!wasReplying)
		Expand(m_mentionAreaHeight);

	RECT rect{};
	GetChildRect(m_hwnd, m_mentionName_hwnd, &rect);

	LPTSTR newStr = ConvertCppStringToTString(userName);
	SetWindowText(m_mentionName_hwnd, newStr);
	InvalidateRect(m_hwnd, &rect, TRUE);
	free(newStr);

	bool mention = GetLocalSettings()->ReplyMentionByDefault();
	SendMessage(m_mentionCheck_hwnd, BM_SETCHECK, mention ? BST_CHECKED : BST_UNCHECKED, 0);

	SetFocus(m_edit_hwnd);
}

void MessageEditor::StopReply()
{
	bool wasReplying = m_bReplying;
	m_bReplying = false;
	m_replyMessage = 0;
	m_replyName = "";

	if (wasReplying)
		Expand(-m_mentionAreaHeight);
}

void MessageEditor::StartEdit(Snowflake messageID)
{
	Message* pMsg = GetMessageCache()->GetLoadedMessage(GetDiscordInstance()->GetCurrentChannelID(), messageID);
	if (!pMsg)
		return;

	StopReply();

	bool wasEditing = m_bEditing;

	m_bEditing = true;
	m_replyName = pMsg->m_message;
	m_replyMessage = messageID;

	// Copy the message text, add carriage returns when line feeds show up, and set it to the edit box.
	std::string msg2 = "";
	msg2.reserve(m_replyName.size() * 2 + 5);
	for (char c : m_replyName) {
		if (c == '\n') msg2 += '\r';
		msg2 += c;
	}
	LPTSTR tstr = ConvertCppStringToTString(msg2);
	msg2.clear();
	m_replyName.clear();

	SetWindowText(m_edit_hwnd, tstr);
	free(tstr);

	if (!wasEditing)
		Expand(m_mentionAreaHeight);

	SendMessage(m_edit_hwnd, WM_UPDATETEXTSIZE, 0, 0);
}

void MessageEditor::StopEdit()
{
	bool wasEditing = m_bEditing;
	m_bEditing = false;
	m_replyMessage = 0;
	m_replyName = "";

	if (wasEditing)
	{
		Expand(-m_mentionAreaHeight);
		SetWindowText(m_edit_hwnd, TEXT(""));
		SendMessage(m_edit_hwnd, WM_UPDATETEXTSIZE, 0, 0);
	}
}

void MessageEditor::AutoCompleteLookup(const std::string& word, char query, std::vector<AutoCompleteMatch>& matches)
{
	switch (query)
	{
		case '#': // CHANNELS
		{
			Guild* pGld = GetDiscordInstance()->GetCurrentGuild();
			if (!pGld)
				break;
			if (pGld->m_snowflake == 0)
				break; // can't do that in a DM channel

			for (auto& chan : pGld->m_channels) {
				if (chan.IsCategory() || chan.IsVoice() || chan.IsDM())
					continue;

				float fzm = word.empty() ? 1.0f : CompareFuzzy(chan.m_name, word.c_str());
				if (fzm != 0.0f)
					matches.push_back(AutoCompleteMatch(chan.m_name, fzm));
			}
			break;
		}
		case '@':
		{
			Guild* pGld = GetDiscordInstance()->GetCurrentGuild();
			if (!pGld)
				break;

			if (pGld->m_snowflake == 0)
			{
				// can do that in DM channels, but have to do it a bit differently.
				Channel* pChan = GetDiscordInstance()->GetCurrentChannel();
				if (!pChan)
					break;

				for (auto& recid : pChan->m_recipients)
				{
					Profile* pf = GetProfileCache()->LookupProfile(recid, "", "", "", false);
					if (!pf)
						continue;

					float fm = word.empty() ? 1.0f : pf->FuzzyMatch(word.c_str(), 0);
					if (fm != 0.0f) {
						// Add user name instead, since that's the one that will be resolved
						matches.push_back(AutoCompleteMatch(pf->GetUsername(), fm));
					}
				}
			}
			else
			{
				// Scan for guild members.
				for (auto& recid : pGld->m_knownMembers)
				{
					Profile* pf = GetProfileCache()->LookupProfile(recid, "", "", "", false);
					if (!pf)
						continue;

					float fm = word.empty() ? 1.0f : pf->FuzzyMatch(word.c_str(), pGld->m_snowflake);
					if (fm != 0.0f) {
						// Add user name instead, since that's the one that will be resolved
						matches.push_back(AutoCompleteMatch(pf->GetUsername(), fm));
					}
				}
			}

			if (word.empty())
				break;

			// send a query to Discord if needed
			if (m_lastRemoteQuery + QUERY_RATE < GetTimeMs())
			{
				// query now!
				Snowflake guildID = GetDiscordInstance()->GetCurrentGuildID();

				if (guildID)
					GetDiscordInstance()->RequestGuildMembers(guildID, word, true);

				m_lastRemoteQuery = GetTimeMs();
			}

			break;
		}
	}
}

void MessageEditor::_AutoCompleteLookup(void* context, const std::string& keyWord, char query, std::vector<AutoCompleteMatch>& matches)
{
	MessageEditor* pEditor = (MessageEditor*) context;
	pEditor->AutoCompleteLookup(keyWord, query, matches);
}

void MessageEditor::OnUpdateText()
{
	EditWndProc(m_edit_hwnd, WM_UPDATETEXTSIZE, 0, 0);

	if (!IsWindowEnabled(m_edit_hwnd))
		return;

	/*
	// This is a VERY loose approximate.  For every \n character there will also be a
	// \r character in the input buffer.  If you need this to be exact, make it go through
	// the entire text every character, or count the characters.  Use for speed.
	int length = Edit_GetTextLength(m_edit_hwnd);
	SendMessage(g_Hwnd, WM_UPDATEMESSAGELENGTH, 0, (LPARAM)length);
	return;
	*/

	// How slow could this possibly be.
	int length = Edit_GetTextLength(m_edit_hwnd);

	TCHAR* tchr = new TCHAR[length + 1];
	Edit_GetText(m_edit_hwnd, tchr, length + 1);

	int actualLength = length;
	for (int i = 0; i < length; i++) {
		if (tchr[i] == (TCHAR)'\r')
			actualLength--;
	}

	SendMessage(g_Hwnd, WM_UPDATEMESSAGELENGTH, 0, (LPARAM)actualLength);

	m_autoComplete.Update(tchr, length);
	delete[] tchr;
}

LRESULT MessageEditor::EditWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	MessageEditor* pThis = (MessageEditor*) GetWindowLongPtr(GetParent(hWnd), GWLP_USERDATA);
	assert(pThis);

	switch (uMsg)
	{
		case WM_DESTROY:
		{
			SetWindowLongPtr(hWnd, GWLP_WNDPROC,  (LONG_PTR) m_editWndProc);
			pThis->m_edit_hwnd = NULL;
			break;
		}
		case WM_KEYUP:
		case WM_KEYDOWN:
		{
			if (pThis->m_autoComplete.HandleKeyMessage(uMsg, wParam, lParam))
				return 0;

			if (wParam == VK_SHIFT)
				m_shiftHeld = uMsg == WM_KEYDOWN;

			break;
		}
		case WM_UPDATETEXTSIZE:
		{
			RECT rect{};
			GetClientRect(g_Hwnd, &rect);
			int maxHeight = (rect.bottom - rect.top) / 3;

			// Now check for expansion.
			// TODO: Figure out why I had to lower the line height by 1 pixel to make the cursor not deviate 1px up
			int lineCount = Edit_GetLineCount(hWnd);
			int expandByTarget = (lineCount - 1) * (pThis->m_lineHeight - 1);
			if (pThis->m_bReplying)
				expandByTarget += pThis->m_mentionAreaHeight;
			if (pThis->m_bEditing)
				expandByTarget += pThis->m_mentionAreaHeight;
			expandByTarget = std::min(expandByTarget, maxHeight);

			pThis->Expand(expandByTarget - pThis->m_expandedBy);
			break;
		}
		case WM_PASTE:
		case WM_CUT:
		case WM_COPY:
		case WM_SETTEXT:
		{
			LRESULT lres = m_editWndProc(hWnd, uMsg, wParam, lParam);
			pThis->OnUpdateText();
			return lres;
		}
		case WM_CHAR:
		{
			if (pThis->m_autoComplete.HandleCharMessage(uMsg, wParam, lParam))
				return 0;

			if (!GetDiscordInstance()->GetCurrentChannel())
				break;

			if (!GetDiscordInstance()->GetCurrentChannel()->HasPermission(PERM_SEND_MESSAGES))
				break;

			if (wParam == '\r' && !m_shiftHeld)
			{
				pThis->TryToSendMessage();
				return 0;
			}

			// Let the edit control modify the text first.
			LRESULT lres = m_editWndProc(hWnd, uMsg, wParam, lParam);
			pThis->OnUpdateText();
			GetDiscordInstance()->Typing();
			return lres;
		}
	}

	return m_editWndProc(hWnd, uMsg, wParam, lParam);
}

void MessageEditor::Layout()
{
	// Re-position controls
	RECT rcClient{};
	GetClientRect(m_hwnd, &rcClient);

	int sendButtonWidth = ScaleByDPI(SEND_BUTTON_WIDTH);
	int sendButtonHeight = ScaleByDPI(SEND_BUTTON_HEIGHT);
	LOGFONT lf{};
	if (!GetObject(g_TypingRegFont, sizeof(LOGFONT), &lf)) {
		assert(!"uh oh");
		return;
	}

	// between 24 and 28.  We gotta have space.
	// TODO: Should really be able to expand up to a couple lines
	m_lineHeight = MulDiv(abs(lf.lfHeight), 96, 72);
	m_mentionAreaHeight = m_lineHeight + ScaleByDPI(4);

	if (m_bReplying)
	{
		ShowOrHideReply(true);

		RECT rcMentionArea = rcClient;
		rcMentionArea.right -= ScaleByDPI(10) + sendButtonWidth;
		rcMentionArea.bottom = rcMentionArea.top + m_mentionAreaHeight;
		rcClient.top = rcMentionArea.bottom;

		RECT rc = rcMentionArea;
		rc.left = rc.right - ScaleByDPI(25);
		MoveWindow(m_mentionCancel_hwnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);

		rc.right = rc.left;
		rc.left = rc.right - ScaleByDPI(25);
		MoveWindow(m_mentionJump_hwnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);

		// Reposition the mention text and check windows
		int mid = (rcMentionArea.left + rcMentionArea.right) / 2;
		rc.right = rc.left;
		rc.left = rc.right - m_mentionTextWidth - ScaleByDPI(CHECKBOX_INTERNAL_SIZE + 6);
		if (rc.left < mid)
			rc.left = mid;
		MoveWindow(m_mentionCheck_hwnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);

		rc.right = rc.left;
		rc.left = rcMentionArea.left + m_replyToTextWidth;
		MoveWindow(m_mentionName_hwnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);

		rc.right = rc.left;
		rc.left = rcMentionArea.left;
		MoveWindow(m_mentionText_hwnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
	}
	else
	{
		ShowOrHideReply(false);
	}

	if (m_bEditing)
	{
		ShowOrHideEdit(true);

		RECT rcMentionArea = rcClient;
		rcMentionArea.right -= ScaleByDPI(10) + sendButtonWidth;
		rcMentionArea.bottom = rcMentionArea.top + m_mentionAreaHeight;
		rcClient.top = rcMentionArea.bottom;

		RECT rc = rcMentionArea;
		rc.left = rc.right - ScaleByDPI(25);
		MoveWindow(m_mentionCancel_hwnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);

		rc.right = rc.left;
		rc.left = rcMentionArea.left;
		MoveWindow(m_editingMessage_hwnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
	}
	else
	{
		ShowOrHideEdit(false);
	}

	UpdateCommonButtonsShown();

	RECT rc = rcClient;
	rc.right -= ScaleByDPI(10) + sendButtonWidth;
	int minHeight = std::max(m_lineHeight + ScaleByDPI(8), sendButtonHeight);
	int height = rc.bottom - rc.top;
	if (height < minHeight)
		height = minHeight;
	
	bool isUploadingAllowed = IsUploadingAllowed();
	RECT rcButton = rc;
	rcButton.right = rcButton.left + ScaleByDPI(30);
	if (isUploadingAllowed)
	{
		RECT rc2 = rcButton;
		rc2.top = rc2.top + (rcButton.bottom - rcButton.top - sendButtonHeight) / 2;
		rc2.bottom = rc2.top + sendButtonHeight;
		ShowWindow(m_btnUpload_hwnd, SW_SHOW);
		MoveWindow(m_btnUpload_hwnd, rc2.left, rc2.top, rc2.right - rc2.left, sendButtonHeight, TRUE);
		rc.left = rcButton.right + ScaleByDPI(2);
	}
	else
	{
		ShowWindow(m_btnUpload_hwnd, SW_HIDE);
	}
	
	MoveWindow(m_edit_hwnd, rc.left, rc.top, rc.right - rc.left, height, TRUE);

	if (isUploadingAllowed != m_bWasUploadingAllowed) {
		rcButton.right += ScaleByDPI(6);
		InvalidateRect(m_hwnd, &rcButton, TRUE);
	}
	
	m_bWasUploadingAllowed = isUploadingAllowed;

	rc = rcClient;
	rc.left = rc.right - sendButtonWidth;
	rc.right = rc.left + sendButtonWidth;
	rc.top += (rcButton.bottom - rcButton.top - sendButtonHeight) / 2;
	rc.bottom = rc.top + sendButtonHeight;
	MoveWindow(m_send_hwnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
}

LRESULT MessageEditor::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	MessageEditor* pThis = (MessageEditor*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

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
			pThis->m_btnUpload_hwnd = NULL;
			pThis->m_mentionText_hwnd = NULL;
			pThis->m_mentionName_hwnd = NULL;
			pThis->m_mentionCheck_hwnd = NULL;
			pThis->m_mentionCancel_hwnd = NULL;
			pThis->m_mentionJump_hwnd = NULL;
			pThis->m_editingMessage_hwnd = NULL;
			break;
		}
		case WM_SIZE:
		{
			pThis->Layout();
			break;
		}
		case WM_COMMAND:
		{
			switch (wParam) {
				case CID_MESSAGEINPUTSEND:
					pThis->TryToSendMessage();
					break;
				case CID_MESSAGEREPLYCANCEL:
					pThis->StopEdit();
					pThis->StopReply();
					break;
				case CID_MESSAGEREPLYJUMP: {
					if (pThis->m_replyMessage)
						SendMessage(g_Hwnd, WM_SENDTOMESSAGE, 0, (LPARAM) &pThis->m_replyMessage);
					break;
				case CID_MESSAGEUPLOAD:
					UploadDialogShow();
					break;
				}
			}
			break;
		}
		case WM_CTLCOLORSTATIC:
		{
			if ((HWND) lParam == pThis->m_mentionName_hwnd)
			{
				SetTextColor((HDC) wParam, pThis->m_userNameColor);
				SetBkColor((HDC) wParam, GetSysColor(COLOR_3DFACE));
				return (LRESULT) GetSysColorBrush(COLOR_3DFACE);
			}
			break;
		}
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void MessageEditor::InitializeClass()
{
	WNDCLASS wc;
	ZeroMemory(&wc, sizeof wc);
	wc.lpszClassName = T_MESSAGE_EDITOR_CLASS;
	wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
	wc.style         = 0;
	wc.hCursor       = LoadCursor (0, IDC_ARROW);
	wc.lpfnWndProc   = MessageEditor::WndProc;

	RegisterClass(&wc);
}

MessageEditor* MessageEditor::Create(HWND hwnd, LPRECT pRect)
{
	MessageEditor* newThis = new MessageEditor;

	int sendButtonWidth = 1, sendButtonHeight = 1;
	int width = pRect->right - pRect->left, height = pRect->bottom - pRect->top;

	newThis->m_parent_hwnd = hwnd;
	newThis->m_hwnd = CreateWindowEx(
		0,
		T_MESSAGE_EDITOR_CLASS,
		NULL,
		WS_CHILD | WS_VISIBLE,
		pRect->left, pRect->top, width, height,
		hwnd,
		(HMENU)CID_MESSAGEEDITOR,
		g_hInstance,
		newThis
	);

	newThis->m_edit_hwnd = CreateWindowEx(
		WS_EX_CLIENTEDGE,
		TEXT("EDIT"),
		NULL,
		WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL,
		0, 0,
		width - ScaleByDPI(10) - sendButtonWidth, height,
		newThis->m_hwnd,
		(HMENU)CID_MESSAGEINPUT,
		g_hInstance,
		newThis
	);

	newThis->m_send_hwnd = CreateWindowEx(
		0,
		TEXT("BUTTON"),
		TmGetTString(IDS_SEND_MESSAGE),
		WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
		width - sendButtonWidth,
		(height - sendButtonHeight) / 2,
		sendButtonWidth,
		sendButtonHeight,
		newThis->m_hwnd,
		(HMENU)CID_MESSAGEINPUTSEND,
		g_hInstance,
		newThis
	);

	// Add the mention items
	LPCTSTR editingText  = TmGetTString(IDS_EDITING_MESSAGE);
	LPCTSTR replyingText = TmGetTString(IDS_REPLYING_TO_MESSAGE);
	LPCTSTR mentionText  = TmGetTString(IDS_MENTION);
	newThis->m_editingMessage_hwnd = CreateWindow(WC_STATIC, editingText,  WS_CHILD | SS_SIMPLE | SS_CENTERIMAGE, 0, 0, 1, 1, newThis->m_hwnd, (HMENU)CID_MESSAGEEDITINGLBL,  g_hInstance, NULL);
	newThis->m_mentionText_hwnd    = CreateWindow(WC_STATIC, replyingText, WS_CHILD | SS_SIMPLE | SS_CENTERIMAGE, 0, 0, 1, 1, newThis->m_hwnd, (HMENU)CID_MESSAGEREPLYTO,     g_hInstance, NULL);
	newThis->m_mentionName_hwnd    = CreateWindow(WC_STATIC, TEXT("UNH"),  WS_CHILD | SS_SIMPLE | SS_CENTERIMAGE, 0, 0, 1, 1, newThis->m_hwnd, (HMENU)CID_MESSAGEREPLYUSER,   g_hInstance, NULL);
	newThis->m_mentionCheck_hwnd   = CreateWindow(WC_BUTTON, mentionText,  WS_CHILD | BS_AUTOCHECKBOX,            0, 0, 1, 1, newThis->m_hwnd, (HMENU)CID_MESSAGEMENTCHECK,   g_hInstance, NULL);
	newThis->m_mentionCancel_hwnd  = CreateWindow(WC_BUTTON, TEXT(""),     WS_CHILD | BS_PUSHBUTTON | BS_ICON,    0, 0, 1, 1, newThis->m_hwnd, (HMENU)CID_MESSAGEREPLYCANCEL, g_hInstance, NULL);
	newThis->m_mentionJump_hwnd    = CreateWindow(WC_BUTTON, TEXT(""),     WS_CHILD | BS_PUSHBUTTON | BS_ICON,    0, 0, 1, 1, newThis->m_hwnd, (HMENU)CID_MESSAGEREPLYJUMP,   g_hInstance, NULL);

	// Add icon buttons
	newThis->m_btnUpload_hwnd = CreateWindow(WC_BUTTON, TEXT(""), WS_CHILD | BS_PUSHBUTTON | BS_ICON, 0, 0, 1, 1, newThis->m_hwnd, (HMENU)CID_MESSAGEUPLOAD, g_hInstance, NULL);

	// Pull some initial measurements.
	HDC hdc = GetDC(newThis->m_hwnd);
	HGDIOBJ objOld = SelectObject(hdc, g_SendButtonFont);

	RECT rcMeasure{};
	DrawText(hdc, mentionText, -1, &rcMeasure, DT_CALCRECT);
	newThis->m_mentionTextWidth = rcMeasure.right - rcMeasure.left + 1;

	SetRectEmpty(&rcMeasure);
	DrawText(hdc, replyingText, -1, &rcMeasure, DT_CALCRECT);
	newThis->m_replyToTextWidth = rcMeasure.right - rcMeasure.left + 1;

	SelectObject(hdc, objOld);
	ReleaseDC(newThis->m_hwnd, hdc);

	// note : Windows XP only
#ifdef NEW_WINDOWS
	// only works on single line text for some reason?
	//SendMessage(newThis->m_hwnd, EM_SETCUEBANNER, TRUE, (LPARAM)L"Type a message...");
#endif

	// subclass the edit control
	m_editWndProc = (WNDPROC) SetWindowLongPtr(newThis->m_edit_hwnd, GWLP_WNDPROC, (LONG_PTR)&MessageEditor::EditWndProc);

	SendMessage(newThis->m_edit_hwnd, WM_SETFONT, (WPARAM)g_SendButtonFont, TRUE);
	SendMessage(newThis->m_send_hwnd, WM_SETFONT, (WPARAM)g_SendButtonFont, TRUE);

	SendMessage(newThis->m_send_hwnd,          BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)g_SendIcon);
	SendMessage(newThis->m_btnUpload_hwnd,     BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)g_UploadIcon);
	SendMessage(newThis->m_mentionJump_hwnd,   BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)g_JumpIcon);
	SendMessage(newThis->m_mentionCancel_hwnd, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)g_CancelIcon);

	SendMessage(newThis->m_mentionText_hwnd,    WM_SETFONT, (WPARAM)g_SendButtonFont, TRUE);
	SendMessage(newThis->m_editingMessage_hwnd, WM_SETFONT, (WPARAM)g_SendButtonFont, TRUE);
	SendMessage(newThis->m_mentionName_hwnd,    WM_SETFONT, (WPARAM)g_AuthorTextFont, TRUE);
	SendMessage(newThis->m_mentionCheck_hwnd,   WM_SETFONT, (WPARAM)g_SendButtonFont, TRUE);

	newThis->m_autoComplete.SetEdit(newThis->m_edit_hwnd);
	newThis->m_autoComplete.SetFont(g_SendButtonFont);
	newThis->m_autoComplete.SetLookup(&_AutoCompleteLookup);
	newThis->m_autoComplete.SetLookupContext(newThis);

	return newThis;
}
