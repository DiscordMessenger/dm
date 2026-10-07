#include "Theme.hpp"

#include <cstdlib>

static int g_textSize = 0;

int GetTextSize()
{
	if (!g_textSize) {
		const char* e = getenv("DM_TEXT_SIZE");
		g_textSize = e ? atoi(e) : 0;
		if (g_textSize < 8 || g_textSize > 40)
			g_textSize = 14;
	}
	return g_textSize;
}

void SetTextSize(int px)
{
	if (px >= 8 && px <= 40)
		g_textSize = px;
}

void ApplyTheme(DrawingContext& ctx)
{
	ctx.px = GetTextSize();
	ctx.fg = 0x1e1f22;
	ctx.bg = 0xffffff;
	ctx.link = 0x0060c0;
	ctx.mention = 0x4752c4;
	ctx.codeBg = 0xf2f3f5;
	ctx.codeFrame = 0xd4d7dc;
	ctx.quoteBar = 0xc4c9ce;
	ctx.muted = 0x6d6f78;
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
