#include <algorithm>
#include "Profile.hpp"
#include "ProfileCache.hpp"
#include "Util.hpp"

float Profile::FuzzyMatch(const char* check, Snowflake guild) const
{
	return std::max({
		CompareFuzzy(GetName(guild), check),
		CompareFuzzy(m_globalName, check),
		CompareFuzzy(m_name, check)
	});
}

void Profile::PutNote() const
{
	GetProfileCache()->PutNote(m_snowflake, m_note);
}
