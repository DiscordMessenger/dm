#include "Fonts.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>
#include <sys/stat.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H
#include FT_TRUETYPE_TAGS_H

#ifndef DM_DATADIR
#define DM_DATADIR "/usr/local/share/discord-messenger"
#endif

namespace
{
	FT_Library g_lib;

	struct Face
	{
		FT_Face face = nullptr;
		int px = 0;          // size currently set
		bool color = false;  // a colour bitmap font (emoji)
	};

	// The faces of each style, then the fallbacks tried for characters a
	// style's face lacks, then the colour emoji face.
	Face g_style[FS_COUNT];
	std::vector<Face> g_fallback;
	Face g_emoji;

	// The emoji face's ligatures (GSUB lookup type 4): first glyph -> the
	// other glyphs and the ligature, longest first.  This is how flags,
	// skin tones, keycaps and ZWJ sequences become one picture.
	struct Ligature
	{
		std::vector<unsigned> rest;
		unsigned glyph;
	};
	std::map<unsigned, std::vector<Ligature>> g_ligatures;

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
		std::vector<uint8_t> bits;   // coverage (outline fonts)
		std::vector<uint32_t> argb;  // colour pixels (emoji)
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

	// A glyph to draw and the text it stands for.
	struct Shaped
	{
		Face* face;
		unsigned index;
		const char* start;
		const char* end;
		unsigned cp;     // its first code point
	};

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

	unsigned Read16(const uint8_t* p) { return (p[0] << 8) | p[1]; }
	unsigned Read32(const uint8_t* p) { return ((unsigned) p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }

	// The glyphs a coverage table lists, in coverage order.
	void ReadCoverage(const uint8_t* t, size_t avail, std::vector<unsigned>& out)
	{
		if (avail < 4)
			return;
		unsigned format = Read16(t), count = Read16(t + 2);
		if (format == 1) {
			for (unsigned i = 0; i < count && 4 + 2 * i + 2 <= avail; i++)
				out.push_back(Read16(t + 4 + 2 * i));
		}
		else if (format == 2) {
			for (unsigned i = 0; i < count && 4 + 6 * i + 6 <= avail; i++) {
				unsigned start = Read16(t + 4 + 6 * i), end = Read16(t + 4 + 6 * i + 2);
				for (unsigned g = start; g <= end; g++)
					out.push_back(g);
			}
		}
	}

	void ReadLigatureSubtable(const uint8_t* base, const uint8_t* t, size_t size)
	{
		size_t avail = size - (t - base);
		if (avail < 6 || Read16(t) != 1)
			return;
		std::vector<unsigned> coverage;
		unsigned covOff = Read16(t + 2);
		if (covOff < avail)
			ReadCoverage(t + covOff, avail - covOff, coverage);
		unsigned setCount = Read16(t + 4);
		for (unsigned i = 0; i < setCount && i < coverage.size() && 6 + 2 * i + 2 <= avail; i++)
		{
			const uint8_t* set = t + Read16(t + 6 + 2 * i);
			if (set + 2 > base + size)
				continue;
			unsigned ligCount = Read16(set);
			for (unsigned j = 0; j < ligCount && set + 2 + 2 * j + 2 <= base + size; j++)
			{
				const uint8_t* lig = set + Read16(set + 2 + 2 * j);
				if (lig + 4 > base + size)
					continue;
				Ligature l;
				l.glyph = Read16(lig);
				unsigned comps = Read16(lig + 2);
				if (comps == 0 || lig + 4 + 2 * (comps - 1) > base + size)
					continue;
				for (unsigned k = 1; k < comps; k++)
					l.rest.push_back(Read16(lig + 4 + 2 * (k - 1)));
				g_ligatures[coverage[i]].push_back(l);
			}
		}
	}

	void LoadLigatures(FT_Face face)
	{
		FT_ULong size = 0;
		if (FT_Load_Sfnt_Table(face, TTAG_GSUB, 0, NULL, &size) || size < 10)
			return;
		std::vector<uint8_t> buf(size);
		if (FT_Load_Sfnt_Table(face, TTAG_GSUB, 0, buf.data(), &size))
			return;
		const uint8_t* g = buf.data();
		unsigned lookupList = Read16(g + 8);
		if (lookupList + 2 > size)
			return;
		const uint8_t* ll = g + lookupList;
		unsigned count = Read16(ll);
		for (unsigned i = 0; i < count && lookupList + 2 + 2 * i + 2 <= size; i++)
		{
			const uint8_t* lookup = ll + Read16(ll + 2 + 2 * i);
			if (lookup + 6 > g + size)
				continue;
			unsigned type = Read16(lookup), subCount = Read16(lookup + 4);
			for (unsigned k = 0; k < subCount && lookup + 6 + 2 * k + 2 <= g + size; k++)
			{
				const uint8_t* sub = lookup + Read16(lookup + 6 + 2 * k);
				unsigned t = type;
				if (t == 7 && sub + 8 <= g + size) {
					// an extension: the real subtable is further away
					t = Read16(sub + 2);
					sub = sub + Read32(sub + 4);
				}
				if (t == 4 && sub < g + size)
					ReadLigatureSubtable(g, sub, size);
			}
		}
		for (auto& e : g_ligatures)
			std::stable_sort(e.second.begin(), e.second.end(),
				[](const Ligature& a, const Ligature& b) { return a.rest.size() > b.rest.size(); });
	}

	// Code points drawn as emoji pictures even where a text font has them
	// (Emoji_Presentation in the BMP, and the pictographs above it).
	bool EmojiPresentation(unsigned cp)
	{
		if (cp >= 0x1f000 && cp <= 0x1faff)
			return cp < 0x1f100 || cp > 0x1f1e5; // not the enclosed letters
		static const unsigned ranges[][2] = {
			{ 0x231a, 0x231b }, { 0x23e9, 0x23ec }, { 0x23f0, 0x23f0 }, { 0x23f3, 0x23f3 },
			{ 0x25fd, 0x25fe }, { 0x2614, 0x2615 }, { 0x2648, 0x2653 }, { 0x267f, 0x267f },
			{ 0x2693, 0x2693 }, { 0x26a1, 0x26a1 }, { 0x26aa, 0x26ab }, { 0x26bd, 0x26be },
			{ 0x26c4, 0x26c5 }, { 0x26ce, 0x26ce }, { 0x26d4, 0x26d4 }, { 0x26ea, 0x26ea },
			{ 0x26f2, 0x26f3 }, { 0x26f5, 0x26f5 }, { 0x26fa, 0x26fa }, { 0x26fd, 0x26fd },
			{ 0x2705, 0x2705 }, { 0x270a, 0x270b }, { 0x2728, 0x2728 }, { 0x274c, 0x274c },
			{ 0x274e, 0x274e }, { 0x2753, 0x2755 }, { 0x2757, 0x2757 }, { 0x2795, 0x2797 },
			{ 0x27b0, 0x27b0 }, { 0x27bf, 0x27bf }, { 0x2b1b, 0x2b1c }, { 0x2b50, 0x2b50 },
			{ 0x2b55, 0x2b55 },
		};
		for (auto& r : ranges)
			if (cp >= r[0] && cp <= r[1])
				return true;
		return false;
	}

	bool IsRegional(unsigned cp) { return cp >= 0x1f1e6 && cp <= 0x1f1ff; }

	// Code points that continue an emoji sequence.
	bool ContinuesEmoji(unsigned cp)
	{
		return cp == 0xfe0f || cp == 0x200d || cp == 0x20e3 ||
			(cp >= 0x1f3fb && cp <= 0x1f3ff) || (cp >= 0xe0020 && cp <= 0xe007f);
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
		if (g_emoji.face) {
			index = FT_Get_Char_Index(g_emoji.face, cp);
			if (index)
				return &g_emoji;
		}
		index = 0; // .notdef box of the style's face
		return f;
	}

	// Breaks UTF-8 text into the glyphs that draw it.
	void Shape(const char* s, size_t n, FontStyle st, std::vector<Shaped>& out)
	{
		out.clear();
		const char* p = s;
		const char* end = s + n;
		while (p < end)
		{
			const char* start = p;
			unsigned cp = DecodeUtf8(p, end);
			if (cp == '\t' || cp == '\n')
				cp = ' ';

			// what follows decides the emoji presentation of some code points
			unsigned next = 0;
			if (p < end) {
				const char* q = p;
				next = DecodeUtf8(q, end);
			}
			bool emoji = g_emoji.face && FT_Get_Char_Index(g_emoji.face, cp) &&
				(EmojiPresentation(cp) || next == 0xfe0f || next == 0x20e3 ||
				 (IsRegional(cp) && IsRegional(next)));

			if (!emoji) {
				if (cp == 0xfe0f || cp == 0xfe0e || cp == 0x200d) {
					// variation selectors and joiners outside emoji: nothing
					if (!out.empty())
						out.back().end = p;
					continue;
				}
				Shaped g;
				g.face = Lookup(st, cp, g.index);
				g.start = start;
				g.end = p;
				g.cp = cp;
				out.push_back(g);
				continue;
			}

			// gather the sequence: modifiers, joiners and what they join,
			// a flag's second letter
			struct Cp { unsigned cp; const char* start; const char* end; };
			std::vector<Cp> seq;
			seq.push_back(Cp{ cp, start, p });
			bool flagDone = false;
			while (p < end)
			{
				const char* q = p;
				unsigned c = DecodeUtf8(q, end);
				unsigned prev = seq.back().cp;
				bool take = ContinuesEmoji(c) || prev == 0x200d ||
					(IsRegional(cp) && IsRegional(c) && !flagDone && seq.size() == 1);
				if (!take)
					break;
				if (IsRegional(c))
					flagDone = true;
				seq.push_back(Cp{ c, p, q });
				p = q;
			}

			// glyphs of the emoji face, then its ligatures, longest first
			std::vector<unsigned> gids;
			for (auto& c : seq)
				gids.push_back(FT_Get_Char_Index(g_emoji.face, c.cp));
			size_t i = 0;
			while (i < seq.size())
			{
				size_t used = 1;
				unsigned glyph = gids[i];
				auto lit = g_ligatures.find(gids[i]);
				if (gids[i] && lit != g_ligatures.end()) {
					// exactly, or passing over variation selectors (keycaps
					// and some flags are listed without them)
					for (int skipVS = 0; skipVS < 2 && used == 1; skipVS++)
					for (auto& l : lit->second) {
						size_t j = i + 1, k = 0;
						for (; k < l.rest.size() && j < seq.size(); j++) {
							if (skipVS && seq[j].cp == 0xfe0f && gids[j] != l.rest[k])
								continue;
							if (gids[j] != l.rest[k])
								break;
							k++;
						}
						if (k == l.rest.size()) {
							glyph = l.glyph;
							used = j - i;
							break;
						}
					}
				}
				unsigned c = seq[i].cp;
				bool invisible = used == 1 && (c == 0xfe0f || c == 0xfe0e || c == 0x200d ||
					(c >= 0xe0020 && c <= 0xe007f));
				if (invisible || (used == 1 && !glyph)) {
					if (!out.empty())
						out.back().end = seq[i].end;
					i++;
					continue;
				}
				Shaped g;
				g.face = &g_emoji;
				g.index = glyph;
				g.start = seq[i].start;
				g.end = seq[i + used - 1].end;
				g.cp = c;
				out.push_back(g);
				i += used;
			}
		}
	}

	// Emoji are drawn a little larger than the text around them.
	int EmojiSize(int px)
	{
		return std::max(8, px * 6 / 5);
	}

	// A colour glyph from the emoji face's bitmap strike, scaled to px.
	void RenderColor(Face* f, unsigned index, int px, Glyph& g)
	{
		FT_Face face = f->face;
		if (face->num_fixed_sizes > 0 && FT_Select_Size(face, 0))
			return;
		if (FT_Load_Glyph(face, index, FT_LOAD_COLOR) != 0)
			return;
		FT_GlyphSlot slot = face->glyph;
		FT_Bitmap& bm = slot->bitmap;
		if (bm.pixel_mode != FT_PIXEL_MODE_BGRA || !bm.width || !bm.rows)
			return;

		int strike = face->num_fixed_sizes > 0 ? (int) (face->available_sizes[0].y_ppem >> 6) : 109;
		if (strike <= 0)
			strike = 109;
		int target = EmojiSize(px);
		double scale = (double) target / strike;
		int sw = (int) bm.width, sh = (int) bm.rows;
		g.w = std::max(1, (int) (sw * scale + 0.5));
		g.h = std::max(1, (int) (sh * scale + 0.5));
		g.left = (int) (slot->bitmap_left * scale);
		g.top = (int) (slot->bitmap_top * scale + 0.5);
		g.advance = std::max(g.w, (int) ((slot->advance.x >> 6) * scale + 0.5));
		g.argb.assign((size_t) g.w * g.h, 0);

		// area average of premultiplied BGRA, out as straight ARGB
		for (int y = 0; y < g.h; y++)
		{
			int y0 = y * sh / g.h, y1 = std::max(y0 + 1, (y + 1) * sh / g.h);
			for (int x = 0; x < g.w; x++)
			{
				int x0 = x * sw / g.w, x1 = std::max(x0 + 1, (x + 1) * sw / g.w);
				unsigned b = 0, gr = 0, r = 0, a = 0, n = 0;
				for (int yy = y0; yy < y1 && yy < sh; yy++)
				for (int xx = x0; xx < x1 && xx < sw; xx++) {
					const uint8_t* q = bm.buffer + yy * bm.pitch + xx * 4;
					b += q[0]; gr += q[1]; r += q[2]; a += q[3];
					n++;
				}
				if (!n || !a)
					continue;
				unsigned A = a / n;
				// un-premultiply: colour sum / alpha sum
				unsigned R = std::min(255u, r * 255 / a), G = std::min(255u, gr * 255 / a), B = std::min(255u, b * 255 / a);
				g.argb[(size_t) y * g.w + x] = (A << 24) | (R << 16) | (G << 8) | B;
			}
		}
	}

	const Glyph& Render(Face* f, unsigned index, int px)
	{
		GlyphKey key{ f->face, px, index };
		auto it = g_glyphs.find(key);
		if (it != g_glyphs.end())
			return it->second;

		Glyph& g = g_glyphs[key];
		if (f->color) {
			RenderColor(f, index, px, g);
			return g;
		}
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

	int Advance(const Shaped& s, int px)
	{
		return Render(s.face, s.index, px).advance;
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
	};
	for (const char* e : extra) {
		std::string path = dir + "/" + e;
		Face f;
		if (Exists(path) && FT_New_Face(g_lib, path.c_str(), 0, &f.face) == 0)
			g_fallback.push_back(f);
	}

	// Colour emoji: Noto Color Emoji (CBDT bitmaps), from the same places.
	for (auto& d : dirs) {
		std::string path = d + "/NotoColorEmoji.ttf";
		if (Exists(path) && FT_New_Face(g_lib, path.c_str(), 0, &g_emoji.face) == 0) {
			if (FT_HAS_COLOR(g_emoji.face)) {
				g_emoji.color = true;
				LoadLigatures(g_emoji.face);
			}
			else {
				FT_Done_Face(g_emoji.face);
				g_emoji.face = nullptr;
			}
			break;
		}
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
	std::vector<Shaped> glyphs;
	Shape(s, n, st, glyphs);
	int w = 0;
	for (auto& g : glyphs)
		w += Advance(g, px);
	return w;
}

int Fonts::Measure(const std::string& s, FontStyle st, int px)
{
	return Measure(s.data(), s.size(), st, px);
}

int Fonts::Draw(Canvas& c, int x, int y, const char* s, size_t n, FontStyle st, int px, Rgb color)
{
	std::vector<Shaped> glyphs;
	Shape(s, n, st, glyphs);
	int x0 = x;
	for (auto& sg : glyphs)
	{
		const Glyph& g = Render(sg.face, sg.index, px);
		if (!g.argb.empty())
			c.BlendArgb(x + g.left, y - g.top, g.argb.data(), g.w, g.h, g.w);
		else if (g.w && g.h)
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
	// up to the first newline only
	const char* nl = (const char*) memchr(s, '\n', n);
	size_t len = nl ? (size_t) (nl - s) : n;

	std::vector<Shaped> glyphs;
	Shape(s, len, st, glyphs);
	const char* lastSpace = nullptr;
	int w = 0;
	for (size_t i = 0; i < glyphs.size(); i++)
	{
		const Shaped& g = glyphs[i];
		w += Advance(g, px);
		if (w > maxWidth) {
			if (lastSpace)
				return lastSpace - s;
			return i == 0 ? (size_t) (g.end - s) : (size_t) (g.start - s);
		}
		if (g.cp == ' ')
			lastSpace = g.end;
	}
	return len;
}

std::string Fonts::Elide(const std::string& s, FontStyle st, int px, int maxWidth)
{
	if (Measure(s, st, px) <= maxWidth)
		return s;
	static const std::string dots = "\xe2\x80\xa6"; // U+2026
	int room = maxWidth - Measure(dots, st, px);
	std::vector<Shaped> glyphs;
	Shape(s.data(), s.size(), st, glyphs);
	int w = 0;
	for (auto& g : glyphs) {
		w += Advance(g, px);
		if (w > room)
			return s.substr(0, g.start - s.data()) + dots;
	}
	return s;
}
