#include "GuildSubWindow.hpp"
#include "Main.hpp"

#include "MessageList.hpp"
#include "MemberList.hpp"
#include "IChannelView.hpp"
#include "MessageEditor.hpp"
#include "StatusBar.hpp"
#include "ProfilePopout.hpp"
#include "UploadDialog.hpp"

#include "../discord/LocalSettings.hpp"

#define GUILD_SUB_WINDOW_CLASS TEXT("DiscordMessengerGuildSubWindowClass")

WNDCLASS GuildSubWindow::m_wndClass {};
bool GuildSubWindow::InitializeClass()
{
	WNDCLASS& wc = m_wndClass;

	wc.lpfnWndProc   = WndProc;
	wc.hInstance     = g_hInstance;
	wc.lpszClassName = GUILD_SUB_WINDOW_CLASS;
	wc.hbrBackground = g_backgroundBrush;
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon         = g_Icon;
	//wc.lpszMenuName  = MAKEINTRESOURCE(IDR_MAINMENU);

	return RegisterClass(&wc) != 0;
}

GuildSubWindow::GuildSubWindow()
{
	m_hwnd = CreateWindow(
		GUILD_SUB_WINDOW_CLASS,
		TEXT("Discord Messenger (sub-window)"),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		ScaleByDPI(800), // TODO: Specify non-hardcoded size
		ScaleByDPI(550),
		NULL,
		NULL,
		g_hInstance,
		this
	);

	if (!m_hwnd) {
		m_bInitFailed = true;
		return;
	}

	ShowWindow(m_hwnd, SW_SHOWNORMAL);
}

GuildSubWindow::~GuildSubWindow()
{
	DbgPrintW("GuildSubWindow destructor!");
}

void GuildSubWindow::Close()
{
	DestroyWindow(m_hwnd);
}

void GuildSubWindow::ProperlySizeControls()
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
	//HWND hWndMel = m_pMemberList->m_mainHwnd;
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
	m_GuildHeaderHeight    = 0; //ScaleByDPI(GUILD_HEADER_HEIGHT);
	m_MemberListWidth      = ScaleByDPI(MEMBER_LIST_WIDTH);
	m_MessageEditorHeight  = ScaleByDPI(MESSAGE_EDITOR_HEIGHT);

	if (m_pMessageEditor)
		m_pMessageEditor->SetJumpPresentHeight(m_BottomBarHeight - m_MessageEditorHeight - scaled10);

	int channelViewListWidth = m_ChannelViewListWidth;
	m_ChannelViewListWidth2 = channelViewListWidth;

	// May need to do some adjustments.
	const int minFullWidth = ScaleByDPI(700);
	if (clientWidth < minFullWidth)
	{
		int widthOfAll3Things =
			clientWidth
			- scaled10 * 3  /* Left edge, Right edge, Between GuildLister and the group */
			- scaled10 * 2; /* Between channelview and messageview, between messageview and memberlist */

		int widthOfAll3ThingsAt800px = ScaleByDPI(648);

		m_ChannelViewListWidth2 = MulDiv(m_ChannelViewListWidth2, widthOfAll3Things, widthOfAll3ThingsAt800px);
		m_MemberListWidth       = MulDiv(m_MemberListWidth,       widthOfAll3Things, widthOfAll3ThingsAt800px);
		channelViewListWidth = m_ChannelViewListWidth2;
	}

	bool bRepaint = true;

	// Create a message list
	rect.bottom -= m_MessageEditorHeight + m_pMessageEditor->ExpandedBy() + scaled10;
	rect.top += m_GuildHeaderHeight;
	if (m_bChannelListVisible) rect.left += channelViewListWidth + scaled10;
	if (m_bMemberListVisible) rect.right -= m_MemberListWidth + scaled10;
	MoveWindow(hWndMsg, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, bRepaint);

	//rect.left = rect.right + scaled10;
	//if (!m_bMemberListVisible) rect.left -= m_MemberListWidth;
	//rect.right = rect.left + m_MemberListWidth;
	//rect.bottom = rect2.bottom;
	//MoveWindow(hWndMel, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, bRepaint);
	rect = rect2;

	rect.left = scaled10;
	rect.right = rect.left + channelViewListWidth;
	rect.top += m_GuildHeaderHeight;
	MoveWindow(hWndChv, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, bRepaint);
	rect = rect2;

	rect.left   = scaled10;
	rect.top    = rect.bottom - m_MessageEditorHeight - m_pMessageEditor->ExpandedBy();
	if (m_bChannelListVisible) rect.left += channelViewListWidth + scaled10;
	if (m_bMemberListVisible) rect.right -= m_MemberListWidth + scaled10;
	int textInputHeight = rect.bottom - rect.top, textInputWidth = rect.right - rect.left;
	MoveWindow(hWndTin, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, bRepaint);
	rect = rect2;

	// Forward the resize event to the status bar.
	MoveWindow(hWndStb, 0, rcClient.bottom - statusBarHeight, rcClient.right - rcClient.left, statusBarHeight, TRUE);
	// 
	// Erase the old rectangle
	InvalidateRect(m_hwnd, &rcSBar, TRUE);

	m_pStatusBar->UpdateParts(rcClient.right - rcClient.left);
}

LRESULT GuildSubWindow::HandleCommand(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (LOWORD(wParam))
	{
		case IDA_PASTE:
		{
			ShowPasteFileDialog(m_pMessageEditor->m_edit_hwnd);
			break;
		}
	}

	return 0;
}

LRESULT GuildSubWindow::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_SHOWUPLOADDIALOG:
		{
			Snowflake* sf = (Snowflake*)lParam;
			UploadDialogShow2(hWnd, *sf);
			delete sf;
			break;
		}
		case WM_CREATE:
		{
			m_hwnd = hWnd;
			m_bMemberListVisible = false;
			m_bChannelListVisible = true;

			RECT rect { 0, 0, 10, 10 };
			m_pMessageList = MessageList::Create(this, NULL, &rect);
			m_pMessageEditor = MessageEditor::Create(this, &rect);
			//m_pMemberList = IMemberList::CreateMemberList(this, &rect);
			m_pChannelView = IChannelView::CreateChannelView(this, &rect);
			m_pStatusBar = StatusBar::Create(this);

			if (//!m_pMemberList ||
				!m_pMessageList ||
				!m_pChannelView ||
				!m_pMessageEditor)
			{
				DbgPrintW("ERROR: A control failed to be created!");
				m_bInitFailed = true;
				break;
			}

			if (m_pMessageEditor)
				m_pMessageEditor->SetMessageList(m_pMessageList);

			break;
		}
		case WM_CLOSE:
		{
			DestroyWindow(hWnd);
			break;
		}
		case WM_COMMAND:
			return HandleCommand(hWnd, uMsg, wParam, lParam);
			
		case WM_DESTROY:
		{
			SAFE_DELETE(m_pChannelView);
			SAFE_DELETE(m_pMessageList);
			//SAFE_DELETE(m_pMemberList);
			SAFE_DELETE(m_pMessageEditor);
			SAFE_DELETE(m_pStatusBar);
			break;
		}
		case WM_UPDATEEMOJI:
		{
			Snowflake sf = *(Snowflake*)lParam;
			m_pMessageList->OnUpdateEmoji(sf);
			break;
		}
		case WM_UPDATEUSER:
		{
			Snowflake sf = *(Snowflake*)lParam;
			m_pMessageList->OnUpdateAvatar(sf);
			//m_pMemberList->OnUpdateAvatar(sf);
			m_pChannelView->OnUpdateAvatar(sf);
			break;
		}
		case WM_UPDATEMESSAGELENGTH:
		{
			m_pStatusBar->UpdateCharacterCounter(int(lParam), MAX_MESSAGE_SIZE);
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
		case WM_UPDATEMEMBERLIST:
		{
			//m_pMemberList->SetGuild(GetCurrentGuildID());
			//m_pMemberList->Update();
			break;
		}
		case WM_UPDATEATTACHMENT:
		{
			ImagePlace pl = *(ImagePlace*)lParam;
			m_pMessageList->OnUpdateEmbed(pl.key);
			break;
		}
		case WM_UPDATECHANACKS:
		{
			Snowflake* sfs = (Snowflake*)lParam;

			Snowflake channelID = sfs[0];
			Snowflake messageID = sfs[1];

			Channel* pChan = GetDiscordInstance()->GetChannelGlobally(channelID);

			// Update the icon for the specific guild
			//m_pGuildLister->RedrawIconForGuild(pChan->m_parentGuild);

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
		case WM_ADDMESSAGE:
		{
			AddMessageParams* pParms = (AddMessageParams*)lParam;

			if (m_pMessageList->GetCurrentChannelID() == pParms->channel)
				m_pMessageList->AddMessage(pParms->msg.m_snowflake, GetForegroundWindow() == hWnd);

			OnStopTyping(pParms->channel, pParms->msg.m_author_snowflake);
			break;
		}
		case WM_UPDATEMESSAGE:
		{
			AddMessageParams* pParms = (AddMessageParams*)lParam;

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
		case WM_SIZE:
		{
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
		case WM_REFRESHMESSAGES:
		{
			RefreshMessagesParams* params = (RefreshMessagesParams*)lParam;

			if (m_pMessageList->GetCurrentChannelID() == params->m_channelId)
				m_pMessageList->RefetchMessages(params->m_gapCulprit, true);

			break;
		}
		case WM_REPOSITIONEVERYTHING:
		{
			ProperlySizeControls();
			break;
		}
		case WM_STARTTYPING:
		{
			TypingParams params = *((TypingParams*)lParam);
			OnTyping(params.m_guild, params.m_channel, params.m_user, params.m_timestamp);
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
	}

	return 0;
}

LRESULT GuildSubWindow::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	GuildSubWindow* pThis = (GuildSubWindow*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if (!pThis && uMsg != WM_NCCREATE)
		return DefWindowProc(hWnd, uMsg, wParam, lParam);

	if (uMsg == WM_NCCREATE)
	{
		CREATESTRUCT* strct = (CREATESTRUCT*) lParam;
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)strct->lpCreateParams);
	}

	if (pThis)
	{
		LRESULT lres = pThis->WindowProc(hWnd, uMsg, wParam, lParam);
		if (lres != 0)
			return lres;
	}

	if (uMsg == WM_DESTROY)
	{
		GetMainWindow()->OnClosedWindow(pThis);
		SetWindowLongPtr(hWnd, GWLP_USERDATA, 0);
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void GuildSubWindow::UpdateWindowTitle()
{
	std::string windowTitle;

	Channel* pChannel = GetCurrentChannel();
	if (pChannel)
		windowTitle += pChannel->GetTypeSymbol() + pChannel->m_name + " - ";

	Guild* pGuild = GetCurrentGuild();
	if (pGuild && pGuild->m_snowflake != 0)
		windowTitle += pGuild->m_name + " - ";

	windowTitle += TmGetString(IDS_PROGRAM_NAME);

	LPTSTR tstr = ConvertCppStringToTString(windowTitle);
	SetWindowText(m_hwnd, tstr);
	free(tstr);
}

void GuildSubWindow::OnTyping(Snowflake guildID, Snowflake channelID, Snowflake userID, time_t timeStamp)
{
	if (channelID == GetCurrentChannelID())
	{
		if (userID == GetDiscordInstance()->GetUserID()) {
			// Ignore typing start from ourselves for the lower bar
			return;
		}

		TypingInfo& ti = GetMainWindow()->GetTypingInfo(channelID);
		TypingUser& tu = ti.m_typingUsers[userID];

		m_pStatusBar->AddTypingName(userID, timeStamp, tu.m_name);
	}
}

void GuildSubWindow::OnStopTyping(Snowflake channelID, Snowflake userID)
{
	if (channelID == GetCurrentChannelID())
		m_pStatusBar->RemoveTypingName(userID);
}

void GuildSubWindow::SetCurrentGuildID(Snowflake sf)
{
	if (m_lastGuildID == sf)
		return;

	m_lastGuildID = sf;

	ChatWindow::SetCurrentGuildID(sf);

	// repaint the guild lister
	//m_pGuildLister->UpdateSelected();
	//m_pGuildHeader->Update();

	SendMessage(m_hwnd, WM_UPDATECHANLIST, 0, 0);
	SendMessage(m_hwnd, WM_UPDATEMEMBERLIST, 0, 0);
}

void GuildSubWindow::SetCurrentChannelID(Snowflake channID)
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

	//m_pGuildHeader->SetGuildID(guildID);
	//m_pGuildHeader->SetChannelID(channID);

	UpdateWindowTitle();

	if (IsWindowActive(m_hwnd))
		SetFocus(m_pMessageEditor->m_edit_hwnd);

	if (!m_pMessageList->GetCurrentChannel())
	{
		InvalidateRect(m_pMessageList->m_hwnd, NULL, TRUE);
		//InvalidateRect(m_pGuildHeader->m_hwnd, NULL, FALSE);
		return;
	}

	GetDiscordInstance()->HandledChannelSwitch();

	m_pMessageList->RefetchMessages(m_pMessageList->GetMessageSentTo());

	InvalidateRect(m_pMessageList->m_hwnd, NULL, MessageList::MayErase());
	//m_pGuildHeader->Update();
	m_pMessageEditor->UpdateTextBox();

	// Update the message editor's typing indicators
	m_pStatusBar->SetChannelID(channID);
	m_pStatusBar->ClearTypers();

	Snowflake myUserID = GetDiscordInstance()->GetUserID();
	TypingInfo& ti = GetMainWindow()->GetTypingInfo(channID);
	for (auto& tu : ti.m_typingUsers) {
		if (tu.second.m_key == myUserID)
			continue;
				
		m_pStatusBar->AddTypingName(tu.second.m_key, tu.second.m_startTimeMS / 1000ULL, tu.second.m_name);
	}
}
