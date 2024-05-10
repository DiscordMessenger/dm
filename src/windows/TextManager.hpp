#pragma once

#include <map>
#include <string>
#include <windows.h>
#include "../resource.h"

class TextManager
{
public:
	std::string GetString(int id);
	LPCTSTR GetTString(int id);

private:
	std::map<int, std::string> m_loadedStrings;
	std::map<int, LPTSTR> m_loadedTStrings;

	void LoadIfNeeded(int id);
};

#define TmGetString(id)  GetTextManager()->GetString(id)
#define TmGetTString(id) GetTextManager()->GetTString(id)

TextManager* GetTextManager();
