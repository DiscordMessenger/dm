#pragma once

#include "IMemberList.hpp"
#include "ChatWindow.hpp"

#define T_MEMBER_LIST_CLASS_OLD TEXT("MemberListOld")

class MemberListOld : public IMemberList
{
	struct Member {
		Snowflake m_id;
		std::string m_name;

		bool operator<(const Member& oth) const {
			if (m_name != oth.m_name)
				return StringCompareCaseInsens(m_name.c_str(), oth.m_name.c_str()) < 0;

			return m_id < oth.m_id;
		}
	};

public:
	static MemberListOld* Create(ChatWindow* parent, LPRECT lprect);
	static void InitializeClass();

	void SetGuild(Snowflake sf) override;
	void Update() override;
	void UpdateMembers(std::set<Snowflake>& mems) override;
	void OnUpdateAvatar(Snowflake sf, bool bAlsoUpdateText = false) override;
	void ClearMembers() override;
	HWND GetListHWND() override;

private:
	static WNDCLASS g_memberListOldClass;
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	HWND m_listHwnd = NULL;
	HWND m_hwndParent = NULL;
	ChatWindow* m_pParent = nullptr;

	Snowflake m_guild = 0;

	std::vector<Member> m_members;
};
