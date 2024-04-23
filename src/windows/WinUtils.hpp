#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <WindowsX.h>
#include <Commctrl.h>

#include <string>
#include <tchar.h>

#include "../discord/ActiveStatus.hpp"
#include "../discord/Frontend.hpp"

#ifdef UNICODE
#define WAsnprintf _snwprintf
#else
#define WAsnprintf _snprintf
#endif

struct NetRequest;

#if _WIN32_WINNT < 0x0600
// Vista definitions
#define WM_MOUSEHWHEEL 0x020E
#endif // _WIN32_WINNT

// String manipulation

#define CHECKBOX_INTERNAL_SIZE 12 // how do I avoid hard coding this??

// note: this must be free()'d!
LPTSTR ConvertCppStringToTString(const std::string& str, size_t* lenOut = NULL);
void ConvertCppStringToTCharArray(const std::string& sstr, TCHAR* buff, size_t szMax);

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
void WindowScroll(HWND hWnd, int diffUpDown);
void WindowScrollXY(HWND hWnd, int diffLeftRight, int diffUpDown);
void DrawBitmap(HDC hdc, HBITMAP bitmap, int x, int y, LPRECT clip = NULL, COLORREF transparent = CLR_NONE, int scaleToWidth = 0, int scaleToHeight = 0);
void FillGradient(HDC hdc, const LPRECT lpRect, int sci1, int sci2, bool vertical);
void FillGradientColors(HDC hdc, const LPRECT lpRect, COLORREF c1, COLORREF c2, bool vertical);
bool IsChildOf(HWND child, HWND of);
int GetSystemDpiU();
int GetSystemDPI(); // cached
void ForgetSystemDPI();
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
int MapIconToOldIfNeeded(int iconID);
void InitializeStatusIcons();
void DrawMentionStatus(HDC hdc, int x, int y, int mentionCount);
void DrawActivityStatus(HDC hdc, int x, int y, eActiveStatus status);
void DrawLoadingBox(HDC hdc, RECT rect);
void DrawErrorBox(HDC hdc, RECT rect);

#ifdef _DEBUG
void DbgPrintW(const char* fmt, ...);
#else
#define DbgPrintW(...)
#endif

// Convenience macro
#define DMIC(iid) MapIconToOldIfNeeded((iid))

// File utils
bool FileExists(const std::string& path);

bool XSetProcessDPIAware();

// Color utils
COLORREF LerpColor(COLORREF a, COLORREF b, int progMul, int progDiv);

// Profile utils
#include "../discord/Profile.hpp"
COLORREF GetNameColor(Profile* pf, Snowflake guild);

// URL utils
void LaunchURL(const std::string& link);

// COM utils
void InitializeCOM(); // used by TTS and shell stuff

#include "ri/reimpl.hpp"
