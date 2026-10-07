#pragma once

#include <string>
#include "Xm.hpp"
#include "TextInterface_Motif.hpp"

// Colours of the drawn panes (lists and messages), taken from the colours
// Motif widgets get from the desktop's scheme, so they match whatever
// 4Dwm uses.
struct Palette
{
	Rgb guildBg, listBg, listFg, listMuted, listHeader;
	Rgb selBg, selFg;
	Rgb unread, badge, badgeFg;
	Rgb msgBg, msgFg, msgMuted, link, mention, codeBg, codeFrame, quoteBar;
	Rgb online, idle, dnd, offline;
};

// Reads the scheme's colours from a realized or created widget.
void InitPalette(Widget w);
const Palette& GetPalette();

// The text size, kept in ~/.discordmessenger/motif.conf (DM_TEXT_SIZE
// overrides it for one run).
void LoadMotifConfig();
void SaveMotifConfig();
int GetTextSize();
void SetTextSize(int px);
void ApplyTheme(DrawingContext& ctx);

// UTF-8 text for Motif widgets, which show ISO 8859-1: characters outside
// it become '?', and emoji are dropped.
std::string Utf8ToLatin1(const std::string& s);
std::string Latin1ToUtf8(const std::string& s);
XmString MakeXmString(const std::string& utf8);
