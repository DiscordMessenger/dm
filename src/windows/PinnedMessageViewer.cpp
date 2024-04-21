#include <nlohmann/json.h>
#include "PinnedMessageViewer.hpp"
#include "MessageList.hpp"

#define DM_PINNED_MESSAGES_CLASS TEXT("DMPinnedMessageListClass");

static Snowflake g_channel, g_guild;
static POINT g_appearXY;
static bool g_rightJustify;
static MessageList* g_pPmvMessageList;
static std::map <Snowflake, std::vector<Message> > g_pinnedMessageMap;
static bool g_bActive;

bool PmvIsActive()
{
	return g_bActive;
}

void PmvAddMessage(Snowflake channelID, const Message& msg)
{
	g_pinnedMessageMap[channelID].push_back(msg);

	if (g_channel == channelID && g_pPmvMessageList)
		g_pPmvMessageList->AddMessage(msg);
}

void PmvInitialize(HWND hWnd)
{
	Channel* pChan = GetDiscordInstance()->GetChannelGlobally(g_channel);
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
	int xPos = g_appearXY.x;
	int yPos = g_appearXY.y;
	if (g_rightJustify) {
		xPos -= width;
	}
	MoveWindow(hWnd, xPos, yPos, width, height, false);

	HWND child = GetDlgItem(hWnd, IDC_MESSAGE_LIST);
	
	GetWindowRect(child, &rect);
	ScreenToClientRect(hWnd, &rect);
	DestroyWindow(child);

	if (g_pPmvMessageList)
		delete g_pPmvMessageList;

	g_pPmvMessageList = nullptr;

	if (g_pinnedMessageMap[g_channel].empty()) {
		Message msg;
		msg.m_type = MessageType::LOADING_PINNED_MESSAGES;
		msg.m_author = TmGetString(IDS_PLEASE_WAIT);
		msg.m_snowflake = 1;
		PmvAddMessage(g_channel, msg);

		GetDiscordInstance()->RequestPinnedMessages(g_channel);
	}

	g_pPmvMessageList = MessageList::Create(hWnd, &rect);
	g_pPmvMessageList->SetManagedByOwner(true);
	g_pPmvMessageList->SetTopDown(true);
	g_pPmvMessageList->SetGuild(g_guild);
	g_pPmvMessageList->SetChannel(g_channel);

	for (auto& msg : g_pinnedMessageMap[g_channel])
		g_pPmvMessageList->AddMessage(msg);

	g_bActive = true;
}

void PmvOnLoadedPins(Snowflake channelID, const std::string& data)
{
	nlohmann::json j = nlohmann::json::parse(data);
	
	if (!j.is_array()) {
		assert(!"uh oh");
		return;
	}

	g_pinnedMessageMap[g_channel].clear();
	if (g_channel == channelID)
		g_pPmvMessageList->ClearMessages();

	if (j.empty()) {
		Message msg;
		msg.m_author = " ";
		msg.m_type = MessageType::NO_PINNED_MESSAGES;
		msg.m_anchor = 1;
		msg.m_snowflake = 1;
		PmvAddMessage(channelID, msg);
	}
	else
	{
		for (auto& msgo : j)
		{
			Message msg;
			msg.Load(msgo, g_guild);
			msg.m_anchor = 1;
			PmvAddMessage(channelID, msg);
		}
	}

	// HACK
	g_pPmvMessageList->Repaint();
}

void PmvOnClickMessage(Snowflake sf)
{
	GetDiscordInstance()->JumpToMessage(g_guild, g_channel, sf);
}

BOOL CALLBACK PmvDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_INITDIALOG:
			PmvInitialize(hWnd);
			return TRUE;

		case WM_COMMAND:
			if (wParam == IDCANCEL) {
				EndDialog(hWnd, 0);
				return TRUE;
			}
			break;

		case WM_CLICKEDMESSAGE:
			EndDialog(hWnd, 0);
			PmvOnClickMessage(*(Snowflake*) lParam);
			return TRUE;

		case WM_DESTROY:
			if (g_pPmvMessageList)
				delete g_pPmvMessageList;
			g_pPmvMessageList = nullptr;
			g_bActive = false;
			break;
	}

	return FALSE;
}

void PmvCreate(Snowflake channelID, Snowflake guildID, int x, int y, bool rightJustify)
{
	g_channel = channelID;
	g_guild = guildID;
	g_appearXY = { x, y };
	g_rightJustify = rightJustify;

	DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_DIALOG_PINNEDMESSAGES), g_Hwnd, PmvDlgProc);
}
