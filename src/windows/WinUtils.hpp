#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>

#include <string>
#include <tchar.h>

#include "../discord/ActiveStatus.hpp"
#include "../discord/Frontend.hpp"

// Cut Down Flags
#define DMCDF_USER32   0x0001
#define DMCDF_GDI32    0x0002
#define DMCDF_SHELL32  0x0004
#define DMCDF_MSIMG32  0x0008
#define DMCDF_SHLWAPI  0x0010
#define DMCDF_CRYPT32  0x0020
#define DMCDF_WS2_32   0x0040
#define DMCDF_OLE32    0x0080
#define DMCDF_COMCTL32 0x0100
#define DMCDF_ALL      0x01FF
#define DMCDF_NODLGEX  0x0200
#define DMCDF_NODRIEX  0x0400

#ifdef UNICODE
#define WAsnprintf _snwprintf
#else
#define WAsnprintf _snprintf
#endif

#ifdef _MSC_VER
#define NORETURN __declspec(noreturn)
#else
#define NORETURN __attribute__((noreturn))
#endif

struct NetRequest;

#if _WIN32_WINNT < 0x0600
// Vista definitions
#define WM_MOUSEHWHEEL 0x020E
#endif // _WIN32_WINNT

#ifdef _MSC_VER

// Well, MSVC defines OPENFILENAME_NT4 for us
#define SIZEOF_OPENFILENAME_NT4 sizeof(OPENFILENAME_NT4)

#else

// MinGW doesn't, so we'll have to hardcode it.
// Luckily OPENFILENAME_NT4 doesn't have fields whose size
// depends on UNICODE, so it's always 76 bytes.
#define SIZEOF_OPENFILENAME_NT4 76

#endif

// String manipulation

#define CHECKBOX_INTERNAL_SIZE 12 // how do I avoid hard coding this??

// note: this must be free()'d!
LPTSTR ConvertCppStringToTString(const std::string& str, size_t* lenOut = NULL);
LPTSTR ConvertToTStringAddCR(const std::string& str, size_t* lenOut = NULL);
void ConvertCppStringToTCharArray(const std::string& sstr, TCHAR* buff, size_t szMax);
std::string MakeStringFromEditData(LPCTSTR tstr);
std::string MakeStringFromUnicodeString(LPCWSTR wstr);
#ifdef UNICODE
#define MakeStringFromTString(x) MakeStringFromUnicodeString(x)
#define ASCIIZ_STR_FMT "%S"
#else
#define MakeStringFromTString(x) std::string(x)
#define ASCIIZ_STR_FMT "%s"
#endif // UNICODE

// Clipboard utils
void CopyStringToClipboard(const std::string& str);

// Error codes
std::string GetStringFromHResult(HRESULT hr);
LPTSTR GetTStringFromHResult(HRESULT hr);

// Window utils
bool IsWindowActive(HWND hWnd);
void WindowScroll(HWND hWnd, int diffUpDown);
void WindowScrollXY(HWND hWnd, int diffLeftRight, int diffUpDown);
void DrawBitmap(HDC hdc, HBITMAP bitmap, int x, int y, LPRECT clip = NULL, COLORREF transparent = CLR_NONE, int scaleToWidth = 0, int scaleToHeight = 0, bool hasAlpha = false);
void FillGradient(HDC hdc, const LPRECT lpRect, int sci1, int sci2, bool vertical);
void FillGradientColors(HDC hdc, const LPRECT lpRect, COLORREF c1, COLORREF c2, bool vertical);
bool IsChildOf(HWND child, HWND of);
void CenterWindow(HWND hWnd, HWND hRelativeTo);
int GetSystemDpiU();
int GetSystemDPI(); // cached
void ForgetSystemDPI();
void SetUserScale(int scale);
int ScaleByUser(int x);
int ScaleByDPI(int pos);
int UnscaleByDPI(int pos);
void ScreenToClientRect(HWND hWnd, RECT* rect);
void ShiftWindow(HWND hWnd, int diffX, int diffY, bool bRepaint);
void ResizeWindow(HWND hWnd, int diffX, int diffY, bool byLeft, bool byTop, bool bRepaint);
void GetChildRect(HWND parent, HWND child, LPRECT rect);
void DownloadFileDialog(HWND hWnd, const std::string& url, const std::string& fileName);
void DownloadOnRequestFail(HWND hWnd, NetRequest* pRequest);
SIZE EnsureMaximumSize(int width, int height, int maxWidth, int maxHeight);
bool Supports32BitIcons(); // Really, this checks if we are using Windows 2000 or older.
bool SupportsDialogEx(); // Really, this checks if we are using Windows NT 3.51 or older.
int MapIconToOldIfNeeded(int iconID);
int MapDialogToOldIfNeeded(int dialogID);
void InitializeStatusIcons();
void DrawMentionStatus(HDC hdc, int x, int y, int mentionCount);
void DrawActivityStatus(HDC hdc, int x, int y, eActiveStatus status);
void DrawLoadingBox(HDC hdc, RECT rect);
void DrawErrorBox(HDC hdc, RECT rect);
bool GetDataFromBitmap(HDC hdc, HBITMAP hbm, BYTE*& pBytes, int& width, int& height, int& bpp);
LRESULT HandleGestureMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, float mulDeltas = 1.0f);
std::string CutOutURLPath(const std::string& url);
int MessageBoxHooked(HWND hWnd, LPCTSTR title, LPCTSTR caption, int flags, int overID, LPCTSTR overText);
int MessageBoxHooked(HWND hWnd, LPCTSTR title, LPCTSTR caption, int flags, int overCount, const int* overID, const LPCTSTR*  overText);
void DrawIconInvert(HDC hdc, HICON hIcon, int x, int y, int sizeX, int sizeY, bool invert);
HBITMAP ResizeWithBackgroundColor(HDC hdc, HBITMAP hBitmap, HBRUSH backgroundColor, bool bHasAlpha, int newSizeX, int newSizeY, int oldSizeX, int oldSizeY);
int GetGradientActiveCaptionColor();
int GetGradientInactiveCaptionColor();
void PrepareCutDownFlags(LPSTR cmdLine);
int GetCutDownFlags();

#ifdef USE_DEBUG_PRINTS
void DbgPrintW(const char* fmt, ...);
#else
#define DbgPrintW(...)
#endif

// Convenience macro
#define DMIC(iid) MapIconToOldIfNeeded((iid))
#define DMDI(iid) MapDialogToOldIfNeeded((iid))

// File utils
bool FileExists(const std::string& path);

bool XSetProcessDPIAware();

// Color utils
COLORREF LerpColor(COLORREF a, COLORREF b, int progMul, int progDiv);
bool IsColorDark(COLORREF cr);
bool IsTextColorDark();
bool IsIconMostlyBlack(HICON hic);

#define IsColorLight(cr)   (!IsColorDark(cr))
#define IsTextColorLight() (!IsTextColorDark())

// Profile utils
#include "../discord/Profile.hpp"
COLORREF GetNameColor(Profile* pf, Snowflake guild);

// URL utils
void LaunchURL(const std::string& link);

// COM utils
void InitializeCOM(); // used by TTS and shell stuff

#define SAFE_DELETE(x) do {	 \
	if ((x)) {				 \
		delete(x);			 \
		(x) = nullptr;		 \
	}						 \
} while (0)

#include "ri/reimpl.hpp"

extern "C" void Terminate(const char*, ...);
