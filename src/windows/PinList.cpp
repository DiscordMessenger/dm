#include <nlohmann/json.h>
#include "PinList.hpp"
#include "MessageList.hpp"

#define DM_PINNED_MESSAGES_CLASS TEXT("DMPinnedMessageListClass");

Snowflake PinList::m_channel, PinList::m_guild;
MessageList* PinList::m_pMessageList;
PinnedMap PinList::m_map;
POINT PinList::m_appearXY;
bool PinList::m_bRightJustify;
bool PinList::m_bActive;

bool PinList::IsActive()
{
	return m_bActive;
}

void PinList::AddMessage(Snowflake channelID, const Message& msg)
{
	m_map[channelID].push_back(msg);

	if (m_channel == channelID && m_pMessageList)
		m_pMessageList->AddMessage(msg);
}

void PinList::Initialize(HWND hWnd)
{
	Channel* pChan = GetDiscordInstance()->GetChannelGlobally(m_channel);
	if (!pChan) {
		EndDialog(hWnd, 0);
		return;
	}

	char buff[4096];
	snprintf(buff, _countof(buff), TmGetString(IDS_PINNED_MESSAGES_IN).c_str(), pChan->m_name.c_str());
	TCHAR* tchr = ConvertCppStringToTString(buff);
	SetWindowText(hWnd, tchr);
	free(tchr);

	// Move the window
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

	HWND child = GetDlgItem(hWnd, IDC_MESSAGE_LIST);
	
	GetWindowRect(child, &rect);
	ScreenToClientRect(hWnd, &rect);
	DestroyWindow(child);

	SAFE_DELETE(m_pMessageList);

	if (m_map[m_channel].empty()) {
		Message msg;
		msg.m_type = MessageType::LOADING_PINNED_MESSAGES;
		msg.m_author = TmGetString(IDS_PLEASE_WAIT);
		msg.m_snowflake = 1;
		AddMessage(m_channel, msg);

		GetDiscordInstance()->RequestPinnedMessages(m_channel);
	}

	m_pMessageList = MessageList::Create(hWnd, &rect);
	m_pMessageList->SetManagedByOwner(true);
	m_pMessageList->SetTopDown(true);
	m_pMessageList->SetGuild(m_guild);
	m_pMessageList->SetChannel(m_channel);

	for (auto& msg : m_map[m_channel])
		m_pMessageList->AddMessage(msg);

	m_bActive = true;
}

void PinList::OnLoadedPins(Snowflake channelID, const std::string& data)
{
	nlohmann::json j = nlohmann::json::parse(data);
	
	if (!j.is_array()) {
		assert(!"uh oh");
		return;
	}

	m_map[m_channel].clear();
	if (m_channel == channelID)
		m_pMessageList->ClearMessages();

	if (j.empty()) {
		Message msg;
		msg.m_author = " ";
		msg.m_type = MessageType::NO_PINNED_MESSAGES;
		msg.m_anchor = 1;
		msg.m_snowflake = 1;
		AddMessage(channelID, msg);
	}
	else
	{
		for (auto& msgo : j)
		{
			Message msg;
			msg.Load(msgo, m_guild);
			msg.m_anchor = 1;
			AddMessage(channelID, msg);
		}
	}

	// HACK
	m_pMessageList->Repaint();
}

void PinList::OnUpdateEmbed(const std::string& key)
{
	if (m_pMessageList)
		m_pMessageList->OnUpdateEmbed(key);
}

void PinList::OnUpdateAvatar(Snowflake key)
{
	if (m_pMessageList)
		m_pMessageList->OnUpdateAvatar(key);
}

void PinList::OnUpdateEmoji(Snowflake key)
{
	if (m_pMessageList)
		m_pMessageList->OnUpdateEmoji(key);
}

void PinList::OnClickMessage(Snowflake sf)
{
	GetDiscordInstance()->JumpToMessage(m_guild, m_channel, sf);
}

INT_PTR CALLBACK PinList::DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_INITDIALOG:
			Initialize(hWnd);
			return TRUE;

		case WM_COMMAND:
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

void PinList::Show(Snowflake channelID, Snowflake guildID, int x, int y, bool rightJustify)
{
	m_channel = channelID;
	m_guild = guildID;
	m_appearXY = { x, y };
	m_bRightJustify = rightJustify;

	DialogBox(g_hInstance, MAKEINTRESOURCE(DMDI(IDD_DIALOG_PINNEDMESSAGES)), g_Hwnd, &DlgProc);
}
