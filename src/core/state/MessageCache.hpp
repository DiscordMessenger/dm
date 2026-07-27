#pragma once

#include <map>
#include <list>
#include <nlohmann/json.h>
#include "../models/Snowflake.hpp"
#include "../models/ScrollDir.hpp"
#include "../models/Message.hpp"

struct MessageChunkList
{
	// int - Offset. How many messages ago was this message posted
	std::map<Snowflake, MessagePtr> m_messages;

	bool m_lastMessagesLoaded = false;
	Snowflake m_guild = 0;

	MessageChunkList();
	void ProcessRequest(ScrollDir::eScrollDir sd, Snowflake anchor, nlohmann::json& j, const std::string& channelName);
	void AddMessage(const Message& msg);
	void EditMessage(const Message& msg);
	void DeleteMessage(Snowflake message);
	int GetMentionCountSince(Snowflake message, Snowflake user);
	MessagePtr GetLoadedMessage(Snowflake message);
};

class MessageCache
{
public:
	MessageCache();
	
	void GetLoadedMessages(Snowflake channel, Snowflake guild, std::list<MessagePtr>& out);

	// note: scroll dir used to add gap message
	void ProcessRequest(Snowflake channel, ScrollDir::eScrollDir sd, Snowflake anchor, nlohmann::json& j, const std::string& channelName);

	void AddMessage(Snowflake channel, const Message& msg);
	void EditMessage(Snowflake channel, const Message& msg);
	void DeleteMessage(Snowflake channel, Snowflake message);
	int GetMentionCountSince(Snowflake channel, Snowflake message, Snowflake user);
	void ClearAllChannels();
	bool IsMessageLoaded(Snowflake channel, Snowflake message);

	MessagePtr GetLoadedMessage(Snowflake channel, Snowflake message);

private:
	std::map <Snowflake, MessageChunkList> m_mapMessages;
};

MessageCache* GetMessageCache();
