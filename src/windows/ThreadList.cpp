#include <nlohmann/json.h>
#include "ThreadList.hpp"
#include "MessageList.hpp"

#define DM_THREAD_LIST_CLASS TEXT("DMThreadListClass");

Snowflake ThreadList::m_channel, ThreadList::m_guild;
POINT ThreadList::m_appearXY;
bool ThreadList::m_bRightJustify;
bool ThreadList::m_bActive;
HWND ThreadList::m_hwnd;

bool ThreadList::IsActive()
{
	return m_bActive;
}

bool ThreadList::IsFocused()
{
	return GetForegroundWindow() == m_hwnd && !IsIconic(m_hwnd);
}

void ThreadList::Initialize(HWND hWnd)
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

	m_bActive = true;
	m_hwnd = hWnd;
}

INT_PTR CALLBACK ThreadList::DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
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
			//OnClickMessage(*(Snowflake*) lParam);
			return TRUE;

		case WM_DESTROY:
			m_bActive = false;
			m_hwnd = NULL;
			break;
	}

	return FALSE;
}

void ThreadList::Show(Snowflake channelID, Snowflake guildID, int x, int y, bool rightJustify)
{
	m_channel = channelID;
	m_guild = guildID;
	m_appearXY = { x, y };
	m_bRightJustify = rightJustify;

	DialogBox(g_hInstance, MAKEINTRESOURCE(DMDI(IDD_DIALOG_PINNEDMESSAGES)), g_Hwnd, &DlgProc);
}
