#include "TextInterface_Win32.hpp"
#include "WinUtils.hpp"

void String::Set(const std::string& text)
{
	Clear();
	m_content = ConvertCppStringToTString(text, &m_size);
}

extern HFONT* g_FntMdStyleArray[FONT_TYPE_COUNT];

// FORMATTEDTEXT INTERFACE
static int MdDetermineFontID(int styleFlags) {
	if (styleFlags & (WORD_CODE | WORD_MLCODE))
		return 8;

	int index = 0;
	if (styleFlags & WORD_STRONG) index |= 1;
	if (styleFlags & WORD_ITALIC) index |= 2;
	if (styleFlags & WORD_ITALIE) index |= 2;
	if (styleFlags & WORD_UNDERL) index |= 4;
	return index;
}

static HFONT MdGetFontByID(int id) {
	return *(g_FntMdStyleArray[id]);
}

static HFONT MdDetermineFont(int styleFlags)
{
	return MdGetFontByID(MdDetermineFontID(styleFlags));
}

Point MdMeasureString(DrawingContext* context, const String& word, int styleFlags, bool& outWasWordWrapped, int maxWidth)
{
	outWasWordWrapped = false;
	RECT rect;
	HDC hdc = context->m_hdc;
	HFONT font = MdDetermineFont(styleFlags);
	HGDIOBJ old = SelectObject(hdc, font);

	if (maxWidth > 0) {
		rect.left = rect.top = 0;
		rect.right = maxWidth;
		rect.bottom = 10000000;
		DrawText(hdc, word.GetWrapped(), -1, &rect, DT_CALCRECT | DT_WORDBREAK);
	}
	else {
		DrawText(hdc, word.GetWrapped(), -1, &rect, DT_CALCRECT);
	}

	if (styleFlags & WORD_MLCODE) {
		SIZE sz;
		GetTextExtentPoint(hdc, word.GetWrapped(), word.GetSize(), &sz);

		outWasWordWrapped = sz.cx > rect.right - rect.left;

		rect.right += 8;
		rect.bottom += 8;
	}

	SelectObject(hdc, old);

	return { rect.right - rect.left, rect.bottom - rect.top };
}

int MdLineHeight(DrawingContext* context, int styleFlags)
{
	int fontId = MdDetermineFontID(styleFlags);
	if (context->m_cachedHeights[fontId])
		return context->m_cachedHeights[fontId];

	HDC hdc = context->m_hdc;
	HFONT font = MdGetFontByID(fontId);
	HGDIOBJ old = SelectObject(hdc, font);
	TEXTMETRIC tm{};
	GetTextMetrics(context->m_hdc, &tm);
	SelectObject(hdc, old);
	int ht = tm.tmHeight;
	context->m_cachedHeights[fontId] = ht;
	return ht;
}

int MdSpaceWidth(DrawingContext* context, int styleFlags)
{
	int fontId = MdDetermineFontID(styleFlags);
	if (context->m_cachedSpaceWidths[fontId])
		return context->m_cachedSpaceWidths[fontId];

	HDC hdc = context->m_hdc;
	HFONT font = MdGetFontByID(fontId);
	HGDIOBJ old = SelectObject(hdc, font);
	SIZE sz{};
	GetTextExtentPoint32(hdc, TEXT(" "), 1, &sz);
	SelectObject(hdc, old);
	int wd = sz.cx;
	context->m_cachedSpaceWidths[fontId] = wd;
	return wd;
}

void MdDrawString(DrawingContext* context, const Rect& rect, const String& str, int styleFlags)
{
	RECT rc = RectToNative(rect);

	if (styleFlags & WORD_MLCODE) {
		rc.left   += 4;
		rc.top    += 4;
		rc.right  -= 4;
		rc.bottom -= 4;
	}
	if (styleFlags & WORD_CODE) {
		// TODO: why should I do this?
		rc.left   -= 2;
		rc.right  -= 2;
	}
	COLORREF oldColor   = CLR_NONE;
	COLORREF oldColorBG = CLR_NONE;
	bool setColor   = false;
	bool setColorBG = false;
	int flags = DT_NOPREFIX;
	if (styleFlags & WORD_LINK) {
		oldColor = SetTextColor(context->m_hdc, COLOR_LINK);
		setColor = true;
	}
	if (styleFlags & (WORD_MENTION | WORD_EVERYONE)) {
		oldColor = SetTextColor(context->m_hdc, COLOR_MENT);
		setColor = true;
		oldColorBG = SetBkColor(context->m_hdc, LerpColor(context->m_bkColor, COLOR_MENT, 10, 100));
		setColorBG = true;
	}
	if (styleFlags & (WORD_CODE | WORD_MLCODE)) {
		oldColorBG = SetBkColor(context->m_hdc, GetSysColor(COLOR_WINDOW));
		setColorBG = true;
		flags |= DT_WORDBREAK;
	}
	HDC hdc = context->m_hdc;
	HFONT font = MdDetermineFont(styleFlags);
	HGDIOBJ old = SelectObject(hdc, font);
	if ((styleFlags & (WORD_AFNEWLINE | WORD_QUOTE)) == (WORD_AFNEWLINE | WORD_QUOTE)) {
		// draw a rectangle to signify the quote
		RECT rc2 = rc;
		rc2.left -= ScaleByDPI(SIZE_QUOTE_INDENT);
		rc2.right = rc2.left + ScaleByDPI(3);
		FillRect(hdc, &rc2, GetSysColorBrush(COLOR_SCROLLBAR));
	}
	DrawText(context->m_hdc, str.GetWrapped(), -1, &rc, flags);
	SelectObject(hdc, old);
	if (setColor)
		SetTextColor(hdc, oldColor);
	if (setColorBG)
		SetBkColor(hdc, oldColorBG);
}

void MdDrawCodeBackground(DrawingContext* context, const Rect& rect)
{
	RECT rc = RectToNative(rect);
	FillRect(context->m_hdc, &rc, GetSysColorBrush(COLOR_WINDOW));
	DrawEdge(context->m_hdc, &rc, BDR_SUNKEN, BF_RECT);
}

int MdGetQuoteIndentSize()
{
	return ScaleByDPI(SIZE_QUOTE_INDENT);
}
