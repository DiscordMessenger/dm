#include "TextInterface_Win32.hpp"
#include "WinUtils.hpp"
#include "AvatarCache.hpp"

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
	if (styleFlags & WORD_HEADER1)
		return (styleFlags & WORD_ITALIC) ? 10 : 9;
	if (styleFlags & WORD_HEADER2)
		return (styleFlags & WORD_ITALIC) ? 12 : 11;

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

	if (styleFlags & WORD_CEMOJI)
	{
		// Is an emoji.  Therefore, render it as a square and go off.
		int height = MdLineHeight(context, styleFlags);
		rect.left = rect.top = 0;
		rect.right = rect.bottom = height;
	}
	else
	{
		if (maxWidth > 0) {
			rect.left = rect.top = 0;
			rect.right = maxWidth;
			rect.bottom = 10000000;
			DrawText(hdc, word.GetWrapped(), -1, &rect, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX | DT_EDITCONTROL);
		}
		else {
			DrawText(hdc, word.GetWrapped(), -1, &rect, DT_CALCRECT | DT_NOPREFIX);
		}

		if (styleFlags & (WORD_MLCODE | WORD_NOFORMAT)) {
			SIZE sz;
			GetTextExtentPoint(hdc, word.GetWrapped(), (int) word.GetSize(), &sz);

			outWasWordWrapped = sz.cx > rect.right - rect.left;

			if (styleFlags & WORD_MLCODE) {
				rect.right += 8;
				rect.bottom += 8;
			}
		}
	}

	SelectObject(hdc, old);

	return { rect.right - rect.left, rect.bottom - rect.top };
}
int GetProfilePictureSize();
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
	HDC hdc = context->m_hdc;

	if (styleFlags & WORD_CEMOJI) {
		// Parse the snowflake by hand because why not
		Snowflake parsed = 0;
		LPCTSTR wrapped = str.GetWrapped();
		size_t size = str.GetSize();

		for (size_t i = 0, colonsSeen = 0; i < size; i++) {
			if (wrapped[i] == (TCHAR) ':')
				colonsSeen++;

			if (colonsSeen > 2)
				break;

			if (colonsSeen == 2) {
				if (wrapped[i] >= (TCHAR) '0' && wrapped[i] <= (TCHAR) '9')
					parsed = parsed * 10 + int(wrapped[i] - (TCHAR) '0');
			}
		}

		int height = MdLineHeight(context, styleFlags);
		if (styleFlags & WORD_HEADER1)
			height = GetProfilePictureSize();

		std::string nameRaw = "EMOJI_" + std::to_string(parsed);
		std::string name = GetAvatarCache()->AddImagePlace(nameRaw, eImagePlace::EMOJIS, nameRaw, parsed, height);

		bool hasAlpha = false;
		HBITMAP hbm = GetAvatarCache()->GetImage(nameRaw, hasAlpha)->GetFirstFrame();

		bool shouldResize = GetProfilePictureSize() != height;

		if (shouldResize)
		{
			HBRUSH hbr = CreateSolidBrush(context->m_bkColor);
			HBITMAP hnbm = ResizeWithBackgroundColor(hdc, hbm, hbr, hasAlpha, height, height, GetProfilePictureSize(), GetProfilePictureSize());
			DrawBitmap(hdc, hnbm, rect.left, rect.top, NULL, CLR_NONE, height, height, false);
			DeleteBitmap(hnbm);
			DeleteBrush(hbr);
		}
		else
		{
			DrawBitmap(hdc, hbm, rect.left, rect.top, NULL, CLR_NONE, height, height, hasAlpha);
		}
		return;
	}

	if (styleFlags & WORD_MLCODE) {
		rc.left   += 4;
		rc.top    += 4;
		rc.right  -= 4;
		rc.bottom -= 4;
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
	if (styleFlags & WORD_CEMOJI) {
		oldColor = SetTextColor(context->m_hdc, RGB(255, 0, 0));
		setColor = true;
	}
	if (styleFlags & (WORD_MENTION | WORD_EVERYONE)) {
		oldColor = SetTextColor(context->m_hdc, COLOR_MENT);
		setColor = true;
		oldColorBG = SetBkColor(context->m_hdc, LerpColor(context->m_bkColor, COLOR_MENT, 10, 100));
		setColorBG = true;
	}
	if (styleFlags & (WORD_CODE | WORD_MLCODE)) {
		oldColorBG = SetBkColor(context->m_hdc, context->InvertIfNeeded(GetSysColor(COLOR_WINDOW)));
		setColorBG = true;
		flags |= DT_WORDBREAK | DT_EDITCONTROL;
	}
	if (styleFlags & WORD_NOFORMAT) {
		flags |= DT_WORDBREAK | DT_EDITCONTROL;
	}

	HFONT font = MdDetermineFont(styleFlags);
	HGDIOBJ old = SelectObject(hdc, font);
	if ((styleFlags & (WORD_AFNEWLINE | WORD_QUOTE)) == (WORD_AFNEWLINE | WORD_QUOTE)) {
		// draw a rectangle to signify the quote
		RECT rc2 = rc;
		rc2.left -= ScaleByDPI(SIZE_QUOTE_INDENT);
		rc2.right = rc2.left + ScaleByDPI(3);
		FillRect(hdc, &rc2, ri::GetSysColorBrush(COLOR_SCROLLBAR));
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

	if (context->m_bInvertTextColor) {
		COLORREF old = ri::SetDCBrushColor(context->m_hdc, context->InvertIfNeeded(GetSysColor(COLOR_WINDOW)));
		FillRect(context->m_hdc, &rc, ri::GetDCBrush());
		ri::SetDCBrushColor(context->m_hdc, old);
	}
	else {
		FillRect(context->m_hdc, &rc, ri::GetSysColorBrush(COLOR_WINDOW));
	}

	ri::DrawEdge(context->m_hdc, &rc, BDR_SUNKEN, BF_RECT);
}

void MdDrawForwardBackground(DrawingContext* context, const Rect& rect)
{
	RECT rc = RectToNative(rect);

	rc.right = rc.left - ScaleByDPI(3);
	rc.left = rc.right - ScaleByDPI(3);

	if (context->m_bInvertTextColor) {
		COLORREF old = ri::SetDCBrushColor(context->m_hdc, context->InvertIfNeeded(GetSysColor(COLOR_GRAYTEXT)));
		FillRect(context->m_hdc, &rc, ri::GetDCBrush());
		ri::SetDCBrushColor(context->m_hdc, old);
	}
	else {
		FillRect(context->m_hdc, &rc, ri::GetSysColorBrush(COLOR_GRAYTEXT));
	}
}

int MdGetQuoteIndentSize()
{
	return ScaleByDPI(SIZE_QUOTE_INDENT);
}
