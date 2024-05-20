#include "Frontend_Win32.hpp"

#include "Main.hpp"
#include "ImageLoader.hpp"
#include "ProfilePopout.hpp"
#include "QRCodeDialog.hpp"
#include "PinnedMessageViewer.hpp"
#include "../discord/UpdateChecker.hpp"
#include "../discord/LocalSettings.hpp"

const std::string g_UserAgent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) discord/1.0.284 Chrome/120.0.6099.283 Electron/28.2.3 Safari/537.36";

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
	if (g_latestSSLError) {
		char buff[128];
		snprintf(buff, sizeof buff, "\n\nAdditionally, an SSL error code of 0x%x was provided.  Consider sending this to iProgramInCpp!", g_latestSSLError);
		newMsg += std::string(buff);
		g_latestSSLError = 0;
	}

	LPCTSTR pMsgBoxText = ConvertCppStringToTString(newMsg);
	MessageBox(g_Hwnd, pMsgBoxText, TEXT("Discord Messenger Error"), MB_OK | MB_ICONERROR);
	free((void*)pMsgBoxText);
	pMsgBoxText = NULL;
}

void Frontend_Win32::OnJsonException(const std::string& message)
{
	std::string err("Json Exception!\n\nMessage: ");
	err += message;
	LPCTSTR pMsgBoxText = ConvertCppStringToTString(err);
	MessageBox(g_Hwnd, pMsgBoxText, TEXT("Discord Messenger Error"), MB_OK | MB_ICONERROR);
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
	snprintf(buff, sizeof buff - 1, "Error while loading settings data, got error code %d from protobuf implementation.", code);
	MessageBoxA(g_Hwnd, buff, "Settings error", MB_ICONERROR);
}

void Frontend_Win32::OnRequestDone(NetRequest* pRequest)
{
	SendMessage(g_Hwnd, WM_REQUESTDONE, 0, (LPARAM) pRequest);
}

void Frontend_Win32::OnLoadedPins(Snowflake channel, const std::string& data)
{
	if (PmvIsActive())
		PmvOnLoadedPins(channel, data);
}

void Frontend_Win32::OnUpdateAvailable(const std::string& url, const std::string& version)
{
	TCHAR buff[2048];
	LPTSTR tstr1 = ConvertCppStringToTString(GetAppVersionString());
	LPTSTR tstr2 = ConvertCppStringToTString(version);
	WAsnprintf(buff, _countof(buff), TmGetTString(IDS_NEW_VERSION_AVAILABLE), tstr1, tstr2);
	free(tstr1);
	free(tstr2);

	if (MessageBox(g_Hwnd, buff, TmGetTString(IDS_PROGRAM_NAME), MB_ICONINFORMATION | MB_YESNO) == IDYES)
	{
		size_t idx = 0, idxsave = 0;
		for (; idx != url.size(); idx++) {
			if (url[idx] == '/')
				idxsave = idx + 1;
		}

		DownloadFileDialog(g_Hwnd, url, url.substr(idxsave));
	}

	GetLocalSettings()->StopUpdateCheckTemporarily();
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

void Frontend_Win32::OnAttachmentDownloaded(bool bIsProfilePicture, const uint8_t* pData, size_t nSize, const std::string& additData)
{
	int nImSize = bIsProfilePicture ? -1 : 0;

	HBITMAP bmp = ImageLoader::ConvertToBitmap(pData, nSize, nImSize, nImSize);
	if (bmp)
	{
		GetAvatarCache()->LoadedResource(additData);
		GetAvatarCache()->SetBitmap(additData, bmp);
		OnUpdateAvatar(additData);
	}

	// store the cached data..
	std::string final_path = GetCachePath() + "\\" + additData;
	FILE* f = fopen(final_path.c_str(), "wb");
	if (!f) {
		DbgPrintW("ERROR: Could not open %s for writing", final_path.c_str());
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
	GetAvatarCache()->SetBitmap(additData, HBITMAP_ERROR);
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

void Frontend_Win32::UpdateChannelAcknowledge(Snowflake channelID)
{
	SendMessage(g_Hwnd, WM_UPDATECHANACKS, 0, (LPARAM) &channelID);
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
	if (GetProfilePopoutUser() == userID)
		::UpdateProfilePopout();
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

void Frontend_Win32::OnWebsocketFail(int gatewayID, int errorCode, const std::string& message, bool isTLSError)
{
	static TCHAR buffer[8192];
	LPTSTR msg = ConvertCppStringToTString(message);
	WAsnprintf(
		buffer,
		_countof(buffer),
		TEXT("ERROR: Could not connect to the websocket gateway.\nConnection was closed with code: %d\n\nMessage: %s"),
		errorCode,
		msg
	);

	if (isTLSError) {
		_tcscat(
			buffer,
			TEXT("\n\nWARNING: Your connection may not be private\n\nDiscord Messenger could not verify that it is connecting to a Websocket service ")
			TEXT("required to use Discord Messenger in real time.")
			TEXT("\n\nThis could be because:")
			TEXT("\no   Your computer does not trust the certificate that the gateway has declared because it is wrong,")
			TEXT("\no   Your computer does not trust the certificate that the gateway has declared because it doesn't trust the root certificate,")
			TEXT("\no   Your computer does not trust the certificate that the gateway has declared because your date and time are incorrect,")
			TEXT("\no   The website reported a protocol which the client is outdated for and cannot handle, or")
			TEXT("\no   SOMEONE IS EAVESDROPPING ON YOU RIGHT NOW! (man-in-the-middle attack)")
			TEXT("\n\nIt is not recommended to proceed if you see this error. Please ensure you can connect to Discord using a ")
			TEXT("capable device on the same network as this one.  If you are able to connect, export the root certificate from ")
			TEXT("discord.com on that device and import it into Windows, if the OS supports it.")
			TEXT("\n\nYou may also disable SSL server verification, and then restart the application. However, you have to ")
			TEXT("take into account the risks that this entails.")
		);
	}

	free(msg);

	MessageBox(g_Hwnd, buffer, TmGetTString(IDS_PROGRAM_NAME), (isTLSError ? MB_ICONWARNING : MB_ICONERROR) | MB_OK);
	SendMessage(g_Hwnd, WM_CONNECTERROR, 0, 0);
}

void Frontend_Win32::RequestQuit()
{
	PostQuitMessage(0);
	::WantQuit();
}

void Frontend_Win32::GetIdentifyProperties(nlohmann::json& j)
{
	j["app_arch"] = "x64";
	j["browser"] = "Discord Client";
	j["browser_user_agent"] = g_UserAgent;
	j["browser_version"] = "28.2.3";
	j["client_build_number"] = 269579;
	j["client_event_source"] = nullptr;
	j["client_version"] = "1.0.284";
	j["native_build_number"] = 44304;
	j["os"] = "Windows";
	j["os_arch"] = "x64";
	j["os_version"] = "10.0.19044";
	j["release_channel"] = "canary";
	j["system_locale"] = "en-US";
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

std::string Frontend_Win32::GetUserAgent()
{
	return g_UserAgent;
}

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
	return TmGetString(IDS_TODAY_AT);
}

std::string Frontend_Win32::GetYesterdayAtText()
{
	return TmGetString(IDS_YESTERDAY_AT);
}

std::string Frontend_Win32::GetFormatDateOnlyText()
{
	return TmGetString(IDS_ONLY_DATE_FMT);
}

std::string Frontend_Win32::GetFormatTimeLongText()
{
	return TmGetString(IDS_LONG_DATE_FMT);
}

std::string Frontend_Win32::GetFormatTimeShortText()
{
	return TmGetString(IDS_SHORT_DATE_FMT);
}

std::string Frontend_Win32::GetFormatTimeShorterText()
{
	return TmGetString(IDS_SHORT_TIME_FMT);
}
