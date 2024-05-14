#pragma once

#include <vector>
#include <string>
#include <windows.h>
#include <commctrl.h>

#include "../discord/Snowflake.hpp"

class ToolBar
{
public:
	static ToolBar* Create(HWND hParent);

	~ToolBar();

public:
public:
	HWND m_hwnd;

private:
	HIMAGELIST m_imageList;

	int m_iconGuild;
	int m_iconPinned;
	int m_iconMembers;
	int m_iconInbox;
	int m_iconSearch;
	int m_iconThreads;
};
