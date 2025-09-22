#include "ChatWindow.hpp"
#include "ImageLoader.hpp"
#include "TextManager.hpp"
#include "UploadDialog.hpp"
#include "WinUtils.hpp"
#include "../discord/DiscordInstance.hpp"

ChatWindow::ChatWindow()
{
	m_chatView = std::make_shared<ChatView>();

	if (GetDiscordInstance())
		GetDiscordInstance()->RegisterView(m_chatView);
}

ChatWindow::~ChatWindow()
{
	if (GetDiscordInstance())
		GetDiscordInstance()->UnregisterView(m_chatView);
}

ChatViewPtr ChatWindow::GetChatView() const
{
	return m_chatView;
}

Snowflake ChatWindow::GetCurrentGuildID() const
{
	return m_chatView->GetCurrentGuildID();
}

Snowflake ChatWindow::GetCurrentChannelID() const
{
	return m_chatView->GetCurrentChannelID();
}

Guild* ChatWindow::GetCurrentGuild()
{
	return m_chatView->GetCurrentGuild();
}

Channel* ChatWindow::GetCurrentChannel()
{
	return m_chatView->GetCurrentChannel();
}

void ChatWindow::SetCurrentGuildID(Snowflake sf)
{
	m_chatView->SetCurrentGuildID(sf);
}

void ChatWindow::SetCurrentChannelID(Snowflake sf)
{
	m_chatView->SetCurrentChannelID(sf);
}

void ChatWindow::ShowPasteFileDialog(HWND editHwnd)
{
	HWND hWnd = GetHWND();
	if (!OpenClipboard(hWnd)) {
		DbgPrintW("Error opening clipboard: %d", GetLastError());
		return;
	}

	HBITMAP hbm = NULL;
	HDC hdc = NULL;
	BYTE* pBytes = nullptr;
	LPCTSTR fileName = nullptr;
	std::vector<uint8_t> data;
	bool isClipboardClosed = false;
	bool hadToUseLegacyBitmap = false;
	int width = 0, height = 0, stride = 0, bpp = 0;
	Channel* pChan = nullptr;

	HANDLE hbmi = GetClipboardData(CF_DIB);
	if (hbmi)
	{
		DbgPrintW("Using CF_DIB to fetch image from clipboard");
		PBITMAPINFO bmi = (PBITMAPINFO) GlobalLock(hbmi);

		// WHAT THE HELL: Windows seems to add 12 extra bytes after the header
		// representing the colors: [RED] [GREEN] [BLUE]. I don't know why we
		// have to do this or how to un-hardcode it.  Yes, in this case, the
		// biClrUsed member is zero.
		// Does it happen to include only part of the BITMAPV4HEADER???
		const int HACK_OFFSET = 12;

		width = bmi->bmiHeader.biWidth;
		height = bmi->bmiHeader.biHeight;
		bpp = bmi->bmiHeader.biBitCount;
		stride = ((((width * bpp) + 31) & ~31) >> 3);

		int sz = stride * height;
		pBytes = new BYTE[sz];
		memcpy(pBytes, (char*) bmi + bmi->bmiHeader.biSize + HACK_OFFSET, sz);

		GlobalUnlock(hbmi);
	}
	else
	{
		// try legacy CF_BITMAP
		DbgPrintW("Using legacy CF_BITMAP");
		HBITMAP hbm = (HBITMAP) GetClipboardData(CF_BITMAP);
		hadToUseLegacyBitmap = true;

		if (!hbm)
		{
			CloseClipboard();

			// No bitmap, forward to the edit control if selected
			HWND hFocus = GetFocus();
			if (hFocus == editHwnd)
				SendMessage(hFocus, WM_PASTE, 0, 0);

			return;
		}

		HDC hdc = GetDC(hWnd);
		bool res = GetDataFromBitmap(hdc, hbm, pBytes, width, height, bpp);
		ReleaseDC(hWnd, hdc);

		if (!res)
			goto _fail;

		stride = ((((width * bpp) + 31) & ~31) >> 3);
	}

	pChan = GetCurrentChannel();
	if (!pChan) {
		DbgPrintW("No channel!");
		goto _fail;
	}

	if (!pChan->HasPermission(PERM_SEND_MESSAGES) ||
		!pChan->HasPermission(PERM_ATTACH_FILES)) {
		DbgPrintW("Can't attach files here!");
		goto _fail;
	}

	// TODO: Don't always force alpha when pasting from CF_DIB or CF_BITMAP.
	// I know this sucks, but I've tried a lot of things...

	// have to force alpha always
	// have to flip vertically if !hadToUseLegacyBitmap
	if (!ImageLoader::ConvertToPNG(&data, pBytes, width, height, stride, bpp, true, !hadToUseLegacyBitmap)) {
		DbgPrintW("Cannot convert to PNG!");
		goto _fail;
	}

	CloseClipboard(); isClipboardClosed = true;

	fileName = TmGetTString(IDS_UNKNOWN_FILE_NAME);
	UploadDialogShowWithFileData(GetHWND(), GetCurrentChannelID(), data.data(), data.size(), fileName);

_fail:
	data.clear();
	if (pBytes) delete[] pBytes;
	if (hdc) ReleaseDC(hWnd, hdc);
	if (hbm) DeleteObject(hbm); // should I do this?
	if (!isClipboardClosed) CloseClipboard();
}
