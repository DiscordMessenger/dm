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

		UPDATE_SUBSCRIPTIONS = 37, // guess - update channels subscribed to
		// payload:
		// example payload: {'d': {'subscriptions': {'GUILDIDHERE': {'channels': {'CHANNELID1': [[0, 99]], 'CHANNELID2': [[0, 99]]}}}}, 'op': 37}
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

public:
	Profile* GetProfile() {
		return GetProfileCache()->LookupProfile(m_mySnowflake, "", "", "", false);
	}

	Snowflake GetUserID() const {
		return m_mySnowflake;
	}

	std::string GetToken() const {
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

	Guild* GetCurrentGuild()
	{
		return GetGuild(m_CurrentGuild);
	}

	Snowflake GetCurrentGuildID() const {
		return m_CurrentGuild;
	}

	Channel* GetChannel(Snowflake sf)
	{
		Guild* pGuild = GetCurrentGuild();
		if (!pGuild) return NULL;
		return pGuild->GetChannel(sf);
	}

	Channel* GetCurrentChannel()
	{
		return GetChannel(m_CurrentChannel);
	}

	Snowflake GetCurrentChannelID() const {
		return m_CurrentChannel;
	}

	Channel* GetChannelGlobally(Snowflake sf) {
		Channel* chn = nullptr;
		for (auto& gld : m_guilds) {
			chn = gld.GetChannel(sf);
			if (chn)
				return chn;
		}

		return m_dmGuild.GetChannel(sf);
	}

	void HandledChannelSwitch() {
		m_messageRequestsInProgress[0] = false;
	}

	// Lookup
	std::string LookupChannelNameGlobally(Snowflake sf);
	std::string LookupRoleNameGlobally(Snowflake sf);
	std::string LookupUserNameGlobally(Snowflake sf, Snowflake gld);

	// Select a guild.
	void OnSelectGuild(Snowflake sf);

	// Select a channel in the current guild.
	void OnSelectChannel(Snowflake sf, bool bSendSubscriptionUpdate = true);

	// Fetch messages in specified channel.
	void RequestMessages(Snowflake sf, ScrollDir::eScrollDir dir = ScrollDir::BEFORE, Snowflake source = 0, Snowflake gapper = 0);

	// Fetch guild members in specified guild.
	void RequestGuildMembers(Snowflake guild, std::set<Snowflake> members, bool bLoadPresences = false);

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

	// Send a message to the current channel.
	bool SendMessageToCurrentChannel(const std::string& msg, Snowflake& tempSf, Snowflake reply = 0, bool mentionReplied = true);

	// Send a message with an attachment to the current channel.
	bool SendMessageAndAttachmentToCurrentChannel(const std::string& msg, Snowflake& tempSf, uint8_t* pAttData, size_t szAtt, const std::string& attName);

	// Edit a message in the current channel.
	bool EditMessageInCurrentChannel(const std::string& msg, Snowflake msgId);

	// Set current activity status.
	void SetActivityStatus(eActiveStatus status, bool bRequestServer = true);

	// Close the current gateway session.
	void CloseGatewaySession();

	// Inform the Discord backend that we are typing.
	void Typing();

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

public:
	DiscordInstance(std::string token) : m_token(token) {
		InitDispatchFunctions();
	}

	void HandleRequest(void* pReq); // actually this expects NetworkerThread::Request*

	void HandleGatewayMessage(const std::string& payload);

	void SendHeartbeat();

	void SendSettingsProto(const std::vector<uint8_t>& data);

	void LoadUserSettings(const std::string& userSettings);

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

	// handle functions
	void HandleREADY(nlohmann::json& j);
	void HandleMESSAGE_CREATE(nlohmann::json& j);
	void HandleMESSAGE_DELETE(nlohmann::json& j);
	void HandleMESSAGE_UPDATE(nlohmann::json& j);
	void HandleMESSAGE_ACK(nlohmann::json& j);
	void HandleUSER_SETTINGS_PROTO_UPDATE(nlohmann::json& j);
	void HandleGUILD_CREATE(nlohmann::json& j);
	void HandleGUILD_DELETE(nlohmann::json& j);
	void HandleCHANNEL_CREATE(nlohmann::json& j);
	void HandleCHANNEL_DELETE(nlohmann::json& j);
	void HandleGUILD_MEMBER_LIST_UPDATE(nlohmann::json& j);
	void HandleGUILD_MEMBERS_CHUNK(nlohmann::json& j);
	void HandleTYPING_START(nlohmann::json& j);
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

Snowflake GetSnowflake(nlohmann::json& j, const std::string& key);

// Fetches a key
std::string GetFieldSafe(nlohmann::json& j, const std::string& key);

int GetFieldSafeInt(nlohmann::json& j, const std::string& key);

#define TYPING_INTERVAL 10000 // 10 sec
