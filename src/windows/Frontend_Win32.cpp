#include "Frontend_Win32.hpp"

#include "Main.hpp"
#include "ImageLoader.hpp"
#include "ProfilePopout.hpp"
#include "QRCodeDialog.hpp"
#include "PinList.hpp"
#include "ProgressDialog.hpp"
#include "ShellNotification.hpp"
#include "../discord/UpdateChecker.hpp"
#include "../discord/LocalSettings.hpp"

void Frontend_Win32::OnLoginAgain()
{
	SendMessage(g_Hwnd, WM_LOGINAGAIN, 0, 0);
}

void Frontend_Win32::OnLoggedOut()
{
	SendMessage(g_Hwnd, WM_LOGGEDOUT, 0, 0);
}

void Frontend_Win32::OnSessionClosed(int errorCode)
{
	SendMessage(g_Hwnd, WM_SESSIONCLOSED, (WPARAM) errorCode, 0);
}

void Frontend_Win32::OnConnecting()
{
	SendMessage(g_Hwnd, WM_CONNECTING, 0, 0);
}

void Frontend_Win32::OnConnected()
{
	SendMessage(g_Hwnd, WM_CONNECTED, 0, 0);
}

void Frontend_Win32::OnAddMessage(Snowflake channelID, const Message& msg)
{
	AddMessageParams parms;
	parms.channel = channelID;
	parms.msg = msg;
	SendMessage(g_Hwnd, WM_ADDMESSAGE, 0, (LPARAM)&parms);
}

void Frontend_Win32::OnUpdateMessage(Snowflake channelID, const Message& msg)
{
	AddMessageParams parms;
	parms.channel = channelID;
	parms.msg = msg;
	SendMessage(g_Hwnd, WM_UPDATEMESSAGE, 0, (LPARAM)&parms);
}

void Frontend_Win32::OnDeleteMessage(Snowflake messageInCurrentChannel)
{
	SendMessage(g_Hwnd, WM_DELETEMESSAGE, 0, (LPARAM)&messageInCurrentChannel);
}

void Frontend_Win32::OnStartTyping(Snowflake userID, Snowflake guildID, Snowflake channelID, time_t startTime)
{
	TypingParams parms;
	parms.m_user = userID;
	parms.m_guild = guildID;
	parms.m_channel = channelID;
	parms.m_timestamp = startTime;
	SendMessage(g_Hwnd, WM_STARTTYPING, 0, (LPARAM)&parms);
}

extern int g_latestSSLError; // HACK -- defined by the NetworkerThread.  Used to debug an issue.

void Frontend_Win32::OnGenericError(const std::string& message)
{
	std::string newMsg = message;

	// TODO HACK: remove soon
	if (g_latestSSLError) {
		char buff[128];
		snprintf(buff, sizeof buff, "\n\nAdditionally, an SSL error code of 0x%x was provided.  Consider sending this to iProgramInCpp!", g_latestSSLError);
		newMsg += std::string(buff);
		g_latestSSLError = 0;
	}

	LPCTSTR pMsgBoxText = ConvertCppStringToTString(newMsg);
	MessageBox(g_Hwnd, pMsgBoxText, TmGetTString(IDS_ERROR), MB_OK | MB_ICONERROR);
	free((void*)pMsgBoxText);
	pMsgBoxText = NULL;
}

void Frontend_Win32::OnJsonException(const std::string& message)
{
	std::string err(TmGetString(IDS_JSON_EXCEPTION));
	err += message;
	LPCTSTR pMsgBoxText = ConvertCppStringToTString(err);
	MessageBox(g_Hwnd, pMsgBoxText, TmGetTString(IDS_ERROR), MB_OK | MB_ICONERROR);
	free((void*)pMsgBoxText);
	pMsgBoxText = NULL;
}

void Frontend_Win32::OnCantViewChannel(const std::string& channelName)
{
	TCHAR buff[4096];
	LPTSTR chanName = ConvertCppStringToTString(channelName);
	WAsnprintf(buff, _countof(buff), TmGetTString(IDS_CANT_VIEW_CHANNEL), chanName);
	free(chanName);

	MessageBox(
		g_Hwnd,
		buff,
		TmGetTString(IDS_PROGRAM_NAME),
		MB_OK | MB_ICONERROR
	);
}

void Frontend_Win32::OnGatewayConnectFailure()
{
	MessageBox(g_Hwnd, TmGetTString(IDS_CONNECT_FAILURE), TmGetTString(IDS_PROGRAM_NAME), MB_ICONERROR | MB_OK);
	RequestQuit();
}

void Frontend_Win32::OnProtobufError(Protobuf::ErrorCode code)
{
	char buff[512];
	buff[511] = 0;
	snprintf(buff, sizeof buff - 1, TmGetString(IDS_PROTOBUF_ERROR).c_str(), code);
	MessageBoxA(g_Hwnd, buff, TmGetString(IDS_ERROR).c_str(), MB_ICONERROR);
}

void Frontend_Win32::OnRequestDone(NetRequest* pRequest)
{
	SendMessage(g_Hwnd, WM_REQUESTDONE, 0, (LPARAM) pRequest);
}

void Frontend_Win32::OnLoadedPins(Snowflake channel, const std::string& data)
{
	if (PinList::IsActive())
		PinList::OnLoadedPins(channel, data);
}

void Frontend_Win32::OnUpdateAvailable(const std::string& url, const std::string& version)
{
	std::string* msg = new std::string[2];
	msg[0] = url;
	msg[1] = version;
	PostMessage(g_Hwnd, WM_UPDATEAVAILABLE, 0, (LPARAM) msg);
}

void Frontend_Win32::OnFailedToSendMessage(Snowflake channel, Snowflake message)
{
	FailedMessageParams parms;
	parms.channel = channel;
	parms.message = message;
	SendMessage(g_Hwnd, WM_FAILMESSAGE, 0, (LPARAM)&parms);
}

void Frontend_Win32::OnFailedToUploadFile(const std::string& file, int error)
{
	// ignore if request was canceled
	if (error == HTTP_CANCELED)
		return;

	TCHAR buff[4096];
	LPTSTR tstr = ConvertCppStringToTString(file);
	WAsnprintf(buff, _countof(buff), TmGetTString(IDS_FAILED_TO_UPLOAD), tstr, error);
	free(tstr);
	MessageBox(g_Hwnd, buff, TmGetTString(IDS_PROGRAM_NAME), MB_ICONERROR | MB_OK);
}

void Frontend_Win32::OnFailedToCheckForUpdates(int result, const std::string& response)
{
	TCHAR buff[2048];
	LPTSTR tstr = ConvertCppStringToTString(response);
	WAsnprintf(buff, _countof(buff), TmGetTString(IDS_FAILED_UPDATE_CHECK), result, tstr);
	free(tstr);

	MessageBox(g_Hwnd, buff, TmGetTString(IDS_PROGRAM_NAME), MB_ICONERROR | MB_OK);
}

void Frontend_Win32::OnStartProgress(Snowflake key, const std::string& fileName, bool isUploading)
{
	ProgressDialog::Show(fileName, key, isUploading, g_Hwnd);
}

bool Frontend_Win32::OnUpdateProgress(Snowflake key, size_t offset, size_t length)
{
	return ProgressDialog::Update(key, offset, length);
}

void Frontend_Win32::OnStopProgress(Snowflake key)
{
	ProgressDialog::Done(key);
}

void Frontend_Win32::OnNotification()
{
	GetShellNotification()->OnNotification();
}

void Frontend_Win32::OnAttachmentDownloaded(bool bIsProfilePicture, const uint8_t* pData, size_t nSize, const std::string& additData)
{
	int nImSize = bIsProfilePicture ? -1 : 0;
	bool bHasAlpha = false;
	HImage* himg = ImageLoader::ConvertToBitmap(pData, nSize, bHasAlpha, false, nImSize, nImSize);

	if (himg)
	{
		if (himg->IsValid())
		{
			GetAvatarCache()->LoadedResource(additData);
			GetAvatarCache()->SetImage(additData, himg, bHasAlpha);
			OnUpdateAvatar(additData);
			// note: stole the resource so that the HImage destructor doesn't delete it.
		}
		else
		{
			delete himg;
		}
	}
	else
	{
		DbgPrintW("Failed to load convert image to bitmap! Unrecognized format?");
	}

	// store the cached data..
	std::string final_path = GetCachePath() + "\\" + additData;
	FILE* f = fopen(final_path.c_str(), "wb");
	if (!f) {
		DbgPrintW("ERROR: Could not open %s for writing", final_path.c_str());
		// TODO: error message
		return;
	}

	fwrite(pData, 1, nSize, f);
	fclose(f);

}

void Frontend_Win32::OnAttachmentFailed(bool bIsProfilePicture, const std::string& additData)
{
	if (bIsProfilePicture) {
		DbgPrintW("Could not load profile picture %s", additData.c_str());
		return;
	}

	GetAvatarCache()->LoadedResource(additData);
	GetAvatarCache()->SetImage(additData, HIMAGE_ERROR, false);
	OnUpdateAvatar(additData);
}

void Frontend_Win32::UpdateSelectedGuild()
{
	SendMessage(g_Hwnd, WM_UPDATESELECTEDGUILD, 0, 0);
}

void Frontend_Win32::UpdateSelectedChannel()
{
	SendMessage(g_Hwnd, WM_UPDATESELECTEDCHANNEL, 0, 0);
}

void Frontend_Win32::UpdateChannelList()
{
	SendMessage(g_Hwnd, WM_UPDATECHANLIST, 0, 0);
}

void Frontend_Win32::UpdateMemberList()
{
	SendMessage(g_Hwnd, WM_UPDATEMEMBERLIST, 0, 0);
}

void Frontend_Win32::UpdateChannelAcknowledge(Snowflake channelID, Snowflake messageID)
{
	Snowflake sfs[2];
	sfs[0] = channelID;
	sfs[1] = messageID;
	SendMessage(g_Hwnd, WM_UPDATECHANACKS, 0, (LPARAM) sfs);
}

void Frontend_Win32::UpdateProfileAvatar(Snowflake userID, const std::string& resid)
{
	UpdateProfileParams parms;
	parms.m_user = userID;
	parms.m_resId = resid;
	SendMessage(g_Hwnd, WM_UPDATEPROFILEAVATAR, 0, (LPARAM) &parms);
}

void Frontend_Win32::UpdateProfilePopout(Snowflake userID)
{
	if (ProfilePopout::GetUser() == userID)
		ProfilePopout::Update();
}

void Frontend_Win32::UpdateUserData(Snowflake userID)
{
	SendMessage(g_Hwnd, WM_UPDATEUSER, 0, (LPARAM) &userID);
}

void Frontend_Win32::UpdateAttachment(Snowflake attID)
{
	SendMessage(g_Hwnd, WM_UPDATEATTACHMENT, 0, (LPARAM) &attID);
}

void Frontend_Win32::RepaintGuildList()
{
	SendMessage(g_Hwnd, WM_REPAINTGUILDLIST, 0, 0);
}

void Frontend_Win32::RepaintProfile()
{
	SendMessage(g_Hwnd, WM_REPAINTPROFILE, 0, 0);
}

void Frontend_Win32::RepaintProfileWithUserID(Snowflake id)
{
	if (GetDiscordInstance()->GetUserID() == id)
		SendMessage(g_Hwnd, WM_REPAINTPROFILE, 0, 0);
}

void Frontend_Win32::RefreshMessages(ScrollDir::eScrollDir sd, Snowflake gapCulprit)
{
	SendMessage(g_Hwnd, WM_REFRESHMESSAGES, (WPARAM) sd, (LPARAM) &gapCulprit);
}

void Frontend_Win32::RefreshMembers(const std::set<Snowflake>& members)
{
	SendMessage(g_Hwnd, WM_REFRESHMEMBERS, 0, (LPARAM) &members);
}

void Frontend_Win32::JumpToMessage(Snowflake messageInCurrentChannel)
{
	SendMessage(g_Hwnd, WM_SENDTOMESSAGE, 0, (LPARAM) &messageInCurrentChannel);
}

void Frontend_Win32::OnWebsocketMessage(int gatewayID, const std::string& payload)
{
	WebsocketMessageParams* pParm = new WebsocketMessageParams;
	pParm->m_gatewayId = gatewayID;
	pParm->m_payload = payload;

	// N.B. The main window shall respond the message immediately with ReplyMessage
	SendMessage(g_Hwnd, WM_WEBSOCKETMESSAGE, 0, (LPARAM) pParm);
}

void Frontend_Win32::OnWebsocketClose(int gatewayID, int errorCode, const std::string& message)
{
	if (GetDiscordInstance()->GetGatewayID() == gatewayID)
		GetDiscordInstance()->GatewayClosed(errorCode);
	else if (GetQRCodeDialog()->GetGatewayID() == gatewayID)
		GetQRCodeDialog()->HandleGatewayClose(errorCode);
	else
		DbgPrintW("Unknown gateway connection %d closed: %d", gatewayID, errorCode);
}

#include <websocketpp/close.hpp>

void Frontend_Win32::OnWebsocketFail(int gatewayID, int errorCode, const std::string& message, bool isTLSError, bool mayRetry)
{
	static TCHAR buffer[8192];
	LPTSTR msg = ConvertCppStringToTString(message);
	WAsnprintf(
		buffer,
		_countof(buffer),
		TmGetTString(IDS_CANNOT_CONNECT_WS),
		errorCode,
		msg
	);
	free(msg);

	if (isTLSError) {
		_tcscat(buffer, TEXT("\n\n"));
		_tcscat(buffer, TmGetTString(IDS_SSL_ERROR_WS));
		_tcscat(buffer, TmGetTString(IDS_SSL_ERROR_2));
	}

	if (mayRetry) {
		_tcscat(buffer, TmGetTString(IDS_CANNOT_CONNECT_WS_2));
	}

	int flags = (mayRetry ? MB_RETRYCANCEL : MB_OK) | (isTLSError ? MB_ICONWARNING : MB_ICONERROR);
	int result = MessageBox(g_Hwnd, buffer, TmGetTString(IDS_PROGRAM_NAME), flags);
	
	if (mayRetry) {
		if (result == IDRETRY) {
			SendMessage(g_Hwnd, WM_CONNECTERROR, 0, 0);
		}
		else {
			SendMessage(g_Hwnd, WM_DESTROY, 0, 0);
			RequestQuit();
		}
	}
	else {
		SendMessage(g_Hwnd, WM_CONNECTERROR2, 0, 0);
	}
}

void Frontend_Win32::RequestQuit()
{
	PostQuitMessage(0);
	::WantQuit();
}

void Frontend_Win32::HideWindow()
{
	ShowWindow(g_Hwnd, SW_HIDE);
}

void Frontend_Win32::RestoreWindow()
{
	ShowWindow(g_Hwnd, SW_RESTORE);
}

void Frontend_Win32::MaximizeWindow()
{
	ShowWindow(g_Hwnd, SW_MAXIMIZE);
}

bool Frontend_Win32::IsWindowMinimized()
{
	return IsIconic(g_Hwnd);
}

#ifdef USE_DEBUG_PRINTS

void DbgPrintWV(const char* fmt, va_list vl)
{
	char pos[8192];
	vsnprintf(pos, sizeof pos, fmt, vl);
	pos[sizeof pos - 1] = 0;
	OutputDebugStringA(pos);
	OutputDebugStringA("\n");
}

void DbgPrintW(const char* fmt, ...)
{
	va_list vl;
	va_start(vl, fmt);
	DbgPrintWV(fmt, vl);
	va_end(vl);
}

void Frontend_Win32::DebugPrint(const char* fmt, va_list vl)
{
	DbgPrintWV(fmt, vl);
}

#endif

void Frontend_Win32::SetHeartbeatInterval(int timeMs)
{
	::SetHeartbeatInterval(timeMs);
}

void Frontend_Win32::LaunchURL(const std::string& url)
{
	::LaunchURL(url);
}

void Frontend_Win32::RegisterIcon(Snowflake sf, const std::string& avatarlnk)
{
	GetAvatarCache()->AddImagePlace(avatarlnk, eImagePlace::ICONS, avatarlnk, sf);
}

void Frontend_Win32::RegisterAvatar(Snowflake sf, const std::string& avatarlnk)
{
	GetAvatarCache()->AddImagePlace(avatarlnk, eImagePlace::AVATARS, avatarlnk, sf);
}

void Frontend_Win32::RegisterAttachment(Snowflake sf, const std::string& avatarlnk)
{
	GetAvatarCache()->AddImagePlace(avatarlnk, eImagePlace::ATTACHMENTS, avatarlnk, sf);
}

void Frontend_Win32::RegisterChannelIcon(Snowflake sf, const std::string& avatarlnk)
{
	GetAvatarCache()->AddImagePlace(avatarlnk, eImagePlace::CHANNEL_ICONS, avatarlnk, sf);
}

std::string Frontend_Win32::GetDirectMessagesText()
{
	return TmGetString(IDS_DIRECT_MESSAGES);
}

std::string Frontend_Win32::GetPleaseWaitText()
{
	return TmGetString(IDS_PLEASE_WAIT);
}

std::string Frontend_Win32::GetMonthName(int mon)
{
	return TmGetString(IDS_MONTH_JANUARY + mon);
}

std::string Frontend_Win32::GetTodayAtText()
{
	return TmGetString(IDS_TODAY_AT) + GetFormatTimestampTimeShort();
}

std::string Frontend_Win32::GetYesterdayAtText()
{
	return TmGetString(IDS_YESTERDAY_AT) + GetFormatTimestampTimeShort();
}

std::string Frontend_Win32::GetFormatDateOnlyText()
{
	return TmGetString(IDS_ONLY_DATE_FMT);
}

std::string Frontend_Win32::GetFormatTimeLongText()
{
	return TmGetString(IDS_LONG_DATE_FMT) + GetFormatTimestampTimeShort();
}

std::string Frontend_Win32::GetFormatTimeShortText()
{
	return TmGetString(IDS_SHORT_DATE_FMT) + GetFormatTimestampTimeShort();
}

std::string Frontend_Win32::GetFormatTimeShorterText()
{
	return GetFormatTimestampTimeShort();
}

std::string Frontend_Win32::GetFormatTimestampTimeShort()
{
	if (GetLocalSettings()->Use12HourTime())
		return "%I:%M %p";
	else
		return "%H:%M";
}

std::string Frontend_Win32::GetFormatTimestampTimeLong()
{
	return "%H:%M:%S";
}

std::string Frontend_Win32::GetFormatTimestampDateShort()
{
	return "%d/%m/%Y";
}

std::string Frontend_Win32::GetFormatTimestampDateLong()
{
	return "%e %B %Y";
}

std::string Frontend_Win32::GetFormatTimestampDateLongTimeShort()
{
	return GetFormatTimestampDateLong() + " " + GetFormatTimestampTimeShort();
}

std::string Frontend_Win32::GetFormatTimestampDateLongTimeLong()
{
	return "%A, " + GetFormatTimestampDateLong() + " " + GetFormatTimestampTimeShort();
}

int Frontend_Win32::GetMinimumWidth()
{
	return ScaleByDPI(600);
}

int Frontend_Win32::GetMinimumHeight()
{
	return ScaleByDPI(400);
}

int Frontend_Win32::GetDefaultWidth()
{
	return ScaleByDPI(1000);
}

int Frontend_Win32::GetDefaultHeight()
{
	return ScaleByDPI(700);
}

bool Frontend_Win32::UseGradientByDefault()
{
	// Are we using Windows 5.00 at least? (2000)
	return LOWORD(GetVersion()) >= 5;
}
