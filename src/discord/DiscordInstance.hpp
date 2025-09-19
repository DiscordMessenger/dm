#pragma once

#include <unordered_map>
#include <algorithm>
#include <string>
#include <list>
#include <set>
#include <nlohmann/json.h>
#include "DiscordAPI.hpp"
#include "Snowflake.hpp"
#include "SettingsManager.hpp"
#include "Guild.hpp"
#include "DiscordRequest.hpp"
#include "MessageCache.hpp"
#include "ProfileCache.hpp"
#include "ScrollDir.hpp"
#include "Message.hpp"
#include "Relationship.hpp"
#include "NotificationManager.hpp"
#include "UserGuildSettings.hpp"
#include "GuildListItem.hpp"
#include "FormattedText.hpp"

struct NetRequest;

struct AddMessageParams
{
	Message msg;
	Snowflake channel;
};

struct FailedMessageParams
{
	Snowflake channel;
	Snowflake message;
};

// Used by GetGuildIDsOrdered
#define BIT_FOLDER 0x8000000000000000

#define C_CHANNEL_HISTORY_MAX (3)
struct ChannelHistory
{
	// latest to earliest
	Snowflake m_history[C_CHANNEL_HISTORY_MAX] = { 0 };

	void Clear() {
		for (int i = 0; i < C_CHANNEL_HISTORY_MAX; i++)
			m_history[i] = 0;
	}

	void AddToHistory(Snowflake sf)
	{
		if (sf == 0)
			return;

		for (int i = 0; i < C_CHANNEL_HISTORY_MAX; i++)
		{
			if (m_history[i] == sf) {
				std::swap(m_history[0], m_history[i]);
				return;
			}
		}

		memmove(&m_history[1], &m_history[0], sizeof(m_history) - sizeof(Snowflake));

		m_history[0] = sf;
	}
};

class QuickMatch
{
public:
	QuickMatch(bool channel, Snowflake id, float fzc, const std::string& name) :
		m_isChannel(channel), m_id(id), m_fuzzy(fzc), m_name(name) {}
	bool IsChannel() const { return m_isChannel; }
	bool IsGuild() const { return !m_isChannel; }
	Snowflake Id() const { return m_id; }

	bool operator<(const QuickMatch& oth) const
	{
		if (m_fuzzy != oth.m_fuzzy)
			return m_fuzzy > oth.m_fuzzy;

		// Channels are prioritized.
		if (m_isChannel && !oth.m_isChannel)
			return true;

		if (!m_isChannel && oth.m_isChannel)
			return false;

		int res = strcmp(m_name.c_str(), oth.m_name.c_str());
		if (res != 0)
			return res < 0;

		return m_id < oth.m_id;
	}

private:
	Snowflake m_id = 0;
	std::string m_name;
	bool m_isChannel = false;
	float m_fuzzy = 0;
};

namespace GatewayOp
{
	enum eOpcode
	{
		DISPATCH,
		HEARTBEAT,
		IDENTIFY,
		PRESENCE_UPDATE,
		VOICE_STATE_UPDATE,
		RESUME = 6,
		RECONNECT,
		REQUEST_GUILD_MEMBERS,
		INVALID_SESSION,
		HELLO,
		HEARTBACK,

		SUBSCRIBE_DM = 13,
		SUBSCRIBE_GUILD,

		UPDATE_SUBSCRIPTIONS = 37, // guess - update channels subscribed to
		// payload:
		// example payload: {'d': {'subscriptions': {'GUILDIDHERE': {'channels': {'CHANNELID1': [[0, 99]], 'CHANNELID2': [[0, 99]]}}}}, 'op': 37}
		// note: dropped in favour of opcode 14
	};
};

struct PendingUpload
{
	// Message
	std::string m_content;
	Snowflake m_tempSF = 0;
	Snowflake m_channelSF = 0;
	// Attachment
	std::string m_name;
	std::vector<uint8_t> m_data;
	// Attachment after first interaction
	std::string m_uploadUrl;
	std::string m_uploadFileName;

	PendingUpload() { }

	PendingUpload(const std::string& n, uint8_t* d, size_t s, const std::string& c, Snowflake tsf, Snowflake csf) :
		m_name(n),
		m_content(c),
		m_tempSF(tsf),
		m_channelSF(csf)
	{
		m_data.resize(s);
		memcpy(m_data.data(), d, s);
	}
};

class DiscordInstance
{
private:
	std::string m_token;

public:
	Snowflake m_mySnowflake = 0;
	Snowflake m_CurrentGuild   = 0;
	Snowflake m_CurrentChannel = 0;

	// Guild DB
	std::list<Guild> m_guilds;

	Guild m_dmGuild;

	// Requests in progress
	std::map<Snowflake, bool> m_messageRequestsInProgress;

	// Gateway url and connection ID
	std::string m_gatewayUrl = "";
	int m_gatewayConnId = -1;
	int m_heartbeatSequenceId = -1;

	// Things we get on ready
	std::string m_gatewayResumeUrl = "";
	std::string m_sessionId = ""; // for resume
	std::string m_sessionType = "";

	// Last time we sent a typing indicator
	uint64_t m_lastTypingSent = 0;

	// Sequence number of ack state
	int m_ackVersion = 0;

	// Next sent attachment snowflake
	Snowflake m_nextAttachmentID = 1;

	// Pending uploads
	std::map<Snowflake, PendingUpload> m_pendingUploads;

	// Channel history
	ChannelHistory m_channelHistory;

	// Relationships
	std::list<Relationship> m_relationships;

	// User guild settings
	UserGuildSettings m_userGuildSettings;

	// Notification manager
	NotificationManager m_notificationManager;

	// List of channels user cannot view because an HTTPS request related
	// to them returned a 403.  Frankly this shouldn't be usable, but oh well.
	std::set<Snowflake> m_channelDenyList;

	// List of guilds and guild folders.
	GuildItemList m_guildItemList;

public:
	Profile* GetProfile() {
		return GetProfileCache()->LookupProfile(m_mySnowflake, "", "", "", false);
	}

	Snowflake GetUserID() const {
		return m_mySnowflake;
	}

	const std::string& GetToken() const {
		return m_token;
	}

	void SetToken(const std::string& token){
		m_token = token;
	}

	int GetGatewayID() const {
		return m_gatewayConnId;
	}

	// TODO optimize this stuff
	Guild* GetGuild(Snowflake sf)
	{
		assert(sf != 1);
		
		if (!sf)
			return &m_dmGuild;

		for (auto& g : m_guilds)
		{
			if (g.m_snowflake == sf)
				return &g;
		}
		return nullptr;
	}

	void GetGuildIDs(std::vector<Snowflake>& sf, bool bUI = false)
	{
		if (bUI) {
			sf.push_back(0); // @me
			sf.push_back(1); // gap
		}

		for (auto& g : m_guilds)
			sf.push_back(g.m_snowflake);
	}

	std::string GetGuildFolderName(Snowflake sf)
	{
		auto items = m_guildItemList.GetItems();
		for (auto& item : *items)
		{
			if (item->GetID() != sf)
				continue;

			auto name = item->GetName();
			if (!name.empty())
				return name;

			if (!item->IsFolder() || item->GetItems()->empty())
				return "Empty Folder";

			name = "";
			auto subitems = item->GetItems();
			for (auto& subitem : *subitems) {
				if (!name.empty())
					name += ", ";
				name += subitem->GetName();
			}

			if (name.size() > 100)
				name = name.substr(0, 97) + "...";
			return name;
		}

		return "Unknown";
	}

	void GetGuildIDsOrdered(std::vector<Snowflake>& sf, bool bUI = false)
	{
		if (bUI) {
			sf.push_back(0); // @me
			sf.push_back(1); // gap
		}

		auto items = m_guildItemList.GetItems();
		for (auto& item : *items)
		{
			// If there is a folder, OR the snowflake with BIT_FOLDER
			Snowflake id = item->GetID() & ~BIT_FOLDER;

			if (item->IsFolder())
			{
				sf.push_back(BIT_FOLDER | item->GetID());

				auto subitems = item->GetItems();
				for (auto& subitem : *subitems)
				{
					sf.push_back(subitem->GetID());
				}

				// add an empty item to terminate this folder
				sf.push_back(BIT_FOLDER);
			}
			else
			{
				sf.push_back(item->GetID());
			}
		}
	}

	Guild* GetCurrentGuild()
	{
		return GetGuild(m_CurrentGuild);
	}

	Snowflake GetCurrentGuildID() const {
		return m_CurrentGuild;
	}

	Channel* GetChannel(Snowflake sf)
	{
		Channel* pChan;
		Guild* pGuild = GetCurrentGuild();
		if (pGuild) {
			pChan = pGuild->GetChannel(sf);
			if (pChan) return pChan;
		}

		for (auto& gld : m_guilds) {
			pChan = gld.GetChannel(sf);
			if (pChan)
				return pChan;
		}

		return m_dmGuild.GetChannel(sf);
	}

	Channel* GetCurrentChannel()
	{
		return GetChannel(m_CurrentChannel);
	}

	Snowflake GetCurrentChannelID() const {
		return m_CurrentChannel;
	}

	Channel* GetChannelGlobally(Snowflake sf) {
		return GetChannel(sf);
	}

	void HandledChannelSwitch() {
		m_messageRequestsInProgress[0] = false;
	}

	bool IsGatewayConnected() const {
		return m_gatewayConnId >= 0;
	}

	// Lookup
	std::string LookupChannelNameGlobally(Snowflake sf);
	std::string LookupRoleName(Snowflake sf, Snowflake guildID);
	std::string LookupRoleNameGlobally(Snowflake sf);
	std::string LookupUserNameGlobally(Snowflake sf, Snowflake gld);

	// Search for channels using the quick switcher query format.
	std::vector<QuickMatch> Search(const std::string& query);

	// Select a guild.
	void OnSelectGuild(Snowflake sf, Snowflake chan = 0);

	// Select a channel in the current guild.
	void OnSelectChannel(Snowflake sf, bool bSendSubscriptionUpdate = true);

	// Fetch messages in specified channel.
	void RequestMessages(Snowflake sf, ScrollDir::eScrollDir dir = ScrollDir::BEFORE, Snowflake source = 0, Snowflake gapper = 0);

	// Fetch guild members in specified guild.
	void RequestGuildMembers(Snowflake guild, std::set<Snowflake> members, bool bLoadPresences = false);

	// Fetch guild members in specified guild.
	void RequestGuildMembers(Snowflake guild, std::string query, bool bLoadPresences = false, int limit = 10);

	// Fetch pinned messages in a specified channel.
	void RequestPinnedMessages(Snowflake channel);

	// Send a refresh to the message list.
	void OnFetchedMessages(Snowflake gap = 0, ScrollDir::eScrollDir dir = ScrollDir::BEFORE);

	// Handle the case where a channel list was fetched from a guild.
	void OnFetchedChannels(Guild* pGld, const std::string& content);

	// Handle the case where the gateway is closed.
	void GatewayClosed(int errorCode);

	// Start a new gateway session.
	void StartGatewaySession();

	// Transform user, channel, or emoji mentions ("@usernamehere") into snowflake mentions (<@12347689436274>).
	std::string ResolveMentions(const std::string& str, Snowflake guild, Snowflake channel);

	// Transform snowflake mentions into user, channel, or emoji mentions.
	std::string ReverseMentions(const std::string& message, Snowflake guild, bool ttsMode = false);

	// Send a message to the current channel.
	bool SendMessageToCurrentChannel(const std::string& msg, Snowflake& tempSf, Snowflake reply = 0, bool mentionReplied = true);

	// Send a message with an attachment to the current channel.
	bool SendMessageAndAttachmentToCurrentChannel(const std::string& msg, Snowflake& tempSf, uint8_t* pAttData, size_t szAtt, const std::string& attName, bool isSpoiler = false);

	// Edit a message in the current channel.
	bool EditMessageInCurrentChannel(const std::string& msg, Snowflake msgId);

	// Set current activity status.
	void SetActivityStatus(eActiveStatus status, bool bRequestServer = true);

	// Close the current gateway session.
	void CloseGatewaySession();

	// Inform the Discord backend that we are typing.
	void Typing();

	// Inform the Discord backend that we have acknowledged messages up to but not including "message".
	// Used by the "mark unread" feature.
	void RequestAcknowledgeMessages(Snowflake channel, Snowflake message, bool manual = true);

	// Inform the Discord backend that we have acknowledged a message.
	void RequestAcknowledgeChannel(Snowflake channel);

	// Mark an entire guild as read.
	void RequestAcknowledgeGuild(Snowflake guild);

	// Request a message deletion.
	void RequestDeleteMessage(Snowflake chan, Snowflake msg);

	// Request to leave a guild.
	void RequestLeaveGuild(Snowflake guild);

	// Update channels that we are subscribed to.
	void UpdateSubscriptions(Snowflake guild, Snowflake channel, bool typing, bool activities, bool threads, int rangeMembers = 99);

	// Request a jump to a message.
	void JumpToMessage(Snowflake guild, Snowflake channel, Snowflake message);

	// Launch a URL.  This could also be a discord.com / discordapp.com / discord.gg link,
	// which we have to specially handle.
	void LaunchURL(const std::string& url);

	// Check if we did the initial API_URL/gateway request.
	bool HasGatewayURL() const { return !m_gatewayUrl.empty(); }

	// Reset the initial gateway URL. Used when switching service providers.
	void ResetGatewayURL();

	// Clears data about the current user when logged out.
	void ClearData();

	// Resolves links automatically in a formatted message.
	void ResolveLinks(FormattedText* message, std::vector<InteractableItem>& interactables, Snowflake guildID);

public:
	DiscordInstance(std::string token) : m_token(token), m_notificationManager(this) {
		InitDispatchFunctions();
	}

	void HandleRequest(NetRequest* pReq);

	void HandleGatewayMessage(const std::string& payload);

	void SendHeartbeat();

	void SendSettingsProto(const std::vector<uint8_t>& data);

	void LoadUserSettings(const std::string& userSettings);

	bool ResortChannels(Snowflake guild);

public:
	// returns user's id. The user parameter is used only if j["user"] doesn't exist
	Snowflake ParseGuildMember(Snowflake guild, nlohmann::json& j, Snowflake user = 0);
	Snowflake ParseGuildMemberOrGroup(Snowflake guild, nlohmann::json& j);

private:
	void InitDispatchFunctions();
	void UpdateSettingsInfo();
	bool SortGuilds();
	void ParseChannel(Channel& c, nlohmann::json& j, int& num);
	void ParseAndAddGuild(nlohmann::json& j);
	void ParsePermissionOverwrites(Channel& c, nlohmann::json& j);
	void ParseReadStateObject(nlohmann::json& j, bool bAlternate);
	void OnUploadAttachmentFirst(NetRequest* pReq);
	void OnUploadAttachmentSecond(NetRequest* pReq);
	void SearchSubGuild(std::vector<QuickMatch>& matches, Guild* pGuild, int matchFlags, const char* query);
	std::string TransformMention(const std::string& source, Snowflake guild, Snowflake channel);

	// handle functions
	void HandleREADY(nlohmann::json& j);
	void HandleREADY_SUPPLEMENTAL(nlohmann::json& j);
	void HandleMESSAGE_CREATE(nlohmann::json& j);
	void HandleMESSAGE_DELETE(nlohmann::json& j);
	void HandleMESSAGE_UPDATE(nlohmann::json& j);
	void HandleMESSAGE_ACK(nlohmann::json& j);
	void HandleUSER_GUILD_SETTINGS_UPDATE(nlohmann::json& j);
	void HandleUSER_SETTINGS_PROTO_UPDATE(nlohmann::json& j);
	void HandleUSER_NOTE_UPDATE(nlohmann::json& j);
	void HandleGUILD_CREATE(nlohmann::json& j);
	void HandleGUILD_DELETE(nlohmann::json& j);
	void HandleCHANNEL_CREATE(nlohmann::json& j);
	void HandleCHANNEL_DELETE(nlohmann::json& j);
	void HandleCHANNEL_UPDATE(nlohmann::json& j);
	void HandleGUILD_MEMBER_LIST_UPDATE(nlohmann::json& j);
	void HandleGUILD_MEMBERS_CHUNK(nlohmann::json& j);
	void HandleTYPING_START(nlohmann::json& j);
	void HandlePRESENCE_UPDATE(nlohmann::json& j);
	void HandlePASSIVE_UPDATE_V1(nlohmann::json& j);

private:
	void HandleGuildMemberListUpdate_Sync(Snowflake guild, nlohmann::json& j);
	void HandleGuildMemberListUpdate_Insert(Snowflake guild, nlohmann::json& j);
	void HandleGuildMemberListUpdate_Delete(Snowflake guild, nlohmann::json& j);
	void HandleGuildMemberListUpdate_Update(Snowflake guild, nlohmann::json& j);
	void HandleMessageInsertOrUpdate(nlohmann::json& j, bool bIsUpdate);
};

DiscordInstance* GetDiscordInstance();

int64_t GetIntFromString(const std::string& str);

Snowflake GetSnowflake(const nlohmann::json& j, const std::string& key);

// Fetches a key
std::string GetFieldSafe(const nlohmann::json& j, const std::string& key);

int GetFieldSafeInt(const nlohmann::json& j, const std::string& key);

#define TYPING_INTERVAL 10000 // 10 sec
