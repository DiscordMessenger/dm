#pragma once

#include "Main.hpp"

#define T_PROFILE_VIEW_CLASS TEXT("ProfileView")

class ProfileView
{
public:
	HWND m_hwnd = NULL;

public:
	ProfileView();
	~ProfileView();

	void Update();

public:
	static WNDCLASS g_ProfileViewClass;

	static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static void InitializeClass();

	static ProfileView* Create(HWND hwnd, LPRECT pRect);
};

