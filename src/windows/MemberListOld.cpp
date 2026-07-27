#include "MemberListOld.hpp"

WNDCLASS MemberListOld::g_memberListOldClass;

MemberListOld* MemberListOld::Create(HWND hWnd, LPRECT rect)
{
	MemberListOld* list = new MemberListOld;

	list->m_hwndParent = hWnd;

	list->m_mainHwnd = CreateWindowEx(
		0,
		T_MEMBER_LIST_CLASS_OLD,
		NULL,
		WS_CHILD | WS_VISIBLE,
		rect->left,
		rect->top,
		rect->right - rect->left,
		rect->bottom - rect->top,
		hWnd,
		(HMENU)CID_MEMBERLIST,
		g_hInstance,
		list
	);

	//assert(list->m_mainHwnd);
	if (!list->m_mainHwnd) {
		DbgPrintW("Couldn't create Member List main window!");
		delete list;
		return nullptr;
	}

	list->m_listHwnd = CreateWindowEx(
		0,
		TEXT("LISTBOX"),
		NULL,
		WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL | WS_BORDER,
		0,
		0,
		rect->right - rect->left,
		rect->bottom - rect->top,
		list->m_mainHwnd,
		(HMENU)1,
		g_hInstance,
		NULL
	);

	if (!list->m_listHwnd) {
		DbgPrintW("Couldn't create Member List list window! (Old)");
		return list;
	}

	SetWindowFont(list->m_listHwnd, g_MessageTextFont, TRUE);
	return list;
}

void MemberListOld::InitializeClass()
{
	WNDCLASS& wc = g_memberListOldClass;

	wc.lpszClassName = T_MEMBER_LIST_CLASS_OLD;
	wc.hbrBackground = ri::GetSysColorBrush(COLOR_3DFACE);
	wc.style = 0;
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.lpfnWndProc = &MemberListOld::WndProc;
	wc.hInstance = g_hInstance;

	RegisterClass(&wc);
}

void MemberListOld::SetGuild(Snowflake sf)
{
	if (m_guild == sf)
		return;

	m_guild = sf;
	ClearMembers();
}

void MemberListOld::Update()
{
	// TODO TODO TODO: Further test this.. I'm not confident that this is working correctly.
	// I have written a test suite for it and have seen it work just as intended but I'm still not sure.

	Guild* pGuild = GetDiscordInstance()->GetGuild(m_guild);
	assert(pGuild);

	std::vector<Member> newMembers;

	// Add each non-group member
	for (auto& mem : pGuild->m_members)
	{
		GuildMember* pMember = pGuild->GetGuildMember(mem);
		if (pMember->m_bIsGroup)
			continue;

		Profile* pf = GetProfileCache()->LookupProfile(pMember->m_user, "", "", "", false);

		// are they online
		if (pf->m_activeStatus == STATUS_OFFLINE && !pf->m_bUsingDefaultData)
			continue;

		Member m;
		m.m_id = pMember->m_user;
		m.m_name = pf->GetName(m_guild);

		newMembers.push_back(m);
	}

	// make sure to sort the new member list by name, keeps things easy
	std::sort(newMembers.begin(), newMembers.end());

	// Calculate diffs between the old and new
	std::map<Snowflake, int> present;
	for (auto& mem : newMembers)
		present[mem.m_id] |= 1;
	for (auto& mem : m_members)
		present[mem.m_id] |= 2;

	// those that were in m_members but aren't in newMembers, get rid of em
	for (size_t i = 0; i < m_members.size(); )
	{
		auto& mem = m_members[i];
		if (present[mem.m_id] != 2) {
			i++;
			continue;
		}

		// erase this one
		int index = (int)i;
		ListBox_DeleteString(m_listHwnd, index);
		m_members.erase(m_members.begin() + i);
	}

	// insert the new ones within the old ones
	for (size_t i = 0, j = 0; i < newMembers.size(); i++)
	{
		// i - index within newMembers
		// j - index within m_members
		if (j < m_members.size() &&
			m_members[j].m_id == newMembers[i].m_id) {
			j++;
			continue;
		}

		// insert!
		LPTSTR tstr = ConvertCppStringToTString(newMembers[i].m_name);
		ListBox_InsertString(m_listHwnd, (int)i, tstr);
		free(tstr);
	}

	m_members = std::move(newMembers);
}

void MemberListOld::UpdateMembers(std::set<Snowflake>& mems)
{
	for (auto mem : mems) {
		OnUpdateAvatar(mem, true);
	}
}

void MemberListOld::OnUpdateAvatar(Snowflake sf, bool bAlsoUpdateText)
{
}

void MemberListOld::ClearMembers()
{
	ListBox_ResetContent(m_listHwnd);
}

HWND MemberListOld::GetListHWND()
{
	return m_listHwnd;
}

LRESULT MemberListOld::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	MemberListOld* pList = (MemberListOld*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	switch (uMsg)
	{
		case WM_NCCREATE: {
			CREATESTRUCT* strct = (CREATESTRUCT*)lParam;
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)strct->lpCreateParams);
			break;
		}

		case WM_DESTROY: {
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR) NULL);
			pList->m_mainHwnd = NULL;
			break;
		}

		case WM_SIZE: {
			assert(pList);
			WORD wWidth  = LOWORD(lParam);
			WORD wHeight = HIWORD(lParam);
			MoveWindow(pList->m_listHwnd, 0, 0, wWidth, wHeight, TRUE);
			break;
		}
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
