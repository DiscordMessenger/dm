#pragma once
#define WIN32_LEAN_AND_MEAN
#include <string>
#include <windows.h>
#include "models/Snowflake.hpp"
#include "models/Message.hpp"

// GET Request:
// 
//   discordapi/channels/[channel_id]/threads/search?archived=true&sort_by=last_message_time&sort_order=desc&limit=25&tag_setting=match_some&offset=0
//   -- when opening a forum
// 
//   discordapi/channels/[channel_id]/threads/search?name=[query]&tag_setting=match_some
//   -- when searching inside a forum
// 
//   discordapi/channels/[channel_id]/threads/search?archived=true&sort_by=last_message_time&sort_order=desc&limit=25&tag_setting=match_some&offset=0
//   -- when opening the thread picker inside a channel (the same as forums... they seem to be related)
//    
// RESPONSE: Array of thread/channel objects

class MessageList;

typedef std::map <Snowflake, std::vector<MessagePtr> > PinnedMap;

class ThreadList
{
public:
	static void Initialize(HWND hWnd);
	static void OnLoadedPins(Snowflake channelID, const std::string& data);
	static void OnUpdateEmbed(const std::string& key);
	static void OnUpdateAvatar(Snowflake key);
	static void OnUpdateEmoji(Snowflake key);
	static bool IsActive();
	static bool IsFocused();
	static void Show(Snowflake channelID, Snowflake guildID, int x, int y, bool rightJustify = false);

protected:
	friend class MessageList;

private:
	static INT_PTR CALLBACK DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
	static Snowflake m_channel, m_guild;
	static POINT m_appearXY;
	static bool m_bActive, m_bRightJustify;
	static HWND m_hwnd;
};
