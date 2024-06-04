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
	float fuzzy = 0;
	AutoCompleteMatch(const std::string& s, float fuz) : str(s), fuzzy(fuz) {}

	bool operator<(const AutoCompleteMatch& oth) const
	{
		if (fuzzy != oth.fuzzy)
			return fuzzy < oth.fuzzy;

		return strcmp(str.c_str(), oth.str.c_str()) < 0;
	}
};

class AutoComplete
{
public:
	typedef void(*LookUpFunction) (const std::string& lookup, char query, std::vector<AutoCompleteMatch>& matchesFound);

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

	bool m_bReachedMaxSizeX = false;
	bool m_bReachedMaxSizeY = false;
	bool m_bHadFirstArrowPress = false;
	int m_startAt = 0; // place where auto-completion starts.

	std::vector<AutoCompleteMatch> m_matches;

	LookUpFunction m_lookup;
};
