#pragma once

#include <string>
#include <nlohmann/json.h>
#include "./Snowflake.hpp"

enum eRelationshipType
{
	REL_NONE,
	REL_FRIEND,
	REL_BLOCKED,
};

class Relationship
{
public:
	// N.B. For friend requests, there's also a "since" field, but not going to use it right now.

	Snowflake m_id = 0;     // ID of the relationship.
	Snowflake m_userID = 0; // ID of the user 'target' of this relationship. Coincidentally most of the time this and m_id match
	eRelationshipType m_type = REL_NONE;
	std::string m_nickname; // Friend nickname

public:
	void Load(const nlohmann::json& j);
};
