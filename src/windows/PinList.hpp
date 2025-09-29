#pragma once
#define WIN32_LEAN_AND_MEAN
#include <string>
#include <windows.h>
#include "models/Snowflake.hpp"
#include "models/Message.hpp"

// GET Request: discordapi/channels/$CHANNEL_ID/pins
// RESPONSE: Array of message objects

class MessageList;

typedef std::map <Snowflake, std::vector<MessagePtr> > PinnedMap;

class PinList
{
public:
	static void Initialize(HWND hWnd);
	static void OnLoadedPins(Snowflake channelID, const std::string& data);
	static void OnUpdateEmbed(const std::string& key);
	static void OnUpdateAvatar(Snowflake key);
	static void OnUpdateEmoji(Snowflake key);
	static bool IsActive();
	static void Show(Snowflake channelID, Snowflake guildID, int x, int y, bool rightJustify = false);

protected:
	friend class MessageList;
	static void OnClickMessage(Snowflake sf);

private:
	static void AddMessage(Snowflake channelID, MessagePtr msg);
	static INT_PTR CALLBACK DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
	static Snowflake m_channel, m_guild;
	static MessageList* m_pMessageList;
	static PinnedMap m_map;
	static POINT m_appearXY;
	static bool m_bActive, m_bRightJustify;
};
