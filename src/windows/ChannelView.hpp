#pragma once

#include "Main.hpp"

#define C_MAX_CHANNEL_MEMBER_LEN (64)

class ChannelView
{
	struct ChannelMember
	{
		TCHAR str[C_MAX_CHANNEL_MEMBER_LEN];
		Snowflake m_snowflake;
		HTREEITEM m_hItem;
		int m_childCount;
	};

	int m_level = 1;

	std::map<Snowflake, int> m_idToIdx;
	std::vector<ChannelMember> m_channels;

	HWND m_hwndParent;
public:
	HWND m_hwnd;

public:
	void ClearChannels();
	void UpdateAcknowledgement(Snowflake sf);
	void AddChannel(const Channel& ch);
	void RemoveCategoryIfNeeded(const Channel& ch);
	void OnNotify(WPARAM wParam, LPARAM lParam);

public:
	static int g_ChannelViewId;

	static ChannelView* Create(HWND hwnd, LPRECT rect);
};

