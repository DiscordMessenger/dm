#include "ProgressDialog.hpp"
#include "Main.hpp"

using UpdateInfo = ProgressDialog::UpdateInfo;

HWND ProgressDialog::m_hwnd;
std::string ProgressDialog::m_fileName;
Snowflake ProgressDialog::m_key;
size_t ProgressDialog::m_offset, ProgressDialog::m_lastOffset, ProgressDialog::m_length;
bool ProgressDialog::m_bCanceling;
UpdateInfo ProgressDialog::m_updateInfo[256];
size_t ProgressDialog::m_updateInfoHead;
bool ProgressDialog::m_bDirection;

UpdateInfo& ProgressDialog::FindUpdateOneSecondAgo()
{
	size_t c = _countof(m_updateInfo);
	size_t i = m_updateInfoHead;
	uint64_t timeMs = GetTimeMs();
	for (i = (i + c - 1) % c; i != m_updateInfoHead; i = (i + c - 1) % c)
	{
		if (timeMs - 1000 >= m_updateInfo[i].time)
			return m_updateInfo[i];
	}

	// base case - just return the item at the head
	return m_updateInfo[i];
}

void ProgressDialog::ResetUpdates()
{
	for (size_t i = 0; i < _countof(m_updateInfo); i++)
	{
		UpdateInfo& ui = m_updateInfo[i];
		ui.offset = 0;
		ui.time = 0;
	}
}

void ProgressDialog::Show(const std::string& fileName, Snowflake key, bool isUploading, HWND hWnd)
{
	if (m_hwnd) {
		DbgPrintW("Dropping uploading dialog because it already exists");
		return;
	}

	m_key = key;
	m_fileName = fileName;
	m_bDirection = isUploading;
	ResetUpdates();

	DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_DIALOG_UPLOADING), hWnd, &DlgProc);
}

bool ProgressDialog::Update(Snowflake key, size_t offset, size_t length)
{
	if (key != m_key) return true;

	UpdateInfo& ui = m_updateInfo[m_updateInfoHead];
	ui.offset = offset;
	ui.time = GetTimeMs();
	m_updateInfoHead = (m_updateInfoHead + 1) % _countof(m_updateInfo);

	m_offset = offset;
	m_length = length;
	return (bool) SendMessage(m_hwnd, WM_UPDATEUPLOADING, 0, 0);
}

void ProgressDialog::Done(Snowflake key)
{
	if (key != m_key) return;
	SendMessage(m_hwnd, WM_DONEUPLOADING, 0, 0);
}

LRESULT ProgressDialog::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	WNDPROC wp = (WNDPROC)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	LRESULT lres = wp(hWnd, uMsg, wParam, lParam);

	switch (uMsg)
	{
		case WM_DESTROY:
			SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR) wp);

		case WM_UPDATEUPLOADING:
			lres = !m_bCanceling;
			break;
	}

	return lres;
}

BOOL ProgressDialog::DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_INITDIALOG: {
			SetWindowText(hWnd, m_bDirection ? TEXT("File Upload") : TEXT("File Download"));

			LONG_PTR lp = GetWindowLongPtr(hWnd, GWLP_WNDPROC);
			SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)&WndProc);
			SetWindowLongPtr(hWnd, GWLP_USERDATA, lp);
			m_hwnd = hWnd;

			LPTSTR fileName = ConvertCppStringToTString(m_fileName);
			SetDlgItemText(hWnd, IDC_UPLOADING_FILENAME, fileName);
			free(fileName);

			SetDlgItemText(hWnd, IDC_UPLOADING_ETA, TEXT("Calculating..."));
			SetDlgItemText(hWnd, IDC_UPLOADING_ACTION, m_bDirection ? TEXT("Uploading:") : TEXT("Downloading:"));

			SendMessage(GetDlgItem(hWnd, IDC_UPLOADING_PROGRESS), PBM_SETRANGE, 0, MAKELPARAM(0, 1000));
			SendMessage(GetDlgItem(hWnd, IDC_UPLOADING_PROGRESS), PBM_SETPOS, 0, 0);

			HDC hdc = GetDC(hWnd);
			int bpp = GetDeviceCaps(hdc, BITSPIXEL);
			ReleaseDC(hWnd, hdc);

			int res;
			bool isHighColor = bpp > 8;

			if (m_bDirection) {
				res = isHighColor ? IDR_AVI_UPLOAD_HC : IDR_AVI_UPLOAD_LC;
			}
			else {
				res = isHighColor ? IDR_AVI_DOWNLOAD_HC : IDR_AVI_DOWNLOAD_LC;
			}

			Animate_Open(GetDlgItem(hWnd, IDC_PROGRESS_ANIMATE), MAKEINTRESOURCE(res));

			CenterWindow(hWnd, g_Hwnd);
			break;
		}

		case WM_DESTROY:
			if (m_hwnd != hWnd)
				break;
			
			m_hwnd = NULL;
			m_key = 0;
			m_fileName.clear();
			m_offset = m_length = 0;
			m_bCanceling = false;
			break;

		case WM_UPDATEUPLOADING: {
			if (m_bCanceling)
				break;

			std::string strProgress = std::to_string(m_offset / 1024) + " of " + std::to_string(m_length / 1024) + " KB";

			UpdateInfo& ui = FindUpdateOneSecondAgo();
			uint64_t timeSpent = ui.time ? (GetTimeMs() - ui.time) : 0;
			size_t bytesUploaded = m_offset - ui.offset;

			size_t bytesPerSec = timeSpent ? size_t(uint64_t(bytesUploaded) * 1000 / timeSpent) : 0;
			
			std::string etaStr;
			if (bytesPerSec) {
				uint64_t eta = bytesPerSec ? (uint64_t(m_length - m_offset) / bytesPerSec) : 0;
				etaStr = FormatDuration(eta) + " (" + strProgress + " uploaded)";
			}
			else {
				etaStr = "Calculating...";
			}

			LPTSTR tstr = ConvertCppStringToTString(etaStr);
			SetDlgItemText(hWnd, IDC_UPLOADING_ETA, tstr);
			free(tstr);

			std::string transferRate = bytesPerSec ? (std::to_string(bytesPerSec / 1024) + " KB/Sec") : "Calculating...";
			tstr = ConvertCppStringToTString(transferRate);
			SetDlgItemText(hWnd, IDC_UPLOADING_XFERRATE, tstr);
			free(tstr);

			int offs = int(uint64_t(m_offset) * 1000 / m_length);
			SendMessage(GetDlgItem(hWnd, IDC_UPLOADING_PROGRESS), PBM_SETPOS, (WPARAM) offs, 0);
			break;
		}

		case WM_DONEUPLOADING:
			EndDialog(hWnd, TRUE);
			break;

		case WM_COMMAND:
			if (wParam == IDCANCEL) {
				SetDlgItemText(hWnd, IDC_UPLOADING_ETA, TEXT("Cancelling..."));
				m_bCanceling = true;
			}
			break;
	}

	return FALSE;
}
