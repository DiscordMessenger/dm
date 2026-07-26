#pragma once

#include "Main.hpp"
#include "IThreadView.hpp"

#define T_THREAD_VIEW_CLASS TEXT("ThreadView")

class ThreadView : public IThreadView
{
public:
	HWND m_hwndParent;
	HWND m_mainHwnd;
	HWND m_listHwnd;
	WNDPROC m_origListWndProc;
	Snowflake m_guild = 0;
	Snowflake m_channel = 0;
	int m_nextItem = 0;
	int m_nextGroup = 0;
	std::vector<ThreadListItem> m_threads;
	std::map<Snowflake, int> m_threadToThreadIdx;
	int m_hotItem = -1;
	
public:
	~ThreadView();

	static ThreadView* Create(HWND hWnd, LPRECT lpRect);
	static void InitializeClass();

	void SetGuild(Snowflake sf) override;
	void SetChannel(Snowflake sf) override;
	void SetThreads(const std::vector<Channel>& channels) override;
	void ClearThreads() override;
	HWND GetHWND() override;
	void StartUpdate() override;
	void StopUpdate() override;
	void PopulateWithDummyData() override;

private:
	void Initialize();

private:
	bool OnNotify(LRESULT& out, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK ListWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
	static WNDCLASS g_ThreadViewClass;
};
