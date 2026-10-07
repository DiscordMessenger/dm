#pragma once

#include <string>
#include "Canvas.hpp"

// Text drawn with FreeType into a Canvas (the X server's own fonts have
// neither Unicode coverage nor antialiasing on most SGI displays).
enum FontStyle
{
	FS_REGULAR,
	FS_BOLD,
	FS_ITALIC,
	FS_BOLDITALIC,
	FS_MONO,
	FS_MONOBOLD,
	FS_COUNT
};

namespace Fonts
{
	// Loads the faces from DM_FONT_DIR, the installed fonts directory, or
	// pkgsrc's TrueType directory.  On failure, err says what is missing.
	bool Init(std::string& err);

	int Ascent(FontStyle st, int px);
	int Descent(FontStyle st, int px);
	inline int LineHeight(FontStyle st, int px) { return Ascent(st, px) + Descent(st, px); }

	// Width of UTF-8 text on one line.
	int Measure(const std::string& s, FontStyle st, int px);
	int Measure(const char* s, size_t n, FontStyle st, int px);

	// Draws UTF-8 text with its baseline at y; returns the advance.
	int Draw(Canvas& c, int x, int y, const std::string& s, FontStyle st, int px, Rgb color);
	int Draw(Canvas& c, int x, int y, const char* s, size_t n, FontStyle st, int px, Rgb color);

	// How many bytes of s fit in maxWidth, broken after a space when one is
	// there (at least one character).
	size_t FitBytes(const char* s, size_t n, FontStyle st, int px, int maxWidth);

	// Truncates text to maxWidth with an ellipsis.
	std::string Elide(const std::string& s, FontStyle st, int px, int maxWidth);
}

// The next code point of UTF-8 text at *p (advancing it), U+FFFD on errors.
unsigned DecodeUtf8(const char*& p, const char* end);
