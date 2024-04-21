#pragma once

#if (_WIN32_WINNT<0x0500)
#ifndef OLD_WINDOWS
#define OLD_WINDOWS
#endif
#endif

#ifndef OLD_WINDOWS
#define NEW_WINDOWS
#endif
