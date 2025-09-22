#include "ChannelViewOld.hpp"

WNDCLASS ChannelViewOld::g_ChannelViewLegacyClass;

ChannelViewOld* ChannelViewOld::Create(ChatWindow* parent, LPRECT rect)
{
	ChannelViewOld* view = new ChannelViewOld;
	view->m_pParent = parent;
	view->m_hwndParent = parent->GetHWND();

	view->m_hwnd = CreateWindowEx(
		0,
		T_CHANNEL_VIEW_CONTAINER_CLASS2,
		NULL,
		WS_CHILD | WS_VISIBLE,
		rect->left,
		rect->top,
		rect->right - rect->left,
		rect->bottom - rect->top,
		view->m_hwndParent,
		(HMENU)(CID_CHANNELVIEW),
		g_hInstance,
		view
	);

	assert(view->m_hwnd);
	if (!view->m_hwnd) {
		DbgPrintW("ChannelViewOld::Create: Could not create main window!");
		delete view;
		return NULL;
	}
	
	view->m_listHwnd = CreateWindowEx(
		0,
		TEXT("LISTBOX"),
		NULL,
		WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL | WS_BORDER,
		rect->left,
		rect->top,
		rect->right - rect->left,
		rect->bottom - rect->top,
		view->m_hwnd,
		(HMENU) 2,
		g_hInstance,
		view
	);
	if (!view->m_listHwnd) {
		DbgPrintW("ChannelViewOld::Create: Could not create list sub-window!");
		delete view;
		return NULL;
	}

	SetWindowFont(view->m_listHwnd, g_MessageTextFont, TRUE);
	return view;
}

void ChannelViewOld::InitializeClass()
{
	WNDCLASS& wc = g_ChannelViewLegacyClass;

	wc.lpszClassName = T_CHANNEL_VIEW_CONTAINER_CLASS2;
	wc.hbrBackground = ri::GetSysColorBrush(COLOR_3DFACE);
	wc.style = 0;
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.lpfnWndProc = &ChannelViewOld::WndProc;
	wc.hInstance = g_hInstance;

	RegisterClass(&wc);
}

void ChannelViewOld::ClearChannels()
{
	m_items.clear();
	m_idToIdx.clear();
	m_nextCategIndex = 0;
	ListBox_ResetContent(m_listHwnd);
}

void ChannelViewOld::OnUpdateAvatar(Snowflake sf)
{
}

void ChannelViewOld::OnUpdateSelectedChannel(Snowflake sf)
{
	if (m_currentChannel == sf)
		return;

	m_currentChannel = sf;

	if (m_items.empty())
		return;

	ChannelMember& member = m_items[m_idToIdx[sf]];
	if (member.m_id != sf)
		return;

	ListBox_SetCurSel(m_listHwnd, m_idToIdx[sf]);
}

void ChannelViewOld::SetMode(bool listMode)
{
}

static std::string toString(int t) {
	std::stringstream ss;
	ss << t;
	return ss.str();
}

static std::string GetChannelString(const Channel& ch)
{
	std::string categBeg = ch.IsCategory() ? "--- " : (ch.m_parentCateg ? "        " : "");
	std::string categEnd = ch.IsCategory() ? " ---" : "";

	std::string mentions = "";
	std::string unreadMarker = "";
	if (ch.WasMentioned()) {
		mentions = "(" + toString(ch.m_mentionCount) + ") ";
	}

	if (ch.HasUnreadMessages()) {
		unreadMarker = " *";
	}

	return categBeg + mentions + ch.GetTypeSymbol() + ch.m_name + categEnd + unreadMarker;
}

void ChannelViewOld::UpdateAcknowledgement(Snowflake sf)
{
	for (size_t i = 0; i < m_items.size(); i++)
	{
		auto& item = m_items[i];
		if (item.m_id != sf)
			continue;

		Channel* pChan = GetDiscordInstance()->GetChannel(sf);
		if (!pChan) {
			DbgPrintW("ERROR: No channel");
			break;
		}

		LPTSTR tstr = ConvertCppStringToTString(GetChannelString(*pChan));
		// but why can't I just set the string's data?
		int oldsel = ListBox_GetCurSel(m_listHwnd);
		ListBox_DeleteString(m_listHwnd, i);
		ListBox_InsertString(m_listHwnd, i, tstr);
		ListBox_SetCurSel(m_listHwnd, oldsel);
		free(tstr);
		break;
	}
}

void ChannelViewOld::AddChannel(const Channel& ch)
{
	if (!ch.HasPermissionConst(PERM_VIEW_CHANNEL))
	{
		// If not a category, return.  We'll remove invisible categories without elements after the fact.
		if (ch.m_channelType != Channel::CATEGORY)
			return;
	}

	// TODO: Implement category order sorting. For now, they're sorted by ID.
	// 11/06/2025 - Are they!?
	int categIndex = ch.IsCategory() ? ch.m_pos : 0;

	m_idToIdx[ch.m_snowflake] = (int) m_items.size();
	m_items.push_back({ ch.m_parentCateg, ch.m_snowflake, ch.m_channelType, GetChannelString(ch), categIndex, ch.m_pos, ch.m_lastSentMsg });
}

void ChannelViewOld::RemoveCategoryIfNeeded(const Channel& ch)
{
}

void ChannelViewOld::CommitChannels()
{
	// calculate category indices
	std::map<Snowflake, int> categIdxs;
	for (auto& item : m_items) {
		if (item.m_type == Channel::CATEGORY)
			categIdxs[item.m_id] = item.m_categIndex;
	}

	for (auto& item : m_items) {
		if (item.m_type != Channel::CATEGORY)
			item.m_categIndex = categIdxs[item.m_category];
	}

	std::sort(m_items.begin(), m_items.end());

	for (size_t i = 0; i < m_items.size(); )
	{
		// remove any empty categories.
		if (m_items[i].IsCategory() && (i + 1 >= m_items.size() || m_items[i + 1].IsCategory()))
			m_items.erase(m_items.begin() + i);
		else
			i++;
	}

	for (auto& item : m_items)
	{
		LPTSTR name = ConvertCppStringToTString(item.m_name);
		ListBox_AddString(m_listHwnd, name);
		free(name);
	}
}

LRESULT ChannelViewOld::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	ChannelViewOld* pView = (ChannelViewOld*) GetWindowLongPtr(hWnd, GWLP_USERDATA);

	switch (uMsg)
	{
		case WM_NCCREATE: {
			CREATESTRUCT* strct = (CREATESTRUCT*)lParam;
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)strct->lpCreateParams);
			break;
		}

		case WM_DESTROY: {
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR) NULL);
			pView->m_hwnd = NULL;
			break;
		}

		case WM_SIZE: {
			assert(pView);
			WORD wWidth  = LOWORD(lParam);
			WORD wHeight = HIWORD(lParam);
			MoveWindow(pView->m_listHwnd, 0, 0, wWidth, wHeight, TRUE);
			break;
		}

		case WM_COMMAND:
		{
			if (HIWORD(wParam) != LBN_SELCHANGE)
				break;

			int selIndex = (int) ListBox_GetCurSel(pView->m_listHwnd);
			if (selIndex < 0 || (size_t)selIndex >= pView->m_items.size())
				break;

			ChannelMember& member = pView->m_items[selIndex];
			if (!member.m_id)
				break;

			if (member.m_type == Channel::CATEGORY)
				break;

			Snowflake sf = member.m_id;

			pView->m_currentChannel = sf;
			pView->m_pParent->GetChatView()->OnSelectChannel(sf);
			break;
		}
	}

	return DefWindowProc( hWnd, uMsg, wParam, lParam );
}