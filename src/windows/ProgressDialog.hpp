#pragma once

#include <windows.h>
#include <string>
#include "models/Snowflake.hpp"

class ProgressDialog
{
public:
	struct UpdateInfo {
		size_t offset;
		uint64_t time;
	};

public:
	static void Show(const std::string& fileName, Snowflake key, bool isUploading, HWND hWnd);
	static void Done(Snowflake key);
	static bool Update(Snowflake key, size_t offset, size_t length);

private:
	static INT_PTR CALLBACK DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static UpdateInfo& FindUpdateOneSecondAgo();
	static void ResetUpdates();

private:
	static HWND m_hwnd;
	static Snowflake m_key;
	static std::string m_fileName;
	static size_t m_offset, m_length, m_lastOffset;
	static bool m_bCanceling;
	static bool m_bDirection;
	static UpdateInfo m_updateInfo[256];
	static size_t m_updateInfoHead;
};

