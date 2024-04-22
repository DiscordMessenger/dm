#pragma once

#include <set>
#include <nlohmann/json.h>
#include "Profile.hpp"
#include "Guild.hpp"

class ProfileCache
{
public:
	~ProfileCache();
	
	// Looks up a profile.
	// Username and globalname are filled in to create a default profile if the profile is not cached.
	// Returns NULL if the Discord servers have reported that the user does not exist.
	Profile* LookupProfile(Snowflake user, const std::string& username, const std::string& globalName, const std::string& avatarLink, bool bRequestServer = true);
	Profile* LoadProfile(Snowflake user, nlohmann::json& j);

	void ClearAll();

	// Let the profile cache know that the specified profile doesn't exist in this guild
	void ProfileDoesntExist(Snowflake user, Snowflake guild);

	// Check if the guild member needs to be requested
	bool NeedRequestGuildMember(Snowflake user, Snowflake guild);

	// Forget a profile.
	void ForgetProfile(Snowflake user);

private:
	void RequestLoadProfile(Snowflake user);

	std::map<Snowflake, Profile> m_profileSets;
	std::set<Snowflake> m_processingRequests;
};

ProfileCache* GetProfileCache();
