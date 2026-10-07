#include "Theme.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <X11/Xresource.h>

#include "utils/Util.hpp"

static int g_textSize = 0;
static Palette g_palette = {
	0xd6d6d6, 0xd6d6d6, 0x000000, 0x5c5c5c, 0x404040,
	0xb0b0b0, 0x000000,
	0x000000, 0xd83a3a, 0xffffff,
	0xf4f4f4, 0x000000, 0x5c5c5c, 0x0040c0, 0x3c45b0, 0xe8e8e8, 0xb8b8b8, 0xa0a0a0,
	0x23a55a, 0xd89a10, 0xd83a3a, 0x80848e,
};

static Rgb PixelToRgb(Display* dpy, Colormap cmap, Pixel p)
{
	XColor xc;
	xc.pixel = p;
	XQueryColor(dpy, cmap, &xc);
	return MakeRgb(xc.red >> 8, xc.green >> 8, xc.blue >> 8);
}

static int Luma(Rgb c)
{
	return (RgbR(c) * 299 + RgbG(c) * 587 + RgbB(c) * 114) / 1000;
}

void InitPalette(Widget w)
{
	Pixel bg = 0, fg = 0;
	Colormap cmap = 0;
	XtVaGetValues(w, XmNbackground, &bg, XmNforeground, &fg, XmNcolormap, &cmap, NULL);
	Display* dpy = XtDisplay(w);
	Pixel top, bottom, select, fgCalc;
	XmGetColors(XtScreen(w), cmap, bg, &fgCalc, &top, &bottom, &select);

	Rgb b = PixelToRgb(dpy, cmap, bg);
	Rgb f = PixelToRgb(dpy, cmap, fg);
	Rgb sel = PixelToRgb(dpy, cmap, select);
	bool darkScheme = Luma(b) < 110;
	Rgb paper = darkScheme ? LerpRgb(b, 0x000000, 25, 100) : LerpRgb(b, 0xffffff, 70, 100);

	Palette& p = g_palette;
	p.guildBg = LerpRgb(b, f, 6, 100);
	p.listBg = b;
	p.listFg = f;
	p.listMuted = LerpRgb(f, b, 45, 100);
	p.listHeader = LerpRgb(f, b, 30, 100);
	p.selBg = sel;
	p.selFg = f;
	p.unread = f;
	p.msgBg = paper;
	p.msgFg = f;
	p.msgMuted = LerpRgb(f, paper, 45, 100);
	p.link = darkScheme ? 0x6cb4ff : 0x0040c0;
	p.mention = darkScheme ? 0xc9cdfb : 0x3c45b0;
	p.codeBg = LerpRgb(paper, f, 6, 100);
	p.codeFrame = LerpRgb(paper, f, 22, 100);
	p.quoteBar = LerpRgb(paper, f, 30, 100);
}

const Palette& GetPalette()
{
	return g_palette;
}

static std::string ConfigPath()
{
	return GetBasePath() + "/motif.conf";
}

void LoadMotifConfig()
{
	FILE* f = fopen(ConfigPath().c_str(), "r");
	if (f) {
		char line[256];
		while (fgets(line, sizeof line, f)) {
			char key[64], val[128];
			if (sscanf(line, " %63[^= ] = %127s", key, val) != 2)
				continue;
			if (!strcmp(key, "textsize"))
				SetTextSize(atoi(val));
		}
		fclose(f);
	}
	if (const char* e = getenv("DM_TEXT_SIZE"))
		SetTextSize(atoi(e));
}

void SaveMotifConfig()
{
	std::string path = ConfigPath(), tmp = path + ".new";
	FILE* f = fopen(tmp.c_str(), "w");
	if (!f)
		return;
	fprintf(f, "textsize = %d\n", GetTextSize());
	if (fclose(f) == 0)
		rename(tmp.c_str(), path.c_str());
	else
		remove(tmp.c_str());
}

int GetTextSize()
{
	return g_textSize ? g_textSize : 14;
}

void SetTextSize(int px)
{
	if (px >= 8 && px <= 40)
		g_textSize = px;
}

void ApplyTheme(DrawingContext& ctx)
{
	const Palette& p = GetPalette();
	ctx.px = GetTextSize();
	ctx.fg = p.msgFg;
	ctx.bg = p.msgBg;
	ctx.link = p.link;
	ctx.mention = p.mention;
	ctx.codeBg = p.codeBg;
	ctx.codeFrame = p.codeFrame;
	ctx.quoteBar = p.quoteBar;
	ctx.muted = p.msgMuted;
}

static unsigned NextCodePoint(const std::string& s, size_t& i)
{
	unsigned char c = (unsigned char) s[i++];
	if (c < 0x80)
		return c;
	int extra = c >= 0xf0 ? 3 : c >= 0xe0 ? 2 : c >= 0xc0 ? 1 : -1;
	if (extra < 0)
		return 0xfffd;
	unsigned cp = c & (0x3f >> extra);
	for (int k = 0; k < extra; k++) {
		if (i >= s.size() || ((unsigned char) s[i] & 0xc0) != 0x80)
			return 0xfffd;
		cp = (cp << 6) | ((unsigned char) s[i++] & 0x3f);
	}
	return cp;
}

std::string Utf8ToLatin1(const std::string& s)
{
	std::string out;
	out.reserve(s.size());
	for (size_t i = 0; i < s.size(); )
	{
		unsigned cp = NextCodePoint(s, i);
		if (cp < 0x100) {
			out += (char) cp;
			continue;
		}
		switch (cp) {
			case 0x2018: case 0x2019: case 0x201b: out += '\''; break;
			case 0x201c: case 0x201d: case 0x201f: out += '"'; break;
			case 0x2010: case 0x2011: case 0x2012: case 0x2013: case 0x2014: case 0x2015: out += '-'; break;
			case 0x2022: case 0x30fb: out += (char) 0xb7; break;
			case 0x2026: out += "..."; break;
			case 0x00a0: out += ' '; break;
			default:
				// emoji, symbols and joiners vanish; other scripts become '?'
				if ((cp >= 0x1f000 && cp < 0x20000) || (cp >= 0x2600 && cp < 0x2800) ||
					(cp >= 0xfe00 && cp < 0xfe10) || cp == 0x200d || (cp >= 0x2190 && cp < 0x2300) ||
					(cp >= 0x2b00 && cp < 0x2c00) || (cp >= 0xe0000 && cp < 0xe0080))
					break;
				out += '?';
		}
	}
	// emoji between words leave double or leading spaces
	size_t a = out.find_first_not_of(' ');
	if (a == std::string::npos)
		return s.empty() || s.find_first_not_of(' ') == std::string::npos ? s : std::string("?");
	return out.substr(a);
}

std::string Latin1ToUtf8(const std::string& s)
{
	std::string out;
	out.reserve(s.size());
	for (unsigned char c : s) {
		if (c < 0x80)
			out += (char) c;
		else {
			out += (char) (0xc0 | (c >> 6));
			out += (char) (0x80 | (c & 0x3f));
		}
	}
	return out;
}

XmString MakeXmString(const std::string& utf8)
{
	return XmStringCreateLocalized((char*) Utf8ToLatin1(utf8).c_str());
}
