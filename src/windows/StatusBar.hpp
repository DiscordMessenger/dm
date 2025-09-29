#pragma once

#include <vector>
#include <string>
#include <windows.h>
#include <commctrl.h>

#include "../core/models/Snowflake.hpp"

#define IDT_EXPIRY    (1)
#define IDT_ANIMATION (2)

#define ANIMATION_MS (200)
#define ANIMATION_FRAMES (3)

struct TypingUser
{
	Snowflake m_key;
	std::string m_name;
	uint64_t m_startTimeMS;
};

class StatusBar
{
public:
	static StatusBar* Create(HWND hParent);
	static void CALLBACK TimerProc(HWND hWnd, UINT uMsg, UINT_PTR uTimerId, DWORD dwTime);

	~StatusBar();

private:
	void RemoveTypingNameInternal(Snowflake sf);
	void SetExpiryTimerIn(int ms);
	void StartAnimation();
	void StopAnimation();
	int GetNextExpiryTime();
	void OnExpiryTick();
	void OnAnimationTick();

public:
	// should only be called by Main.cpp
	void DrawItem(LPDRAWITEMSTRUCT lpDIS);
	void UpdateParts(int width);

	void AddTypingName(Snowflake sf, time_t startTime, const std::string& name);
	void RemoveTypingName(Snowflake sf);
	void ClearTypers();
	void UpdateCharacterCounter(int nChars, int nCharsMax);

public:
	HWND m_hwnd = NULL;

private:
	std::vector<TypingUser> m_typingUsers;
	UINT_PTR m_timerExpireEvent;
	UINT_PTR m_timerAnimEvent;
	int m_anim_frame_number = 0;
	RECT m_typing_status_rect;
	RECT m_typing_animation_rect;
};
