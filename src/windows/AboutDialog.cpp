#include "Main.hpp"
#include "AboutDialog.hpp"

static LRESULT CALLBACK DialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_INITDIALOG: {
			std::string name = TmGetString(IDS_PROGRAM_NAME) + " " + GetAppVersionString();
#ifdef IS_64_BIT
			name += " (x64)";
#endif
			LPTSTR tstr = ConvertCppStringToTString(name);

			SetDlgItemText(hWnd, IDC_PROGRAM_NAME, tstr);
			free(tstr);
			break;
		}

		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK) {
				EndDialog(hWnd, LOWORD(wParam));
				return TRUE;
			}

			break;
		case WM_CLOSE:
			EndDialog(hWnd, IDCANCEL);
			return TRUE;
	}

	return 0L;
}

void About()
{
	DialogBox(g_hInstance, MAKEINTRESOURCE(DMDI(IDD_DIALOG_ABOUT)), GetMainHWND(), (DLGPROC)DialogProc);
}
