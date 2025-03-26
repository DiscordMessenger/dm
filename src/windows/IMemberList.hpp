#pragma once

#include "Main.hpp"

class IMemberList
{
public:
	HWND m_mainHwnd;

public:
	static IMemberList* CreateMemberList(HWND hwnd, LPRECT lprect);
	static void InitializeClasses();

	virtual void SetGuild(Snowflake sf) = 0;
	virtual void Update() = 0;
	virtual void UpdateMembers(std::set<Snowflake>& mems) = 0;
	virtual void OnUpdateAvatar(Snowflake sf, bool bAlsoUpdateText = false) = 0;
	virtual void ClearMembers() = 0;
	virtual HWND GetListHWND() = 0;

public:
	virtual ~IMemberList() {};
};
