#include "MainWindow.hpp"
#include <shellapi.h>

// Controls
#include "StatusBar.hpp"
#include "MessageList.hpp"
#include "GuildLister.hpp"
#include "GuildHeader.hpp"
#include "ProfileView.hpp"
#include "IMemberList.hpp"
#include "IChannelView.hpp"
#include "MessageEditor.hpp"
#include "LoadingMessage.hpp"

// Other Stuff
#include "Main.hpp"
#include "CrashDebugger.hpp"
#include "PinList.hpp"
#include "ProfilePopout.hpp"
#include "UploadDialog.hpp"
#include "ShellNotification.hpp"
#include "QRCodeDialog.hpp"
#include "LogonDialog.hpp"
#include "ImageViewer.hpp"
#include "OptionsDialog.hpp"
#include "AboutDialog.hpp"
#include "TextToSpeech.hpp"
#include "QuickSwitcher.hpp"
#include "GuildSubWindow.hpp"

#include "../discord/WebsocketClient.hpp"
#include "../discord/LocalSettings.hpp"
#include "../discord/UpdateChecker.hpp"

DiscordInstance* g_pDiscordInstance;
MainWindow* g_pMainWindow;

DiscordInstance* GetDiscordInstance() {
	return g_pDiscordInstance;
}
std::string GetDiscordToken() {
	return g_pDiscordInstance->GetToken();
}
MainWindow* GetMainWindow() {
	return g_pMainWindow;
}
HWND GetMainHWND() {
	if (!g_pMainWindow)
		return NULL;
	return g_pMainWindow->GetHWND();
}

const CHAR g_StartupArg[] = "/startup";

MainWindow::MainWindow(LPCTSTR pClassName, int nShowCmd)
{
	if (g_pMainWindow) {
		DbgPrintW("There can be only one.");
		m_bInitFailed = true;
		return;
	}

	g_pMainWindow = this;

	g_pDiscordInstance = new DiscordInstance(GetLocalSettings()->GetToken());

	// since we're the main window, g_pDiscordInstance didn't exist yet, therefore
	// register us manually
	g_pDiscordInstance->RegisterView(GetChatView());

	assert(GetChatView()->GetID() == 0);

	// Initialize the window class.
	WNDCLASS& wc = m_wndClass;

	wc.lpfnWndProc   = WndProc;
	wc.hInstance     = g_hInstance;
	wc.lpszClassName = pClassName;
	wc.hbrBackground = g_backgroundBrush;
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon         = g_Icon = LoadIcon(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_ICON)));
	wc.lpszMenuName  = MAKEINTRESOURCE(IDR_MAINMENU);

	if (!RegisterClass(&wc)) {
		DbgPrintW("MainWindow RegisterClass failed!");
		m_bInitFailed = true;
		return;
	}

	if (!GuildSubWindow::InitializeClass()) {
		DbgPrintW("GuildSubWindow RegisterClass failed!");
		return;
	}

	// Get the window creation parameters.
	int wndWidth = 0, wndHeight = 0;
	bool startMaximized = false;
	GetLocalSettings()->GetWindowSize(wndWidth, wndHeight);
	startMaximized = GetLocalSettings()->GetStartMaximized() || GetLocalSettings()->GetMaximized();

	int flags = WS_OVERLAPPEDWINDOW;
	if (startMaximized) {
		flags |= WS_MAXIMIZE;
	}

	m_hwnd = CreateWindow(
		/* class */      pClassName,
		/* title */      TmGetTString(IDS_PROGRAM_NAME),
		/* style */      flags,
		/* x pos */      CW_USEDEFAULT,
		/* y pos */      CW_USEDEFAULT,
		/* x siz */      wndWidth,
		/* y siz */      wndHeight,
		/* parent */     NULL,
		/* menu  */      NULL,
		/* instance */   g_hInstance,
		/* lpparam */    NULL
	);
	if (!m_hwnd) {
		DbgPrintW("Main window failed to create!");
		m_bInitFailed = true;
		return;
	}

	// Show the main window if needed.
	if (!IsFromStartup() || !GetLocalSettings()->GetStartMinimized())
		ShowWindow(m_hwnd, startMaximized ? SW_SHOWMAXIMIZED : nShowCmd);
}

MainWindow::~MainWindow()
{
	if (g_pMainWindow == this) {
		g_pMainWindow = nullptr;
	}

	SAFE_DELETE(g_pDiscordInstance);
	SAFE_DELETE(m_pStatusBar);
	SAFE_DELETE(m_pMessageList);
	SAFE_DELETE(m_pProfileView);
	SAFE_DELETE(m_pGuildHeader);
	SAFE_DELETE(m_pGuildLister);
	SAFE_DELETE(m_pMemberList);
	SAFE_DELETE(m_pChannelView);
	SAFE_DELETE(m_pMessageEditor);
	SAFE_DELETE(m_pLoadingMessage);

	if (m_HeartbeatTimer != 0) KillTimer(m_hwnd, m_HeartbeatTimer);
	if (m_TryAgainTimer != 0)  KillTimer(m_hwnd, m_TryAgainTimer);
}

bool MainWindow::IsPartOfMainWindow(HWND hWnd)
{
	return hWnd == m_pChannelView->GetListHWND() ||
	       hWnd == m_pChannelView->GetTreeHWND() ||
	       hWnd == m_pMemberList->GetListHWND() ||
	       hWnd == m_pMemberList->m_mainHwnd ||
	       hWnd == m_pGuildLister->m_hwnd ||
	       hWnd == m_pMessageList->m_hwnd;
}

bool HasViewIDSpecifier(UINT uMsg)
{
	switch (uMsg)
	{
		case WM_UPDATESELECTEDGUILD:
		case WM_UPDATESELECTEDCHANNEL:
		case WM_UPDATECHANLIST:
		case WM_UPDATEMEMBERLIST:
		case WM_REFRESHMEMBERS:
		case WM_SENDTOMESSAGE:
		case WM_DELETEMESSAGE:
		case WM_CLOSEVIEW:
			return true;
	}

	return false;
}

bool ShouldMirrorEventToSubViews(UINT uMsg)
{
	switch (uMsg)
	{
		case WM_UPDATEUSER:
		case WM_UPDATEEMOJI:
		case WM_UPDATECHANACKS:
		case WM_UPDATEATTACHMENT:
		case WM_REPAINTGUILDLIST:
		case WM_REPAINTPROFILE:
		case WM_REFRESHMESSAGES:
		case WM_REPAINTMSGLIST:
		case WM_RECALCMSGLIST:
		case WM_MSGLISTUPDATEMODE:
		case WM_ADDMESSAGE:
		case WM_UPDATEMESSAGE:
		case WM_DELETEMESSAGE:
		case WM_STARTTYPING:
			return true;
	}

	return false;
}

void MainWindow::UpdateMemberListVisibility()
{
	int off = ScaleByDPI(10) + m_MemberListWidth;

	if (m_bMemberListVisible) {
		ShowWindow(m_pMemberList->m_mainHwnd, SW_SHOWNORMAL);
		UpdateWindow(m_pMemberList->m_mainHwnd);
		ResizeWindow(m_pMessageList->m_hwnd, -off, 0, false, false, true);
		ResizeWindow(m_pMessageEditor->m_hwnd, -off, 0, false, false, true);
	}
	else {
		ShowWindow(m_pMemberList->m_mainHwnd, SW_HIDE);
		UpdateWindow(m_pMemberList->m_mainHwnd);
		ResizeWindow(m_pMessageList->m_hwnd, off, 0, false, false, true);
		ResizeWindow(m_pMessageEditor->m_hwnd, off, 0, false, false, true);
	}
}

LRESULT MainWindow::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	KeepOverridingTheFilter();

	m_agerCounter++;
	if (m_agerCounter >= 50) {
		m_agerCounter = 0;
		GetAvatarCache()->AgeBitmaps();
	}

	// Main view always has view ID of 0
	if (HasViewIDSpecifier(uMsg) && wParam != 0)
	{
		for (auto& window : m_subWindows) {
			if (window->GetChatView()->GetID() == (int) wParam)
				return SendMessage(window->GetHWND(), uMsg, wParam, lParam);
		}
	}

	switch (uMsg)
	{
		case WM_UPDATEEMOJI:
		{
			Snowflake sf = *(Snowflake*)lParam;
			m_pMessageList->OnUpdateEmoji(sf);
			m_pGuildHeader->OnUpdateEmoji(sf);
			PinList::OnUpdateEmoji(sf);
			break;
		}
		case WM_UPDATEUSER:
		{
			Snowflake sf = *(Snowflake*)lParam;
			m_pMessageList->OnUpdateAvatar(sf);
			m_pMemberList->OnUpdateAvatar(sf);
			m_pChannelView->OnUpdateAvatar(sf);
			PinList::OnUpdateAvatar(sf);

			if (ProfilePopout::GetUser() == sf)
				ProfilePopout::Update();
			break;
		}
		case WM_HTTPERROR:
		{
			const char** arr = (const char**)lParam;
			return OnHTTPError(CutOutURLPath(std::string((const char*)arr[0])), std::string((const char*)arr[1]), (bool)wParam);
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
			m_pStatusBar->UpdateCharacterCounter(int(lParam), MAX_MESSAGE_SIZE);
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
			m_pMessageList->OnUpdateEmbed(pl.key);
			PinList::OnUpdateEmbed(pl.key);
			break;
		}
		case WM_REPAINTPROFILE:
		{
			m_pProfileView->Update();
			break;
		}
		case WM_UPDATEPROFILEPOPOUT:
		{
			ProfilePopout::Update();
			break;
		}
		case WM_REPAINTGUILDLIST:
		{
			m_pGuildLister->Update();
			break;
		}
		case WM_REFRESHMESSAGES:
		{
			RefreshMessagesParams* params = (RefreshMessagesParams*)lParam;

			if (m_pMessageList->GetCurrentChannelID() == params->m_channelId)
				m_pMessageList->RefetchMessages(params->m_gapCulprit, true);

			break;
		}
		case WM_REPAINTMSGLIST:
		{
			InvalidateRect(m_pMessageList->m_hwnd, NULL, false);
			break;
		}
		case WM_MSGLISTUPDATEMODE:
		{
			m_pMessageList->UpdateBackgroundBrush();
			InvalidateRect(m_pMessageList->m_hwnd, NULL, false);
			break;
		}
		case WM_RECALCMSGLIST:
		{
			m_pMessageList->FullRecalcAndRepaint();
			break;
		}
		case WM_SHOWUPLOADDIALOG:
		{
			Snowflake* sf = (Snowflake*) lParam;
			UploadDialogShow2(hWnd, *sf);
			delete sf;
			break;
		}
		case WM_TOGGLEMEMBERS:
		{
			m_bMemberListVisible ^= 1;
			UpdateMemberListVisibility();
			break;
		}
		case WM_TOGGLECHANNELS:
		{
			m_bChannelListVisible ^= 1;
			int off = ScaleByDPI(10) + m_ChannelViewListWidth2;

			if (m_bChannelListVisible) {
				ShowWindow(m_pChannelView->m_hwnd, SW_SHOWNORMAL);
				ShowWindow(m_pProfileView->m_hwnd, SW_SHOWNORMAL);
				UpdateWindow(m_pChannelView->m_hwnd);
				UpdateWindow(m_pProfileView->m_hwnd);
				ResizeWindow(m_pMessageList->m_hwnd, off, 0, true, false, true);
				ResizeWindow(m_pMessageEditor->m_hwnd, off, 0, true, false, true);
			}
			else {
				ShowWindow(m_pChannelView->m_hwnd, SW_HIDE);
				ShowWindow(m_pProfileView->m_hwnd, SW_HIDE);
				UpdateWindow(m_pChannelView->m_hwnd);
				UpdateWindow(m_pProfileView->m_hwnd);
				ResizeWindow(m_pMessageList->m_hwnd, -off, 0, true, false, true);
				ResizeWindow(m_pMessageEditor->m_hwnd, -off, 0, true, false, true);
			}

			break;
		}
		case WM_UPDATESELECTEDGUILD:
		{
			SetCurrentGuildID(GetCurrentGuildID());
			break;
		}
		case WM_UPDATESELECTEDCHANNEL:
		{
			SetCurrentChannelID(GetCurrentChannelID());
			break;
		}
		case WM_UPDATECHANACKS:
		{
			Snowflake* sfs = (Snowflake*)lParam;

			Snowflake channelID = sfs[0];
			Snowflake messageID = sfs[1];

			Channel* pChan = GetDiscordInstance()->GetChannelGlobally(channelID);

			// Update the icon for the specific guild
			m_pGuildLister->RedrawIconForGuild(pChan->m_parentGuild);

			// Get the channel as is in the current guild; if found,
			// update the channel view's ack status
			Guild* pGuild = GetDiscordInstance()->GetGuild(GetCurrentGuildID());
			if (!pGuild) break;

			pChan = pGuild->GetChannel(channelID);
			if (!pChan)
				break;

			m_pChannelView->UpdateAcknowledgement(channelID);
			if (m_pMessageList->GetCurrentChannelID() == channelID)
				m_pMessageList->SetLastViewedMessage(messageID, true);
			break;
		}
		case WM_UPDATEMEMBERLIST:
		{
			m_pMemberList->SetGuild(GetCurrentGuildID());
			m_pMemberList->Update();
			break;
		}
		case WM_UPDATECHANLIST:
		{
			// repaint the channel view
			m_pChannelView->ClearChannels();

			// get the current guild
			Guild* pGuild = GetDiscordInstance()->GetGuild(GetCurrentGuildID());
			if (!pGuild) break;

			m_pChannelView->SetMode(pGuild->m_snowflake == 0);

			// if the channels haven't been fetched yet
			if (!pGuild->m_bChannelsLoaded)
			{
				pGuild->RequestFetchChannels();
			}
			else
			{
				for (auto& ch : pGuild->m_channels)
					m_pChannelView->AddChannel(ch);
				for (auto& ch : pGuild->m_channels)
					m_pChannelView->RemoveCategoryIfNeeded(ch);

				m_pChannelView->CommitChannels();
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

			Channel* pChan = GetDiscordInstance()->GetChannel(pParms->channel);
			if (!pChan)
				break;

			if (pChan->IsDM()) {
				if (GetDiscordInstance()->ResortChannels(pChan->m_parentGuild))
					SendMessage(m_hwnd, WM_UPDATECHANLIST, 0, 0);
			}

			if (m_pMessageList->GetCurrentChannelID() == pParms->channel)
				m_pMessageList->AddMessage(pParms->msg.m_snowflake, GetForegroundWindow() == hWnd);

			OnStopTyping(pParms->channel, pParms->msg.m_author_snowflake);
			break;
		}
		case WM_UPDATEMESSAGE:
		{
			AddMessageParams* pParms = (AddMessageParams*)lParam;

			GetMessageCache()->EditMessage(pParms->channel, pParms->msg);

			if (m_pMessageList->GetCurrentChannelID() == pParms->channel)
				m_pMessageList->EditMessage(pParms->msg.m_snowflake);

			break;
		}
		case WM_DELETEMESSAGE:
		{
			Snowflake sf = *(Snowflake*)lParam;
			m_pMessageList->DeleteMessage(sf);
			
			if (sf == m_pMessageEditor->ReplyingTo())
				m_pMessageEditor->StopReply();

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

			delete pParm;
			break;
		}
		case WM_REFRESHMEMBERS:
		{
			auto* memsToUpdate = (std::set<Snowflake>*)lParam;

			m_pMessageList->UpdateMembers(*memsToUpdate);
			m_pMemberList ->UpdateMembers(*memsToUpdate);
			m_pMessageEditor->OnLoadedMemberChunk();

			break;
		}
		case WM_CREATE:
		{
			m_hwnd = hWnd;

			m_bMemberListVisible = true;
			m_bChannelListVisible = true;
			ForgetSystemDPI();

			g_ProfilePictureSize = ScaleByDPI(PROFILE_PICTURE_SIZE_DEF);

			if (GetLocalSettings()->GetToken().empty())
			{
				if (!LogonDialogShow())
				{
					PostQuitMessage(0);
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

			if (GetLocalSettings()->IsFirstStart())
			{
				MessageBox(
					m_hwnd,
					TmGetTString(IDS_WELCOME_MSG),
					TmGetTString(IDS_PROGRAM_NAME),
					MB_ICONINFORMATION | MB_OK
				);
			}

			if (g_SendIcon)     DeleteObject(g_SendIcon);
			if (g_JumpIcon)     DeleteObject(g_JumpIcon);
			if (g_CancelIcon)   DeleteObject(g_CancelIcon);
			if (g_UploadIcon)   DeleteObject(g_UploadIcon);
			if (g_DownloadIcon) DeleteObject(g_DownloadIcon);
			
			bool x = NT31SimplifiedInterface();
			g_SendIcon     = x ? NULL : (HICON)ri::LoadImage(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_SEND)),     IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
			g_JumpIcon     = x ? NULL : (HICON)ri::LoadImage(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_JUMP)),     IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
			g_CancelIcon   = x ? NULL : (HICON)ri::LoadImage(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_CANCEL)),   IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
			g_UploadIcon   = x ? NULL : (HICON)ri::LoadImage(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_UPLOAD)),   IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);

			g_BotIcon      = (HICON) ri::LoadImage(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_BOT)),      IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
			g_DownloadIcon = (HICON) ri::LoadImage(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_DOWNLOAD)), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);

			MessageList::InitializeClass();
			ProfileView::InitializeClass();
			GuildHeader::InitializeClass();
			GuildLister::InitializeClass();
			AutoComplete::InitializeClass();
			IMemberList::InitializeClasses();
			IChannelView::InitializeClasses();
			MessageEditor::InitializeClass();
			LoadingMessage::InitializeClass();

			RECT rect = {}, rect2 = {};
			GetClientRect(m_hwnd, &rect);

			rect2 = rect;
			rect.left   += 10;
			rect.top    += 10;
			rect.right  -= 10;
			rect.bottom -= 10;

			RECT rcLoading = { 0, 0, 300, 200 };
			POINT pt{ 0, 0 };
			OffsetRect(&rcLoading, rect2.right / 2 - rcLoading.right / 2, 0);// rect2.bottom / 2 - rcLoading.bottom / 2);
			ClientToScreen(m_hwnd, &pt);
			OffsetRect(&rcLoading, pt.x, pt.y);

			m_pStatusBar   = StatusBar::Create(this);
			m_pMessageList = MessageList::Create(this, NULL, &rect);
			m_pProfileView = ProfileView::Create(m_hwnd, &rect, CID_PROFILEVIEW, true);
			m_pGuildHeader = GuildHeader::Create(this, &rect);
			m_pGuildLister = GuildLister::Create(this, &rect);
			m_pMemberList  = IMemberList::CreateMemberList(this, &rect);
			m_pChannelView = IChannelView::CreateChannelView(this, &rect);
			m_pMessageEditor = MessageEditor::Create(this, &rect);
			m_pLoadingMessage = LoadingMessage::Create(m_hwnd, &rcLoading);

			if (!m_pStatusBar ||
				!m_pMemberList ||
				!m_pMessageList ||
				!m_pProfileView ||
				!m_pGuildLister ||
				!m_pGuildHeader ||
				!m_pChannelView ||
				!m_pMessageEditor ||
				!m_pLoadingMessage)
			{
				DbgPrintW("ERROR: A control failed to be created!");
				m_bInitFailed = true;
				break;
			}

			if (m_pMessageEditor)
				m_pMessageEditor->SetMessageList(m_pMessageList);

			SendMessage(hWnd, WM_LOGINAGAIN, 0, 0);
			PostMessage(hWnd, WM_POSTINIT, 0, 0);
			break;
		}
		case WM_POSTINIT:
		{
			GetShellNotification()->Initialize();
			break;
		}
		case WM_CONNECTERROR:
			// try it again
			EnableMenuItem(GetSubMenu(GetMenu(hWnd), 0), ID_FILE_RECONNECTTODISCORD, MF_ENABLED);
			DbgPrintW("Trying to connect to websocket again in %d ms", m_TryAgainTimerInterval);

			TryConnectAgainIn(m_TryAgainTimerInterval);
			m_TryAgainTimerInterval = m_TryAgainTimerInterval * 115 / 100;
			if (m_TryAgainTimerInterval > 10000)
				m_TryAgainTimerInterval = 10000;

			break;

		case WM_CONNECTERROR2:
		case WM_CONNECTED:
		{
			ResetTryAgainInTime();
			if (m_TryAgainTimer)
			{
				KillTimer(hWnd, m_TryAgainTimer);
				m_TryAgainTimer = 0;
			}

			m_pLoadingMessage->Hide();

			if (IsFromStartup() && GetLocalSettings()->GetStartMinimized())
			{
				GetFrontend()->HideWindow();
			}

			EnableMenuItem(GetSubMenu(GetMenu(hWnd), 0), ID_FILE_RECONNECTTODISCORD, MF_GRAYED);
			break;
		}

		case WM_CONNECTING:
		{
			if (!IsFromStartup() || !GetLocalSettings()->GetStartMinimized())
				m_pLoadingMessage->Show();

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

		case WM_CLOSEBYPASSTRAY:
			AddOrRemoveAppFromStartup();
			CloseCleanup();
			DestroyWindow(hWnd);
			break;

		case WM_SETBROWSINGPAST:
			if (wParam)
				m_pMessageEditor->StartBrowsingPast();
			else
				m_pMessageEditor->StopBrowsingPast();
			break;

		case WM_CLOSE:
			CloseCleanup();

			if (GetLocalSettings()->GetMinimizeToNotif() && LOBYTE(GetVersion()) >= 4)
			{
				GetFrontend()->HideWindow();
				return 1;
			}
			break;

		case WM_SIZE: {
			int width = LOWORD(lParam);
			int height = HIWORD(lParam);

			RECT r{ 0, 0, width, height };
			AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);

			// Save the new size
			GetLocalSettings()->SetWindowSize(r.right - r.left, r.bottom - r.top);
			GetLocalSettings()->SetMaximized(wParam == SIZE_MAXIMIZED);

			ProfilePopout::Dismiss();
			ProperlySizeControls();
			break;
		}
		case WM_REPOSITIONEVERYTHING:
		{
			ProperlySizeControls();
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

		case WM_KILLFOCUS:
			GetLocalSettings()->Save();
			break;

		case WM_DESTROY:
		{
			if (GetDiscordInstance())
				GetDiscordInstance()->CloseGatewaySession();
			if (GetAvatarCache())
				GetAvatarCache()->WipeBitmaps();

			SAFE_DELETE(m_pChannelView);
			SAFE_DELETE(m_pGuildHeader);
			SAFE_DELETE(m_pGuildLister);
			SAFE_DELETE(m_pMessageList);
			SAFE_DELETE(m_pProfileView);

			SAFE_DELETE(m_pStatusBar);
			SAFE_DELETE(m_pMemberList);
			SAFE_DELETE(m_pMessageEditor);
			SAFE_DELETE(m_pLoadingMessage);

			GetShellNotification()->Deinitialize();

			PostQuitMessage(0);
			break;
		}
		case WM_DRAWITEM:
		{
			switch (wParam)
			{
				case CID_STATUSBAR:
					m_pStatusBar->DrawItem((LPDRAWITEMSTRUCT)lParam);
					break;
			}
			break;
		}
		case WM_RECREATEMEMBERLIST:
		{
			RECT rc{};
			GetChildRect(hWnd, m_pMemberList->m_mainHwnd, &rc);
			SAFE_DELETE(m_pMemberList);

			m_pMemberList = IMemberList::CreateMemberList(this, &rc);

			WindowProc(hWnd, WM_UPDATEMEMBERLIST, 0, 0);
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
					Guild* pGuild = GetCurrentGuild();
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
			m_pMessageList->SendToMessage(sf);
			break;
		}
		case WM_SENDMESSAGE:
		{
			SendMessageParams parms = *((SendMessageParams*)lParam);

			std::string msg = MakeStringFromEditData(parms.m_rawMessage);

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
				return GetDiscordInstance()->EditMessage(parms.m_channel, msg, parms.m_replyTo) ? 0 : 1;
			
			if (!GetDiscordInstance()->SendAMessage(parms.m_channel, msg, sf, parms.m_replyTo, parms.m_bMention))
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

			time_t lastTime = m_pMessageList->GetLastSentMessageTime();
			Profile* pf = GetDiscordInstance()->GetProfile();
			MessagePtr m = MakeMessage();
			m->m_snowflake = psmap->m_snowflake;
			m->m_author_snowflake = pf->m_snowflake;
			m->m_author = pf->m_name;
			m->m_avatar = pf->m_avatarlnk;
			m->m_message = psmap->m_message;
			m->m_type = MessageType::SENDING_MESSAGE;
			m->SetTime(time(NULL));
			m->m_dateFull = "Sending...";
			m->m_dateCompact = "Sending...";
			m_pMessageList->AddMessage(m, true);
			return 0;
		}
		case WM_FAILMESSAGE:
		{
			FailedMessageParams parms = *((FailedMessageParams*)lParam);
			if (m_pMessageList->GetCurrentChannelID() != parms.channel)
				return 0;

			m_pMessageList->OnFailedToSendMessage(parms.message);
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

			EnableMenuItem(GetSubMenu(GetMenu(hWnd), 0), ID_FILE_RECONNECTTODISCORD, MF_ENABLED);
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
			
			if (!LogonDialogShow()) {
				PostQuitMessage(0);
				break;
			}

			// fallthrough

		case WM_LOGGEDOUT2:
			// try again with the new token
			GetHTTPClient()->StopAllRequests();
			GetAvatarCache()->WipeBitmaps();
			GetAvatarCache()->ClearProcessingRequests();
			GetProfileCache()->ClearAll();

			if (g_pDiscordInstance) delete g_pDiscordInstance;
			g_pDiscordInstance = new DiscordInstance(GetLocalSettings()->GetToken());

			m_pChannelView->ClearChannels();
			m_pMemberList->ClearMembers();
			m_pGuildLister->Update();
			m_pGuildHeader->Update();
			m_pProfileView->Update();
			GetDiscordInstance()->GatewayClosed(CloseCode::LOG_ON_AGAIN);
			break;
		}
		case WM_STARTREPLY:
		{
			Snowflake* sf = (Snowflake*)lParam;
			// sf[0] - Message ID
			// sf[1] - Author ID
			m_pMessageEditor->StartReply(sf[0], sf[1]);

			break;
		}
		case WM_STARTEDITING:
		{
			Snowflake* sf = (Snowflake*)lParam;
			m_pMessageEditor->StartEdit(*sf);
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
			if (wParam == VK_F11) {
				Message msg;
				msg.m_author = "Test Author";
				msg.m_message = "Test message!!";
				GetNotificationManager()->OnMessageCreate(0, 1, msg);
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
			LPCTSTR file = (LPCTSTR) lParam;
			size_t sl = _tcslen(file);
			bool isExe = false;

			if (sl > 4 && (
				_tcscmp(file + sl - 4, TEXT(".exe")) == 0 ||
				_tcscmp(file + sl - 4, TEXT(".scr")) == 0 ||
				_tcscmp(file + sl - 4, TEXT(".lnk")) == 0 ||
				_tcscmp(file + sl - 4, TEXT(".zip")) == 0 ||
				_tcscmp(file + sl - 4, TEXT(".rar")) == 0 ||
				_tcscmp(file + sl - 4, TEXT(".7z"))  == 0)) {
				isExe = true;
			}

			TCHAR buff[4096];
			WAsnprintf(
				buff,
				_countof(buff),
				isExe ? TmGetTString(IDS_SAVED_STRING_EXE) : TmGetTString(IDS_SAVED_UPDATE),
				file
			);
			buff[_countof(buff) - 1] = 0;

			// TODO: Probably should automatically extract and install it or something
			int res = MessageBox(
				hWnd,
				buff,
				TmGetTString(IDS_PROGRAM_NAME),
				MB_ICONINFORMATION | MB_YESNO
			);
			
			if (res == IDYES) {
				ShellExecute(m_hwnd, TEXT("open"), file, NULL, NULL, SW_SHOWNORMAL);
			}

			break;
		}
		case WM_UPDATEAVAILABLE:
		{
			std::string* msg = (std::string*) lParam;
			auto& url = msg[0];
			auto& version = msg[1];
			
			TCHAR buff[2048];
			LPTSTR tstr1 = ConvertCppStringToTString(std::string(GetAppVersionString()));
			LPTSTR tstr2 = ConvertCppStringToTString(version);
			WAsnprintf(buff, _countof(buff), TmGetTString(IDS_NEW_VERSION_AVAILABLE), tstr1, tstr2);
			free(tstr1);
			free(tstr2);

			if (MessageBox(m_hwnd, buff, TmGetTString(IDS_PROGRAM_NAME), MB_ICONINFORMATION | MB_YESNO) == IDYES)
			{
				size_t idx = 0, idxsave = 0;
				for (; idx != url.size(); idx++) {
					if (url[idx] == '/')
						idxsave = idx + 1;
				}

				DownloadFileDialog(m_hwnd, url, url.substr(idxsave));
			}
			else
			{
				GetLocalSettings()->StopUpdateCheckTemporarily();
			}

			delete[] msg;
			break;
		}
		case WM_NOTIFMANAGERCALLBACK:
		{
			GetShellNotification()->Callback(wParam, lParam);
			break;
		}
		case WM_RESTOREAPP:
			if (GetLocalSettings()->GetMaximized())
				GetFrontend()->MaximizeWindow();
			else
				GetFrontend()->RestoreWindow();
			break;
	}

	if (ShouldMirrorEventToSubViews(uMsg))
	{
		for (auto& window : m_subWindows)
			SendMessage(window->GetHWND(), uMsg, wParam, lParam);
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

LRESULT MainWindow::HandleCommand(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
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
					GetDiscordInstance()->ClearData();
					GetLocalSettings()->SetToken("");
					GetLocalSettings()->Save();
					m_pChannelView->ClearChannels();
					m_pMemberList->ClearMembers();
					m_pStatusBar->ClearTypers();
					m_pMessageList->ClearMessages();
					m_pGuildLister->ClearTooltips();
					return TRUE;
				}
			}
			break;
		}
		case ID_FILE_RECONNECTTODISCORD:
		{
			if (!GetDiscordInstance()->IsGatewayConnected())
				SendMessage(hWnd, WM_LOGINAGAIN, 0, 0);

			break;
		}
		case ID_FILE_EXIT:
		{
			SendMessage(hWnd, WM_CLOSEBYPASSTRAY, 0, 0);
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
			m_pGuildLister->AskLeave(GetCurrentGuildID());
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
			HWND focus = GetFocus();

			if (IsChild(m_hwnd, focus))
			{
				ShowPasteFileDialog(m_pMessageEditor->m_edit_hwnd);
			}
			else
			{
				// maybe it's pasted into one of the children
				for (auto& window : m_subWindows)
				{
					if (IsChild(window->GetHWND(), focus))
						SendMessage(window->GetHWND(), WM_COMMAND, wParam, lParam);
				}
			}
			break;
		}

		case ID_NOTIFICATION_SHOW:
			SendMessage(m_hwnd, WM_RESTOREAPP, 0, 0);
			break;

		case ID_NOTIFICATION_EXIT:
			SendMessage(m_hwnd, WM_CLOSEBYPASSTRAY, 0, 0);
			break;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}


void MainWindow::SetHeartbeatInterval(int timeMs)
{
	if (m_HeartbeatTimerInterval == timeMs)
		return;

	assert(timeMs > 100 || timeMs == 0);

	if (m_HeartbeatTimer != 0)
	{
		KillTimer(m_hwnd, m_HeartbeatTimer);
		m_HeartbeatTimer = 0;
	}

	if (timeMs != 0)
	{
		m_HeartbeatTimer = SetTimer(m_hwnd, 0, timeMs, OnHeartbeatTimer);
	}
}

void MainWindow::OnHeartbeatTimer(HWND hWnd, UINT uInt, UINT_PTR uIntPtr, DWORD dWord)
{
	// Don't care about the parameters, tell discord instance to send a heartbeat
	GetDiscordInstance()->SendHeartbeat();
}

void MainWindow::OnTyping(Snowflake guildID, Snowflake channelID, Snowflake userID, time_t timeStamp)
{
	TypingInfo& ti = m_typingInfo[channelID];

	TypingUser tu;
	tu.m_startTimeMS = timeStamp * 1000LL;
	tu.m_key = userID;
	tu.m_name = GetProfileCache()->LookupProfile(userID, "", "", "", false)->GetName(guildID);
	ti.m_typingUsers[userID] = tu;

	// Send an update to the message editor
	if (channelID == GetCurrentChannelID())
	{
		if (userID == GetDiscordInstance()->GetUserID()) {
			// Ignore typing start from ourselves for the lower bar
			return;
		}

		m_pStatusBar->AddTypingName(userID, timeStamp, tu.m_name);
	}
}

void MainWindow::OnStopTyping(Snowflake channelID, Snowflake userID)
{
	m_typingInfo[channelID].m_typingUsers[userID].m_startTimeMS = 0;

	if (channelID == GetCurrentChannelID())
		m_pStatusBar->RemoveTypingName(userID);
}

void MainWindow::CreateGuildSubWindow(Snowflake guildId, Snowflake channelId)
{
	auto window = std::make_shared<GuildSubWindow>(guildId, channelId);
	if (window->InitFailed()) {
		DbgPrintW("Failed to create guild sub window!");
		return;
	}

	m_subWindows.push_back(window);
}

void MainWindow::CloseSubWindowByViewID(int viewID)
{
	for (auto& window : m_subWindows)
	{
		if (window->GetChatView()->GetID() == viewID)
			window->Close();
	}
}

void MainWindow::Close()
{
	DbgPrintW("To close the main window, you have to press the X");
}

void MainWindow::OnClosedWindow(ChatWindow* ptr)
{
	for (auto iter = m_subWindows.begin();
		iter != m_subWindows.end();
		++iter)
	{
		if (iter->get() == ptr)
		{
			m_subWindows.erase(iter);
			return;
		}
	}
}

void MainWindow::MirrorMessageToSubViewByChannelID(Snowflake channelId, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	for (auto& window : m_subWindows)
	{
		if (window->GetCurrentChannelID() == channelId)
		{
			SendMessage(window->GetHWND(), uMsg, wParam, lParam);
			return;
		}
	}
}

TypingInfo& MainWindow::GetTypingInfo(Snowflake sf)
{
	return m_typingInfo[sf];
}

void MainWindow::SetCurrentGuildID(Snowflake sf)
{
	if (m_lastGuildID == sf)
		return;

	bool wasVisible = m_bMemberListVisible;
	if (m_lastGuildID == 0 && sf != 0)
	{
		m_bMemberListVisible = m_bMemberListVisibleBackup;
	}
	else if (m_lastGuildID != 0 && sf == 0)
	{
		m_bMemberListVisibleBackup = m_bMemberListVisible;
		m_bMemberListVisible = false;
	}

	m_lastGuildID = sf;

	ChatWindow::SetCurrentGuildID(sf);

	// repaint the guild lister
	m_pGuildLister->UpdateSelected();
	m_pGuildHeader->Update();

	if (m_bMemberListVisible != wasVisible)
		UpdateMemberListVisibility();

	SendMessage(m_hwnd, WM_UPDATECHANLIST, 0, 0);
	SendMessage(m_hwnd, WM_UPDATEMEMBERLIST, 0, 0);
}

void MainWindow::SetCurrentChannelID(Snowflake channID)
{
	if (m_lastChannelID == channID)
		return;

	m_lastChannelID = channID;

	Channel* pChan = GetDiscordInstance()->GetChannelGlobally(channID);
	if (!pChan)
		return;

	Guild* pGuild = GetDiscordInstance()->GetGuild(pChan->m_parentGuild);
	if (!pGuild)
		return;

	Snowflake guildID = pGuild->m_snowflake;
	SetCurrentGuildID(guildID);

	ChatWindow::SetCurrentChannelID(channID);

	m_pMessageEditor->StopReply();
	m_pMessageEditor->Layout();

	m_pChannelView->OnUpdateSelectedChannel(channID);

	// repaint the message view
	m_pMessageList->ClearMessages();
	m_pMessageList->SetGuild(guildID);
	m_pMessageList->SetChannel(channID);

	m_pMessageList->UpdateAllowDrop();

	m_pMessageEditor->SetGuildID(guildID);
	m_pMessageEditor->SetChannelID(channID);

	m_pGuildHeader->SetGuildID(guildID);
	m_pGuildHeader->SetChannelID(channID);

	UpdateMainWindowTitle();

	if (IsWindowActive(m_hwnd))
		SetFocus(m_pMessageEditor->m_edit_hwnd);

	if (!m_pMessageList->GetCurrentChannel())
	{
		InvalidateRect(m_pMessageList->m_hwnd, NULL, TRUE);
		InvalidateRect(m_pGuildHeader->m_hwnd, NULL, FALSE);
		return;
	}

	GetDiscordInstance()->HandledChannelSwitch();

	m_pMessageList->RefetchMessages(m_pMessageList->GetMessageSentTo());

	InvalidateRect(m_pMessageList->m_hwnd, NULL, MessageList::MayErase());
	m_pGuildHeader->Update();
	m_pMessageEditor->UpdateTextBox();

	m_pStatusBar->SetChannelID(channID);

	// Update the message editor's typing indicators
	m_pStatusBar->ClearTypers();

	Snowflake myUserID = GetDiscordInstance()->GetUserID();
	TypingInfo& ti = m_typingInfo[channID];
	for (auto& tu : ti.m_typingUsers) {
		if (tu.second.m_key == myUserID)
			continue;
				
		m_pStatusBar->AddTypingName(tu.second.m_key, tu.second.m_startTimeMS / 1000ULL, tu.second.m_name);
	}
}

LRESULT MainWindow::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return GetMainWindow()->WindowProc(hWnd, uMsg, wParam, lParam);
}

void MainWindow::TryAgainTimer(HWND hWnd, UINT uMsg, UINT_PTR uTimerID, DWORD dwParam)
{
	if (uTimerID != m_TryAgainTimerId)
		return;

	KillTimer(hWnd, m_TryAgainTimer);
	m_TryAgainTimer = 0;

	GetDiscordInstance()->StartGatewaySession();
}

void CALLBACK MainWindow::TryAgainTimerStatic(HWND hWnd, UINT uMsg, UINT_PTR uTimerID, DWORD dwParam)
{
	return GetMainWindow()->TryAgainTimer(hWnd, uMsg, uTimerID, dwParam);
}

void MainWindow::TryConnectAgainIn(int time)
{
	m_TryAgainTimer = SetTimer(m_hwnd, m_TryAgainTimerId, time, TryAgainTimerStatic);
}

void MainWindow::ResetTryAgainInTime()
{
	m_TryAgainTimerInterval = 500;
}

void MainWindow::UpdateMainWindowTitle()
{
	std::string windowTitle;

	Channel* pChannel = m_pMessageList->GetCurrentChannel();
	if (pChannel)
		windowTitle += pChannel->GetTypeSymbol() + pChannel->m_name + " - ";

	windowTitle += TmGetString(IDS_PROGRAM_NAME);

	LPTSTR tstr = ConvertCppStringToTString(windowTitle);
	SetWindowText(m_hwnd, tstr);
	free(tstr);
}

void MainWindow::ProperlySizeControls()
{
	RECT rect = {}, rect2 = {}, rcClient = {}, rcSBar = {};
	GetClientRect(m_hwnd, &rect);
	int clientWidth = rect.right - rect.left;
	rcClient = rect;

	int scaled10 = ScaleByDPI(10);

	rect.left   += scaled10;
	rect.top    += scaled10;
	rect.right  -= scaled10;
	rect.bottom -= scaled10;

	HWND hWndMsg = m_pMessageList->m_hwnd;
	HWND hWndChv = m_pChannelView->m_hwnd;
	HWND hWndPfv = m_pProfileView->m_hwnd;
	HWND hWndGuh = m_pGuildHeader->m_hwnd;
	HWND hWndGul = m_pGuildLister->m_hwnd;
	HWND hWndMel = m_pMemberList->m_mainHwnd;
	HWND hWndTin = m_pMessageEditor->m_hwnd;
	HWND hWndStb = m_pStatusBar->m_hwnd;

	int statusBarHeight = 0;
	GetChildRect(m_hwnd, hWndStb, &rcSBar);
	statusBarHeight = rcSBar.bottom - rcSBar.top;

	rect.bottom -= statusBarHeight;
	rect2 = rect;

	g_ProfilePictureSize   = ScaleByDPI(PROFILE_PICTURE_SIZE_DEF);
	m_ChannelViewListWidth = ScaleByDPI(CHANNEL_VIEW_LIST_WIDTH);
	m_BottomBarHeight      = ScaleByDPI(BOTTOM_BAR_HEIGHT);
	m_BottomInputHeight    = ScaleByDPI(BOTTOM_INPUT_HEIGHT);
	m_SendButtonWidth      = ScaleByDPI(SEND_BUTTON_WIDTH);
	m_SendButtonHeight     = ScaleByDPI(SEND_BUTTON_HEIGHT);
	m_GuildHeaderHeight    = ScaleByDPI(GUILD_HEADER_HEIGHT);
	m_GuildListerWidth     = ScaleByDPI(PROFILE_PICTURE_SIZE_DEF + 12) + GUILD_LISTER_BORDER_SIZE * 4;
	m_MemberListWidth      = ScaleByDPI(MEMBER_LIST_WIDTH);
	m_MessageEditorHeight  = ScaleByDPI(MESSAGE_EDITOR_HEIGHT);

	if (m_pMessageEditor)
		m_pMessageEditor->SetJumpPresentHeight(m_BottomBarHeight - m_MessageEditorHeight - scaled10);

	int guildListerWidth = m_GuildListerWidth;
	int channelViewListWidth = m_ChannelViewListWidth;
	if (GetLocalSettings()->ShowScrollBarOnGuildList()) {
		int offset = GetSystemMetrics(SM_CXVSCROLL);
		guildListerWidth += offset;
		channelViewListWidth -= offset;
	}

	bool bRepaintGuildHeader = m_GuildListerWidth2 != guildListerWidth;
	m_GuildListerWidth2 = guildListerWidth;
	m_ChannelViewListWidth2 = channelViewListWidth;

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
			- m_GuildListerWidth2; /* The guild list itself. So just the channelview, messageview and memberlist summed */

		int widthOfAll3ThingsAt800px = ScaleByDPI(694);

		m_ChannelViewListWidth2 = MulDiv(m_ChannelViewListWidth2, widthOfAll3Things, widthOfAll3ThingsAt800px);
		m_MemberListWidth       = MulDiv(m_MemberListWidth,       widthOfAll3Things, widthOfAll3ThingsAt800px);
		channelViewListWidth = m_ChannelViewListWidth2;
	}

	bool bRepaint = true;

	// Create a message list
	rect.left += guildListerWidth + scaled10;
	rect.bottom -= m_MessageEditorHeight + m_pMessageEditor->ExpandedBy() + scaled10;
	rect.top += m_GuildHeaderHeight;
	if (m_bChannelListVisible) rect.left += channelViewListWidth + scaled10;
	if (m_bMemberListVisible) rect.right -= m_MemberListWidth + scaled10;
	MoveWindow(hWndMsg, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, bRepaint);

	rect.left = rect.right + scaled10;
	if (!m_bMemberListVisible) rect.left -= m_MemberListWidth;
	rect.right = rect.left + m_MemberListWidth;
	rect.bottom = rect2.bottom;
	MoveWindow(hWndMel, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, bRepaint);
	rect = rect2;

	rect.left += guildListerWidth + scaled10;
	rect.right = rect.left + channelViewListWidth;
	rect.bottom -= m_BottomBarHeight;
	rect.top += m_GuildHeaderHeight;
	MoveWindow(hWndChv, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, bRepaint);
	rect = rect2;

	rect.left += guildListerWidth + scaled10;
	rect.top = rect.bottom - m_BottomBarHeight + scaled10;
	rect.right = rect.left + channelViewListWidth;
	MoveWindow(hWndPfv, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, TRUE);
	rect = rect2;

	rect.left   = scaled10 + guildListerWidth + scaled10;
	rect.top    = rect.bottom - m_MessageEditorHeight - m_pMessageEditor->ExpandedBy();
	if (m_bChannelListVisible) rect.left += channelViewListWidth + scaled10;
	if (m_bMemberListVisible) rect.right -= m_MemberListWidth + scaled10;
	int textInputHeight = rect.bottom - rect.top, textInputWidth = rect.right - rect.left;
	MoveWindow(hWndTin, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, bRepaint);
	rect = rect2;

	rect.left += guildListerWidth + scaled10;
	rect.bottom = rect.top + m_GuildHeaderHeight - scaled10;
	MoveWindow(hWndGuh, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, bRepaintGuildHeader);
	rect = rect2;

	rect.right = rect.left + guildListerWidth;
	MoveWindow(hWndGul, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, bRepaint);
	rect = rect2;

	// Forward the resize event to the status bar.
	MoveWindow(hWndStb, 0, rcClient.bottom - statusBarHeight, rcClient.right - rcClient.left, statusBarHeight, TRUE);
	// Erase the old rectangle
	InvalidateRect(m_hwnd, &rcSBar, TRUE);

	if (bFullRepaintProfileView)
		InvalidateRect(hWndPfv, NULL, FALSE);

	m_pStatusBar->UpdateParts(rcClient.right - rcClient.left);
}

void MainWindow::AddOrRemoveAppFromStartup()
{
	HKEY hkey = NULL;
	RegCreateKey(HKEY_CURRENT_USER, TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), &hkey);

	LPCTSTR value = TmGetTString(IDS_PROGRAM_NAME);

	if (GetLocalSettings()->GetOpenOnStartup())
	{
		TCHAR tPath[MAX_PATH];
		const DWORD length = GetModuleFileName(NULL, tPath, MAX_PATH);
		
		const std::string sPath = "\"" + MakeStringFromTString(tPath) + "\" " + std::string(g_StartupArg);
		const LPTSTR finalPath = ConvertCppStringToTString(sPath);

		RegSetValueEx(hkey, value, 0, REG_SZ, (BYTE *)finalPath, (DWORD)((_tcslen(finalPath) + 1) * sizeof(TCHAR)));
	}
	else
	{
		RegDeleteValue(hkey, value);
	}
}

void MainWindow::CloseCleanup()
{
	KillImageViewer();
	ProfilePopout::Dismiss();
	AutoComplete::DismissAutoCompleteWindowsIfNeeded(m_hwnd);
	m_pLoadingMessage->Hide();

	while (!m_subWindows.empty())
		m_subWindows[0]->Close();
}

void MainWindow::OnUpdateAvatar(const std::string& resid)
{
	// depending on where this is, update said control
	ImagePlace ip = GetAvatarCache()->GetPlace(resid);

	switch (ip.type)
	{
		case eImagePlace::AVATARS:
		{
			if (GetDiscordInstance()->GetUserID() == ip.sf)
				SendMessage(m_hwnd, WM_REPAINTPROFILE, 0, 0);

			if (ProfilePopout::GetUser() == ip.sf)
				SendMessage(m_hwnd, WM_UPDATEPROFILEPOPOUT, 0, 0);

			Snowflake sf = ip.sf;
			SendMessage(m_hwnd, WM_UPDATEUSER, 0, (LPARAM) &sf);
			break;
		}
		case eImagePlace::ICONS:
		{
			SendMessage(m_hwnd, WM_REPAINTGUILDLIST, 0, (LPARAM)&ip.sf);
			break;
		}
		case eImagePlace::ATTACHMENTS:
		{
			SendMessage(m_hwnd, WM_UPDATEATTACHMENT, 0, (LPARAM) &ip);
			break;
		}
		case eImagePlace::EMOJIS:
		{
			SendMessage(m_hwnd, WM_UPDATEEMOJI, 0, (LPARAM) &ip.sf);
			break;
		}
	}
}

int MainWindow::OnHTTPError(const std::string& url, const std::string& reasonString, bool isSSL)
{
	LPTSTR urlt = ConvertCppStringToTString(url);
	LPTSTR rstt = ConvertCppStringToTString(reasonString);
	LPCTSTR title;
	static TCHAR buffer[8192];

	if (isSSL) {
		title = TmGetTString(IDS_SSL_ERROR_TITLE);
		_tcscpy(buffer, TmGetTString(IDS_SSL_ERROR_1));
		_tcscat(buffer, urlt);
		_tcscat(buffer, TEXT("\n\n"));
		_tcscat(buffer, rstt);
		_tcscat(buffer, TEXT("\n\n"));
		_tcscat(buffer, TmGetTString(IDS_SSL_ERROR_2));
		_tcscat(buffer, TEXT("\n\n"));
		_tcscat(buffer, TmGetTString(IDS_SSL_ERROR_3));
	}
	else {
		title = TmGetTString(IDS_CONNECT_ERROR_TITLE);
		_tcscpy(buffer, TmGetTString(IDS_CONNECT_ERROR_1));
		_tcscat(buffer, urlt);
		_tcscat(buffer, TEXT("\n\n"));
		_tcscat(buffer, rstt);
		_tcscat(buffer, TEXT("\n\n"));
		_tcscat(buffer, TmGetTString(IDS_CONNECT_ERROR_2));
		_tcscat(buffer, TEXT("\n\n"));
		_tcscat(buffer, TmGetTString(IDS_CONNECT_ERROR_3));
	}

	free(urlt);
	free(rstt);
	
	size_t l = _tcslen(buffer);
	return MessageBox(m_hwnd, buffer, title, (isSSL ? MB_ABORTRETRYIGNORE : MB_RETRYCANCEL) | MB_ICONWARNING);
}
