#include "Main.hpp"
#include "AboutDialog.hpp"

static LRESULT CALLBACK DialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if ((uMsg == WM_COMMAND && LOWORD(wParam) == IDOK) || uMsg == WM_CLOSE)
	{
		EndDialog(hWnd, wParam);
		return TRUE;
	}

	return 0L;
}

void About()
{
	DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_DIALOG_ABOUT), g_Hwnd, (DLGPROC)DialogProc);
}
