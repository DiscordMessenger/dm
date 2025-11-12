#include <fstream>
#include <nlohmann/json.h>
#include "LocalSettings.hpp"
#include "../utils/Util.hpp"
#include "../network/DiscordAPI.hpp"
#include "../Frontend.hpp"
using nlohmann::json;

static LocalSettings* g_pInstance;

LocalSettings* GetLocalSettings()
{
	if (!g_pInstance)
		g_pInstance = new LocalSettings;

	return g_pInstance;
}

LocalSettings::LocalSettings()
{
	// Add some default trusted domains:
	m_trustedDomains.insert("discord.gg");
	m_trustedDomains.insert("discord.com");
	m_trustedDomains.insert("discordapp.com");
	m_trustedDomains.insert("canary.discord.com");
	m_trustedDomains.insert("canary.discordapp.com");
	m_trustedDomains.insert("cdn.discord.com");
	m_trustedDomains.insert("cdn.discordapp.com");
	m_trustedDomains.insert("media.discordapp.net");

	m_discordApi = OFFICIAL_DISCORD_API;
	m_discordCdn = OFFICIAL_DISCORD_CDN;
}

bool LocalSettings::Load()
{
	m_bIsFirstStart = false;

	std::string data = GetFrontend()->LoadConfig();

	if (data.empty()) {
		m_bIsFirstStart = true;
		return false;
	}

	if (GetFrontend()->UseGradientByDefault())
		m_messageStyle = MS_GRADIENT;
	else
		m_messageStyle = MS_3DFACE;

	json j = json::parse(data);

	// Load properties from the json object.
	if (j.contains("Token"))
		m_token = j["Token"];

	if (j.contains("DiscordAPI"))
		m_discordApi = j["DiscordAPI"];

	if (j.contains("MessageStyle"))
		m_messageStyle = eMessageStyle(int(j["MessageStyle"]));

	if (j.contains("TrustedDomains")) {
		for (auto& dom : j["TrustedDomains"])
			m_trustedDomains.insert(dom);
	}

	if (j.contains("ReplyMentionDefault"))
		m_bReplyMentionDefault = j["ReplyMentionDefault"];

	if (j.contains("SaveWindowSize"))
		m_bSaveWindowSize = j["SaveWindowSize"];

	if (j.contains("StartMaximized"))
		m_bStartMaximized = j["StartMaximized"];

	if (j.contains("OpenOnStartup"))
		m_bOpenOnStartup = j["OpenOnStartup"];

	if (j.contains("StartMinimized"))
		m_bStartMinimized = j["StartMinimized"];

	if (j.contains("MinimizeToNotif"))
		m_bMinimizeToNotif = j["MinimizeToNotif"];

	if (j.contains("Maximized"))
		m_bMaximized = j["Maximized"];

	if (j.contains("EnableTLSVerification"))
		m_bEnableTLSVerification = j["EnableTLSVerification"];

	if (j.contains("DisableFormatting"))
		m_bDisableFormatting = j["DisableFormatting"];

	if (j.contains("ShowScrollBarOnGuildList"))
		m_bShowScrollBarOnGuildList = j["ShowScrollBarOnGuildList"];

	if (j.contains("CompactMemberList"))
		m_bCompactMemberList = j["CompactMemberList"];

	if (j.contains("ImageBackgroundFileName"))
		m_imageBackgroundFileName = j["ImageBackgroundFileName"];

	if (j.contains("WatermarkAlignment"))
		m_imageAlignment = eImageAlignment(int(j["WatermarkAlignment"]));

	if (j.contains("UserScale"))
		m_userScale = int(j["UserScale"]);

	if (j.contains("ShowAttachmentImages"))
		m_bShowAttachmentImages = j["ShowAttachmentImages"];

	if (j.contains("ShowEmbedImages"))
		m_bShowEmbedImages = j["ShowEmbedImages"];

	if (j.contains("ShowEmbedContent"))
		m_bShowEmbedContent = j["ShowEmbedContent"];

	if (j.contains("EnableNotifications"))
		m_bEnableNotifications = j["EnableNotifications"];

	if (j.contains("FlashOnNotification"))
		m_bFlashOnNotification = j["FlashOnNotification"];

	if (j.contains("Use12HourTime"))
		m_bUse12HourTime = j["Use12HourTime"];

	if (j.contains("ShowBlockedMessages"))
		m_bShowBlockedMessages = j["ShowBlockedMessages"];

	if (m_bSaveWindowSize)
	{
		if (j.contains("WindowWidth"))
			m_width = j["WindowWidth"];

		if (j.contains("WindowHeight"))
			m_height = j["WindowHeight"];

		if (m_width < GetFrontend()->GetMinimumWidth())
			m_width = GetFrontend()->GetMinimumWidth();
		if (m_height < GetFrontend()->GetMinimumHeight())
			m_height = GetFrontend()->GetMinimumHeight();
	}
	else
	{
		m_width = GetFrontend()->GetDefaultWidth();
		m_height = GetFrontend()->GetDefaultHeight();
	}

	if (j.contains("CheckUpdates")) {
		m_bCheckUpdates = j["CheckUpdates"];
		m_bAskToCheckUpdates = false;
	}
	else {
		m_bAskToCheckUpdates = true;
	}

	if (j.contains("RemindUpdateCheckOn"))
		m_remindUpdatesOn = (time_t) (long long) j["RemindUpdateCheckOn"];

	if (j.contains("AddExtraHeaders"))
		m_bAddExtraHeaders = j["AddExtraHeaders"];
	return true;
}

bool LocalSettings::Save()
{
	json j;
	json trustedDomains;

	for (auto& dom : m_trustedDomains)
		trustedDomains.push_back(dom);

	j["Token"] = m_token;
	j["DiscordAPI"] = m_discordApi;
	j["MessageStyle"] = int(m_messageStyle);
	j["TrustedDomains"] = trustedDomains;
	j["ReplyMentionDefault"] = m_bReplyMentionDefault;
	j["StartMaximized"] = m_bStartMaximized;
	j["SaveWindowSize"] = m_bSaveWindowSize;
	j["OpenOnStartup"] = m_bOpenOnStartup;
	j["StartMinimized"] = m_bStartMinimized;
	j["MinimizeToNotif"] = m_bMinimizeToNotif;
	j["Maximized"] = m_bMaximized;
	j["CheckUpdates"] = m_bCheckUpdates;
	j["EnableTLSVerification"] = m_bEnableTLSVerification;
	j["DisableFormatting"] = m_bDisableFormatting;
	j["ShowScrollBarOnGuildList"] = m_bShowScrollBarOnGuildList;
	j["CompactMemberList"] = m_bCompactMemberList;
	j["RemindUpdateCheckOn"] = (long long)(m_remindUpdatesOn);
	j["ImageBackgroundFileName"] = m_imageBackgroundFileName;
	j["WatermarkAlignment"] = int(m_imageAlignment);
	j["UserScale"] = m_userScale;
	j["AddExtraHeaders"] = m_bAddExtraHeaders;
	j["ShowAttachmentImages"] = m_bShowAttachmentImages;
	j["ShowEmbedImages"] = m_bShowEmbedImages;
	j["ShowEmbedContent"] = m_bShowEmbedContent;
	j["EnableNotifications"] = m_bEnableNotifications;
	j["FlashOnNotification"] = m_bFlashOnNotification;
	j["Use12HourTime"] = m_bUse12HourTime;
	j["ShowBlockedMessages"] = m_bShowBlockedMessages;
	
	if (m_bSaveWindowSize) {
		j["WindowWidth"] = m_width;
		j["WindowHeight"] = m_height;
	}

	return GetFrontend()->SaveConfig(j.dump());
}

bool LocalSettings::CheckTrustedDomain(const std::string& url)
{
	// check if the domain belongs to the trusted list
	std::string domain, resource;
	SplitURL(url, domain, resource);
	return m_trustedDomains.find(domain) != m_trustedDomains.end();
}

void LocalSettings::StopUpdateCheckTemporarily()
{
	// Remind again in 3 days
	m_remindUpdatesOn = time(NULL) + time_t(3LL * 24 * 60 * 60);
}
