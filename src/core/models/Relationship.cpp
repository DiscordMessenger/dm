#include "Relationship.hpp"
#include "../utils/Util.hpp"

void Relationship::Load(const nlohmann::json& j)
{
	m_id       = GetSnowflake(j, "id");
	m_userID   = GetSnowflake(j, "user_id");
	m_nickname = GetFieldSafe(j, "nickname");
	m_type     = (eRelationshipType)GetFieldSafeInt(j, "type");
}
