#include "NotificationViewer.hpp"
#include "MessageList.hpp"
#include "Main.hpp"

MessageList* NotificationViewer::m_pMessageList;
POINT NotificationViewer::m_appearXY;
bool NotificationViewer::m_bRightJustify;
bool NotificationViewer::m_bActive;

bool NotificationViewer::IsActive()
{
	return m_bActive;
}

void NotificationViewer::Show(int x, int y, bool rightJustify)
{
	m_appearXY = { x, y };
	m_bRightJustify = rightJustify;

	DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_DIALOG_NOTIFICATIONS), g_Hwnd, DlgProc);
}

void NotificationViewer::Initialize(HWND hWnd)
{
	RECT rect{};
	GetWindowRect(hWnd, &rect);
	int width = rect.right - rect.left;
	int height = rect.bottom - rect.top;
	int xPos = m_appearXY.x;
	int yPos = m_appearXY.y;
	if (m_bRightJustify) {
		xPos -= width;
	}
	MoveWindow(hWnd, xPos, yPos, width, height, false);

	SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)LoadIcon(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_NOTIFICATION))));

	HWND child = GetDlgItem(hWnd, IDC_NOTIFS_MARK_AS_READ);
	int sm = GetSystemMetrics(SM_CXSMICON);
	HICON hico = (HICON)ri::LoadImage(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_MARK_READ)), IMAGE_ICON, sm, sm, LR_SHARED | LR_CREATEDIBSECTION);
	SendMessage(child, BM_SETIMAGE, IMAGE_ICON, (LPARAM) hico);

	child = GetDlgItem(hWnd, IDC_MESSAGE_LIST);

	GetWindowRect(child, &rect);
	ScreenToClientRect(hWnd, &rect);
	DestroyWindow(child);

	SAFE_DELETE(m_pMessageList);

	m_pMessageList = MessageList::Create(hWnd, &rect);
	m_pMessageList->SetManagedByOwner(true);
	m_pMessageList->SetTopDown(true);
	m_pMessageList->SetGuild(0);
	m_pMessageList->SetChannel(0);

	const auto& notifs = GetNotificationManager()->GetNotifications();
	Message msg;
	if (notifs.empty())
	{
		msg.m_type = MessageType::NO_NOTIFICATIONS;
		msg.m_snowflake = 1;
		m_pMessageList->AddMessage(msg);
	}
	else for (const auto& notif : notifs)
	{
		msg.m_type      = MessageType::DEFAULT;
		msg.m_avatar    = notif.m_avatarLnk;
		msg.m_message   = notif.m_contents;
		msg.m_snowflake = notif.m_sourceMessage;
		msg.m_anchor    = notif.m_sourceChannel;
		msg.m_bRead     = notif.m_bRead;
		msg.SetTime(notif.m_timeReceived);

		// special handling for the author
		std::string guildName = "Unknown", channelName = "Unknown";
		Guild* pGuild = GetDiscordInstance()->GetGuild(notif.m_sourceGuild);
		if (pGuild) {
			guildName = pGuild->m_name;
			Channel* pChan = pGuild->GetChannel(notif.m_sourceChannel);
			if (pChan) {
				if (pChan->m_channelType == Channel::GROUPDM)
					channelName = "\"" + pChan->m_name + "\"";
				if (pChan->m_channelType == Channel::DM)
					channelName = "";
				else
					channelName = pChan->GetTypeSymbol() + pChan->m_name;
			}
		}

		std::string details = "";

		msg.m_author = notif.m_author;
		
		details = notif.m_bIsReply ? "replied " : "";
		if (!channelName.empty()) {
			details += "in " + channelName + " ";
		}
		if (notif.m_sourceGuild) {
			details += "(" + guildName + ")";
		}

		msg.m_dateFull    = details + " - " + msg.m_dateFull;
		msg.m_dateCompact = details + " - " + msg.m_dateCompact;
		msg.m_dateOnly    = details + " - " + msg.m_dateOnly;

		m_pMessageList->AddMessage(msg);
	}

	m_bActive = false;
}

void NotificationViewer::OnResize(HWND hWnd, int newWidth, int newHeight)
{
	HWND hLabel = GetDlgItem(hWnd, IDC_NOTIFICATION_HINT);
	HWND hButton = GetDlgItem(hWnd, IDC_NOTIFS_MARK_AS_READ);
	HWND hList = GetDlgItem(hWnd, CID_MESSAGELIST);

	RECT rcLabel{};
	GetChildRect(hWnd, hLabel, &rcLabel);
	RECT rcList{};
	GetChildRect(hWnd, hList, &rcList);
	RECT rcButton{};
	GetChildRect(hWnd, hButton, &rcButton);

	MoveWindow(
		hButton,
		newWidth - rcLabel.left - (rcButton.right - rcButton.left),
		rcButton.top,
		rcButton.right - rcButton.left,
		rcButton.bottom - rcButton.top,
		TRUE
	);
	InvalidateRect(hLabel, NULL, FALSE);

	MoveWindow(
		hLabel,
		rcLabel.left,
		rcLabel.top,
		newWidth - rcLabel.left * 2 - (rcButton.right - rcButton.left),
		rcLabel.bottom - rcLabel.top,
		TRUE
	);
	InvalidateRect(hLabel, NULL, FALSE);

	MoveWindow(
		hList,
		rcList.left,
		rcList.top,
		newWidth - rcList.left * 2,
		newHeight - rcList.top - rcLabel.top,
		TRUE
	);
	InvalidateRect(hList, NULL, FALSE);
}

void NotificationViewer::OnClickMessage(Snowflake sf)
{
	// Look for the notification with this ID.
	auto& notifs = GetNotificationManager()->GetNotifications();

	for (auto& notif : notifs)
	{
		if (notif.m_sourceMessage != sf)
			continue;

		// Go there
		GetDiscordInstance()->JumpToMessage(
			notif.m_sourceGuild,
			notif.m_sourceChannel,
			notif.m_sourceMessage
		);

		// Mark it as read:
		notif.m_bRead = true;
	}
}

void NotificationViewer::MarkAllAsRead()
{
	auto& notifs = GetNotificationManager()->GetNotifications();
	for (auto& notif : notifs)
		notif.m_bRead = true;
}

BOOL NotificationViewer::DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_INITDIALOG:
			Initialize(hWnd);
			return TRUE;

		case WM_SIZE:
			OnResize(hWnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			break;

		case WM_GETMINMAXINFO:
		{
			LPMINMAXINFO lmmi = (LPMINMAXINFO) lParam;
			lmmi->ptMinTrackSize = { 400, 300 };
			return TRUE;
		}

		case WM_COMMAND:
			if (wParam == IDC_NOTIFS_MARK_AS_READ) {
				// Mark all notifications as read and exit.
				MarkAllAsRead();
				EndDialog(hWnd, 0);
				return TRUE;
			}
			if (wParam == IDCANCEL) {
				EndDialog(hWnd, 0);
				return TRUE;
			}
			break;

		case WM_CLICKEDMESSAGE:
			EndDialog(hWnd, 0);
			OnClickMessage(*(Snowflake*) lParam);
			return TRUE;

		case WM_DESTROY:
			SAFE_DELETE(m_pMessageList);
			m_bActive = false;
			break;
	}

	return FALSE;
}
