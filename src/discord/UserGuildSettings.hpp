#pragma once

#include <ctime>
#include <nlohmann/json.h>
#include "Snowflake.hpp"

enum eMessageNotifications
{
	NOTIF_ALL_MESSAGES,
	NOTIF_ONLY_MENTIONS,
	NOTIF_NO_MESSAGES,
	NOTIF_INHERIT_PARENT,
};

enum eMessageOverrideFlags
{
	FLAG_SHOWN = (1 << 12),
};

struct ChannelOverride
{
	Snowflake m_channelId = 0;
	bool m_bCollapsed = false;
	int m_flags = 0;
	eMessageNotifications m_notifications = NOTIF_ALL_MESSAGES;
	bool m_bMuted = false;
	time_t m_muteEndTime = 0;
	int m_selectedTimeWindow = 0;

	void Load(const nlohmann::json& j);
};
