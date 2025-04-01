#pragma once

#include <string>
#include <vector>
#include <windows.h>
#include <commctrl.h>

class QuickSwitcher
{
public:
	static void ShowDialog();
	static LRESULT CALLBACK TextWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK HijackWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static INT_PTR CALLBACK DialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
	static void OnUpdateQuery(HWND hWnd, const std::string& query);
	static void SwitchToChannelAtIndex(int idx);
	static void SwitchToSelectedChannel(HWND hWnd);
	static void HandleGetDispInfo(NMLVDISPINFO* pInfo);
	static void HandleItemChanged(HWND hWnd, NMLISTVIEW* pInfo);
	static void HandleItemActivate(HWND hWnd, int iItem);
	static LRESULT HandleCustomDraw(HWND hWnd, NMLVCUSTOMDRAW* pInfo);
	static void CreateImageList();
	static void DestroyImageList();
	static int GetItemCount(HWND hWnd);
};
