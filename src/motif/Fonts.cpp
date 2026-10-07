#include "Fonts.hpp"

#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>
#include <sys/stat.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#ifndef DM_DATADIR
#define DM_DATADIR "/usr/local/share/discord-messenger"
#endif

namespace
{
	FT_Library g_lib;

	struct Face
	{
		FT_Face face = nullptr;
		int px = 0; // size currently set
	};

	// The faces of each style, then the fallbacks tried for characters a
	// style's face lacks.
	Face g_style[FS_COUNT];
	std::vector<Face> g_fallback;

	const char* const g_styleFiles[FS_COUNT] = {
		"DejaVuSans.ttf",
		"DejaVuSans-Bold.ttf",
		"DejaVuSans-Oblique.ttf",
		"DejaVuSans-BoldOblique.ttf",
		"DejaVuSansMono.ttf",
		"DejaVuSansMono-Bold.ttf",
	};

	struct Glyph
	{
		std::vector<uint8_t> bits;
		int w = 0, h = 0, left = 0, top = 0, advance = 0;
	};

	// key: face pointer, pixel size, glyph index
	struct GlyphKey
	{
		const void* face;
		int px;
		unsigned index;
		bool operator<(const GlyphKey& o) const {
			if (face != o.face) return face < o.face;
			if (px != o.px) return px < o.px;
			return index < o.index;
		}
	};

	std::map<GlyphKey, Glyph> g_glyphs;
	std::map<std::pair<int, int>, std::pair<int, int>> g_metrics; // (style, px) -> (ascent, descent)

	bool Exists(const std::string& p)
	{
		struct stat st;
		return stat(p.c_str(), &st) == 0;
	}

	void SetSize(Face& f, int px)
	{
		if (f.px != px) {
			FT_Set_Pixel_Sizes(f.face, 0, px);
			f.px = px;
		}
	}

	// The face and glyph index that draw code point cp in style st.
	Face* Lookup(FontStyle st, unsigned cp, unsigned& index)
	{
		Face* f = &g_style[st];
		index = FT_Get_Char_Index(f->face, cp);
		if (index)
			return f;
		if (st != FS_REGULAR) {
			index = FT_Get_Char_Index(g_style[FS_REGULAR].face, cp);
			if (index)
				return &g_style[FS_REGULAR];
		}
		for (Face& fb : g_fallback) {
			index = FT_Get_Char_Index(fb.face, cp);
			if (index)
				return &fb;
		}
		index = 0; // .notdef box of the style's face
		return f;
	}

	const Glyph& Render(Face* f, unsigned index, int px)
	{
		GlyphKey key{ f->face, px, index };
		auto it = g_glyphs.find(key);
		if (it != g_glyphs.end())
			return it->second;

		Glyph& g = g_glyphs[key];
		SetSize(*f, px);
		if (FT_Load_Glyph(f->face, index, FT_LOAD_DEFAULT | FT_LOAD_TARGET_LIGHT) == 0 &&
			FT_Render_Glyph(f->face->glyph, FT_RENDER_MODE_NORMAL) == 0)
		{
			FT_GlyphSlot slot = f->face->glyph;
			FT_Bitmap& bm = slot->bitmap;
			g.w = (int) bm.width;
			g.h = (int) bm.rows;
			g.left = slot->bitmap_left;
			g.top = slot->bitmap_top;
			g.advance = (int) ((slot->advance.x + 32) >> 6);
			g.bits.resize((size_t) g.w * g.h);
			for (int y = 0; y < g.h; y++)
				memcpy(&g.bits[(size_t) y * g.w], bm.buffer + y * bm.pitch, g.w);
		}
		return g;
	}

	int Advance(FontStyle st, unsigned cp, int px)
	{
		if (cp == '\t')
			cp = ' ';
		unsigned index;
		Face* f = Lookup(st, cp, index);
		return Render(f, index, px).advance;
	}
}

unsigned DecodeUtf8(const char*& p, const char* end)
{
	unsigned char c = (unsigned char) *p++;
	if (c < 0x80)
		return c;
	int extra = c >= 0xf0 ? 3 : c >= 0xe0 ? 2 : c >= 0xc0 ? 1 : -1;
	if (extra < 0)
		return 0xfffd;
	unsigned cp = c & (0x3f >> extra);
	for (int i = 0; i < extra; i++) {
		if (p >= end || ((unsigned char) *p & 0xc0) != 0x80)
			return 0xfffd;
		cp = (cp << 6) | ((unsigned char) *p++ & 0x3f);
	}
	return cp;
}

bool Fonts::Init(std::string& err)
{
	if (FT_Init_FreeType(&g_lib)) {
		err = "FreeType could not start.";
		return false;
	}

	std::vector<std::string> dirs;
	if (const char* d = getenv("DM_FONT_DIR"))
		dirs.push_back(d);
	dirs.push_back(DM_DATADIR "/fonts");
	dirs.push_back("/opt/pkgsrc/share/fonts/X11/TTF");
	dirs.push_back("/usr/pkg/share/fonts/X11/TTF");
	dirs.push_back("/usr/share/fonts/truetype/dejavu");
	dirs.push_back("/usr/local/share/fonts");

	std::string dir;
	for (auto& d : dirs) {
		if (Exists(d + "/" + g_styleFiles[0])) {
			dir = d;
			break;
		}
	}
	if (dir.empty()) {
		err = "The DejaVu fonts were not found (looked in DM_FONT_DIR, " DM_DATADIR "/fonts and pkgsrc's TrueType directory).";
		return false;
	}

	for (int i = 0; i < FS_COUNT; i++)
	{
		std::string path = dir + "/" + g_styleFiles[i];
		if (FT_New_Face(g_lib, path.c_str(), 0, &g_style[i].face)) {
			// a missing style falls back to the regular face
			if (i == 0) {
				err = "Could not load " + path + ".";
				return false;
			}
			g_style[i].face = nullptr;
		}
	}
	for (int i = 1; i < FS_COUNT; i++)
		if (!g_style[i].face)
			g_style[i] = g_style[i >= FS_MONO ? (g_style[FS_MONO].face ? FS_MONO : FS_REGULAR) : FS_REGULAR];

	// Extra faces for scripts and symbols DejaVu lacks, when present.
	const char* const extra[] = {
		"NotoSansSymbols2-Regular.ttf", "NotoSansSymbols-Regular.ttf",
		"NotoSansCJK-Regular.ttc", "DroidSansFallback.ttf",
		"NotoEmoji-Regular.ttf", "Symbola.ttf",
	};
	for (const char* e : extra) {
		std::string path = dir + "/" + e;
		Face f;
		if (Exists(path) && FT_New_Face(g_lib, path.c_str(), 0, &f.face) == 0)
			g_fallback.push_back(f);
	}
	return true;
}

int Fonts::Ascent(FontStyle st, int px)
{
	auto key = std::make_pair((int) st, px);
	auto it = g_metrics.find(key);
	if (it == g_metrics.end()) {
		Face& f = g_style[st];
		SetSize(f, px);
		int asc = (int) ((f.face->size->metrics.ascender + 63) >> 6);
		int desc = (int) ((-f.face->size->metrics.descender + 63) >> 6);
		it = g_metrics.insert(std::make_pair(key, std::make_pair(asc, desc))).first;
	}
	return it->second.first;
}

int Fonts::Descent(FontStyle st, int px)
{
	Ascent(st, px);
	return g_metrics[std::make_pair((int) st, px)].second;
}

int Fonts::Measure(const char* s, size_t n, FontStyle st, int px)
{
	const char* p = s;
	const char* end = s + n;
	int w = 0;
	while (p < end)
		w += Advance(st, DecodeUtf8(p, end), px);
	return w;
}

int Fonts::Measure(const std::string& s, FontStyle st, int px)
{
	return Measure(s.data(), s.size(), st, px);
}

int Fonts::Draw(Canvas& c, int x, int y, const char* s, size_t n, FontStyle st, int px, Rgb color)
{
	const char* p = s;
	const char* end = s + n;
	int x0 = x;
	while (p < end)
	{
		unsigned cp = DecodeUtf8(p, end);
		if (cp == '\t')
			cp = ' ';
		unsigned index;
		Face* f = Lookup(st, cp, index);
		const Glyph& g = Render(f, index, px);
		if (g.w && g.h)
			c.BlendMask(x + g.left, y - g.top, g.bits.data(), g.w, g.h, g.w, color);
		x += g.advance;
	}
	return x - x0;
}

int Fonts::Draw(Canvas& c, int x, int y, const std::string& s, FontStyle st, int px, Rgb color)
{
	return Draw(c, x, y, s.data(), s.size(), st, px, color);
}

size_t Fonts::FitBytes(const char* s, size_t n, FontStyle st, int px, int maxWidth)
{
	const char* p = s;
	const char* end = s + n;
	const char* lastSpace = nullptr;
	int w = 0;
	while (p < end)
	{
		const char* start = p;
		unsigned cp = DecodeUtf8(p, end);
		if (cp == '\n')
			return start - s;
		w += Advance(st, cp, px);
		if (w > maxWidth) {
			if (lastSpace)
				return lastSpace - s;
			return start == s ? (size_t) (p - s) : (size_t) (start - s);
		}
		if (cp == ' ')
			lastSpace = p;
	}
	return n;
}

std::string Fonts::Elide(const std::string& s, FontStyle st, int px, int maxWidth)
{
	if (Measure(s, st, px) <= maxWidth)
		return s;
	static const std::string dots = "\xe2\x80\xa6"; // U+2026
	int room = maxWidth - Measure(dots, st, px);
	const char* p = s.data();
	const char* end = p + s.size();
	int w = 0;
	while (p < end) {
		const char* start = p;
		w += Advance(st, DecodeUtf8(p, end), px);
		if (w > room)
			return s.substr(0, start - s.data()) + dots;
	}
	return s;
}
