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
	if (!strLen)
		lstrcpy(loadBuffer, TEXT("[string doesn't exist]"));

	m_loadedStrings[id] = MakeStringFromUnicodeString(loadBuffer);

	TCHAR* tchr = new TCHAR[strLen + 1];
	lstrcpy(tchr, loadBuffer);
	m_loadedTStrings[id] = tchr;
}
