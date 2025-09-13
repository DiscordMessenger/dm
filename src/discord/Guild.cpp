#include "DiscordAPI.hpp"
#include "Guild.hpp"
#include "Util.hpp"
#include "HTTPClient.hpp"
#include "DiscordRequest.hpp"
#include "DiscordInstance.hpp"

void GuildRole::Load(iprog::JsonObject& j)
{
	m_id = GetSnowflake(j, "id");
	m_permissions = GetIntFromString(j["permissions"]);
	m_name = GetFieldSafe(j, "name");
	m_colorOriginal = GetFieldSafeInt(j, "color");
	m_bHoist       = GetFieldSafeBool(j, "hoist", false);
	m_bManaged     = GetFieldSafeBool(j, "managed", false);
	m_bMentionable = GetFieldSafeBool(j, "mentionable", false);
	m_position = GetFieldSafeInt(j, "position");
	m_colorOriginal = GetFieldSafeInt(j, "color");
}

GuildMember* Guild::GetGuildMember(Snowflake sf)
{
	return &GetProfileCache()->LookupProfile(sf, "", "", "", false)->m_guildMembers[m_snowflake];
}

void Guild::RequestFetchChannels()
{
	std::string url;

	if (m_snowflake)
		url = GetDiscordAPI() + "guilds/" + std::to_string(m_snowflake) + "/channels";
	else
		url = GetDiscordAPI() + "users/@me/channels";

	GetHTTPClient()->PerformRequest(
		true,
		NetRequest::GET,
		url,
		DiscordRequest::GUILD,
		m_snowflake,
		"",
		GetDiscordInstance()->GetToken()
	);
}

std::string Guild::GetGroupName(Snowflake id)
{
	if (id == GROUP_ONLINE)
		return "Online";
	if (id == GROUP_OFFLINE)
		return "Offline";

	for (auto& role : m_roles) {
		if (role.first == id) {
			return role.second.m_name;
		}
	}

	return std::to_string(id);
}

// Wow, thanks https://discord.com/developers/docs/topics/permissions#permission-overwrites
uint64_t Guild::ComputeBasePermissions(Snowflake member)
{
	if (m_ownerId == member)
		return PERM_ALL;

	// Get the everyone role
	GuildRole& everyone = m_roles[m_snowflake];
	uint64_t perms = everyone.m_permissions;

	Profile* pf = GetProfileCache()->LookupProfile(member, "", "", "", false);
	GuildMember& gm = pf->m_guildMembers[m_snowflake];
	for (auto& roleId : gm.m_roles) {
		GuildRole& grole = m_roles[roleId];
		perms |= grole.m_permissions;
	}

	if (perms & PERM_ADMINISTRATOR)
		return PERM_ALL;

	return perms;
}

bool Guild::IsFirstChannel(Snowflake channel)
{
	Snowflake lowestId = Snowflake(-1);

	for (auto& chan : m_channels) {
		if (lowestId > chan.m_snowflake && chan.IsText())
			lowestId = chan.m_snowflake;
	}

	return lowestId == channel;
}
