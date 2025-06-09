#pragma once

// ** Configuration **

// Whether to allow DM to hijack its abort() function so that it can show
// info that's More Useful (such as a stack trace!!)
//
// MinGW builds will always do this while only released MSVC builds will.
// No x64 builds will allow this.
#if (defined MINGW_SPECIFIC_HACKS || !defined _DEBUG) && !defined _WIN64
#define ALLOW_ABORT_HIJACKING
#define ABORT_ON_EXCEPTION
#endif

//#define FILE_DEBUG_HIJACKER

// ** Setup **
void SetupCrashDebugging();
