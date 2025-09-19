#pragma once

#include <iprogsjson.hpp>
#include <string>
#include <vector>
#include <map>
#include <cassert>
#include "Snowflake.hpp"

struct MessagePollOption
{
	int m_answerId = 0;
	std::string m_text;
	std::string m_emojiUTF8;
	Snowflake m_emojiSF = 0;
	bool m_bMeVoted = false;
	int m_voteCount = 0;
};

class MessagePoll
{
public:
	bool m_bAllowMultiselect = false;
	std::string m_question = "";
	std::map<int, MessagePollOption> m_options;
	time_t m_expiry = 0;
	bool m_bIsFinalized = false;

public:
	MessagePoll(const iprog::JsonObject&);
	MessagePoll(const MessagePoll& oth);

};
