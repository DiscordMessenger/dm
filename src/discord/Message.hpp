#pragma once
#include <string>
#include <vector>
#include <set>
#include <ctime>
#include <memory>
#include <iprogsjson.hpp>
#include "Snowflake.hpp"
#include "Attachment.hpp"
#include "MessageType.hpp"
#include "MessagePoll.hpp"

// XXX: Ok, I'm going to be cheap here and implement a separate class for the referenced message stuff.
struct ReferenceMessage
{
	MessageType::eType m_type = MessageType::DEFAULT;
	Snowflake m_snowflake = 0;
	Snowflake m_author_snowflake = 0;
	Snowflake m_webhook_id = 0;
	std::string m_message;
	std::string m_author;
	std::string m_avatar;
	time_t m_timestamp;
	bool m_bHasAttachments = false;
	bool m_bHasComponents = false;
	bool m_bHasEmbeds = false;
	bool m_bMentionsAuthor = false;
	bool m_bIsAuthorBot = false;
	std::set<Snowflake> m_userMentions;

	void Load(iprog::JsonObject& msgData, Snowflake guild);
};

struct RichEmbedField
{
	std::string m_title;
	std::string m_value;
	bool m_bInline = false;
};

struct RichEmbed
{
	enum {
		UNKNOWN,
		RICH,
		IMAGE,
		VIDEO,
		GIFV,
		ARTICLE,
		LINK,
	};
	int m_type = UNKNOWN;
	int m_color = 0;
	std::string m_typeStr;
	// body
	std::string m_title;
	std::string m_url;
	std::string m_description;
	// author
	std::string m_providerName;
	std::string m_providerUrl;
	// author
	std::string m_authorName;
	std::string m_authorUrl;
	std::string m_authorIconUrl;
	std::string m_authorIconProxiedUrl;
	// footer text
	std::string m_footerText;
	std::string m_footerIconUrl;
	std::string m_footerIconProxiedUrl;
	// shared between images and videos
	bool m_bHasImage = false;
	std::string m_imageUrl;
	std::string m_imageProxiedUrl;
	int m_imageWidth = 0;
	int m_imageHeight = 0;
	// thumbnail image
	bool m_bHasThumbnail = false;
	std::string m_thumbnailUrl;
	std::string m_thumbnailProxiedUrl;
	int m_thumbnailWidth = 0;
	int m_thumbnailHeight = 0;
	// time as shown in the footer
	time_t m_timestamp = 0;
	// list of fields
	std::vector<RichEmbedField> m_fields;

	void Load(iprog::JsonObject& j);
};

class Message
{
public:
	MessageType::eType m_type = MessageType::DEFAULT;
	Snowflake m_snowflake = 0;
	Snowflake m_author_snowflake = 0;
	std::string m_message = "";
	std::string m_author = "";
	std::string m_avatar = "";
	Snowflake m_anchor = 0; // for gap messages
	Snowflake m_nonce = 0; // to create messages
	std::vector<Attachment> m_attachments;
	std::string m_dateFull = "";
	std::string m_dateCompact = "";
	std::string m_dateOnly = "";
	std::string m_editedText = "";
	std::string m_editedTextCompact = "";
	time_t m_dateTime = 0;
	time_t m_timeEdited = 0;
	std::set<Snowflake> m_userMentions;
	std::set<Snowflake> m_roleMentions;
	bool m_bMentionedEveryone = false;
	bool m_bIsAuthorBot = false;
	bool m_bRead = false; // valid only for the notification viewer messages
	bool m_bIsForward = false;
	Snowflake m_refMessageGuild = 0;
	Snowflake m_refMessageChannel = 0;
	Snowflake m_refMessageSnowflake = 0;
	std::vector<RichEmbed> m_embeds;
	Snowflake m_webhookId = 0;
	std::shared_ptr<MessagePoll> m_pMessagePoll;
	std::shared_ptr<ReferenceMessage> m_pReferencedMessage;

public:
	Message() {}
	Message(Snowflake snowflake, Snowflake authSF, const std::string& msg, const std::string& auth, const std::string& date) :
		m_snowflake(snowflake), m_author_snowflake(authSF), m_message(msg), m_author(auth)
	{
		SetDate(date);
	}

	bool IsLoadGap() const {
		return m_type == MessageType::GAP_UP || m_type == MessageType::GAP_DOWN || m_type == MessageType::GAP_AROUND;
	}

	bool IsWebHook() const {
		return m_webhookId != 0;
	}
	
	bool IsReply() const {
		return m_pReferencedMessage && !m_bIsForward;
	}

public:
	void SetDate(const std::string& dateStr);
	void SetTime(time_t t);
	void SetDateEdited(const std::string& dateStr);
	void SetTimeEdited(time_t t);

	bool CheckWasMentioned(Snowflake user, Snowflake guild, bool bSuppressEveryone = false, bool bSuppressRoles = false) const;

	void Load(iprog::JsonObject& j, Snowflake guild);
};

typedef std::shared_ptr<Message> MessagePtr;

static MessagePtr MakeMessage() {
	return std::make_shared<Message>();
}

static MessagePtr MakeMessage(const Message& msg) {
	return std::make_shared<Message>(msg);
}
