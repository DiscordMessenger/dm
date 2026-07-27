#include "Emoji.hpp"
#include "Util.hpp"
#include "../state/ProfileCache.hpp"

void Emoji::Load(const nlohmann::json& j)
{
	m_id = GetSnowflake(j, "id");
	m_name = GetFieldSafe(j, "name");
	m_bAvailable = GetFieldSafeBool(j, "available", true);
	m_bManaged = GetFieldSafeBool(j, "managed", false);
	m_bAnimated = GetFieldSafeBool(j, "animated", false);
	m_bRequireColons = GetFieldSafeBool(j, "require_colons", true);

	auto it = j.find("user");
	if (it != j.end())
	{
		Profile* pf = GetProfileCache()->LoadProfile(0, it.value());
		if (pf)
			m_user = pf->m_snowflake;
	}
}
