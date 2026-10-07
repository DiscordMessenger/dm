#pragma once

#include <string>
#include "Xm.hpp"
#include "TextInterface_Motif.hpp"

// Colours of the drawn panes (lists and messages).
struct Palette
{
	Rgb guildBg, listBg, listFg, listMuted, listHeader;
	Rgb selBg, selFg, hoverBg;
	Rgb unread, badge, badgeFg;
	Rgb msgBg, msgFg, msgMuted, link, mention, codeBg, codeFrame, quoteBar;
	Rgb online, idle, dnd, offline;
	// Motif widgets in the dark theme (empty: leave Motif's colours)
	const char* widgetBg;
	const char* widgetFg;
	const char* textBg;
};

// The theme and text size, kept in ~/.discordmessenger/motif.conf.
// DM_THEME=dark|light and DM_TEXT_SIZE override them for one run.
void LoadMotifConfig();
void SaveMotifConfig();
bool IsDarkTheme();
void SetDarkTheme(bool dark); // takes effect at the next start
const Palette& GetPalette();

int GetTextSize();
void SetTextSize(int px);
void ApplyTheme(DrawingContext& ctx);

// Puts the theme's colours for Motif widgets into the display's resource
// database; call before creating widgets.
void ApplyThemeResources(Display* dpy);

// UTF-8 text for Motif widgets, which show ISO 8859-1: characters outside
// it become '?', and emoji are dropped.
std::string Utf8ToLatin1(const std::string& s);
std::string Latin1ToUtf8(const std::string& s);
XmString MakeXmString(const std::string& utf8);
