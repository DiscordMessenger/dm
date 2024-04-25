#pragma once

#include <string>
#include <vector>
#include <windows.h>
#include <commctrl.h>

class QuickSwitcher
{
public:
	static void ShowDialog();
	static INT_PTR CALLBACK DialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
	static void OnUpdateQuery(HWND hWnd, const std::string& query);
	static void SwitchToChannelAtIndex(int idx);
	static void SwitchToSelectedChannel(HWND hWnd);
	static void HandleGetDispInfo(NMLVDISPINFO* pInfo);
	static void HandleItemChanged(HWND hWnd, NMLISTVIEW* pInfo);
	static void HandleItemActivate(HWND hWnd, NMITEMACTIVATE* pInfo);
	static void CreateImageList();
	static void DestroyImageList();
};
