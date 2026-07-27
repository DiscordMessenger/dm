#include <nlohmann/json.h>
#include "Channel.hpp"
#include "../state/MessageCache.hpp"
#include "../DiscordInstance.hpp"

// Thanks https://discord.com/developers/docs/topics/permissions#permission-overwrites
uint64_t Channel::ComputePermissionOverwrites(Snowflake Member, uint64_t BasePermissions) const
{
	// Administrator overrides any potential permission overwrites.
	if (BasePermissions & PERM_ADMINISTRATOR)
		return PERM_ALL;

	// Find @everyone overwrite and apply it.
	auto everyoneIter = m_overwrites.find(m_parentGuild);
	if (everyoneIter != m_overwrites.end())
	{
		BasePermissions &= ~everyoneIter->second.m_deny;
		BasePermissions |= everyoneIter->second.m_allow;
	}

	// Apply role specific overwrites.
	Profile* pf = GetProfileCache()->LookupProfile(Member, "", "", "", false);
	GuildMember& gm = pf->m_guildMembers[m_parentGuild];
	uint64_t Allow = 0, Deny = 0;
	for (auto roleId : gm.m_roles)
	{
		auto roleIter = m_overwrites.find(roleId);
		if (roleIter == m_overwrites.end())
			continue; // not found!

		Allow |= roleIter->second.m_allow;
		Deny  &= ~roleIter->second.m_deny;
	}

	BasePermissions &= ~Deny;
	BasePermissions |= Allow;

	// Apply a member specific overwrite, if it exists.
	auto memberIter = m_overwrites.find(Member);
	if (memberIter != m_overwrites.end())
	{
		BasePermissions &= ~memberIter->second.m_deny;
		BasePermissions |= memberIter->second.m_allow;
	}

	return BasePermissions;
}

bool Channel::HasPermissionUser(Snowflake sf, uint64_t Permission)
{
	Guild* pGuild = GetDiscordInstance()->GetGuild(m_parentGuild);
	assert(pGuild);
	uint64_t perms = ComputePermissionOverwrites(sf, pGuild->ComputeBasePermissions(sf));

	return (perms & Permission) != 0;
}

bool Channel::HasPermission(uint64_t Permission)
{
	if (!m_bCurrentUserPermsCalculated)
	{
		// calculate them
		Guild* pGuild = GetDiscordInstance()->GetGuild(m_parentGuild);
		assert(pGuild);
		Snowflake currUser = GetDiscordInstance()->GetProfile()->m_snowflake;
		m_currentUserPerms = ComputePermissionOverwrites(currUser, pGuild->ComputeBasePermissions(currUser));
	}

	return (m_currentUserPerms & Permission) != 0;
}

bool Channel::HasPermissionConst(uint64_t Permission) const
{
	uint64_t perms = m_currentUserPerms;

	if (!m_bCurrentUserPermsCalculated)
	{
		// calculate them
		Guild* pGuild = GetDiscordInstance()->GetGuild(m_parentGuild);
		assert(pGuild);
		Snowflake currUser = GetDiscordInstance()->GetProfile()->m_snowflake;
		perms = ComputePermissionOverwrites(currUser, pGuild->ComputeBasePermissions(currUser));
	}

	return (perms & Permission) != 0;
}
