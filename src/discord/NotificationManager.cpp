#include "NotificationManager.hpp"
#include "DiscordInstance.hpp"
#include "Frontend.hpp"
#include "Util.hpp"

NotificationManager::NotificationManager(DiscordInstance* pDiscord):
	m_pDiscord(pDiscord)
{
}

void NotificationManager::OnMessageCreate(Snowflake guildID, Snowflake channelID, const Message& msg)
{
	if (!IsNotificationWorthy(guildID, channelID, msg))
		return;

	DbgPrintF("NEW NOTIFICATION: Guild %lld  Channel %lld  Message %lld : '%s'", guildID, channelID, msg.m_snowflake, msg.m_message.c_str());
}

bool NotificationManager::IsNotificationWorthy(Snowflake guildID, Snowflake channelID, const Message& msg)
{
	if (msg.m_author_snowflake)
	{
		if (msg.m_author_snowflake == m_pDiscord->m_mySnowflake)
			// Shouldn't be notified for messages we post ourselves!
			return false;

		// Check if a block relationship exists. If yes, this message is not notification worthy.
		for (const auto& rel : m_pDiscord->m_relationships)
		{
			if (rel.m_userID == msg.m_author_snowflake && rel.m_type == REL_BLOCKED)
				return false;
		}
	}

	// If we are focused on this very channel and the window is not minimized, return
	if (channelID == m_pDiscord->GetCurrentChannelID() && !GetFrontend()->IsWindowMinimized())
		return false;

	auto pSettings = m_pDiscord->m_userGuildSettings.GetSettings(guildID);

	auto messageNotifications = NOTIF_ALL_MESSAGES;
	bool suppEveryone = false, suppRoles = false;

	if (pSettings)
	{
		if (pSettings->IsMuted())
			return false;

		messageNotifications = pSettings->m_messageNotifications;

		auto pOverride = pSettings->GetOverride(channelID);
		if (pOverride)
		{
			// Check if specifically that channel was muted
			if (pOverride->IsMuted())
				return false;

			if (pOverride->m_notifications != NOTIF_INHERIT_PARENT)
				// Channel has its own override, use that
				messageNotifications = pOverride->m_notifications;
		}

		suppEveryone = pSettings->m_bSuppressEveryone;
		suppRoles = pSettings->m_bSuppressRoles;
	}
	else
	{
		// No guild setting, use default:
		Guild* pGld = m_pDiscord->GetGuild(guildID);
		if (pGld)
			messageNotifications = pGld->m_defaultMessageNotifications;
	}

	switch (messageNotifications)
	{
		case NOTIF_NO_MESSAGES:
			return false;

		case NOTIF_ALL_MESSAGES:
			return true;
	}

	assert(messageNotifications == NOTIF_ONLY_MENTIONS);

	// ok, only mentions. Check if we have been mentioned.
	return msg.CheckWasMentioned(m_pDiscord->m_mySnowflake, guildID, suppEveryone, suppRoles);
}
