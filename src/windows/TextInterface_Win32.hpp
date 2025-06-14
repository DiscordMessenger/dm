#pragma once

#include <windows.h>
#include <commctrl.h>
#include "../discord/TextInterface.hpp"
#include "../discord/FormattedText.hpp"

#define COLOR_LINK    RGB(  0, 108, 235)
#define COLOR_MENT    RGB( 88, 101, 242)

#define SIZE_QUOTE_INDENT (10)
#define FONT_TYPE_COUNT (13)

struct DrawingContext {
	HDC m_hdc = NULL;
	int m_cachedHeights[FONT_TYPE_COUNT]{ 0 };
	int m_cachedSpaceWidths[FONT_TYPE_COUNT]{ 0 };
	bool m_bInvertTextColor = false;
	DrawingContext(HDC hdc) { m_hdc = hdc; }

	// Background color - Used for blending
	COLORREF m_bkColor = CLR_NONE;
	void SetBackgroundColor(COLORREF cr) {
		m_bkColor = cr;
	}
	void SetInvertTextColor(bool invert) {
		m_bInvertTextColor = invert;
	}
	COLORREF InvertIfNeeded(COLORREF cr) {
		if (m_bInvertTextColor)
			cr ^= 0xffffff;
		return cr;
	}
};

static RECT RectToNative(const Rect& rc) {
	RECT rcNative{ rc.left, rc.top, rc.right, rc.bottom };
	return rcNative;
}
