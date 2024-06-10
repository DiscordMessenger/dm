#pragma once

#include <string>
#include <list>
#include "Message.hpp"

class DiscordInstance;

class Notification
{
public:



public:
	std::string m_author;
	std::string m_contents;
	Snowflake m_sourceGuild = 0, m_sourceChannel = 0, m_sourceMessage = 0;
};

class NotificationManager
{
public:
	NotificationManager(DiscordInstance*);

	void OnMessageCreate(Snowflake guildID, Snowflake channelID, const Message& msg);



private:
	bool IsNotificationWorthy(Snowflake guildID, Snowflake channelID, const Message& msg);

private:
	DiscordInstance* m_pDiscord;
	std::list<Notification> m_notifications;
};

