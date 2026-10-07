#pragma once

#include <string>
#include "Frontend.hpp"

// What every Unix frontend shares: requests and gateway traffic handed to
// the UI thread (MainQueue), the session start-up, settings files, texts and
// date formats.  A frontend derives from this and overrides what it shows.
//
// Calls that arrive on network threads (requests done, gateway messages,
// websocket failures, errors, progress) are moved to the UI thread here or
// in the hooks' documentation below; everything else is called by the
// DiscordInstance, which runs on the UI thread.
class Frontend_Posix : public Frontend
{
public:
	// Starts (or restarts) the session: fetches the gateway URL if needed,
	// then connects.  UI thread.
	void StartSession();

	void OnLoginAgain() override;
	void OnLoggedOut() override {}
	void OnSessionClosed(int errorCode) override {}
	void OnConnecting() override {}
	void OnConnected() override {}
	void OnAddMessage(Snowflake channelID, const Message& msg) override;
	void OnUpdateMessage(Snowflake channelID, const Message& msg) override;
	void OnDeleteMessage(Snowflake messageInCurrentChannel) override {}
	void OnStartTyping(Snowflake userID, Snowflake guildID, Snowflake channelID, time_t startTime) override {}
	void OnAttachmentDownloaded(bool bIsProfilePicture, const uint8_t* pData, size_t nSize, const std::string& additData) override {}
	void OnAttachmentFailed(bool bIsProfilePicture, const std::string& additData) override {}
	void OnRequestDone(NetRequest* pRequest) override;
	void OnLoadedPins(Snowflake channel, const std::string& data) override {}
	void OnUpdateAvailable(const std::string& url, const std::string& version) override {}
	void OnFailedToSendMessage(Snowflake channel, Snowflake message) override {}
	void OnFailedToUploadFile(const std::string& file, int error) override;
	void OnFailedToCheckForUpdates(int result, const std::string& response) override {}
	void OnStartProgress(Snowflake key, const std::string& fileName, bool isUploading) override {}
	bool OnUpdateProgress(Snowflake key, size_t offset, size_t length) override { return true; }
	void OnStopProgress(Snowflake key) override {}
	void OnNotification() override {}
	void OnGenericError(const std::string& message) override;
	void OnJsonException(const std::string& message) override;
	void OnCantViewChannel(const std::string& channelName) override;
	void OnGatewayConnectFailure() override;
	void OnProtobufError(Protobuf::ErrorCode code) override;
	void UpdateSelectedGuild() override {}
	void UpdateSelectedChannel() override {}
	void UpdateChannelList() override {}
	void UpdateMemberList() override {}
	void UpdateChannelAcknowledge(Snowflake channelID, Snowflake messageID) override {}
	void UpdateProfileAvatar(Snowflake userID, const std::string& resid) override {}
	void UpdateProfilePopout(Snowflake userID) override {}
	void UpdateUserData(Snowflake userID) override {}
	void UpdateAttachment(Snowflake attID) override {}
	void RepaintGuildList() override {}
	void RepaintProfile() override {}
	void RepaintProfileWithUserID(Snowflake id) override {}
	void RefreshMessages(ScrollDir::eScrollDir sd, Snowflake gapCulprit) override {}
	void RefreshMembers(const std::set<Snowflake>& members) override {}
	void JumpToMessage(Snowflake messageInCurrentChannel) override {}
	void LaunchURL(const std::string& url) override;
	void OnWebsocketMessage(int gatewayID, const std::string& payload) override;
	void OnWebsocketClose(int gatewayID, int errorCode, const std::string& message) override;
	void OnWebsocketFail(int gatewayID, int errorCode, const std::string& message, bool isTLSError, bool mayRetry) override;
	void RegisterIcon(Snowflake sf, const std::string& avatarlnk) override {}
	void RegisterAvatar(Snowflake sf, const std::string& avatarlnk) override {}
	void RegisterAttachment(Snowflake sf, const std::string& avatarlnk) override {}
	void RegisterChannelIcon(Snowflake sf, const std::string& avatarlnk) override {}
	std::string LoadConfig() override;
	bool SaveConfig(const std::string& configJson) override;
	bool IsWindowMinimized() override { return false; }
	bool IsWindowFocused() override { return true; }
	std::string GetDirectMessagesText() override;
	std::string GetPleaseWaitText() override;
	std::string GetMonthName(int index) override;
	std::string GetTodayAtText() override;
	std::string GetYesterdayAtText() override;
	std::string GetFormatDateOnlyText() override;
	std::string GetFormatTimeLongText() override;
	std::string GetFormatTimeShortText() override;
	std::string GetFormatTimeShorterText() override;
	std::string GetFormatTimestampTimeShort() override;
	std::string GetFormatTimestampTimeLong() override;
	std::string GetFormatTimestampDateShort() override;
	std::string GetFormatTimestampDateLong() override;
	std::string GetFormatTimestampDateLongTimeShort() override;
	std::string GetFormatTimestampDateLongTimeLong() override;
	void HideWindow() override {}
	void RestoreWindow() override {}
	void MaximizeWindow() override {}
	int GetMinimumWidth() override { return 600; }
	int GetMinimumHeight() override { return 400; }
	int GetDefaultWidth() override { return 1000; }
	int GetDefaultHeight() override { return 700; }
#ifdef USE_DEBUG_PRINTS
	void DebugPrint(const char* fmt, va_list vl) override;
#endif
	bool UseGradientByDefault() override { return false; }

protected:
	// Shows an error or a notice to the user.  UI thread.
	virtual void ShowError(const std::string& message) = 0;

	// The gateway connection failed; mayRetry says whether trying again can
	// help.  The default reconnects after a growing delay (ScheduleRetry).
	// UI thread.
	virtual void OnConnectFailed(const std::string& message, bool isTLSError, bool mayRetry);

	// Calls StartSession after ms milliseconds.  UI thread.
	virtual void ScheduleReconnect(int ms) = 0;

	int m_retryDelayMs = 1000;
};

// Sets up the settings directory ($HOME/.discordmessenger, or DM_HOME) and
// its cache directory.  Call before loading settings.
void SetupPosixPaths();
