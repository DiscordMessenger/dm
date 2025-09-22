#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <memory>
#include "../discord/ChatView.hpp"

class ChatWindow
{
public:
	ChatWindow();
	virtual ~ChatWindow();

	virtual HWND GetHWND() const = 0;
	virtual void Close() = 0;

	ChatViewPtr GetChatView() const;

	Snowflake GetCurrentGuildID() const;
	Snowflake GetCurrentChannelID() const;

	Guild* GetCurrentGuild();
	Channel* GetCurrentChannel();

	virtual void SetCurrentGuildID(Snowflake sf);
	virtual void SetCurrentChannelID(Snowflake sf);
	virtual bool IsChannelListVisible() const { return false; }
	virtual int GetGuildListerWidth() const { return 0; }

private:
	ChatViewPtr m_chatView;
};
