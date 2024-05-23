#pragma once

#include <map>
#include <list>
#include <nlohmann/json.h>
#include "Snowflake.hpp"
#include "ScrollDir.hpp"
#include "Message.hpp"

struct MessageChunkList
{
	// int - Offset. How many messages ago was this message posted
	std::map<Snowflake, Message> m_messages;

	bool m_lastMessagesLoaded = false;
	Snowflake m_guild = 0;

	MessageChunkList();
	void ProcessRequest(ScrollDir::eScrollDir sd, Snowflake anchor, nlohmann::json& j);
	void AddMessage(const Message& msg);
	void EditMessage(const Message& msg);
	void DeleteMessage(Snowflake message);
	int GetMentionCountSince(Snowflake message, Snowflake user);
	Message* GetLoadedMessage(Snowflake message);
};

class MessageCache
{
public:
	MessageCache();
	
	void GetLoadedMessages(Snowflake channel, Snowflake guild, std::list<Message>& out);

	// note: scroll dir used to add gap message
	void ProcessRequest(Snowflake channel, ScrollDir::eScrollDir sd, Snowflake anchor, nlohmann::json& j);

	void AddMessage(Snowflake channel, const Message& msg);
	void EditMessage(Snowflake channel, const Message& msg);
	void DeleteMessage(Snowflake channel, Snowflake message);
	int GetMentionCountSince(Snowflake channel, Snowflake message, Snowflake user);
	void ClearAllChannels();

	// NOTE: Returned message pointer is invalidated when the specific channel is updated. So fetch
	// as fast as possible!
	Message* GetLoadedMessage(Snowflake channel, Snowflake message);

private:
	std::map <Snowflake, MessageChunkList> m_mapMessages;
};

MessageCache* GetMessageCache();
