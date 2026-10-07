#include "TextInterface_Motif.hpp"

#include <algorithm>
#include <vector>

#include "text/FormattedText.hpp"
#include "text/TextInterface.hpp"

static const int CODE_PAD = 4;      // inside multi-line code blocks
static const int QUOTE_INDENT = 12;

void MdFontFor(const DrawingContext* ctx, int f, FontStyle& st, int& px)
{
	px = ctx->px;
	if (f & (WORD_CODE | WORD_MLCODE)) {
		st = (f & WORD_STRONG) ? FS_MONOBOLD : FS_MONO;
		px = ctx->px - 1;
		return;
	}

	bool bold = (f & (WORD_STRONG | WORD_HEADER1 | WORD_HEADER2)) != 0;
	bool italic = (f & (WORD_ITALIC | WORD_ITALIE)) != 0;
	st = bold ? (italic ? FS_BOLDITALIC : FS_BOLD) : (italic ? FS_ITALIC : FS_REGULAR);

	if (f & WORD_HEADER1)
		px = ctx->px * 3 / 2 + 2;
	else if (f & WORD_HEADER2)
		px = ctx->px * 5 / 4 + 1;
	else if (f & WORD_SMALLER)
		px = ctx->px - 3;
}

// Text broken into lines: at newlines, and to fit maxWidth when it is > 0.
static void WrapLines(const std::string& s, FontStyle st, int px, int maxWidth, std::vector<std::string>& lines, bool& wrapped)
{
	wrapped = false;
	size_t pos = 0;
	for (;;)
	{
		size_t nl = s.find('\n', pos);
		std::string para = s.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
		if (maxWidth <= 0) {
			lines.push_back(para);
		}
		else {
			size_t off = 0;
			do {
				size_t n = Fonts::FitBytes(para.data() + off, para.size() - off, st, px, maxWidth);
				if (n < para.size() - off)
					wrapped = true;
				lines.push_back(para.substr(off, n));
				off += n;
			} while (off < para.size());
		}
		if (nl == std::string::npos)
			break;
		pos = nl + 1;
	}
}

Point MdMeasureString(DrawingContext* ctx, const String& word, int styleFlags, bool& outWasWordWrapped, int maxWidth)
{
	outWasWordWrapped = false;
	const std::string& s = word.GetWrapped();

	if (styleFlags & WORD_CEMOJI) {
		int h = MdLineHeight(ctx, styleFlags);
		return Point(h, h);
	}

	FontStyle st;
	int px;
	MdFontFor(ctx, styleFlags, st, px);
	int lh = Fonts::LineHeight(st, px);

	bool block = (styleFlags & (WORD_MLCODE | WORD_NOFORMAT)) != 0;
	int pad = (styleFlags & WORD_MLCODE) ? 2 * CODE_PAD : 0;

	std::vector<std::string> lines;
	WrapLines(s, st, px, block && maxWidth > 0 ? maxWidth - pad : 0, lines, outWasWordWrapped);

	int w = 0;
	for (auto& l : lines)
		w = std::max(w, Fonts::Measure(l, st, px));

	// a code block spans the whole width it was given
	if ((styleFlags & WORD_MLCODE) && maxWidth > 0)
		w = maxWidth - pad;

	return Point(w + pad, (int) lines.size() * lh + pad);
}

int MdLineHeight(DrawingContext* ctx, int styleFlags)
{
	FontStyle st;
	int px;
	MdFontFor(ctx, styleFlags, st, px);
	return Fonts::LineHeight(st, px) + 2;
}

int MdSpaceWidth(DrawingContext* ctx, int styleFlags)
{
	FontStyle st;
	int px;
	MdFontFor(ctx, styleFlags, st, px);
	return Fonts::Measure(" ", 1, st, px);
}

void MdDrawString(DrawingContext* ctx, const Rect& rect, const String& str, int styleFlags)
{
	if (!ctx->canvas)
		return;
	Canvas& c = *ctx->canvas;
	const std::string& s = str.GetWrapped();

	if (styleFlags & WORD_CEMOJI) {
		// Custom emoji images are not loaded yet: show the emoji's name.
		int h = rect.Height();
		c.FillRounded(rect.left, rect.top, h, h, 4, LerpRgb(ctx->bg, ctx->muted, 1, 3));
		size_t a = s.find(':'), b = s.find(':', a + 1);
		std::string name = (a != std::string::npos && b != std::string::npos) ? s.substr(a + 1, b - a - 1) : "?";
		if (!name.empty())
			Fonts::Draw(c, rect.left + 2, rect.top + h * 3 / 4, std::string(name, 0, 1), FS_BOLD, h / 2, ctx->fg);
		return;
	}

	FontStyle st;
	int px;
	MdFontFor(ctx, styleFlags, st, px);
	int asc = Fonts::Ascent(st, px);
	int lh = Fonts::LineHeight(st, px);

	Rgb color = ctx->fg;
	if (styleFlags & WORD_LINK)
		color = ctx->link;
	if (styleFlags & (WORD_MENTION | WORD_EVERYONE)) {
		color = ctx->mention;
		c.FillRounded(rect.left - 1, rect.top, rect.Width() + 2, rect.Height(), 3, LerpRgb(ctx->bg, ctx->mention, 15, 100));
	}
	if (styleFlags & WORD_SMALLER)
		color = ctx->muted;
	if ((styleFlags & WORD_CODE) && !(styleFlags & WORD_MLCODE))
		c.FillRounded(rect.left - 1, rect.top, rect.Width() + 2, rect.Height(), 3, ctx->codeBg);

	if ((styleFlags & (WORD_AFNEWLINE | WORD_QUOTE)) == (WORD_AFNEWLINE | WORD_QUOTE))
		c.Fill(rect.left - QUOTE_INDENT, rect.top, 3, rect.Height(), ctx->quoteBar);

	Rect rc = rect;
	if (styleFlags & WORD_MLCODE) {
		rc.left += CODE_PAD;
		rc.top += CODE_PAD;
		rc.right -= CODE_PAD;
		rc.bottom -= CODE_PAD;
	}

	bool block = (styleFlags & (WORD_MLCODE | WORD_NOFORMAT)) != 0;
	std::vector<std::string> lines;
	bool wrapped;
	WrapLines(s, st, px, block ? rc.Width() : 0, lines, wrapped);

	int y = rc.top;
	for (auto& l : lines) {
		int w = Fonts::Draw(c, rc.left, y + asc, l, st, px, color);
		if (styleFlags & (WORD_UNDERL | WORD_LINK))
			c.HLine(rc.left, y + asc + 2, w, color);
		y += lh;
	}
}

void MdDrawCodeBackground(DrawingContext* ctx, const Rect& rect)
{
	if (!ctx->canvas)
		return;
	ctx->canvas->Fill(rect.left, rect.top, rect.Width(), rect.Height(), ctx->codeBg);
	ctx->canvas->Frame(rect.left, rect.top, rect.Width(), rect.Height(), ctx->codeFrame);
}

void MdDrawForwardBackground(DrawingContext* ctx, const Rect& rect)
{
	if (!ctx->canvas)
		return;
	ctx->canvas->Fill(rect.left - 6, rect.top, 3, rect.Height(), ctx->quoteBar);
}

int MdGetQuoteIndentSize()
{
	return QUOTE_INDENT;
}

void MdSetClippingRect(DrawingContext* ctx, const Rect& rect)
{
	if (ctx->canvas)
		ctx->canvas->SetClip(rect.left, rect.top, rect.Width(), rect.Height());
}

void MdClearClippingRect(DrawingContext* ctx)
{
	if (ctx->canvas)
		ctx->canvas->ClearClip();
}
