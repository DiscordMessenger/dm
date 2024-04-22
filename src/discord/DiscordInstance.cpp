﻿#include <nlohmann/json.h>
#include <boost/base64/base64.hpp>
#include "DiscordInstance.hpp"
#include "WebsocketClient.hpp"
#include "SettingsManager.hpp"
#include "Util.hpp"
#include "Frontend.hpp"
#include "HTTPClient.hpp"

#define DISCORD_WSS_DETAILS "?encoding=json&v=" DISCORD_API_VERSION

#ifndef _DEBUG
#define TRY try
#define CATCH catch (Json::exception& ex) { OnJsonException(ex); }
#else
#define TRY do
#define CATCH while (0)
#endif

using Json = nlohmann::json;

// Creates a temporary snowflake based on time.
Snowflake CreateTemporarySnowflake()
{
	Snowflake sf = GetTimeMs(); // returns from UNIX epoch
	const Snowflake discordEpoch = 1420070400000; // January 1, 2015 in UNIX epoch
	sf -= discordEpoch; // turn it into Discord epoch
	sf <<= 22;
	// internal worker ID, internal process ID and increment are left as zero.
	return sf;
}

std::string GetStatusStringFromGameJsonObject(Json& game)
{
	std::string dump = game.dump();
	int type = game["type"];

	switch (type) {
		default:
			return "huh?";
		case ACTIVITY_PLAYING:
			return "Playing " + GetFieldSafe(game, "name");
		case ACTIVITY_STREAMING:
			return "Streaming " + GetFieldSafe(game, "details");
		case ACTIVITY_LISTENING:
			return "Listening to " + GetFieldSafe(game, "name");
		case ACTIVITY_WATCHING:
			return "Watching " + GetFieldSafe(game, "name");
		case ACTIVITY_COMPETING:
			return "Competing";
		case ACTIVITY_CUSTOM_STATUS:
			return GetFieldSafe(game, "state");
	}
}

void DebugResponse(NetRequest* pReq)
{
	//std::string str = std::to_string(pReq->result) + ": \"" + pReq->response + "\"\n";
	//OutputDebugStringA(str.c_str());
}

void OnJsonException(Json::exception & ex)
{
	GetFrontend()->OnJsonException(ex.what());
}

void DiscordInstance::OnFetchedChannels(Guild* pGld, const std::string& content)
{
	Json j = Json::parse(content);

	pGld->m_channels.clear();

	Snowflake chan = 0;

	int order = 1;
	for (auto& elem : j)
	{
		Channel c;
		ParseChannel(c, elem, order);
		c.m_parentGuild = pGld->m_snowflake;
		pGld->m_channels.push_back(c);
	}

	pGld->m_channels.sort();
	pGld->m_bChannelsLoaded = true;
	pGld->m_currentChannel = chan;

	GetFrontend()->UpdateSelectedGuild();
}

void DiscordInstance::OnSelectChannel(Snowflake sf, bool bSendSubscriptionUpdate)
{
	if (m_CurrentChannel == sf)
		return;

	// check if there are any channels and select the first (for now)
	Guild* pGuild = GetGuild(m_CurrentGuild);
	if (!pGuild) return;

	Channel* pChan = pGuild->GetChannel(sf);

	if (!pChan)
	{
		if (sf != 0)
			return;
	}
	else if (pChan->m_channelType == Channel::CATEGORY)
		return;

	// Check if we have permission to view the channel.
	if (pChan && !pChan->HasPermission(PERM_VIEW_CHANNEL))
	{
		GetFrontend()->OnCantViewChannel(pChan->m_name);
		return;
	}

	m_CurrentChannel = sf;
	GetFrontend()->UpdateSelectedChannel();

	if (!GetCurrentChannel() || !GetCurrentGuild())
		return;

	// send an update subscriptions message
	if (bSendSubscriptionUpdate) {
		if (m_CurrentGuild != 0) {
			UpdateSubscriptions(m_CurrentGuild, sf, false, false, false);
		}
	}
}

void DiscordInstance::RequestMessages(Snowflake sf, ScrollDir::eScrollDir dir, Snowflake source, Snowflake gapper)
{
	if (m_messageRequestsInProgress[source])
		return; // already going

	m_messageRequestsInProgress[source] = true;

	std::string dirStr;
	switch (dir)
	{
		case ScrollDir::BEFORE:
			dirStr = "before";
			break;
		case ScrollDir::AFTER:
			dirStr = "after";
			break;
		case ScrollDir::AROUND:
			dirStr = "around";
			break;
	}

	std::string messageUrl = DISCORD_API "channels/" + std::to_string(sf) + "/messages?";

	if (source != 0)
	{
		messageUrl += dirStr + "=" + std::to_string(source) + "&";
	}

	// Add limit
	messageUrl += "limit=50";

	GetHTTPClient()->PerformRequest(
		true,
		NetRequest::GET,
		messageUrl,
		DiscordRequest::MESSAGES,
		sf,
		"",
		m_token,
		dirStr[1] + std::to_string(gapper)
	);
}

void DiscordInstance::RequestPinnedMessages(Snowflake channel)
{
	std::string messageUrl = DISCORD_API "channels/" + std::to_string(channel) + "/pins";

	GetHTTPClient()->PerformRequest(
		true,
		NetRequest::GET,
		messageUrl,
		DiscordRequest::PINS,
		channel,
		"",
		m_token
	);
}

void DiscordInstance::RequestGuildMembers(Snowflake guild, std::set<Snowflake> members, bool bLoadPresences)
{
	Json data;
	Json guildIdArray, userIdsArray;
	guildIdArray.push_back(guild);

	int i = 0;
	for (auto mem : members) {
		if (mem != 0)
			userIdsArray.push_back(std::to_string(mem));
	}

	if (userIdsArray.empty())
		return;

	data["guild_id"] = guildIdArray;
	data["user_ids"] = userIdsArray;
	data["presences"] = bLoadPresences;
	data["limit"] = nullptr;
	data["query"] = nullptr;

	Json j;
	j["op"] = int(GatewayOp::REQUEST_GUILD_MEMBERS);
	j["d"] = data;

	GetWebsocketClient()->SendMsg(m_gatewayConnId, j.dump());
}

std::string DiscordInstance::LookupChannelNameGlobally(Snowflake sf)
{
	for (auto& gld : m_guilds)
	{
		for (auto& chan : gld.m_channels)
		{
			if (chan.m_snowflake == sf)
				return chan.m_name;
		}
	}

#ifdef _DEBUG
	return "!! " + std::to_string(sf);
#else
	return std::to_string(sf);
#endif
}

std::string DiscordInstance::LookupRoleNameGlobally(Snowflake sf)
{
	for (auto& gld : m_guilds)
	{
		for (auto& role : gld.m_roles)
		{
			if (role.first == sf)
				return role.second.m_name;
		}
	}

#ifdef _DEBUG
	return "deleted-role-" + std::to_string(sf);
#else
	return std::to_string(sf);
#endif
}

std::string DiscordInstance::LookupUserNameGlobally(Snowflake sf, Snowflake gld)
{
	std::string placeholderName = std::to_string(sf);
	Profile* pf = GetProfileCache()->LookupProfile(sf, "", "", "", false);
	if (!pf)
		return placeholderName;

	return pf->GetName(gld);
}

void DiscordInstance::OnSelectGuild(Snowflake sf)
{
	if (m_CurrentGuild == sf)
		return;

	// select the guild
	m_CurrentGuild = sf;
	m_CurrentChannel = 0;

	// check if there are any channels and select the first (for now)
	Guild* pGuild = GetGuild(sf);
	if (!pGuild) return;

	GetFrontend()->RepaintGuildList();
	GetFrontend()->UpdateSelectedGuild();

	if (pGuild->m_bChannelsLoaded && pGuild->m_channels.size())
	{
		// Determine the first channel we should load.
		// TODO: Note, unfortunately this isn't really the right order, but it works for now.
		if (pGuild->m_currentChannel == 0)
		{
			for (auto& ch : pGuild->m_channels)
			{
				if (ch.HasPermission(PERM_VIEW_CHANNEL) && ch.m_channelType != Channel::CATEGORY) {
					pGuild->m_currentChannel = ch.m_snowflake;
					break;
				}
			}
		}

		OnSelectChannel(pGuild->m_currentChannel);
	}

	UpdateSubscriptions(sf, pGuild->m_currentChannel, true, true, true);
}

void OnUpdateAvatar(const std::string& resid);

void DiscordInstance::HandleRequest(void* pRequestPtr)
{
	NetRequest* pRequest = (NetRequest*)pRequestPtr;

	if (pRequest->itype == DiscordRequest::UPLOAD_ATTACHMENT) {
		OnUploadAttachmentFirst(pRequest);
		return;
	}

	if (pRequest->itype == DiscordRequest::UPLOAD_ATTACHMENT_2) {
		OnUploadAttachmentSecond(pRequest);
		return;
	}

	using namespace DiscordRequest;

	// if we didn't get a 200, authentication is invalid and we should exit.
	std::string str;
	bool bExitAfterError = false, bShowMessageBox = false, bSendLoggedOutMessage = false, bJustExitMate = false;

	switch (pRequest->result)
	{
		case HTTP_UNAUTHORIZED:
		{
			bSendLoggedOutMessage = true;
			break;
		}

		case HTTP_FORBIDDEN:
		{
			switch (pRequest->itype)
			{
				case PROFILE:
					return; // no mutual guilds

				case MESSAGES:
					str = "Sorry, you're not allowed to view that channel.";
					OnSelectChannel(0);
					bShowMessageBox = true;
					bExitAfterError = false;
					break;

				case MESSAGE_CREATE:
				{
					bJustExitMate = true;
					// Can't create a message here, buckaroo
					Snowflake nonce = GetIntFromString(pRequest->additional_data);

					GetFrontend()->OnFailedToSendMessage(m_CurrentChannel, nonce);

					Message msg;
					msg.m_type = MessageType::DEFAULT;
					msg.m_snowflake = nonce + 1;
					msg.m_author_snowflake = 1; // *1
					msg.m_author = "Clyde";
					msg.m_message = "Your message could not be delivered. This is usually because you don't share a server "
						"with the recipient or the recipient is only accepting direct messages from friends. You can see th"
						"e full list of reasons here: https://support.discord.com/hc/en-us/articles/360060145013";
					msg.SetTime(time(NULL));

					// *1 - I checked, the official Discord client also does that :)
					GetFrontend()->OnAddMessage(m_CurrentChannel, msg);

					break;
				}
					
				default:
				_default:
					str = "Access to the resource " + pRequest->url + " is forbidden.";
					bShowMessageBox = true;
					break;
			}
			break;
		}

		case HTTP_NOTFOUND:
		{
			if (pRequest->itype == IMAGE)
				return;

			if (pRequest->itype == PROFILE)
				return;

			str = "The following resource " + pRequest->url + " was not found.";
			bExitAfterError = false;
			bShowMessageBox = true;
			break;
		}

		case HTTP_BADREQUEST:
		{
			str = "A bug occurred and the client has sent an invalid request.\n"
				"The resource in question is: " + pRequest->url + "\n\n";
			bExitAfterError = false;
			bShowMessageBox = true;
			break;
		}

		case HTTP_TOOMANYREQS:
		{
			str = "You're issuing requests too fast!  Try again later.  Maybe grab a seltzer and calm down.\n"
				"The resource in question is: " + pRequest->url + "\n\n";
			bExitAfterError = false;
			bShowMessageBox = true;
			break;
		}

		case HTTP_OK:
		case HTTP_NOCONTENT:
			break;

		default:
			str = "Unknown HTTP code " + std::to_string(pRequest->result) + ".\n"
				"The resource in question is: " + pRequest->url + "\n\n" + pRequest->response;
			bExitAfterError = false;
			bShowMessageBox = true;
			break;
	}

	if (bShowMessageBox)
		GetFrontend()->OnGenericError(str);

	if (bExitAfterError)
		GetFrontend()->RequestQuit();

	if (bSendLoggedOutMessage)
		GetFrontend()->OnLoggedOut();

	if (bShowMessageBox || bExitAfterError || bSendLoggedOutMessage || bJustExitMate)
		return;

	if (pRequest->result == HTTP_NOCONTENT)
		// no content to parse
		return;

#ifndef _DEBUG
	try
#endif
	{
		//DebugResponse(pRequest);
		Json j;
		
		if (pRequest->itype != IMAGE && pRequest->itype != IMAGE_ATTACHMENT)
		{
			j = Json::parse(pRequest->response);
		}

		switch (pRequest->itype)
		{
			case IMAGE:
			case IMAGE_ATTACHMENT:
			{
				// Since the request is passed in as a string, this could do for getting the binary shit out of it
				const uint8_t* pData = (const uint8_t*)pRequest->response.data();
				const size_t nSize = pRequest->response.size();

				GetFrontend()->OnAttachmentDownloaded(
					pRequest->itype == IMAGE,
					pData,
					nSize,
					pRequest->additional_data
				);

				break;
			}
			case PROFILE:
			{
				Snowflake userSF = GetSnowflake(j, "id");
				
				GetProfileCache()->LoadProfile(userSF, j);
				
				if (pRequest->key == 0) {
					m_mySnowflake = userSF;
					GetFrontend()->RepaintProfile();
				}

				break;
			}
			case GUILDS:
			{
				// reload guild DB
				m_guilds.clear();
				
				for (auto& elem : j)
					ParseAndAddGuild(elem);

				m_CurrentChannel = 0;

				// select the first one, if possible
				Snowflake guildsf = 0;
				if (m_guilds.size() > 0)
					guildsf = m_guilds.front().m_snowflake;

				OnSelectGuild(guildsf);

				break;
			}
			case GUILD:
			{
				OnFetchedChannels(GetGuild(pRequest->key), pRequest->response);
				break;
			}
			case PINS:
			{
				GetFrontend()->OnLoadedPins(pRequest->key, pRequest->response);
				break;
			}
			case MESSAGES:
			{
				ScrollDir::eScrollDir sd = ScrollDir::BEFORE;
				switch (pRequest->additional_data[0]) {
					case /*b*/'e': sd = ScrollDir::BEFORE; break;
					case /*a*/'f': sd = ScrollDir::AFTER;  break;
					case /*a*/'r': sd = ScrollDir::AROUND; break;
				}
				DbgPrintF("Processing request %d (%c)", sd, pRequest->additional_data[0]);
				uint64_t ts = GetTimeUs();
				GetMessageCache()->ProcessRequest(pRequest->key, sd, (Snowflake)GetIntFromString(pRequest->additional_data.substr(1)), j);
				uint64_t te = GetTimeUs();
				DbgPrintF("Total process took %lld us", te - ts);

				break;
			};
			case GATEWAY:
			{
				m_gatewayUrl = j["url"];

				if (m_gatewayUrl.empty())
				{
					DbgPrintF("Gateway request ended up with empty url");
					break;
				}

				// Add a trailing / if it doesn't exist.
				if (m_gatewayUrl[m_gatewayUrl.size() - 1] != '/')
					m_gatewayUrl += '/';

				DbgPrintF("Discord Gateway url: %s", m_gatewayUrl.c_str());
				StartGatewaySession();
				break;
			}
		}
	}
#ifndef _DEBUG
	catch (Json::exception& ex)
	{
		OnJsonException(ex);
	}
#endif
}

void DiscordInstance::OnFetchedMessages(Snowflake gap, ScrollDir::eScrollDir sd)
{
	GetFrontend()->RefreshMessages(sd, gap);
}

void DiscordInstance::GatewayClosed(int errorCode)
{
	m_gatewayConnId = -1;

	switch (errorCode)
	{
		// Websocketpp codes
		case websocketpp::close::status::abnormal_close:
		case websocketpp::close::status::going_away:
		case websocketpp::close::status::service_restart:
		case CloseCode::LOG_ON_AGAIN:
		case CloseCode::INVALID_SEQ:
		case CloseCode::SESSION_TIMED_OUT:
		{
			//SendMessage(g_Hwnd, WM_LOGINAGAIN, 0, 0);
			StartGatewaySession();
			break;
		}
		case CloseCode::AUTHENTICATION_FAILED:
		case CloseCode::NOT_AUTHENTICATED:
		{
			GetFrontend()->OnLoggedOut();
			break;
		}
		default:
		{
			GetFrontend()->OnSessionClosed(errorCode);
			break;
		}
	}
}

void DiscordInstance::StartGatewaySession()
{
	GetFrontend()->OnConnecting();
	if (m_gatewayConnId)
		GetWebsocketClient()->Close(m_gatewayConnId, websocketpp::close::status::normal);

	int connID = GetWebsocketClient()->Connect(m_gatewayUrl + DISCORD_WSS_DETAILS);

	if (connID < 0)
		GetFrontend()->OnGatewayConnectFailure();

	m_gatewayConnId = connID;
}

typedef void(DiscordInstance::*DispatchFunction)(Json& j);

std::map <std::string, DispatchFunction> g_dispatchFunctions;
void DiscordInstance::HandleGatewayMessage(const std::string& payload)
{
	DbgPrintF("Got Payload: %s [PAYLOAD ENDS HERE]", payload.c_str());

	Json j = Json::parse(payload);

	int op = j["op"];
	using namespace GatewayOp;
	switch (op)
	{
		case HELLO:
		{
			GetFrontend()->SetHeartbeatInterval(j["d"]["heartbeat_interval"]);

			// hello packet - send an identification back
			Json jout;
			jout["op"] = IDENTIFY;

			Json data, presenceData, propertiesData;
			data["token"] = m_token;
			data["compress"] = false;
			// note: real Discord client sends "capabilities" field, undocumented so not gonna bother really
			data["capabilities"] = 16381;

			presenceData["activities"] = Json::array();
			presenceData["afk"] = false;
			presenceData["broadcast"] = nullptr;
			presenceData["since"] = 0;
			presenceData["status"] = "online";

			GetFrontend()->GetIdentifyProperties(propertiesData);

			data["presence"] = presenceData;
			data["properties"] = propertiesData;
			jout["d"] = data;

			GetWebsocketClient()->SendMsg(m_gatewayConnId, jout.dump());

			// send a heartbeat too, we'd like to keep things simple
			SendHeartbeat();

			break;
		}
		case HEARTBACK:
		{
			DbgPrintF("Heartbeat acknowledged");
			break;
		}
		case DISPATCH:
		{
			std::string dispatchCode = j["t"];
			m_heartbeatSequenceId = j["s"];

			DispatchFunction df = g_dispatchFunctions[dispatchCode];
			if (!df)
			{
				DbgPrintF("ERROR: Unknown dispatch function %s", dispatchCode.c_str());
				break;
			}

			//yeah.
			(this->*df)(j);

			break;
		}
		default:
		{
			DbgPrintF("Unhandled message %d", op);
			break;
		}
	}
}

void DiscordInstance::SendHeartbeat()
{
	DbgPrintF("Sending heartbeat");

	using namespace GatewayOp;
	Json j;
	j["op"] = HEARTBEAT;

	if (m_heartbeatSequenceId < 0)
		j["d"] = nullptr;
	else
		j["d"] = m_heartbeatSequenceId;

	GetWebsocketClient()->SendMsg(m_gatewayConnId, j.dump());
}

bool DiscordInstance::EditMessageInCurrentChannel(const std::string& msg, Snowflake msgId)
{
	if (!GetCurrentChannel() || !GetCurrentGuild())
		return false;

	Channel* pChan = GetCurrentChannel();
	
	if (!pChan->HasPermission(PERM_SEND_MESSAGES))
		return false;
	
	Message* pMsg = GetMessageCache()->GetLoadedMessage(pChan->m_snowflake, msgId);
	if (!pMsg)
		return false;

	Json j;
	if (!pMsg->m_referencedMessage.m_bMentionsAuthor) {
		Json alm, prs;
		prs.push_back("users");
		prs.push_back("roles");
		prs.push_back("everyone");
		alm["parse"] = prs;
		alm["replied_user"] = false;
		j["allowed_mentions"] = alm;
	}

	j["content"] = msg;

	GetHTTPClient()->PerformRequest(
		true,
		NetRequest::PATCH,
		DISCORD_API "channels/" + std::to_string(pChan->m_snowflake) + "/messages/" + std::to_string(msgId),
		DiscordRequest::MESSAGE_CREATE,
		pChan->m_snowflake,
		j.dump(),
		m_token,
		std::to_string(msgId)
	);

	return true;
}

bool DiscordInstance::SendMessageToCurrentChannel(const std::string& msg, Snowflake& tempSf, Snowflake replyTo, bool mentionReplied)
{
	if (!GetCurrentChannel() || !GetCurrentGuild())
		return false;

	Channel* pChan = GetCurrentChannel();
	tempSf = CreateTemporarySnowflake();

	if (!pChan->HasPermission(PERM_SEND_MESSAGES))
		return false;
	
	Json j;
	j["content"] = msg;
	j["flags"] = 0;
	j["nonce"] = std::to_string(tempSf);
	j["tts"] = false;
	j["mobile_network_type"] = "unknown";

	if (replyTo)
	{
		Json mr;
		mr["guild_id"] = m_CurrentGuild;
		mr["channel_id"] = m_CurrentChannel;
		mr["message_id"] = replyTo;
		j["message_reference"] = mr;
	}

	if (!mentionReplied)
	{
		Json parse;
		Json allmen;
		parse.push_back("users");
		parse.push_back("roles");
		parse.push_back("everyone");
		allmen["parse"] = parse;
		allmen["replied_user"] = false;
		j["allowed_mentions"] = allmen;
	}
	
	// Nonce used to show which messages were sent.

	GetHTTPClient()->PerformRequest(
		true,
		NetRequest::POST_JSON,
		DISCORD_API "channels/" + std::to_string(pChan->m_snowflake) + "/messages",
		DiscordRequest::MESSAGE_CREATE,
		pChan->m_snowflake,
		j.dump(),
		m_token,
		std::to_string(tempSf)
	);

	// typing indicator goes away when a message is received, so allow sending one in like 100ms
	m_lastTypingSent = GetTimeMs() - TYPING_INTERVAL + 100;

	return true;
}

void DiscordInstance::Typing()
{
	if (!GetCurrentChannel() || !GetCurrentGuild())
		return;

	Channel* pChan = GetCurrentChannel();

	if (m_lastTypingSent + TYPING_INTERVAL >= GetTimeMs())
		return;

	m_lastTypingSent = GetTimeMs();

	GetHTTPClient()->PerformRequest(
		true,
		NetRequest::POST,
		DISCORD_API "channels/" + std::to_string(pChan->m_snowflake) + "/typing",
		0,
		DiscordRequest::TYPING,
		"",
		m_token
	);
}

void DiscordInstance::RequestAcknowledgeChannel(Snowflake channel)
{
	Channel* pChan = GetChannelGlobally(channel);

	if (!pChan) {
		DbgPrintF("DiscordInstance::RequestAcknowledgeChannel requested ack for invalid channel %lld?", channel);
		return;
	}

	Json j;
	j["token"] = nullptr;
	if (pChan->m_lastViewedNum >= 0)
		j["last_viewed"] = pChan->m_lastViewedNum;

	std::string url = DISCORD_API "channels/" + std::to_string(channel) + "/messages/" + std::to_string(pChan->m_lastSentMsg) + "/ack";

	GetHTTPClient()->PerformRequest(
		true,
		NetRequest::POST_JSON,
		url,
		DiscordRequest::ACK,
		0,
		j.dump(),
		m_token
	);
}

void DiscordInstance::RequestAcknowledgeGuild(Snowflake guild)
{
	Guild* pGuild = GetGuild(guild);
	if (!pGuild)
		return;

	Json readStates;
	Json j;

	for (auto& ch : pGuild->m_channels)
	{
		if (!ch.HasUnreadMessages())
			continue;

		Json item;
		item["channel_id"] = std::to_string(ch.m_snowflake);
		item["message_id"] = std::to_string(ch.m_lastSentMsg);
		item["read_state_type"] = 0; //XXX: not sure what this is

		readStates.push_back(item);
	}

	j["read_states"] = readStates;

	GetHTTPClient()->PerformRequest(
		true,
		NetRequest::POST_JSON,
		DISCORD_API "read-states/ack-bulk",
		DiscordRequest::ACK_BULK,
		0,
		j.dump(),
		m_token
	);
}

void DiscordInstance::RequestDeleteMessage(Snowflake chan, Snowflake msg)
{
	std::string url = DISCORD_API "channels/" + std::to_string(chan) + "/messages/" + std::to_string(msg);

	GetHTTPClient()->PerformRequest(
		true,
		NetRequest::DELETE_,
		url,
		0,
		DiscordRequest::DELETE_MESSAGE,
		"",
		m_token
	);
}

void DiscordInstance::UpdateSubscriptions(Snowflake guildId, Snowflake channelId, bool typing, bool activities, bool threads, int rangeMembers)
{
	if (guildId == 0) {
		// TODO
		return;
	}

	Json j, data, subs, guild, channels, rangeParent, rangeChildren[1];

	// Amount of users loaded
	int arr[2] = { 0, rangeMembers };
	rangeChildren[0] = arr;
	rangeParent = rangeChildren;

	if (channelId != 0)
		channels[std::to_string(channelId)] = rangeParent;

	guild["channels"] = channels;
	if (typing)
		guild["typing"] = true;
	if (activities)
		guild["activities"] = true;
	if (threads)
		guild["threads"] = true;

	subs[std::to_string(guildId)] = guild;
	
	data["subscriptions"] = subs;

	j["d"] = data;
	j["op"] = GatewayOp::UPDATE_SUBSCRIPTIONS;

	DbgPrintF("Would be: %s", j.dump().c_str());
	GetWebsocketClient()->SendMsg(m_gatewayConnId, j.dump());
}

void DiscordInstance::RequestLeaveGuild(Snowflake guild)
{
	Json j;
	j["lurking"] = false;

	GetHTTPClient()->PerformRequest(
		true,
		NetRequest::DELETE_,
		DISCORD_API "users/@me/guilds/" + std::to_string(guild),
		DiscordRequest::LEAVE_GUILD,
		guild,
		j.dump(),
		GetToken()
	);
}

void DiscordInstance::JumpToMessage(Snowflake guild, Snowflake channel, Snowflake message)
{
	// jump there!
	if (m_CurrentGuild != guild) {
		OnSelectGuild(guild);
	}
	if (m_CurrentChannel != channel) {
		OnSelectChannel(channel);
	}

	GetFrontend()->JumpToMessage(message);
}

void DiscordInstance::LaunchURL(const std::string& url)
{
	std::string domain, resource;
	SplitURL(url, domain, resource);

	bool isDisGG = domain == "discord.gg";
	if (domain == "discord.gg") {
		DbgPrintF("Invite link: %s\n", url.c_str());
	}
	else if (domain == "discord.com" || domain == "discordapp.com" || domain == "canary.discord.com" || domain == "canary.discordapp.com") {
		// bone headed way to parse the URL
		for (auto& chr : resource)
			if (chr == '/')
				chr = ' ';
		std::stringstream ss(resource);
		
		std::string glds = "";
		Snowflake gldid = 0, chan = 0, msg = 0;
		std::string read;
		if (!(ss >> read))
			return;

		if (read == "channels") {
			if (!(ss >> glds >> chan >> msg))
				return;

			if (glds == "@me") {
				gldid = 0;
			}
			else {
				gldid = GetIntFromString(glds);
				if (gldid == 0 && glds != "0")
					return;
			}

			JumpToMessage(gldid, chan, msg);
			return;
		}
		else return;
	}

	GetFrontend()->LaunchURL(url);
}

void DiscordInstance::SetActivityStatus(eActiveStatus status, bool bRequestServer)
{
	DbgPrintF("Setting activity status to %d", status);
	GetProfile()->m_activeStatus = status;

	if (!bRequestServer)
		return;

	GetSettingsManager()->SetOnlineIndicator(GetProfile()->m_activeStatus);
	GetSettingsManager()->FlushSettings();
}

void DiscordInstance::CloseGatewaySession()
{
	if (m_gatewayConnId < 0) return;

	GetWebsocketClient()->Close(m_gatewayConnId, websocketpp::close::status::normal);
	m_gatewayConnId = -1;
}

void DiscordInstance::SendSettingsProto(const std::vector<uint8_t>& data)
{
	if (data.empty())
		return;

	char* buffer = new char[base64::encoded_size(data.size()) + 1];
	size_t sz = base64::encode(buffer, data.data(), data.size());

	std::string dataToSend(buffer, sz);
	Json j;
	j["settings"] = dataToSend;

	delete[] buffer;

	// send it!!
	GetHTTPClient()->PerformRequest(
		true,
		NetRequest::PATCH,
		DISCORD_API "users/@me/settings-proto/1",
		0,
		0,
		j.dump(),
		m_token
	);
}

void DiscordInstance::LoadUserSettings(const std::string& userSettings)
{
	// load some old stuff so we can check it and send appropriate events
	std::string oldStatus = GetSettingsManager()->GetCustomStatusText();
	eActiveStatus oldActive = GetProfile()->m_activeStatus;

	GetSettingsManager()->LoadDataBase64(userSettings);

	std::string newStatus = GetSettingsManager()->GetCustomStatusText();
	eActiveStatus newActive = GetProfile()->m_activeStatus;

	UpdateSettingsInfo();

	if (oldStatus != newStatus || oldActive != newActive)
		GetFrontend()->RepaintProfile();
}

// Update relevant information after receiving it from the server
void DiscordInstance::UpdateSettingsInfo()
{
	GetProfile()->m_status = GetSettingsManager()->GetCustomStatusText();
	SetActivityStatus(GetSettingsManager()->GetOnlineIndicator(), false);
}

void DiscordInstance::ParsePermissionOverwrites(Channel& channel, nlohmann::json& j)
{
	if (!j.contains("permission_overwrites"))
		return;

	auto& pos = j["permission_overwrites"];
	if (!pos.is_array())
		return;

	channel.m_overwrites.clear();
	for (auto& po : pos)
	{
		Overwrite ow;
		ow.m_allow = GetIntFromString(GetFieldSafe(po, "allow"));
		ow.m_deny = GetIntFromString(GetFieldSafe(po, "deny"));
		ow.m_affected = GetIntFromString(GetFieldSafe(po, "id"));
		ow.m_bIsMember = GetFieldSafeInt(po, "type") != 0;
		channel.m_overwrites[ow.m_affected] = ow;
	}
}

void DiscordInstance::ParseReadStateObject(nlohmann::json& readState, bool bAlternate)
{
	Snowflake id = GetSnowflake(readState, bAlternate ? "channel_id" : "id");
	Snowflake lastMessageId = GetSnowflake(readState, bAlternate ? "message_id" : "last_message_id");
	int flags = GetFieldSafeInt(readState, "flags");
	int lastViewed = GetFieldSafeInt(readState, "last_viewed");
	int mentionCount = GetFieldSafeInt(readState, "mention_count");
	bool bHaveLastViewed = flags & RSTATE_FLAG_HAS_LASTVIEWED;

	// TOTAL FABRICATION:  I guess you have to ignore earlier versions?
	if (readState.contains("version"))
	{
		int version = GetFieldSafeInt(readState, "version");
		if (m_ackVersion >= version) {
			DbgPrintF("Received MESSAGE_ACK version %d, latest version is %d, skipping", version, m_ackVersion);
			return;
		}
	}

	Channel* pChan = GetChannelGlobally(id);
	if (!pChan) {
		// N.B. this is normal, readstates from guilds which you have left remain for some reason
		DbgPrintF("Read state for channel %lld which doesn't exist, skipping", id);
		return;
	}

	pChan->m_lastViewedNum = bHaveLastViewed ? -1 : lastViewed;
	pChan->m_lastViewedMsg = lastMessageId;
	pChan->m_mentionCount = mentionCount;

	GetFrontend()->UpdateChannelAcknowledge(id);
}

void DiscordInstance::ParseChannel(Channel& c, nlohmann::json& chan, int& num)
{
	c.m_snowflake = GetSnowflake(chan, "id");
	c.m_channelType = Channel::eChannelType(int(chan["type"]));

	switch (c.m_channelType)
	{
		default:
		{
			c.m_name = GetFieldSafe(chan, "name");
			if (c.m_name.empty())
				c.m_name = "Channel #" + std::to_string(c.m_snowflake);
			break;
		}
		case Channel::DM:
		case Channel::GROUPDM:
		{
			if (chan.contains("name"))
			{
				// cool, that shall be the name then
				c.m_name = GetFieldSafe(chan, "name");

				if (!c.m_name.empty())
					break;
			}

			// look through the recipients then
			std::string name = "";
			for (auto& rec : chan["recipient_ids"])
			{
				Snowflake userID = GetIntFromString(rec);

				Profile* pf = GetProfileCache()->LookupProfile(userID, "", "", "", true);
				if (!name.empty())
					name += ", ";
				name += pf->m_globalName;
			}

			c.m_name = name;

			break;
		}
	}

	if (chan.contains("position"))
		c.m_pos = chan["position"];
	else
		c.m_pos = num++;

	c.m_lastSentMsg = GetSnowflake(chan, "last_message_id");
	c.m_parentCateg = GetSnowflake(chan, "parent_id");
	c.m_topic = GetFieldSafe(chan, "topic");

	ParsePermissionOverwrites(c, chan);

	if (c.m_channelType == Channel::CATEGORY)
		c.m_parentCateg = c.m_snowflake;

	// TOOD: Add proper sorting order.
}

bool DiscordInstance::SortGuilds()
{
	// Sort.
	m_guilds.sort();

	std::vector<Snowflake> ids = GetSettingsManager()->GetGuildFolders();

	// Convert to set.
	std::map<Snowflake, int> ids_set;
	for (size_t idx = 0; idx < ids.size(); idx++)
		ids_set[ids[idx]] = int(idx);

	// arbitrarily large integer. Surely no one will ever cross 10K guilds, but even then,
	// Discord won't let you join more than 200 (100 if you aren't using nitro), so who cares?!
	int orderOffset = 10000;
	int unordered = 1;

	for (auto& gld : m_guilds)
	{
		auto iter = ids_set.find(gld.m_snowflake);
		if (iter == ids_set.end())
		{
			// not found in guild folders
			gld.m_order = orderOffset - unordered;
			unordered++;
		}
		else
		{
			gld.m_order = orderOffset + iter->second;
		}
	}

	// Check if already sorted, if yes, don't need to do a redundant check
	bool sorted = true;
	int lastOrder = -1;
	for (auto& gld : m_guilds)
	{
		if (lastOrder > gld.m_order) {
			sorted = false;
			break;
		}

		lastOrder = gld.m_order;
	}

	// Ok, now sort again.
	if (sorted)
		return false;

	m_guilds.sort();
	return true;
}

void DiscordInstance::ParseAndAddGuild(nlohmann::json& elem)
{
	std::string uu = elem.dump();

	Guild g;
	g.m_snowflake = GetSnowflake(elem, "id");
	Json& props = elem["properties"];

	if (props.contains("name") && props["name"].is_string())
		g.m_name = props["name"];
	else if (elem.contains("name") && elem["name"].is_string())
		g.m_name = elem["name"];

	if (props.contains("owner_id"))
		g.m_ownerId = GetSnowflake(props, "owner_id");
	else
		g.m_ownerId = 0;

	// parse avatar
	g.m_avatarlnk = GetFieldSafe(props, "icon");

	if (!g.m_avatarlnk.empty())
		GetFrontend()->RegisterIcon(g.m_snowflake, g.m_avatarlnk);

	// parse channels
	Json& channels = elem["channels"];

	Snowflake currChan = 0;
	DbgPrintF("Starting guild %s", g.m_name.c_str());

	int order = 1;
	for (auto& chan : channels)
	{
		Channel c;
		ParseChannel(c, chan, order);
		c.m_parentGuild = g.m_snowflake;
		g.m_channels.push_back(c);
	}

	g.m_channels.sort();

	for (auto& chan : g.m_channels)
	{
		if (currChan == 0 && chan.m_channelType != Channel::CATEGORY)
			currChan = chan.m_snowflake;
	}

	g.m_bChannelsLoaded = true;
	g.m_currentChannel = 0;

	// parse roles
	Json& roles = elem["roles"];
	for (auto& rolej : roles)
	{
		GuildRole role;
		role.Load(rolej);
		g.m_roles[role.m_id] = role;
	}

	// Check if the guild already exists.  If it does, replace its contents.
	// I'm not totally sure why discord sends a GUILD_CREATE event.  Perhaps
	// the server I was testing with is considered a "lazy guild"?
	for (auto& gld : m_guilds)
	{
		if (gld.m_snowflake == g.m_snowflake) {
			gld = g;
			return;
		}
	}

	m_guilds.push_front(g);
}

// DISPATCH FUNCTIONS

#define DECL(Code) g_dispatchFunctions[#Code] = &DiscordInstance::Handle ## Code

void DiscordInstance::InitDispatchFunctions()
{
	g_dispatchFunctions.clear();
	DECL(READY);
	DECL(MESSAGE_CREATE);
	DECL(MESSAGE_UPDATE);
	DECL(MESSAGE_DELETE);
	DECL(MESSAGE_ACK);
	DECL(USER_SETTINGS_PROTO_UPDATE);
	DECL(GUILD_CREATE);
	DECL(GUILD_DELETE);
	DECL(CHANNEL_CREATE);
	DECL(CHANNEL_DELETE);
	DECL(GUILD_MEMBER_LIST_UPDATE);
	DECL(GUILD_MEMBERS_CHUNK);
	DECL(TYPING_START);
	DECL(PASSIVE_UPDATE_V1);

	m_dmGuild.m_name = GetFrontend()->GetDirectMessagesText();
}

#undef DECL

void DiscordInstance::HandleREADY(Json& j)
{
	GetFrontend()->OnConnected();

	std::string str = j.dump();
	Json& data = j["d"];
	m_gatewayResumeUrl = data["resume_gateway_url"];
	m_sessionId = data["session_id"];
	m_sessionType = data["session_type"];

	// ==== reload user
	Json& user = data["user"];

	m_mySnowflake = GetSnowflake(user, "id");

	GetProfileCache()->LoadProfile(m_mySnowflake, user);
	Profile* pf = GetProfile();

	m_dmGuild.m_ownerId = pf->m_snowflake;

	// ==== load user settings
	LoadUserSettings(data["user_settings_proto"]);

	// ==== reload guild DB
	Json& guilds = data["guilds"];
	m_guilds.clear();

	std::vector<Snowflake> guildIds; // used by merged members
	for (auto& elem : guilds) {
		guildIds.push_back(GetSnowflake(elem, "id"));
	}

	// reverse because that's pretty much the order Discord stores guilds in
	for (auto& elem : guilds)
		ParseAndAddGuild(elem);

	SortGuilds();

	m_CurrentChannel = 0;

	// ==== find session object
	Json sesh;
	for (auto& se : data["sessions"]) {
		if (se["session_id"] == m_sessionId) {
			sesh = se;
			break;
		}
	}

	if (sesh.empty())
		DbgPrintF("No session found with id %s!", m_sessionId.c_str());
	assert(!sesh.empty());

	//eActiveStatus status = GetStatusFromString(sesh["status"]);
	//SetActivityStatus(status, false);

	// ==== load users
	if (data.contains("users") && data["users"].is_array())
	{
		auto& membs = data["users"];

		for (auto& memb : membs) {
			Snowflake id = GetSnowflake(memb, "id");
			GetProfileCache()->LoadProfile(id, memb);
		}
	}

	// ==== load merged members
	if (data.contains("merged_members") && data["merged_members"].is_array())
	{
		// no meme, this is MErged MEmberS
		Json& memes = data["merged_members"];
		int idx = -1;
		for (auto& meme : memes)
		{
			idx++;
			// I don't know why there's another array, just pick the first one
			Json& memesub = meme[0];
			Snowflake sf = GetSnowflake(memesub, "user_id");
			GuildMember& gm = pf->m_guildMembers[guildIds[idx]];
			gm.m_nick = GetFieldSafe(memesub, "nick");
			gm.m_avatar = GetFieldSafe(memesub, "avatar");

			// add all roles
			gm.m_roles.clear();
			for (auto& role : memesub["roles"]) {
				Snowflake roleid = GetSnowflakeFromJsonObject(role);
				gm.m_roles.push_back(roleid);
			}
		}
	}

	// ==== load private channels
	if (data.contains("private_channels") && data["private_channels"].is_array())
	{
		auto& chans = data["private_channels"];
		
		Guild* pGld = &m_dmGuild;
		pGld->m_channels.clear();

		Snowflake chan = 0;

		int order = 1;
		for (auto& elem : chans)
		{
			Channel c;
			ParseChannel(c, elem, order);
			c.m_parentGuild = pGld->m_snowflake;
			pGld->m_channels.push_back(c);
		}

		pGld->m_channels.sort();
		pGld->m_bChannelsLoaded = true;
		pGld->m_currentChannel = chan;
	}

	// ==== load read_state
	if (data.contains("read_state") && data["read_state"].is_object())
	{
		// Members: "entries" (array), "partial" (true/false, for now false), "version"
		m_ackVersion = 0;

		Json& readStateData = data["read_state"];
		Json& readStates = readStateData["entries"];
		for (Json& readState : readStates)
			ParseReadStateObject(readState, false);

		m_ackVersion = GetFieldSafeInt(readStateData, "version");
	}

	// ==== load relationships - friends list
	// ==== load resume_gateway_url
	// ==== load user_guild_settings


	// select the first guild, if possible
	Snowflake guildsf = 0;
	if (m_guilds.size() > 0)
		guildsf = m_guilds.front().m_snowflake;

	OnSelectGuild(guildsf);
}

void DiscordInstance::HandleMessageInsertOrUpdate(Json& j, bool bIsUpdate)
{
	Json& data = j["d"];
	Json& author = data["author"];

	Snowflake guildId = GetSnowflake(data, "guild_id");
	Snowflake channelId = GetSnowflake(data, "channel_id");
	Snowflake messageId = GetSnowflake(data, "id");

	Guild* pGld = GetGuild(guildId);
	if (!pGld)
		return;

	Channel* pChan = GetChannel(channelId);
	if (!pChan)
		return;

	Message msg;

	Message *pOldMsg = GetMessageCache()->GetLoadedMessage(channelId, messageId);
	if (pOldMsg)
	{
		msg = *pOldMsg; // copy it asap
		pOldMsg = NULL;
	}
	else if (bIsUpdate) return;

	msg.Load(data, guildId);

	Snowflake oldSentMsg = pChan->m_lastSentMsg;
	pChan->m_lastSentMsg = std::max(pChan->m_lastSentMsg, messageId);

	if (!bIsUpdate)
	{
		if (msg.CheckWasMentioned(m_mySnowflake, guildId))
			pChan->m_mentionCount++;
	}

	if (m_CurrentChannel == channelId)
	{
		if (pChan->m_lastViewedMsg == oldSentMsg)
			pChan->m_lastViewedMsg = pChan->m_lastViewedMsg;
		else
			GetFrontend()->UpdateChannelAcknowledge(channelId);
	}
	else
	{
		GetFrontend()->UpdateChannelAcknowledge(channelId);
	}

	if (bIsUpdate)
		GetFrontend()->OnUpdateMessage(channelId, msg);
	else
		GetFrontend()->OnAddMessage(channelId, msg);
}

void DiscordInstance::HandleMESSAGE_CREATE(Json& j)
{
	HandleMessageInsertOrUpdate(j, false);
}

void DiscordInstance::HandleMESSAGE_UPDATE(Json& j)
{
	HandleMessageInsertOrUpdate(j, true);
}

void DiscordInstance::HandleMESSAGE_DELETE(Json& j)
{
	Json& data = j["d"];
	Snowflake guildId = GetIntFromString(GetFieldSafe(data, "guild_id"));
	Snowflake channelId = GetIntFromString(data["channel_id"]);
	Snowflake messageId = GetIntFromString(data["id"]);
	
	GetMessageCache()->DeleteMessage(channelId, messageId);

	if (m_CurrentGuild != guildId || m_CurrentChannel != channelId)
		return;

	GetFrontend()->OnDeleteMessage(messageId);
}

void DiscordInstance::HandleMESSAGE_ACK(nlohmann::json& j)
{
	Json& data = j["d"];

	// NOTE: Seems like this version of the read state object has different names for channel ID and message ID.  Also differing versions?
	// Maybe you're supposed to ignore things with an earlier version?
	ParseReadStateObject(data, true);
}

void DiscordInstance::HandleUSER_SETTINGS_PROTO_UPDATE(Json& j)
{
	//{"t":"USER_SETTINGS_PROTO_UPDATE","s":X,"op":0,"d":{"settings":{"type":1,"proto":"blabla"},"partial":false}} [PAYLOAD ENDS HERE]
	// d.settings.type and d.partial fields irrelevant probably

	LoadUserSettings(j["d"]["settings"]["proto"]);

	// The guild list may have updated.
	if (SortGuilds())
		GetFrontend()->RepaintGuildList();
}

void DiscordInstance::HandleGUILD_CREATE(Json& j)
{
	Json& data = j["d"];
	ParseAndAddGuild(data);

	GetFrontend()->RepaintGuildList();
}

void DiscordInstance::HandleGUILD_DELETE(Json& j)
{
	Json& data = j["d"];
	Snowflake sf = GetSnowflake(data, "id");

	for (auto iter = m_guilds.begin();
		iter != m_guilds.end();
		++iter)
	{
		if (iter->m_snowflake == sf)
		{
			m_guilds.erase(iter);
			GetFrontend()->RepaintGuildList();

			if (m_CurrentGuild == sf)
			{
				Snowflake sf = 0;
				if (!m_guilds.empty())
					sf = m_guilds.begin()->m_snowflake;
				OnSelectGuild(sf);
			}
			break;
		}
	}
}

void DiscordInstance::HandleCHANNEL_CREATE(Json& j)
{
	Json& data = j["d"];
	Snowflake guildId = GetSnowflake(data, "guild_id");//will return 0 if no guild

	int ord = 0;
	Channel chn;
	ParseChannel(chn, data, ord);

	Guild* pGuild = GetGuild(guildId);
	if (!pGuild)
		return;

	chn.m_parentGuild = pGuild->m_snowflake;
	pGuild->m_channels.push_back(chn);
	pGuild->m_channels.sort();

	if (m_CurrentGuild == guildId)
		GetFrontend()->UpdateChannelList();
}

void DiscordInstance::HandleCHANNEL_DELETE(Json& j)
{
	Json& data = j["d"];
	Snowflake guildId = GetSnowflake(data, "guild_id");//will return 0 if no guild
	Snowflake channelId = GetSnowflake(data, "id");

	Guild* pGuild = GetGuild(guildId);
	if (!GetGuild(guildId))
		return;

	for (auto iter = pGuild->m_channels.begin();
		iter != pGuild->m_channels.end();
		++iter)
	{
		if (iter->m_snowflake == channelId) {
			pGuild->m_channels.erase(iter);
			break;
		}
	}

	if (m_CurrentChannel == channelId)
		OnSelectChannel(0);

	if (m_CurrentGuild == guildId)
		GetFrontend()->UpdateChannelList();
}

static Snowflake GetGroupId(std::string idStr)
{
	/**/ if (idStr == "online")  return GROUP_ONLINE;
	else if (idStr == "offline") return GROUP_OFFLINE;
	else return GetIntFromString(idStr);
}

void DiscordInstance::HandleGUILD_MEMBER_LIST_UPDATE(Json& j)
{
	Json& data = j["d"];
	Snowflake guildId = GetSnowflake(data, "guild_id");

	Guild* pGld = GetGuild(guildId);
	if (!pGld)
		return;

	for (auto& op : data["groups"])
	{
		Snowflake groupId = GetGroupId(GetFieldSafe(op, "id"));
		int count = GetFieldSafeInt(op, "count");

		GuildMember* pGroupMember = pGld->GetGuildMember(groupId);
		pGroupMember->m_groupCount = count;
	}

	pGld->m_memberCount = GetFieldSafeInt(data, "member_count");
	pGld->m_onlineCount = GetFieldSafeInt(data, "online_count");

	for (auto& op : data["ops"])
	{
		std::string opCode = op["op"];

		if (opCode == "SYNC") {
			HandleGuildMemberListUpdate_Sync(guildId, op);
			continue;
		}
		if (opCode == "INSERT") {
			HandleGuildMemberListUpdate_Insert(guildId, op);
			continue;
		}
		if (opCode == "DELETE") {
			HandleGuildMemberListUpdate_Delete(guildId, op);
			continue;
		}
		if (opCode == "UPDATE") {
			HandleGuildMemberListUpdate_Update(guildId, op);
			continue;
		}
		if (opCode == "INVALIDATE") {
			// nothing really
			continue;
		}
		assert(!"TODO"); // what else
	}

	Snowflake currentGroup = 0;
	for (auto& member : pGld->m_members) {
		GuildMember* pMember = pGld->GetGuildMember(member);

		if (pMember->m_bIsGroup)
			currentGroup = pMember->m_groupId;
		else
			pMember->m_groupId = currentGroup;
	}

	GetFrontend()->UpdateMemberList();
}

Snowflake DiscordInstance::ParseGuildMember(Snowflake guild, nlohmann::json& memb, Snowflake userID)
{
	Json& pres = memb["presence"], &user = memb["user"], & roles = memb["roles"];
	Profile* pf = nullptr;

	if (user.is_object())
	{
		Snowflake id = GetSnowflake(user, "id");
		pf = GetProfileCache()->LoadProfile(id, user);
		userID = id;
	}
	else
	{
		// N.B. We should already know stuff about this user if we're parsing here.  When adding a message,
		// a separate "author" member is passed, so profile should have already been loaded from there.
		pf = GetProfileCache()->LookupProfile(userID, "", "", "", false);
	}

	std::string avatarOverride = GetFieldSafe(memb, "avatar");
	std::string nameOverride = GetFieldSafe(memb, "nick");

	GuildMember& gm = pf->m_guildMembers[guild];
	gm.m_user = pf->m_snowflake;
	gm.m_avatar = avatarOverride;
	gm.m_nick = nameOverride;
	gm.m_user = pf->m_snowflake;
	gm.m_joinedAt = ParseTime(GetFieldSafe(memb, "joined_at"));
	gm.m_bIsLoadedFromChunk = true;
	gm.m_groupId = 0; // to be filled in by the group layout

	// TODO: Not sure if this is the guild specific or global status. Probably guild specific
	if (!pres.is_null()) {
		pf->m_activeStatus = GetStatusFromString(GetFieldSafe(pres, "status"));
		if (pres.contains("game") && !pres["game"].is_null()) {
			pf->m_status = GetStatusStringFromGameJsonObject(pres["game"]);
		}
		if (pres.contains("activities") && !pres["activities"].is_null()) {
			Json& activities = pres["activities"];
			if (activities.size() != 0) {
				pf->m_status = GetStatusStringFromGameJsonObject(activities[0]);
			}
		}
	}
	else {
		pf->m_activeStatus = STATUS_OFFLINE;
	}

	gm.m_roles.clear();
	for (auto& it : roles)
		gm.m_roles.push_back(GetSnowflakeFromJsonObject(it));

	return userID;
}

void DiscordInstance::HandlePASSIVE_UPDATE_V1(nlohmann::json& j)
{
	Json& data = j["d"];
	Snowflake guildId = GetSnowflake(data, "guild_id");

	Guild* pGld = GetGuild(guildId);
	if (!pGld)
		return;
	
	if (!data["channels"].is_array())
		return;
	
	for (Json& chan : data["channels"])
	{
		Snowflake channelID = GetSnowflake(chan, "id");
		Snowflake lastMessageID = GetSnowflake(chan, "last_message_id");

		Channel* pChan = pGld->GetChannel(channelID);
		if (!pChan)
			continue;

		pChan->m_lastSentMsg = lastMessageID;
		// XXX: Also "last_pin_timestamp" sometimes.
	}
}

void DiscordInstance::HandleGUILD_MEMBERS_CHUNK(nlohmann::json& j)
{
	Json& data = j["d"];
	Snowflake guildId = GetSnowflake(data, "guild_id");

	Guild* pGld = GetGuild(guildId);
	if (!pGld)
		return;

	std::set<Snowflake> memsToRefresh;
	if (data["members"].is_array())
	{
		for (Json& mem : data["members"])
			memsToRefresh.insert(ParseGuildMember(guildId, mem));
	}

	if (data["not_found"].is_array())
	{
		for (auto& nf : data["not_found"]) {
			GetProfileCache()->ProfileDoesntExist(GetSnowflakeFromJsonObject(nf), guildId);
		}
	}

	if (m_CurrentGuild == guildId)
		GetFrontend()->RefreshMembers(memsToRefresh);
}

void DiscordInstance::HandleTYPING_START(nlohmann::json& j)
{
	Json& data = j["d"];
	Snowflake guildID = 0;
	Snowflake userID = GetSnowflake(data, "user_id");
	Snowflake chanID = GetSnowflake(data, "channel_id");
	if (data.contains("guild_id"))
	{
		guildID = GetSnowflake(data, "guild_id");
		userID = ParseGuildMember(guildID, data["member"], userID);
	}

	time_t currTime = time(NULL);
	time_t startTime = time_t(GetFieldSafeInt(data, "timestamp"));

	// If there is major desync just use the current time. Could be because of time zone differences for example
	if (abs(int64_t(startTime) - int64_t(currTime)) > 10000)
		startTime = currTime;

	GetFrontend()->OnStartTyping(userID, guildID, chanID, startTime);
}

Snowflake DiscordInstance::ParseGuildMemberOrGroup(Snowflake guild, nlohmann::json& item)
{
	if (item.contains("group")) {
		Json& grp = item["group"];

		// fake a profile to integrate with the existing stuff
		Snowflake groupId = GetGroupId(GetFieldSafe(grp, "id"));
		Profile* pf = GetProfileCache()->LookupProfile(groupId, "GROUP", "GROUP", "", false);

		GuildMember& gm = pf->m_guildMembers[guild];
		gm.m_groupId = groupId;
		gm.m_bIsGroup = true;
		return gm.m_groupId;
	}
	else if (item.contains("member"))
	{
		assert(item["member"].is_object());
		return ParseGuildMember(guild, item["member"]);
	}
	else
	{
		assert(!"Huh?");
		return 0;
	}
}

void DiscordInstance::HandleGuildMemberListUpdate_Sync(Snowflake guild, nlohmann::json& jx)
{
	Guild* pGld = GetGuild(guild);
	assert(pGld);

	pGld->m_members.clear();

	// TODO: j["range"].  Is the range we told discord about
	// when subscribing to the channel using websocket.
	int m_ordinal = 0;
	Json& items = jx["items"];
	for (auto& item : items)
		pGld->m_members.push_back(ParseGuildMemberOrGroup(guild, item));
}

void DiscordInstance::HandleGuildMemberListUpdate_Insert(Snowflake guild, nlohmann::json& j)
{
	Guild* pGld = GetGuild(guild);
	assert(pGld);

	Json& item = j["item"];
	int index = j["index"];
	Snowflake sf = ParseGuildMemberOrGroup(guild, item);

	if (index < 0 || index >= pGld->m_members.size() + 1) {
		assert(!"huh");
		return;
	}

	pGld->m_members.insert(pGld->m_members.begin() + index, sf);
}

void DiscordInstance::HandleGuildMemberListUpdate_Delete(Snowflake guild, nlohmann::json& j)
{
	Guild* pGld = GetGuild(guild);
	assert(pGld);

	int index = j["index"];

	if (index < 0 || index >= pGld->m_members.size()) {
		assert(!"huh");
		return;
	}
	
	Snowflake memberId = pGld->m_members[index];
	GuildMember& member = GetProfileCache()->LookupProfile(memberId, "", "", "", false)->m_guildMembers[guild];

	if (member.m_bIsGroup) {
		// also remove that group
		//TODO: GetProfileCache()->ForgetProfile(memberId);
	}

	pGld->m_members.erase(pGld->m_members.begin() + index);
}

void DiscordInstance::HandleGuildMemberListUpdate_Update(Snowflake guild, nlohmann::json& j)
{
	Guild* pGld = GetGuild(guild);
	assert(pGld);

	int index = j["index"];

	if (index < 0 || index >= pGld->m_members.size()) {
		assert(!"huh");
		return;
	}

	pGld->m_members[index] = ParseGuildMemberOrGroup(guild, j["item"]);
}

void DiscordInstance::OnUploadAttachmentFirst(NetRequest* pReq)
{
	auto& ups = m_pendingUploads;

	if (pReq->result != HTTP_OK)
	{
		// Delete enqueued upload
		auto iter = ups.find(pReq->key);
		if (iter != ups.end())
			ups.erase(iter);

		GetFrontend()->OnFailedToUploadFile(pReq->additional_data, pReq->result);
		return;
	}

	Json j = Json::parse(pReq->response);
	assert(j["attachments"].size() == 1);

	for (auto& att : j["attachments"])
	{
		Snowflake id = GetSnowflakeFromJsonObject(att["id"]);

		PendingUpload& up = ups[id];
		up.m_uploadUrl = GetFieldSafe(att, "upload_url");
		up.m_uploadFileName = GetFieldSafe(att, "upload_filename");

		// Send data to the upload URL
		uint8_t* pNewData = new uint8_t[up.m_data.size()];
		memcpy(pNewData, up.m_data.data(), up.m_data.size());

		GetHTTPClient()->PerformRequest(
			true,
			NetRequest::PUT_OCTETS,
			up.m_uploadUrl,
			DiscordRequest::UPLOAD_ATTACHMENT_2,
			pReq->key,
			"",
			GetToken(),
			"",
			nullptr, // default processing
			pNewData,
			up.m_data.size()
		);
	}
}

void DiscordInstance::OnUploadAttachmentSecond(NetRequest* pReq)
{
	auto& ups = m_pendingUploads;
	auto iter = ups.find(pReq->key);
	if (iter == ups.end())
		return;

	if (pReq->result != HTTP_OK)
	{
		// Delete enqueued upload
		ups.erase(iter);
		GetFrontend()->OnFailedToUploadFile(pReq->additional_data, pReq->result);
		return;
	}

	// Ok!  Now that we have uploaded the data, time to send the actual message.
	PendingUpload& up = iter->second;

	Json j, attachment, stickerIds;
	attachment["id"] = std::to_string(pReq->key);
	attachment["filename"] = up.m_name;
	attachment["uploaded_filename"] = up.m_uploadFileName;

	stickerIds = Json::array();

	j["content"] = up.m_content;
	j["nonce"] = std::to_string(up.m_tempSF);
	j["channel_id"] = std::to_string(up.m_channelSF);
	j["type"] = 0;
	j["attachments"].push_back(attachment);
	j["sticker_ids"] = stickerIds;

	GetHTTPClient()->PerformRequest(
		true,
		NetRequest::POST_JSON,
		DISCORD_API "channels/" + std::to_string(up.m_channelSF) + "/messages",
		DiscordRequest::MESSAGE_CREATE,
		up.m_channelSF,
		j.dump(),
		GetToken(),
		std::to_string(up.m_tempSF)
	);

	// typing indicator goes away when a message is received, so allow sending one in like 100ms
	m_lastTypingSent = GetTimeMs() - TYPING_INTERVAL + 100;

	// Then erase it
	ups.erase(iter);
}

bool DiscordInstance::SendMessageAndAttachmentToCurrentChannel(
	const std::string& msg,
	Snowflake& tempSf,
	uint8_t* attData,
	size_t attSize,
	const std::string& attName)
{
	if (!GetCurrentChannel() || !GetCurrentGuild())
		return false;

	Channel* pChan = GetCurrentChannel();
	tempSf = CreateTemporarySnowflake();

	if (!pChan->HasPermission(PERM_SEND_MESSAGES) || !pChan->HasPermission(PERM_ATTACH_FILES))
		return false;
	
	Json file;
	file["filename"]  = attName;
	file["file_size"] = int(attSize);
	file["is_clip"]   = false;
	file["id"]        = std::to_string(m_nextAttachmentID);

	Json files;
	files.push_back(file);

	Json j;
	j["files"] = files;

	m_pendingUploads[m_nextAttachmentID] = PendingUpload(attName, attData, attSize, msg, tempSf, m_CurrentChannel);

	GetHTTPClient()->PerformRequest(
		true,
		NetRequest::POST_JSON,
		DISCORD_API "channels/" + std::to_string(m_CurrentChannel) + "/attachments",
		DiscordRequest::UPLOAD_ATTACHMENT,
		m_nextAttachmentID,
		j.dump(),
		m_token,
		attName,
		nullptr // default processing
	);

	m_nextAttachmentID++;
	return true;
}