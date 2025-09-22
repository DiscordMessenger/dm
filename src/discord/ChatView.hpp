#pragma once

#include <memory>
#include "Snowflake.hpp"

struct Guild;
struct Channel;

class ChatView
{
public:
	Snowflake GetCurrentGuildID() const { return m_guildID; }
	Snowflake GetCurrentChannelID() const { return m_channelID; }

	Guild* GetCurrentGuild();
	Channel* GetCurrentChannel();

	void SetCurrentGuildID(Snowflake sf) { m_guildID = sf; }
	void SetCurrentChannelID(Snowflake sf) { m_channelID = sf; }

private:
	Snowflake m_guildID = 0;
	Snowflake m_channelID = 0;
};

typedef std::shared_ptr<ChatView> ChatViewPtr;
