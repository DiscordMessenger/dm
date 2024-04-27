#pragma once

#define WIN32_LEAN_AND_MEAN
#include <vector>
#include <windows.h>
#include <shellapi.h>

#include "../discord/Snowflake.hpp"

struct NotificationData
{
	Snowflake m_guild;
	Snowflake m_channel;
	Snowflake m_message;
	std::string m_title;
	std::string m_text;
};

class NotificationManager
{
public:
	~NotificationManager();
	void CreateNotification();
	void DeleteNotification();
	void AddNotification(NotificationData& data);

public: // really only Main.cpp should use this.
	void Callback(WPARAM wParam, LPARAM lParam);

private:
	void ShowPopupMenu(POINT pt, bool bRightButton);
};
