#pragma once

#include <string>
#include <iprogsjson.hpp>
#include "Snowflake.hpp"

class Emoji
{
public:
	void Load(const iprog::JsonObject& j);

public:
	Snowflake m_id = 0;
	std::string m_name;
	bool m_bAvailable = false;
	bool m_bRequireColons = false; // ignored as long as we support only server emoji
	bool m_bAnimated = false; // ignored
	bool m_bManaged = false; // ignored
	Snowflake m_user = 0;
};
