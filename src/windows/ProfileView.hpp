#pragma once

#include "Main.hpp"

#define T_PROFILE_VIEW_CLASS TEXT("ProfileView")

class ProfileView
{
public:
	HWND m_hwnd = NULL;

	// An autonomous profile view fetches data from the current
	// DiscordInstance's profile instead of depending on someone
	// to update it.
	bool m_bAutonomous = false;

	// only used when an un-autonomous control is used
	LPTSTR m_name = NULL;
	LPTSTR m_username = NULL;
	std::string m_avatarLnk = "";
	eActiveStatus m_activeStatus = STATUS_OFFLINE;

public:
	ProfileView();
	~ProfileView();

	void Update();

	// NOTE: Pointers will be owned by the profile view after this.
	void SetData(LPTSTR name, LPTSTR username, eActiveStatus astatus, const std::string& avlnk);

public:
	static WNDCLASS g_ProfileViewClass;

	static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static void InitializeClass();

	static ProfileView* Create(HWND hwnd, LPRECT pRect, int id, bool autonomous);
};

