#include <nlohmann/json.h>
#include <boost/base64/base64.hpp>
#include "DiscordInstance.hpp"
#include "WebsocketClient.hpp"
#include "SettingsManager.hpp"
#include "Util.hpp"
#include "Frontend.hpp"
#include "HTTPClient.hpp"
#include "DiscordClientConfig.hpp"

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

	if (!game.contains("type") || !game["type"].is_number_integer()) {
		DbgPrintF("Returning nothing because type didn't exist");
		return "";
	}

	int type = game["type"];
	switch (type) {
		default:
			return "";
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
	if (m_channelDenyList.find(sf) != m_channelDenyList.end() ||
		(pChan && !pChan->HasPermission(PERM_VIEW_CHANNEL)))
	{
		GetFrontend()->OnCantViewChannel(pChan->m_name);
		return;
	}

	m_channelHistory.AddToHistory(m_CurrentChannel);
	m_CurrentChannel = sf;

	pGuild->m_currentChannel = m_CurrentChannel;

	GetFrontend()->UpdateSelectedChannel();

	if (!GetCurrentChannel() || !GetCurrentGuild())
		return;

	// send an update subscriptions message
	if (bSendSubscriptionUpdate) {
		UpdateSubscriptions(m_CurrentGuild, sf, false, false, false);
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

	std::string messageUrl = GetDiscordAPI() + "channels/" + std::to_string(sf) + "/messages?";

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
	std::string messageUrl = GetDiscordAPI() + "channels/" + std::to_string(channel) + "/pins";

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

void DiscordInstance::RequestGuildMembers(Snowflake guild, std::string query, bool bLoadPresences, int limit)
{
	Json guildIdArray;
	guildIdArray.push_back(guild);

	Json data;
	data["query"] = query;
	data["limit"] = limit;
	data["user_ids"] = nullptr;
	data["guild_id"] = guildIdArray;
	data["presences"] = bLoadPresences;

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

std::string DiscordInstance::LookupRoleName(Snowflake sf, Snowflake guildID)
{
	Guild* pGld = GetGuild(guildID);
	if (pGld)
	{
		for (const auto& role : pGld->m_roles)
		{
			if (role.first == sf)
				return role.second.m_name;
		}
	}

	return "deleted-role-" + std::to_string(sf);
}

std::string DiscordInstance::LookupRoleNameGlobally(Snowflake sf)
{
	for (const auto& gld : m_guilds)
	{
		for (const auto& role : gld.m_roles)
		{
			if (role.first == sf)
				return role.second.m_name;
		}
	}

	return "deleted-role-" + std::to_string(sf);
}

std::string DiscordInstance::LookupUserNameGlobally(Snowflake sf, Snowflake gld)
{
	std::string placeholderName = std::to_string(sf);
	Profile* pf = GetProfileCache()->LookupProfile(sf, "", "", "", false);
	if (!pf)
		return placeholderName;

	return pf->GetName(gld);
}

#define MATCH_TEXT   (1 << 0)
#define MATCH_VOICE  (1 << 1)
#define MATCH_DMS    (1 << 2)
#define MATCH_GUILDS (1 << 3)

void DiscordInstance::SearchSubGuild(std::vector<QuickMatch>& matches, Guild* pGuild, int matchFlags, const char* queryPtr)
{
	for (auto& chan : pGuild->m_channels)
	{
		if (chan.m_snowflake == m_CurrentChannel)
			continue;

		if (chan.IsText()) {
			if (~matchFlags & MATCH_TEXT)
				continue;
		}
		else if (chan.IsDM()) {
			if (~matchFlags & MATCH_DMS)
				continue;
		}
		else if (chan.IsVoice()) {
			if (~matchFlags & MATCH_VOICE)
				continue;
		}
		else continue;

		if (!chan.HasPermission(PERM_VIEW_CHANNEL))
			continue;

		float fzc = CompareFuzzy(chan.m_name, queryPtr);
		if (fzc != 0.0f)
			matches.push_back(QuickMatch(true, chan.m_snowflake, fzc, chan.m_name));
	}
}

std::vector<QuickMatch> DiscordInstance::Search(const std::string& query)
{
	std::vector<QuickMatch> matches;

	if (query.empty())
	{
		// Special mode - Show the last three channels.
		for (int i = 0; i < C_CHANNEL_HISTORY_MAX; i++)
		{
			Snowflake chan = m_channelHistory.m_history[i];
			if (!chan)
				continue;

			Channel* pChan = GetChannel(chan);
			if (!pChan)
				continue;
			if (!pChan->HasPermission(PERM_VIEW_CHANNEL))
				continue;

			// Calculate a fake fuzzy factor to avoid effects of sorting.
			float ff = float(C_CHANNEL_HISTORY_MAX - i) / float(C_CHANNEL_HISTORY_MAX);

			matches.push_back(QuickMatch(true, chan, ff, pChan->m_name));
		}
	}
	else
	{
		char firstChar = query[0];
		int matchFlags = MATCH_TEXT | MATCH_VOICE | MATCH_DMS | MATCH_GUILDS;
		bool cutoff = false;

		switch (firstChar)
		{
			case '#': matchFlags = MATCH_TEXT;   cutoff = true; break;
			case '@': matchFlags = MATCH_DMS;    cutoff = true; break;
			case '!': matchFlags = MATCH_VOICE;  cutoff = true; break;
			case '*': matchFlags = MATCH_GUILDS; cutoff = true; break;
		}

		const char* queryPtr = query.c_str();
		if (cutoff) queryPtr++;

		if (matchFlags & (MATCH_TEXT | MATCH_DMS | MATCH_VOICE))
		{
			for (auto& gld : m_guilds)
				SearchSubGuild(matches, &gld, matchFlags, queryPtr);

			SearchSubGuild(matches, &m_dmGuild, matchFlags, queryPtr);
		}

		if (matchFlags & MATCH_GUILDS)
		{
			for (auto& gld : m_guilds)
			{
				if (gld.m_snowflake == m_CurrentGuild)
					continue;

				float fzc = CompareFuzzy(gld.m_name, queryPtr);

				if (fzc != 0.0f)
					matches.push_back(QuickMatch(false, gld.m_snowflake, fzc, gld.m_name));
			}
		}
	}

	// Sort the matches
	std::sort(matches.begin(), matches.end());

	return matches;
}

void DiscordInstance::OnSelectGuild(Snowflake sf, Snowflake chan)
{
	if (m_CurrentGuild == sf)
	{
		if (chan)
			OnSelectChannel(chan);

		return;
	}

	m_channelHistory.AddToHistory(m_CurrentChannel);

	// select the guild
	m_CurrentGuild = sf;

	// check if there are any channels and select the first (for now)
	Guild* pGuild = GetGuild(sf);
	if (!pGuild) return;
	
	GetFrontend()->UpdateSelectedGuild();

	if (pGuild->m_bChannelsLoaded && pGuild->m_channels.size())
	{
		// Determine the first channel we should load.
		// TODO: Note, unfortunately this isn't really the right order, but it works for now.
		if (!chan && pGuild->m_currentChannel == 0)
		{
			for (auto& ch : pGuild->m_channels)
			{
				if (ch.HasPermission(PERM_VIEW_CHANNEL) && ch.m_channelType != Channel::CATEGORY) {
					pGuild->m_currentChannel = ch.m_snowflake;
					break;
				}
			}
		}

		if (!chan)
			chan = pGuild->m_currentChannel;

		OnSelectChannel(chan);
	}
	else
	{
		OnSelectChannel(0);
	}

	UpdateSubscriptions(sf, chan, true, true, true);
}

void OnUpdateAvatar(const std::string& resid);

void DiscordInstance::HandleRequest(NetRequest* pRequest)
{
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

	// OpenSSL debug stuff
	unsigned long err_code;
	char errStringBuffer[260]; // OpenSSL says buffer must be >256 bytes

	if (pRequest->IsMediaRequest() && !pRequest->IsOk()) {
		DbgPrintF("WARNING: Request for media from url %s failed with error %d. Message: %s", pRequest->url.c_str(), pRequest->result, pRequest->ErrorMessage().c_str());
		GetFrontend()->OnAttachmentFailed(pRequest->itype == IMAGE, pRequest->additional_data);
		return;
	}

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
				{
					Channel* pChan = GetChannelGlobally(pRequest->key);
					std::string name;
					if (pChan)
						name = "#" + pChan->m_name;
					else
						name = "Unknown Channel " + std::to_string(pRequest->key);

					str = "Sorry, you're not allowed to view the channel " + name + ".";

					if (m_CurrentChannel == pRequest->key)
						OnSelectChannel(0);

					m_channelDenyList.insert(pRequest->key);

					bShowMessageBox = true;
					bExitAfterError = false;
					break;
				}

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
					str = "Access to the resource " + pRequest->url + " is forbidden.";
					bShowMessageBox = true;
					break;
			}
			break;
		}

		case HTTP_NOTFOUND:
			if (pRequest->itype == USER_NOTE)
				// Just means the note doesn't exist.
				return;

		case HTTP_BADGATEWAY:
		case HTTP_UNSUPPMEDIA:
		{
			DbgPrintF("Resource %s not loaded due to error %d", pRequest->url.c_str(), pRequest->result);

			if (pRequest->IsMediaRequest()) {
				GetFrontend()->OnAttachmentFailed(pRequest->itype == IMAGE, pRequest->additional_data);
				return;
			}

			if (pRequest->itype == PROFILE) {
				DbgPrintF("Could not load profile %lld", pRequest->key);
				return;
			}

			std::string suffix = " was not found. (404)";
			switch (pRequest->result) {
				case HTTP_BADGATEWAY:
					suffix = " could not be accessed due to a bad gateway. (502)";
					break;
				case HTTP_UNSUPPMEDIA:
					suffix = " could not be accessed due to an unsupported media type. (415)";
					break;
			}

			str = "The following resource " + pRequest->url + suffix + "\n\nAdditionally, the server responded with the following:\n" +
				pRequest->response;
			bExitAfterError = false;
			bShowMessageBox = true;
			break;
		}

		case HTTP_BADREQUEST:
		{
			str = "A bug occurred and the client has sent an invalid request.\n"
				"The resource in question is: " + pRequest->url + "\n\n" + pRequest->response;
			bExitAfterError = false;
			bShowMessageBox = true;
			break;
		}

		case HTTP_TOOMANYREQS:
		{
			str = "You're issuing requests too fast!  Try again later.  Maybe grab a seltzer and calm down.\n"
				"The resource in question is: " + pRequest->url;
			bExitAfterError = false;
			bShowMessageBox = true;
			break;
		}

		case HTTP_OK:
		case HTTP_NOCONTENT:
			break;

		default:
			str = "Unknown HTTP code " + std::to_string(pRequest->result) + " (" + pRequest->ErrorMessage() + ")\n"
				"URL requested: " + pRequest->url + "\n\nResponse:" + pRequest->response;

			while ((err_code = ERR_get_error()) != 0) {
				str += "\nAdditional OpenSSL Error: " + std::string(ERR_error_string(err_code, errStringBuffer));
			}

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
				DbgPrintF("Attachment at url %s downloaded.", pRequest->url.c_str());

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
				Snowflake userSF = 0;
				if (j.contains("user"))
					userSF = GetSnowflake(j["user"], "id");
				else
					userSF = GetSnowflake(j, "id");
				
				GetProfileCache()->LoadProfile(userSF, j);
				
				if (pRequest->key == 0) {
					m_mySnowflake = userSF;
					GetFrontend()->RepaintProfile();
				}

				break;
			}
			case USER_NOTE:
			{
				Snowflake userSF = GetSnowflake(j, "note_user_id");
				if (userSF == 0)
					break;

				Profile* pf = GetProfileCache()->LookupProfile(userSF, "", "", "", false);
				if (pf) {
					pf->m_note = GetFieldSafe(j, "note");
					pf->m_bNoteFetched = true;

					GetFrontend()->UpdateProfilePopout(userSF);
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

				GetFrontend()->RepaintGuildList();

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
				Channel* pChan = GetDiscordInstance()->GetChannel(pRequest->key);
				if (!pChan)
					break;

				ScrollDir::eScrollDir sd = ScrollDir::BEFORE;
				switch (pRequest->additional_data[0]) {
					case /*b*/'e': sd = ScrollDir::BEFORE; break;
					case /*a*/'f': sd = ScrollDir::AFTER;  break;
					case /*a*/'r': sd = ScrollDir::AROUND; break;
				}
				DbgPrintF("Processing request %d (%c)", sd, pRequest->additional_data[0]);
				uint64_t ts = GetTimeUs();
				GetMessageCache()->ProcessRequest(
					pRequest->key,
					sd,
					(Snowflake)GetIntFromString(pRequest->additional_data.substr(1)),
					j,
					pChan->GetTypeSymbol() + pChan->m_name
				);
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
		case websocketpp::close::status::normal:
		case CloseCode::LOG_ON_AGAIN:
		case CloseCode::INVALID_SEQ:
		case CloseCode::SESSION_TIMED_OUT:
		{
			GetFrontend()->OnLoginAgain();
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

std::string DiscordInstance::TransformMention(const std::string& source, Snowflake guild, Snowflake channel)
{
	if (source.empty())
		return source;

	// Look for people with that name in the guild's known list
	std::string longestMatchStr;
	std::string longestMatchMeta;
	Snowflake longestMatchID = 0;

	char firstChar = source[0];

	if (guild)
	{
		Guild* pGuild = GetGuild(guild);
		if (!pGuild)
			return source;

		switch (firstChar)
		{
			case '@':
				// Look for people whose user names the source starts with.
				for (auto pfid : pGuild->m_knownMembers)
				{
					Profile* pf = GetProfileCache()->LookupProfile(pfid, "", "", "", false);
					std::string uname = "@" + pf->GetUsername();
					if (!BeginsWith(source, uname))
						continue;

					assert(uname.size() <= source.size());
					if (longestMatchStr.size() >= uname.size())
						continue;

					longestMatchStr = uname;
					longestMatchID = pf->m_snowflake;
				}
				break;

			case '#':
				// Look for channels whose names the source starts with.
				for (const auto& chan : pGuild->m_channels)
				{
					std::string cname = "#" + chan.m_name;
					if (!BeginsWith(source, cname))
						continue;

					assert(cname.size() <= source.size());
					if (longestMatchStr.size() >= cname.size())
						continue;

					longestMatchStr = cname;
					longestMatchID = chan.m_snowflake;
				}
				break;

			case ':':
				// Look for server emojis whose names the source starts with.
				for (const auto& em : pGuild->m_emoji)
				{
					std::string ename = ":" + em.second.m_name + ":";
					if (!BeginsWith(source, ename))
						continue;

					assert(ename.size() <= source.size());
					if (longestMatchStr.size() >= ename.size())
						continue;

					longestMatchStr = ename;
					longestMatchID = em.first;
					longestMatchMeta = ":" + em.second.m_name;
				}
				break;
		}
	}
	else
	{
		Channel* pChan = m_dmGuild.GetChannel(channel);
		if (!pChan)
			return source;

		switch (firstChar)
		{
			case '@':
				// Look for recipients in this DM/group DM.
				for (auto pfid : pChan->m_recipients)
				{
					Profile* pf = GetProfileCache()->LookupProfile(pfid, "", "", "", false);
					std::string uname = "@" + pf->GetUsername();
					if (!BeginsWith(source, uname))
						continue;

					if (longestMatchStr.size() >= uname.size())
						continue;

					longestMatchStr = uname;
					longestMatchID = pf->m_snowflake;
				}
				break;
			// Note: don't handle channel mentions, you have to add those by ID like <#idhere>
		}
	}

	if (longestMatchID == 0)
		return source;
	
	assert(source.substr(0, longestMatchStr.size()) == longestMatchStr);

	std::string
	str  = "<";
	str += longestMatchMeta;
	str += firstChar;
	str += std::to_string(longestMatchID);
	str += ">";
	str += source.substr(longestMatchStr.size());
	return str;
}

std::string DiscordInstance::ResolveMentions(const std::string& str, Snowflake guild, Snowflake channel)
{
	bool hasMent = false;
	for (char c : str) {
		if (c == '@' || c == '#' || c == ':') {
			hasMent = true;
			break;
		}
	}

	if (!hasMent)
		return str;

	// look for each word with an @
	std::string finalStr = "";
	finalStr.reserve(str.size() * 2);

	for (size_t i = 0; i < str.size(); )
	{
		if (str[i] != '@' && str[i] != '#' && str[i] != ':')
		{
			finalStr += str[i];
			i++;
			continue;
		}

		char firstChr = str[i];

		// Have an @, #, or :, search for another such symbol, because that denotes the beginning of
		// another mention. I'd break it at spaces too, but some user names also start with spaces.
		size_t j;
		for (j = i + 1; j < str.size(); j++) {
			if (str[j] == '@' || str[j] == '#' || str[j] == ':')
				break;
		}

		if (firstChr == ':') {
			if (j == str.size() || str[j] != ':') {
				// not a valid emoji, just skip this guy
				finalStr += str.substr(i, j - i);
				i = j;
				continue;
			}
			else j++;
		}

		std::string addition = TransformMention(str.substr(i, j - i), guild, channel);
		finalStr += addition;
		i = j;
	}

	return finalStr;
}

std::string DiscordInstance::ReverseMentions(const std::string& message, Snowflake guildID, bool ttsMode)
{
	bool hasMent = false;
	bool hasOpen = false;
	bool hasClose = false;
	for (char c : message) {
		if (c == '@' || c == '#' || c == ':')
			hasMent = true;
		if (c == '<')
			hasOpen = true;
		if (c == '>')
			hasClose = true;
		if (hasMent && hasOpen && hasClose)
			break;
	}

	if (!hasMent || !hasOpen || !hasClose)
		// no point
		return message;

	std::string newStr = "";

	for (size_t i = 0; i < message.size(); i++)
	{
		if (message[i] != '<')
		{
		DefaultHandling:
			newStr += message[i];
			continue;
		}

		size_t mentStart = i;
		i++;

		for (; i < message.size() && message[i] != '<' && message[i] != '>'; i++);

		if (i == message.size() || message[i] != '>') {
		ErrorParsing:
			i = mentStart;
			goto DefaultHandling;
		}

		i++;
		std::string mentStr = message.substr(mentStart, i - mentStart);
		i--; // go back so that this character is skipped.

		// Now it's time to try to decode that mention.
		if (mentStr.size() < 4)
			goto ErrorParsing;

		if (mentStr[0] != '<' || mentStr[mentStr.size() - 1] != '>') {
			assert(!"Then how did we get here?");
			goto ErrorParsing;
		}

		// tear off the '<' and '>'
		mentStr = mentStr.substr(1, mentStr.size() - 2);
		std::string resultStr = mentStr;

		char mentType = mentStr[0];
		switch (mentType)
		{
		case '@':
		{
			bool isRole = false;
			bool hasExclam = false;

			if (mentStr[1] == '&')
				isRole = true;
			// not totally sure what this does. I only know that certain things use it
			if (mentStr[1] == '!')
				hasExclam = true;

			std::string mentDest = mentStr.substr((isRole || hasExclam) ? 2 : 1);
			Snowflake sf = (Snowflake)GetIntFromString(mentDest);

			if (isRole)
				resultStr = (ttsMode ? "" : "@") + LookupRoleName(sf, guildID);
			else
				resultStr = (ttsMode ? "" : "@") + LookupUserNameGlobally(sf, guildID);

			break;
		}

		case '#':
		{
			std::string mentDest = mentStr.substr(1);
			Snowflake sf = (Snowflake)GetIntFromString(mentDest);
			Channel* pChan = GetChannelGlobally(sf);
			if (!pChan)
				goto ErrorParsing;

			resultStr = (ttsMode ? "" : pChan->GetTypeSymbol()) + pChan->m_name;
			break;
		}

		case ':':
		{
			// look for the other :
			size_t i;
			for (i = 1; i < mentStr.size(); i++) {
				if (mentStr[i] == ':')
					break;
			}
			if (i == mentStr.size())
				goto ErrorParsing;

			std::string mentDest = mentStr.substr(i + 1);
			Snowflake sf = (Snowflake)GetIntFromString(mentDest);
			
			Guild* pGld = GetGuild(guildID);
			if (!pGld) {
				// Actually trust the first part, we have no way to check I don't think
			TrustFirstPart:
				resultStr = mentStr.substr(0, i + 1);
			}
			else {
				// Look up the name of the emoji in the guild.
				auto emit = pGld->m_emoji.find(sf);
				if (emit == pGld->m_emoji.end())
					goto TrustFirstPart;
				
				resultStr = ":" + emit->second.m_name + ":";
			}

			assert(!resultStr.empty() && resultStr[0] == ':' && resultStr[resultStr.size() - 1] == ':');

			if (ttsMode && resultStr.size() >= 2)
				resultStr = " emoji " + resultStr.substr(1, resultStr.size() - 2);

			break;
		}

		default:
			goto ErrorParsing;
		}

		newStr += resultStr;
	}

	return newStr;
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

			propertiesData = GetClientConfig()->Serialize();

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
			if (!j.contains("t")) {
				DbgPrintF("Error, dispatch opcode doesn't contain type");
				break;
			}

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

bool DiscordInstance::EditMessageInCurrentChannel(const std::string& msg_, Snowflake msgId)
{
	if (!GetCurrentChannel() || !GetCurrentGuild())
		return false;

	std::string msg = ResolveMentions(msg_, m_CurrentGuild, m_CurrentChannel);

	Channel* pChan = GetCurrentChannel();
	
	if (!pChan->HasPermission(PERM_SEND_MESSAGES))
		return false;
	
	MessagePtr pMsg = GetMessageCache()->GetLoadedMessage(pChan->m_snowflake, msgId);
	if (!pMsg)
		return false;

	Json j;
	if (pMsg->m_pReferencedMessage &&
		!pMsg->m_pReferencedMessage->m_bMentionsAuthor) {
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
		GetDiscordAPI() + "channels/" + std::to_string(pChan->m_snowflake) + "/messages/" + std::to_string(msgId),
		DiscordRequest::MESSAGE_CREATE,
		pChan->m_snowflake,
		j.dump(),
		m_token,
		std::to_string(msgId)
	);

	return true;
}

bool DiscordInstance::SendMessageToCurrentChannel(const std::string& msg_, Snowflake& tempSf, Snowflake replyTo, bool mentionReplied)
{
	if (!GetCurrentChannel() || !GetCurrentGuild())
		return false;

	std::string msg = ResolveMentions(msg_, m_CurrentGuild, m_CurrentChannel);

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
		if (m_CurrentGuild)
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
		GetDiscordAPI() + "channels/" + std::to_string(pChan->m_snowflake) + "/messages",
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
		GetDiscordAPI() + "channels/" + std::to_string(pChan->m_snowflake) + "/typing",
		0,
		DiscordRequest::TYPING,
		"",
		m_token
	);
}

void DiscordInstance::RequestAcknowledgeMessages(Snowflake channel, Snowflake message, bool manual)
{
	Channel* pChan = GetChannelGlobally(channel);

	if (!pChan) {
		DbgPrintF("DiscordInstance::RequestAcknowledgeChannel requested ack for invalid channel %lld?", channel);
		return;
	}

	int mentCount = GetMessageCache()->GetMentionCountSince(channel, message, m_mySnowflake);

	Json j;
	j["manual"] = manual;
	j["mention_count"] = mentCount;

	std::string url = GetDiscordAPI() + "channels/" + std::to_string(channel) + "/messages/" + std::to_string(message) + "/ack";

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

void DiscordInstance::RequestAcknowledgeChannel(Snowflake channel)
{
	Channel* pChan = GetChannelGlobally(channel);

	DbgPrintF("RequestAcknowledgeChannel: %lld", channel);

	if (!pChan) {
		DbgPrintF("DiscordInstance::RequestAcknowledgeChannel requested ack for invalid channel %lld?", channel);
		return;
	}

	Json j;
	j["token"] = nullptr;
	if (pChan->m_lastViewedNum >= 0)
		j["last_viewed"] = pChan->m_lastViewedNum;

	std::string url = GetDiscordAPI() + "channels/" + std::to_string(channel) + "/messages/" + std::to_string(pChan->m_lastSentMsg) + "/ack";

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
		GetDiscordAPI() + "read-states/ack-bulk",
		DiscordRequest::ACK_BULK,
		0,
		j.dump(),
		m_token
	);
}

void DiscordInstance::RequestDeleteMessage(Snowflake chan, Snowflake msg)
{
	std::string url = GetDiscordAPI() + "channels/" + std::to_string(chan) + "/messages/" + std::to_string(msg);

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
	Json j, data;

	if (guildId == 0)
	{
		// TODO - Subscriptions for DMs and groups.
		j["op"] = GatewayOp::SUBSCRIBE_DM;

		data["channel_id"] = channelId;
	}
	else
	{
		j["op"] = GatewayOp::SUBSCRIBE_GUILD;

		Json subs, guild, channels, rangeParent, rangeChildren[1];

		// Amount of users loaded
		int arr[2] = { 0, rangeMembers };
		rangeChildren[0] = arr;
		rangeParent = rangeChildren;

		if (channelId != 0)
			channels[std::to_string(channelId)] = rangeParent;

		data["guild_id"] = std::to_string(guildId);

		if (typing)
			data["typing"] = true;
		if (activities)
			data["activities"] = true;
		if (threads)
			data["threads"] = true;

		data["channels"] = channels;
	}

	j["d"] = data;

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
		GetDiscordAPI() + "users/@me/guilds/" + std::to_string(guild),
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
	if (message)
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
	}

	GetFrontend()->LaunchURL(url);
}

void DiscordInstance::ResetGatewayURL()
{
	m_gatewayUrl = "";
}

void DiscordInstance::ClearData()
{
	CloseGatewaySession();

	m_guilds.clear();
	m_dmGuild.m_channels.clear();
	m_messageRequestsInProgress.clear();
	m_gatewayUrl.clear();
	m_gatewayResumeUrl.clear();
	m_sessionId.clear();
	m_sessionType.clear();
	m_pendingUploads.clear();
	m_channelHistory.Clear();
	m_userGuildSettings.Clear();
	m_channelDenyList.clear();
	m_relationships.clear();

	m_mySnowflake = 0;
	m_CurrentGuild = 0;
	m_CurrentChannel = 0;
	m_gatewayConnId = -1;
	m_heartbeatSequenceId = -1;
	m_ackVersion = 0;
	m_nextAttachmentID = 1;
}

void DiscordInstance::ResolveLinks(FormattedText* message, std::vector<InteractableItem>& interactables, Snowflake guildID)
{
	auto& words = message->GetWords();
	for (size_t i = 0; i < words.size(); i++)
	{
		Word& word = words[i];
		bool isLink = word.m_flags & WORD_LINK;
		bool isMent = word.m_flags & WORD_MENTION;
		bool isTime = word.m_flags & WORD_TIMESTAMP;

		InteractableItem item;
		/**/ if (isLink) item.m_type = InteractableItem::LINK;
		else if (isMent) item.m_type = InteractableItem::MENTION;
		else if (isTime) item.m_type = InteractableItem::TIMESTAMP;

		if (item.m_type == InteractableItem::NONE)
			continue;

		item.m_wordIndex = i;
		item.m_text = word.GetContentOverride();
		item.m_destination = word.m_content;

		bool changed = false;
		if (isMent && !word.m_content.empty())
		{
			char mentType = word.m_content[0];

			if (mentType == '#')
			{
				std::string mentDest = word.m_content.substr(1);
				Snowflake sf = (Snowflake)GetIntFromString(mentDest);
				item.m_text = "#" + GetDiscordInstance()->LookupChannelNameGlobally(sf);
				item.m_affected = sf;
				changed = true;
			}
			else
			{
				bool isRole = false;
				bool hasExclam = false;
				if (word.m_content.size() > 2) {
					if (word.m_content[1] == '&')
						isRole = true;

					// not totally sure what this does. I only know that certain things use it
					if (word.m_content[1] == '!')
						hasExclam = true;
				}

				std::string mentDest = word.m_content.substr((isRole || hasExclam) ? 2 : 1);
				Snowflake sf = (Snowflake)GetIntFromString(mentDest);
				item.m_affected = sf;

				if (isRole)
					item.m_text = "@" + GetDiscordInstance()->LookupRoleName(sf, guildID);
				else
					item.m_text = "@" + GetDiscordInstance()->LookupUserNameGlobally(sf, guildID);
				changed = true;
			}
		}

		if (changed)
			word.SetContentOverride(item.m_text);

		interactables.push_back(item);
	}
}

void DiscordInstance::SetActivityStatus(eActiveStatus status, bool bRequestServer)
{
	DbgPrintF("Setting activity status to %d", status);
	GetProfile()->m_activeStatus = status;

	if (!bRequestServer)
		return;

	GetSettingsManager()->SetOnlineIndicator(GetProfile()->m_activeStatus);
	GetSettingsManager()->FlushSettings();
	GetFrontend()->RepaintProfile();
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
		GetDiscordAPI() + "users/@me/settings-proto/1",
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

bool DiscordInstance::ResortChannels(Snowflake guild)
{
	Guild* pGld = GetGuild(guild);
	if (!pGld)
		return false;

	// check if sorted
	if (std::is_sorted(pGld->m_channels.begin(), pGld->m_channels.end()))
		return false;
	
	pGld->m_channels.sort();
	return true;
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

	GetFrontend()->UpdateChannelAcknowledge(id, lastMessageId);
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
			c.m_name.clear();

			if (c.m_channelType == Channel::DM)
			{
				std::string avatar = "";
				if (chan["recipient_ids"].is_array())
				{
					assert(chan["recipient_ids"].size() > 0);
					if (chan["recipient_ids"].size() > 0)
					{
						c.m_recipients.clear();
						c.m_recipients.push_back(GetSnowflakeFromJsonObject(chan["recipient_ids"][0]));

						Profile* pf = GetProfileCache()->LookupProfile(c.GetDMRecipient(), "", "", "", false);
						avatar = pf->m_avatarlnk;
					}
				}
				else if (chan["recipients"].is_array())
				{
					assert(chan["recipients"].size() > 0);
					if (chan["recipients"].size() > 0)
					{
						Json& rec = chan["recipients"][0];
						c.m_recipients.clear();
						c.m_recipients.push_back(GetSnowflake(rec, "id"));
						avatar = GetFieldSafe(rec, "avatar");
					}
				}

				c.m_avatarLnk = avatar;
			}
			else
			{
				assert(c.m_channelType == Channel::GROUPDM);

				c.m_recipients.clear();

				if (chan["recipient_ids"].is_array()) {
					for (auto& rec : chan["recipient_ids"]) {
						c.m_recipients.push_back(GetSnowflakeFromJsonObject(rec));
					}
				}
				if (chan["recipients"].is_array()) {
					for (auto& rec : chan["recipients"]) {
						Snowflake theirID = GetSnowflake(rec, "id");
						c.m_recipients.push_back(theirID);
						GetProfileCache()->LoadProfile(theirID, rec);
					}
				}

				c.m_avatarLnk = GetFieldSafe(chan, "icon");

				if (!c.m_avatarLnk.empty())
					GetFrontend()->RegisterChannelIcon(c.m_snowflake, c.m_avatarLnk);
			}

			if (chan.contains("name"))
			{
				// cool, that shall be the name then
				c.m_name = GetFieldSafe(chan, "name");

				if (!c.m_name.empty())
					break;
			}

			// look through the recipients anyway
			std::string name = "";
			for (auto& rec : chan["recipients"])
			{
				if (!name.empty())
					name += ", ";

				name += GetGlobalName(rec);
			}

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
#ifdef DISABLE_GUILD_FOLDERS
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
#else
	GuildItemList gil = std::move(m_guildItemList);
	m_guildItemList.Clear();

	std::map<Snowflake, std::string> folderNames;
	std::vector<std::pair<Snowflake, Snowflake>> guilds; // pair (folderid, guildid)

	GetSettingsManager()->GetGuildFoldersEx(folderNames, guilds);

	std::map<Snowflake, bool> folderAdded;
	std::map<Snowflake, bool> guildAdded;

	// Add each guild in order.  If there is a folder, add it first if needed.
	for (auto& guild : guilds)
	{
		if (!folderAdded[guild.first] && guild.first) {
			folderAdded[guild.first] = true;
			m_guildItemList.AddFolder(guild.first, folderNames[guild.first]);
		}

		// Set the guild added flag to true even if we aren't adding it, because
		// I don't feel like adding a redundant check below
		guildAdded[guild.second] = true;

		// Fetch info about the actual guild.
		Guild* gld = GetGuild(guild.second);
		std::string name, avatar;
		if (!gld) {
			DbgPrintF("Guild %lld in guild folders doesn't actually exist", guild.second);
			continue;
		}

		name = gld->m_name;
		avatar = gld->m_avatarlnk;

		m_guildItemList.AddGuild(guild.first, guild.second, name, avatar);
	}

	// If there are any leftover guilds to add, add those as well.
	for (auto& guild : m_guilds)
	{
		if (!guildAdded[guild.m_snowflake]) {
			DbgPrintF("Guild %lld isn't in guild folders, adding to the end", guild.m_snowflake);
			m_guildItemList.AddGuild(0, guild.m_snowflake, guild.m_name, guild.m_avatarlnk);
		}
	}

	m_guildItemList.Dump();

	// TODO: Add a name to empty guild folders.

	return gil.CompareOrder(m_guildItemList) == false;
#endif
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

	// parse default notification settings
	g.m_defaultMessageNotifications = eMessageNotifications(GetFieldSafeInt(props, "default_message_notifications"));

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

	// parse emoji
	Json& emojis = elem["emojis"];
	for (auto& emojij : emojis)
	{
		Emoji emoji;
		emoji.Load(emojij);
		g.m_emoji[emoji.m_id] = emoji;
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
	DECL(READY_SUPPLEMENTAL);
	DECL(MESSAGE_CREATE);
	DECL(MESSAGE_UPDATE);
	DECL(MESSAGE_DELETE);
	DECL(MESSAGE_ACK);
	DECL(USER_SETTINGS_PROTO_UPDATE);
	DECL(USER_GUILD_SETTINGS_UPDATE);
	DECL(USER_NOTE_UPDATE);
	DECL(GUILD_CREATE);
	DECL(GUILD_DELETE);
	DECL(CHANNEL_CREATE);
	DECL(CHANNEL_DELETE);
	DECL(CHANNEL_UPDATE);
	DECL(GUILD_MEMBER_LIST_UPDATE);
	DECL(GUILD_MEMBERS_CHUNK);
	DECL(TYPING_START);
	DECL(PRESENCE_UPDATE);
	DECL(PASSIVE_UPDATE_V1);

	m_dmGuild.m_name = GetFrontend()->GetDirectMessagesText();
}

#undef DECL

static std::string GetStatusFromActivities(Json& activities)
{
	if (!activities.is_array() || activities.empty())
		return "";

	for (auto& activity : activities)
	{
		if (GetFieldSafe(activity, "name") == "Custom Status")
			// prioritize custom status
			return GetStatusStringFromGameJsonObject(activity);
	}

	return GetStatusStringFromGameJsonObject(activities[0]);
}

void DiscordInstance::HandleREADY_SUPPLEMENTAL(Json& j)
{
	Json& data = j["d"];

	std::vector<Snowflake> guildsVec;

	Json& guilds = data["guilds"];
	for (int i = 0; i < int(guilds.size()); i++)
		guildsVec.push_back(GetSnowflake(guilds[i], "id"));

	Json& mergedPresences = data["merged_presences"];

	Json& merPreFriends = mergedPresences["friends"];
	Json& merPreGuilds  = mergedPresences["guilds"];
	assert(merPreGuilds.size() == guildsVec.size());

	size_t ms = std::min(merPreGuilds.size(), guilds.size());
	for (int i = 0; i < int(ms); i++)
	{
		Json& guildPres = merPreGuilds[i];

		for (auto& memPres : guildPres) {
			Snowflake userID = GetSnowflake(memPres, "user_id");
			Profile* pf = GetProfileCache()->LookupProfile(userID, "", "", "", false);

			if (memPres.contains("status")) {
				std::string status = GetFieldSafe(memPres, "status");
				if (!status.empty())
					pf->m_activeStatus = GetStatusFromString(status);
			}

			// Look for any activities -- TODO: Server specific activities
			if (memPres.contains("game") && !memPres["game"].is_null())
				pf->m_status = GetStatusStringFromGameJsonObject(memPres["game"]);
			else if (memPres.contains("activities") && !memPres["activities"].is_null())
				pf->m_status = GetStatusFromActivities(memPres["activities"]);
			else
				pf->m_status = "";
		}
	}

	for (auto& friendPres : merPreFriends)
	{
		Snowflake userID = GetSnowflake(friendPres, "user_id");

		Profile* pf = GetProfileCache()->LookupProfile(userID, "", "", "", false);
		if (friendPres.contains("status")) {
			std::string status = GetFieldSafe(friendPres, "status");
			if (!status.empty())
				pf->m_activeStatus = GetStatusFromString(status);
		}

		// Look for any activities
		if (friendPres.contains("game") && !friendPres["game"].is_null())
			pf->m_status = GetStatusStringFromGameJsonObject(friendPres["game"]);
		else if (friendPres.contains("activities") && !friendPres["activities"].is_null())
			pf->m_status = GetStatusFromActivities(friendPres["activities"]);
		else
			pf->m_status = "";
	}
}

void DiscordInstance::HandleREADY(Json& j)
{
	GetFrontend()->OnConnected();

#ifdef _DEBUG
	std::string str = j.dump();
#endif

	Json& data = j["d"];
	m_gatewayResumeUrl = data["resume_gateway_url"];
	m_sessionId = data["session_id"];
	m_sessionType = data["session_type"];

	// ==== reload user
	Json& user = data["user"];

	Snowflake oldSnowflake = m_mySnowflake;
	m_mySnowflake = GetSnowflake(user, "id");

	bool firstReadyOnThisUser = m_mySnowflake != oldSnowflake;

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

			for (auto& memesub : meme)
			{
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

	// ==== load relationships - friends and blocked
	if (data.contains("relationships") && data["relationships"].is_array())
	{
		auto& rels = data["relationships"];
		for (auto& elem : rels)
		{
			Relationship rel;
			rel.Load(elem);
			m_relationships.push_back(rel);
		}
	}

	// ==== load resume_gateway_url
	// ==== load user_guild_settings
	if (data.contains("user_guild_settings") && data["user_guild_settings"].is_object())
	{
		m_userGuildSettings.Clear();
		m_userGuildSettings.Load(data["user_guild_settings"]);
	}
	
	// select the first guild, if possible
	Snowflake guildsf = 0;

	if (firstReadyOnThisUser) {
		m_CurrentChannel = 0;

		auto list = m_guildItemList.GetItems();
		if (list->size() > 0)
			guildsf = list->front()->GetID();
	}
	else {
		guildsf = m_CurrentGuild;
	}

	GetFrontend()->RepaintGuildList();
	OnSelectGuild(guildsf);

	// Doing this because the contents of all channels might be outdated.
	GetMessageCache()->ClearAllChannels();
	GetFrontend()->UpdateSelectedChannel();
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

	MessagePtr pOldMsg = GetMessageCache()->GetLoadedMessage(channelId, messageId);
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
		bool suppEveryone = false, suppRoles = false;
		auto pSettings = m_userGuildSettings.GetSettings(guildId);
		if (pSettings) {
			suppEveryone = pSettings->m_bSuppressEveryone;
			suppRoles    = pSettings->m_bSuppressRoles;
		}

		if (msg.CheckWasMentioned(m_mySnowflake, guildId, suppEveryone, suppRoles) && m_CurrentChannel != channelId)
			pChan->m_mentionCount++;
	}

	bool updateAck = false;

	if (m_CurrentChannel == channelId && pChan->m_lastViewedMsg == oldSentMsg)
		pChan->m_lastViewedMsg = pChan->m_lastSentMsg;
	else
		updateAck = true;

	if (bIsUpdate)
		GetFrontend()->OnUpdateMessage(channelId, msg);
	else
		GetFrontend()->OnAddMessage(channelId, msg);

	if (updateAck)
		GetFrontend()->UpdateChannelAcknowledge(channelId, pChan->m_lastViewedMsg);

	if (!bIsUpdate)
		m_notificationManager.OnMessageCreate(guildId, channelId, msg);
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

void DiscordInstance::HandleUSER_GUILD_SETTINGS_UPDATE(nlohmann::json& j)
{
	Json& data = j["d"];
	Snowflake guildID = GetSnowflake(data, "guild_id");

	GuildSettings* pSettings = m_userGuildSettings.GetOrCreateSettings(guildID);
	pSettings->Load(data);
}

void DiscordInstance::HandleUSER_NOTE_UPDATE(nlohmann::json& j)
{
	Json& data = j["d"];
	Snowflake uid = GetSnowflake(data, "id");
	std::string note = GetFieldSafe(data, "note");

	Profile* pf = GetProfileCache()->LookupProfile(uid, "", "", "", false);
	if (!pf) return;

	pf->m_note = note;
	pf->m_bNoteFetched = true;

	GetFrontend()->UpdateProfilePopout(uid);
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
	Snowflake guildID = GetSnowflake(data, "id");
	ParseAndAddGuild(data);

	Guild* guild = GetGuild(guildID);
	if (guild)
	{
		m_guildItemList.AddGuild(0, guildID, guild->m_name, guild->m_avatarlnk);
	}

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
			m_guildItemList.EraseGuild(sf);
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

	Guild* pGuild = GetGuild(guildId);
	if (!pGuild)
		return;

	int ord = 0;
	Channel chn;
	ParseChannel(chn, data, ord);

	chn.m_parentGuild = pGuild->m_snowflake;
	pGuild->m_channels.push_back(chn);
	pGuild->m_channels.sort();

	if (m_CurrentGuild == guildId)
		GetFrontend()->UpdateChannelList();
}

void DiscordInstance::HandleCHANNEL_UPDATE(Json& j)
{
	Json& data = j["d"];
	Snowflake guildId = GetSnowflake(data, "guild_id");//will return 0 if no guild
	Snowflake channelId = GetSnowflake(data, "id");

	Guild* pGuild = GetGuild(guildId);
	if (!pGuild)
		return;

	Channel* pChan = pGuild->GetChannel(channelId);
	if (!pChan)
		return;

	int position = pChan->m_pos;
	Snowflake oldCategory = pChan->m_parentCateg;
	uint64_t oldPerms = pChan->ComputePermissionOverwrites(m_mySnowflake, pGuild->ComputeBasePermissions(m_mySnowflake));

	int ord = 0;
	ParseChannel(*pChan, data, ord);

	// If the position, permissions, or parent category changed, refresh the channel.
	bool modifiedOrder = position != pChan->m_pos || oldCategory != pChan->m_parentCateg;

	if (modifiedOrder) {
		pGuild->m_channels.sort();
	}

	if (modifiedOrder || oldPerms != pChan->ComputePermissionOverwrites(m_mySnowflake, pGuild->ComputeBasePermissions(m_mySnowflake))) {
		GetFrontend()->UpdateChannelList();
	}
}

void DiscordInstance::HandleCHANNEL_DELETE(Json& j)
{
	Json& data = j["d"];
	Snowflake guildId = GetSnowflake(data, "guild_id");//will return 0 if no guild
	Snowflake channelId = GetSnowflake(data, "id");

	Guild* pGuild = GetGuild(guildId);
	if (!pGuild)
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

	Guild* pGuild = GetGuild(guild);
	if (pGuild)
		pGuild->AddKnownMember(pf->m_snowflake);

	// TODO: Not sure if this is the guild specific or global status. Probably guild specific
	if (!pres.is_null()) {
		if (pres.contains("status")) {
			auto status = GetFieldSafe(pres, "status");
			if (!status.empty())
				pf->m_activeStatus = GetStatusFromString(status);
		}
		if (pres.contains("game") && !pres["game"].is_null()) {
			pf->m_status = GetStatusStringFromGameJsonObject(pres["game"]);
		}
		else if (pres.contains("activities") && !pres["activities"].is_null()) {
			pf->m_status = GetStatusFromActivities(pres["activities"]);
		}
		else {
			pf->m_status = "";
		}
	}

	gm.m_roles.clear();
	for (auto& it : roles)
		gm.m_roles.push_back(GetSnowflakeFromJsonObject(it));

	return userID;
}

void DiscordInstance::HandlePRESENCE_UPDATE(nlohmann::json& j)
{
	Json& data = j["d"];

	Json& user = data["user"];
	Snowflake userID = GetSnowflake(user, "id");

	Profile* pf = GetProfileCache()->LookupProfile(userID, "", "", "", false);

	// Note: Updating these first because maybe LoadProfile triggers a refresh
	if (data.contains("status")) {
		std::string stat = GetFieldSafe(data, "status");
		if (!stat.empty())
			pf->m_activeStatus = GetStatusFromString(stat);
	}

	// Look for any activities -- TODO: Server specific activities
	if (data.contains("game") && !data["game"].is_null())
		pf->m_status = GetStatusStringFromGameJsonObject(data["game"]);
	else if (data.contains("activities") && !data["activities"].is_null())
		pf->m_status = GetStatusFromActivities(data["activities"]);
	else
		pf->m_status = "";

	if (user.contains("global_name")) // the full user object is provided
		GetProfileCache()->LoadProfile(userID, user);
	else
		GetFrontend()->UpdateUserData(userID);
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

	if (index < 0 || index >= int(pGld->m_members.size() + 1)) {
		//assert(!"huh");
		// TODO: Treat this case somehow
		return;
	}

	pGld->m_members.insert(pGld->m_members.begin() + index, sf);
}

void DiscordInstance::HandleGuildMemberListUpdate_Delete(Snowflake guild, nlohmann::json& j)
{
	Guild* pGld = GetGuild(guild);
	assert(pGld);

	int index = j["index"];

	if (index < 0 || index >= int(pGld->m_members.size())) {
		//assert(!"huh");
		// TODO: Treat this case somehow
		return;
	}
	
	Snowflake memberId = pGld->m_members[index];
	GuildMember& member = GetProfileCache()->LookupProfile(memberId, "", "", "", false)->m_guildMembers[guild];

	if (member.m_bIsGroup) {
		// also remove that group
		GetProfileCache()->ForgetProfile(memberId);
	}

	pGld->m_members.erase(pGld->m_members.begin() + index);
}

void DiscordInstance::HandleGuildMemberListUpdate_Update(Snowflake guild, nlohmann::json& j)
{
	Guild* pGld = GetGuild(guild);
	assert(pGld);

	int index = j["index"];

	if (index < 0 || index >= int(pGld->m_members.size())) {
		//assert(!"huh");
		// TODO: Treat this case somehow
		return;
	}

	Snowflake sf = ParseGuildMemberOrGroup(guild, j["item"]);
	pGld->m_members[index] = sf;

	std::set<Snowflake> updates{ sf };
	GetFrontend()->RefreshMembers(updates);
}

void DiscordInstance::OnUploadAttachmentFirst(NetRequest* pReq)
{
	auto& ups = m_pendingUploads;

	if (pReq->result != HTTP_OK)
	{
		// Delete enqueued upload
		auto iter = ups.find(pReq->key);
		std::string name = iter->second.m_uploadFileName;
		if (iter != ups.end())
			ups.erase(iter);

		GetFrontend()->OnFailedToUploadFile(name, pReq->result);
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
			NetRequest::PUT_OCTETS_PROGRESS,
			up.m_uploadUrl,
			DiscordRequest::UPLOAD_ATTACHMENT_2,
			pReq->key,
			"",
			"",//GetToken(),
			"",
			nullptr, // default processing
			pNewData,
			up.m_data.size()
		);

		GetFrontend()->OnStartProgress(pReq->key, up.m_name, true);
	}
}

void DiscordInstance::OnUploadAttachmentSecond(NetRequest* pReq)
{
	auto& ups = m_pendingUploads;
	auto iter = ups.find(pReq->key);
	if (iter == ups.end())
		return;

	if (pReq->result == HTTP_PROGRESS)
	{
		// N.B. totally safe to access because corresponding networker thread is locked up waiting for us
		pReq->m_bCancelOp = GetFrontend()->OnUpdateProgress(pReq->key, pReq->GetOffset(), pReq->GetTotalBytes());
		return;
	}

	if (pReq->result != HTTP_OK)
	{
		// Delete enqueued upload
		ups.erase(iter);
		GetFrontend()->OnFailedToUploadFile(pReq->additional_data, pReq->result);
		GetFrontend()->OnStopProgress(pReq->key);
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
		GetDiscordAPI() + "channels/" + std::to_string(up.m_channelSF) + "/messages",
		DiscordRequest::MESSAGE_CREATE,
		up.m_channelSF,
		j.dump(),
		GetToken(),
		std::to_string(up.m_tempSF)
	);

	GetFrontend()->OnStopProgress(pReq->key);

	// typing indicator goes away when a message is received, so allow sending one in like 100ms
	m_lastTypingSent = GetTimeMs() - TYPING_INTERVAL + 100;

	// Then erase it
	ups.erase(iter);
}

bool DiscordInstance::SendMessageAndAttachmentToCurrentChannel(
	const std::string& msg_,
	Snowflake& tempSf,
	uint8_t* attData,
	size_t attSize,
	const std::string& attName,
	bool isSpoiler)
{
	if (!GetCurrentChannel() || !GetCurrentGuild())
		return false;

	std::string msg = ResolveMentions(msg_, m_CurrentGuild, m_CurrentChannel);

	Channel* pChan = GetCurrentChannel();
	tempSf = CreateTemporarySnowflake();

	if (!pChan->HasPermission(PERM_SEND_MESSAGES) || !pChan->HasPermission(PERM_ATTACH_FILES))
		return false;
	
	std::string newAttName = (isSpoiler ? "SPOILER_" : "") + attName;

	Json file;
	file["filename"]  = newAttName;
	file["file_size"] = int(attSize);
	file["is_clip"]   = false;
	file["id"]        = std::to_string(m_nextAttachmentID);

	Json files;
	files.push_back(file);

	Json j;
	j["files"] = files;

	m_pendingUploads[m_nextAttachmentID] = PendingUpload(newAttName, attData, attSize, msg, tempSf, m_CurrentChannel);

	GetHTTPClient()->PerformRequest(
		true,
		NetRequest::POST_JSON,
		GetDiscordAPI() + "channels/" + std::to_string(m_CurrentChannel) + "/attachments",
		DiscordRequest::UPLOAD_ATTACHMENT,
		m_nextAttachmentID,
		j.dump(),
		m_token,
		newAttName,
		nullptr // default processing
	);

	m_nextAttachmentID++;
	return true;
}
