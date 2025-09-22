#pragma once

#include <memory>
#include "Snowflake.hpp"

struct Guild;
struct Channel;

class ChatView
{
public:
	int GetID() const { return m_viewID; }
	void SetID(int i) { m_viewID = i; }

	Snowflake GetCurrentGuildID() const { return m_guildID; }
	Snowflake GetCurrentChannelID() const { return m_channelID; }

	Guild* GetCurrentGuild();
	Channel* GetCurrentChannel();

	void SetCurrentGuildID(Snowflake sf) { m_guildID = sf; }
	void SetCurrentChannelID(Snowflake sf) { m_channelID = sf; }

	void OnSelectChannel(Snowflake sf, bool bSendSubscriptionUpdate = true);
	void OnSelectGuild(Snowflake sf, Snowflake chan = 0);

private:
	Snowflake m_guildID = 0;
	Snowflake m_channelID = 0;

	int m_viewID = 0;
};

typedef std::shared_ptr<ChatView> ChatViewPtr;
