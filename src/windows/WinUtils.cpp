#define WIN32_LEAN_AND_MEAN
#define FKG_FORCED_USAGE
#include <algorithm>
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include "WinUtils.hpp"

#include "../resource.h"
#include "WindowMessages.hpp"
#include "Measurements.hpp"
#include "NetworkerThread.hpp"
#include "ImageViewer.hpp"
#include "TextManager.hpp"

#ifndef OLD_WINDOWS
#include <shlwapi.h>
#include "../discord/Util.hpp"
#endif

#define DEFAULT_DPI (96)

extern HWND g_Hwnd; // main.hpp
extern HINSTANCE g_hInstance; // main.hpp

void InitializeCOM()
{
	if (FAILED(CoInitialize(NULL)))
		exit(1);
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

int ScaleByDPI(int pos)
{
	return MulDiv( pos, GetSystemDPI(), DEFAULT_DPI );
}

int UnscaleByDPI(int pos)
{
	return MulDiv( pos, DEFAULT_DPI, GetSystemDPI() );
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
	LPTSTR tstr = NULL;
	FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		hr,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPWSTR)&tstr,
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
		LocalFree(tstr);

	return newStr;
}

std::string GetStringFromHResult(HRESULT hr)
{
	LPTSTR tstr = GetTStringFromHResult(hr);
	std::string str = MakeStringFromUnicodeString(tstr);
	LocalFree(tstr);
	return str;
}

void CopyStringToClipboard(const std::string& str)
{
	if (OpenClipboard(g_Hwnd))
	{
		EmptyClipboard();

		LPCTSTR ctstr = ConvertCppStringToTString(str);

		size_t stringSize = (str.length() + 1) * sizeof(TCHAR);

		// create a global buffer and fill its text in
		HGLOBAL clipbuffer;
		char* buffer;
		clipbuffer = GlobalAlloc(GMEM_DDESHARE, stringSize);
		buffer = (char*)GlobalLock(clipbuffer);
		memcpy(buffer, ctstr, stringSize);
		GlobalUnlock(clipbuffer);

		// set the clipboard data
		SetClipboardData(CF_UNICODETEXT, clipbuffer);

		// release everything we have
		CloseClipboard();

		free((void*)ctstr);
	}
}

LPTSTR ConvertCppStringToTString(const std::string& sstr, size_t* lenOut)
{
	size_t sz = sstr.size() + 1;
	TCHAR* str = (TCHAR*)calloc(sz, sizeof(TCHAR));

#ifdef UNICODE
	size_t convertedChars = MultiByteToWideChar(CP_UTF8, 0, sstr.c_str(), -1, str, sz);
#else // ANSI
	strcpy_s(str, sz, sstr.c_str());
#endif

	if (lenOut)
		*lenOut = convertedChars;

	return (LPTSTR)str;
}

void ConvertCppStringToTCharArray(const std::string& sstr, TCHAR* buff, size_t szMax)
{
	int len = MultiByteToWideChar(CP_UTF8, 0, sstr.c_str(), -1, buff, (int)szMax - 1);
	buff[szMax - 1] = 0;
	buff[len] = 0;
}

// note: this works on ANSI too
std::string MakeStringFromUnicodeString(LPCWSTR wstr)
{
	size_t sl = lstrlenW(wstr);
	// generate the size of the UTF-8 string
	size_t slmax = (sl + 1) * 4; // whatever
	char* chr = (char*)malloc(slmax);
	size_t sz;
	//_wcstombs_s_l(&sz, chr, slmax, wstr, slmax, CP_UTF8);
	WideCharToMultiByte(CP_UTF8, 0, wstr, -1, chr, slmax, NULL, NULL);

	chr[slmax - 1] = 0; // ensure the null terminator is there
	std::string final_str(chr);
	free(chr);
	return final_str;
}

std::string CreateMessageLink(Snowflake guild, Snowflake channel, Snowflake message)
{
	std::string guildStr = std::to_string(guild);
	if (!guild) guildStr = "@me";

	return "https://discord.com/channels/" + guildStr + "/" + std::to_string(channel) + "/" + std::to_string(message);
}

void DrawBitmap(HDC hdc, HBITMAP bitmap, int x, int y, LPRECT clip, COLORREF transparent, int scaleToWidth, int scaleToHeight)
{
	HRGN hrgn = NULL;
	if (clip) {
		hrgn = CreateRectRgn(clip->left, clip->top, clip->right, clip->bottom);
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

	if (transparent != CLR_NONE)
	{
		ri::TransparentBlt(hdc, x, y, scaledW, scaledH, hdcMem, 0, 0, bm.bmWidth, bm.bmHeight, RGB(255, 0, 255));
	}
	else if (scaledW != bm.bmWidth || scaledH != bm.bmHeight)
	{
		int oldMode = SetStretchBltMode(hdc, HALFTONE);
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

void FillGradient(HDC hdc, const LPRECT lpRect, int sci1, int sci2, bool vertical)
{
	if (ri::HaveMsImg()) {
		COLORREF c1 = GetSysColor(sci1);
		COLORREF c2 = GetSysColor(sci2);
		FillGradientColors(hdc, lpRect, c1, c2, vertical);
	}
	else {
		HBRUSH hbr = GetSysColorBrush(sci1);
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
			hbm = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pvBits, NULL, 0);
			if (!hbm) {
				DbgPrintW("FillGradientColors : Could not create bitmap object!");
				goto _FALLBACK;
			}

			SelectObject(hdcDest, hbm);

			BITMAP bm{};
			if (!GetObject(hbm, sizeof(BITMAP), &bm)) {
				DbgPrintW("FillGradientColors : Could not get bitmap object!");
				DeleteObject(hbm);
				DeleteDC(hdcDest);
				goto _FALLBACK;
			}

			// Alpha Gradient
			PUINT32 puBits = (PUINT32)pvBits;
			COLORREF clr = (c1 == CLR_NONE ? c2 : c1);

			BYTE bytes[sizeof(COLORREF)];
			memcpy(bytes, &clr, sizeof(COLORREF));

			for (LONG y = 0, index = 0; y < ulHeight; y++)
			{
				UCHAR pcalpha;
				if (vertical) {
					if (firstTrans)
						pcalpha = (UCHAR)((ulHeight - y) * 255 / ulHeight);
					else
						pcalpha = (UCHAR)(y * 255 / ulHeight);
				}
				for (LONG x = 0; x < ulWidth; x++, index++)
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

			// Write the object onto the main DC
			BLENDFUNCTION bf{};
			bf.BlendOp = AC_SRC_OVER;
			bf.AlphaFormat = AC_SRC_ALPHA;
			bf.BlendFlags = 0;
			bf.SourceConstantAlpha = 50;

			ri::AlphaBlend(hdc, lpRect->left, lpRect->top, rc.right, rc.bottom, hdcDest, 0, 0, rc.right, rc.bottom, bf);

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
		FillRect(hdc, lpRect, GetStockBrush(DC_BRUSH));
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

#include "../discord/DiscordInstance.hpp"

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
	free(tstr);

	if (res > 32) return; // was fine

	TCHAR buff[4096];
	switch (res)
	{
		case 0:
			WAsnprintf(buff, _countof(buff), TEXT("Whoa, like, can't launch the URL " ASCIIZ_STR_FMT "!  Yer outa memory, duuude!"), link.c_str());
			break;

		default:
			WAsnprintf(buff, _countof(buff), TEXT("Can't launch URL " ASCIIZ_STR_FMT ", ShellExecute returned error %d.  Look \"ShellExecute error %d\" if you care."), link.c_str(), res, res);
			break;
	}

	MessageBox(g_Hwnd, buff, TEXT("Whoops"), MB_ICONERROR | MB_OK);
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
}

void DownloadFileResponse(NetRequest* pRequest)
{
	HWND hWnd = (HWND)pRequest->key;

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

	if (!WriteFile(hnd, pData, nSize, &written, NULL))
		goto _error;

	if (written != (DWORD)nSize)
		goto _error;

	CloseHandle(hnd);
	hnd = INVALID_HANDLE_VALUE;

	// yay!
	DbgPrintW("File saved: %s", pRequest->additional_data.c_str());
	free(fileName);
	SendMessage(hWnd, WM_IMAGESAVED, 0, 0);
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
	ofn.lStructSize = sizeof(ofn);
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
		NetRequest::GET,
		url,
		0,
		(uint64_t) hWnd,
		"",
		"",
		fileNameSave,
		DownloadFileResponse
	);

	SendMessage(hWnd, WM_IMAGESAVING, 0, 0);

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
		case IDI_PROFILE_BORDER_GOLD: return IDI_PROFILE_BORDER_GOLD_2K;
		case IDI_CHANNEL_MENTIONED: return IDI_CHANNEL_MENTIONED_2K;
		case IDI_CHANNEL_UNREAD: return IDI_CHANNEL_UNREAD_2K;
		case IDI_PROFILE_BORDER: return IDI_PROFILE_BORDER_2K;
		case IDI_TYPING_FRAME1: return IDI_TYPING_FRAME1_2K;
		case IDI_TYPING_FRAME2: return IDI_TYPING_FRAME2_2K;
		case IDI_TYPING_FRAME3: return IDI_TYPING_FRAME3_2K;
		case IDI_REPLY_PIECE: return IDI_REPLY_PIECE_2K;
		case IDI_CATEGORY: return IDI_CATEGORY_2K;
		case IDI_CHANNEL: return IDI_CHANNEL_2K;
		case IDI_MEMBERS: return IDI_MEMBERS_2K;
		case IDI_GROUPDM: return IDI_GROUPDM_2K;
		case IDI_SERVER: return IDI_SERVER_2K;
		case IDI_VOICE: return IDI_VOICE_2K;
		case IDI_FILE: return IDI_FILE_2K;
		case IDI_JUMP: return IDI_JUMP_2K;
		case IDI_SEND: return IDI_SEND_2K;
		case IDI_BOT: return IDI_BOT_2K;
		case IDI_PIN: return IDI_PIN_2K;
		case IDI_DM: return IDI_DM_2K;

		default: // No mapping
			return iconID;
	}
}

HICON g_MentionMarkerIcons[10];
HICON g_ProfileStatusIcons[4];
HICON g_WaitIcon;
HICON g_ImgErrorIcon;

void InitializeStatusIcons()
{
	for (int i = 0; i < 10; i++) {
		g_MentionMarkerIcons[i] = (HICON) LoadImage(g_hInstance, MAKEINTRESOURCE(IDI_MENTION_MARKER_1 + i), IMAGE_ICON, 0, 0, LR_CREATEDIBSECTION | LR_SHARED);
	}
	for (int i = 0; i < 4; i++) {
		g_ProfileStatusIcons[i] = (HICON) LoadImage(g_hInstance, MAKEINTRESOURCE(IDI_STATUS_ONLINE + i), IMAGE_ICON, 0, 0, LR_CREATEDIBSECTION | LR_SHARED);
	}

	int smcxicon   = GetSystemMetrics(SM_CXICON);
	int smcxsmicon = GetSystemMetrics(SM_CXSMICON);
	g_WaitIcon     = (HICON) LoadImage(g_hInstance, MAKEINTRESOURCE(IDI_WAIT),        IMAGE_ICON, smcxicon,   smcxicon,   LR_CREATEDIBSECTION | LR_SHARED);
	g_ImgErrorIcon = (HICON) LoadImage(g_hInstance, MAKEINTRESOURCE(IDI_IMAGE_ERROR), IMAGE_ICON, smcxsmicon, smcxsmicon, LR_CREATEDIBSECTION | LR_SHARED);
}

void DrawIconInsideProfilePicture(HDC hdc, int xp, int yp, HICON icon)
{
	int pfpSize = ScaleByDPI(PROFILE_PICTURE_SIZE_DEF);
	int iconSize = ScaleByDPI(16);

	int xb = xp + pfpSize - iconSize;
	int yb = yp + pfpSize - iconSize;

	DrawIconEx(hdc, xb, yb, icon, iconSize, iconSize, 0, NULL, DI_COMPAT | DI_NORMAL);
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
	DrawEdge(hdc, &rect, BDR_SUNKEN, BF_RECT);

	int smcxicon = GetSystemMetrics(SM_CXICON);
	int x = rect.left + (rect.right - rect.left - smcxicon) / 2;
	int y = rect.top  + (rect.bottom - rect.top - smcxicon) / 2;
	DrawIconEx(hdc, x, y, g_WaitIcon, smcxicon, smcxicon, 0, NULL, DI_COMPAT | DI_NORMAL);
}

void DrawErrorBox(HDC hdc, RECT rect)
{
	DrawEdge(hdc, &rect, BDR_SUNKEN, BF_RECT);

	int smcxsmicon = GetSystemMetrics(SM_CXSMICON);
	int x = rect.left + (rect.right - rect.left - smcxsmicon) / 2;
	int y = rect.top  + (rect.bottom - rect.top - smcxsmicon) / 2;
	DrawIconEx(hdc, x, y, g_ImgErrorIcon, smcxsmicon, smcxsmicon, 0, NULL, DI_COMPAT | DI_NORMAL);
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
	bmi.bmiHeader.biHeight = -bm.bmHeight;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = bm.bmBitsPixel;
	bmi.bmiHeader.biCompression = BI_RGB;
	bmi.bmiHeader.biSizeImage = 0;

	pBytes = new BYTE[bm.bmWidth * bm.bmHeight * (bm.bmBitsPixel / 8)];

	if (!GetDIBits(hdc, hbm, 0, bm.bmHeight, pBytes, &bmi, DIB_RGB_COLORS)) {
		DbgPrintW("Error, can't get DI bits for bitmap!");
		delete[] pBytes;
		return false;
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

int HandleGestureMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, float mulDeltas)
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
