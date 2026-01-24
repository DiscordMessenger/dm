#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

// Use RAII to start and end a double buffering context.
// Draw data to HdcMem(), then it will be flushed to the provided
// HDC, such as a window's HDC.
class DoubleBufferingHelper
{
public:
	DoubleBufferingHelper(HDC hdc, const RECT& paintRect);
	~DoubleBufferingHelper();

	HDC HdcMem() const {
		return m_hdcMem;
	}

public:
	// create a rectangle region with the correct viewport offset.
	static HRGN CreateRectRgn(HDC hdc, RECT rect);

private:
	HDC m_hdc;
	HDC m_hdcMem;
	HGDIOBJ m_oldObj;
	HBITMAP m_hBitmap;
	RECT m_paintRect;
	POINT m_oldOrg;
	int m_prWidth, m_prHeight;
};
