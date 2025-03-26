#pragma once

#include "Main.hpp"

class IChannelView
{
public:
	HWND m_hwnd = NULL;

public:
	static IChannelView* CreateChannelView(HWND hwnd, LPRECT rect);
	static void InitializeClasses();

public:
	virtual ~IChannelView() {}
	virtual void ClearChannels() = 0;
	virtual void OnUpdateAvatar(Snowflake sf) = 0;
	virtual void OnUpdateSelectedChannel(Snowflake sf) = 0;
	virtual void UpdateAcknowledgement(Snowflake sf) = 0;
	virtual void SetMode(bool listMode) = 0;
	virtual void AddChannel(const Channel& channel) = 0;
	virtual void RemoveCategoryIfNeeded(const Channel& channel) = 0;
	virtual void CommitChannels() = 0;

	// these are only used for checking if character events should be redirected
	// so, these can be the same for all it cares
	virtual HWND GetListHWND() = 0;
	virtual HWND GetTreeHWND() = 0;
};
