#include "UserGuildSettings.hpp"
#include "Util.hpp"

void MuteConfig::Load(const iprog::JsonObject& j)
{
	m_endTime = ParseTime(GetFieldSafe(j, "end_time"));
	m_selectedTimeWindow = GetFieldSafeInt(j, "selected_time_window");
}

void MuteConfig::Load(const iprog::JsonObject& parent, const std::string& key)
{
	auto it = parent.find(key);
	if (it == parent.end() || !it->is_structured())
		return;

	Load(*it);
}

void ChannelOverride::Load(const iprog::JsonObject& j)
{
	m_channelId = GetSnowflake(j, "channel_id");
	m_bCollapsed = GetFieldSafeBool(j, "collapsed", false);
	m_flags = GetFieldSafeInt(j, "flags");
	m_notifications = eMessageNotifications(GetFieldSafeInt(j, "message_notifications"));
	m_bMuted = GetFieldSafeBool(j, "muted", false);
	m_muteConfig.Load(j, "mute_config");
}

bool ChannelOverride::IsMuted() const
{
	return m_bMuted && (
		m_muteConfig.m_selectedTimeWindow == -1 ||
		m_muteConfig.m_endTime > time(NULL)
	);
}

const ChannelOverride* GuildSettings::GetOverride(Snowflake channel) const
{
	auto it = m_channelOverride.find(channel);
	if (it == m_channelOverride.end())
		return nullptr;

	return &it->second;
}

bool GuildSettings::IsMuted() const
{
	return m_bMuted && (
		m_muteConfig.m_selectedTimeWindow == -1 ||
		m_muteConfig.m_endTime > time(NULL)
	);
}

void GuildSettings::Load(const iprog::JsonObject& j)
{
	m_guildID = GetSnowflake(j, "guild_id");
	m_flags = GetFieldSafeInt(j, "flags");
	m_version = GetFieldSafeInt(j, "version");
	m_notifyHighlights = GetFieldSafeInt(j, "notify_highlights");
	m_bHideMutedChannels = GetFieldSafeBool(j, "hide_muted_channels", false);
	m_bMobilePush = GetFieldSafeBool(j, "mobile_push", true);
	m_bMuted = GetFieldSafeBool(j, "muted", false);
	m_bMuteScheduledEvents = GetFieldSafeBool(j, "mute_scheduled_events", true);
	m_bSuppressEveryone = GetFieldSafeBool(j, "suppress_everyone", false);
	m_bSuppressRoles = GetFieldSafeBool(j, "suppress_roles", false);
	m_muteConfig.Load(j, "mute_config");
	m_messageNotifications = eMessageNotifications(GetFieldSafeInt(j, "message_notifications"));

	auto it = j.find("channel_overrides");
	if (it != j.end() && it->is_array())
	{
		for (const auto& co : *it)
		{
			ChannelOverride cov;
			cov.Load(co);
			m_channelOverride[cov.m_channelId] = cov;
		}
	}
}

void UserGuildSettings::Load(const iprog::JsonObject& j)
{
	bool partial = GetFieldSafeBool(j, "partial", false);
	if (!partial)
		m_guildSettings.clear();

	int version = GetFieldSafeInt(j, "version");
	if (m_bLoaded && m_version > version)
		return;

	m_bLoaded = true;
	m_version = version;

	auto it = j.find("entries");
	if (it == j.end() || !it->is_array())
		return;

	for (const auto& entry : *it)
	{
		GuildSettings gs;
		gs.Load(entry);
		m_guildSettings[gs.m_guildID] = gs;
	}
}

void UserGuildSettings::Clear()
{
	m_bLoaded = false;
	m_version = 0;
	m_guildSettings.clear();
}

GuildSettings* UserGuildSettings::GetOrCreateSettings(Snowflake guild)
{
	return &m_guildSettings[guild];
}

const GuildSettings* UserGuildSettings::GetSettings(Snowflake guild) const
{
	auto it = m_guildSettings.find(guild);
	if (it == m_guildSettings.end())
		return nullptr;

	return &it->second;
}
