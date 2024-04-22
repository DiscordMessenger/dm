#include "UploadDialog.hpp"
#include "Main.hpp"
#include <CommDlg.h>
#include <ShellAPI.h>
#include <nlohmann/json.h>
using Json = nlohmann::json;

struct UploadDialogData
{
	LPTSTR m_lpstrFile = nullptr;
	LPTSTR m_lpstrFileTitle = nullptr;
	SHFILEINFO m_sfi{};
	TCHAR m_comment[4096] = { 0 };
	bool m_bSpoiler = false;
	DWORD m_fileSize = 0;
	BYTE* m_pFileData = nullptr;

	~UploadDialogData()
	{
		if (m_sfi.hIcon)
			DestroyIcon(m_sfi.hIcon);

		if (m_pFileData)
			delete[] m_pFileData;
	}
};

static int g_UploadId = 1;
static std::map<int, UploadDialogData*> g_PendingUploads;


void UploadDialogFireInitialRequest(int id)
{
	UploadDialogData* data = g_PendingUploads[id];

	Json file;
	file["filename"]  = MakeStringFromTString(data->m_lpstrFileTitle);
	file["file_size"] = data->m_fileSize;
	file["is_clip"] = false;
	file["id"] = std::to_string(id);

	Json files;
	files.push_back(file);

	Json j;
	j["files"] = files;
}

void UploadDialogOnFileTooBig(HWND hWnd)
{
	MessageBox(hWnd, TmGetTString(IDS_FILE_TOO_BIG), TmGetTString(IDS_PROGRAM_NAME), MB_OK | MB_ICONERROR);
}
void UploadDialogOnEmptyFile(HWND hWnd)
{
	MessageBox(hWnd, TmGetTString(IDS_FILE_EMPTY), TmGetTString(IDS_PROGRAM_NAME), MB_OK | MB_ICONERROR);
}
void UploadDialogOutOfMemory(HWND hWnd)
{
	MessageBox(hWnd, TmGetTString(IDS_FILE_OUT_OF_MEMORY), TmGetTString(IDS_PROGRAM_NAME), MB_OK | MB_ICONERROR);
}
void UploadDialogCantReadFile(HWND hWnd)
{
	MessageBox(hWnd, TmGetTString(IDS_FILE_NO_READ), TmGetTString(IDS_PROGRAM_NAME), MB_OK | MB_ICONERROR);
}
void UploadDialogCantOpenFile(HWND hWnd)
{
	MessageBox(hWnd, TmGetTString(IDS_FILE_NO_OPEN), TmGetTString(IDS_PROGRAM_NAME), MB_OK | MB_ICONERROR);
}

typedef void(*FailErrorDialog)(HWND hWnd);
enum ReadFileError {
	RFE_SUCCESS,
	RFE_CANTOPEN,
	RFE_FILEEMPTY,
	RFE_FILETOOBIG,
	RFE_NOMEMORY,
	RFE_CANTREAD,
};

static const FailErrorDialog g_FailDialogs[] = {
	NULL,
	UploadDialogCantOpenFile,
	UploadDialogOnEmptyFile,
	UploadDialogOnFileTooBig,
	UploadDialogOutOfMemory,
	UploadDialogOutOfMemory,
	UploadDialogCantReadFile,
};

static int FailAndClose(HANDLE hFile, int errorCode) {
	CloseHandle(hFile);
	return errorCode;
}

static int FailDeallocAndClose(HANDLE hFile, BYTE*& pbFileData, int errorCode) {
	delete[] pbFileData;
	pbFileData = NULL;
	return FailAndClose(hFile, errorCode);
}

int UploadDialogTryReadFile(LPTSTR pszFileName, DWORD& dwFileSize, BYTE*& pbFileData)
{
	HANDLE hFile = CreateFile(pszFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return RFE_CANTOPEN;

	bool bReadFailed = false;
	dwFileSize = GetFileSize(hFile, NULL);
	if (dwFileSize == INVALID_FILE_SIZE)
		return FailAndClose(hFile, RFE_FILETOOBIG);

	if (dwFileSize == 0)
		return FailAndClose(hFile, RFE_FILEEMPTY);

	pbFileData = new(std::nothrow) BYTE[dwFileSize];
	if (!pbFileData)
		return FailAndClose(hFile, RFE_NOMEMORY);

	DWORD bytesRead = 0;
	if (!ReadFile(hFile, pbFileData, dwFileSize, &bytesRead, NULL))
		return FailDeallocAndClose(hFile, pbFileData, RFE_CANTREAD);

	if (bytesRead != dwFileSize)
		return FailDeallocAndClose(hFile, pbFileData, RFE_CANTREAD);

	CloseHandle(hFile);
	return RFE_SUCCESS;
}

INT_PTR CALLBACK UploadDialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	UploadDialogData* pData = (UploadDialogData*) GetWindowLongPtr(hWnd, GWLP_USERDATA);

	switch (uMsg)
	{
		case WM_INITDIALOG:
		{
			Channel* pChan = GetDiscordInstance()->GetCurrentChannel();
			if (!pChan) {
				EndDialog(hWnd, IDCANCEL);
				return TRUE;
			}

			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)lParam);
			pData = (UploadDialogData*)lParam;
			pData->m_comment[0] = 0;

			DbgPrintW("Reading file %S", pData->m_lpstrFile);

			int erc = UploadDialogTryReadFile(pData->m_lpstrFile, pData->m_fileSize, pData->m_pFileData);
			if (erc > 0) {
				g_FailDialogs[erc](hWnd);
				EndDialog(hWnd, IDCANCEL);
				return TRUE;
			}

			// TODO: Win32 docs say we should do this in a background thread.
			DbgPrintW("Getting file info for %S", pData->m_lpstrFile);
			SHGetFileInfo(pData->m_lpstrFile, 0, &pData->m_sfi, sizeof(SHFILEINFO), SHGFI_ICON);
			DbgPrintW("Got the file info!");

			SetDlgItemText(hWnd, IDC_FILE_NAME, pData->m_lpstrFileTitle);
			
			LPTSTR tstr = ConvertCppStringToTString("#" + pChan->m_name);
			SetDlgItemText(hWnd, IDC_CHANNEL_NAME, tstr);
			SendMessage(GetDlgItem(hWnd, IDC_CHANNEL_NAME), WM_SETFONT, (WPARAM)g_AuthorTextFont, TRUE);
			free(tstr);

			Static_SetIcon(GetDlgItem(hWnd, IDC_FILE_ICON), pData->m_sfi.hIcon);

			RECT rect1, rect2;
			GetWindowRect(hWnd, &rect1);
			GetWindowRect(g_Hwnd, &rect2);

			OffsetRect(&rect1, -rect1.left, -rect1.top);
			int width = rect1.right - rect1.left, height = rect1.bottom - rect1.top;

			OffsetRect(
				&rect1,
				(rect2.left+rect2.right)/2-(rect1.right-rect1.left)/2,
				(rect2.top+rect2.bottom)/2-(rect1.bottom-rect1.top)/2
			);

			MoveWindow(hWnd, rect1.left, rect1.top, rect1.right - rect1.left, rect1.bottom - rect1.top, TRUE);

			break;
		}
		case WM_COMMAND:
		{
			if (wParam == IDOK || wParam == IDCANCEL)
			{
				GetDlgItemText(hWnd, IDC_ATTACH_COMMENT, pData->m_comment, sizeof pData->m_comment);
				pData->m_bSpoiler = IsDlgButtonChecked(hWnd, IDC_MARK_AS_SPOILER);
				EndDialog(hWnd, wParam);
				return TRUE;
			}

			break;
		}
	}

	return FALSE;
}

void UploadDialogShowWithFileName(LPTSTR lpstrFileName, LPTSTR lpstrFileTitle)
{
	UploadDialogData* data = new UploadDialogData;
	data->m_lpstrFile = lpstrFileName;
	data->m_lpstrFileTitle = lpstrFileTitle;

	if (DialogBoxParam(
			g_hInstance,
			MAKEINTRESOURCE(IDD_DIALOG_UPLOADFILES),
			g_Hwnd,
			UploadDialogProc,
			(LPARAM) data
		) == IDCANCEL)
	{
		DbgPrintW("User cancelled upload operation at upload box");
		delete data;
		return;
	}

	Snowflake sf;
	std::string content = MakeStringFromTString(data->m_comment);
	if (GetDiscordInstance()->SendMessageAndAttachmentToCurrentChannel(
			content,
			sf,
			data->m_pFileData,
			data->m_fileSize,
			MakeStringFromTString(data->m_lpstrFileTitle)
		))
	{
		// NOTE: DiscordInstance now owns the data, not us!
		data->m_pFileData = NULL;

		SendMessageAuxParams smap;
		smap.m_message = content;
		smap.m_snowflake = sf;
		SendMessage(g_Hwnd, WM_SENDMESSAGEAUX, 0, (LPARAM)&smap);
	}
	else
	{
		// TODO: Placeholder, replace
		MessageBox(g_Hwnd, TmGetTString(IDS_CANNOT_UPLOAD_ATTACHMENT), TmGetTString(IDS_PROGRAM_NAME), MB_OK);
	}

	delete data;
}

void UploadDialogShow2()
{
	const int MAX_FILE = 4096;
	TCHAR buffer[MAX_FILE];
	TCHAR buffer2[MAX_FILE];
	buffer[0] = buffer2[0] = 0;

	OPENFILENAME ofn{};
	ofn.lStructSize    = sizeof ofn;
	ofn.hwndOwner      = g_Hwnd;
	ofn.hInstance      = g_hInstance;
	ofn.nMaxFile       = MAX_FILE;
	ofn.lpstrFile      = buffer;
	ofn.nMaxFileTitle  = MAX_FILE;
	ofn.lpstrFileTitle = buffer2;
	ofn.lpstrFilter    = TEXT("All files\0*.*\0\0");
	ofn.lpstrTitle     = TEXT("Upload file");

	if (!GetOpenFileName(&ofn)) {
		// maybe they cancelled. hope there's no error!
		DbgPrintW("User cancelled upload operation at file name box");
		return;
	}

	UploadDialogShowWithFileName(
		ofn.lpstrFile,
		ofn.lpstrFileTitle
	);
}

void UploadDialogShow()
{
	PostMessage(g_Hwnd, WM_SHOWUPLOADDIALOG, 0, 0);
}
