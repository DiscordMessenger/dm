#include "Profile.hpp"
#include "Util.hpp"

std::string Profile::GetUsername() const
{
	std::string un = m_name;
	if (m_discrim)
		un += "#" + FormatDiscrim(m_discrim);
	return un;
}

float Profile::FuzzyMatch(const char* check, Snowflake guild) const
{
	return std::max({
		CompareFuzzy(GetName(guild), check),
		CompareFuzzy(m_globalName, check),
		CompareFuzzy(m_name, check)
	});
}
