#pragma once

#include <string>
#include "Xm.hpp"
#include "TextInterface_Motif.hpp"

// Text size and colours of the drawn panes.  DM_TEXT_SIZE sets the size of
// message text in pixels (default 14).
int GetTextSize();
void SetTextSize(int px);
void ApplyTheme(DrawingContext& ctx);

// UTF-8 text for Motif widgets, which show ISO 8859-1: characters outside
// it become '?', and emoji are dropped.
std::string Utf8ToLatin1(const std::string& s);
std::string Latin1ToUtf8(const std::string& s);
XmString MakeXmString(const std::string& utf8);
