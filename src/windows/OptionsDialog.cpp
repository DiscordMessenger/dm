#include "Config.hpp"
#include "OptionsDialog.hpp"
#include "../discord/LocalSettings.hpp"

#ifdef NEW_WINDOWS
#include <uxtheme.h>
#endif
#include <commdlg.h>

const eMessageStyle g_indexToMessageStyle[] = {
	MS_3DFACE,
	MS_GRADIENT,
	MS_FLAT,
	MS_FLATBR,
	MS_IMAGE,
};

#define C_PAGES (3)

enum ePage
{
	PG_ACCOUNT_AND_PRIVACY,
	PG_APPEARANCE,
	PG_CONNECTION,
};

#pragma pack(push, 1)
typedef struct
{
	WORD dlgVer;
	WORD signature;
	DWORD helpID;
	DWORD exStyle;
	DWORD style;
	WORD cDlgItems;
	short x;
	short y;
	short cx;
	short cy;
}
DLGTEMPLATEEX; // huh??
#pragma pack(pop)

struct DialogHeader
{
	HWND hwndTab = NULL;     // tab control
	HWND hwndDisplay = NULL; // current child dialog box
	RECT rcDisplay{};
	DLGTEMPLATEEX* apRes[C_PAGES] = { NULL };
	int  pageNum = 0;
};

void AddTab(HWND hwndTab, TCITEM& tie, int index, LPTSTR str)
{
	assert(index >= 0 && index < C_PAGES && "Huh?");

	tie.pszText = str;

	TabCtrl_InsertItem(hwndTab, index, &tie);
}

DLGTEMPLATEEX* LockDialogResource(LPCTSTR lpszResName)
{
	HRSRC hrsrc = FindResource(NULL, lpszResName, RT_DIALOG);
	HGLOBAL hglb = LoadResource(g_hInstance, hrsrc);
	return (DLGTEMPLATEEX*)LockResource(hglb);
}

#ifdef NEW_WINDOWS
// XXX: Shim to avoid linking against uxtheme.lib
HRESULT XEnableThemeDialogTexture(HWND hWnd, DWORD dwFlags)
{
	typedef HRESULT(STDAPICALLTYPE *pEnableThemeDialogTexture)(HWND, DWORD);

	static HMODULE hndl = (HMODULE)INVALID_HANDLE_VALUE;
	static pEnableThemeDialogTexture etdt;
	static bool etdtLoaded = false;

	if (!etdtLoaded)
	{
		if (hndl == INVALID_HANDLE_VALUE)
			hndl = LoadLibrary(TEXT("uxtheme.dll"));

		if (!hndl)
			return S_OK; // simulate OK

		etdt = (pEnableThemeDialogTexture)GetProcAddress(hndl, "EnableThemeDialogTexture");
		etdtLoaded = true;
	}

	if (!etdt) {
		auto err = GetLastError();
		return S_OK;
	}

	return etdt(hWnd, dwFlags);
}
#endif

void WINAPI OnChildDialogInit(HWND hwndDlg)
{
	HWND hwndParent = GetParent(hwndDlg);
	DialogHeader* pHdr = (DialogHeader*)GetWindowLongPtr(hwndParent, GWLP_USERDATA);

	SetWindowPos(hwndDlg, NULL, pHdr->rcDisplay.left,
		pHdr->rcDisplay.top,//-2,
		(pHdr->rcDisplay.right - pHdr->rcDisplay.left),
		(pHdr->rcDisplay.bottom - pHdr->rcDisplay.top),
		SWP_SHOWWINDOW);

#ifdef NEW_WINDOWS
	XEnableThemeDialogTexture(hwndDlg, ETDT_ENABLETAB);
#endif

	int pageNum = pHdr->pageNum;

	//if the page is 0 (Account & Privacy)
	switch (pageNum)
	{
		case PG_ACCOUNT_AND_PRIVACY:
		{
			Profile* pProfile = GetDiscordInstance()->GetProfile();

			HWND hwnd;

			hwnd = GetDlgItem(hwndDlg, IDC_MY_ACCOUNT_NAME);

			std::string name = pProfile->m_name;
			if (pProfile->m_discrim)
				name += "#" + FormatDiscrim(pProfile->m_discrim);

			LPCTSTR lpctstr = ConvertCppStringToTString(name);
			SetWindowText(hwnd, lpctstr);
			free((void*)lpctstr);

			hwnd = GetDlgItem(hwndDlg, IDC_STATIC_PROFILE_IMAGE);

			bool unusedHasAlpha = false;
			HBITMAP hbm = GetAvatarCache()->GetBitmap(pProfile->m_avatarlnk, unusedHasAlpha);
			SendMessage(hwnd, STM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)hbm);

			eExplicitFilter filter = GetSettingsManager()->GetExplicitFilter();
			CheckRadioButton(hwndDlg, IDC_SDM_LEVEL0, IDC_SDM_LEVEL2, IDC_SDM_LEVEL0 + int(filter));

			CheckDlgButton(hwndDlg, IDC_ENABLE_DMS, GetSettingsManager()->GetDMBlockDefault() ? BST_UNCHECKED : BST_CHECKED);
			break;
		}
		case PG_APPEARANCE:
		{
			CheckRadioButton(
				hwndDlg,
				IDC_APPEARANCE_COZY,
				IDC_APPEARANCE_COMPACT,
				GetSettingsManager()->GetMessageCompact() ? IDC_APPEARANCE_COMPACT : IDC_APPEARANCE_COZY
			);

			CheckDlgButton(hwndDlg, IDC_SAVE_WINDOW_SIZE, GetLocalSettings()->GetSaveWindowSize() ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hwndDlg, IDC_START_MAXIMIZED, GetLocalSettings()->GetStartMaximized() ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hwndDlg, IDC_DISABLE_FORMATTING, GetLocalSettings()->DisableFormatting() ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hwndDlg, IDC_SCROLL_BAR_ON_GUILDS, GetLocalSettings()->ShowScrollBarOnGuildList() ? BST_CHECKED : BST_UNCHECKED);

			LPTSTR tstr = ConvertCppStringToTString(GetLocalSettings()->GetImageBackgroundFileName());
			SetDlgItemText(hwndDlg, IDC_ACTIVE_IMAGE_EDIT, tstr);
			free(tstr);

			HWND hCBox = GetDlgItem(hwndDlg, IDC_MESSAGE_STYLE);
			// NOTE: these must match the order in g_indexToMessageStyle!
			ComboBox_AddString(hCBox, TEXT("3-D frame"));
			ComboBox_AddString(hCBox, TEXT("Gradient"));
			ComboBox_AddString(hCBox, TEXT("Flat color 1 (3-D face color)"));
			ComboBox_AddString(hCBox, TEXT("Flat color 2 (window color)"));
			ComboBox_AddString(hCBox, TEXT("Flat with image background"));

			// determine message style selection
			ComboBox_SetCurSel(hCBox, 0);

			eMessageStyle msgStyle = GetLocalSettings()->GetMessageStyle();
			for (size_t i = 0; i < _countof(g_indexToMessageStyle); i++)
			{
				if (msgStyle == g_indexToMessageStyle[i])
				{
					ComboBox_SetCurSel(hCBox, i);
					break;
				}
			}

			hCBox = GetDlgItem(hwndDlg, IDC_COMBO_ALIGNMENT);
			// NOTE: these must match the order in eImageAlignment!
			ComboBox_AddString(hCBox, TEXT("Lower right"));
			ComboBox_AddString(hCBox, TEXT("Upper left"));
			ComboBox_AddString(hCBox, TEXT("Center"));
			ComboBox_AddString(hCBox, TEXT("Upper right"));
			ComboBox_AddString(hCBox, TEXT("Lower left"));
			ComboBox_AddString(hCBox, TEXT("Upper center"));
			ComboBox_AddString(hCBox, TEXT("Lower center"));
			ComboBox_AddString(hCBox, TEXT("Middle left"));
			ComboBox_AddString(hCBox, TEXT("Middle right"));
			ComboBox_SetCurSel(hCBox, int(GetLocalSettings()->GetImageAlignment()));

			bool isWatermarkStyle = msgStyle == MS_IMAGE;
			EnableWindow(GetDlgItem(hwndDlg, IDC_ACTIVE_IMAGE_EDIT),   isWatermarkStyle);
			EnableWindow(GetDlgItem(hwndDlg, IDC_ACTIVE_IMAGE_BROWSE), isWatermarkStyle);
			EnableWindow(GetDlgItem(hwndDlg, IDC_COMBO_ALIGNMENT),     isWatermarkStyle);

			break;
		}
		case PG_CONNECTION:
		{
			LPTSTR tstrAPI = ConvertCppStringToTString(GetDiscordAPI());
			LPTSTR tstrCDN = ConvertCppStringToTString(GetDiscordCDN());

			SetDlgItemText(hwndDlg, IDC_EDIT_DISCORDAPI, tstrAPI);
			SetDlgItemText(hwndDlg, IDC_EDIT_DISCORDCDN, tstrCDN);

			CheckDlgButton(hwndDlg, IDC_ENABLE_TLS_CHECKS, GetLocalSettings()->EnableTLSVerification() ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hwndDlg, IDC_CHECK_UPDATES, GetLocalSettings()->CheckUpdates() ? BST_CHECKED : BST_UNCHECKED);

			free(tstrAPI);
			free(tstrCDN);

			break;
		}
	}

	return;
}

INT_PTR CALLBACK ChildDialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	HWND hwndParent = GetParent(hWnd);
	DialogHeader* pHdr = (DialogHeader*)GetWindowLongPtr(hwndParent, GWLP_USERDATA);
	switch (uMsg)
	{
		case WM_COMMAND:
		{
			switch (pHdr->pageNum)
			{
				case PG_ACCOUNT_AND_PRIVACY:
				{
					switch (wParam)
					{
						case IDC_REVEALEMAIL: {
							Profile* pProfile = GetDiscordInstance()->GetProfile();
							HWND hwndText = GetDlgItem(hWnd, IDC_EMAIL_DISPLAY);
							LPCTSTR lpctstr = ConvertCppStringToTString(pProfile->m_email);
							SetWindowText(hwndText, lpctstr);
							free((void*)lpctstr);
							break;
						}
						case IDC_LOG_OUT: {
							if (MessageBox(hWnd, TmGetTString(IDS_LOG_OUT_MESSAGE), TmGetTString(IDS_PROGRAM_NAME), MB_YESNO | MB_ICONQUESTION) == IDYES)
							{
								// Log them out!
								EndDialog(hWnd, 0);
								EndDialog(hwndParent, OPTIONS_RESULT_LOGOUT);
								return TRUE;
							}
							break;
						}
						case IDC_SDM_LEVEL0:
							GetSettingsManager()->SetExplicitFilter(FILTER_NONE);
							GetSettingsManager()->FlushSettings();
							break;
						case IDC_SDM_LEVEL1:
							GetSettingsManager()->SetExplicitFilter(FILTER_EXCEPTFRIENDS);
							GetSettingsManager()->FlushSettings();
							break;
						case IDC_SDM_LEVEL2:
							GetSettingsManager()->SetExplicitFilter(FILTER_TOTAL);
							GetSettingsManager()->FlushSettings();
							break;
						case IDC_ENABLE_DMS:
						{
							bool updateAllServers = MessageBox(
								hwndParent,
								TmGetTString(IDS_DMBLOCK_APPLY_RETROACTIVELY),
								TmGetTString(IDS_PROGRAM_NAME),
								MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2
							) == IDYES;

							bool isChecked = IsDlgButtonChecked(hWnd, IDC_ENABLE_DMS);

							std::vector<Snowflake> blocklist;
							if (updateAllServers)
							{
								if (!isChecked)
									GetDiscordInstance()->GetGuildIDs(blocklist, false);

								GetSettingsManager()->SetGuildDMBlocklist(blocklist);
							}

							GetSettingsManager()->SetDMBlockDefault(!isChecked);
							GetSettingsManager()->FlushSettings();
							break;
						}
					}
					break;
				}
				case PG_APPEARANCE:
				{
					switch (LOWORD(wParam))
					{
						case IDC_MESSAGE_STYLE:
						{
							if (HIWORD(wParam) != CBN_SELCHANGE)
								break;

							int sel = ComboBox_GetCurSel((HWND) lParam);
							if (sel == CB_ERR || sel < 0 || sel >= int(_countof(g_indexToMessageStyle)))
								break;

							eMessageStyle style = g_indexToMessageStyle[sel];
							GetLocalSettings()->SetMessageStyle(style);

							bool enable = style == MS_IMAGE;
							EnableWindow(GetDlgItem(hWnd, IDC_ACTIVE_IMAGE_EDIT),   enable);
							EnableWindow(GetDlgItem(hWnd, IDC_ACTIVE_IMAGE_BROWSE), enable);
							EnableWindow(GetDlgItem(hWnd, IDC_COMBO_ALIGNMENT),     enable);

							SendMessage(g_Hwnd, WM_MSGLISTUPDATEMODE, 0, 0);
							break;
						}
						case IDC_COMBO_ALIGNMENT:
						{
							if (HIWORD(wParam) != CBN_SELCHANGE)
								break;

							int sel = ComboBox_GetCurSel((HWND)lParam);
							if (sel == CB_ERR || sel < 0 || sel >= int(ALIGN_COUNT))
								break;

							eImageAlignment align = eImageAlignment(sel);
							GetLocalSettings()->SetImageAlignment(align);

							if (GetLocalSettings()->GetMessageStyle() == MS_IMAGE)
								SendMessage(g_Hwnd, WM_MSGLISTUPDATEMODE, 0, 0);

							break;
						}
						case IDC_ACTIVE_IMAGE_BROWSE:
						{
							const int MAX_FILE = 4096;
							TCHAR buffer[MAX_FILE];
							buffer[0] = 0;

						#ifdef WEBP_SUP
						#define COND_WEBP ";*.webp"
						#else
						#define COND_WEBP ""
						#endif

							OPENFILENAME ofn{};
							ofn.lStructSize    = sizeof ofn;
							ofn.hwndOwner      = g_Hwnd;
							ofn.hInstance      = g_hInstance;
							ofn.nMaxFile       = MAX_FILE;
							ofn.lpstrFile      = buffer;
							ofn.nMaxFileTitle  = 0;
							ofn.lpstrFileTitle = NULL;
							ofn.lpstrFilter    = TEXT("Image files\0*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tga" COND_WEBP "\0\0");
							ofn.lpstrTitle     = TmGetTString(IDS_SELECT_BACKGROUND_IMAGE);

							if (!GetOpenFileName(&ofn)) {
								// operation cancelled
								break;
							}

							std::string fileName = MakeStringFromTString(ofn.lpstrFile);
							GetLocalSettings()->SetImageBackgroundFileName(fileName);
							SendMessage(g_Hwnd, WM_MSGLISTUPDATEMODE, 0, 0);
							SetDlgItemText(hWnd, IDC_ACTIVE_IMAGE_EDIT, ofn.lpstrFile);
							break;
						}

						case IDC_APPEARANCE_COZY:
							GetSettingsManager()->SetMessageCompact(false);
							GetSettingsManager()->FlushSettings();
							SendMessage(g_Hwnd, WM_RECALCMSGLIST, 0, 0);
							break;
						case IDC_APPEARANCE_COMPACT:
							GetSettingsManager()->SetMessageCompact(true);
							GetSettingsManager()->FlushSettings();
							SendMessage(g_Hwnd, WM_RECALCMSGLIST, 0, 0);
							break;
						case IDC_DISABLE_FORMATTING:
							GetLocalSettings()->SetDisableFormatting(IsDlgButtonChecked(hWnd, IDC_DISABLE_FORMATTING));
							SendMessage(g_Hwnd, WM_RECALCMSGLIST, 0, 0);
							break;
						case IDC_SCROLL_BAR_ON_GUILDS:
							GetLocalSettings()->SetShowScrollBarOnGuildList(IsDlgButtonChecked(hWnd, IDC_SCROLL_BAR_ON_GUILDS));
							SendMessage(g_Hwnd, WM_REPOSITIONEVERYTHING, 0, 0);
							break;
						case IDC_SAVE_WINDOW_SIZE:
							GetLocalSettings()->SetSaveWindowSize(IsDlgButtonChecked(hWnd, IDC_SAVE_WINDOW_SIZE));
							break;
						case IDC_START_MAXIMIZED:
							GetLocalSettings()->SetStartMaximized(IsDlgButtonChecked(hWnd, IDC_START_MAXIMIZED));
							break;
					}
					break;
				}
				case PG_CONNECTION:
				{
					switch (wParam)
					{
						case IDC_ENABLE_TLS_CHECKS:
						{
							bool state = IsDlgButtonChecked(hWnd, IDC_ENABLE_TLS_CHECKS);
							if (!state) {
								if (MessageBox(hWnd, TmGetTString(IDS_CERT_SETTING_CONFIRM), TmGetTString(IDS_PROGRAM_NAME), MB_ICONWARNING | MB_YESNO) != IDYES)
								{
									CheckDlgButton(hWnd, IDC_ENABLE_TLS_CHECKS, BST_CHECKED);
									break;
								}
							}

							GetLocalSettings()->SetEnableTLSVerification(state);
							break;
						}

						case IDC_CHECK_UPDATES:
						{
							GetLocalSettings()->SetCheckUpdates(IsDlgButtonChecked(hWnd, IDC_CHECK_UPDATES));
							break;
						}

						case IDC_REVERTTODEFAULT:
						case IDC_UPDATE:
						{
							std::string api, cdn;

							if (wParam == IDC_REVERTTODEFAULT)
							{
								api = OFFICIAL_DISCORD_API;
								cdn = OFFICIAL_DISCORD_CDN;
							}
							else
							{
								TCHAR tchrAPI[512], tchrCDN[512];
								if (!GetDlgItemText(hWnd, IDC_EDIT_DISCORDAPI, tchrAPI, _countof(tchrAPI)) ||
									!GetDlgItemText(hWnd, IDC_EDIT_DISCORDCDN, tchrCDN, _countof(tchrCDN))) {
									MessageBox(hWnd, TmGetTString(IDS_URL_EMPTY), TmGetTString(IDS_PROGRAM_NAME), MB_ICONERROR | MB_OK);
									break;
								}

								api = MakeStringFromTString(tchrAPI);
								cdn = MakeStringFromTString(tchrCDN);
							}

							if (api.empty() || cdn.empty()) {
								MessageBox(hWnd, TmGetTString(IDS_URL_EMPTY), TmGetTString(IDS_PROGRAM_NAME), MB_ICONERROR | MB_OK);
								break;
							}

							if (api[api.size() - 1] != '/') api += '/';
							if (cdn[cdn.size() - 1] != '/') cdn += '/';

							if (MessageBox(hWnd, TmGetTString(wParam == IDC_UPDATE ? IDS_CONFIRM_SET_URLS : IDS_CONFIRM_OFFICIAL_URLS),
								TmGetTString(IDS_PROGRAM_NAME), MB_ICONQUESTION | MB_YESNO) != IDYES)
								break;
							
							// Ok!... your choice.
							GetLocalSettings()->SetToken("");
							GetLocalSettings()->SetDiscordAPI(api);
							GetLocalSettings()->SetDiscordCDN(cdn);
							GetDiscordInstance()->ResetGatewayURL();
							EndDialog(hWnd, 0);
							EndDialog(hwndParent, OPTIONS_RESULT_LOGOUT);

							break;
						}
					}
					break;
				}
			}
			break;
		}
		case WM_INITDIALOG:
		{
			OnChildDialogInit(hWnd);
			break;
		}
	}
	return 0L;
}

// Processes the TCN_SELCHANGE notification.
// hwndDlg - handle to the parent dialog box.
//
void OnSelChanged(HWND hwndDlg)
{
	// Get the dialog header data.
	DialogHeader *pHdr = (DialogHeader*) GetWindowLongPtr(hwndDlg, GWLP_USERDATA);

	// Get the index of the selected tab.
	int iSel = TabCtrl_GetCurSel(pHdr->hwndTab);

	// Destroy the current child dialog box, if any.
	if (pHdr->hwndDisplay != NULL)
		DestroyWindow(pHdr->hwndDisplay);

	// Create the new child dialog box. Note that g_hInst is the
	// global instance handle.
	pHdr->pageNum     = iSel;
	pHdr->hwndDisplay = CreateDialogIndirect(g_hInstance, (DLGTEMPLATE *)pHdr->apRes[iSel], hwndDlg, ChildDialogProc);

	return;
}

HRESULT OnPreferenceDialogInit(HWND hWnd)
{
#ifndef MINGW_SPECIFIC_HACKS
	INITCOMMONCONTROLSEX iccex;
	iccex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	iccex.dwICC  = ICC_TAB_CLASSES;
	InitCommonControlsEx(&iccex);
#endif

	DWORD dwDlgBase = GetDialogBaseUnits();
	int cxMargin = LOWORD(dwDlgBase) / 4;
	int cyMargin = HIWORD(dwDlgBase) / 8;

	DialogHeader* pHeader = new DialogHeader;
	SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG)pHeader);

	// Get the tab control. Note the hardcoded sizes:
	HWND hwndTab = GetDlgItem(hWnd, IDC_OPTIONS_TABS);
	if (!hwndTab) {
		delete pHeader;
		return HRESULT_FROM_WIN32(GetLastError());
	}

	pHeader->hwndTab = hwndTab;

	// Add a bunch of tabs.
	TCITEM tie;
	tie.mask = TCIF_TEXT | TCIF_IMAGE;

	AddTab(hwndTab, tie, PG_ACCOUNT_AND_PRIVACY, (LPTSTR)TmGetTString(IDS_ACCOUNT_PRIVACY));
	AddTab(hwndTab, tie, PG_APPEARANCE,          (LPTSTR)TmGetTString(IDS_APPEARANCE));
	AddTab(hwndTab, tie, PG_CONNECTION,          (LPTSTR)TmGetTString(IDS_CONNECTION));

	// Lock the resources for the three child dialog boxes.
	pHeader->apRes[PG_ACCOUNT_AND_PRIVACY] = LockDialogResource(MAKEINTRESOURCE(IDD_DIALOG_MY_ACCOUNT));
	pHeader->apRes[     PG_APPEARANCE    ] = LockDialogResource(MAKEINTRESOURCE(IDD_DIALOG_APPEARANCE));
	pHeader->apRes[     PG_CONNECTION    ] = LockDialogResource(MAKEINTRESOURCE(IDD_DIALOG_CONNECTION));

	RECT rcTab;
	SetRectEmpty(&rcTab);

	for (int i = 0; i < C_PAGES; i++)
	{
		if (rcTab.right < pHeader->apRes[i]->cx)
			rcTab.right = pHeader->apRes[i]->cx;
		if (rcTab.bottom < pHeader->apRes[i]->cy)
			rcTab.bottom = pHeader->apRes[i]->cy;
	}

	MapDialogRect(hWnd, &rcTab);

	// Calculate how large to make the tab control, so
	// the display area can accommodate all the child dialog boxes.
	TabCtrl_AdjustRect(pHeader->hwndTab, TRUE, &rcTab);
	OffsetRect(&rcTab, cxMargin - rcTab.left, cyMargin - rcTab.top);

	// Calculate the display rectangle.
	CopyRect(&pHeader->rcDisplay, &rcTab);
	TabCtrl_AdjustRect(pHeader->hwndTab, FALSE, &pHeader->rcDisplay);

	// Set the size and position of the tab control, buttons,
	// and dialog box.
	SetWindowPos(pHeader->hwndTab, NULL, rcTab.left, rcTab.top,
		rcTab.right - rcTab.left, rcTab.bottom - rcTab.top,
		SWP_NOZORDER);

	// Size the dialog box.
	SetWindowPos(hWnd, NULL, 0, 0,
		rcTab.right + cyMargin + (2 * GetSystemMetrics(SM_CXDLGFRAME)),
		rcTab.bottom + (2 * cyMargin)
		+ (2 * GetSystemMetrics(SM_CYDLGFRAME))
		+ GetSystemMetrics(SM_CYCAPTION),
		SWP_NOMOVE | SWP_NOZORDER);

	// Simulate selection of the first item.
	OnSelChanged(hWnd);

	return S_OK;
}

static bool OptionsOnNotify(HWND hwndTab, HWND hWnd, LPARAM lParam)
{
	switch (((LPNMHDR)lParam)->code)
	{
		case TCN_SELCHANGE:
			OnSelChanged(hWnd);
			break;
	}
	return TRUE;
}

static INT_PTR CALLBACK DialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_INITDIALOG:
		{
			OnPreferenceDialogInit(hWnd);
			break;
		}
		case WM_NOTIFY:
		{
			LPNMHDR hdr = (LPNMHDR)lParam;
			return OptionsOnNotify(hdr->hwndFrom, hWnd, lParam);
		}
		case WM_DESTROY:
		{
			DialogHeader* pData = (DialogHeader*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
			if (pData)
				delete pData;

			// fallthrough
		}
		case WM_CLOSE:
		{
			EndDialog(hWnd, OPTIONS_RESULT_OK);
			break;
		}
	}

	return 0L;
}

int ShowOptionsDialog()
{
	return DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_DIALOG_OPTIONS), g_Hwnd, DialogProc);
}
