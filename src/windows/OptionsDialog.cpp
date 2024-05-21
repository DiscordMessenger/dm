#include "Config.hpp"
#include "OptionsDialog.hpp"
#include "../discord/LocalSettings.hpp"

#ifdef NEW_WINDOWS
#include <uxtheme.h>
#endif

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
	HWND hwndTab;     // tab control
	HWND hwndDisplay; // current child dialog box
	RECT rcDisplay;
	DLGTEMPLATEEX* apRes[C_PAGES];
	int  pageNum;
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

			int mStyleId = IDC_RADIO_3D_FRAME;
			switch (GetLocalSettings()->GetMessageStyle())
			{
				case MS_3DFACE:   mStyleId = IDC_RADIO_3D_FRAME; break;
				case MS_FLAT:     mStyleId = IDC_RADIO_FLAT_1;   break;
				case MS_FLATBR:   mStyleId = IDC_RADIO_FLAT_2;   break;
				case MS_GRADIENT: mStyleId = IDC_RADIO_GRADIENT; break;
			}

			CheckRadioButton(
				hwndDlg,
				IDC_RADIO_3D_FRAME,
				IDC_RADIO_FLAT_2,
				mStyleId
			);

			CheckDlgButton(hwndDlg, IDC_SAVE_WINDOW_SIZE, GetLocalSettings()->GetSaveWindowSize() ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hwndDlg, IDC_START_MAXIMIZED,  GetLocalSettings()->GetStartMaximized() ? BST_CHECKED : BST_UNCHECKED);

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
							if (MessageBox(hWnd,
								TEXT("Are you sure you want to log out?\n\nThis won't revoke your token, so you can log in with the same token again."),
								TmGetTString(IDS_PROGRAM_NAME), MB_YESNO | MB_ICONQUESTION) == IDYES)
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
					switch (wParam)
					{
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
						case IDC_RADIO_3D_FRAME:
							GetLocalSettings()->SetMessageStyle(MS_3DFACE);
							SendMessage(g_Hwnd, WM_MSGLISTUPDATEMODE, 0, 0);
							break;
						case IDC_RADIO_GRADIENT:
							GetLocalSettings()->SetMessageStyle(MS_GRADIENT);
							SendMessage(g_Hwnd, WM_MSGLISTUPDATEMODE, 0, 0);
							break;
						case IDC_RADIO_FLAT_1:
							GetLocalSettings()->SetMessageStyle(MS_FLAT);
							SendMessage(g_Hwnd, WM_MSGLISTUPDATEMODE, 0, 0);
							break;
						case IDC_RADIO_FLAT_2:
							GetLocalSettings()->SetMessageStyle(MS_FLATBR);
							SendMessage(g_Hwnd, WM_MSGLISTUPDATEMODE, 0, 0);
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
								if (MessageBox(
										hWnd,
										TEXT("WARNING:\n\nYou are about to change the TLS certificate verification setting. This may expose you to MITM (man-in-the-middle) ")
										TEXT("attacks. It is recommended you instead import the required certificates into Windows.\n\nAre you sure you want to do this? ")
										TEXT("(you can re-enable it anytime in the preferences menu)"),
										TmGetTString(IDS_PROGRAM_NAME),
										MB_ICONWARNING | MB_YESNO
									) != IDYES)
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

	DialogHeader* pHeader = (DialogHeader*)LocalAlloc(LPTR, sizeof(DialogHeader)); // not sure why they're using LocalAlloc here

	SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG)pHeader);

	// Get the tab control. Note the hardcoded sizes:
	HWND hwndTab = GetDlgItem(hWnd, IDC_OPTIONS_TABS);
	if (!hwndTab)
		return HRESULT_FROM_WIN32(GetLastError());

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
		case WM_CLOSE:
		case WM_DESTROY:
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
