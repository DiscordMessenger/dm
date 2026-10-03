#pragma once

#include <windows.h>

class WindowContainer
{
public:
	WindowContainer();
	~WindowContainer();
	
	static void InitializeClass();

	HWND InitWindow(DWORD dwExStyle, LPCTSTR className, LPCTSTR windowName, DWORD dwStyle, int X, int Y, int width, int height, HWND hwndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam);

	HWND GetHWND();
	HWND GetVerticalScrollbarHWND();
	HWND GetHorizontalScrollbarHWND();
	HWND GetContainerHWND();

	void GetScrollInfo(int nBar, SCROLLINFO* outScrollInfo);
	void SetScrollInfo(int nBar, SCROLLINFO* inScrollInfo, bool redraw);

	void KillWindow();

	WindowContainer* GetParentContainer(HWND hWnd);

private:
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
	// the contained HWND
	HWND m_hwnd = NULL;
	HWND m_parentHwnd = NULL;

	// the scroll bar HWNDs, if they opted into it
	HWND m_vscrollHwnd = NULL, m_hscrollHwnd = NULL;

	// whether to draw an edge around everything
	DWORD m_exStyle = 0, m_style = 0;
};

