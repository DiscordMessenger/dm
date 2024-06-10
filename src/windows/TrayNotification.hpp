#pragma once

#include <windows.h>

class TrayNotification
{
public:
	~TrayNotification();
	void Initialize();
	void Deinitialize();

	// used by Frontend_Win32. Received from DiscordLibrary as a tip
	// to show the latest notification.
	void OnNotification();

	// really only Main.cpp should use this.
	void Callback(WPARAM wParam, LPARAM lParam);

private:
	bool m_bInitialized = false;
};

TrayNotification* GetTrayNotification();
