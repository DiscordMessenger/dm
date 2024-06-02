#include "Main.hpp"
#include "LogonDialog.hpp"
#include "../discord/LocalSettings.hpp"

BOOL LogonDialogOnCommand(HWND hWnd, WPARAM wParam)
{
	switch (wParam) {
		case IDOK:
		{
			// Log In
			if (IsDlgButtonChecked(hWnd, IDC_RADIO_TOKEN))
			{
				// Log In Using Token
				TCHAR buff[256];
				int len = GetDlgItemText(hWnd, IDC_EDIT_TOKEN, buff, _countof(buff) - 1);

				if (len == 0 || len >= _countof(buff) - 1) {
					MessageBox(hWnd, TmGetTString(IDS_TOKEN_TOO_LONG), TmGetTString(IDS_PROGRAM_NAME), MB_ICONERROR | MB_OK);
					return 0;
				}

				buff[_countof(buff) - 1] = 0;
				buff[len] = 0;

				GetLocalSettings()->SetToken(MakeStringFromTString(buff));
				if (GetDiscordInstance())
					GetDiscordInstance()->ResetGatewayURL();
				EndDialog(hWnd, IDOK);
				return TRUE;
			}
			else
			{
				DbgPrintW("TODO: Log In Using E-mail And Password");
				EndDialog(hWnd, IDCANCEL);
				return TRUE;
			}

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
	return DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_DIALOG_LOGON), g_Hwnd, DialogProc) != IDCANCEL;
}
