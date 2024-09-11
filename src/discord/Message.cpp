#include "Message.hpp"
#include "DiscordInstance.hpp"
#include "Util.hpp"

using Json = nlohmann::json;

void Message::SetDate(const std::string & dateStr)
{
	if (dateStr.empty()) {
		m_dateTime = 0;
		m_dateFull = m_dateCompact = "";
		return;
	}

	SetTime(ParseTime(dateStr));
}

void Message::SetTime(time_t t)
{
	m_dateTime = t;
	m_dateFull = FormatTimeLong(m_dateTime, true);
	m_dateCompact = FormatTimeShorter(m_dateTime);
	m_dateOnly = FormatDate(m_dateTime);
}

void Message::SetDateEdited(const std::string& dateStr)
{
	if (dateStr.empty()) {
		m_timeEdited = 0;
		m_editedText = "";
		return;
	}

	SetTimeEdited(ParseTime(dateStr));
}

void Message::SetTimeEdited(time_t t)
{
	m_timeEdited = t;
	m_editedText = "(edited " + FormatTimeLong(m_timeEdited, true) + ")";
	m_editedTextCompact = "(edited)";
}

bool Message::CheckWasMentioned(Snowflake user, Snowflake guild, bool bSuppressEveryone, bool bSuppressRoles) const
{
	if (!bSuppressEveryone && m_bMentionedEveryone)
		return true;

	if (m_userMentions.find(user) != m_userMentions.end())
		return true;

	if (!guild)
		return false;

	Profile* pf = GetProfileCache()->LookupProfile(user, "", "", "", false);
	if (!pf->HasGuildMemberProfile(guild))
		return false;

	if (!bSuppressRoles)
	{
		GuildMember* gm = &pf->m_guildMembers[guild];
		for (auto role : gm->m_roles) {
			if (m_roleMentions.find(role) != m_roleMentions.end())
				return true;
		}
	}

	return false;
}

void ReferenceMessage::Load(nlohmann::json& data, Snowflake guild)
{
	// Cheap clone
	Json& author = data["author"];
	Snowflake messageId = GetSnowflake(data, "id");
	Snowflake authorId = 0;
	std::string authorName = "";
	std::string userName = "", avatar = "";
	bool isBot = false;

	m_webhook_id = GetSnowflake(data, "webhook_id");

	if (author.is_object())
	{
		authorId = GetSnowflake(author, "id");
		authorName = GetGlobalName(author);
		userName = GetUsername(author);
		avatar = GetFieldSafe(author, "avatar");
		isBot = GetFieldSafeBool(author, "bot", false);

		m_author = authorName;
		m_avatar = avatar;
		m_bIsAuthorBot = isBot;

		m_author_snowflake = authorId;
		if (authorId && !m_webhook_id) {
			Profile* pf = GetProfileCache()->LookupProfile(authorId, userName, authorName, avatar, false);
			m_author = pf->GetName(guild);
		}
	}
	
	m_snowflake = messageId;
	m_type = (MessageType::eType)GetFieldSafeInt(data, "type");
	m_message = GetFieldSafe(data, "content");
	m_bHasAttachments = data["attachments"].is_array() && data["attachments"].size() > 0;
	m_bHasComponents = data["embeds"].is_array() && data["embeds"].size() > 0;
	m_bHasEmbeds = data["embeds"].is_array() && data["embeds"].size() > 0;
	m_bMentionsAuthor = false;

	if (data["mentions"].is_array())
	{
		for (auto& ment : data["mentions"])
		{
			Snowflake id = GetSnowflake(ment, "id");
			std::string avatar = GetFieldSafe(ment, "avatar");
			std::string globName = GetGlobalName(ment);
			std::string userName = GetUsername(ment);
			Profile* pf = GetProfileCache()->LoadProfile(id, ment);
			m_userMentions.insert(id);
		}
	}
}

void Attachment::Load(Json& j)
{
	m_id = GetSnowflake(j, "id");
	m_width  = GetFieldSafeInt(j, "width");
	m_height = GetFieldSafeInt(j, "height");
	m_size   = GetFieldSafeInt(j, "size");
	m_flags  = GetFieldSafeInt(j, "flags");
	m_proxyUrl    = GetFieldSafe(j, "proxy_url");
	m_actualUrl   = GetFieldSafe(j, "url");
	m_fileName    = GetFieldSafe(j, "filename");
	m_contentType = ContentType::GetFromString(GetFieldSafe(j, "content_type"));

	// If width and height are set, but no content type was assigned, try to automatically detect the type of content.
	if (m_contentType == ContentType::BLOB && m_width != 0 && m_height != 0)
	{
		if (EndsWithCaseInsens(m_fileName, ".png"))  m_contentType = ContentType::PNG;
		if (EndsWithCaseInsens(m_fileName, ".gif"))  m_contentType = ContentType::GIF;
		if (EndsWithCaseInsens(m_fileName, ".jpg"))  m_contentType = ContentType::JPEG;
		if (EndsWithCaseInsens(m_fileName, ".jpeg")) m_contentType = ContentType::JPEG;
		if (EndsWithCaseInsens(m_fileName, ".webp")) m_contentType = ContentType::WEBP;
	}

	UpdatePreviewSize();
}

void RichEmbed::Load(Json& j)
{
	auto& footer = j["footer"];
	auto& image = j["image"];
	auto& thumbnail = j["thumbnail"];
	auto& author = j["author"];
	auto& fields = j["fields"];
	auto& provider = j["provider"];
	//auto& video = j["video"];

	m_typeStr = GetFieldSafe(j, "type");

	// hate this, but what can you do
	/**/ if (m_typeStr == "rich")    m_type = RICH;
	else if (m_typeStr == "image")   m_type = IMAGE;
	else if (m_typeStr == "video")   m_type = VIDEO;
	else if (m_typeStr == "gifv")    m_type = GIFV;
	else if (m_typeStr == "article") m_type = ARTICLE;
	else if (m_typeStr == "link")    m_type = LINK;

	m_title = GetFieldSafe(j, "title");
	m_description = GetFieldSafe(j, "description");
	m_url = GetFieldSafe(j, "url");
	m_timestamp = ParseTime(GetFieldSafe(j, "timestamp"));
	m_color = GetFieldSafeInt(j, "color");
	
	if (footer.is_object()) {
		m_footerText = GetFieldSafe(footer, "text");
		m_footerIconUrl = GetFieldSafe(footer, "icon_url");
		m_footerIconProxiedUrl = GetFieldSafe(footer, "proxy_icon_url");
	}
	if (image.is_object()) {
		m_bHasImage = true;
		m_imageUrl = GetFieldSafe(image, "url");
		m_imageProxiedUrl = GetFieldSafe(image, "proxy_url");
		m_imageWidth = GetFieldSafeInt(image, "width");
		m_imageHeight = GetFieldSafeInt(image, "height");
	}
	if (thumbnail.is_object()) {
		m_bHasThumbnail = true;
		m_thumbnailUrl = GetFieldSafe(thumbnail, "url");
		m_thumbnailProxiedUrl = GetFieldSafe(thumbnail, "proxy_url");
		m_thumbnailWidth = GetFieldSafeInt(thumbnail, "width");
		m_thumbnailHeight = GetFieldSafeInt(thumbnail, "height");
	}
	if (author.is_object()) {
		m_authorName = GetFieldSafe(author, "name");
		m_authorUrl = GetFieldSafe(author, "url");
		m_authorIconUrl = GetFieldSafe(author, "icon_url");
		m_authorIconProxiedUrl = GetFieldSafe(author, "proxy_icon_url");
	}
	if (provider.is_object()) {
		m_providerName = GetFieldSafe(provider, "name");
		m_providerUrl = GetFieldSafe(provider, "url");
	}
	// video
}

void Message::Load(Json& data, Snowflake guild)
{
	Json& author = data["author"];

	m_pMessagePoll.reset();

	Snowflake messageId = GetSnowflake(data, "id");

	Snowflake authorId = m_author_snowflake;
	std::string authorName = m_author;
	std::string userName = "", avatar = "";
	bool isBot = false;

	if (author.is_object())
	{
		authorId = GetSnowflake(author, "id");
		authorName = GetGlobalName(author);
		userName = GetUsername(author);
		avatar = GetFieldSafe(author, "avatar");
		isBot = GetFieldSafeBool(author, "bot", false);
	}

	if (data.contains("content"))
		m_message = GetFieldSafe(data, "content");

	if (data.contains("timestamp"))
		SetDate(GetFieldSafe(data, "timestamp"));

	if (data.contains("edited_timestamp"))
		SetDateEdited(GetFieldSafe(data, "edited_timestamp"));

	if (data.contains("type"))
		m_type = (MessageType::eType)data["type"];

	if (data.contains("webhook_id"))
		m_webhookId = GetSnowflake(data, "webhook_id");

	auto& member = data["member"];
	if (member.is_object() && guild)
		authorId = GetDiscordInstance()->ParseGuildMember(guild, member, authorId);

	if (data.contains("nonce"))
		m_anchor = GetIntFromString(data["nonce"]);

	m_snowflake = messageId;
	m_author_snowflake = authorId;

	m_author = authorName;
	m_avatar = avatar;
	m_bIsAuthorBot = isBot;

	if (authorId && !IsWebHook()) {
		Profile* pf = GetProfileCache()->LookupProfile(authorId, userName, authorName, avatar, false);
		m_author = pf->GetName(guild);
		m_avatar = pf->m_avatarlnk;
	}

	if (data["attachments"].is_array())
	{
		m_attachments.clear();
		for (auto& attd : data["attachments"])
		{
			Attachment att;
			att.Load(attd);
			m_attachments.push_back(att);
		}
	}

	if (data["embeds"].is_array())
	{
		m_embeds.clear();
		for (auto& attd : data["embeds"])
		{
			RichEmbed emb;
			emb.Load(attd);
			m_embeds.push_back(emb);
		}
	}

	if (data.contains("poll"))
		m_pMessagePoll = std::make_shared<MessagePoll>(data["poll"]);

	Json& msgRef = data["message_reference"];
	Json& refdMsg = data["referenced_message"];

	if (!msgRef.is_null())
	{
		m_pReferencedMessage = std::make_shared<ReferenceMessage>();
		ReferenceMessage& rMsg = *m_pReferencedMessage;

		if (msgRef["guild_id"].is_null())
			m_refMessageGuild = guild;
		else
			m_refMessageGuild = GetSnowflake(msgRef, "guild_id");

		assert(!msgRef["channel_id"].is_null());

		m_refMessageChannel   = GetSnowflake(msgRef, "channel_id");
		m_refMessageSnowflake = GetSnowflake(msgRef, "message_id");

		if (!refdMsg.is_null()) {
			rMsg.Load(refdMsg, guild);
		}
		else {
			rMsg.m_author = "Original message was deleted.";
			rMsg.m_author_snowflake = 0;
		}
	}

	if (data["mentions"].is_array())
	{
		for (auto& ment : data["mentions"])
		{
			Snowflake id = GetSnowflake(ment, "id");
			std::string avatar = GetFieldSafe(ment, "avatar");
			std::string globName = GetGlobalName(ment);
			std::string userName = GetUsername(ment);

			Profile* pf = GetProfileCache()->LoadProfile(id, ment);

			m_userMentions.insert(id);

			if (m_pReferencedMessage != nullptr &&
				m_pReferencedMessage->m_author_snowflake == id)
				m_pReferencedMessage->m_bMentionsAuthor = true;
		}
	}

	if (data["mention_roles"].is_array())
	{
		for (auto& ment : data["mention_roles"])
			m_roleMentions.insert(GetSnowflakeFromJsonObject(ment));
	}

	if (data["mention_everyone"].is_boolean())
		m_bMentionedEveryone = data["mention_everyone"];
}
