#pragma once

#include <windows.h>

#include "../discord/NotificationManager.hpp"

class ShellNotification
{
public:
	~ShellNotification();
	void Initialize();
	void Deinitialize();

	// used by Frontend_Win32. Received from DiscordLibrary as a tip
	// to show the latest notification.
	void OnNotification();

	// really only Main.cpp should use this.
	void Callback(WPARAM wParam, LPARAM lParam);

private:
	void ShowBalloon(const std::string& titleString, const std::string& contents);
	void ShowBalloonForOneNotification(Notification* pNotif);
	void ShowBalloonForNotifications(const std::vector<Notification*>& pNotifs);

private:
	bool m_bInitialized = false;
	bool m_bBalloonActive = false;
	bool m_bAnyNotificationsSinceLastTime = false;
};

ShellNotification* GetShellNotification();
