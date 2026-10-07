#pragma once

#include "Canvas.hpp"
#include "Fonts.hpp"

// What the core's formatted-text renderer (the Md* functions) draws with.
// canvas is null while only measuring.
struct DrawingContext
{
	Canvas* canvas = nullptr;
	int px = 14;            // body text size in pixels
	Rgb fg = 0x000000;
	Rgb bg = 0xffffff;
	Rgb link = 0x0645ad;
	Rgb mention = 0x4752c4;
	Rgb codeBg = 0xf0f0f0;
	Rgb codeFrame = 0xc8c8c8;
	Rgb quoteBar = 0xb0b0b0;
	Rgb muted = 0x808080;
};

// The font and size the formatted-text style flags (WORD_*) ask for.
void MdFontFor(const DrawingContext* ctx, int styleFlags, FontStyle& st, int& px);
