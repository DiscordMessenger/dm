#pragma once

#include <set>
#include <cstdarg>
#include <nlohmann/json.h>
#include <protobuf/Protobuf.hpp>
#include "Snowflake.hpp"
#include "ScrollDir.hpp"
#include "Message.hpp"
#include "HTTPClient.hpp"
#include "Util.hpp"

// Cross platform interface between the DiscordInstance and the actual front-end.
class Frontend
{
public:
	Frontend() {}
	virtual ~Frontend() {}

	// Events
	virtual void OnLoginAgain() = 0;
	virtual void OnLoggedOut() = 0;
	virtual void OnSessionClosed(int errorCode) = 0;
	virtual void OnConnecting() = 0;
	virtual void OnConnected() = 0;
	virtual void OnAddMessage(Snowflake channelID, const Message& msg) = 0;
	virtual void OnUpdateMessage(Snowflake channelID, const Message& msg) = 0;
	virtual void OnDeleteMessage(Snowflake messageInCurrentChannel) = 0;
	virtual void OnStartTyping(Snowflake userID, Snowflake guildID, Snowflake channelID, time_t startTime) = 0;
	virtual void OnAttachmentDownloaded(bool bIsProfilePicture, const uint8_t* pData, size_t nSize, const std::string& additData) = 0;
	virtual void OnAttachmentFailed(bool bIsProfilePicture, const std::string& additData) = 0;
	virtual void OnRequestDone(NetRequest* pRequest) = 0;
	virtual void OnLoadedPins(Snowflake channel, const std::string& data) = 0;
	virtual void OnUpdateAvailable(const std::string& url, const std::string& version) = 0;
	virtual void OnFailedToSendMessage(Snowflake channel, Snowflake message) = 0;
	virtual void OnFailedToUploadFile(const std::string& file, int error) = 0;
	virtual void OnFailedToCheckForUpdates(int result, const std::string& response) = 0;

	// Error messages
	virtual void OnGenericError(const std::string& message) = 0;
	virtual void OnJsonException(const std::string& message) = 0;
	virtual void OnCantViewChannel(const std::string& channelName) = 0;
	virtual void OnGatewayConnectFailure() = 0;
	virtual void OnProtobufError(Protobuf::ErrorCode code) = 0;

	// Update requests
	virtual void UpdateSelectedGuild() = 0;
	virtual void UpdateSelectedChannel() = 0;
	virtual void UpdateChannelList() = 0;
	virtual void UpdateMemberList() = 0;
	virtual void UpdateChannelAcknowledge(Snowflake channelID, Snowflake messageID) = 0;
	virtual void UpdateProfileAvatar(Snowflake userID, const std::string& resid) = 0;
	virtual void UpdateProfilePopout(Snowflake userID) = 0; // <-- Updates if userID is the ID of the profile currently open
	virtual void UpdateUserData(Snowflake userID) = 0;
	virtual void UpdateAttachment(Snowflake attID) = 0;
	virtual void RepaintGuildList() = 0;
	virtual void RepaintProfile() = 0;
	virtual void RepaintProfileWithUserID(Snowflake id) = 0;
	virtual void RefreshMessages(ScrollDir::eScrollDir sd, Snowflake gapCulprit) = 0;
	virtual void RefreshMembers(const std::set<Snowflake>& members) = 0;

	// Interactive requests
	virtual void JumpToMessage(Snowflake messageInCurrentChannel) = 0;
	virtual void LaunchURL(const std::string& url) = 0;

	// Called by WebSocketClient, dispatches to relevant places including DiscordInstance
	virtual void OnWebsocketMessage(int gatewayID, const std::string& payload) = 0;
	virtual void OnWebsocketClose(int gatewayID, int errorCode, const std::string& message) = 0;
	virtual void OnWebsocketFail(int gatewayID, int errorCode, const std::string& message, bool isTLSError, bool mayRetry) = 0;

	// Heartbeat interval
	virtual void SetHeartbeatInterval(int timeMs) = 0;

	// Interface with AvatarCache
	virtual void RegisterIcon(Snowflake sf, const std::string& avatarlnk) = 0;
	virtual void RegisterAvatar(Snowflake sf, const std::string& avatarlnk) = 0;
	virtual void RegisterAttachment(Snowflake sf, const std::string& avatarlnk) = 0;
	virtual void RegisterChannelIcon(Snowflake sf, const std::string& avatarlnk) = 0;

	// Quit
	virtual void RequestQuit() = 0;

	// User stuff
	virtual void GetIdentifyProperties(nlohmann::json& j) = 0;
	virtual std::string GetUserAgent() = 0;

	// Strings
	virtual std::string GetDirectMessagesText() = 0;
	virtual std::string GetPleaseWaitText() = 0;
	virtual std::string GetMonthName(int index) = 0;
	virtual std::string GetTodayAtText() = 0;
	virtual std::string GetYesterdayAtText() = 0;
	virtual std::string GetFormatDateOnlyText() = 0;
	virtual std::string GetFormatTimeLongText() = 0;
	virtual std::string GetFormatTimeShortText() = 0;
	virtual std::string GetFormatTimeShorterText() = 0;

	// Debugging
#ifdef USE_DEBUG_PRINTS
	virtual void DebugPrint(const char* fmt, va_list vl) = 0;
#endif
};

// Defined in the specific platform that this application is compiled for.
Frontend* GetFrontend();
