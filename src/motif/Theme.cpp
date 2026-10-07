#include "Theme.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <X11/Xresource.h>

#include "utils/Util.hpp"

static int g_textSize = 0;
static bool g_dark = false;
static bool g_darkSaved = false; // what the config file says (applies at the next start)

static const Palette g_light = {
	0xe3e5e8, 0xf2f3f5, 0x313338, 0x6d6f78, 0x5c5e66,
	0xd4d7dc, 0x060607, 0xe8eaed,
	0x060607, 0xf23f43, 0xffffff,
	0xffffff, 0x1e1f22, 0x6d6f78, 0x0060c0, 0x4752c4, 0xf2f3f5, 0xd4d7dc, 0xc4c9ce,
	0x23a55a, 0xf0b232, 0xf23f43, 0x80848e,
	nullptr, nullptr, nullptr,
};

static const Palette g_darkPalette = {
	0x1e1f22, 0x2b2d31, 0x949ba4, 0x80848e, 0x949ba4,
	0x404249, 0xf2f3f5, 0x35373c,
	0xf2f3f5, 0xf23f43, 0xffffff,
	0x313338, 0xdbdee1, 0x949ba4, 0x00a8fc, 0xc9cdfb, 0x2b2d31, 0x1e1f22, 0x4e5058,
	0x23a55a, 0xf0b232, 0xf23f43, 0x80848e,
	"#2b2d31", "#dbdee1", "#383a40",
};

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
			if (!strcmp(key, "theme"))
				g_dark = g_darkSaved = !strcmp(val, "dark");
			else if (!strcmp(key, "textsize"))
				SetTextSize(atoi(val));
		}
		fclose(f);
	}
	if (const char* e = getenv("DM_THEME"))
		g_dark = !strcmp(e, "dark");
	if (const char* e = getenv("DM_TEXT_SIZE"))
		SetTextSize(atoi(e));
}

void SaveMotifConfig()
{
	std::string path = ConfigPath(), tmp = path + ".new";
	FILE* f = fopen(tmp.c_str(), "w");
	if (!f)
		return;
	fprintf(f, "theme = %s\ntextsize = %d\n", g_darkSaved ? "dark" : "light", GetTextSize());
	if (fclose(f) == 0)
		rename(tmp.c_str(), path.c_str());
	else
		remove(tmp.c_str());
}

bool IsDarkTheme()
{
	return g_dark;
}

void SetDarkTheme(bool dark)
{
	g_darkSaved = dark;
	SaveMotifConfig();
}

const Palette& GetPalette()
{
	return g_dark ? g_darkPalette : g_light;
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

void ApplyThemeResources(Display* dpy)
{
	const Palette& p = GetPalette();
	if (!p.widgetBg)
		return;
	XrmDatabase db = XtDatabase(dpy);
	std::string lines[] = {
		std::string("DiscordMessenger*background: ") + p.widgetBg,
		std::string("DiscordMessenger*foreground: ") + p.widgetFg,
		std::string("DiscordMessenger*XmText*background: ") + p.textBg,
		std::string("DiscordMessenger*XmTextField*background: ") + p.textBg,
		std::string("DiscordMessenger*XmText*foreground: ") + p.widgetFg,
		std::string("DiscordMessenger*XmTextField*foreground: ") + p.widgetFg,
		"DiscordMessenger*useSchemes: none",
	};
	for (auto& l : lines)
		XrmPutLineResource(&db, l.c_str());
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
