#pragma once

#include <string>
#include <list>
#include <ctime>
#include "../models/Message.hpp"

class DiscordInstance;

struct Notification
{
	std::string m_author;
	std::string m_contents;
	std::string m_avatarLnk;
	time_t m_timeReceived = 0;
	Snowflake m_sourceGuild = 0, m_sourceChannel = 0, m_sourceMessage = 0;
	bool m_bRead = false;
	bool m_bIsReply = false;
};

class NotificationManager
{
public:
	NotificationManager(DiscordInstance*);
	void OnMessageCreate(Snowflake guildID, Snowflake channelID, const Message& msg);
	Notification* GetLatestNotification();
	void MarkNotificationsRead(Snowflake channelID);

	std::list<Notification>& GetNotifications() {
		return m_notifications;
	}

private:
	bool IsNotificationWorthy(Snowflake guildID, Snowflake channelID, const Message& msg);

private:
	DiscordInstance* m_pDiscord;
	std::list<Notification> m_notifications;
};

NotificationManager* GetNotificationManager();
