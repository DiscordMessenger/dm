#pragma once

#include "Config.hpp"

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <windowsx.h>
#include <userenv.h>
#include <shlobj.h>
#include <commctrl.h>
#ifdef NEW_WINDOWS
#include <shlwapi.h>
#endif

#include <sstream>
#include <cassert>
#include <clocale>
#include <vector>
#include <string>
#include <queue>
#include <list>
#include <map>

#if defined NEW_WINDOWS && defined UNICODE && !defined MINGW_SPECIFIC_HACKS
#define USE_SPEECH
#endif

#if !defined MINGW_SPECIFIC_HACKS
#define WEBP_SUP
#endif

#define STBI_SUP

#include "../resource.h"

#include "MissingDefinitions.hpp"
#include "WindowMessages.hpp"
#include "Measurements.hpp"
#include "TextManager.hpp"
#include "WinUtils.hpp"
#include "AvatarCache.hpp"
#include "NetworkerThread.hpp"
#include "TextInterface_Win32.hpp"

#include "../discord/DiscordAPI.hpp"
#include "../discord/SettingsManager.hpp"
#include "../discord/Util.hpp"
#include "../discord/ProfileCache.hpp"
#include "../discord/DiscordInstance.hpp"

#define MAX_MESSAGE_SIZE 2000 // 4000 with nitro

extern HINSTANCE g_hInstance;
extern WNDCLASS g_MainWindowClass;
extern HBRUSH   g_backgroundBrush;
extern HWND     g_Hwnd;
extern HICON    g_Icon;
extern HICON    g_BotIcon;
extern HICON    g_SendIcon;
extern HICON    g_JumpIcon;
extern HICON    g_CancelIcon;
extern HICON    g_UploadIcon;
extern HICON    g_DownloadIcon;

extern HFONT
	g_MessageTextFont,
	g_AuthorTextFont,
	g_AuthorTextUnderlinedFont,
	g_DateTextFont,
	g_GuildCaptionFont,
	g_GuildSubtitleFont,
	g_GuildInfoFont,
	g_AccountInfoFont,
	g_AccountTagFont,
	g_SendButtonFont,
	g_ReplyTextFont,
	g_TypingRegFont,
	g_TypingBoldFont;

extern HICON g_ProfileBorderIcon;
extern HICON g_ProfileBorderIconGold;

enum eComboIDs
{
	CID_MESSAGELIST = 9999,
	CID_CHANNELVIEW,
	CID_PROFILEVIEW,
	CID_MESSAGEINPUT,
	CID_MESSAGEINPUTSEND,
	CID_GUILDHEADER,
	CID_GUILDLISTER,
	CID_MEMBERLIST,
	CID_ROLELIST,
	CID_MESSAGEEDITOR,
	CID_MESSAGEREPLYTO,
	CID_MESSAGEMENTCHECK,
	CID_MESSAGEREPLYUSER,
	CID_MESSAGEREPLYCANCEL,
	CID_MESSAGEREPLYJUMP,
	CID_MESSAGEUPLOAD,
	CID_MESSAGEEDITINGLBL,
	CID_STATUSBAR,
};

struct UpdateProfileParams
{
	Snowflake m_user;
	std::string m_resId;
};

struct WebsocketMessageParams
{
	int m_gatewayId;
	std::string m_payload;
};

struct TypingParams
{
	Snowflake m_user;
	Snowflake m_guild;
	Snowflake m_channel;
	time_t m_timestamp;
};

struct SendMessageParams
{
	LPTSTR m_rawMessage;
	Snowflake m_replyTo;
	bool m_bEdit;
	bool m_bReply;
	bool m_bMention;
};

struct SendMessageAuxParams
{
	std::string m_message;
	Snowflake m_snowflake;
};

std::string FormatDiscrim(int discrim);
std::string GetStringFromHResult(HRESULT hr);
std::string GetDiscordToken();
void OnUpdateAvatar(const std::string& resid);
DiscordInstance* GetDiscordInstance();
void WantQuit();
void SetHeartbeatInterval(int timeMs);
int GetProfilePictureSize();
HBITMAP GetDefaultBitmap();
