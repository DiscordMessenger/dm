#include "DiscordAPI.hpp"
#include "ProfileCache.hpp"
#include "Util.hpp"
#include "Frontend.hpp"
#include "DiscordRequest.hpp"
#include "DiscordInstance.hpp"

static ProfileCache g_ProfileCache;

ProfileCache* GetProfileCache()
{
	return &g_ProfileCache;
}

ProfileCache::~ProfileCache()
{
}

Profile* ProfileCache::LookupProfile(Snowflake user, const std::string& username, const std::string& globalName, const std::string& avatarLink, bool bRequestServer)
{
	Profile* pProf = &m_profileSets[user];

	if (!pProf->m_bUsingDefaultData)
		return pProf;

	pProf->m_snowflake = user;

	if (pProf->m_name.empty())
		pProf->m_name = username;
	if (pProf->m_globalName.empty())
		pProf->m_globalName = globalName;

	if (!avatarLink.empty()) {
		pProf->m_avatarlnk  = avatarLink;
		GetFrontend()->RegisterAvatar(user, avatarLink);
	}

	if (bRequestServer)
		RequestLoadProfile(user);

	return pProf;
}

Profile* ProfileCache::LoadProfile(Snowflake user, nlohmann::json& jx)
{
	std::string str = jx.dump();
	Profile* pf = LookupProfile(user, "", "", "", false);

	bool anythingChanged = false;
	std::string oldName   = pf->m_name;
	std::string oldGName  = pf->m_globalName;
	std::string oldAvatar = pf->m_avatarlnk;
	bool oldIsBot = pf->m_bIsBot;

	auto iter = m_processingRequests.find(user);
	if (iter != m_processingRequests.end())
		m_processingRequests.erase(iter);

	auto& userData = jx;
	if (userData.contains("user"))
		userData = jx["user"];

	pf->m_snowflake  = user;
	pf->m_name       = GetUsername(userData);
	pf->m_discrim    = userData.contains("discriminator") ? GetIntFromString(jx["discriminator"]) : 0;
	pf->m_bio        = GetFieldSafe(userData, "bio");
	pf->m_globalName = GetGlobalName(userData);
	pf->m_bUsingDefaultData = false;
	pf->m_bIsBot     = userData["bot"].is_boolean() ? bool(userData["bot"]) : false;

	if (userData.contains("email"))
		pf->m_email = GetFieldSafe(userData, "email");

	// Avatar links formatted as https://cdn.discordapp.com/avatars/<userid>/<avatarlnk>
	if (userData["avatar"].is_string()) {
		pf->m_avatarlnk = userData["avatar"];
		GetFrontend()->RegisterAvatar(pf->m_snowflake, pf->m_avatarlnk);
	}
	else {
		pf->m_avatarlnk = "";
	}

	GetFrontend()->UpdateUserData(pf->m_snowflake);
	GetFrontend()->RepaintProfileWithUserID(pf->m_snowflake);

	return pf;
}

void ProfileCache::ClearAll()
{
	m_profileSets.clear();
	m_processingRequests.clear();
}

void ProfileCache::ProfileDoesntExist(Snowflake user, Snowflake guild)
{
	Profile* pf = &m_profileSets[user];
	pf->m_guildMembers[guild].m_bIsLoadedFromChunk = true;
	pf->m_guildMembers[guild].m_bIsGroup = false;
	pf->m_guildMembers[guild].m_bExists = false;
}

bool ProfileCache::NeedRequestGuildMember(Snowflake user, Snowflake guild)
{
	if (!user)
		return false;

	// TODO: Deleted User

	Profile* pf = LookupProfile(user, "", "", "", false);
	if (!pf->HasGuildMemberProfile(guild))
		return true;
	
	if (!pf->m_guildMembers[guild].m_bIsLoadedFromChunk)
		return true;

	return false;
}

void ProfileCache::ForgetProfile(Snowflake user)
{
	auto iter = m_profileSets.find(user);
	if (iter != m_profileSets.end())
		m_profileSets.erase(iter);
}

void ProfileCache::RequestLoadProfile(Snowflake user)
{
	DbgPrintF("Request load for profile %lld", user);

	if (!user)
		return;

	if (m_processingRequests.find(user) != m_processingRequests.end())
		return;

	m_processingRequests.insert(user);

	std::string additionalData = "";
	std::string userSource = "";
	
	additionalData += "&with_mutual_guilds=true";
	additionalData += "&with_mutual_friends_count=false";

	if (!additionalData.empty() && additionalData[0] == '&')
		additionalData[0] = '?';

	GetHTTPClient()->PerformRequest(
		true,
		NetRequest::GET,
		GetDiscordAPI() + "users/" + std::to_string(user) + "/profile",
		DiscordRequest::PROFILE,
		user,
		additionalData,
		GetDiscordInstance()->GetToken(),
		"0"
	);
}
