#pragma once

#include <set>
#include <iprogsjson.hpp>
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
	Profile* LoadProfile(Snowflake user, const iprog::JsonObject& j);

	void ClearAll();

	// Let the profile cache know that the specified profile doesn't exist in this guild
	void ProfileDoesntExist(Snowflake user, Snowflake guild);

	// Check if the guild member needs to be requested
	bool NeedRequestGuildMember(Snowflake user, Snowflake guild);

	// Forget a profile.
	void ForgetProfile(Snowflake user);

	// Request extra data from a profile.
	void RequestExtraData(Snowflake user, Snowflake guild = 0, bool mutualGuilds = true, bool mutualFriends = true);

	// Request note data from a profile.
	void RequestNote(Snowflake user);

protected:
	friend struct Profile;
	void PutNote(Snowflake user, const std::string& note) const;

private:
	void RequestLoadProfile(Snowflake user, Snowflake guild = 0, bool mutualGuilds = true, bool mutualFriends = true);

	std::map<Snowflake, Profile> m_profileSets;
	std::set<Snowflake> m_processingRequests;
};

ProfileCache* GetProfileCache();
