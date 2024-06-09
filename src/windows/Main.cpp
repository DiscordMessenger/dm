#include "Config.hpp"
#include "Main.hpp"
#include "AboutDialog.hpp"
#include "OptionsDialog.hpp"
#include "MessageList.hpp"
#include "ChannelView.hpp"
#include "ProfileView.hpp"
#include "MessageEditor.hpp"
#include "GuildHeader.hpp"
#include "GuildLister.hpp"
#include "TextToSpeech.hpp"
#include "NetworkerThread.hpp"
#include "ImageLoader.hpp"
#include "ProfilePopout.hpp"
#include "ImageViewer.hpp"
#include "PinList.hpp"
#include "MemberList.hpp"
#include "LogonDialog.hpp"
#include "QRCodeDialog.hpp"
#include "LoadingMessage.hpp"
#include "UploadDialog.hpp"
#include "StatusBar.hpp"
#include "QuickSwitcher.hpp"
#include "Frontend_Win32.hpp"
#include "ProgressDialog.hpp"
#include "AutoComplete.hpp"
#include "../discord/LocalSettings.hpp"
#include "../discord/WebsocketClient.hpp"
#include "../discord/UpdateChecker.hpp"

#include <system_error>

// proportions:
int g_ProfilePictureSize;
int g_ChannelViewListWidth;
int g_BottomBarHeight;
int g_BottomInputHeight;
int g_SendButtonWidth;
int g_SendButtonHeight;
int g_GuildHeaderHeight;
int g_GuildListerWidth;
int g_MemberListWidth;
int g_MessageEditorHeight;
int g_GuildListerWidth2;
int g_ChannelViewListWidth2;

StatusBar  * g_pStatusBar;
MessageList* g_pMessageList;
ChannelView* g_pChannelView;
ProfileView* g_pProfileView;
GuildHeader* g_pGuildHeader;
GuildLister* g_pGuildLister;
MemberList * g_pMemberList;
MessageEditor* g_pMessageEditor;
LoadingMessage* g_pLoadingMessage;

int GetProfilePictureSize()
{
	return g_ProfilePictureSize;
}

void FindBasePath()
{
	TCHAR pwStr[MAX_PATH];
	pwStr[0] = 0;
	LPCTSTR p1 = NULL, p2 = NULL;
	
	if (SUCCEEDED(ri::SHGetFolderPath(g_Hwnd, CSIDL_APPDATA, NULL, 0, pwStr)))
	{
		SetBasePath(MakeStringFromTString(pwStr));
	}
	else
	{
		MessageBox(g_Hwnd, TmGetTString(IDS_NO_APPDATA), TmGetTString(IDS_NON_CRITICAL_ERROR), MB_OK | MB_ICONERROR);
		SetBasePath(".");
	}
}

void SetupCachePathIfNeeded()
{
	FindBasePath();

	LPCTSTR p1 = NULL, p2 = NULL;
	p1 = ConvertCppStringToTString(GetBasePath());
	p2 = ConvertCppStringToTString(GetCachePath());

	// if these already exist, it's fine..
	if (!CreateDirectory(p1, NULL))
	{
		if (GetLastError() != ERROR_ALREADY_EXISTS)
			goto _failure;
	}
	if (!CreateDirectory(p2, NULL))
	{
		if (GetLastError() != ERROR_ALREADY_EXISTS)
			goto _failure;
	}

	if (p1) free((void*)p1);
	if (p2) free((void*)p2);

	return;
_failure:
	MessageBox(g_Hwnd, TmGetTString(IDS_NO_CACHE_DIR), TmGetTString(IDS_NON_CRITICAL_ERROR), MB_OK | MB_ICONERROR);
	SetBasePath(".");

	if (p1) free((void*)p1);
	if (p2) free((void*)p2);
}

DiscordInstance* g_pDiscordInstance;

DiscordInstance* GetDiscordInstance()
{
	return g_pDiscordInstance;
}

std::string GetDiscordToken()
{
	return g_pDiscordInstance->GetToken();
}

HINSTANCE g_hInstance;
WNDCLASS g_MainWindowClass;
HBRUSH   g_backgroundBrush;
HWND     g_Hwnd;
HICON    g_Icon;
HICON    g_BotIcon;
HICON    g_SendIcon;
HICON    g_JumpIcon;
HICON    g_CancelIcon;
HICON    g_UploadIcon;
HICON    g_DownloadIcon;
HICON    g_ProfileBorderIcon;
HICON    g_ProfileBorderIconGold;
HFONT
	g_MessageTextFont,
	g_AuthorTextFont,
	g_AuthorTextUnderlinedFont,
	g_DateTextFont,
	g_GuildCaptionFont,
	g_GuildSubtitleFont,
	g_GuildInfoFont,
	g_AccountInfoFont,
	g_AccountTagFont,
	g_SendButtonFont,
	g_ReplyTextFont,
	g_ReplyTextBoldFont,
	g_TypingRegFont,
	g_TypingBoldFont;

HBITMAP g_DefaultProfilePicture;
UINT_PTR g_HeartbeatTimer;
int g_HeartbeatTimerInterval;

HBITMAP GetDefaultBitmap() {
	return g_DefaultProfilePicture;
}

void WantQuit()
{
	GetHTTPClient()->PrepareQuit();
}

void OnUpdateAvatar(const std::string& resid)
{
	// depending on where this is, update said control
	ImagePlace ip = GetAvatarCache()->GetPlace(resid);

	switch (ip.type)
	{
		case eImagePlace::AVATARS:
		{
			if (GetDiscordInstance()->GetUserID() == ip.sf)
				SendMessage(g_Hwnd, WM_REPAINTPROFILE, 0, 0);

			if (ProfilePopout::GetUser() == ip.sf)
				SendMessage(g_Hwnd, WM_UPDATEPROFILEPOPOUT, 0, 0);

			Snowflake sf = ip.sf;
			SendMessage(g_Hwnd, WM_UPDATEUSER, 0, (LPARAM) &sf);
			break;
		}
		case eImagePlace::ICONS:
		{
			SendMessage(g_Hwnd, WM_REPAINTGUILDLIST, 0, (LPARAM)&ip.sf);
			break;
		}
		case eImagePlace::ATTACHMENTS:
		{
			SendMessage(g_Hwnd, WM_UPDATEATTACHMENT, 0, (LPARAM) &ip);
			break;
		}
		case eImagePlace::EMOJIS:
		{
			SendMessage(g_Hwnd, WM_UPDATEEMOJI, 0, (LPARAM) &ip.sf);
			break;
		}
	}
}

bool g_bMemberListVisible = false;
bool g_bChannelListVisible = false;

void ProperlySizeControls(HWND hWnd)
{
	RECT rect = {}, rect2 = {}, rcClient = {}, rcSBar = {};
	GetClientRect(hWnd, &rect);
	int clientWidth = rect.right - rect.left;
	rcClient = rect;

	int scaled10 = ScaleByDPI(10);

	rect.left   += scaled10;
	rect.top    += scaled10;
	rect.right  -= scaled10;
	rect.bottom -= scaled10;

	HWND hWndMsg = g_pMessageList->m_hwnd;
	HWND hWndChv = g_pChannelView->m_hwnd;
	HWND hWndPfv = g_pProfileView->m_hwnd;
	HWND hWndGuh = g_pGuildHeader->m_hwnd;
	HWND hWndGul = g_pGuildLister->m_hwnd;
	HWND hWndMel = g_pMemberList->m_mainHwnd;
	HWND hWndTin = g_pMessageEditor->m_hwnd;
	HWND hWndStb = g_pStatusBar->m_hwnd;

	int statusBarHeight = 0;
	GetChildRect(hWnd, hWndStb, &rcSBar);
	statusBarHeight = rcSBar.bottom - rcSBar.top;

	rect.bottom -= statusBarHeight;
	rect2 = rect;

	g_ProfilePictureSize   = ScaleByDPI(PROFILE_PICTURE_SIZE_DEF);
	g_ChannelViewListWidth = ScaleByDPI(CHANNEL_VIEW_LIST_WIDTH);
	g_BottomBarHeight      = ScaleByDPI(BOTTOM_BAR_HEIGHT);
	g_BottomInputHeight    = ScaleByDPI(BOTTOM_INPUT_HEIGHT);
	g_SendButtonWidth      = ScaleByDPI(SEND_BUTTON_WIDTH);
	g_SendButtonHeight     = ScaleByDPI(SEND_BUTTON_HEIGHT);
	g_GuildHeaderHeight    = ScaleByDPI(GUILD_HEADER_HEIGHT);
	g_GuildListerWidth     = ScaleByDPI(PROFILE_PICTURE_SIZE_DEF + 12) + GUILD_LISTER_BORDER_SIZE * 4;
	g_MemberListWidth      = ScaleByDPI(MEMBER_LIST_WIDTH);
	g_MessageEditorHeight  = ScaleByDPI(MESSAGE_EDITOR_HEIGHT);

	int guildListerWidth = g_GuildListerWidth;
	int channelViewListWidth = g_ChannelViewListWidth;
	if (GetLocalSettings()->ShowScrollBarOnGuildList()) {
		int offset = GetSystemMetrics(SM_CXVSCROLL);
		guildListerWidth += offset;
		channelViewListWidth -= offset;
	}

	bool bRepaintGuildHeader = g_GuildListerWidth2 != guildListerWidth;
	g_GuildListerWidth2 = guildListerWidth;
	g_ChannelViewListWidth2 = channelViewListWidth;

	// May need to do some adjustments.
	bool bFullRepaintProfileView = false;
	const int minFullWidth = ScaleByDPI(800);
	if (clientWidth < minFullWidth)
	{
		bFullRepaintProfileView = true;

		int widthOfAll3Things =
			clientWidth
			- scaled10 * 3 /* Left edge, Right edge, Between GuildLister and the group */
			- scaled10 * 2 /* Between channelview and messageview, between messageview and memberlist */
			- g_GuildListerWidth2; /* The guild list itself. So just the channelview, messageview and memberlist summed */

		const int widthOfAll3ThingsAt800px = 694;

		g_ChannelViewListWidth2 = MulDiv(g_ChannelViewListWidth2, widthOfAll3Things, widthOfAll3ThingsAt800px);
		g_MemberListWidth       = MulDiv(g_MemberListWidth,       widthOfAll3Things, widthOfAll3ThingsAt800px);
		channelViewListWidth = g_ChannelViewListWidth2;
	}

	bool bRepaint = true;

	// Create a message list
	rect.left += guildListerWidth + scaled10;
	rect.bottom -= g_MessageEditorHeight + g_pMessageEditor->ExpandedBy() + scaled10;
	rect.top += g_GuildHeaderHeight;
	if (g_bChannelListVisible) rect.left += channelViewListWidth + scaled10;
	if (g_bMemberListVisible) rect.right -= g_MemberListWidth + scaled10;
	MoveWindow(hWndMsg, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, bRepaint);

	rect.left = rect.right + scaled10;
	if (!g_bMemberListVisible) rect.left -= g_MemberListWidth;
	rect.right = rect.left + g_MemberListWidth;
	rect.bottom = rect2.bottom;
	MoveWindow(hWndMel, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, bRepaint);
	rect = rect2;

	rect.left += guildListerWidth + scaled10;
	rect.right = rect.left + channelViewListWidth;
	rect.bottom -= g_BottomBarHeight;
	rect.top += g_GuildHeaderHeight;
	MoveWindow(hWndChv, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, bRepaint);
	rect = rect2;

	rect.left += guildListerWidth + scaled10;
	rect.top = rect.bottom - g_BottomBarHeight + scaled10;
	rect.right = rect.left + channelViewListWidth;
	MoveWindow(hWndPfv, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, TRUE);
	rect = rect2;

	rect.left   = scaled10 + guildListerWidth + scaled10;
	rect.top    = rect.bottom - g_MessageEditorHeight - g_pMessageEditor->ExpandedBy();
	if (g_bChannelListVisible) rect.left += channelViewListWidth + scaled10;
	if (g_bMemberListVisible) rect.right -= g_MemberListWidth + scaled10;
	int textInputHeight = rect.bottom - rect.top, textInputWidth = rect.right - rect.left;
	MoveWindow(hWndTin, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, bRepaint);
	rect = rect2;

	rect.left += guildListerWidth + scaled10;
	rect.bottom = rect.top + g_GuildHeaderHeight - scaled10;
	MoveWindow(hWndGuh, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, bRepaintGuildHeader);
	rect = rect2;

	rect.right = rect.left + guildListerWidth;
	MoveWindow(hWndGul, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, bRepaint);
	rect = rect2;

	// Forward the resize event to the status bar.
	MoveWindow(hWndStb, 0, rcClient.bottom - statusBarHeight, rcClient.right - rcClient.left, statusBarHeight, TRUE);
	// Erase the old rectangle
	InvalidateRect(hWnd, &rcSBar, TRUE);

	if (bFullRepaintProfileView)
		InvalidateRect(hWndPfv, NULL, FALSE);

	g_pStatusBar->UpdateParts(rcClient.right - rcClient.left);
}

bool g_bQuittingEarly = false;
UINT_PTR g_deferredRestartSessionTimer = 0;
void CALLBACK DeferredRestartSession(HWND hWnd, UINT uInt, UINT_PTR uIntPtr, DWORD dWord)
{
	KillTimer(hWnd, g_deferredRestartSessionTimer);
	g_deferredRestartSessionTimer = 0;

	if (GetDiscordInstance()->GetGatewayID() < 0)
		GetDiscordInstance()->StartGatewaySession();
}

INT_PTR CALLBACK DDialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	return 0L;
}

struct TypingInfo {
	std::map<Snowflake, TypingUser> m_typingUsers;
};

std::map<Snowflake, TypingInfo> g_typingInfo;

void OnTyping(Snowflake guildID, Snowflake channelID, Snowflake userID, time_t timeStamp)
{
	TypingInfo& ti = g_typingInfo[channelID];

	TypingUser tu;
	tu.m_startTimeMS = timeStamp * 1000LL;
	tu.m_key = userID;
	tu.m_name = GetProfileCache()->LookupProfile(userID, "", "", "", false)->GetName(guildID);
	ti.m_typingUsers[userID] = tu;

	// Send an update to the message editor
	if (channelID == GetDiscordInstance()->GetCurrentChannelID())
	{
		if (userID == GetDiscordInstance()->GetUserID()) {
			// Ignore typing start from ourselves for the lower bar
			return;
		}

		g_pStatusBar->AddTypingName(userID, timeStamp, tu.m_name);
	}
}

void OnStopTyping(Snowflake channelID, Snowflake userID)
{
	g_typingInfo[channelID].m_typingUsers[userID].m_startTimeMS = 0;

	if (channelID == GetDiscordInstance()->GetCurrentChannelID())
		g_pStatusBar->RemoveTypingName(userID);
}

void UpdateMainWindowTitle(HWND hWnd)
{
	std::string windowTitle;

	Channel* pChannel = GetDiscordInstance()->GetCurrentChannel();
	if (pChannel)
		windowTitle += pChannel->GetTypeSymbol() + pChannel->m_name + " - ";

	windowTitle += TmGetString(IDS_PROGRAM_NAME);
	
	LPTSTR tstr = ConvertCppStringToTString(windowTitle);
	SetWindowText(hWnd, tstr);
	free(tstr);
}

int OnSSLError(const std::string& url)
{
	LPTSTR urlt = ConvertCppStringToTString(url);
	static TCHAR buffer[8192];
	_tcscpy(buffer, TmGetTString(IDS_SSL_ERROR_1));
	_tcscat(buffer, urlt);
	_tcscat(buffer, TmGetTString(IDS_SSL_ERROR_2));
	_tcscat(buffer, TmGetTString(IDS_SSL_ERROR_3));
	free(urlt);

	size_t l = _tcslen(buffer);

	return MessageBox(g_Hwnd, buffer, TmGetTString(IDS_SSL_ERROR_TITLE), MB_ABORTRETRYIGNORE | MB_ICONWARNING);
}

BOOL HandleCommand(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (LOWORD(wParam))
	{
		case ID_FILE_PREFERENCES:
		{
			switch (ShowOptionsDialog())
			{
				case OPTIONS_RESULT_LOGOUT: {
					PostMessage(hWnd, WM_LOGGEDOUT, 100, 0);
					GetDiscordInstance()->SetToken("");
					g_pChannelView->ClearChannels();
					g_pMemberList->ClearMembers();
					g_pStatusBar->ClearTypers();
					g_pMessageList->ClearMessages();
					g_pGuildLister->ClearTooltips();
					return TRUE;
				}
			}
			break;
		}
		case ID_FILE_EXIT:
		{
			SendMessage(hWnd, WM_CLOSE, 0, 0);
			return 0;
		}
		case ID_HELP_ABOUTDISCORDMESSENGER:
		{
			About();
			break;
		}
		case ID_FILE_STOPALLSPEECH:
		{
			TextToSpeech::StopAll();
			break;
		}
		case ID_ONLINESTATUSPLACEHOLDER_ONLINE:
		{
			GetDiscordInstance()->SetActivityStatus(eActiveStatus::STATUS_ONLINE);
			break;
		}
		case ID_ONLINESTATUSPLACEHOLDER_IDLE:
		{
			GetDiscordInstance()->SetActivityStatus(eActiveStatus::STATUS_IDLE);
			break;
		}
		case ID_ONLINESTATUSPLACEHOLDER_DONOTDISTURB:
		{
			GetDiscordInstance()->SetActivityStatus(eActiveStatus::STATUS_DND);
			break;
		}
		case ID_ONLINESTATUSPLACEHOLDER_INVISIBLE:
		{
			GetDiscordInstance()->SetActivityStatus(eActiveStatus::STATUS_OFFLINE);
			break;
		}
		case IDM_LEAVEGUILD: // Server > Leave Server
		{
			g_pGuildLister->AskLeave(GetDiscordInstance()->GetCurrentGuildID());
			break;
		}
		// Accelerators
		case IDA_SEARCH:
		case ID_ACTIONS_SEARCH:
		{
			// TODO
			//GetWebsocketClient()->Close(GetDiscordInstance()->GetGatewayID(), websocketpp::close::status::normal);
			DbgPrintW("Search!");
			break;
		}
		case IDA_QUICKSWITCHER:
		case ID_ACTIONS_QUICKSWITCHER:
		{
			QuickSwitcher::ShowDialog();
			break;
		}
		case IDA_PASTE:
		{
			if (!OpenClipboard(hWnd)) {
				DbgPrintW("Error opening clipboard: %d", GetLastError());
				break;
			}

			HBITMAP hbm = NULL;
			HDC hdc = NULL;
			BYTE* pBytes = nullptr;
			LPCTSTR fileName = nullptr;
			std::vector<uint8_t> data;
			bool isClipboardClosed = false;
			bool hadToUseLegacyBitmap = false;
			int width = 0, height = 0, stride = 0, bpp = 0;
			Channel* pChan = nullptr;

			HANDLE hbmi = GetClipboardData(CF_DIB);
			if (hbmi)
			{
				DbgPrintW("Using CF_DIB to fetch image from clipboard");
				PBITMAPINFO bmi = (PBITMAPINFO) GlobalLock(hbmi);

				// WHAT THE HELL: Windows seems to add 12 extra bytes after the header
				// representing the colors: [RED] [GREEN] [BLUE]. I don't know why we
				// have to do this or how to un-hardcode it.  Yes, in this case, the
				// biClrUsed member is zero.
				// Does it happen to include only part of the BITMAPV4HEADER???
				const int HACK_OFFSET = 12;

				width = bmi->bmiHeader.biWidth;
				height = bmi->bmiHeader.biHeight;
				bpp = bmi->bmiHeader.biBitCount;
				stride = ((((width * bpp) + 31) & ~31) >> 3);

				int sz = stride * height;
				pBytes = new BYTE[sz];
				memcpy(pBytes, (char*) bmi + bmi->bmiHeader.biSize + HACK_OFFSET, sz);

				GlobalUnlock(hbmi);
			}
			else
			{
				// try legacy CF_BITMAP
				DbgPrintW("Using legacy CF_BITMAP");
				HBITMAP hbm = (HBITMAP) GetClipboardData(CF_BITMAP);
				hadToUseLegacyBitmap = true;

				if (!hbm)
				{
					CloseClipboard();

					// No bitmap, forward to the edit control if selected
					HWND hFocus = GetFocus();
					if (hFocus == g_pMessageEditor->m_edit_hwnd)
						SendMessage(hFocus, WM_PASTE, 0, 0);

					return FALSE;
				}

				HDC hdc = GetDC(hWnd);
				bool res = GetDataFromBitmap(hdc, hbm, pBytes, width, height, bpp);
				ReleaseDC(hWnd, hdc);

				if (!res)
					goto _fail;

				stride = ((((width * bpp) + 31) & ~31) >> 3);
			}

			pChan = GetDiscordInstance()->GetCurrentChannel();
			if (!pChan) {
				DbgPrintW("No channel!");
				goto _fail;
			}

			if (!pChan->HasPermission(PERM_SEND_MESSAGES) ||
				!pChan->HasPermission(PERM_ATTACH_FILES)) {
				DbgPrintW("Can't attach files here!");
				goto _fail;
			}

			// TODO: Don't always force alpha when pasting from CF_DIB or CF_BITMAP.
			// I know this sucks, but I've tried a lot of things...

			// have to force alpha always
			// have to flip vertically if !hadToUseLegacyBitmap
			if (!ImageLoader::ConvertToPNG(&data, pBytes, width, height, stride, 32, true, !hadToUseLegacyBitmap)) {
				DbgPrintW("Cannot convert to PNG!");
				goto _fail;
			}

			CloseClipboard(); isClipboardClosed = true;

			fileName = TmGetTString(IDS_UNKNOWN_FILE_NAME);
			UploadDialogShowWithFileData(data.data(), data.size(), fileName);

		_fail:
			data.clear();
			if (pBytes) delete[] pBytes;
			if (hdc) ReleaseDC(hWnd, hdc);
			if (hbm) DeleteObject(hbm); // should I do this?
			if (!isClipboardClosed) CloseClipboard();
			break;
		}
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

int g_tryAgainTimerElapse = 100;
UINT_PTR g_tryAgainTimer = 0;
const UINT_PTR g_tryAgainTimerId = 123456;

void CALLBACK TryAgainTimer(HWND hWnd, UINT uMsg, UINT_PTR uTimerID, DWORD dwParam) {
	if (uTimerID != g_tryAgainTimerId)
		return;

	KillTimer(hWnd, g_tryAgainTimer);
	g_tryAgainTimer = 0;
	GetDiscordInstance()->StartGatewaySession();
}

void TryConnectAgainIn(int time) {
	g_tryAgainTimer = SetTimer(g_Hwnd, g_tryAgainTimerId, time, TryAgainTimer);
}

void ResetTryAgainInTime() {
	g_tryAgainTimerElapse = 500;
}

int g_agerCounter = 0;

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	g_agerCounter++;
	if (g_agerCounter >= 50) {
		g_agerCounter = 0;
		GetAvatarCache()->AgeBitmaps();
	}

	switch (uMsg)
	{
		case WM_UPDATEEMOJI:
		{
			Snowflake sf = *(Snowflake*) lParam;
			g_pMessageList->OnUpdateEmoji(sf);
			PinList::OnUpdateEmoji(sf);
			break;
		}
		case WM_UPDATEUSER:
		{
			Snowflake sf = *(Snowflake*) lParam;
			g_pMessageList->OnUpdateAvatar(sf);
			g_pMemberList->OnUpdateAvatar(sf);
			g_pChannelView->OnUpdateAvatar(sf);
			PinList::OnUpdateAvatar(sf);

			if (ProfilePopout::GetUser() == sf)
				ProfilePopout::Update();
			break;
		}
		case WM_SSLERROR:
		{
			return OnSSLError(CutOutURLPath(std::string((const char*)lParam)));
		}
		case WM_FORCERESTART:
		{
			MessageBox(hWnd, TmGetTString(IDS_RESTART_CONFIG_CHANGE), TmGetTString(IDS_PROGRAM_NAME), MB_OK | MB_ICONINFORMATION);
			if (InSendMessage())
				ReplyMessage(0);
			SendMessage(hWnd, WM_DESTROY, 0, 0);
			return 0;
		}
		case WM_UPDATEMESSAGELENGTH:
		{
			g_pStatusBar->UpdateCharacterCounter(int(lParam), MAX_MESSAGE_SIZE);
			break;
		}
		case WM_UPDATEPROFILEAVATAR:
		{
			UpdateProfileParams parms = *(UpdateProfileParams*) lParam;
			GetAvatarCache()->EraseBitmap(parms.m_resId);
			break;
		}
		case WM_UPDATEATTACHMENT:
		{
			ImagePlace pl = *(ImagePlace*)lParam;
			g_pMessageList->OnUpdateEmbed(pl.key);
			PinList::OnUpdateEmbed(pl.key);
			break;
		}
		case WM_REPAINTPROFILE:
		{
			g_pProfileView->Update();
			break;
		}
		case WM_UPDATEPROFILEPOPOUT:
		{
			ProfilePopout::Update();
			break;
		}
		case WM_REPAINTGUILDLIST:
		{
			g_pGuildLister->Update();
			break;
		}
		case WM_REFRESHMESSAGES:
		{
			Snowflake sf = *(Snowflake*)lParam;
			g_pMessageList->RefetchMessages(sf, true);
			break;
		}
		case WM_REPAINTMSGLIST:
		{
			InvalidateRect(g_pMessageList->m_hwnd, NULL, false);
			break;
		}
		case WM_MSGLISTUPDATEMODE:
		{
			g_pMessageList->UpdateBackgroundBrush();
			InvalidateRect(g_pMessageList->m_hwnd, NULL, false);
			break;
		}
		case WM_RECALCMSGLIST:
		{
			g_pMessageList->FullRecalcAndRepaint();
			break;
		}
		case WM_SHOWUPLOADDIALOG:
		{
			UploadDialogShow2();
			break;
		}
		case WM_UPDATESELECTEDCHANNEL:
		{
			g_pMessageEditor->StopReply();
			g_pMessageEditor->Layout();

			Snowflake guildID = GetDiscordInstance()->GetCurrentGuildID();
			Snowflake channID = GetDiscordInstance()->GetCurrentChannelID();

			g_pChannelView->OnUpdateSelectedChannel(channID);

			// repaint the message view
			g_pMessageList->ClearMessages();
			g_pMessageList->SetGuild(guildID);
			g_pMessageList->SetChannel(channID);

			g_pMessageList->UpdateAllowDrop();

			UpdateMainWindowTitle(hWnd);
			SetFocus(g_pMessageEditor->m_edit_hwnd);

			if (!GetDiscordInstance()->GetCurrentChannel())
			{
				InvalidateRect(g_pMessageList->m_hwnd, NULL, TRUE);
				InvalidateRect(g_pGuildHeader->m_hwnd, NULL, FALSE);
				break;
			}

			GetDiscordInstance()->HandledChannelSwitch();

			g_pMessageList->RefetchMessages();

			InvalidateRect(g_pMessageList->m_hwnd, NULL, MessageList::MayErase());
			g_pGuildHeader->Update();
			g_pMessageEditor->UpdateTextBox();

			// Update the message editor's typing indicators
			g_pStatusBar->ClearTypers();

			Snowflake myUserID = GetDiscordInstance()->GetUserID();
			TypingInfo& ti = g_typingInfo[channID];
			for (auto& tu : ti.m_typingUsers) {
				if (tu.second.m_key == myUserID)
					continue;
				
				g_pStatusBar->AddTypingName(tu.second.m_key, tu.second.m_startTimeMS / 1000ULL, tu.second.m_name);
			}

			break;
		}
		case WM_UPDATEMEMBERLIST:
		{
			g_pMemberList->SetGuild(GetDiscordInstance()->GetCurrentGuild()->m_snowflake);
			g_pMemberList->Update();
			break;
		}
		case WM_TOGGLEMEMBERS:
		{
			g_bMemberListVisible ^= 1;
			int off = ScaleByDPI(10) + g_MemberListWidth;

			if (g_bMemberListVisible) {
				ShowWindow(g_pMemberList->m_mainHwnd, SW_SHOWNORMAL);
				UpdateWindow(g_pMemberList->m_mainHwnd);
				ResizeWindow(g_pMessageList->m_hwnd, -off, 0, false, false, true);
				ResizeWindow(g_pMessageEditor->m_hwnd, -off, 0, false, false, true);
			}
			else {
				ShowWindow(g_pMemberList->m_mainHwnd, SW_HIDE);
				UpdateWindow(g_pMemberList->m_mainHwnd);
				ResizeWindow(g_pMessageList->m_hwnd, off, 0, false, false, true);
				ResizeWindow(g_pMessageEditor->m_hwnd, off, 0, false, false, true);
			}

			break;
		}
		case WM_TOGGLECHANNELS:
		{
			g_bChannelListVisible ^= 1;
			int off = ScaleByDPI(10) + g_ChannelViewListWidth2;

			if (g_bChannelListVisible) {
				ShowWindow(g_pChannelView->m_hwnd, SW_SHOWNORMAL);
				ShowWindow(g_pProfileView->m_hwnd, SW_SHOWNORMAL);
				UpdateWindow(g_pChannelView->m_hwnd);
				UpdateWindow(g_pProfileView->m_hwnd);
				ResizeWindow(g_pMessageList->m_hwnd, off, 0, true, false, true);
				ResizeWindow(g_pMessageEditor->m_hwnd, off, 0, true, false, true);
			}
			else {
				ShowWindow(g_pChannelView->m_hwnd, SW_HIDE);
				ShowWindow(g_pProfileView->m_hwnd, SW_HIDE);
				UpdateWindow(g_pChannelView->m_hwnd);
				UpdateWindow(g_pProfileView->m_hwnd);
				ResizeWindow(g_pMessageList->m_hwnd, -off, 0, true, false, true);
				ResizeWindow(g_pMessageEditor->m_hwnd, -off, 0, true, false, true);
			}

			break;
		}
		case WM_UPDATESELECTEDGUILD:
		{
			// repaint the guild lister
			g_pGuildLister->UpdateSelected();
			g_pGuildHeader->Update();

			SendMessage(hWnd, WM_UPDATECHANLIST, 0, 0);
			SendMessage(hWnd, WM_UPDATEMEMBERLIST, 0, 0);
			break;
		}
		case WM_UPDATECHANACKS:
		{
			Snowflake* sfs = (Snowflake*)lParam;

			Snowflake channelID = sfs[0];
			Snowflake messageID = sfs[1];
			auto inst = GetDiscordInstance();

			Channel* pChan = GetDiscordInstance()->GetChannelGlobally(channelID);

			// Update the icon for the specific guild
			g_pGuildLister->RedrawIconForGuild(pChan->m_parentGuild);

			// Get the channel as is in the current guild; if found,
			// update the channel view's ack status
			Guild* pGuild = inst->GetGuild(inst->m_CurrentGuild);
			if (!pGuild) break;

			pChan = pGuild->GetChannel(channelID);
			if (!pChan)
				break;

			g_pChannelView->UpdateAcknowledgement(channelID);
			if (g_pMessageList->GetCurrentChannel() == channelID)
				g_pMessageList->SetLastViewedMessage(messageID, true);
			break;
		}
		case WM_UPDATECHANLIST:
		{
			// repaint the channel view
			g_pChannelView->ClearChannels();

			// get the current guild
			auto inst = GetDiscordInstance();
			Guild* pGuild = inst->GetGuild(inst->m_CurrentGuild);
			if (!pGuild) break;

			g_pChannelView->SetMode(pGuild->m_snowflake == 0);

			// if the channels haven't been fetched yet
			if (!pGuild->m_bChannelsLoaded)
			{
				pGuild->RequestFetchChannels();
			}
			else
			{
				for (auto& ch : pGuild->m_channels)
					g_pChannelView->AddChannel(ch);
				for (auto& ch : pGuild->m_channels)
					g_pChannelView->RemoveCategoryIfNeeded(ch);
			}

			break;
		}
		// wParam = request code, lParam = Request*
		case WM_REQUESTDONE:
		{
			NetRequest cloneReq = * (NetRequest*) lParam;

			// Round-trip time should probably be low to avoid stalling.
			if (InSendMessage())
				ReplyMessage(0);

			GetDiscordInstance()->HandleRequest(&cloneReq);
			break;
		}
		case WM_ADDMESSAGE:
		{
			AddMessageParams* pParms = (AddMessageParams*)lParam;

			GetMessageCache()->AddMessage(pParms->channel, pParms->msg);

			if (g_pMessageList->GetCurrentChannel() == pParms->channel)
			{
				g_pMessageList->AddMessage(pParms->msg, GetForegroundWindow() == hWnd);
				OnStopTyping(pParms->channel, pParms->msg.m_author_snowflake);
			}

			Channel* pChan = GetDiscordInstance()->GetChannel(pParms->channel);
			if (!pChan)
				break;

			if (pChan->IsDM()) {
				if (GetDiscordInstance()->ResortChannels(pChan->m_parentGuild))
					SendMessage(g_Hwnd, WM_UPDATECHANLIST, 0, 0);
			}

			break;
		}
		case WM_UPDATEMESSAGE:
		{
			AddMessageParams* pParms = (AddMessageParams*)lParam;

			GetMessageCache()->EditMessage(pParms->channel, pParms->msg);

			if (g_pMessageList->GetCurrentChannel() == pParms->channel)
				g_pMessageList->EditMessage(pParms->msg);

			break;
		}
		case WM_DELETEMESSAGE:
		{
			Snowflake sf = *(Snowflake*)lParam;
			g_pMessageList->DeleteMessage(sf);
			
			if (sf == g_pMessageEditor->ReplyingTo())
				g_pMessageEditor->StopReply();

			break;
		}
		case WM_WEBSOCKETMESSAGE:
		{
			if (InSendMessage())
				ReplyMessage(0);

			WebsocketMessageParams* pParm = (WebsocketMessageParams*) lParam;
			DbgPrintW("RECEIVED MESSAGE: %s\n", pParm->m_payload.c_str());

			if (GetDiscordInstance()->GetGatewayID() == pParm->m_gatewayId)
				GetDiscordInstance()->HandleGatewayMessage(pParm->m_payload);

			if (GetQRCodeDialog()->GetGatewayID() == pParm->m_gatewayId)
				GetQRCodeDialog()->HandleGatewayMessage(pParm->m_payload);

			break;
		}
		case WM_REFRESHMEMBERS:
		{
			auto* memsToUpdate = (std::set<Snowflake>*)lParam;

			g_pMessageList->UpdateMembers(*memsToUpdate);
			g_pMemberList ->UpdateMembers(*memsToUpdate);
			g_pMessageEditor->OnLoadedMemberChunk();

			break;
		}
		case WM_CREATE:
		{
			g_bMemberListVisible = true;
			g_bChannelListVisible = true;
			ForgetSystemDPI();
			g_ProfilePictureSize = ScaleByDPI(PROFILE_PICTURE_SIZE_DEF);

			try {
				GetWebsocketClient()->Init();
			}
			catch (...) {
				MessageBox(hWnd, TmGetTString(IDS_CANNOT_INIT_WS), TmGetTString(IDS_PROGRAM_NAME), MB_ICONERROR | MB_OK);
				g_bQuittingEarly = true;
				break;
			}

			if (GetLocalSettings()->IsFirstStart())
			{
				MessageBox(
					hWnd,
					TmGetTString(IDS_WELCOME_MSG),
					TmGetTString(IDS_PROGRAM_NAME),
					MB_ICONINFORMATION | MB_OK
				);

				if (!LogonDialogShow())
				{
					g_bQuittingEarly = true;
					break;
				}
			}

			if (GetLocalSettings()->AskToCheckUpdates())
			{
				bool check = MessageBox(
					hWnd,
					TmGetTString(IDS_CHECK_UPDATES),
					TmGetTString(IDS_PROGRAM_NAME),
					MB_ICONQUESTION | MB_YESNO
				) == IDYES;

				GetLocalSettings()->SetCheckUpdates(check);
			}

			if (GetLocalSettings()->CheckUpdates())
			{
				UpdateChecker::StartCheckingForUpdates();
			}

			if (g_SendIcon)     DeleteObject(g_SendIcon);
			if (g_JumpIcon)     DeleteObject(g_JumpIcon);
			if (g_CancelIcon)   DeleteObject(g_CancelIcon);
			if (g_UploadIcon)   DeleteObject(g_UploadIcon);
			if (g_DownloadIcon) DeleteObject(g_DownloadIcon);

			g_pDiscordInstance = new DiscordInstance(GetLocalSettings()->GetToken());
			g_BotIcon      = (HICON) LoadImage(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_BOT)),      IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
			g_SendIcon     = (HICON) LoadImage(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_SEND)),     IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
			g_JumpIcon     = (HICON) LoadImage(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_JUMP)),     IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
			g_CancelIcon   = (HICON) LoadImage(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_CANCEL)),   IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
			g_UploadIcon   = (HICON) LoadImage(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_UPLOAD)),   IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
			g_DownloadIcon = (HICON) LoadImage(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_DOWNLOAD)), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);

			MessageList::InitializeClass();
			ChannelView::InitializeClass();
			ProfileView::InitializeClass();
			GuildHeader::InitializeClass();
			GuildLister::InitializeClass();
			MemberList ::InitializeClass();
			AutoComplete::InitializeClass();
			MessageEditor::InitializeClass();
			LoadingMessage::InitializeClass();

			RECT rect = {}, rect2 = {};
			GetClientRect(hWnd, &rect);

			rect2 = rect;
			rect.left   += 10;
			rect.top    += 10;
			rect.right  -= 10;
			rect.bottom -= 10;

			RECT rcLoading = { 0, 0, 300, 200 };
			POINT pt{ 0, 0 };
			OffsetRect(&rcLoading, rect2.right / 2 - rcLoading.right / 2, 0);// rect2.bottom / 2 - rcLoading.bottom / 2);
			ClientToScreen(hWnd, &pt);
			OffsetRect(&rcLoading, pt.x, pt.y);

			g_pStatusBar   = StatusBar::Create(hWnd);
			g_pMessageList = MessageList::Create(hWnd, &rect);
			g_pChannelView = ChannelView::Create(hWnd, &rect);
			g_pProfileView = ProfileView::Create(hWnd, &rect, CID_PROFILEVIEW, true);
			g_pGuildHeader = GuildHeader::Create(hWnd, &rect);
			g_pGuildLister = GuildLister::Create(hWnd, &rect);
			g_pMemberList  = MemberList ::Create(hWnd, &rect);
			g_pMessageEditor = MessageEditor::Create(hWnd, &rect);
			g_pLoadingMessage = LoadingMessage::Create(hWnd, &rcLoading);

			SendMessage(hWnd, WM_LOGINAGAIN, 0, 0);
			break;
		}
		case WM_CONNECTERROR:
			// try it again
			DbgPrintW("Trying to connect to websocket again in %d ms", g_tryAgainTimerElapse);
			TryConnectAgainIn(g_tryAgainTimerElapse);
			g_tryAgainTimerElapse = g_tryAgainTimerElapse * 115 / 100;
			if (g_tryAgainTimerElapse > 10000)
				g_tryAgainTimerElapse = 10000;
			break;
		case WM_CONNECTERROR2:
		case WM_CONNECTED:
			ResetTryAgainInTime();
			if (g_tryAgainTimer) {
				KillTimer(hWnd, g_tryAgainTimer);
				g_tryAgainTimer = 0;
			}
			g_pLoadingMessage->Hide();
			break;
		case WM_CONNECTING: {
			g_pLoadingMessage->Show();
			break;
		}
		case WM_LOGINAGAIN:
		{
			if (GetDiscordInstance()->HasGatewayURL()) {
				GetDiscordInstance()->StartGatewaySession();
			}
			else {
				GetHTTPClient()->PerformRequest(
					false,
					NetRequest::GET,
					GetDiscordAPI() + "gateway",
					DiscordRequest::GATEWAY,
					0, "", GetDiscordToken()
				);
			}

			break;
		}

		case WM_CLOSE:
			KillImageViewer();
			ProfilePopout::Dismiss();
			AutoComplete::DismissAutoCompleteWindowsIfNeeded(hWnd);
			g_pLoadingMessage->Hide();
			break;

		case WM_SIZE: {
			int width = LOWORD(lParam);
			int height = HIWORD(lParam);
			// Save the new size
			GetLocalSettings()->SetWindowSize(UnscaleByDPI(width), UnscaleByDPI(height));

			ProfilePopout::Dismiss();
			ProperlySizeControls(hWnd);
			break;
		}
		case WM_REPOSITIONEVERYTHING:
		{
			ProperlySizeControls(hWnd);
			break;
		}
		case WM_MOVE: {
			ProfilePopout::Dismiss();
			AutoComplete::DismissAutoCompleteWindowsIfNeeded(hWnd);
			break;
		}
		case WM_GETMINMAXINFO: {
			PMINMAXINFO pMinMaxInfo = (PMINMAXINFO) lParam;
			// these are the minimums as of now!
			pMinMaxInfo->ptMinTrackSize.x = ScaleByDPI(640);
			pMinMaxInfo->ptMinTrackSize.y = ScaleByDPI(400);
			break;
		}
		case WM_COMMAND:
			return HandleCommand(hWnd, uMsg, wParam, lParam);

		case WM_DESTROY:
		{
			if (GetDiscordInstance())
				GetDiscordInstance()->CloseGatewaySession();
			if (GetAvatarCache())
				GetAvatarCache()->WipeBitmaps();

			SAFE_DELETE(g_pChannelView);
			SAFE_DELETE(g_pGuildHeader);
			SAFE_DELETE(g_pGuildLister);
			SAFE_DELETE(g_pMessageList);
			SAFE_DELETE(g_pProfileView);

			SAFE_DELETE(g_pStatusBar);
			SAFE_DELETE(g_pMemberList);
			SAFE_DELETE(g_pMessageEditor);
			SAFE_DELETE(g_pLoadingMessage);

			PostQuitMessage(0);
			break;
		}
		case WM_DRAWITEM:
		{
			switch (wParam)
			{
				case CID_STATUSBAR:
					g_pStatusBar->DrawItem((LPDRAWITEMSTRUCT)lParam);
					break;
			}
			break;
		}
		case WM_INITMENUPOPUP:
		{
			HMENU Menu = (HMENU)wParam;
			int firstMenuItem = GetMenuItemID(Menu, 0);

			switch (firstMenuItem)
			{
				case ID_ONLINESTATUSPLACEHOLDER_ONLINE:
				{
					Profile* pf = GetDiscordInstance()->GetProfile();
					if (!pf)
						break;

					// It's the account status menu
					CheckMenuItem(Menu, ID_ONLINESTATUSPLACEHOLDER_ONLINE,       pf->m_activeStatus == STATUS_ONLINE  ? MF_CHECKED : 0);
					CheckMenuItem(Menu, ID_ONLINESTATUSPLACEHOLDER_IDLE,         pf->m_activeStatus == STATUS_IDLE    ? MF_CHECKED : 0);
					CheckMenuItem(Menu, ID_ONLINESTATUSPLACEHOLDER_DONOTDISTURB, pf->m_activeStatus == STATUS_DND     ? MF_CHECKED : 0);
					CheckMenuItem(Menu, ID_ONLINESTATUSPLACEHOLDER_INVISIBLE,    pf->m_activeStatus == STATUS_OFFLINE ? MF_CHECKED : 0);
					break;
				}
				case IDM_INVITEPEOPLE:
				{
					Profile* pf = GetDiscordInstance()->GetProfile();
					Guild* pGuild = GetDiscordInstance()->GetCurrentGuild();
					if (!pGuild || !pf)
						break;

					EnableMenuItem(Menu, IDM_LEAVEGUILD, pGuild->m_ownerId == pf->m_snowflake ? MF_GRAYED : MF_ENABLED);
					break;
				}
			}

			break;
		}
		case WM_SENDTOMESSAGE:
		{
			Snowflake sf = *((Snowflake*)lParam);
			g_pMessageList->SendToMessage(sf);
			break;
		}
		case WM_SENDMESSAGE:
		{
			SendMessageParams parms = *((SendMessageParams*)lParam);

			std::string msg;

			// This gets rid of all '\r' characters that the text edit control adds.
			std::vector<TCHAR> tch2;
			TCHAR* tcString = parms.m_rawMessage;
			for (; *tcString; tcString++) {
				if (*tcString != '\r')
					tch2.push_back(*tcString);
			}
			tch2.push_back(0);

#ifdef UNICODE
			msg = MakeStringFromUnicodeString(tch2.data());
#else
			msg = std::string(tch2.data(), tch2.size());
#endif

			if (msg.empty())
				return 1;

			// send it!
			if (msg.size() >= MAX_MESSAGE_SIZE)
			{
				MessageBox(
					hWnd,
					TmGetTString(IDS_MESSAGE_TOO_LONG_MSG),
					TmGetTString(IDS_PROGRAM_NAME),
					MB_ICONINFORMATION | MB_OK
				);
				return 1;
			}

			Snowflake sf;
			if (parms.m_bEdit)
				return GetDiscordInstance()->EditMessageInCurrentChannel(msg, parms.m_replyTo) ? 0 : 1;
			
			if (!GetDiscordInstance()->SendMessageToCurrentChannel(msg, sf, parms.m_replyTo, parms.m_bMention))
				return 1;

			SendMessageAuxParams smap;
			smap.m_message = msg;
			smap.m_snowflake = sf;
			SendMessage(hWnd, WM_SENDMESSAGEAUX, 0, (LPARAM)&smap);
			return 0;
		}
		case WM_SENDMESSAGEAUX:
		{
			SendMessageAuxParams* psmap = (SendMessageAuxParams*) lParam;

			time_t lastTime = g_pMessageList->GetLastSentMessageTime();
			Profile* pf = GetDiscordInstance()->GetProfile();
			Message m;
			m.m_snowflake = psmap->m_snowflake;
			m.m_author_snowflake = pf->m_snowflake;
			m.m_author = pf->m_name;
			m.m_message = psmap->m_message;
			m.m_type = MessageType::SENDING_MESSAGE;
			m.SetTime(time(NULL));
			m.m_dateFull = "Sending...";
			m.m_dateCompact = "Sending...";
			g_pMessageList->AddMessage(m, true);
			return 0;
		}
		case WM_FAILMESSAGE:
		{
			FailedMessageParams parms = *((FailedMessageParams*)lParam);
			if (g_pMessageList->GetCurrentChannel() != parms.channel)
				return 0;

			g_pMessageList->OnFailedToSendMessage(parms.message);
			return 0;
		}
		case WM_STARTTYPING:
		{
			TypingParams params  = *((TypingParams*) lParam);
			OnTyping(params.m_guild, params.m_channel, params.m_user, params.m_timestamp);
			break;
		}
		case WM_SESSIONCLOSED:
		{
			TCHAR buf[512];
			WAsnprintf(buf, _countof(buf), TmGetTString(IDS_SESSION_ERROR), (int)wParam);
			buf[_countof(buf) - 1] = 0;

			MessageBox(
				hWnd,
				buf,
				TmGetTString(IDS_PROGRAM_NAME),
				MB_ICONERROR | MB_OK
			);
			PostQuitMessage(0);
			break;
		}
		case WM_SHOWPROFILEPOPOUT:
		{
			auto* pParams = (ShowProfilePopoutParams*) lParam;
			ProfilePopout::DeferredShow(*pParams);
			delete pParams;
			break;
		}
		case WM_LOGGEDOUT:
		{
			if (wParam != 100)
			{
				MessageBox(
					hWnd,
					TmGetTString(IDS_NOTLOGGEDIN),
					TmGetTString(IDS_PROGRAM_NAME),
					MB_ICONERROR | MB_OK
				);
			}

			if (!LogonDialogShow())
				PostQuitMessage(0);
			else
			{
		case WM_LOGGEDOUT2:
				// try again with the new token
				GetHTTPClient()->StopAllRequests();
				GetAvatarCache()->WipeBitmaps();
				GetAvatarCache()->ClearProcessingRequests();
				GetProfileCache()->ClearAll();
				if (g_pDiscordInstance) delete g_pDiscordInstance;
				g_pDiscordInstance = new DiscordInstance(GetLocalSettings()->GetToken());
				g_pChannelView->ClearChannels();
				g_pMemberList->ClearMembers();
				g_pGuildLister->Update();
				g_pGuildHeader->Update();
				g_pProfileView->Update();
				GetDiscordInstance()->GatewayClosed(CloseCode::LOG_ON_AGAIN);
			}
			break;
		}
		case WM_STARTREPLY:
		{
			Snowflake* sf = (Snowflake*)lParam;
			// sf[0] - Message ID
			// sf[1] - Author ID
			g_pMessageEditor->StartReply(sf[0], sf[1]);

			break;
		}
		case WM_STARTEDITING:
		{
			Snowflake* sf = (Snowflake*)lParam;
			g_pMessageEditor->StartEdit(*sf);
			break;
		}
		case WM_KEYUP: {

#ifdef _DEBUG
			if (wParam == VK_F6) {
				SendMessage(hWnd, WM_LOGGEDOUT2, 0, 0);
			}
			if (wParam == VK_F7) {
				SendMessage(hWnd, WM_LOGGEDOUT, 0, 0);
			}
			if (wParam == VK_F8) {
				GetQRCodeDialog()->Show();
			}
			if (wParam == VK_F9) {
				ProgressDialog::Show("Test!", 1234, true, g_Hwnd);
			}
			if (wParam == VK_F10) {
				ProgressDialog::Show("Test!", 1234, false, g_Hwnd);
			}
#endif

			break;
		}
		case WM_ACTIVATE:
		{
			if (wParam == WA_INACTIVE && (HWND) lParam != ProfilePopout::GetHWND())
				ProfilePopout::Dismiss();

			if (wParam == WA_INACTIVE)
				AutoComplete::DismissAutoCompleteWindowsIfNeeded((HWND) lParam);

			break;
		}
		case WM_IMAGESAVED:
		{
			TCHAR buff[4096];
			WAsnprintf(buff, _countof(buff), TmGetTString(IDS_SAVED_UPDATE), (LPCTSTR) lParam);
			buff[_countof(buff) - 1] = 0;

			// TODO: Probably should automatically extract and install it or something
			MessageBox(
				hWnd,
				buff,
				TmGetTString(IDS_PROGRAM_NAME),
				MB_ICONINFORMATION | MB_OK
			);
			break;
		}
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

HFONT
	g_FntMd,
	g_FntMdB,
	g_FntMdI,
	g_FntMdBI,
	g_FntMdU,
	g_FntMdBU,
	g_FntMdIU,
	g_FntMdBIU,
	g_FntMdCode;

HFONT* g_FntMdStyleArray[FONT_TYPE_COUNT] = {
	&g_FntMd,   // 0
	&g_FntMdB,  // 1
	&g_FntMdI,  // 2
	&g_FntMdBI, // 2|1
	&g_FntMdU,  // 4
	&g_FntMdBU, // 4|1
	&g_FntMdIU, // 4|2
	&g_FntMdBIU,// 4|2|1
	&g_FntMdCode, // 9
};

void InitializeFonts()
{
	LOGFONT lf{};
	bool haveFont = SystemParametersInfo(SPI_GETICONTITLELOGFONT, sizeof lf, &lf, 0);
	lf.lfItalic = false;
	lf.lfUnderline = false;
	auto oldWeight = lf.lfWeight;

	lf.lfHeight = ScaleByUser(lf.lfHeight);

	HFONT hf, hfb, hfi, hfbi, hfu, hfbu, hfiu, hfbiu;
	hf = hfb = hfi = hfbi = hfu = hfbu = hfiu = hfbiu = NULL;

	if (haveFont)
		hf = CreateFontIndirect(&lf);

	if (!hf) {
		hf = GetStockFont(SYSTEM_FONT);
	}

	if (haveFont) {
		lf.lfWeight = 700;
		hfb = CreateFontIndirect(&lf);
		lf.lfItalic = true;
		hfbi = CreateFontIndirect(&lf);
		lf.lfUnderline = true;
		hfbiu = CreateFontIndirect(&lf);
		lf.lfItalic = false;
		hfbu = CreateFontIndirect(&lf);
		lf.lfWeight = oldWeight;
		hfu = CreateFontIndirect(&lf);
		lf.lfItalic = true;
		hfiu = CreateFontIndirect(&lf);
		lf.lfUnderline = false;
		hfi = CreateFontIndirect(&lf);
		// lf.lfItalic = false; -- yes I want it italic
		int oldSize = lf.lfHeight;
		lf.lfHeight = MulDiv(lf.lfHeight, 8, 10);
		g_ReplyTextFont = CreateFontIndirect(&lf);
		lf.lfHeight = oldSize;
	}

	if (!hfb)   hfb = hf;
	if (!hfi)   hfi = hf;
	if (!hfbi)  hfbi = hf;
	if (!hfb)   hfb = hf;
	if (!hfbu)  hfbu = hf;
	if (!hfiu)  hfiu = hf;
	if (!hfbiu) hfbiu = hf;

	g_FntMd = hf;
	g_FntMdB = hfb;
	g_FntMdI = hfi;
	g_FntMdU = hfu;
	g_FntMdBI = hfbi;
	g_FntMdBU = hfbu;
	g_FntMdIU = hfiu;
	g_FntMdBIU = hfbiu;

	g_MessageTextFont = hf;
	g_AuthorTextFont = hfb;
	g_DateTextFont = hfi;
	g_GuildCaptionFont = hfb;
	g_GuildSubtitleFont = hf;
	g_GuildInfoFont = hfi;
	g_AccountInfoFont = hfb;
	g_AccountTagFont = hf;
	g_SendButtonFont = hf;
	g_AuthorTextUnderlinedFont = hfbu;
	g_TypingRegFont = hfi;
	g_TypingBoldFont = hfbi;

	// monospace font
	LOGFONT lfMono{};
	if (!GetObject(GetStockObject(ANSI_FIXED_FONT), sizeof(LOGFONT), &lfMono)) {
		// fallback
		g_FntMdCode = GetStockFont(ANSI_FIXED_FONT);
	}
	else
	{
		// copy all properties except the face name
		_tcscpy(lfMono.lfFaceName, TEXT("Courier New"));
		lfMono.lfWidth = 0;
		lfMono.lfHeight = ScaleByUser(12 * GetSystemDPI() / 72);

		g_FntMdCode = CreateFontIndirect(&lfMono);
		if (!g_FntMdCode) {
			g_FntMdCode = GetStockFont(ANSI_FIXED_FONT);
		}
	}
}

Frontend_Win32* g_pFrontEnd;
NetworkerThreadManager* g_pHTTPClient;

Frontend* GetFrontend()
{
	return g_pFrontEnd;
}

HTTPClient* GetHTTPClient()
{
	return g_pHTTPClient;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nShowCmd)
{
	g_hInstance = hInstance;
	ri::InitReimplementation();

	InitializeCOM(); // important because otherwise TTS/shell stuff might not work
	InitCommonControls(); // actually a dummy but adds the needed reference to comctl32
	// (see https://devblogs.microsoft.com/oldnewthing/20050718-16/?p=34913 )
	XSetProcessDPIAware();

	g_pFrontEnd = new Frontend_Win32;
	g_pHTTPClient = new NetworkerThreadManager;

	// Create a background brush.
	g_backgroundBrush = GetSysColorBrush(COLOR_3DFACE);

	SetupCachePathIfNeeded();
	GetLocalSettings()->Load();

	SetUserScale(GetLocalSettings()->GetUserScale());

	int wndWidth = 0, wndHeight = 0;
	bool startMaximized = false;
	GetLocalSettings()->GetWindowSize(wndWidth, wndHeight);
	startMaximized = GetLocalSettings()->GetStartMaximized();

	// Initialize the window class.
	WNDCLASS& wc = g_MainWindowClass;

	wc.lpfnWndProc   = WindowProc;
	wc.hInstance     = hInstance;
	wc.lpszClassName = TEXT("DiscordMessengerClass");
	wc.hbrBackground = g_backgroundBrush;
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon         = g_Icon = LoadIcon(hInstance, MAKEINTRESOURCE(DMIC(IDI_ICON)));
	wc.lpszMenuName  = MAKEINTRESOURCE(IDR_MAINMENU);

	// NOTE: Despite that we pass LR_SHARED, if this "isn't a standard size" (whatever Microsoft means), we must still delete it!!
	g_DefaultProfilePicture = (HBITMAP)LoadImage(hInstance, MAKEINTRESOURCE(IDB_DEFAULT),                   IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION | LR_SHARED);
	g_ProfileBorderIcon     = (HICON)  LoadImage(hInstance, MAKEINTRESOURCE(DMIC(IDI_PROFILE_BORDER)),      IMAGE_ICON,   0, 0, LR_CREATEDIBSECTION | LR_SHARED);
	g_ProfileBorderIconGold = (HICON)  LoadImage(hInstance, MAKEINTRESOURCE(DMIC(IDI_PROFILE_BORDER_GOLD)), IMAGE_ICON,   0, 0, LR_CREATEDIBSECTION | LR_SHARED);

	InitializeStatusIcons();

	// Register the class.
	if (!RegisterClass(&wc))
		return 1;

	if (!RegisterImageViewerClass())
		return 1;

	HACCEL hAccTable = LoadAccelerators(g_hInstance, MAKEINTRESOURCE(IDR_MAIN_ACCELS));

	// Initialize the worker thread manager
	GetHTTPClient()->Init();

	// Create some fonts.
	InitializeFonts();

	int flags = WS_OVERLAPPEDWINDOW;
	if (startMaximized) {
		flags |= WS_MAXIMIZE;
	}

	g_Hwnd = CreateWindow(
		/* class */      TEXT("DiscordMessengerClass"),
		/* title */      TmGetTString(IDS_PROGRAM_NAME),
		/* style */      flags,
		/* x pos */      CW_USEDEFAULT,
		/* y pos */      CW_USEDEFAULT,
		/* x siz */      ScaleByDPI(wndWidth),
		/* y siz */      ScaleByDPI(wndHeight),
		/* parent */     NULL,
		/* menu  */      NULL,
		/* instance */   hInstance,
		/* lpparam */    NULL
	);

	if (!g_Hwnd) return 1;

	MSG msg = { };

	if (g_bQuittingEarly) {
		DestroyWindow(g_Hwnd);
		g_Hwnd = NULL;
	}
	else {
		TextToSpeech::Initialize();

		// Run the message loop.
		ShowWindow (g_Hwnd, startMaximized ? SW_SHOWMAXIMIZED : nShowCmd);

		while (GetMessage(&msg, NULL, 0, 0) > 0)
		{
			//
			// Hack.  This inspects ALL messages, including ones delivered to children
			// of the main window.  This is fine, since that means that I won't have to
			// add code to the child windows themselves to dismiss the popout.
			//
			// Note, this is safe to do because if msg's hwnd member is equal to
			// ProfilePopout::m_hwnd, IsDialogMessage has already called its dialogproc,
			// and returned false, therefore we wouldn't even get here.
			//
			switch (msg.message)
			{
				case WM_LBUTTONDOWN:
				case WM_RBUTTONDOWN:
				case WM_MBUTTONDOWN:
					// If the focus isn't sent to a child of the profile popout, dismiss the latter
					if (!IsChildOf(msg.hwnd, ProfilePopout::GetHWND()))
						ProfilePopout::Dismiss();

					// fallthrough

				case WM_WINDOWPOSCHANGED:
				case WM_MOVE:
				case WM_SIZE:
					AutoComplete::DismissAutoCompleteWindowsIfNeeded(msg.hwnd);
					break;
			}

			if (IsWindow(ProfilePopout::GetHWND()) && IsDialogMessage(ProfilePopout::GetHWND(), &msg))
				continue;

			if (hAccTable &&
				TranslateAccelerator(g_Hwnd, hAccTable, &msg))
				continue;

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		TextToSpeech::Deinitialize();
	}

	UnregisterClass(wc.lpszClassName, hInstance);
	GetLocalSettings()->Save();
	GetWebsocketClient()->Kill();
	GetHTTPClient()->Kill();
	delete g_pFrontEnd;
	delete g_pHTTPClient;
	return (int)msg.wParam;
}

void CALLBACK OnHeartbeatTimer(HWND hWnd, UINT uInt, UINT_PTR uIntPtr, DWORD dWord)
{
	// Don't care about the parameters, tell discord instance to send a heartbeat
	GetDiscordInstance()->SendHeartbeat();
}

void SetHeartbeatInterval(int timeMs)
{
	if (g_HeartbeatTimerInterval == timeMs)
		return;

	assert(timeMs > 100 || timeMs == 0);

	if (g_HeartbeatTimer != 0)
	{
		KillTimer(g_Hwnd, g_HeartbeatTimer);
		g_HeartbeatTimer = 0;
	}

	if (timeMs != 0)
	{
		g_HeartbeatTimer = SetTimer(g_Hwnd, 0, timeMs, OnHeartbeatTimer);
	}
}
