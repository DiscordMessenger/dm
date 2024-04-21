#include "../discord/TextInterface.hpp"
#include "WinUtils.hpp"

void String::Set(const std::string& text)
{
	Clear();
	m_content = ConvertCppStringToTString(text, &m_size);
}
