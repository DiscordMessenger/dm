#include "TextManager.hpp"
#include "Main.hpp"

// Text manager singleton
static TextManager g_TextManager;
TextManager* GetTextManager() {
	return &g_TextManager;
}

static TCHAR loadBuffer[65536];

std::string TextManager::GetString(int id)
{
	LoadIfNeeded(id);
	return m_loadedStrings[id];
}

LPCTSTR TextManager::GetTString(int id)
{
	LoadIfNeeded(id);
	return m_loadedTStrings[id];
}

void TextManager::LoadIfNeeded(int id)
{
	if (!m_loadedStrings[id].empty())
		return;

	int strLen = LoadString(g_hInstance, id, loadBuffer, _countof(loadBuffer));
	if (!strLen) {
		_tcscpy(loadBuffer, TEXT("[string doesn't exist]"));
		strLen = int(_tcslen(loadBuffer));
	}

	m_loadedStrings[id] = MakeStringFromTString(loadBuffer);

	TCHAR* tchr = new TCHAR[strLen + 1];
	_tcscpy(tchr, loadBuffer);
	m_loadedTStrings[id] = tchr;
}
