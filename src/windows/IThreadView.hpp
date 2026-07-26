#pragma once

#include "Main.hpp"

struct ThreadListItem
{
	Snowflake m_id;
	Snowflake m_ownerID;
	Snowflake m_lastSentMsg;
	Snowflake m_lastViewedMsg;
	std::string m_name;
};

class IThreadView
{
public:
	static IThreadView* CreateThreadView(HWND hWnd, LPRECT lprect);
	static void InitializeClasses();

	static ThreadListItem Simplify(const Channel& chan);

	virtual void SetGuild(Snowflake sf) = 0;
	virtual void SetChannel(Snowflake sf) = 0;
	virtual void ClearThreads() = 0;
	virtual void SetThreads(const std::vector<Channel>& channels) = 0;
	virtual HWND GetHWND() = 0;
	virtual void StartUpdate() = 0;
	virtual void StopUpdate() = 0;
	virtual void PopulateWithDummyData() {};

public:
	virtual ~IThreadView() {}
};