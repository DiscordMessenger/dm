#pragma once
#include "Main.hpp"

#define T_LOADING_MESSAGE_CLASS TEXT("LoadingMessage")

class LoadingMessage
{
public:
	HWND m_hwnd = NULL;

public:
	LoadingMessage();
	~LoadingMessage();

	void Show();
	void Hide();
	void DrawLoading(HDC hdc);
	void UpdateSize(int width, int height);

public:
	static WNDCLASS g_LoadingMessageClass;

	static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static void InitializeClass();

	static LoadingMessage* Create(HWND hwnd, LPRECT pRect, int comboId = 0);

private:
	HWND parHwnd = NULL;
	HICON m_icon = NULL;
	HICON m_dot_0 = NULL;
	HICON m_dot_1 = NULL;
	int m_icon_stage = 0;
	UINT_PTR m_timer_id = 0;
	RECT m_load_rect {};

	void CreateWnd();
};
