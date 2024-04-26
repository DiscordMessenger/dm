#pragma once

#include <vector>
#include <map>
#include "Main.hpp"

#define T_MEMBER_LIST_CLASS TEXT("MemberList")

class MemberList
{
private:
	HWND m_hwndParent;

public:
	HWND m_listHwnd;
	HWND m_mainHwnd;
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
	HBITMAP m_imageOffline;
	HBITMAP m_imageDnd;
	HBITMAP m_imageIdle;
	HBITMAP m_imageOnline;

public:
	~MemberList();
	void StartUpdate();
	void StopUpdate();
	void ClearMembers();
	void SetGuild(Snowflake g);
	void Update();
	void OnUpdateAvatar(Snowflake user, bool bAlsoUpdateText = false);
	void UpdateMembers(std::set<Snowflake>& mems);

public:
	static MemberList* Create(HWND hWnd, LPRECT lpRect);
	static void InitializeClass();

private:
	void Initialize();
	void InitializeImageList();
	bool OnNotify(LRESULT& out, WPARAM wParam, LPARAM lParam);

	static LRESULT CALLBACK ListWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	static WNDCLASS g_MemberListClass;
};

