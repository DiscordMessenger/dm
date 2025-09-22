#include "Config.hpp"
#include "Main.hpp"
#include "IChannelView.hpp"
#include "IMemberList.hpp"
#include "AboutDialog.hpp"
#include "OptionsDialog.hpp"
#include "MessageList.hpp"
#include "ProfileView.hpp"
#include "MessageEditor.hpp"
#include "GuildHeader.hpp"
#include "GuildLister.hpp"
#include "TextToSpeech.hpp"
#include "NetworkerThread.hpp"
#include "ImageLoader.hpp"
#include "ProfilePopout.hpp"
#include "ImageViewer.hpp"
#include "PinList.hpp"
#include "LogonDialog.hpp"
#include "QRCodeDialog.hpp"
#include "LoadingMessage.hpp"
#include "UploadDialog.hpp"
#include "StatusBar.hpp"
#include "QuickSwitcher.hpp"
#include "Frontend_Win32.hpp"
#include "ProgressDialog.hpp"
#include "AutoComplete.hpp"
#include "ShellNotification.hpp"
#include "InstanceMutex.hpp"
#include "CrashDebugger.hpp"
#include "../discord/LocalSettings.hpp"
#include "../discord/WebsocketClient.hpp"
#include "../discord/UpdateChecker.hpp"

#include "MainWindow.hpp"

#include <system_error>
#include <shellapi.h>

// proportions:
int g_ProfilePictureSize;

int GetProfilePictureSize()
{
	return g_ProfilePictureSize;
}

void FindBasePath()
{
	TCHAR pwStr[MAX_PATH];
	pwStr[0] = 0;
	LPCTSTR p1 = NULL, p2 = NULL;
	
	if (SUCCEEDED(ri::SHGetFolderPath(GetMainHWND(), CSIDL_APPDATA, NULL, 0, pwStr)))
	{
		SetBasePath(MakeStringFromTString(pwStr));
	}
	else
	{
		MessageBox(GetMainHWND(), TmGetTString(IDS_NO_APPDATA), TmGetTString(IDS_NON_CRITICAL_ERROR), MB_OK | MB_ICONERROR);
		SetBasePath(".");
	}
}

bool TryThisBasePath()
{
	LPCTSTR p1 = NULL, p2 = NULL;
	p1 = ConvertCppStringToTString(GetBasePath());
	p2 = ConvertCppStringToTString(GetCachePath());

	bool result = true;
	
	// if these already exist, it's fine..
	if (!CreateDirectory(p1, NULL) || !CreateDirectory(p2, NULL))
	{
		if (GetLastError() != ERROR_ALREADY_EXISTS)
			result = false;
	}

	if (p1) free((void*)p1);
	if (p2) free((void*)p2);

	return result;
}

void SetupCachePathIfNeeded()
{
	SetProgramNamePath("DiscordMessenger");
	FindBasePath();

	if (TryThisBasePath())
		return;
	
	// Ok, try the SFN version
	SetProgramNamePath("Discordm");
	if (TryThisBasePath())
		return;

	MessageBox(GetMainHWND(), TmGetTString(IDS_NO_CACHE_DIR), TmGetTString(IDS_NON_CRITICAL_ERROR), MB_OK | MB_ICONERROR);
	SetBasePath(".");
}

HINSTANCE g_hInstance;
WNDCLASS g_MainWindowClass;
HBRUSH   g_backgroundBrush;
HICON    g_Icon;
HICON    g_BotIcon;
HICON    g_SendIcon;
HICON    g_JumpIcon;
HICON    g_CancelIcon;
HICON    g_UploadIcon;
HICON    g_DownloadIcon;
HICON    g_ProfileBorderIcon;
HICON    g_ProfileBorderIconGold;
HFONT
	g_MessageTextFont,
	g_AuthorTextFont,
	g_AuthorTextUnderlinedFont,
	g_DateTextFont,
	g_GuildCaptionFont,
	g_GuildSubtitleFont,
	g_GuildInfoFont,
	g_AccountInfoFont,
	g_AccountTagFont,
	g_SendButtonFont,
	g_ReplyTextFont,
	g_ReplyTextBoldFont,
	g_TypingRegFont,
	g_TypingBoldFont;

// icons grabbed from shell32
HICON g_folderClosedIcon;
HICON g_folderOpenIcon;

HBITMAP g_DefaultProfilePicture;
HImage g_defaultImage;

HBITMAP GetDefaultBitmap() {
	return g_DefaultProfilePicture;
}
HImage* GetDefaultImage() {
	return &g_defaultImage;
}

void WantQuit()
{
	GetHTTPClient()->PrepareQuit();
}

bool g_bQuittingEarly = false;

INT_PTR CALLBACK DDialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	return 0L;
}

const CHAR g_StartupArg[] = "/startup";

bool g_bFromStartup = false;

void CheckIfItsStartup(const LPSTR pCmdLine)
{
	g_bFromStartup = strstr(pCmdLine, g_StartupArg);
}

bool IsFromStartup() {
	return g_bFromStartup;
}

HFONT
	g_FntMd,
	g_FntMdB,
	g_FntMdI,
	g_FntMdBI,
	g_FntMdU,
	g_FntMdBU,
	g_FntMdIU,
	g_FntMdBIU,
	g_FntMdCode,
	g_FntMdHdr,
	g_FntMdHdr2,
	g_FntMdHdrI,
	g_FntMdHdrI2;

HFONT* g_FntMdStyleArray[FONT_TYPE_COUNT] = {
	&g_FntMd,   // 0
	&g_FntMdB,  // 1
	&g_FntMdI,  // 2
	&g_FntMdBI, // 2|1
	&g_FntMdU,  // 4
	&g_FntMdBU, // 4|1
	&g_FntMdIU, // 4|2
	&g_FntMdBIU,// 4|2|1
	&g_FntMdCode, // 8
	&g_FntMdHdr,  // 9
	&g_FntMdHdrI, // 10
	&g_FntMdHdr2, // 11
	&g_FntMdHdrI2,// 12
	&g_ReplyTextFont, // 13
};

void InitializeFonts()
{
	LOGFONT lf{};
	bool haveFont = SystemParametersInfo(SPI_GETICONTITLELOGFONT, sizeof lf, &lf, 0);
	lf.lfItalic = false;
	lf.lfUnderline = false;
	auto oldWeight = lf.lfWeight;

	lf.lfHeight = ScaleByUser(lf.lfHeight);

	HFONT hf, hfb, hfi, hfbi, hfu, hfbu, hfiu, hfbiu, hfbh, hfbh2, hfbih, hfbih2;
	hf = hfb = hfi = hfbi = hfu = hfbu = hfiu = hfbiu = hfbh = hfbh2 = hfbih = hfbih2 = NULL;

	if (haveFont)
		hf = CreateFontIndirect(&lf);

	if (!hf) {
		hf = GetStockFont(SYSTEM_FONT);
	}

	if (haveFont) {
		int h1 = -MulDiv(lf.lfHeight, 8, 3);
		int h2 = MulDiv(h1, 4, 5);

		if (LOBYTE(GetVersion()) <= 0x4)
		{
			h1 = MulDiv(lf.lfHeight, 6, 4);
			h2 = MulDiv(lf.lfHeight, 5, 4);
		}

		// BOLD
		lf.lfWeight = 700;
		hfb = CreateFontIndirect(&lf);

		// BOLD h1
		int oldh = lf.lfHeight;
		lf.lfHeight = h1;
		hfbh = CreateFontIndirect(&lf);

		// BOLD h2
		lf.lfHeight = h2;
		hfbh2 = CreateFontIndirect(&lf);
		lf.lfHeight = oldh;

		// BOLD ITALIC
		lf.lfItalic = true;
		hfbi = CreateFontIndirect(&lf);
		
		// BOLD ITALIC h1
		lf.lfHeight = h1;
		hfbih = CreateFontIndirect(&lf);

		// BOLD ITALIC h2
		lf.lfHeight = h2;
		hfbih2 = CreateFontIndirect(&lf);
		lf.lfHeight = oldh;

		// BOLD ITALIC UNDERLINE
		lf.lfUnderline = true;
		hfbiu = CreateFontIndirect(&lf);

		// BOLD UNDERLINE
		lf.lfItalic = false;
		hfbu = CreateFontIndirect(&lf);

		// UNDERLINE
		lf.lfWeight = oldWeight;
		hfu = CreateFontIndirect(&lf);

		// ITALIC UNDERLINE
		lf.lfItalic = true;
		hfiu = CreateFontIndirect(&lf);

		// ITALIC
		lf.lfUnderline = false;
		hfi = CreateFontIndirect(&lf);
		// lf.lfItalic = false; -- yes I want it italic
		int oldSize = lf.lfHeight;
		lf.lfHeight = MulDiv(lf.lfHeight, 8, 10);
		g_ReplyTextFont = CreateFontIndirect(&lf);
		lf.lfHeight = oldSize;
	}

	if (!hfb)    hfb = hf;
	if (!hfi)    hfi = hf;
	if (!hfbi)   hfbi = hf;
	if (!hfb)    hfb = hf;
	if (!hfbu)   hfbu = hf;
	if (!hfiu)   hfiu = hf;
	if (!hfbiu)  hfbiu = hf;
	if (!hfbh)   hfbh = hf;
	if (!hfbh2)  hfbh2 = hf;
	if (!hfbih)  hfbih = hf;
	if (!hfbih2) hfbih2 = hf;

	g_FntMd = hf;
	g_FntMdB = hfb;
	g_FntMdI = hfi;
	g_FntMdU = hfu;
	g_FntMdBI = hfbi;
	g_FntMdBU = hfbu;
	g_FntMdIU = hfiu;
	g_FntMdBIU = hfbiu;
	g_FntMdHdr = hfbh;
	g_FntMdHdr2 = hfbh2;
	g_FntMdHdrI = hfbih;
	g_FntMdHdrI2 = hfbih2;

	g_MessageTextFont = hf;
	g_AuthorTextFont = hfb;
	g_DateTextFont = hfi;
	g_GuildCaptionFont = hfb;
	g_GuildSubtitleFont = hf;
	g_GuildInfoFont = hfi;
	g_AccountInfoFont = hfb;
	g_AccountTagFont = hf;
	g_SendButtonFont = hf;
	g_AuthorTextUnderlinedFont = hfbu;
	g_TypingRegFont = hfi;
	g_TypingBoldFont = hfbi;

	// monospace font
	LOGFONT lfMono{};
	if (!GetObject(GetStockObject(ANSI_FIXED_FONT), sizeof(LOGFONT), &lfMono)) {
		// fallback
		g_FntMdCode = GetStockFont(ANSI_FIXED_FONT);
	}
	else
	{
		// copy all properties except the face name
		_tcscpy(lfMono.lfFaceName, TEXT("Courier New"));
		lfMono.lfWidth = 0;
		lfMono.lfHeight = ScaleByUser(12 * GetSystemDPI() / 72);

		g_FntMdCode = CreateFontIndirect(&lfMono);
		if (!g_FntMdCode) {
			g_FntMdCode = GetStockFont(ANSI_FIXED_FONT);
		}
	}
}

Frontend_Win32* g_pFrontEnd;
NetworkerThreadManager* g_pHTTPClient;

Frontend* GetFrontend()
{
	return g_pFrontEnd;
}

HTTPClient* GetHTTPClient()
{
	return g_pHTTPClient;
}

InstanceMutex g_instanceMutex;

static bool ForceSingleInstance(LPCTSTR pClassName)
{
	HRESULT hResult = g_instanceMutex.Init();

	if (hResult != ERROR_ALREADY_EXISTS)
		return true;

	HWND hWnd = FindWindow(pClassName, NULL);
	if (hWnd)
	{
		DWORD_PTR dwResult = 0; // ignored
		if (SendMessageTimeout(hWnd, WM_RESTOREAPP, 0, 0, SMTO_BLOCK | SMTO_ABORTIFHUNG | SMTO_ERRORONEXIT, 3000, &dwResult))
			// The message was received by the app to restore its window.
			return false;

		// Ok, so we probably have a hung process.
		// Instruct them to close or terminate the hung process.
		MessageBox(NULL, TmGetTString(IDS_TERMINATE_HUNG_PROCESS), TmGetTString(IDS_PROGRAM_NAME), MB_ICONWARNING | MB_OK);
		return false;
	}
	return false;
}

#include "MemberListOld.hpp"

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nShowCmd)
{
	MSG msg = { };
	g_hInstance = hInstance;

	PrepareCutDownFlags(pCmdLine);

	ri::InitReimplementation();
	ri::SetProcessDPIAware();

#ifdef ALLOW_ABORT_HIJACKING
	SetupCrashDebugging();
#endif

	ERR_load_crypto_strings();
	LPCTSTR pClassName = TEXT("DiscordMessengerClass");

	InitializeCOM(); // important because otherwise TTS/shell stuff might not work
	InitCommonControls(); // actually a dummy but adds the needed reference to comctl32
	// (see https://devblogs.microsoft.com/oldnewthing/20050718-16/?p=34913 )

	if (!ForceSingleInstance(pClassName))
		return 0;

	CheckIfItsStartup(pCmdLine);

	g_pFrontEnd = new Frontend_Win32;
	g_pHTTPClient = new NetworkerThreadManager;

	// Create a background brush.
	g_backgroundBrush = ri::GetSysColorBrush(COLOR_3DFACE);

	SetupCachePathIfNeeded();

	LocalSettings* pSettings = GetLocalSettings();
	DWORD version = GetVersion();

	// Load the settings, and fix up some settings that are
	// actually bad to leave on on NT 3.x or NT 4
	pSettings->Load();

	if (LOBYTE(version) < 5 && pSettings->GetMessageStyle() == MS_GRADIENT)
	{
		// You probably shouldn't be using the Gradient
		// theme by default because it looks like crap.
		// That's why I disabled it.
		pSettings->SetMessageStyle(MS_3DFACE);
	}

	if (LOBYTE(version) < 4)
	{
		// You definitely shouldn't minimize yourself to the
		// taskbar because there is no taskbar to minimize
		// yourself to.
		pSettings->SetMinimizeToNotif(false);
	}

	SetUserScale(GetLocalSettings()->GetUserScale());

	// NOTE: Despite that we pass LR_SHARED, if this "isn't a standard size" (whatever Microsoft means), we must still delete it!!
	g_DefaultProfilePicture = (HBITMAP)ri::LoadImage(hInstance, MAKEINTRESOURCE(IDB_DEFAULT),                   IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION | LR_SHARED);
	g_ProfileBorderIcon     = (HICON)  ri::LoadImage(hInstance, MAKEINTRESOURCE(DMIC(IDI_PROFILE_BORDER)),      IMAGE_ICON,   0, 0, LR_CREATEDIBSECTION | LR_SHARED);
	g_ProfileBorderIconGold = (HICON)  ri::LoadImage(hInstance, MAKEINTRESOURCE(DMIC(IDI_PROFILE_BORDER_GOLD)), IMAGE_ICON,   0, 0, LR_CREATEDIBSECTION | LR_SHARED);

	g_defaultImage.Frames.resize(1);
	g_defaultImage.Frames[0].Bitmap = g_DefaultProfilePicture;
	g_defaultImage.Width = g_defaultImage.Height = GetProfilePictureSize();

	// Find the folder icons within shell32.
	HMODULE hshell32 = GetModuleHandle(TEXT("shell32.dll"));
	if (hshell32) {
		g_folderClosedIcon = LoadIcon(hshell32, MAKEINTRESOURCE(4));
		g_folderOpenIcon   = LoadIcon(hshell32, MAKEINTRESOURCE(5));
	}

	if (!g_folderClosedIcon) g_folderClosedIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_FOLDER));
	if (!g_folderOpenIcon)   g_folderOpenIcon   = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_FOLDER));

	InitializeStatusIcons();

	if (!RegisterImageViewerClass())
		return 1;

	HACCEL hAccTable = LoadAccelerators(g_hInstance, MAKEINTRESOURCE(IDR_MAIN_ACCELS));

	// Initialize the HTTP client and websocket client
	GetHTTPClient()->Init();

	try
	{
		GetWebsocketClient()->Init();
	}
	catch (websocketpp::exception ex)
	{
		Terminate("hey, websocketpp excepted: %s | %d | %s", ex.what(), ex.code(), ex.m_msg.c_str());
	}
	catch (std::exception ex)
	{
		Terminate("hey, websocketpp excepted (generic std::exception): %s", ex.what());
	}
	catch (...)
	{
		MessageBox(NULL, TmGetTString(IDS_CANNOT_INIT_WS), TmGetTString(IDS_PROGRAM_NAME), MB_ICONERROR | MB_OK);
		return 1;
	}

	// Initialize DPI
	ForgetSystemDPI();
	g_ProfilePictureSize = ScaleByDPI(PROFILE_PICTURE_SIZE_DEF);

	// Create some fonts.
	InitializeFonts();

	TextToSpeech::Initialize();

	// Create the main window;
	MainWindow mainWindow(TEXT("DiscordMessengerClass"), nShowCmd);
	if (!mainWindow.InitFailed())
	{
		// Run the message loop.
		while (GetMessage(&msg, NULL, 0, 0) > 0)
		{
			//
			// Hack.  This inspects ALL messages, including ones delivered to children
			// of the main window.  This is fine, since that means that I won't have to
			// add code to the child windows themselves to dismiss the popout.
			//
			// Also redirect all WM_CHAR messages on other focusable windows to the message
			// editor.
			//
			switch (msg.message)
			{
				case WM_LBUTTONDOWN:
				case WM_RBUTTONDOWN:
				case WM_MBUTTONDOWN:
					// If the focus isn't sent to a child of the profile popout, dismiss the latter
					if (!IsChildOf(msg.hwnd, ProfilePopout::GetHWND()))
						ProfilePopout::Dismiss();
	
					// fallthrough
	
				case WM_WINDOWPOSCHANGED:
				case WM_MOVE:
				case WM_SIZE:
					AutoComplete::DismissAutoCompleteWindowsIfNeeded(msg.hwnd);
					break;
	
				case WM_CHAR:
					if (mainWindow.IsPartOfMainWindow(msg.hwnd))
					{
						msg.hwnd = mainWindow.GetMessageEditor()->m_edit_hwnd;
						SetFocus(msg.hwnd);
					}
					break;
			}
	
			if (IsWindow(ProfilePopout::GetHWND()) && IsDialogMessage(ProfilePopout::GetHWND(), &msg))
				continue;
	
			if (hAccTable &&
				TranslateAccelerator(mainWindow.GetHWND(), hAccTable, &msg))
				continue;
	
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	
quitEarly:
	TextToSpeech::Deinitialize();
	GetLocalSettings()->Save();
	GetWebsocketClient()->Kill();
	GetHTTPClient()->Kill();
	delete g_pFrontEnd;
	delete g_pHTTPClient;
	return (int)msg.wParam;
}

void SetHeartbeatInterval(int timeMs)
{
	GetMainWindow()->SetHeartbeatInterval(timeMs);
}
