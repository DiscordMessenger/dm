#pragma once

#include <vector>
#include <map>
#include "Main.hpp"
#include "IMemberList.hpp"

#define T_MEMBER_LIST_CLASS TEXT("MemberList")

class MemberList : public IMemberList
{
private:
	HWND m_hwndParent;
	ChatWindow* m_pParent = nullptr;

public:
	HWND m_listHwnd;
	//HWND m_mainHwnd;
	WNDPROC m_origListWndProc;
	Snowflake m_guild = 0;
	int m_nextItem = 0;
	int m_nextGroup = 0;
	std::map<HBITMAP, int> m_imageIDs;
	std::vector<Snowflake> m_items;
	std::vector<Snowflake> m_groups;
	std::map<Snowflake, int> m_grpToGrpIdx;
	std::map<Snowflake, int> m_usrToUsrIdx;
	int m_hotItem = -1;

public:
	~MemberList();
	void ClearMembers() override;
	void SetGuild(Snowflake g) override;
	void Update() override;
	void OnUpdateAvatar(Snowflake user, bool bAlsoUpdateText = false) override;
	void UpdateMembers(std::set<Snowflake>& mems) override;
	HWND GetListHWND() override { return m_listHwnd; }
	void StartUpdate();
	void StopUpdate();

public:
	static MemberList* Create(ChatWindow* parent, LPRECT lpRect);
	static void InitializeClass();

private:
	void Initialize();
	bool OnNotify(LRESULT& out, WPARAM wParam, LPARAM lParam);

	static LRESULT CALLBACK ListWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	static WNDCLASS g_MemberListClass;
};

