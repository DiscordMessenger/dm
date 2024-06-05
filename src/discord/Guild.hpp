#pragma once

#include <string>
#include <list>
#include <map>
#include <vector>
#include <nlohmann/json.h>
#include "Snowflake.hpp"
#include "Channel.hpp"
#include "Emoji.hpp"

// obscenely high numbers s.t. that they show last
#define GROUP_ONLINE  9000000000000000000LL
#define GROUP_OFFLINE 9000000000000000001LL

#include "Permissions.hpp"
#include "ProfileCache.hpp"
#include "GuildMember.hpp"

struct GuildRole
{
	Snowflake m_id = 0;
	std::string m_name;
	int m_colorOriginal = 0;
	std::string m_icon; // avatar
	std::string m_unicodeEmoji;
	// role is pinned in the user listing according to Discord API.
	// Probably actually means "show separate from other online members"
	bool m_bHoist = false;
	bool m_bManaged = false;
	bool m_bMentionable = 0;
	int m_position = 0;
	uint64_t m_permissions = 0;

	bool operator<(const GuildRole& oth) const
	{
		if (m_position != oth.m_position) return m_position > oth.m_position;
		if (m_id != oth.m_id) return m_id > oth.m_id;
		return false;
	}

	void Load(nlohmann::json& j);
};

struct GuildMemberGroup
{
	int m_count = 0;

	GuildMemberGroup() {}
	GuildMemberGroup(int count) : m_count(count) {}
};

struct Guild
{
	Snowflake m_snowflake = 0;
	std::string m_name = "";
	std::string m_avatarlnk = "";

	bool m_bChannelsLoaded = false;
	std::list<Channel> m_channels;
	Snowflake m_currentChannel = 0;

	std::map<Snowflake, GuildRole> m_roles;
	std::map<Snowflake, Emoji> m_emoji;
	std::vector<Snowflake> m_members;
	int m_memberCount = 0, m_onlineCount = 0;

	Snowflake m_ownerId = 0;

	std::set<Snowflake> m_knownMembers;

	int m_order = 0;

	bool operator<(const Guild& other) const {
		if (m_order != other.m_order)
			return m_order < other.m_order;
		return m_snowflake < other.m_snowflake;
	}

	Channel* GetChannel(Snowflake sf) {
		for (auto& ch : m_channels) {
			if (ch.m_snowflake == sf)
				return &ch;
		}
		return nullptr;
	}

	GuildMember* GetGuildMember(Snowflake sf);

	Guild(Snowflake sf, const std::string& name) : m_snowflake(sf), m_name(name)
	{}

	Guild() {}

	void RequestFetchChannels();

	std::string GetGroupName(Snowflake id);

	uint64_t ComputeBasePermissions(Snowflake member);

	bool IsFirstChannel(Snowflake channel);

	void AddKnownMember(Snowflake sf) {
		m_knownMembers.insert(sf);
	}
};
