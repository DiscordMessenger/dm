#include "ImageViewer.hpp"
#include "ImageLoader.hpp"
#include "Main.hpp"
#include "NetworkerThread.hpp"

#define DM_IMAGE_VIEWER_CLASS       TEXT("DMImageViewerClass")
#define DM_IMAGE_VIEWER_CHILD_CLASS TEXT("DMImageViewerChildClass")

#define EDGE_SIZE          ScaleByDPI(4)
#define IBOTTOM_BAR_HEIGHT ScaleByDPI(36)
#define HORZ_SCROLL_BAR_ID (1)
#define VERT_SCROLL_BAR_ID (2)
#define SAVE_BUTTON_ID     (3)

static HWND g_ivHwnd;
static std::string g_ProxyURL, g_ActualURL, g_FileName;
static int g_ivWidth, g_ivHeight;
static int g_ivPreviewWidth, g_ivPreviewHeight;
static int g_ivPreviewImageWidth, g_ivPreviewImageHeight;
static bool g_ivbHaveScrollBars;
static HWND g_ivChildHwnd, g_ivButtonHwnd;
static HBITMAP g_hBitmapPreview, g_hBitmapFull;
static HCURSOR g_curZoomIn, g_curZoomOut;

void KillImageBitmaps()
{
	if (g_hBitmapFull)    DeleteObject(g_hBitmapFull);
	if (g_hBitmapPreview) DeleteObject(g_hBitmapPreview);
	g_hBitmapFull = g_hBitmapPreview = NULL;
}

static bool g_bChildZoomedIn = false;

static HBITMAP GetBitmap()
{
	if (g_bChildZoomedIn)
		return g_hBitmapFull;
	else
		return g_hBitmapPreview;
}

static void UpdateScrollInfoPos(LPSCROLLINFO si, WPARAM wParam, int yChar)
{
	switch (LOWORD(wParam))
	{
		case SB_THUMBTRACK:
			si->nPos = si->nTrackPos;
			break;
		case SB_LINEUP:
			si->nPos -= yChar;
			break;
		case SB_LINEDOWN:
			si->nPos += yChar;
			break;
		case SB_PAGEUP:
			si->nPos -= si->nPage;
			break;
		case SB_PAGEDOWN:
			si->nPos += si->nPage;
			break;
		case SB_TOP:
			si->nPos = si->nMin;
			break;
		case SB_BOTTOM:
			si->nPos = si->nMax;
			break;
	}
}

static bool ImageViewerNeedScrollBarsAtAll(int winWidth, int winHeight)
{
	return (g_ivWidth != winWidth || g_ivHeight != winHeight);
}

static bool ImageViewerNeedScrollBars(int winWidth, int winHeight)
{
	return g_bChildZoomedIn && (g_ivWidth != winWidth || g_ivHeight != winHeight);
}

LRESULT CALLBACK ImageViewerChildWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	SCROLLINFO si{};
	si.cbSize = sizeof(si);
	int tph = 0, tpv = 0;
	bool useTPVals = false;

	switch (uMsg)
	{
		case WM_LBUTTONDOWN:
		{
			SetFocus(hWnd);
			RECT rect = {};
			GetClientRect(hWnd, &rect);
			int winWidth = rect.right - rect.left;
			int winHeight = rect.bottom - rect.top;
			if (ImageViewerNeedScrollBarsAtAll(winWidth, winHeight))
			{
				g_bChildZoomedIn ^= 1;
				SetClassLongPtr(hWnd, GCLP_HCURSOR, (LONG_PTR)(g_bChildZoomedIn ? g_curZoomOut : g_curZoomIn));
				SendMessage(hWnd, WM_UPDATEBITMAP, 0, 0);
			}
			break;
		}
		case WM_UPDATEBITMAP:
		{
			HBITMAP hbm = GetBitmap();
			if (!hbm)
				return 0;

			BITMAP bm;
			memset(&bm, 0, sizeof(bm));
			if (!GetObject(hbm, sizeof(BITMAP), &bm)) {
				MessageBox(hWnd, TEXT("Could not show bitmap!  Windows API mismatch?"), TmGetTString(IDS_PROGRAM_NAME), MB_ICONERROR);
				break;
			}

			bool bzi = g_bChildZoomedIn;

			int prevWidth  = abs(bm.bmWidth);
			int prevHeight = abs(bm.bmHeight);

			RECT rect = {};
			GetClientRect(hWnd, &rect);
			int winWidth  = rect.right - rect.left;
			int winHeight = rect.bottom - rect.top;

			if (!ImageViewerNeedScrollBars(winWidth, winHeight))
			{
				bzi = false;
				winWidth   = g_ivPreviewWidth;
				winHeight  = g_ivPreviewHeight;
				prevWidth  = g_ivPreviewWidth;
				prevHeight = g_ivPreviewHeight;
			}
			else
			{
				prevWidth  += GetSystemMetrics(SM_CXVSCROLL);
				prevHeight += GetSystemMetrics(SM_CYHSCROLL);
			}

			si.fMask = SIF_ALL;

			// update the vertical one
			GetScrollInfo(hWnd, SBS_VERT, &si);
			si.nPos = 0;
			si.nMin = 0;
			si.nMax = bzi ? prevHeight : 0;
			si.nPage = bzi ? (winHeight * si.nMax / prevHeight) : 0;
			si.nTrackPos = 0;
			SetScrollInfo(hWnd, SBS_VERT, &si, true);

			// update the horizontal one
			GetScrollInfo(hWnd, SBS_HORZ, &si);
			si.nPos = 0;
			si.nMin = 0;
			si.nMax = bzi ? prevWidth : 0;
			si.nPage = bzi ? (winWidth * si.nMax / prevWidth) : 0;
			si.nTrackPos = 0;
			SetScrollInfo(hWnd, SBS_HORZ, &si, true);

			InvalidateRect(hWnd, NULL, false);
			break;
		}
		case WM_MOUSEWHEEL:
		case WM_MOUSEHWHEEL:
		{
			// insane amounts of hackery
			RECT rect = {};
			GetClientRect(hWnd, &rect);
			int winWidth = rect.right - rect.left;
			int winHeight = rect.bottom - rect.top;

			if (!ImageViewerNeedScrollBars(winWidth, winHeight))
				break;

			short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
			bool bIsHorz = uMsg == WM_MOUSEHWHEEL;
			int id = bIsHorz ? SBS_HORZ : SBS_VERT;

			if (bIsHorz)
				zDelta = -zDelta;

			si.fMask = SIF_ALL;
			GetScrollInfo(hWnd, id, &si);
			si.nTrackPos = si.nPos - (zDelta / 3);
			if (si.nTrackPos < si.nMin) si.nTrackPos = si.nMin;
			if (si.nTrackPos > si.nMax) si.nTrackPos = si.nMax;
			SetScrollInfo(hWnd, id, &si, false);

			if (bIsHorz)
				tph = si.nTrackPos;
			else
				tpv = si.nTrackPos;

			useTPVals = true;

			wParam = SB_THUMBTRACK;
			uMsg = bIsHorz ? WM_HSCROLL : WM_VSCROLL;
			goto _lblFallthru;
		}
		case WM_HSCROLL:
		case WM_VSCROLL:
		{
		_lblFallthru:
			TEXTMETRIC tm;
			HDC hdc = GetDC(hWnd);
			GetTextMetrics(hdc, &tm);
			ReleaseDC(hWnd, hdc);
			int yChar = tm.tmHeight + tm.tmExternalLeading;

			si.fMask = SIF_ALL;

			GetScrollInfo(hWnd, SBS_HORZ, &si);
			if (useTPVals) si.nTrackPos = tph;
			int oldXPos = si.nPos;
			if (uMsg == WM_HSCROLL) {
				UpdateScrollInfoPos(&si, wParam, yChar);
				SetScrollInfo(hWnd, SBS_HORZ, &si, true);
				GetScrollInfo(hWnd, SBS_HORZ, &si);
			}
			int newXPos = si.nPos;

			GetScrollInfo(hWnd, SBS_VERT, &si);
			if (useTPVals) si.nTrackPos = tpv;
			int oldYPos = si.nPos;
			if (uMsg == WM_VSCROLL) {
				UpdateScrollInfoPos(&si, wParam, yChar);
				SetScrollInfo(hWnd, SBS_VERT, &si, true);
				GetScrollInfo(hWnd, SBS_VERT, &si);
			}
			int newYPos = si.nPos;

			int diffUD = newYPos - oldYPos;
			int diffLR = newXPos - oldXPos;

			WindowScrollXY(hWnd, diffLR, diffUD);
			break;
		}
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);

			HBITMAP hbm = GetBitmap();
			if (!hbm) {
				COLORREF oldBk = SetBkColor(hdc, GetSysColor (COLOR_3DDKSHADOW));
				COLORREF oldText = SetTextColor(hdc, RGB(255, 255, 255));
				HGDIOBJ oldObject = SelectObject(hdc, g_MessageTextFont);

				LPCTSTR str = TmGetTString(IDS_PLEASE_WAIT);
				TextOut(hdc, 10, 10, str, _tcslen(str));

				SelectObject(hdc, oldObject);
				SetBkColor(hdc, oldBk);
				SetTextColor(hdc, oldText);
				EndPaint(hWnd, &ps);
				break;
			}

			int xOffs, yOffs;
			si.fMask = SIF_POS;
			GetScrollInfo(hWnd, SBS_VERT, &si);
			yOffs = si.nPos;
			GetScrollInfo(hWnd, SBS_HORZ, &si);
			xOffs = si.nPos;

			if (!g_bChildZoomedIn)
			{
				// If the image is smaller than the fitting box:
				if (g_ivPreviewImageWidth  < g_ivPreviewWidth ||
					g_ivPreviewImageHeight < g_ivPreviewHeight)
				{
					xOffs = -(g_ivPreviewWidth  - g_ivPreviewImageWidth)  / 2;
					yOffs = -(g_ivPreviewHeight - g_ivPreviewImageHeight) / 2;
				}
			}

			// draw the image
			DrawBitmap(hdc, hbm, -xOffs, -yOffs, NULL);

			EndPaint(hWnd, &ps);
			break;
		}
		case WM_DESTROY:
		{
			KillImageBitmaps();
			g_ivChildHwnd = g_ivHwnd = NULL;
			break;
		}
		case WM_CREATE:
		{
			RECT rect = {};
			GetClientRect(hWnd, &rect);
			int winWidth = rect.right - rect.left;
			int winHeight = rect.bottom - rect.top;
			if (ImageViewerNeedScrollBarsAtAll(winWidth, winHeight))
				SetClassLongPtr(hWnd, GCLP_HCURSOR, (LONG_PTR)g_curZoomIn);
			else
				SetClassLongPtr(hWnd, GCLP_HCURSOR, (LONG_PTR)LoadCursor(NULL, IDC_ARROW));
		}
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void ImageViewerOnLoad(NetRequest* pRequest)
{
	if (pRequest->result != HTTP_OK)
	{
		DownloadOnRequestFail(g_ivHwnd, pRequest);
		KillImageViewer();
		return;
	}

	// Since the response is passed in as a string, this could do for getting the binary stuff out of it
	const uint8_t* pData = (const uint8_t*)pRequest->response.data();
	const size_t nSize = pRequest->response.size();

	KillImageBitmaps();

	g_hBitmapFull = ImageLoader::ConvertToBitmap(pData, nSize, 0, 0);
	g_hBitmapPreview = ImageLoader::ConvertToBitmap(pData, nSize, g_ivPreviewImageWidth, g_ivPreviewImageHeight);

	g_bChildZoomedIn = false;
	SendMessage(g_ivChildHwnd, WM_UPDATEBITMAP, 0, 0);
}

void ImageViewerRequestSave()
{
	DownloadFileDialog(g_ivHwnd, g_ActualURL, g_FileName);
}

void ImageViewerOnLoadNT(NetRequest* pRequest)
{
	SendMessage(g_ivHwnd, WM_REQUESTDONE, 0, (LPARAM)pRequest);
}

void ImageViewerDownloadResponseNT(NetRequest* pRequest)
{
	SendMessage(g_ivHwnd, WM_REQUESTDONE, 1, (LPARAM)pRequest);
}

UINT_PTR g_ivTimerId;

void CALLBACK ImageViewerSavedTimerExpiry(HWND hWnd, UINT, UINT_PTR, DWORD)
{
	KillTimer(hWnd, g_ivTimerId);
	SendMessage(g_ivHwnd, WM_IMAGECLEARSAVE, 0, 0);
}

LRESULT CALLBACK ImageViewerWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_REQUESTDONE:
		{
			ImageViewerOnLoad((NetRequest*) lParam);
			return 0;
		}
		case WM_IMAGECLEARSAVE: {
			SendMessage(g_ivButtonHwnd, WM_SETTEXT, 0, (LPARAM)TmGetTString(IDS_SAVE_IMAGE));
			break;
		}
		case WM_IMAGESAVING: {
			SendMessage(g_ivButtonHwnd, WM_SETTEXT, 0, (LPARAM)TmGetTString(IDS_IMAGE_SAVING));
			break;
		}
		case WM_IMAGESAVED: {
			SendMessage(g_ivButtonHwnd, WM_SETTEXT, 0, (LPARAM)TmGetTString(IDS_IMAGE_SAVED));
			g_ivTimerId = SetTimer(g_ivHwnd, 0, 2000, ImageViewerSavedTimerExpiry);
			break;
		}
		case WM_DESTROY: {
			KillTimer(hWnd, g_ivTimerId);
			break;
		}
		case WM_COMMAND:
		{
			if (LOWORD(wParam) == SAVE_BUTTON_ID)
				ImageViewerRequestSave();

			break;
		}
		case WM_CREATE:
		{
			// Request the attachment
			GetHTTPClient()->PerformRequest(
				true,
				NetRequest::GET,
				g_ProxyURL,
				0,
				0,
				"",
				"",
				"",
				ImageViewerOnLoadNT
			);
			break;
		}
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void KillImageViewer()
{
	if (!g_ivHwnd)
		return;

	DestroyWindow(g_ivHwnd);
	g_ivHwnd = NULL;
	g_ivChildHwnd = NULL;
}

void ImageViewerFinishSaveIfNeeded()
{
	if (g_ivHwnd)
		SendMessage(g_ivHwnd, WM_IMAGESAVED, 0, 0);
}

void ImageViewerClearSaveIfNeeded()
{
	if (g_ivHwnd)
		SendMessage(g_ivHwnd, WM_IMAGECLEARSAVE, 0, 0);
}

bool RegisterImageViewerClass()
{
	g_curZoomIn  = LoadCursor(g_hInstance, MAKEINTRESOURCE(IDC_ZOOMIN));
	g_curZoomOut = LoadCursor(g_hInstance, MAKEINTRESOURCE(IDC_ZOOMOUT));

	WNDCLASS wc;
	memset(&wc, 0, sizeof wc);
	WNDCLASS wc2;
	memset(&wc2, 0, sizeof wc2);

	wc.lpfnWndProc   = ImageViewerWndProc;
	wc.hInstance     = g_hInstance;
	wc.lpszClassName = DM_IMAGE_VIEWER_CLASS;
	wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon         = g_Icon = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_ICON));

	if (!RegisterClass(&wc))
		return false;

	wc2.lpfnWndProc   = ImageViewerChildWndProc;
	wc2.hInstance     = g_hInstance;
	wc2.lpszClassName = DM_IMAGE_VIEWER_CHILD_CLASS;
	wc2.hbrBackground = GetSysColorBrush(COLOR_3DDKSHADOW);
	wc2.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc2.hIcon         = g_Icon = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_ICON));

	return RegisterClass(&wc2) != 0;
}

void CreateImageViewer(const std::string& proxyURL, const std::string& url, const std::string& fileName, int width, int height)
{
	// Kill it if present:
	KillImageViewer();

	g_ProxyURL  = proxyURL;
	g_FileName  = fileName;
	g_ActualURL = url;

	LPCTSTR tstr = ConvertCppStringToTString(fileName);

	int scrollBarWidth  = GetSystemMetrics(SM_CXVSCROLL);
	int scrollBarHeight = GetSystemMetrics(SM_CYHSCROLL);
	int wWidth  = width;
	int wHeight = height;
	int screenWidth  = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);
	int maxWidth  = screenWidth  - 200;
	int maxHeight = screenHeight - 200 - IBOTTOM_BAR_HEIGHT;

	g_ivWidth = width;
	g_ivHeight = height;

	g_bChildZoomedIn = true;

	int iWidth  = wWidth;
	int iHeight = wHeight;

	// Ensure aspect ratio preservation while keeping scale down
	if (wWidth > maxWidth)
	{
		iHeight = iHeight * maxWidth / iWidth;
		iWidth = maxWidth;
		wWidth = maxWidth;
	}

	if (wHeight > maxHeight)
	{
		iWidth = iWidth * maxHeight / iHeight;
		wHeight = maxHeight;
		iHeight = maxHeight;
	}

	bool addScrollBars = wWidth != width || wHeight != height;

	g_ivPreviewWidth  = wWidth;
	g_ivPreviewHeight = wHeight;

	g_ivPreviewImageWidth  = iWidth;
	g_ivPreviewImageHeight = iHeight;

	RECT rc = { 0, 0, wWidth, wHeight + IBOTTOM_BAR_HEIGHT };

	g_ivbHaveScrollBars = addScrollBars;

	DWORD dwStyle = WS_SYSMENU | WS_CAPTION;
	AdjustWindowRect(&rc, dwStyle, false);

	int winWidth  = rc.right - rc.left + EDGE_SIZE;
	int winHeight = rc.bottom - rc.top + EDGE_SIZE;

	RECT parentRect = { 0, 0, 0, 0 };
	GetWindowRect(g_Hwnd, &parentRect);

	int scaled5 = ScaleByDPI(5);
	int scaled10 = ScaleByDPI(10);

	int xPos = (parentRect.left + parentRect.right) / 2 - winWidth / 2;
	int yPos = (parentRect.top + parentRect.bottom) / 2 - winHeight / 2;

	if (xPos < 0)
		xPos = 0;
	if (yPos < 0)
		yPos = 0;
	if (xPos > screenWidth - winWidth)
		xPos = screenWidth - winWidth;
	if (yPos > screenHeight - winHeight)
		yPos = screenHeight - winHeight;

	// Actually create the window:
	g_ivHwnd = CreateWindow(
		TEXT("DMImageViewerClass"),
		tstr,
		dwStyle,
		xPos,
		yPos,
		winWidth,
		winHeight,
		NULL,
		NULL,
		g_hInstance,
		NULL
	);

	if (!g_ivHwnd) {
		DbgPrintW("Image viewer window could not be created");
		return;
	}

	RECT rcWindow;
	GetClientRect(g_ivHwnd, &rcWindow);
	rcWindow.bottom -= IBOTTOM_BAR_HEIGHT;
	OffsetRect(&rcWindow, -rcWindow.left, -rcWindow.top);

	g_ivChildHwnd = CreateWindowEx(
		WS_EX_CLIENTEDGE,
		DM_IMAGE_VIEWER_CHILD_CLASS,
		NULL,
		WS_CHILD | WS_VISIBLE | (addScrollBars ? (WS_HSCROLL | WS_VSCROLL) : 0),
		(rcWindow.right  - wWidth  - EDGE_SIZE) / 2,
		(rcWindow.bottom - wHeight - EDGE_SIZE) / 2,
		wWidth  + EDGE_SIZE,
		wHeight + EDGE_SIZE,
		g_ivHwnd,
		0,
		g_hInstance,
		NULL
	);

	LPCTSTR tSaveImageAs = TmGetTString(IDS_SAVE_IMAGE);

	HDC hdc = GetDC(g_ivHwnd);
	HGDIOBJ crap = SelectObject(hdc, g_MessageTextFont);
	RECT rcSave = { 0, 0, 0, 0 };
	DrawText(hdc, tSaveImageAs, -1, &rcSave, DT_CALCRECT);
	LONG widSave = rcSave.right - rcSave.left + scaled10;
	SelectObject(hdc, crap);
	ReleaseDC(g_ivHwnd, hdc);

	g_ivButtonHwnd = CreateWindow(
		TEXT("BUTTON"),
		tSaveImageAs,
		WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
		scaled5, wHeight + EDGE_SIZE + scaled5,
		std::min( rcWindow.right - scaled10, widSave ),
		IBOTTOM_BAR_HEIGHT - scaled10,
		g_ivHwnd,
		(HMENU) SAVE_BUTTON_ID,
		g_hInstance,
		NULL
	);

	SendMessage(g_ivButtonHwnd, WM_SETFONT, (WPARAM)g_MessageTextFont, 0);

	if (!g_ivChildHwnd || !g_ivButtonHwnd) {
		DbgPrintW("Image viewer child window could not be created");
		KillImageViewer();
		return;
	}

	ShowWindow(g_ivHwnd, SW_SHOW);
	UpdateWindow(g_ivHwnd);
}
