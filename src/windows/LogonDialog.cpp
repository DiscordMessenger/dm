#include "Main.hpp"
#include "LogonDialog.hpp"
#include "WinUtils.hpp"
#include "config/LocalSettings.hpp"

BOOL LogonDialogOnCommand(HWND hWnd, WPARAM wParam)
{
	switch (LOWORD(wParam))
	{
		case IDOK:
		{
			// Log In Using Token
			TCHAR buff[256];
			int len = GetDlgItemText(hWnd, IDC_EDIT_TOKEN, buff, _countof(buff) - 1);

			if (len >= _countof(buff) - 1) {
			TokenTooLong:
				MessageBox(hWnd, TmGetTString(IDS_TOKEN_TOO_LONG), TmGetTString(IDS_PROGRAM_NAME), MB_ICONERROR | MB_OK);
				return 0;
			}

			if (len < 70) {
			TokenTooShort:
				MessageBox(hWnd, TmGetTString(IDS_TOKEN_TOO_SHORT), TmGetTString(IDS_PROGRAM_NAME), MB_ICONERROR | MB_OK);
				return 0;
			}

			buff[_countof(buff) - 1] = 0;
			buff[len] = 0;

			std::string token = MakeStringFromTString(buff);

			// filter out characters
			token = FilterToken(token);
			if (token.size() > 75)
				goto TokenTooLong;
			if (token.size() < 70)
				goto TokenTooShort;

			GetLocalSettings()->SetToken(token);
			if (GetDiscordInstance())
				GetDiscordInstance()->ResetGatewayURL();
			EndDialog(hWnd, IDOK);
			return TRUE;
		}

		case IDC_HOW_GET_TOKEN:
		{
			MessageBox(hWnd, TmGetTString(IDS_GET_TOKEN_TUTORIAL), TmGetTString(IDS_PROGRAM_NAME), MB_OK);
			break;
		}

		case IDCANCEL:
		{
			EndDialog(hWnd, IDCANCEL);
			return TRUE;
		}
	}

	return FALSE;
}

static INT_PTR CALLBACK DialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_INITDIALOG:
		{
			// check the token one
			CheckRadioButton(hWnd, IDC_RADIO_EMAILPASS, IDC_RADIO_TOKEN, IDC_RADIO_TOKEN);

			// TODO: Disable some things because they aren't implemented
			EnableWindow(GetDlgItem(hWnd, IDC_RADIO_EMAILPASS), FALSE);
			EnableWindow(GetDlgItem(hWnd, IDC_EDIT_EMAIL),      FALSE);
			EnableWindow(GetDlgItem(hWnd, IDC_EDIT_PASSWORD),   FALSE);
			EnableWindow(GetDlgItem(hWnd, IDC_LBL_EMAIL),       FALSE);
			EnableWindow(GetDlgItem(hWnd, IDC_LBL_PASSWORD),    FALSE);
			EnableWindow(GetDlgItem(hWnd, IDC_LOGIN_QR_CODE),   FALSE);
			break;
		}
		case WM_COMMAND:
		{
			return LogonDialogOnCommand(hWnd, wParam);
		}
	}

	return 0;
}

bool LogonDialogShow()
{
	return DialogBox(g_hInstance, MAKEINTRESOURCE(DMDI(IDD_DIALOG_LOGON)), g_Hwnd, DialogProc) != IDCANCEL;
}
