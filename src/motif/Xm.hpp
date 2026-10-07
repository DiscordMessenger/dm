#pragma once

// Include this before any Xt or Motif header.
//
// Xt names char* "String"; the core has a text class of that name.  The
// core's text headers are read first, and Xt's typedef is renamed XtString
// for the rest of the file (Xt's functions are C, so their names do not
// change).
#include "text/TextInterface.hpp"
#include "text/FormattedText.hpp"

#define String XtString
#include <X11/Intrinsic.h>
#include <Xm/Xm.h>
