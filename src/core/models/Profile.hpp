#pragma once
#include <string>
#include <vector>
#include <map>
#include "./Snowflake.hpp"
#include "./ActiveStatus.hpp"
#include "./GuildMember.hpp"

struct Profile
{
	// Basic data. Provided with almost every user object.
	bool m_bUsingDefaultData = true;
	Snowflake m_snowflake = 0;
	std::string m_name;
	std::string m_globalName;
	int m_discrim = 0;
	std::string m_email; // Used only for the user's own profile!
	std::string m_avatarlnk = "";
	bool m_bIsBot = false;

	// Note
	bool m_bNoteFetched = false;
	std::string m_note = "";

	// Extra data. Loaded as part of a "PROFILE" request.
	bool m_bExtraDataFetched = false;
	std::string m_bio = "";
	std::string m_pronouns = "";

	// Activity data. This is updated by presence updates.
	eActiveStatus m_activeStatus = STATUS_OFFLINE;
	std::string m_status = "";

	std::map<Snowflake, GuildMember> m_guildMembers;

	Profile() {}

	Profile(Snowflake s, const std::string& name, int disc, const std::string& email) :
		m_snowflake(s), m_name(name), m_discrim(disc), m_email(email)
	{
	}

	bool HasGuildMemberProfile(Snowflake guild) const {
		auto fnd = m_guildMembers.find(guild);
		return fnd != m_guildMembers.end();
	}

	std::string GetName(Snowflake guild) const {
		auto it = m_guildMembers.find(guild);
		if (it == m_guildMembers.end())
			return m_globalName;
		const GuildMember& gm = it->second;
		if (gm.m_nick.empty())
			return m_globalName;
		return gm.m_nick;
	}

	std::string GetStatus(Snowflake guild) const {
		auto it = m_guildMembers.find(guild);
		if (it == m_guildMembers.end())
			return m_status;
		const GuildMember& gm = it->second;
		if (gm.m_nick.empty())
			return m_status;
		return gm.m_status;
	}

	const std::string& GetUsername() const { return m_name; }
	float FuzzyMatch(const char* check, Snowflake guild) const;

	// Sends the Discord backend a request to either remove or set the note.
	void PutNote() const;
};
