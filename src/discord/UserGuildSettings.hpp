#pragma once

#include <ctime>
#include <map>
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

struct MuteConfig
{
	time_t m_endTime = 0;
	int m_selectedTimeWindow = 0;

	void Load(const nlohmann::json& j);
	void Load(const nlohmann::json& parent, const std::string& key);
};

struct ChannelOverride
{
	Snowflake m_channelId = 0;
	bool m_bCollapsed = false;
	int m_flags = 0;
	eMessageNotifications m_notifications = NOTIF_ALL_MESSAGES;
	bool m_bMuted = false;
	MuteConfig m_muteConfig;

	void Load(const nlohmann::json& j);
	bool IsMuted() const;
};

struct GuildSettings
{
	Snowflake m_guildID = 0;
	int m_flags = 0;
	int m_notifyHighlights = 0;
	int m_version = 0;
	bool m_bHideMutedChannels = false;
	bool m_bMobilePush = false;
	bool m_bMuted = false;
	bool m_bMuteScheduledEvents = false;
	bool m_bSuppressEveryone = false;
	bool m_bSuppressRoles = false;
	MuteConfig m_muteConfig;
	eMessageNotifications m_messageNotifications = NOTIF_ALL_MESSAGES;
	std::map<Snowflake, ChannelOverride> m_channelOverride;

	void Load(const nlohmann::json& j);
	const ChannelOverride* GetOverride(Snowflake channel) const;
	bool IsMuted() const;
};

struct UserGuildSettings
{
	bool m_bLoaded = false;
	std::map<Snowflake, GuildSettings> m_guildSettings;
	int m_version = 0;

	void Load(const nlohmann::json& j);
	void Clear();

	const GuildSettings* GetSettings(Snowflake guild) const;

	GuildSettings* GetOrCreateSettings(Snowflake guild);
};
