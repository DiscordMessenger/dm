#pragma once
#include <set>
#include <map>
#include "Main.hpp"
#include "../discord/FormattedText.hpp"

#define T_MESSAGE_LIST_PARENT_CLASS TEXT("MessageListParent")
#define T_MESSAGE_LIST_CLASS TEXT("MessageList")

// note: you need to regenerate the entire vector when updating attachments
class AttachmentItem
{
public:
	AttachmentItem() {}
	~AttachmentItem() {
		Clear();
	}

	AttachmentItem(AttachmentItem&& c) // move
	{
		m_pAttachment = c.m_pAttachment;
		m_boxRect = c.m_boxRect;
		m_textRect = c.m_textRect;
		m_nameTextSize = c.m_nameTextSize;
		m_sizeTextSize = c.m_sizeTextSize;
		m_nameText = c.m_nameText;
		m_sizeText = c.m_sizeText;
		m_bHighlighted = c.m_bHighlighted;

		c.m_nameText = NULL;
		c.m_sizeText = NULL;
	}

	AttachmentItem& operator=(const AttachmentItem& c) // copy
	{
		m_pAttachment = c.m_pAttachment;
		m_boxRect = c.m_boxRect;
		m_textRect = c.m_textRect;
		m_nameTextSize = c.m_nameTextSize;
		m_sizeTextSize = c.m_sizeTextSize;
		m_bHighlighted = c.m_bHighlighted;
		if (c.m_nameText) {
			m_nameText = new TCHAR[m_nameTextSize + 1];
			_tcscpy(m_nameText, c.m_nameText);
		}
		if (c.m_sizeText) {
			m_sizeText = new TCHAR[m_sizeTextSize + 1];
			_tcscpy(m_sizeText, c.m_sizeText);
		}
		return (*this);
	}

	AttachmentItem(const AttachmentItem& c) // copy
	{
		*this = c;
	}

	void Clear() {
		free(m_nameText);
		free(m_sizeText);
		m_nameText = m_sizeText = NULL;
	}
	
	void Update() {
		Clear();
		m_nameText = ConvertCppStringToTString(m_pAttachment->m_fileName, &m_nameTextSize);
		m_sizeText = ConvertCppStringToTString(GetSizeString(m_pAttachment->m_size), &m_sizeTextSize);

		m_resourceID = GetAvatarCache()->MakeIdentifier(
			std::to_string(m_pAttachment->m_id) +
			m_pAttachment->m_proxyUrl +
			m_pAttachment->m_actualUrl
		);
	}

public:
	Attachment* m_pAttachment = nullptr;
	LPTSTR m_nameText = nullptr;
	LPTSTR m_sizeText = nullptr;
	std::string m_resourceID = "";
	size_t m_nameTextSize = 0;
	size_t m_sizeTextSize = 0;
	RECT m_boxRect;
	RECT m_textRect;
	RECT m_addRect;
	bool m_bHighlighted = false;
};

enum {
	EMBED_IN_TITLE,
	EMBED_IN_AUTHOR,
	EMBED_IN_PROVIDER,
};

class InteractableItem
{
public:
	enum {
		NONE,
		LINK,
		MENTION,
		TIMESTAMP,
		EMBED_IMAGE,
		EMBED_LINK,
	};
	enum {
		EMBED_OFFSET = 1000000000,
	};
public:
	bool TypeUpdatedFromWords() const {
		return m_type != EMBED_IMAGE && m_type != EMBED_LINK;
	}
	bool ShouldInvalidateOnHover() const {
		return m_type != EMBED_IMAGE;
	}
	bool UseLinkColor() const {
		if (m_type == EMBED_LINK)
			return m_placeInEmbed == EMBED_IN_TITLE;
		return true;
	}
	int m_type = NONE;
	RECT m_rect{};
	std::string m_text;
	std::string m_destination;
	std::string m_proxyDest;
	bool m_bHighlighted = false;
	size_t m_wordIndex = 0;
	Snowflake m_affected = 0;
	int m_imageWidth = 0, m_imageHeight = 0;
	int m_placeInEmbed = 0;
};

struct RichEmbedFieldItem
{
	RichEmbedField* m_pField = nullptr;
	String m_title;
	String m_value;
	bool m_bInline = false;
	void Update();
};

class MessageList;

class RichEmbedItem
{
public:
	RichEmbed* m_pEmbed = nullptr;
	// Rects are laid out by Draw().
	RECT m_rect{};
	RECT m_imageRect{};
	RECT m_thumbnailRect{};
	RECT m_titleRect{};
	RECT m_authorRect{};
	RECT m_providerRect{};
	// Sizes are computed by Measure().
	SIZE m_size{};
	SIZE m_providerSize{};
	SIZE m_authorSize{};
	SIZE m_titleSize{};
	SIZE m_descriptionSize{};
	SIZE m_footerSize{};
	SIZE m_dateSize{};
	SIZE m_imageSize{};
	SIZE m_thumbnailSize{};

	time_t m_dateTime = 0;
	COLORREF m_color = 0;
	String m_providerName;
	String m_authorName;
	String m_title;
	String m_description;
	String m_footerText;
	String m_date;
	std::string m_imageResourceID;
	std::string m_thumbnailResourceID;
	std::vector<RichEmbedFieldItem> m_fields;

public:
	void Update();

	// N.B. For these, the Y height is not used.
	void Measure(HDC hdc, RECT& messageRect, bool isCompact);
	void Draw(HDC hdc, RECT& messageRect, MessageList* pList);
};

class MessagePollData
{
public:
	std::shared_ptr<MessagePoll> m_pMessagePoll;
	int m_width = 0;
	int m_height = 0;
	String m_question;
	std::map<int, String> m_answerTexts;
	std::map<int, String> m_emojiTexts;
	RECT m_questionRect;

public:
	MessagePollData() {}
	MessagePollData(std::shared_ptr<MessagePoll>& mp) {
		m_pMessagePoll = mp;
	}
	MessagePollData(const MessagePollData& oth) {
		if (oth.m_pMessagePoll != nullptr)
			m_pMessagePoll = oth.m_pMessagePoll;
	}
	MessagePollData(MessagePollData&& oth) noexcept {
		if (oth.m_pMessagePoll != nullptr)
			m_pMessagePoll = std::move(oth.m_pMessagePoll);
	}

public:
	void Update();
	void ShiftUp(int amount);

	void Measure(HDC hdc, RECT& messageRect, bool isCompact);
	void Draw(HDC hdc, RECT& messageRect, MessageList* pList);
};

class MessageItem
{
private:
	friend class MessageList;

	Message m_msg;
	RECT m_rect; // rectangle inside scrollHwnd
	RECT m_authorRect{};
	RECT m_messageRect{};
	RECT m_avatarRect{};
	RECT m_refAvatarRect{};
	RECT m_refMsgRect{};
	int m_height = 0;
	int m_textHeight = 0;
	int m_authHeight = 0;
	int m_authWidth = 0;
	int m_dateWidth = 0;
	int m_replyMessageWidth = 0;
	int m_replyAuthorWidth = 0;
	int m_replyHeight = 0;
	int m_attachHeight = 0;
	int m_embedHeight = 0;
	int m_pollHeight = 0;
	int m_placeInChain = 0; // (1)
	bool m_bIsDateGap = false;
	FormattedText m_message;
	// generated by ConvertCppStringToTString
	LPCTSTR m_author = NULL;
	LPCTSTR m_date = NULL;
	LPCTSTR m_dateEdited = NULL;
	LPCTSTR m_replyMsg = NULL;
	LPCTSTR m_replyAuth = NULL;

	bool m_authorHighlighted = false;
	std::vector<AttachmentItem> m_attachmentData;
	std::vector<InteractableItem> m_interactableData;
	std::vector<RichEmbedItem> m_embedData;
	MessagePollData* m_pMessagePollData = nullptr;
	bool m_bKeepWordsUpdate = false;
	bool m_bKeepHeightRecalc = false;
	bool m_bWasMentioned = false;
	bool m_bNeedUpdate = true;

	// Notes!
	//
	// (1) - Place in a chain of messages sent by the same person in short intervals
	//       of time. If zero, the whole message is drawn, otherwise only a simplified
	//       view is provided, with just the text.

public:
	MessageItem() {}
	MessageItem(const MessageItem& other) { // copy
		m_msg = other.m_msg;
		m_rect = other.m_rect;
		m_authorRect = other.m_authorRect;
		m_refAvatarRect = other.m_refAvatarRect;
		m_messageRect = other.m_messageRect;
		m_height = other.m_height;
		m_textHeight = other.m_textHeight;
		m_authHeight = other.m_authHeight;
		m_authWidth = other.m_authWidth;
		m_dateWidth = other.m_dateWidth;
		m_bKeepWordsUpdate = other.m_bKeepWordsUpdate;
		m_bKeepHeightRecalc = other.m_bKeepHeightRecalc;
		m_attachmentData = other.m_attachmentData;
		m_interactableData = other.m_interactableData;
		m_embedData = other.m_embedData;
		m_bWasMentioned = other.m_bWasMentioned;
		m_replyMessageWidth = other.m_replyMessageWidth;
		m_replyAuthorWidth = other.m_replyAuthorWidth;
		m_replyHeight = other.m_replyHeight;
		m_attachHeight = other.m_attachHeight;
		m_embedHeight = other.m_embedHeight;
		m_placeInChain = other.m_placeInChain;
		m_bIsDateGap = other.m_bIsDateGap;
		m_bNeedUpdate = true;

		if (other.m_pMessagePollData)
			m_pMessagePollData = new MessagePollData(*other.m_pMessagePollData);

		// Update pointers to the attachment data
		for (size_t i = 0; i < m_attachmentData.size(); i++) {
			ptrdiff_t offs = m_attachmentData[i].m_pAttachment - other.m_msg.m_attachments.data();
			m_attachmentData[i].m_pAttachment = (m_msg.m_attachments.data() + offs);
		}
		// Update pointers to the embed data
		for (size_t i = 0; i < m_embedData.size(); i++) {
			ptrdiff_t offs = m_embedData[i].m_pEmbed - other.m_msg.m_embeds.data();
			m_embedData[i].m_pEmbed = (m_msg.m_embeds.data() + offs);
		}
	}
	MessageItem(MessageItem&& other) noexcept { // move
		if (this == &other) {
			assert(!"why?");
			return;
		}

		m_msg = other.m_msg;
		m_rect = other.m_rect;
		m_authorRect = other.m_authorRect;
		m_refAvatarRect = other.m_refAvatarRect;
		m_messageRect = other.m_messageRect;
		m_height = other.m_height;
		m_textHeight = other.m_textHeight;
		m_authHeight = other.m_authHeight;
		m_authWidth = other.m_authWidth;
		m_dateWidth = other.m_dateWidth;
		m_message = other.m_message; other.m_message.Clear();
		m_author  = other.m_author ; other.m_author  = NULL;
		m_date    = other.m_date   ; other.m_date    = NULL;
		m_dateEdited = other.m_dateEdited; other.m_dateEdited = NULL;
		m_replyMsg = other.m_replyMsg; other.m_replyMsg = NULL;
		m_replyAuth = other.m_replyAuth; other.m_replyAuth = NULL;
		m_bKeepWordsUpdate = other.m_bKeepWordsUpdate;
		m_bKeepHeightRecalc = other.m_bKeepHeightRecalc;
		m_attachmentData = std::move(other.m_attachmentData);
		m_interactableData = std::move(other.m_interactableData);
		m_embedData = std::move(other.m_embedData);
		m_bWasMentioned = other.m_bWasMentioned;
		m_replyMessageWidth = other.m_replyMessageWidth;
		m_replyAuthorWidth = other.m_replyAuthorWidth;
		m_replyHeight = other.m_replyHeight;
		m_attachHeight = other.m_attachHeight;
		m_embedHeight = other.m_embedHeight;
		m_bNeedUpdate = other.m_bNeedUpdate;
		m_placeInChain = other.m_placeInChain;
		m_bIsDateGap = other.m_bIsDateGap;
		m_pMessagePollData = other.m_pMessagePollData; other.m_pMessagePollData = nullptr;

		// Update pointers to the attachment data
		for (size_t i = 0; i < m_attachmentData.size(); i++) {
			ptrdiff_t offs = m_attachmentData[i].m_pAttachment - other.m_msg.m_attachments.data();
			m_attachmentData[i].m_pAttachment = (m_msg.m_attachments.data() + offs);
		}
		// Update pointers to the embed data
		for (size_t i = 0; i < m_embedData.size(); i++) {
			ptrdiff_t offs = m_embedData[i].m_pEmbed - other.m_msg.m_embeds.data();
			m_embedData[i].m_pEmbed = (m_msg.m_embeds.data() + offs);
		}
	}
	void ClearAttachmentDataRects() {
		for (auto& att : m_attachmentData) {
			SetRectEmpty(&att.m_boxRect);
			SetRectEmpty(&att.m_textRect);
		}
	}
	void ClearInteractableDataRects() {
		for (auto& inte : m_interactableData) {
			SetRectEmpty(&inte.m_rect);
		}
	}
	void Clear() {
		m_message.Clear();
		free((void*)m_author);
		free((void*)m_date);
		free((void*)m_dateEdited);
		free((void*)m_replyMsg);
		free((void*)m_replyAuth);
		m_author = m_date = m_dateEdited = m_replyMsg = m_replyAuth = NULL;
		if (m_pMessagePollData) {
			delete m_pMessagePollData;
			m_pMessagePollData = nullptr;
		}
		m_bNeedUpdate = true;
	}
	~MessageItem() {
		Clear();
	}

	void Update(Snowflake guildID);
	void ShiftUp(int amount);
};

class MessageList
{
public:
	HWND m_hwnd = NULL;

private:
	UINT_PTR m_flash_timer = 0;
	int m_flash_counter = 0;

	Snowflake m_guildID   = 440442961147199490;
	Snowflake m_channelID = 1026090868303732766;

	std::list<MessageItem> m_messages;

	Snowflake m_rightClickedMessage = 0;
	Snowflake m_highlightedMessage = 0;
	Snowflake m_highlightedAttachment = 0;
	Snowflake m_highlightedAttachmentMessage = 0;
	size_t    m_highlightedInteractable = SIZE_MAX;
	Snowflake m_highlightedInteractableMessage = 0;

	Snowflake m_messageSentTo = 0;
	Snowflake m_emphasizedMessage = 0; // flashed message
	Snowflake m_firstShownMessage = 0;
	Snowflake m_previousLastReadMessage = 0;

	HBITMAP  m_backgroundImage = NULL;
	HBRUSH   m_backgroundBrush = NULL;
	COLORREF m_backgroundColor = 0;
	HBRUSH   m_defaultBackgroundBrush = NULL;
	bool     m_bInvertTextColors = false;
	bool     m_bBackgroundHasAlpha = false;
	bool     m_bDontDeleteBackgroundBrush = true;

	bool m_bAcknowledgeNow = true;

	int m_total_height = 0;
	int m_oldPos = 0;

	int m_oldWidth = -1;
	int m_oldHeight = -1;

	bool m_bManagedByOwner = false;
	bool m_bIsTopDown = false;

	void Scroll(int amount, RECT* rcClip = NULL, bool shiftAllRects = true);
	
	void MessageHeightChanged(int oldHeight, int newHeight, bool toStart = false);
	void AddMessageInternal(const Message& msg, bool toStart, bool updateLastViewedMessage = false, bool resetAnchor = true);
	void UpdateScrollBar(int addToHeight, int diffNow, bool toStart, bool update = true, int offsetY = 0, bool addingMessage = false);

	void FlashMessage(Snowflake msg);
	bool IsFlashingMessage() const;
	void EndFlashMessage();

	void ReloadBackground();
	void UnloadBackground();

	std::list<MessageItem>::iterator FindMessage(Snowflake sf);

	std::list<MessageItem>::iterator FindMessageByPoint(POINT pt);
	std::list<MessageItem>::iterator FindMessageByPointAuthorRect(POINT pt);
	std::list<MessageItem>::iterator FindMessageByPointReplyRect(POINT pt);

public:
	COLORREF GetDarkerBackgroundColor() const;

	COLORREF InvertIfNeeded(COLORREF color) const
	{
		return m_bInvertTextColors ? (color ^ 0xFFFFFF) : color;
	}

	void DrawReplyPieceIcon(HDC hdc, int leftX, int topY);

public:
	MessageList();
	~MessageList();


	void MeasureMessage(
		HDC hdc,
		FormattedText&,
		LPCTSTR strAuth,
		LPCTSTR strDate,
		LPCTSTR strDateEdit,
		LPCTSTR strReplyMsg,
		LPCTSTR strReplyAuth,
		bool isAuthorBot,
		bool isForward,
		const RECT& msgRect,
		int& height,
		int& authheight,
		int& authwidth,
		int& datewidth,
		int& replymsgwidth,
		int& replyauthwidth,
		int& replyheight,
		int placeinchain
	);

	void AddMessageStart(const Message& msg) {
		AddMessageInternal(msg, true, false);
	}
	void AddMessage(const Message& msg, bool updateLastViewedMessage = false) {
		AddMessageInternal(msg, false, updateLastViewedMessage);
	}
	void EditMessage(const Message& newMsg); // NOTE: the message HAS to have existed before!
	void DeleteMessage(Snowflake sf);
	void SetLastViewedMessage(Snowflake sf, bool refreshItAlso);

	void Repaint();
	void ClearMessages();
	void RefetchMessages(Snowflake gapCulprit = 0, bool causedByLoad = false);

	void SetGuild(Snowflake sf) {
		m_guildID = sf;
	}

	void SetChannel(Snowflake sf) {
		m_channelID = sf;
		m_bAcknowledgeNow = true;
	}

	void SetManagedByOwner(bool bNew) {
		m_bManagedByOwner = bNew;
	}

	void SetTopDown (bool bNew) {
		m_bIsTopDown = bNew;
	}

	Snowflake GetCurrentChannel() const {
		return m_channelID;
	}

	time_t GetLastSentMessageTime() const {
		if (m_messages.empty()) return 0;
		return m_messages.rbegin()->m_msg.m_dateTime;
	}

	Snowflake GetMessageSentTo() const {
		return m_messageSentTo;
	}

	void ProperlyResizeSubWindows();
	int RecalcMessageSizes(bool update, int& repaintSize, Snowflake addedMessagesBeforeThisID, Snowflake addedMessagesAfterThisID);
	void FullRecalcAndRepaint();
	void OnUpdateAttachment(Snowflake sf);
	void OnUpdateEmbed(const std::string& res);
	void OnUpdateAvatar(Snowflake sf);
	void OnUpdateEmoji(Snowflake sf);
	void OnFailedToSendMessage(Snowflake sf);
	void UpdateMembers(std::set<Snowflake>& mems);
	void UpdateBackgroundBrush();
	bool SendToMessage(Snowflake sf, bool addGapIfNeeded = true, bool forceInvalidate = false);
	void UpdateAllowDrop();
	bool ShouldBeDateGap(time_t oldTime, time_t newTime);

	// TODO: Wouldn't it be more sane to have a pointer to the last message item, or something?
	bool ShouldStartNewChain(Snowflake prevAuthor, time_t prevTime, int prevPlaceInChain, MessageType::eType prevType, const std::string& prevAuthorName, const std::string& prevAuthorAvatar, const MessageItem& item, bool ifChainTooLongToo);

public:
	static WNDCLASS g_MsgListClass;
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static void InitializeClass();
	static MessageList* Create (HWND hwnd, LPRECT pRect);
	static bool IsCompact();
	static bool MayErase();// checks if InvalidateRect or *Rgn should NEVER be passed true
	
protected:
	friend class MessageItem;
	static bool IsActionMessage(MessageType::eType msgType);
	static bool IsReplyableActionMessage(MessageType::eType msgType); // is this an action message that one can reply to
	static bool IsClientSideMessage(MessageType::eType msgType);

private:
	void HitTestReply(POINT pt, BOOL& hit);
	void HitTestAuthor(POINT pt, BOOL& hit);
	void HitTestAttachments(POINT pt, BOOL& hit);
	void HitTestInteractables(POINT pt, BOOL& hit);

	void AdjustHeightInfo(const MessageItem& msg, int& height, int& textheight, int& authheight, int& replyheight, int& attachheight, int& embedheight, int& pollheight);
	void OpenAttachment( AttachmentItem* pItem );
	void OpenInteractable( InteractableItem* pItem, MessageItem* pMsg );

	void DrawDefaultAttachment(HDC hdc, RECT& paintRect, AttachmentItem& attachItem, RECT& attachRect);
	void DrawImageAttachment(HDC hdc, RECT& paintRect, AttachmentItem& attachItem, RECT& attachRect);

	void DetermineMessageMeasurements(MessageItem& mi, HDC hdc = NULL, const LPRECT rect = NULL);

	int DrawMessageReply(HDC hdc, MessageItem& item, RECT& rc);
	void DrawMessage(HDC hdc, MessageItem& item, RECT& msgRect, RECT& clientRect, RECT& paintRect, DrawingContext& mddc, COLORREF chosenBkColor, bool drawNewMarker);

	void PaintBackground(HDC hdc, RECT& paintRect, RECT& clientRect);
	void Paint(HDC hdc, RECT& rcPaint);

	void RequestMarkRead();
	void HandleRightClickMenuCommand(int command);

	// [Left] [Author] [pinned] [a message](uid) [ to this channel.

	// DetermineMessageData determines data for an action message.  It is designed to allow for
	// rendering of an action message with the following format:
	// [messagePart1] [AUTHOR_NAME] [messagePart2] [clickableString:clickableLink] [messagePart3].
	// For example, [] [AUTHOR_NAME] [ pinned ] [a message:https://discord.com/channels/...] [ to this channel.]

	// This is a horrible mess!
	static void DetermineMessageData(
		Snowflake guildID,                /* IN */
		Snowflake channelID,              /* IN */
		MessageType::eType msgType,	      /* IN */
		Snowflake id,				      /* IN */
		Snowflake authorId,		          /* IN */
		Snowflake refMsgGuildID,          /* IN */
		Snowflake refMsgChannelID,        /* IN */
		Snowflake refMsgMessageID,        /* IN */
		const std::set<Snowflake>& ments, /* IN */
		const std::string& content,	      /* IN */
		LPCTSTR& messagePart1,		      /* OUT */
		LPCTSTR& messagePart2,		      /* OUT */
		LPCTSTR& messagePart3,		      /* OUT */
		LPCTSTR& clickableString,	      /* OUT */
		std::string& clickableLink,	      /* OUT */
		Snowflake& clickableSF,           /* OUT */
		LPTSTR& freedStringSpace,	      /* OUT */
		int& icon					      /* OUT */
	);

	static void ConfirmOpenLink(const std::string& link);

	static COLORREF DrawMentionBackground(HDC hdc, RECT& rc, COLORREF chosenBkColor);

	// called by OnUpdateEmoji
	static void InvalidateEmote(void* context, const Rect& rc);
};
