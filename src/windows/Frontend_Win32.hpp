#pragma once

#include "../discord/Frontend.hpp"

class Frontend_Win32 : public Frontend
{
public:
	Frontend_Win32() {}
	~Frontend_Win32() {}

	void OnLoginAgain() override;
	void OnLoggedOut() override;
	void OnSessionClosed(int errorCode) override;
	void OnConnecting() override;
	void OnConnected() override;
	void OnAddMessage(Snowflake channelID, const Message& msg) override;
	void OnUpdateMessage(Snowflake channelID, const Message& msg) override;
	void OnDeleteMessage(Snowflake messageInCurrentChannel) override;
	void OnStartTyping(Snowflake userID, Snowflake guildID, Snowflake channelID, time_t startTime) override;
	void OnRequestDone(NetRequest* pRequest) override;
	void OnLoadedPins(Snowflake channel, const std::string& data) override;
	void OnFailedToSendMessage(Snowflake channel, Snowflake message) override;
	void OnFailedToUploadFile(const std::string& file, int error) override;
	void OnGenericError(const std::string& message) override;
	void OnJsonException(const std::string& message) override;
	void OnCantViewChannel(const std::string& channelName) override;
	void OnGatewayConnectFailure() override;
	void OnProtobufError(Protobuf::ErrorCode code) override;
	void OnAttachmentDownloaded(bool bIsProfilePicture, const uint8_t* pData, size_t nSize, const std::string& additData) override;
	void OnAttachmentFailed(bool bIsProfilePicture, const std::string& additData) override;
	void UpdateSelectedGuild() override;
	void UpdateSelectedChannel() override;
	void UpdateChannelList() override;
	void UpdateMemberList() override;
	void UpdateChannelAcknowledge(Snowflake channelID) override;
	void UpdateProfileAvatar(Snowflake userID, const std::string& resid) override;
	void UpdateProfilePopout(Snowflake userID) override;
	void UpdateAttachment(Snowflake attID) override;
	void RepaintGuildList() override;
	void RepaintProfile() override;
	void RepaintProfileWithUserID(Snowflake id) override;
	void RefreshMessages(ScrollDir::eScrollDir sd, Snowflake gapCulprit) override;
	void RefreshMembers(const std::set<Snowflake>& members) override;
	void JumpToMessage(Snowflake messageInCurrentChannel) override;
	void OnWebsocketMessage(int gatewayID, const std::string& payload) override;
	void OnWebsocketClose(int gatewayID, int errorCode) override;
	void OnWebsocketFail(int gatewayID, int errorCode) override;
	void SetHeartbeatInterval(int timeMs) override;
	void LaunchURL(const std::string& url) override;
	void RegisterIcon(Snowflake sf, const std::string& avatarlnk) override;
	void RegisterAvatar(Snowflake sf, const std::string& avatarlnk) override;
	void RegisterAttachment(Snowflake sf, const std::string& avatarlnk) override;
	void RequestQuit() override;
	void GetIdentifyProperties(nlohmann::json& j) override;
	std::string GetUserAgent() override;
	std::string GetDirectMessagesText() override;
	std::string GetPleaseWaitText() override;
	std::string GetMonthName(int index) override;
	std::string GetTodayAtText() override;
	std::string GetYesterdayAtText() override;
	std::string GetFormatDateOnlyText() override;
	std::string GetFormatTimeLongText() override;
	std::string GetFormatTimeShortText() override;
	std::string GetFormatTimeShorterText() override;

#ifdef _DEBUG
	void DebugPrint(const char* fmt, va_list vl) override;
#endif
};
