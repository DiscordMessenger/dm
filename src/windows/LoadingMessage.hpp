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

public:
	static WNDCLASS g_LoadingMessageClass;

	static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static void InitializeClass();

	static LoadingMessage* Create(HWND hwnd, LPRECT pRect, int comboId = 0);

private:
	HWND parHwnd = NULL;

	void CreateWnd();
};
