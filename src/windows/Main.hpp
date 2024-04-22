#pragma once

#include "Config.hpp"

#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <WindowsX.h>
#include <Userenv.h>
#include <Shlobj.h>
#include <Commctrl.h>
#ifdef NEW_WINDOWS
#include <Shlwapi.h>
#endif

#include <sstream>
#include <cassert>
#include <clocale>
#include <vector>
#include <string>
#include <queue>
#include <list>
#include <map>

#if defined NEW_WINDOWS && !defined MINGW_SPECIFIC_HACKS
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

#include "../discord/DiscordAPI.hpp"
#include "../discord/SettingsManager.hpp"
#include "../discord/Util.hpp"
#include "../discord/TextInterface.hpp"
#include "../discord/RectAndPoint.hpp"
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

#define COLOR_LINK    RGB(  0, 108, 235)
#define COLOR_MENT    RGB( 88, 101, 242)

#define SIZE_QUOTE_INDENT (10)
#define FONT_TYPE_COUNT (9)

struct DrawingContext {
	HDC m_hdc = NULL;
	int m_cachedHeights[FONT_TYPE_COUNT] { 0 };
	int m_cachedSpaceWidths[FONT_TYPE_COUNT] { 0 };
	DrawingContext(HDC hdc) { m_hdc = hdc; }

	// Background color - Used for blending
	COLORREF m_bkColor = CLR_NONE;
	void SetBackgroundColor(COLORREF cr) {
		m_bkColor = cr;
	}
};

static RECT RectToNative(const Rect& rc) {
	RECT rcNative { rc.left, rc.top, rc.right, rc.bottom };
	return rcNative;
}
