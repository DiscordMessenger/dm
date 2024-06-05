#pragma once

#include <vector>
#include <string>
#include <cassert>
#include <windows.h>
#include <windowsx.h>
#include "WinUtils.hpp"

struct AutoCompleteMatch
{
	std::string str;
	std::string substr;
	float fuzzy = 0;

	AutoCompleteMatch() {}

	AutoCompleteMatch(const std::string& s, const std::string& ss, float fuz):
		str(s), substr(ss), fuzzy(fuz) {}

	bool operator<(const AutoCompleteMatch& oth) const
	{
		if (fuzzy != oth.fuzzy)
			return fuzzy > oth.fuzzy;

		int scr = strcmp(str.c_str(), oth.str.c_str());
		if (scr != 0)
			return scr < 0;

		return strcmp(substr.c_str(), oth.substr.c_str());
	}
};

class AutoComplete
{
public:
	typedef void(*LookUpFunction) (void* context, const std::string& lookup, char query, std::vector<AutoCompleteMatch>& matchesFound);

public:
	AutoComplete() {};
	~AutoComplete();

	// Updates the autocomplete widget with the text in the edit control.
	// If NULL, text is fetched using GetWindowText.
	// The pointer passed into here is not freed.
	void Update(LPCTSTR textInEditControl = NULL, int length = 0);

	// This is used to change the selection of the autocompletion list while
	// focused on the main window's edit control.
	bool HandleKeyMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

	// This is used to apply the auto-completion when pressing ENTER or TAB.
	// Returns whether or not the key press was handled.
	bool HandleCharMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

	// Sets the managed edit control.
	void SetEdit(HWND hWnd);

	// Sets the font used by the autocomplete window.
	void SetFont(HFONT hFont);

	// Sets the lookup function used by the autocomplete window.
	void SetLookup(LookUpFunction function);

	// Sets the lookup context passed in as parameter to the lookup function.
	void SetLookupContext(void* context);

	// Checks if this is active.
	bool IsActive() const
	{
		return m_hwnd != NULL;
	}

private:
	void ShowOrMove(POINT pt);
	void Hide();
	void Commit();
	int GetSelectionIndex();
	LRESULT HandleCustomDraw(HWND hWnd, NMLVCUSTOMDRAW* pInfo);
	LRESULT HandleGetDispInfo(HWND hWnd, NMLVDISPINFO* pInfo);

	// The private function of ::Update(). Here the textInEditControl MUST be valid. 
	void _Update(LPCTSTR textInEditControl, int length);

	static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

public:
	static bool InitializeClass();

private:
	HWND m_hwnd = NULL;
	HWND m_listHwnd = NULL;

	// This is not part of the autocomplete instance itself, instead, AutoComplete
	// functions on top of this edit control.
	HWND m_editHwnd = NULL;

	HFONT m_hFont = NULL;

	int m_widthColumn1 = 0;
	int m_widthColumn2 = 0;
	bool m_bReachedMaxSizeY = false;
	bool m_bHadFirstArrowPress = false;
	int m_startAt = 0; // place where auto-completion starts.

	std::vector<AutoCompleteMatch> m_matches;

	LookUpFunction m_lookup;
	void* m_pLookupContext = NULL;

	TCHAR m_dispInfoBuffer1[256];
	TCHAR m_dispInfoBuffer2[256];
};
