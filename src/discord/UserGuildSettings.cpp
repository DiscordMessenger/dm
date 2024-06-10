#include "UserGuildSettings.hpp"
#include "Util.hpp"

void ChannelOverride::Load(const nlohmann::json& j)
{
	m_channelId = GetSnowflake(j, "channel_id");
	m_bCollapsed = GetFieldSafeBool(j, "collapsed", false);
	m_flags = GetFieldSafeInt(j, "flags");
	m_notifications = eMessageNotifications(GetFieldSafeInt(j, "message_notifications"));

	m_bMuted = GetFieldSafeBool(j, "muted", false);
	auto it = j.find("mute_config");
	if (it != j.end() && !it->is_null()) {
		m_muteEndTime = ParseTime((*it)["end_time"]);
		m_selectedTimeWindow = GetFieldSafeInt(*it, "selected_time_window");
	}
	else {
		m_muteEndTime = 0;
		m_selectedTimeWindow = 0;
	}
}
