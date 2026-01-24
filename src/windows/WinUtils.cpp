#define WIN32_LEAN_AND_MEAN
#ifndef FKG_FORCED_USAGE
#define FKG_FORCED_USAGE
#endif

#include <algorithm>
#include <climits>
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include "WinUtils.hpp"

#include "../resource.h"
#include "WindowMessages.hpp"
#include "Measurements.hpp"
#include "NetworkerThread.hpp"
#include "ImageViewer.hpp"
#include "TextManager.hpp"
#include "ProgressDialog.hpp"

#ifndef OLD_WINDOWS
#include <shlwapi.h>
#include "utils/Util.hpp"
#endif

constexpr int DEFAULT_DPI  = 96;
constexpr int NORMAL_SCALE = 1000;

extern HWND g_Hwnd; // main.hpp
extern HINSTANCE g_hInstance; // main.hpp

void InitializeCOM()
{
	ri::CoInitialize(NULL);
}

int GetSystemDpiU()
{
	HWND hwnd = g_Hwnd;
	if (!hwnd) hwnd = GetDesktopWindow();
	HDC hdc = GetDC(hwnd);
	int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
	ReleaseDC(hwnd, hdc);
	return dpi;
}

static int g_systemDPICache = 0;
int GetSystemDPI()
{
	if (g_systemDPICache) return g_systemDPICache;
	return g_systemDPICache = GetSystemDpiU();
}

void ForgetSystemDPI()
{
	g_systemDPICache = 0;
}

int g_Scale = NORMAL_SCALE;

void SetUserScale(int scale)
{
	g_Scale = scale;
}

int ScaleByUser(int x)
{
	return MulDiv(x, g_Scale, NORMAL_SCALE);
}

int ScaleByDPI(int pos)
{
	return MulDiv( pos, g_Scale * GetSystemDPI(), NORMAL_SCALE * DEFAULT_DPI );
}

int UnscaleByDPI(int pos)
{
	return MulDiv( pos, NORMAL_SCALE * DEFAULT_DPI, g_Scale * GetSystemDPI() );
}

void ScreenToClientRect(HWND hWnd, RECT* rect)
{
	POINT p1 { rect->left, rect->top }, p2 { rect->right, rect->bottom };
	ScreenToClient(hWnd, &p1);
	ScreenToClient(hWnd, &p2);
	rect->left   = p1.x;
	rect->top    = p1.y;
	rect->right  = p2.x;
	rect->bottom = p2.y;
}

void GetChildRect(HWND parent, HWND child, LPRECT rect)
{
	GetWindowRect(child, rect);

	if (parent)
		ScreenToClientRect(parent, rect);
}

void ShiftWindow(HWND hWnd, int diffX, int diffY, bool bRepaint)
{
	HWND hParWnd = GetParent(hWnd);
	RECT rc{};

	GetChildRect(hParWnd, hWnd, &rc);
	rc.left   += diffX;
	rc.top    += diffY;
	rc.right  += diffX;
	rc.bottom += diffY;
	MoveWindow(hWnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, bRepaint);
}

void ResizeWindow(HWND hWnd, int diffX, int diffY, bool byLeft, bool byTop, bool bRepaint)
{
	HWND hParWnd = GetParent(hWnd);
	RECT rc{};

	GetChildRect(hParWnd, hWnd, &rc);
	if (byLeft) rc.left   += diffX;
	else        rc.right  += diffX;
	if (byTop)  rc.top    += diffY;
	else        rc.bottom += diffY;
	MoveWindow(hWnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, bRepaint);
}

void WindowScroll(HWND hWnd, int diffUpDown)
{
	ScrollWindowEx(hWnd, 0, -diffUpDown, NULL, NULL, NULL, NULL, SW_INVALIDATE);
	UpdateWindow(hWnd);
}

void WindowScrollXY(HWND hWnd, int diffLeftRight, int diffUpDown)
{
	ScrollWindowEx(hWnd, -diffLeftRight, -diffUpDown, NULL, NULL, NULL, NULL, SW_INVALIDATE);
	UpdateWindow(hWnd);
}

bool IsWindowActive(HWND hWnd)
{
	return IsWindowVisible(hWnd) && GetForegroundWindow() == hWnd;
}

bool FileExists(const std::string& path)
{
#ifdef OLD_WINDOWS
	FILE* f = fopen(path.c_str(), "rb");
	if (!f)
		return false;
	fclose(f);
	return true;
#else
	LPCTSTR str = ConvertCppStringToTString(path);
	bool res = ri::PathFileExists(str);
	free((void*)str);
	return res;
#endif
}

LPTSTR GetTStringFromHResult(HRESULT hr)
{
	LPCTSTR tstr = NULL;
	FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		hr,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&tstr,
		0,
		NULL
	);

	bool bFree = true;
	if (!tstr) {
		bFree = false;
		tstr = TEXT("Unknown Error Code");
	}

	// dupe it to be able to use free() instead of LocalFree()
	size_t newSz = _tcslen(tstr);
	TCHAR* newStr = new TCHAR[newSz + 1];
	_tcscpy(newStr, tstr);

	if (bFree)
		LocalFree((HLOCAL) tstr);

	return newStr;
}

std::string GetStringFromHResult(HRESULT hr)
{
	LPTSTR tstr = GetTStringFromHResult(hr);
	std::string str = MakeStringFromTString(tstr);
	free(tstr);
	return str;
}

void CopyStringToClipboard(const std::string& str)
{
	if (OpenClipboard(g_Hwnd))
	{
		EmptyClipboard();

		// This expands all of the new line characters inside str into
		// a carriagereturn-linefeed sequence.
		std::string str2;
		str2.reserve(24 + str.size());

		for (size_t i = 0; i < str.size(); i++)
		{
			if (str[i] == '\n' && (i == 0 || str[i] != '\r'))
				str2.push_back('\r');
			str2.push_back(str[i]);
		}

		LPCTSTR ctstr = ConvertCppStringToTString(str2);

		size_t stringSize = (str.length() + 1) * sizeof(TCHAR);

		// create a global buffer and fill its text in
		HGLOBAL clipbuffer;
		char* buffer;
		clipbuffer = GlobalAlloc(GMEM_DDESHARE, stringSize);
		buffer = (char*)GlobalLock(clipbuffer);
		memcpy(buffer, ctstr, stringSize);
		GlobalUnlock(clipbuffer);

		// set the clipboard data
#if UNICODE
		SetClipboardData(CF_UNICODETEXT, clipbuffer);
#else
		SetClipboardData(CF_TEXT, clipbuffer);
#endif

		// release everything we have
		CloseClipboard();

		free((void*)ctstr);
	}
}

// Borrowed from NanoShellOS: https://github.com/iProgramMC/NanoShellOS/blob/master/src/utf8.c
#define REPLACEMENT_CHAR 0xFFFD
int Utf8DecodeCharacter(const char* pByteSeq, int* pSizeOut)
{
	const uint8_t* chars = (const uint8_t*)pByteSeq;

	uint8_t char0 = chars[0];

	if (char0 < 0x80)
	{
		// 1 byte ASCII character. Fine
		if (pSizeOut) *pSizeOut = 1;
		return char0;
	}

	char char1 = chars[1];
	if (char1 == 0) return REPLACEMENT_CHAR; // this character is broken.
	if (char0 < 0xE0)
	{
		// 2 byte character.
		int codepoint = ((char0 & 0x1F) << 6) | (char1 & 0x3F);
		if (pSizeOut) *pSizeOut = 2;
		return codepoint;
	}

	char char2 = chars[2];
	if (char2 == 0) return REPLACEMENT_CHAR; // this character is broken.
	if (char0 < 0xF0)
	{
		// 3 byte character.
		int codepoint = ((char0 & 0xF) << 12) | ((char1 & 0x3F) << 6) | (char2 & 0x3F);
		if (pSizeOut) *pSizeOut = 3;
		return codepoint;
	}

	char char3 = chars[3];
	// 4 byte character.
	if (char3 == 0) return REPLACEMENT_CHAR;

	int codepoint = ((char0 & 0x07) << 18) | ((char1 & 0x3F) << 12) | ((char2 & 0x3F) << 6) | (char3 & 0x3F);
	if (pSizeOut) *pSizeOut = 4;
	return codepoint;
}

LPTSTR ConvertCppStringToTString(const std::string& sstr, size_t* lenOut)
{
#ifdef UNICODE
	size_t sz = sstr.size() + 1;
	TCHAR* str = (TCHAR*)calloc(sz, sizeof(TCHAR));
	size_t convertedChars = (size_t)MultiByteToWideChar(CP_UTF8, 0, sstr.c_str(), -1, str, (int) sz);
#else // ANSI
	size_t convertedChars = 0;
	LPTSTR str = NULL;

	// check if this string is pure ASCII
	bool isPureAscii = true;
	const char* ptr = sstr.c_str();
	while (*ptr) {
		if (((int)*ptr) & 0x80) {
			isPureAscii = false;
			break;
		}
		ptr++;
	}

	if (isPureAscii)
	{
	dumbConversion:
		size_t sz = sstr.size() + 1;
		str = (TCHAR*)calloc(sz, sizeof(TCHAR));
		strncpy(str, sstr.c_str(), sz);
		str[sz - 1] = 0;
		convertedChars = sz - 1;
	}
	else
	{
		// not pure ASCII
		// decode all of the UTF-8 characters into a wide character string
		size_t index = 0, maxsz = sstr.size() + 2;
		WCHAR* wcs = (WCHAR*) calloc(maxsz, sizeof(WCHAR));

		ptr = sstr.c_str();
		while (*ptr && index + 1 < maxsz)
		{
			int sz = 0;
			int decoded = Utf8DecodeCharacter(ptr, &sz);
			if (sz == 0) {
				sz = 1;
				decoded = REPLACEMENT_CHAR;
			}

			if (decoded > 0xFFFF)
				decoded = REPLACEMENT_CHAR;

			wcs[index++] = (WCHAR) decoded;
			ptr += sz;
		}

		wcs[index] = 0;

		// This is a scam and causes the app to not parse the UTF-8 at all.
		// Believe it or not I also tried to add the info that we support UTF-8
		// to the manifest but it still doesn't work.
		//int codepage = CP_ACP;
		//if (LOBYTE(GetVersion()) >= 6)
		//	codepage = CP_UTF8;

		// and then try to turn it into a multi-byte string
		int sz = WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK | WC_DEFAULTCHAR, wcs, index, NULL, 0, NULL, NULL);
		if (sz == 0) {
			// failure ... just do the dumb conversion
			DbgPrintW("WideCharToMultiByte failed: %d", GetLastError());
			free(wcs);
			goto dumbConversion;
		}

		// avoid overflow
		if (sz > INT_MAX - 2)
			sz = INT_MAX - 2;

		CHAR defaultChar = 0x7F;

		str = (LPTSTR) calloc(sz + 1, sizeof(TCHAR));
		int sz2 = WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK | WC_DEFAULTCHAR, wcs, index, str, sz + 1, &defaultChar, NULL);
		if (sz2 == 0)
		{
			// CP_ACP might be UTF-8 for some reason, try with the default one
			sz2 = WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK | WC_DEFAULTCHAR, wcs, index, str, sz + 1, NULL, NULL);
			
			if (sz2 == 0)
			{
				DbgPrintW("Second WideCharToMultiByte failed: %d", GetLastError());
				free(wcs);
				free(str);
				goto dumbConversion;
			}
		}

		// converted!
		convertedChars = (size_t) sz2;
	}
#endif

	if (lenOut)
		*lenOut = convertedChars;

	return (LPTSTR) str;
}

LPTSTR ConvertToTStringAddCR(const std::string& str, size_t* lenOut)
{
	std::string str2;
	str2.reserve(str.size() + 16);

	for (auto& c : str) {
		if (c == '\n')
			str2 += '\r';
		str2 += c;
	}

	return ConvertCppStringToTString(str2, lenOut);
}

void ConvertCppStringToTCharArray(const std::string& sstr, TCHAR* buff, size_t szMax)
{
	if (!szMax)
		return;

	if (sstr.empty()) {
		*buff = 0;
		return;
	}

#ifdef UNICODE
	int len = MultiByteToWideChar(CP_UTF8, 0, sstr.c_str(), -1, buff, (int)szMax - 1);
	buff[szMax - 1] = 0;
#else
	strncpy(buff, sstr.c_str(), szMax);
	buff[szMax - 1] = 0;
	int len = strlen(buff);
#endif
	buff[len] = 0;
}

// note: this works on ANSI too
std::string MakeStringFromUnicodeString(LPCWSTR wstr)
{
	size_t sl = lstrlenW(wstr);
	// generate the size of the UTF-8 string
	size_t slmax = (sl + 1) * 4; // whatever
	char* chr = (char*)malloc(slmax);
	//_wcstombs_s_l(&sz, chr, slmax, wstr, slmax, CP_UTF8);
	WideCharToMultiByte(CP_UTF8, 0, wstr, -1, chr, (int) slmax, NULL, NULL);

	chr[slmax - 1] = 0; // ensure the null terminator is there
	std::string final_str(chr);
	free(chr);
	return final_str;
}

std::string MakeStringFromEditData(LPCTSTR tstr)
{
	std::vector<TCHAR> tch2;
	const TCHAR* tcString = tstr;
	for (; *tcString; tcString++) {
		if (*tcString != '\r')
			tch2.push_back(*tcString);
	}
	tch2.push_back(0);

	return MakeStringFromTString(tch2.data());
}

std::string CreateChannelLink(Snowflake guild, Snowflake channel)
{
	std::string guildStr = std::to_string(guild);
	if (!guild) guildStr = "@me";

	return "https://discord.com/channels/" + guildStr + "/" + std::to_string(channel);
}

std::string CreateMessageLink(Snowflake guild, Snowflake channel, Snowflake message)
{
	std::string guildStr = std::to_string(guild);
	if (!guild) guildStr = "@me";

	return "https://discord.com/channels/" + guildStr + "/" + std::to_string(channel) + "/" + std::to_string(message);
}

void DrawBitmap(HDC hdc, HBITMAP bitmap, int x, int y, LPRECT clip, COLORREF transparent, int scaleToWidth, int scaleToHeight, bool hasAlpha)
{
	HRGN hrgn = NULL;
	if (clip) {
		RECT clipCopy = *clip;
		POINT vpOrg = {};
		GetViewportOrgEx(hdc, &vpOrg);
		OffsetRect(&clipCopy, vpOrg.x, vpOrg.y);
		hrgn = CreateRectRgn(clipCopy.left, clipCopy.top, clipCopy.right, clipCopy.bottom);
		SelectClipRgn(hdc, hrgn);
	}

	HDC hdcMem = CreateCompatibleDC(hdc);
	BITMAP bm = {};
	HGDIOBJ objOld = SelectObject(hdcMem, bitmap);
	GetObject(bitmap, sizeof bm, &bm);

	if (hrgn)
		SelectClipRgn(hdc, hrgn);

	int scaledW = bm.bmWidth;
	int scaledH = bm.bmHeight;

	if (scaleToWidth != scaledW && scaleToWidth != 0) {
		if (scaleToHeight)
			scaledH = scaleToHeight;
		else
			scaledH = MulDiv(scaledH, scaleToWidth, bm.bmWidth);
		scaledW = scaleToWidth;
	}
	if (scaleToHeight != scaledH && scaleToHeight != 0) {
		if (scaleToWidth)
			scaledW = scaleToWidth;
		else
			scaledW = MulDiv(scaledW, scaleToHeight, scaledH);
		scaledH = scaleToHeight;
	}

	if (hasAlpha)
	{
		BLENDFUNCTION bf{};
		bf.BlendOp = AC_SRC_OVER;
		bf.AlphaFormat = AC_SRC_ALPHA;
		bf.BlendFlags = 0;
		bf.SourceConstantAlpha = 255;
		ri::AlphaBlend(hdc, x, y, scaledW, scaledH, hdcMem, 0, 0, bm.bmWidth, bm.bmHeight, bf);
	}
	else if (transparent != CLR_NONE)
	{
		ri::TransparentBlt(hdc, x, y, scaledW, scaledH, hdcMem, 0, 0, bm.bmWidth, bm.bmHeight, RGB(255, 0, 255));
	}
	else if (scaledW != bm.bmWidth || scaledH != bm.bmHeight)
	{
		int oldMode = SetStretchBltMode(hdc, ri::GetHalfToneStretchMode());
		StretchBlt(hdc, x, y, scaledW, scaledH, hdcMem, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
		SetStretchBltMode(hdc, oldMode);
	}
	else
	{
		BitBlt(hdc, x, y, bm.bmWidth, bm.bmHeight, hdcMem, 0, 0, SRCCOPY);
	}

	SelectClipRgn(hdc, NULL);
	SelectObject(hdcMem, objOld);
	DeleteObject(hdcMem);

	if (clip) {
		SelectClipRgn(hdc, NULL);
	}
	if (hrgn) {
		DeleteObject(hrgn);
	}
}

static bool HasGradientCaption()
{
	auto version = LOWORD(GetVersion());
	if (LOBYTE(version) < 4) return false;
	if (LOBYTE(version) > 4) return true;
	return HIBYTE(version) > 1;
}

int GetGradientActiveCaptionColor()
{
	if (!HasGradientCaption())
		return COLOR_ACTIVECAPTION;

	return COLOR_GRADIENTACTIVECAPTION;
}

int GetGradientInactiveCaptionColor()
{
	if (!HasGradientCaption())
		return COLOR_INACTIVECAPTION;

	return COLOR_GRADIENTINACTIVECAPTION;
}

void FillGradient(HDC hdc, const LPRECT lpRect, int sci1, int sci2, bool vertical)
{
	if (!HasGradientCaption()) {
		if (sci1 == COLOR_GRADIENTACTIVECAPTION)
			sci1 = COLOR_ACTIVECAPTION;
		if (sci1 == COLOR_GRADIENTINACTIVECAPTION)
			sci1 = COLOR_INACTIVECAPTION;
	}
	if (ri::HaveMsImg()) {
		COLORREF c1 = GetSysColor(sci1);
		COLORREF c2 = GetSysColor(sci2);
		FillGradientColors(hdc, lpRect, c1, c2, vertical);
	}
	else {
		HBRUSH hbr = ri::GetSysColorBrush(sci1);
		FillRect(hdc, lpRect, hbr);
	}
}

void FillGradientColors(HDC hdc, const LPRECT lpRect, COLORREF c1, COLORREF c2, bool vertical)
{
	if (lpRect->left >= lpRect->right || lpRect->top >= lpRect->bottom)
		return;

	if (ri::HaveMsImg())
	{
		bool haveAlpha = false;
		HDC hdcDest = hdc;
		HBITMAP hbm = NULL;
		RECT rc = *lpRect;
		if (c1 == CLR_NONE || c2 == CLR_NONE)
		{
			haveAlpha = true;

			bool firstTrans = c1 == CLR_NONE;

			// This is gonna suck

			// Create a compatible DC and a backing bitmap
			hdcDest = CreateCompatibleDC(hdc);
			OffsetRect(&rc, -rc.left, -rc.top);

			//hbm = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
			BITMAPINFO bmi{};
			ULONG ulWidth, ulHeight;
			bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			bmi.bmiHeader.biWidth = ulWidth = rc.right;
			bmi.bmiHeader.biHeight = ulHeight = rc.bottom;
			bmi.bmiHeader.biPlanes = 1;
			bmi.bmiHeader.biBitCount = 32;         // four 8-bit components 
			bmi.bmiHeader.biCompression = BI_RGB;
			bmi.bmiHeader.biSizeImage = ulWidth * ulHeight * 4;

			void* pvBits = NULL;
			hbm = ri::CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pvBits, NULL, 0);
			if (!hbm) {
				DbgPrintW("FillGradientColors : Could not create bitmap object!");
				goto _FALLBACK;
			}

			SelectObject(hdcDest, hbm);
			
			// Alpha Gradient
			PUINT32 puBits = (PUINT32)pvBits;
			COLORREF clr = (c1 == CLR_NONE ? c2 : c1);

			BYTE bytes[sizeof(COLORREF)];
			memcpy(bytes, &clr, sizeof(COLORREF));

			for (ULONG y = 0, index = 0; y < ulHeight; y++)
			{
				UCHAR pcalpha;
				if (vertical) {
					if (firstTrans)
						pcalpha = (UCHAR)((ulHeight - y) * 255 / ulHeight);
					else
						pcalpha = (UCHAR)(y * 255 / ulHeight);
				}
				for (ULONG x = 0; x < ulWidth; x++, index++)
				{
					UCHAR alpha;
					if (!vertical) {
						if (firstTrans)
							alpha = (UCHAR)(x * 255 / ulWidth);
						else
							alpha = (UCHAR)((ulWidth - x) * 255 / ulWidth);
					}
					else {
						alpha = pcalpha;
					}
					puBits[index] =
						(alpha << 24) |
						((bytes[0] * alpha / 255) << 16) |
						((bytes[1] * alpha / 255) << 8) |
						(bytes[2] * alpha / 255);
				}
			}

			ri::CommitDIBSection(hdc, hbm, &bmi, puBits);

			// Write the object onto the main DC
			BLENDFUNCTION bf{};
			bf.BlendOp = AC_SRC_OVER;
			bf.AlphaFormat = AC_SRC_ALPHA;
			bf.BlendFlags = 0;
			bf.SourceConstantAlpha = 50;

			ri::AlphaBlend(hdc, lpRect->left, lpRect->top, rc.right, rc.bottom, hdcDest, 0, 0, rc.right, rc.bottom, bf);

			ri::ReleaseDIBSection(puBits);
			DeleteObject(hbm);
			DeleteDC(hdcDest);
			hdcDest = hdc;
		}
		else
		{
		_FALLBACK:
			TRIVERTEX vt[2] = { {}, {} };
			vt[0].x = rc.left;
			vt[0].y = rc.top;
			vt[1].x = rc.right;
			vt[1].y = rc.bottom;
			vt[0].Red = (c1 & 0xFF) << 8;
			vt[0].Green = ((c1 >> 8) & 0xFF) << 8;
			vt[0].Blue = ((c1 >> 16) & 0xFF) << 8;
			vt[0].Alpha = 0;
			vt[1].Red = int(c2 & 0xFF) << 8;
			vt[1].Green = int((c2 >> 8) & 0xFF) << 8;
			vt[1].Blue = int((c2 >> 16) & 0xFF) << 8;
			vt[1].Alpha = 0;

			GRADIENT_RECT grect;
			grect.UpperLeft = 0;
			grect.LowerRight = 1;

			ri::GradientFill(hdcDest, vt, 2, &grect, 1, vertical ? GRADIENT_FILL_RECT_V : GRADIENT_FILL_RECT_H);
		}
	}
	else
	{
		COLORREF oldColor = ri::SetDCBrushColor(hdc, c1);
		FillRect(hdc, lpRect, ri::GetDCBrush());
		ri::SetDCBrushColor(hdc, oldColor);
	}
}

bool XSetProcessDPIAware()
{
	typedef BOOL(WINAPI* PSETPROCESSDPIAWARE)(VOID);

	PSETPROCESSDPIAWARE pSetProcessDpiAware;

	HMODULE hndl = GetModuleHandle(TEXT("User32.dll"));
	if (!hndl)
		return true;

	pSetProcessDpiAware = (PSETPROCESSDPIAWARE)GetProcAddress(hndl, "SetProcessDPIAware");

	if (!pSetProcessDpiAware)
		return true;

	return pSetProcessDpiAware();
}

COLORREF LerpColor(COLORREF a, COLORREF b, int progMul, int progDiv)
{
	int r1, g1, b1;
	r1 = a & 0xFF;
	g1 = (a >> 8) & 0xFF;
	b1 = (a >> 16) & 0xFF;

	int r2, g2, b2;
	r2 = b & 0xFF;
	g2 = (b >> 8) & 0xFF;
	b2 = (b >> 16) & 0xFF;

	r1 += MulDiv(r2 - r1, progMul, progDiv);
	g1 += MulDiv(g2 - g1, progMul, progDiv);
	b1 += MulDiv(b2 - b1, progMul, progDiv);
	r1 = std::max(r1, 0); r1 = std::min(r1, 255);
	g1 = std::max(g1, 0); g1 = std::min(g1, 255);
	b1 = std::max(b1, 0); b1 = std::min(b1, 255);

	return RGB(r1, g1, b1);
}

#include "DiscordInstance.hpp"

COLORREF GetNameColor(Profile* pf, Snowflake guild)
{
	Guild* pGuild = GetDiscordInstance()->GetGuild(guild);
	if (!pGuild)
		return CLR_NONE;

	int winnerPos = -1;
	COLORREF winnerCol = CLR_NONE;

	auto& gldroles = pGuild->m_roles;
	auto& memroles = pf->m_guildMembers[guild].m_roles;
	for (auto& role : memroles)
	{
		auto& gldrole = gldroles[role];
		if (winnerPos < gldrole.m_position && gldrole.m_colorOriginal != 0) {
			int col = gldrole.m_colorOriginal;
			winnerPos = gldrole.m_position;
			winnerCol = RGB((col >> 16) & 0xFF, (col >> 8) & 0xFF, (col) & 0xFF);
		}
	}

	return winnerCol;
}

void LaunchURL(const std::string& link)
{
	LPTSTR tstr = ConvertCppStringToTString(link);
	INT_PTR res = (INT_PTR) ShellExecute(g_Hwnd, TEXT("open"), tstr, NULL, NULL, SW_SHOWNORMAL);

	if (res > 32) {
		free(tstr);
		return; // was fine
	}

	LPCTSTR errorCode = NULL;
	TCHAR buff[4096];
	switch (res)
	{
		case 0:
		case SE_ERR_OOM:
			errorCode = TEXT("Out of memory");
			break;

		case SE_ERR_FNF:
			errorCode = TEXT("File not found");
			break;

		case SE_ERR_PNF:
			errorCode = TEXT("Path not found");
			break;

		case SE_ERR_ACCESSDENIED:
			errorCode = TEXT("Access denied");
			break;
	}

	if (errorCode)
		WAsnprintf(buff, _countof(buff), TmGetTString(IDS_CANT_LAUNCH_URL_UNS), tstr, errorCode);
	else
		WAsnprintf(buff, _countof(buff), TmGetTString(IDS_CANT_LAUNCH_URL_ERR), tstr, res, res);

	free(tstr);
	MessageBox(g_Hwnd, buff, TmGetTString(IDS_PROGRAM_NAME), MB_ICONERROR | MB_OK);
}

bool IsChildOf(HWND child, HWND of)
{
	while (child)
	{
		if (child == of)
			return true;

		child = GetParent(child);
	}

	return false;
}

void DownloadOnRequestFail(HWND hWnd, NetRequest* pRequest)
{
	SendMessage(hWnd, WM_IMAGECLEARSAVE, 0, 0);

	TCHAR buff[4096];
	LPTSTR str = ConvertCppStringToTString(pRequest->url);
	LPTSTR str2 = ConvertCppStringToTString(pRequest->response);
	WAsnprintf(buff, _countof(buff), TmGetTString(IDS_ERROR_ACCESSING_FILE), str, str2);
	buff[_countof(buff) - 1] = 0;
	free(str);
	free(str2);

	MessageBox(hWnd, buff, TmGetTString(IDS_PROGRAM_NAME), MB_ICONERROR);
	ProgressDialog::Done(pRequest->key);
}

void DownloadFileResponse(NetRequest* pRequest)
{
	HWND hWnd = (HWND)pRequest->key;

	if (pRequest->result == HTTP_PROGRESS)
	{
		// N.B. totally safe to access because corresponding networker thread is locked up waiting for us
		pRequest->m_bCancelOp = ProgressDialog::Update(pRequest->key, pRequest->GetOffset(), pRequest->GetTotalBytes());
		return;
	}
	
	if (pRequest->result != HTTP_OK)
	{
		DownloadOnRequestFail(hWnd, pRequest);
		return;
	}

	// Since the response is passed in as a string, this should do for getting the binary stuff out of it
	const uint8_t* pData = (const uint8_t*)pRequest->response.data();
	const size_t nSize = pRequest->response.size();

	LPTSTR fileName = ConvertCppStringToTString(pRequest->additional_data);
	HANDLE hnd = CreateFile(fileName, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	DWORD written = 0;
	if (hnd == INVALID_HANDLE_VALUE)
		goto _error;

	if (!WriteFile(hnd, pData, (DWORD) nSize, &written, NULL))
		goto _error;

	if (written != (DWORD)nSize)
		goto _error;

	CloseHandle(hnd);
	hnd = INVALID_HANDLE_VALUE;

	// yay!
	DbgPrintW("File saved: %s", pRequest->additional_data.c_str());
	ProgressDialog::Done(pRequest->key);
	SendMessage(hWnd, WM_IMAGESAVED, 0, (LPARAM)fileName);
	free(fileName);
	return;

_error:
	if (hnd != INVALID_HANDLE_VALUE)
		CloseHandle(hnd);

	TCHAR buff[4096];
	LPTSTR errorStr = GetTStringFromHResult(GetLastError());
	WAsnprintf(buff, _countof(buff), TmGetTString(IDS_ERROR_SAVING_FILE), fileName, errorStr);
	buff[_countof(buff) - 1] = 0;
	free(errorStr);
	free(fileName);
	MessageBox(hWnd, buff, TmGetTString(IDS_PROGRAM_NAME), MB_ICONERROR);
	ProgressDialog::Done(pRequest->key);
	SendMessage(hWnd, WM_IMAGECLEARSAVE, 0, 0);
	return;
}

LPCTSTR GetFilter(const std::string& fileName)
{
	if (EndsWith(fileName, ".png"))  return TEXT("PNG file\0*.png\0\0");
	if (EndsWith(fileName, ".jpg") ||
		EndsWith(fileName, ".jpeg")) return TEXT("JPG file\0*.jpg\0JPEG file\0*.jpeg\0\0");
	if (EndsWith(fileName, ".gif"))  return TEXT("GIF file\0*.webp\0\0");
	if (EndsWith(fileName, ".webp")) return TEXT("WEBP file\0*.webp\0\0");
	if (EndsWith(fileName, ".zip"))  return TEXT("ZIP file\0*.zip\0\0");
	return TEXT("All files\0*.*\0\0");
}

void DownloadFileDialog(HWND hWnd, const std::string& url, const std::string& fileName)
{
	const int MAX_FILE = 4096;

	LPTSTR fileTitle2 = ConvertCppStringToTString(fileName);

	LPTSTR buff = (LPTSTR) new TCHAR[MAX_FILE];
	buff[MAX_FILE - 1] = 0;
	buff[0] = 0;
	_tcsncpy(buff, fileTitle2, MAX_FILE);
	free(fileTitle2);

	OPENFILENAME ofn{};
	ofn.lStructSize = SIZEOF_OPENFILENAME_NT4;
	ofn.hwndOwner   = hWnd;
	ofn.hInstance   = g_hInstance;
	ofn.lpstrFilter = GetFilter(fileName);
	ofn.lpstrFile   = buff;
	ofn.nMaxFile    = MAX_FILE;
	ofn.lpstrTitle  = TmGetTString(IDS_SAVE_FILE_AS_TITLE);

	int nFileExtension = 0;
	for (int i = int(fileName.size()) - 1; i > 0 && !nFileExtension; i--)
	{
		if (fileName[i] == '.')
			nFileExtension = i + 1;
	}

	ofn.nFileExtension = nFileExtension;
	ofn.lpstrDefExt = nFileExtension ? (buff + nFileExtension) : TEXT("");

	std::string fileNameSave;

	if (!GetSaveFileName(&ofn)) {
		// maybe they cancelled.  I hope there's no error!
		goto _cleanup;
	}

	// perform the save
	fileNameSave = MakeStringFromTString(ofn.lpstrFile);

	GetHTTPClient()->PerformRequest(
		true,
		NetRequest::GET_PROGRESS,
		url,
		0,
		(uint64_t) hWnd,
		"",
		"",
		fileNameSave,
		DownloadFileResponse
	);

	SendMessage(hWnd, WM_IMAGESAVING, 0, 0);

	ProgressDialog::Show(fileNameSave, (Snowflake) hWnd, false, hWnd);

_cleanup:
	free(buff);
}

SIZE EnsureMaximumSize(int width, int height, int maxWidth, int maxHeight)
{
	SIZE sz { width, height };

	if (maxHeight > 0 && sz.cx > maxWidth)
	{
		if (sz.cx == 0) {
			sz.cy = 0;
			return sz;
		}
		sz.cy = MulDiv(sz.cy, maxWidth, sz.cx);
		sz.cx = maxWidth;
	}
	if (maxHeight > 0 && sz.cy > maxHeight)
	{
		if (sz.cy == 0) {
			sz.cx = 0;
			return sz;
		}
		sz.cx = MulDiv(sz.cx, maxHeight, sz.cy);
		sz.cy = maxHeight;
	}

	return sz;
}

int g_cutDownFlags = -1;

void PrepareCutDownFlags(LPSTR cmdLine)
{
	// Okay, calculate it.
	LPSTR found = strstr(cmdLine, "/help");
	if (found)
	{
		// Well they asked for help so we should give them help shouldn't we?
		MessageBox(
			NULL,
			TEXT(
				"You found the secret Discord Messenger command line help!\n\n"
				"Okay, just kidding.  But have some potentially useful command line switches:\n\n"
				"/startup - Used when the \"Open Discord Messenger when your computer starts\" option is checked.  Starts the application minimized.\n"
				"/help - Shows this text box.  This is the only setting right now."
				"/disable=[hexmask] - Disables certain DLLs from loading within MWAS.\n"
				"\t001 - user32.dll\n"
				"\t002 - gdi32.dll\n"
				"\t004 - shell32.dll\n"
				"\t008 - msimg32.dll\n"
				"\t010 - shlwapi.dll\n"
				"\t020 - crypt32.dll\n"
				"\t040 - ws2_32.dll\n"
				"\t080 - ole32.dll\n"
				"\t100 - comctl32.dll\n"
				"\t1FF - all of the DLLs\n\n"
				"If you are running on beta builds of Windows 95, approximately build 263 or so, "
				"the suggested disable flags are \"/disable=FFF\" to disable everything and make "
				"the application run with the bare minimum of dependencies."
			),
			TEXT("Hey, a secret"),
			MB_ICONINFORMATION | MB_OK
		);

		exit(1);
	}

	found = strstr(cmdLine, "/disable=");
	if (!found) {
		g_cutDownFlags = 0;
		return;
	}

	int disableFlags = 0;

	found += 9; // length of "/disable="
	(void) sscanf(found, "%x", &disableFlags);

	g_cutDownFlags = disableFlags;
}

int GetCutDownFlags()
{
	return g_cutDownFlags;
}

bool SupportsDialogEx()
{
	static bool _initted = false;
	static bool _supports = false;

	if (_initted)
		return _supports;

	if (GetCutDownFlags() & DMCDF_NODLGEX) {
		_initted = true;
		_supports = false;
		return false;
	}

	// I know GetVersion() is deprecated, but what are you gonna do?
	// It'll return 6.2.9200, but fine, that's still not Windows 2000.

	// Note - declared FKG_FORCED_USAGE at the top to prevent VC from
	// preventing me from using it.
	DWORD version = GetVersion();
	WORD majmin = LOWORD(version);
	BYTE major = LOBYTE(majmin);
	BYTE minor = HIBYTE(majmin);

	_initted = true;

	// Check for Windows 4.00 and greater.
	if (major > 3) _supports = true;
	// Check for Windows NT 3.51 or greater.
	else if (major == 3 && minor > 50) _supports = true;
	// Surely using a version older than 3.51, so don't support.
	else _supports = false;

	return _supports;
}

bool Supports32BitIcons()
{
	static bool _initted = false;
	static bool _supports = false;

	if (_initted)
		return _supports;

	// I know GetVersion() is deprecated, but what are you gonna do?
	// It'll return 6.2.9200, but fine, that's still not Windows 2000.
	
	// Note - declared FKG_FORCED_USAGE at the top to prevent VC from
	// preventing me from using it.
	DWORD version = GetVersion();
	WORD majmin = LOWORD(version);
	BYTE major = LOBYTE(majmin);
	BYTE minor = HIBYTE(majmin);

	_initted = true;

	// Check for Windows 6.00 and greater.
	if (major > 5) _supports = true;
	// Check for Windows 5.1 or greater.
	else if (major == 5 && minor > 0) _supports = true;
	// Surely using a version older than 5.1, so don't support.
	else _supports = false;

	return _supports;
}

int MapIconToOldIfNeeded(int iconID)
{
	// No mapping if icon ID is supported.
	if (Supports32BitIcons())
		return iconID;

	switch (iconID)
	{
		case IDI_PROFILE_BORDER_UNREAD: return IDI_PROFILE_BORDER_UNREAD_2K;
		case IDI_PROFILE_BORDER_GOLD: return IDI_PROFILE_BORDER_GOLD_2K;
		case IDI_CHANNEL_MENTIONED: return IDI_CHANNEL_MENTIONED_2K;
		case IDI_CHANNEL_UNREAD: return IDI_CHANNEL_UNREAD_2K;
		case IDI_PROFILE_BORDER: return IDI_PROFILE_BORDER_2K;
		case IDI_TYPING_FRAME1: return IDI_TYPING_FRAME1_2K;
		case IDI_TYPING_FRAME2: return IDI_TYPING_FRAME2_2K;
		case IDI_TYPING_FRAME3: return IDI_TYPING_FRAME3_2K;
		case IDI_NOTIFICATION: return IDI_NOTIFICATION_2K;
		case IDI_REPLY_PIECE: return IDI_REPLY_PIECE_2K;
		case IDI_SHIFT_RIGHT: return IDI_SHIFT_RIGHT_2K;
		case IDI_NEW_INLINE: return IDI_NEW_INLINE_2K;
		case IDI_SHIFT_LEFT: return IDI_SHIFT_LEFT_2K;
		case IDI_CATEGORY: return IDI_CATEGORY_2K;
		case IDI_CHANNEL: return IDI_CHANNEL_2K;
		case IDI_MEMBERS: return IDI_MEMBERS_2K;
		case IDI_GROUPDM: return IDI_GROUPDM_2K;
		case IDI_SERVER: return IDI_SERVER_2K;
		case IDI_BOOST: return IDI_BOOST_2K;
		case IDI_VOICE: return IDI_VOICE_2K;
		case IDI_FILE: return IDI_FILE_2K;
		case IDI_JUMP: return IDI_JUMP_2K;
		case IDI_SEND: return IDI_SEND_2K;
		case IDI_ICON: return IDI_ICON_2K;
		case IDI_BOT: return IDI_BOT_2K;
		case IDI_NEW: return IDI_NEW_2K;
		case IDI_PIN: return IDI_PIN_2K;
		case IDI_DM: return IDI_DM_2K;

		default: // No mapping
			return iconID;
	}
}

int MapDialogToOldIfNeeded(int iid)
{
	if (SupportsDialogEx())
		return iid;

	switch (iid)
	{
		case IDD_DIALOG_ABOUT: return IDD_DIALOG_ABOUT_ND;
		case IDD_DIALOG_GUILD_CHOOSER: return IDD_DIALOG_GUILD_CHOOSER_ND;
		case IDD_DIALOG_LOGON: return IDD_DIALOG_LOGON_ND;
		case IDD_DIALOG_NOTIFICATIONS: return IDD_DIALOG_NOTIFICATIONS_ND;
		case IDD_DIALOG_PINNEDMESSAGES: return IDD_DIALOG_PINNEDMESSAGES_ND;
		case IDD_DIALOG_PROFILE_POPOUT: return IDD_DIALOG_PROFILE_POPOUT_ND;
		case IDD_DIALOG_QUICK_SWITCHER: return IDD_DIALOG_QUICK_SWITCHER_ND;
		case IDD_DIALOG_UPLOADFILES: return IDD_DIALOG_UPLOADFILES_ND;
		case IDD_DIALOG_UPLOADING: return IDD_DIALOG_UPLOADING_ND;
		case IDD_DIALOG_OPTIONS: return IDD_DIALOG_PREFERENCES_ND;

		default: // no mapping
			DbgPrintW("Warning, no non-dialogex-mapping for dialog id %d. You might see a broken dialog", iid);
			return iid;
	}
}

HICON g_MentionMarkerIcons[10];
HICON g_ProfileStatusIcons[4];
HICON g_WaitIcon;
HICON g_ImgErrorIcon;

void InitializeStatusIcons()
{
	for (int i = 0; i < 10; i++) {
		g_MentionMarkerIcons[i] = (HICON) ri::LoadImage(g_hInstance, MAKEINTRESOURCE(IDI_MENTION_MARKER_1 + i), IMAGE_ICON, 0, 0, LR_CREATEDIBSECTION | LR_SHARED);
	}
	for (int i = 0; i < 4; i++) {
		g_ProfileStatusIcons[i] = (HICON) ri::LoadImage(g_hInstance, MAKEINTRESOURCE(IDI_STATUS_OFFLINE + i), IMAGE_ICON, 0, 0, LR_CREATEDIBSECTION | LR_SHARED);
	}

	int smcxicon   = GetSystemMetrics(SM_CXICON);
	int smcxsmicon = GetSystemMetrics(SM_CXSMICON);
	g_WaitIcon     = (HICON) ri::LoadImage(g_hInstance, MAKEINTRESOURCE(IDI_WAIT),        IMAGE_ICON, smcxicon,   smcxicon,   LR_CREATEDIBSECTION | LR_SHARED);
	g_ImgErrorIcon = (HICON) ri::LoadImage(g_hInstance, MAKEINTRESOURCE(IDI_IMAGE_ERROR), IMAGE_ICON, smcxsmicon, smcxsmicon, LR_CREATEDIBSECTION | LR_SHARED);
}

void DrawIconInsideProfilePicture(HDC hdc, int xp, int yp, HICON icon)
{
	int pfpSize = ScaleByDPI(PROFILE_PICTURE_SIZE_DEF);
	int iconSize = ScaleByDPI(16);

	int xb = xp + pfpSize - iconSize;
	int yb = yp + pfpSize - iconSize;

	ri::DrawIconEx(hdc, xb, yb, icon, iconSize, iconSize, 0, NULL, DI_COMPAT | DI_NORMAL);
}

void DrawMentionStatus(HDC hdc, int x, int y, int mentionCount)
{
	if (mentionCount < 1) return;
	mentionCount--;
	if (mentionCount > 9) mentionCount = 9;
	DrawIconInsideProfilePicture(hdc, x, y, g_MentionMarkerIcons[mentionCount]);
}

// Draws the activity status inside a profile picture sized and shaped box.
void DrawActivityStatus(HDC hdc, int x, int y, eActiveStatus status)
{
	DrawIconInsideProfilePicture(hdc, x, y, g_ProfileStatusIcons[int(status)]);
}

void DrawLoadingBox(HDC hdc, RECT rect)
{
	ri::DrawEdge(hdc, &rect, BDR_SUNKEN, BF_RECT);

	HRGN rgn = CreateRectRgnIndirect(&rect);
	SelectClipRgn(hdc, rgn);

	int smcxicon = GetSystemMetrics(SM_CXICON);
	int x = rect.left + (rect.right - rect.left - smcxicon) / 2;
	int y = rect.top  + (rect.bottom - rect.top - smcxicon) / 2;
	ri::DrawIconEx(hdc, x, y, g_WaitIcon, smcxicon, smcxicon, 0, NULL, DI_COMPAT | DI_NORMAL);

	SelectClipRgn(hdc, NULL);
	DeleteRgn(rgn);
}

void DrawErrorBox(HDC hdc, RECT rect)
{
	ri::DrawEdge(hdc, &rect, BDR_SUNKEN, BF_RECT);

	HRGN rgn = CreateRectRgnIndirect(&rect);
	SelectClipRgn(hdc, rgn);

	int smcxsmicon = GetSystemMetrics(SM_CXSMICON);
	int x = rect.left + (rect.right - rect.left - smcxsmicon) / 2;
	int y = rect.top  + (rect.bottom - rect.top - smcxsmicon) / 2;
	ri::DrawIconEx(hdc, x, y, g_ImgErrorIcon, smcxsmicon, smcxsmicon, 0, NULL, DI_COMPAT | DI_NORMAL);

	SelectClipRgn(hdc, NULL);
	DeleteRgn(rgn);
}

bool GetDataFromBitmap(HDC hdc, HBITMAP hbm, BYTE*& pBytes, int& width, int& height, int& bpp)
{	
	// Man what the hell
	BITMAP bm;
	BITMAPINFO bmi;

	ZeroMemory(&bm, sizeof bm);
	if (!GetObject(hbm, sizeof bm, &bm)) {
		DbgPrintW("Cannot obtain pointer to bitmap!");
		return false;
	}

	width = bm.bmWidth;
	height = bm.bmHeight;
	bpp = bm.bmBitsPixel;

	ZeroMemory(&bmi, sizeof bmi);
	bmi.bmiHeader.biSize = sizeof bmi.bmiHeader;
	bmi.bmiHeader.biWidth = bm.bmWidth;
	bmi.bmiHeader.biHeight = bm.bmHeight;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = bm.bmBitsPixel;
	bmi.bmiHeader.biCompression = BI_RGB;
	bmi.bmiHeader.biSizeImage = 0;

	int pitch = (((bm.bmWidth * bm.bmBitsPixel) + 31) & ~31) >> 3;

	pBytes = new BYTE[pitch * bm.bmHeight];

	if (!GetDIBits(hdc, hbm, 0, bm.bmHeight, pBytes, &bmi, DIB_RGB_COLORS))
	{
		DbgPrintW("Error, can't get DI bits for bitmap!");
		delete[] pBytes;
		return false;
	}

	// swap in software, because in Windows NT 3.1, using negative height images is glitchy
	for (int y1 = 0, y2 = bm.bmHeight - 1; y1 < y2; y1++, y2--)
	{
		BYTE *scan1 = &pBytes[y1 * pitch], *scan2 = &pBytes[y2 * pitch];
		DWORD *scan1D = (DWORD*) scan1, *scan2D = (DWORD*) scan2;

		for (int x = 0; x * 4 < pitch; x++) {
			std::swap(scan1D[x], scan2D[x]);
		}
	}

	return true;
}

std::string CutOutURLPath(const std::string& url)
{
	const char* pData = url.c_str();

	// All this is to cut out the part after the URL. (so https://discord.com/test -> https://discord.com)
	int slashesToSkip = 1;

	const char* startLookUp = strstr(pData, "://");
	if (startLookUp)
		slashesToSkip = 3;
	else
		startLookUp = pData;

	size_t i = 0;
	for (; startLookUp[i] != '\0'; i++) {
		if (startLookUp[i] == '/')
			slashesToSkip--;
		if (!slashesToSkip)
			break;
	}

	std::string actualUrl;
	if (slashesToSkip != 0) {
		actualUrl = std::string(pData);
	}
	else {
		actualUrl = std::string(pData, i + (startLookUp - pData));
	}

	return actualUrl;
}

LRESULT HandleGestureMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, float mulDeltas)
{
	// N.B.  If WINVER is less than Windows 7, then MWAS will take on winuser's duties
	// of defining the structs and constants used.
	GESTUREINFO gi = { 0 };
	gi.cbSize = sizeof(GESTUREINFO);
	if (!ri::GetGestureInfo((HGESTUREINFO)lParam, &gi))
		return DefWindowProc(hWnd, uMsg, wParam, lParam);

	static SHORT S_lastX = 0;
	static SHORT S_lastY = 0;
	static bool S_beginSent = true;
	switch (gi.dwID)
	{
		case GID_BEGIN:
		case GID_END: {
			S_beginSent = gi.dwID == GID_BEGIN;
			S_lastX = gi.ptsLocation.x;
			S_lastY = gi.ptsLocation.y;
			return 0;
		}

		case GID_PAN: {
			assert(S_beginSent);
			short xDelta = short((gi.ptsLocation.x - S_lastX) * mulDeltas);
			short yDelta = short((gi.ptsLocation.y - S_lastY) * mulDeltas);

			SendMessage(hWnd, WM_MOUSEWHEEL, MAKEWPARAM(0, yDelta), 0);
			SendMessage(hWnd, WM_MOUSEHWHEEL, MAKEWPARAM(0, -xDelta), 0);

			S_lastX = gi.ptsLocation.x;
			S_lastY = gi.ptsLocation.y;
			return 0;
		}
	}

	(void)mulDeltas;
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void CenterWindow(HWND hWnd, HWND hRelativeTo)
{
	RECT rect1, rect2;
	GetWindowRect(hWnd, &rect1);
	GetWindowRect(hRelativeTo, &rect2);

	OffsetRect(&rect1, -rect1.left, -rect1.top);
	int width = rect1.right - rect1.left, height = rect1.bottom - rect1.top;

	OffsetRect(
		&rect1,
		(rect2.left + rect2.right) / 2 - (rect1.right - rect1.left) / 2,
		(rect2.top + rect2.bottom) / 2 - (rect1.bottom - rect1.top) / 2
	);

	MoveWindow(hWnd, rect1.left, rect1.top, rect1.right - rect1.left, rect1.bottom - rect1.top, TRUE);
}

#ifndef MINGW_SPECIFIC_HACKS
#include <mutex>
using Wmutex = std::mutex;
#else
#include <iprog/mutex.hpp>
using Wmutex = iprog::mutex;
#endif

static const LPCTSTR* s_msgBoxText;
static const int*     s_msgBoxIDReplaced;

static Wmutex s_msgBoxMutex;
static int    s_msgBoxCount;
static HHOOK  s_msgBoxHook;

LRESULT CALLBACK MessageBoxHookFunc(int code, WPARAM wParam, LPARAM lParam)
{
	if (code == HC_ACTION)
	{
		CWPRETSTRUCT* cwp = (CWPRETSTRUCT*)lParam;

		if (cwp->message == WM_INITDIALOG) {
			HWND hWnd = cwp->hwnd;

			for (int i = 0; i < s_msgBoxCount; i++) {
				SetDlgItemText(hWnd, s_msgBoxIDReplaced[i], s_msgBoxText[i]);
			}
			s_msgBoxCount = 0;
		}
		
		LRESULT res = CallNextHookEx(s_msgBoxHook, code, wParam, lParam);

		if (cwp->message == WM_INITDIALOG) {
			// just unhook the hook, we don't care about overriding any other behavior
			UnhookWindowsHookEx(s_msgBoxHook);
			s_msgBoxHook = NULL;

			// mutex was locked before the MessageBox call!
#pragma warning(push)
#pragma warning(disable : 26110)
			s_msgBoxMutex.unlock();
#pragma warning(pop)
		}

		return res;
	}

	return CallNextHookEx(s_msgBoxHook, code, wParam, lParam);
}

// This calls MessageBox() with the important difference that one of the buttons (specified by overID)
// will have its text changed via a hook.
int MessageBoxHooked(HWND hWnd, LPCTSTR title, LPCTSTR caption, int flags, int overCount, const int* overID, const LPCTSTR* overText)
{
	s_msgBoxMutex.lock(); // will be unlocked in MessageBox's hook

	s_msgBoxCount = overCount;
	s_msgBoxText = overText;
	s_msgBoxIDReplaced = overID;

	s_msgBoxHook = SetWindowsHookEx(WH_CALLWNDPROCRET, MessageBoxHookFunc, NULL, GetCurrentThreadId());

	return MessageBox(hWnd, title, caption, flags);
}

int MessageBoxHooked(HWND hWnd, LPCTSTR title, LPCTSTR caption, int flags, int overID, LPCTSTR overText)
{
	return MessageBoxHooked(hWnd, title, caption, flags, 1, &overID, &overText);
}

bool IsColorDark(COLORREF cr)
{
	uint8_t c1 = cr & 0xFF; cr >>= 8;
	uint8_t c2 = cr & 0xFF; cr >>= 8;
	uint8_t c3 = cr & 0xFF;
	int avg = (c1 + c2 + c3) / 3;
	return avg <= 128;
}

bool IsTextColorDark()
{
	return IsColorDark(GetSysColor(COLOR_CAPTIONTEXT));
}

std::map<HICON, bool> m_bMostlyBlack;

bool IsIconMostlyBlack(HICON hic)
{
	auto it = m_bMostlyBlack.find(hic);
	if (it != m_bMostlyBlack.end())
		return it->second;

	bool isit = false;
	ICONINFO ii{};
	GetIconInfo(hic, &ii);

	if (ii.hbmColor)
	{
		BITMAP bm{};
		GetObject(ii.hbmColor, sizeof bm, &bm);

		BITMAPINFO bmi;
		ZeroMemory(&bmi, sizeof(BITMAPINFO));
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = bm.bmWidth;
		bmi.bmiHeader.biHeight = bm.bmHeight;
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 32;
		bmi.bmiHeader.biCompression = BI_RGB;

		uint32_t* bits = new uint32_t[bm.bmWidth * bm.bmHeight];

		HDC hdc = GetDC(g_Hwnd);
		if (GetDIBits(hdc, ii.hbmColor, 0, bm.bmHeight, bits, &bmi, DIB_RGB_COLORS))
		{
			// check!
			isit = true;
			size_t pcount = bm.bmWidth * bm.bmHeight;
			for (size_t i = 0; i < pcount; i++) {
				uint32_t cr = bits[i];
				uint8_t b1 = cr & 0xFF; cr >>= 8;
				uint8_t b2 = cr & 0xFF; cr >>= 8;
				uint8_t b3 = cr & 0xFF;
				if ((b1 + b2 + b3) >= 0x24) {
					// not mostly black
					isit = false;
					break;
				}
			}
		}

		ReleaseDC(g_Hwnd, hdc);
		if (ii.hbmColor) DeleteBitmap(ii.hbmColor);
		if (ii.hbmMask)  DeleteBitmap(ii.hbmMask);
		delete[] bits;
	}
	else
	{
		// it just is.
		isit = true;
	}

	m_bMostlyBlack[hic] = isit;
	return isit;
}

void DrawIconInvert(HDC hdc, HICON hIcon, int x, int y, int sizeX, int sizeY, bool invert)
{
	if (!invert) {
		ri::DrawIconEx(hdc, x, y, hIcon, sizeX, sizeY, 0, NULL, DI_NORMAL | DI_COMPAT);
		return;
	}

	HDC hdcMem = CreateCompatibleDC(hdc);
	HBITMAP hbm = CreateCompatibleBitmap(hdc, sizeX, sizeY);
	HGDIOBJ oldObj = SelectObject(hdcMem, hbm);

	// take the contents of the HDC there and invert them.
	BitBlt(hdcMem, 0, 0, sizeX, sizeY, hdc, x, y, NOTSRCCOPY);

	ri::DrawIconEx(hdcMem, 0, 0, hIcon, sizeX, sizeY, 0, NULL, DI_NORMAL | DI_COMPAT);

	// now blit it back out inverted
	BitBlt(hdc, x, y, sizeX, sizeY, hdcMem, 0, 0, NOTSRCCOPY);

	// clean up
	SelectObject(hdcMem, oldObj);
	DeleteBitmap(hbm);
	DeleteDC(hdcMem);
}

// Resizes a transparent image with a transparent color, without looking bad.
// NOTE: Creates a new bitmap.  Make sure to delete the bitmap when done.
HBITMAP ResizeWithBackgroundColor(HDC hdc, HBITMAP hBitmap, HBRUSH backgroundColor, bool bHasAlpha, int newSizeX, int newSizeY, int oldSizeX, int oldSizeY)
{
	// create 3 compatible DCs. Ungodly
	HDC hdc1 = CreateCompatibleDC(hdc);
	HDC hdc2 = CreateCompatibleDC(hdc);
	HDC hdc3 = CreateCompatibleDC(hdc);

	// this "hack" bitmap will hold an intermediate: the profile picture but behind a COLOR_3DFACE background.
	// This allows us to call StretchBlt with no loss of quality while also looking like it's blended in.
	HBITMAP hackBitmap = CreateCompatibleBitmap(hdc, oldSizeX, oldSizeY);
	HBITMAP newBitmap = CreateCompatibleBitmap(hdc, newSizeX, newSizeY);

	// select the bitmaps
	HGDIOBJ a1 = SelectObject(hdc1, hBitmap);
	HGDIOBJ a2 = SelectObject(hdc2, hackBitmap);
	HGDIOBJ a3 = SelectObject(hdc3, newBitmap);

	// into hdc2, we will first draw the background and then alphablend the profile picture if has alpha
	RECT rcFull = { 0, 0, oldSizeX, oldSizeY };
	BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
	FillRect(hdc2, &rcFull, backgroundColor);

	if (bHasAlpha)
		ri::AlphaBlend(hdc2, 0, 0, oldSizeX, oldSizeY, hdc1, 0, 0, oldSizeX, oldSizeY, bf);
	else
		BitBlt(hdc2, 0, 0, oldSizeX, oldSizeY, hdc1, 0, 0, SRCCOPY);

	// now draw from hdc2 into hdc3
	int mode = SetStretchBltMode(hdc3, ri::GetHalfToneStretchMode());
	StretchBlt(hdc3, 0, 0, newSizeX, newSizeY, hdc2, 0, 0, oldSizeX, oldSizeY, SRCCOPY);
	SetStretchBltMode(hdc3, mode);

	// cleanup
	SelectObject(hdc1, a1);
	SelectObject(hdc2, a2);
	SelectObject(hdc3, a3);
	DeleteDC(hdc1);
	DeleteDC(hdc2);
	DeleteDC(hdc3);
	DeleteObject(hackBitmap);

	return newBitmap;
}

static time_t FileTimeToTimeT(LPFILETIME ft)
{
	ULARGE_INTEGER uli {};
	uli.LowPart = ft->dwLowDateTime;
	uli.HighPart = ft->dwHighDateTime;
	auto lt = (uli.QuadPart - 116444736000000000LL) / 10000000LL;

	assert(lt < 2000000000);
	if (lt > 2147483647)
		lt = 2147483647;

	return (time_t) lt;
}

time_t MakeGMTime(const tm* ptime)
{
	SYSTEMTIME st { };
	st.wYear      = ptime->tm_year ? (1900 + ptime->tm_year) : 0;
	st.wMonth     = 1 + ptime->tm_mon;
	st.wDay       = ptime->tm_mday;
	st.wDayOfWeek = ptime->tm_wday;
	st.wHour      = ptime->tm_hour;
	st.wMinute    = ptime->tm_min;
	st.wSecond    = ptime->tm_sec;

	FILETIME ft { };
	SystemTimeToFileTime(&st, &ft);

	if (ft.dwHighDateTime == 0 && ft.dwLowDateTime == 0)
		return 0;

	return FileTimeToTimeT(&ft);
}

// N.B. WINVER<=0x0500 doesn't define it. We'll force it
#ifndef IDC_HAND
#define IDC_HAND            MAKEINTRESOURCE(32649)
#endif//IDC_HAND

HCURSOR GetHandCursor()
{
	static HCURSOR loaded = NULL;
	if (loaded)
		return loaded;

	loaded = LoadCursor(NULL, IDC_HAND);
	if (!loaded)
		loaded = LoadCursor(g_hInstance, MAKEINTRESOURCE(IDC_CLICKER));

	return loaded;
}
