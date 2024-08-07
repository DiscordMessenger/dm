#pragma once

#include <string>
#include <set>
#include <ctime>

enum eMessageStyle
{
	MS_3DFACE,
	MS_FLAT,
	MS_GRADIENT,
	MS_FLATBR,
	MS_IMAGE,

	MS_MAX,
};

enum eImageAlignment
{
	ALIGN_LOWER_RIGHT,
	ALIGN_UPPER_LEFT,
	ALIGN_CENTER,
	ALIGN_UPPER_RIGHT,
	ALIGN_LOWER_LEFT,
	ALIGN_UPPER_CENTER,
	ALIGN_LOWER_CENTER,
	ALIGN_MIDDLE_LEFT,
	ALIGN_MIDDLE_RIGHT,
	ALIGN_COUNT,
};

class LocalSettings
{
public:
	LocalSettings();

	bool Load();
	bool Save();
	std::string GetToken() const {
		return m_token;
	}
	void SetToken(const std::string& str) {
		m_token = str;
	}
	eMessageStyle GetMessageStyle() const {
		return m_messageStyle;
	}
	void SetMessageStyle(eMessageStyle ms) {
		m_messageStyle = ms;
	}
	bool CheckTrustedDomain(const std::string& url);
	
	bool ReplyMentionByDefault() const {
		return m_bReplyMentionDefault;
	}
	void SetReplyMentionByDefault(bool b) {
		m_bReplyMentionDefault = b;
	}
	void SetWindowSize(int w, int h) {
		m_width = w; m_height = h;
	}
	void GetWindowSize(int& w, int& h) {
		w = m_width; h = m_height;
	}
	void SetSaveWindowSize(bool b) {
		m_bSaveWindowSize = b;
	}
	bool GetSaveWindowSize() const {
		return m_bSaveWindowSize;
	}
	void SetStartMaximized(bool b) {
		m_bStartMaximized = b;
	}
	bool GetStartMaximized() const {
		return m_bStartMaximized;
	}
	void SetOpenOnStartup(bool b) {
		m_bOpenOnStartup = b;
	}
	bool GetOpenOnStartup() const {
		return m_bOpenOnStartup;
	}
	void SetStartMinimized(bool b) {
		m_bStartMinimized = b;
	}
	bool GetStartMinimized() const {
		return m_bStartMinimized;
	}
	void SetMinimizeToNotif(bool b) {
		m_bMinimizeToNotif = b;
	}
	bool GetMinimizeToNotif() const {
		return m_bMinimizeToNotif;
	}
	void SetMaximized(bool b) {
		m_bMaximized = b;
	}
	bool GetMaximized() const {
		return m_bMaximized;
	}
	bool IsFirstStart() const {
		return m_bIsFirstStart;
	}
	const std::string& GetDiscordAPI() const {
		return m_discordApi;
	}
	void SetDiscordAPI(const std::string& str) {
		m_discordApi = str;
	}
	const std::string& GetDiscordCDN() const {
		return m_discordCdn;
	}
	void SetDiscordCDN(const std::string& str) {
		m_discordCdn = str;
	}
	void SetCheckUpdates(bool b) {
		m_bCheckUpdates = b;
		m_bAskToCheckUpdates = false;
	}
	bool CheckUpdates() const {
		if (!m_bCheckUpdates)
			return false;

		return time(NULL) >= m_remindUpdatesOn;
	}
	bool CheckUpdatesOption() const {
		return m_bCheckUpdates;
	}
	bool AskToCheckUpdates() const {
		return m_bAskToCheckUpdates;
	}
	bool EnableTLSVerification() const {
		return m_bEnableTLSVerification;
	}
	void SetEnableTLSVerification(bool b) {
		m_bEnableTLSVerification = b;
	}
	bool AddExtraHeaders() const {
		return m_bAddExtraHeaders;
	}
	void SetAddExtraHeaders(bool b) {
		m_bAddExtraHeaders = b;
	}
	void StopUpdateCheckTemporarily();
	bool DisableFormatting() const {
		return m_bDisableFormatting;
	}
	void SetDisableFormatting(bool b) {
		m_bDisableFormatting = b;
	}
	bool ShowScrollBarOnGuildList() const {
		return m_bShowScrollBarOnGuildList;
	}
	void SetShowScrollBarOnGuildList(bool b) {
		m_bShowScrollBarOnGuildList = b;
	}
	const std::string& GetImageBackgroundFileName() const {
		return m_imageBackgroundFileName;
	}
	void SetImageBackgroundFileName(const std::string& fn) {
		m_imageBackgroundFileName = fn;
	}
	eImageAlignment GetImageAlignment() const {
		return m_imageAlignment;
	}
	void SetImageAlignment(eImageAlignment align) {
		m_imageAlignment = align;
	}
	int GetUserScale() const {
		return m_userScale;
	}
	void SetUserScale(int userScale) {
		m_userScale = userScale;
	}
	bool GetCompactMemberList() const {
		return m_bCompactMemberList;
	}
	void SetCompactMemberList(bool b) {
		m_bCompactMemberList = b;
	}
	bool ShowAttachmentImages() const {
		return m_bShowAttachmentImages;
	}
	bool ShowEmbedImages() const {
		return m_bShowEmbedImages;
	}
	bool ShowEmbedContent() const {
		return m_bShowEmbedContent;
	}
	void SetShowAttachmentImages(bool b) {
		m_bShowAttachmentImages = b;
	}
	void SetShowEmbedImages(bool b) {
		m_bShowEmbedImages = b;
	}
	void SetShowEmbedContent(bool b) {
		m_bShowEmbedContent = b;
	}
	bool EnableNotifications() const {
		return m_bEnableNotifications;
	}
	bool FlashOnNotification() const {
		return m_bFlashOnNotification;
	}
	void SetEnableNotifications(bool b) {
		m_bEnableNotifications = b;
	}
	void SetFlashOnNotification(bool b) {
		m_bFlashOnNotification = b;
	}

private:
	std::string m_token;
	std::string m_discordApi;
	std::string m_discordCdn;
	std::set<std::string> m_trustedDomains;
	eMessageStyle m_messageStyle = MS_GRADIENT;
	eImageAlignment m_imageAlignment = ALIGN_LOWER_RIGHT;
	std::string m_imageBackgroundFileName = "";
	bool m_bReplyMentionDefault = true;
	bool m_bSaveWindowSize = false;
	bool m_bStartMaximized = false;
	bool m_bOpenOnStartup = false;
	bool m_bStartMinimized = false;
	bool m_bMinimizeToNotif = true;
	bool m_bMaximized = false;
	bool m_bIsFirstStart = false;
	bool m_bCheckUpdates = false;
	bool m_bAskToCheckUpdates = true;
	bool m_bEnableTLSVerification = true;
	bool m_bDisableFormatting = false;
	bool m_bShowScrollBarOnGuildList = false;
	bool m_bCompactMemberList = false;
	bool m_bAddExtraHeaders = true;
	bool m_bShowAttachmentImages = true;
	bool m_bShowEmbedImages = true;
	bool m_bShowEmbedContent = true;
	bool m_bEnableNotifications = true;
	bool m_bFlashOnNotification = true;
	time_t m_remindUpdatesOn = 0;
	int m_width = 1000;
	int m_height = 700;
	int m_userScale = 1000;
};

LocalSettings* GetLocalSettings();
