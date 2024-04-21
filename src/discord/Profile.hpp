#pragma once
#include <string>
#include <vector>
#include <map>
#include "Snowflake.hpp"
#include "ActiveStatus.hpp"
#include "GuildMember.hpp"

struct Profile
{
	Snowflake m_snowflake = 0;
	std::string m_name;
	std::string m_globalName;
	int m_discrim = 0;
	std::string m_email;
	std::string m_bio = "";
	std::string m_status = "";// "(i) User is suspected to be COOL as SHIT. Please report any COOL activity to Discord staff.";
	std::string m_avatarlnk = "";
	eActiveStatus m_activeStatus = STATUS_ONLINE;
	bool m_bUsingDefaultData = true;
	bool m_bIsBot = false;

	std::map<Snowflake, GuildMember> m_guildMembers;

	Profile() {}

	Profile(Snowflake s, const std::string& name, int disc, const std::string& email) :
		m_snowflake(s), m_name(name), m_discrim(disc), m_email(email)
	{
	}

	bool HasGuildMemberProfile(Snowflake guild) {
		auto fnd = m_guildMembers.find(guild);
		return fnd != m_guildMembers.end();
	}

	std::string GetName(Snowflake guild) {
		if (!HasGuildMemberProfile(guild))
			return m_globalName;
		GuildMember& gm = m_guildMembers[guild];
		if (gm.m_nick.empty())
			return m_globalName;
		return gm.m_nick;
	}

	std::string GetStatus(Snowflake guild) {
		if (!HasGuildMemberProfile(guild))
			return m_status;
		GuildMember& gm = m_guildMembers[guild];
		if (gm.m_nick.empty())
			return m_status;
		return gm.m_status;
	}
};
