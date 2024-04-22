#include "MessageList.hpp"
#include "TextToSpeech.hpp"
#include "ProfilePopout.hpp"
#include "ImageViewer.hpp"
#include "UploadDialog.hpp"
#include "../discord/LocalSettings.hpp"

#define STRAVAILABLE(str) ((str) && (str)[0] != 0)

// N.B. WINVER<=0x0500 doesn't define it. We'll force it
#ifndef IDC_HAND
#define IDC_HAND            MAKEINTRESOURCE(32649)
#endif//IDC_HAND

#define ATTACHMENT_HEIGHT (ScaleByDPI(40))
#define ATTACHMENT_GAP    (ScaleByDPI(4))
#define DATE_GAP_HEIGHT   (ScaleByDPI(20))
#define PROFILE_PICTURE_GAP (PROFILE_PICTURE_SIZE + 8);

static int GetProfileBorderRenderSize()
{
	return ScaleByDPI(Supports32BitIcons() ? (PROFILE_PICTURE_SIZE_DEF + 12) : 64);
}

WNDCLASS MessageList::g_MsgListClass, MessageList::g_MsgListParentClass;

static HICON g_ReplyPieceIcon;

static const int g_WelcomeTextIds[] =
{
	0, IDS_WELCOME_001_R,
	0, IDS_WELCOME_002_R,
	IDS_WELCOME_003_L, IDS_WELCOME_003_R,
	IDS_WELCOME_004_L, IDS_WELCOME_004_R,
	0, IDS_WELCOME_005_R,
	0, IDS_WELCOME_006_R,
	0, IDS_WELCOME_007_R,
	IDS_WELCOME_008_L, IDS_WELCOME_008_R,
	0, IDS_WELCOME_009_R,
	IDS_WELCOME_010_L, IDS_WELCOME_EXCLAM,
	IDS_WELCOME_011_L, IDS_WELCOME_PERIOD,
	IDS_WELCOME_012_L, IDS_WELCOME_PERIOD,
	IDS_WELCOME_013_L, IDS_WELCOME_EXCLAM,
};

static const int g_WelcomeTextCount = 13;

MessageList::MessageList()
{
}
MessageList::~MessageList()
{
}

bool MessageList::IsCompact()
{
	return GetSettingsManager()->GetMessageCompact();
}

void RichEmbedFieldItem::Update()
{
	m_title.Set(m_pField->m_title);
	m_value.Set(m_pField->m_value);
	m_bInline = m_pField->m_bInline;
}

void RichEmbedItem::Update()
{
	m_color = RGB(
		(m_pEmbed->m_color >> 16) & 0xFF,
		(m_pEmbed->m_color >> 8) & 0xFF,
		(m_pEmbed->m_color >> 0) & 0xFF
	);
	m_dateTime = m_pEmbed->m_timestamp;
	m_title.Set(m_pEmbed->m_title);
	m_description.Set(m_pEmbed->m_description);
	m_authorName.Set(m_pEmbed->m_authorName);
	m_footerText.Set(m_pEmbed->m_footerText);
	m_providerName.Set(m_pEmbed->m_providerName);
	if (m_dateTime >= 0)
		m_date.Set(FormatTimeLong(m_dateTime));

	const size_t sz = m_pEmbed->m_fields.size();
	m_fields.resize(sz);
	for (size_t i = 0; i < sz; i++)
	{
		m_fields[i].m_pField = &m_pEmbed->m_fields[i];
		m_fields[i].Update();
	}
}

void RichEmbedItem::Measure(HDC hdc, RECT& messageRect, bool isCompact)
{
	const int borderSize = ScaleByDPI(10);
	SIZE sz { 0, 0 };

	RECT rcItem = messageRect;
	rcItem.left   += borderSize;
	rcItem.top    += borderSize;
	rcItem.right  -= borderSize * 2;
	rcItem.bottom -= borderSize * 2;
	rcItem.right -= ScaleByDPI(20);
	rcItem.bottom -= ScaleByDPI(20);
	if (!isCompact)
		rcItem.right -= ScaleByDPI(PROFILE_PICTURE_SIZE_DEF + 12);

	const int maxBoxWidth = ScaleByDPI(500);
	int width = rcItem.right - rcItem.left;
	if (width > maxBoxWidth) {
		width = maxBoxWidth;
		rcItem.right = rcItem.left + width;
	}

	HGDIOBJ oldObj = SelectObject(hdc, GetStockFont(ANSI_VAR_FONT));

	// TODO: avoid repeating myself
	RECT rcMeasure;

	// Calculate the provider.
	if (!m_providerName.Empty()) {
		SelectObject(hdc, g_ReplyTextFont);
		RECT rcMeasure = rcItem;
		DrawText(hdc, m_providerName.GetWrapped(), -1, &rcMeasure, DT_CALCRECT | DT_SINGLELINE | DT_WORD_ELLIPSIS);
		m_providerSize.cx = rcMeasure.right - rcMeasure.left;
		m_providerSize.cy = rcMeasure.bottom - rcMeasure.top;
	}
	else m_providerSize = sz;

	// Calculate the author.
	if (!m_authorName.Empty()) {
		SelectObject(hdc, g_DateTextFont);
		rcMeasure = rcItem;
		DrawText(hdc, m_authorName.GetWrapped(), -1, &rcMeasure, DT_CALCRECT | DT_SINGLELINE | DT_WORD_ELLIPSIS);
		m_authorSize.cx = rcMeasure.right - rcMeasure.left;
		m_authorSize.cy = rcMeasure.bottom - rcMeasure.top;
	}
	else m_authorSize = sz;

	// Calculate the title.
	if (!m_title.Empty()) {
		SelectObject(hdc, g_AuthorTextFont);
		rcMeasure = rcItem;
		DrawText(hdc, m_title.GetWrapped(), -1, &rcMeasure, DT_CALCRECT | DT_SINGLELINE | DT_WORD_ELLIPSIS);
		m_titleSize.cx = rcMeasure.right - rcMeasure.left;
		m_titleSize.cy = rcMeasure.bottom - rcMeasure.top;
	}
	else m_titleSize = sz;

	// Calculate the description.
	if (!m_description.Empty()) {
		SelectObject(hdc, g_MessageTextFont);
		rcMeasure = rcItem;
		DrawText(hdc, m_description.GetWrapped(), -1, &rcMeasure, DT_CALCRECT | DT_WORDBREAK | DT_WORD_ELLIPSIS);
		m_descriptionSize.cx = rcMeasure.right - rcMeasure.left;
		m_descriptionSize.cy = rcMeasure.bottom - rcMeasure.top;
	}
	else m_descriptionSize = sz;

	// Calculate the footer.
	if (!m_footerText.Empty()) {
		SelectObject(hdc, g_ReplyTextFont);
		rcMeasure = rcItem;
		DrawText(hdc, m_footerText.GetWrapped(), -1, &rcMeasure, DT_CALCRECT | DT_SINGLELINE | DT_WORD_ELLIPSIS);
		m_footerSize.cx = rcMeasure.right - rcMeasure.left;
		m_footerSize.cy = rcMeasure.bottom - rcMeasure.top;
	}
	else m_footerSize = sz;

	// Calculate the date.
	if (!m_date.Empty()) {
		SelectObject(hdc, g_ReplyTextFont);
		rcMeasure = rcItem;
		rcMeasure.left += m_footerSize.cx + ScaleByDPI(15);
		DrawText(hdc, m_date.GetWrapped(), -1, &rcMeasure, DT_CALCRECT | DT_SINGLELINE | DT_WORD_ELLIPSIS);
		m_dateSize.cx = rcMeasure.right - rcMeasure.left;
		m_dateSize.cy = rcMeasure.bottom - rcMeasure.top;
	}
	else m_dateSize = sz;

	int gap = ScaleByDPI(5);
	LONG footerPlusDateXSize = m_footerSize.cx;
	if (m_dateSize.cx) {
		if (footerPlusDateXSize)
			footerPlusDateXSize += ScaleByDPI(15);
		footerPlusDateXSize += m_dateSize.cx;
	}

	sz.cx = std::max({ m_providerSize.cx, m_authorSize.cx, m_titleSize.cx, m_descriptionSize.cx, footerPlusDateXSize });
	
	const int maxHeight = ScaleByDPI(200);
	int maxImgWidth = std::max(int(rcItem.right - rcItem.left), 10);

	// Calculate the embedded image.
	if (m_pEmbed->m_bHasThumbnail)
		m_thumbnailSize = EnsureMaximumSize(m_pEmbed->m_thumbnailWidth, m_pEmbed->m_thumbnailHeight, maxImgWidth, maxHeight);
	else if (m_pEmbed->m_bHasImage)
		m_imageSize = EnsureMaximumSize(m_pEmbed->m_imageWidth, m_pEmbed->m_imageHeight, maxImgWidth, maxHeight);

	sz.cx = std::max({ sz.cx, m_imageSize.cx, m_thumbnailSize.cx });
	sz.cy = m_providerSize.cy;
	if (m_authorSize.cy)      sz.cy += m_authorSize.cy      + (sz.cy != 0 ? gap : 0);
	if (m_titleSize.cy)       sz.cy += m_titleSize.cy       + (sz.cy != 0 ? gap : 0);
	if (m_imageSize.cy)       sz.cy += m_imageSize.cy       + (sz.cy != 0 ? gap : 0);
	if (m_thumbnailSize.cy)   sz.cy += m_thumbnailSize.cy   + (sz.cy != 0 ? gap : 0);
	if (m_descriptionSize.cy) sz.cy += m_descriptionSize.cy + (sz.cy != 0 ? gap : 0);
	if (m_footerSize.cy || m_dateSize.cy)
		sz.cy += std::max(m_footerSize.cy, m_dateSize.cy) + (sz.cy != 0 ? gap : 0);

	sz.cx += borderSize * 2;
	sz.cy += borderSize * 2;
	m_size = sz;

	SelectObject(hdc, oldObj);
}

void RichEmbedItem::Draw(HDC hdc, RECT& messageRect)
{
	RECT rc = messageRect;
	rc.right = rc.left + m_size.cx + ScaleByDPI(4);
	rc.bottom = rc.top + m_size.cy;

	assert(m_size.cx && m_size.cy);

	RECT rcGradient = rc, rcLine = rc;
	rcLine.right = rcLine.left + ScaleByDPI(4);
	rcGradient.left += ScaleByDPI(4);
	COLORREF oldColor = SetBkColor(hdc, GetSysColor(COLOR_SCROLLBAR));
	if (GetLocalSettings()->GetMessageStyle() == MS_GRADIENT)
		FillGradientColors(hdc, &rcGradient, GetSysColor(COLOR_WINDOWFRAME), CLR_NONE, true);
	else
		FillRect(hdc, &rcGradient, GetSysColorBrush(COLOR_SCROLLBAR));

	COLORREF oldCol = ri::SetDCBrushColor(hdc, m_color);
	FillRect(hdc, &rcLine, GetStockBrush(DC_BRUSH));
	ri::SetDCBrushColor(hdc, oldCol);

	// Draw the thing
	const int borderSize = ScaleByDPI(10);
	const int gap = ScaleByDPI(5);
	int sizeY = 0;
	rc.left   += ScaleByDPI(4);
	rc.left   += borderSize;
	rc.top    += borderSize;
	rc.right  -= borderSize;
	rc.bottom -= borderSize;
	HGDIOBJ oldObj = SelectObject(hdc, GetStockFont(ANSI_VAR_FONT));
	COLORREF windowTextColor = GetSysColor(COLOR_WINDOWTEXT);

	if (m_providerSize.cy) {
		SelectObject(hdc, g_ReplyTextFont);
		if (sizeY) sizeY += gap;
		RECT rcText = rc;
		rcText.top = rc.top + sizeY;
		rcText.bottom = rcText.top + m_providerSize.cy;
		DrawText(hdc, m_providerName.GetWrapped(), -1, &rcText, DT_SINGLELINE | DT_WORD_ELLIPSIS | DT_CALCRECT);
		DrawText(hdc, m_providerName.GetWrapped(), -1, &rcText, DT_SINGLELINE | DT_WORD_ELLIPSIS);
		m_providerRect = rcText;
		sizeY += m_providerSize.cy;
	}
	if (m_authorSize.cy) {
		SelectObject(hdc, g_DateTextFont);
		if (sizeY) sizeY += gap;
		RECT rcText = rc;
		rcText.top = rc.top + sizeY;
		rcText.bottom = rcText.top + m_authorSize.cy;
		DrawText(hdc, m_authorName.GetWrapped(), -1, &rcText, DT_SINGLELINE | DT_WORD_ELLIPSIS | DT_CALCRECT);
		DrawText(hdc, m_authorName.GetWrapped(), -1, &rcText, DT_SINGLELINE | DT_WORD_ELLIPSIS);
		m_authorRect = rcText;
		sizeY += m_authorSize.cy;
	}
	if (m_titleSize.cy) {
		SelectObject(hdc, g_AuthorTextFont);
		if (sizeY) sizeY += gap;
		RECT rcText = rc;
		rcText.top = rc.top + sizeY;
		rcText.bottom = rcText.top + m_titleSize.cy;
		COLORREF oldColorText = SetTextColor(hdc, m_pEmbed->m_url.empty() ? windowTextColor : COLOR_LINK);
		DrawText(hdc, m_title.GetWrapped(), -1, &rcText, DT_SINGLELINE | DT_WORD_ELLIPSIS | DT_CALCRECT);
		DrawText(hdc, m_title.GetWrapped(), -1, &rcText, DT_SINGLELINE | DT_WORD_ELLIPSIS);
		SetTextColor(hdc, oldColorText);
		m_titleRect = rcText;
		sizeY += m_titleSize.cy;
	}
	if (m_descriptionSize.cy) {
		SelectObject(hdc, g_MessageTextFont);
		if (sizeY) sizeY += gap;
		RECT rcText = rc;
		rcText.top = rc.top + sizeY;
		rcText.bottom = rcText.top + m_descriptionSize.cy;
		DrawText(hdc, m_description.GetWrapped(), -1, &rcText, DT_WORDBREAK | DT_WORD_ELLIPSIS);
		sizeY += m_descriptionSize.cy;
	}
	if (m_thumbnailSize.cy) {
		m_thumbnailResourceID = GetAvatarCache()->MakeIdentifier(m_pEmbed->m_thumbnailUrl);
		GetAvatarCache()->AddImagePlace(m_thumbnailResourceID, eImagePlace::ATTACHMENTS, m_pEmbed->m_thumbnailProxiedUrl);
		HBITMAP hbm = GetAvatarCache()->GetBitmap(m_pEmbed->m_thumbnailUrl);
		if (sizeY) sizeY += gap;
		m_thumbnailRect = { rc.left, rc.top + sizeY, rc.left + m_thumbnailSize.cx, rc.top + sizeY + m_thumbnailSize.cy };
		DrawBitmap(hdc, hbm, rc.left, rc.top + sizeY, NULL, CLR_NONE, m_thumbnailSize.cx, m_thumbnailSize.cy);
		sizeY += m_thumbnailSize.cy;
	}
	if (m_imageSize.cy) {
		m_imageResourceID = GetAvatarCache()->MakeIdentifier(m_pEmbed->m_imageUrl);
		GetAvatarCache()->AddImagePlace(m_imageResourceID, eImagePlace::ATTACHMENTS, m_pEmbed->m_imageProxiedUrl);
		HBITMAP hbm = GetAvatarCache()->GetBitmap(m_pEmbed->m_imageUrl);
		if (sizeY) sizeY += gap;
		m_imageRect = { rc.left, rc.top + sizeY, rc.left + m_imageSize.cx, rc.top + sizeY + m_imageSize.cy };
		DrawBitmap(hdc, hbm, rc.left, rc.top + sizeY, NULL, CLR_NONE, m_imageSize.cx, m_imageSize.cy);
		sizeY += m_imageSize.cy;
	}
	if (m_footerSize.cy) {
		SelectObject(hdc, g_ReplyTextFont);
		if (sizeY) sizeY += gap;
		RECT rcText = rc;
		rcText.top = rc.top + sizeY;
		rcText.bottom = rcText.top + m_footerSize.cy;
		DrawText(hdc, m_footerText.GetWrapped(), -1, &rcText, DT_SINGLELINE | DT_WORDBREAK | DT_WORD_ELLIPSIS);
		sizeY += m_footerSize.cy;
		if (m_dateSize.cy) {
			rcText.left += m_footerSize.cx + ScaleByDPI(15);
			DrawText(hdc, m_date.GetWrapped(), -1, &rcText, DT_SINGLELINE | DT_WORDBREAK | DT_WORD_ELLIPSIS);
		}
	}
	else if (m_dateSize.cy) {
		SelectObject(hdc, g_ReplyTextFont);
		if (sizeY) sizeY += gap;
		RECT rcText = rc;
		rcText.top = rc.top + sizeY;
		rcText.bottom = rcText.top + m_footerSize.cy;
		DrawText(hdc, m_date.GetWrapped(), -1, &rcText, DT_SINGLELINE | DT_WORDBREAK | DT_WORD_ELLIPSIS);
		sizeY += m_footerSize.cy;
	}

	SetBkColor(hdc, oldColor);
	SelectObject(hdc, oldObj);
}

void MessageList::MeasureMessage(
	HDC hdc,
	FormattedText& strMsg,
	LPCTSTR strAuth,
	LPCTSTR strDate,
	LPCTSTR strDateEdit,
	LPCTSTR strReplyMsg,
	LPCTSTR strReplyAuth,
	const RECT& msgRect,
	int& textheight,
	int& authheight,
	int& authwidth,
	int& datewidth,
	int& replymsgwidth,
	int& replyauthwidth,
	int& replyheight)
{
	bool bIsCompact = IsCompact();

	// measure the author text
	HGDIOBJ gdiObj = SelectObject(hdc, g_AuthorTextFont);
	RECT rcCommon1 = msgRect, rcCommon2;
	rcCommon1.right  -= ScaleByDPI(20);
	rcCommon1.bottom -= ScaleByDPI(20);
	rcCommon2 = rcCommon1;
	if (!bIsCompact)
		rcCommon1.right -= ScaleByDPI(PROFILE_PICTURE_SIZE_DEF + 12);

	RECT rc = rcCommon1;
	DrawText(hdc, strAuth, -1, &rc, DT_CALCRECT | DT_NOPREFIX);
	authheight = rc.bottom - rc.top, authwidth = rc.right - rc.left;
	
	if (strReplyMsg || strReplyAuth)
	{
		int replyheight2 = 0;
		SelectObject(hdc, g_ReplyTextFont);
		rc = rcCommon1;
		if (strReplyAuth) {
			replyheight = DrawText(hdc, strReplyAuth, -1, &rc, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
			replyauthwidth = rc.right - rc.left;
		}
		else replyheight = replyauthwidth = 0;

		if (strReplyMsg) {
			rc = rcCommon1;
			rc.right -= ScaleByDPI(5) - replyauthwidth;
			replyheight2 = DrawText(hdc, strReplyMsg, -1, &rc, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
			replymsgwidth = rc.right - rc.left;
		}
		else replyheight2 = replymsgwidth = 0;

		replyheight = std::max(replyheight, replyheight2);
	}
	else {
		replyheight = replyauthwidth = replymsgwidth = 0;
	}

	if (bIsCompact)
	{
		datewidth = 0;
		SelectObject(hdc, g_DateTextFont);
		rc = msgRect;
		rc.right  -= ScaleByDPI(20);
		rc.bottom -= ScaleByDPI(20);
		DrawText(hdc, strDate, -1, &rc, DT_CALCRECT | DT_NOPREFIX);
		datewidth += rc.right - rc.left;
		if (strDateEdit) {
			rc = msgRect;
			rc.right  -= ScaleByDPI(20);
			rc.bottom -= ScaleByDPI(20);
			DrawText(hdc, strDateEdit, -1, &rc, DT_CALCRECT | DT_NOPREFIX);
			datewidth += rc.right - rc.left + ScaleByDPI(10);
		}
	}
	else datewidth = 0;

	SelectObject(hdc, g_MessageTextFont);
	rc = msgRect;
	rc.right  -= ScaleByDPI(20);
	rc.bottom -= ScaleByDPI(20);
	if (!bIsCompact)
		rc.right -= ScaleByDPI(PROFILE_PICTURE_SIZE_DEF + 12);

	DrawingContext mddc(hdc);

	strMsg.Layout(&mddc, Rect(W32RECT(rc)), bIsCompact ? (authwidth + datewidth + ScaleByDPI(10)) : 0);
	Rect ext = strMsg.GetExtent();
	textheight = ext.Height();

	SelectObject(hdc, gdiObj);
}

void MessageItem::Update(Snowflake guildID)
{
	bool isCompact = GetSettingsManager()->GetMessageCompact();
	bool isAction = MessageList::IsActionMessage(m_msg.m_type);

	Clear();
	m_bNeedUpdate = false;
	m_author = ConvertCppStringToTString(m_msg.m_author);
	m_date = ConvertCppStringToTString(isCompact ? m_msg.m_dateCompact : m_msg.m_dateFull);
	m_dateEdited = ConvertCppStringToTString(isCompact ? m_msg.m_editedTextCompact : m_msg.m_editedText);

	if (m_msg.m_bHaveReferencedMessage)
	{
		std::string authorStr = m_msg.m_referencedMessage.m_author;
		if (m_msg.m_referencedMessage.m_bMentionsAuthor) {
			authorStr = "@" + authorStr;
		}

		m_replyMsg = ConvertCppStringToTString(m_msg.m_referencedMessage.m_message);
		m_replyAuth = ConvertCppStringToTString(authorStr);
	}

	m_bWasMentioned = m_msg.CheckWasMentioned(GetDiscordInstance()->GetUserID(), guildID);

	size_t sz = m_msg.m_attachments.size();
	m_attachmentData.clear();
	m_attachmentData.resize(sz);

	for (size_t i = 0; i < sz; i++)
	{
		auto& item = m_attachmentData[i];
		item.m_pAttachment = &m_msg.m_attachments[i];
		item.Update();
	}

	m_interactableData.clear();

	if (!isAction)
	{
		if (m_message.Empty())
			m_message.SetMessage(m_msg.m_message);

		auto& words = m_message.GetWords();
		for (size_t i = 0; i < words.size(); i++) {
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
					if (word.m_content.size() > 2) {
						if (word.m_content[1] == '&')
							isRole = true;
					}

					std::string mentDest = word.m_content.substr(isRole ? 2 : 1);
					Snowflake sf = (Snowflake)GetIntFromString(mentDest);
					item.m_affected = sf;

					if (isRole)
						item.m_text = "@" + GetDiscordInstance()->LookupRoleNameGlobally(sf);
					else
						item.m_text = "@" + GetDiscordInstance()->LookupUserNameGlobally(sf, guildID);
					changed = true;
				}
			}

			if (changed)
				word.SetContentOverride(item.m_text);

			m_interactableData.push_back(item);
		}
	}
	else
	{
		InteractableItem ii;
		ii.m_type = InteractableItem::LINK;
		ii.m_text = "";
		ii.m_destination = "";
		ii.m_wordIndex = 1;
		m_interactableData.clear();
		m_interactableData.push_back(ii);
	}

	m_embedData.clear();
	m_embedData.resize(m_msg.m_embeds.size());
	for (size_t i = 0; i < m_msg.m_embeds.size(); i++)
	{
		RichEmbedItem& item = m_embedData[i];
		RichEmbed& embed = m_msg.m_embeds[i];

		item.m_pEmbed = &embed;
		item.Update();

		// If there is an image:
		if (item.m_pEmbed->m_bHasImage || item.m_pEmbed->m_bHasThumbnail)
		{
			InteractableItem ii;
			ii.m_type = InteractableItem::EMBED_IMAGE;
			ii.m_text = "";
			if (item.m_pEmbed->m_bHasThumbnail) {
				ii.m_imageWidth  = item.m_pEmbed->m_thumbnailWidth;
				ii.m_imageHeight = item.m_pEmbed->m_thumbnailHeight;
			}
			else {
				ii.m_imageWidth  = item.m_pEmbed->m_imageWidth;
				ii.m_imageHeight = item.m_pEmbed->m_imageHeight;
			}
			if (item.m_pEmbed->m_bHasImage) {
				ii.m_destination = item.m_pEmbed->m_imageUrl;
				ii.m_proxyDest   = item.m_pEmbed->m_imageProxiedUrl;
			}
			else {
				ii.m_destination = item.m_pEmbed->m_thumbnailUrl;
				ii.m_proxyDest   = item.m_pEmbed->m_thumbnailProxiedUrl;
			}
			ii.m_wordIndex = i + (size_t)InteractableItem::EMBED_OFFSET;
			m_interactableData.push_back(ii);
		}

		InteractableItem iil;
		iil.m_type = InteractableItem::EMBED_LINK;
		iil.m_wordIndex = i + (size_t)InteractableItem::EMBED_OFFSET;
		iil.m_text = "";
		if (!item.m_pEmbed->m_providerUrl.empty() && !item.m_pEmbed->m_providerName.empty())
		{
			iil.m_destination = item.m_pEmbed->m_providerUrl;
			iil.m_placeInEmbed = EMBED_IN_PROVIDER;
			m_interactableData.push_back(iil);
		}
		if (!item.m_pEmbed->m_authorUrl.empty() && !item.m_pEmbed->m_authorName.empty())
		{
			iil.m_destination = item.m_pEmbed->m_authorUrl;
			iil.m_placeInEmbed = EMBED_IN_AUTHOR;
			m_interactableData.push_back(iil);
		}
		if (!item.m_pEmbed->m_url.empty() && !item.m_pEmbed->m_title.empty())
		{
			iil.m_destination = item.m_pEmbed->m_url;
			iil.m_placeInEmbed = EMBED_IN_TITLE;
			m_interactableData.push_back(iil);
		}
	}
}

LRESULT CALLBACK MessageList::ParentWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	MessageList* pThis = (MessageList*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	switch (uMsg)
	{
		case WM_SIZE:
		{
			pThis->ProperlyResizeSubWindows();
			break;
		}
		case WM_NCCREATE:
		{
			CREATESTRUCT* strct = (CREATESTRUCT*)lParam;

			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)strct->lpCreateParams);

			break;
		}
		case WM_VSCROLL:
		{
			SendMessage(pThis->m_scrollable_hwnd, WM_VSCROLL, wParam, lParam);
			break;
		}
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void MessageList::DeleteMessage(Snowflake sf)
{
	std::list<MessageItem>::reverse_iterator iter;

	for (iter = m_messages.rbegin(); iter != m_messages.rend(); ++iter)
	{
		if (sf == iter->m_msg.m_snowflake)
			break;
	}

	if (iter == m_messages.rend())
	{
		DbgPrintW("Message with id %lld not found to be deleted", sf);
		return;
	}

	// delete it
	RECT messageRect = iter->m_rect;
	int messageHeight = iter->m_height;
	m_messages.erase(--(iter.base())); // still stupid that I have to do this decrement crap

	UpdateScrollBar(-messageHeight, -messageHeight, false, false);

	SendMessage(m_scrollable_hwnd, WM_MSGLIST_PULLDOWN, 0, (LPARAM)&messageRect);
}

void MessageList::Repaint()
{
	InvalidateRect(m_scrollable_hwnd, NULL, false);
}

void MessageList::ClearMessages()
{
	m_messages.clear();
	m_total_height = 0;
}

void MessageList::RefetchMessages(Snowflake gapCulprit)
{
	RECT rect;
	GetClientRect(m_scrollable_hwnd, &rect);

	int pageHeight = rect.bottom - rect.top;

	SCROLLINFO scrollInfo;
	scrollInfo.cbSize = sizeof scrollInfo;
	scrollInfo.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;

	GetScrollInfo(m_scroll_hwnd, SB_CTL, &scrollInfo);

	// Find the position of the old message
	int oldYMessage = 0;
	for (auto& msg : m_messages) {
		if (msg.m_msg.m_snowflake == m_firstShownMessage) {
			if (m_bManagedByOwner && msg.m_msg.m_bIsDateGap)
				oldYMessage += DATE_GAP_HEIGHT;
			break;
		}
		oldYMessage += msg.m_height;
	}

	int oldScrollDiff = oldYMessage - scrollInfo.nPos;

	bool haveUpdateRect = false;
	RECT updateRect;

	std::set<Snowflake> usersToLoad;

	uint64_t ts = GetTimeUs();
	if (gapCulprit)
	{
		for (auto iter = m_messages.begin(); iter != m_messages.end(); ++iter)
		{
			if (gapCulprit == iter->m_msg.m_snowflake)
			{
				updateRect = iter->m_rect;
				haveUpdateRect = true;
				break;
			}
		}
	}

	std::list<MessageItem> oldMessages = std::move(m_messages);
	m_messages.clear();

	std::map<Snowflake, MessageItem*> oldMessageKey;

	for (auto it = oldMessages.begin();
		it != oldMessages.end();
		++it) {
		oldMessageKey[it->m_msg.m_snowflake] = &*it;
	}

	// this sucks, but eh
	std::list<Message> msgs;
	GetMessageCache()->GetLoadedMessages(m_channelID, m_guildID, msgs);

	// If you can't read the message history, no */messages GET requests can be issued, so do this
	// dumb hack to disable them
	Guild* pGuild = GetDiscordInstance()->GetGuild(m_guildID);
	assert(pGuild);
	Channel* pChannel = pGuild->GetChannel(m_channelID);
	assert(pChannel);

	bool canViewMessageHistory = pChannel->HasPermission(PERM_READ_MESSAGE_HISTORY);

	if (!canViewMessageHistory)
	{
		for (auto& msg : msgs)
		{
			if (msg.m_type == MessageType::GAP_UP || msg.m_type == MessageType::GAP_DOWN || msg.m_type == MessageType::GAP_AROUND)
			{
				msg.m_type = MessageType::CANT_VIEW_MSG_HISTORY;
				msg.m_message = "";
				msg.m_author = "#" + pChannel->m_name;
			}
		}
	}

	std::list<Message>::iterator start = msgs.begin(), end = msgs.end();

	// Find the message we are looking at.

	// TODO: Fix this.  It's supposed to only include messages from the island we are looking at, for
	// a nicer experience, but it doesn't work...

	/*
	//m_firstShownMessage
	for (auto iter = msgs.begin(); iter != msgs.end(); ++iter)
	{
		auto iter2 = iter;
		++iter2;

		if (iter->m_snowflake <= m_firstShownMessage && m_firstShownMessage < iter2->m_snowflake) {
			start = end = iter;
			break;
		}
	}

	// Retreat the start iterator until a gap message is found.
	for (; start != msgs.begin(); --start)
	{
		if (start->IsLoadGap())
			break;
	}

	// Advance the end iterator until a gap message is found.
	for (; end != msgs.end(); ++end)
	{
		if (end->IsLoadGap())
			break;
	}
	
	if (end != msgs.end())
		++end;
	*/

	for (auto iter = start; iter != end; ++iter)
	{
		auto& msg = *iter;

		// If the message is already present:
		auto oldMsg = oldMessageKey[msg.m_snowflake];

		if (GetProfileCache()->NeedRequestGuildMember(msg.m_author_snowflake, m_guildID))
			usersToLoad.insert(msg.m_author_snowflake);

		bool bNeedInsertNew = false;
		if (!oldMsg) {
			bNeedInsertNew = true;
		}
		else if (oldMsg->m_msg.m_message != msg.m_message ||
			oldMsg->m_msg.m_dateTime != msg.m_dateTime) {
			bNeedInsertNew = true;
		}

		if (bNeedInsertNew)
		{
			MessageItem mi;
			mi.m_msg = msg;
			mi.Update(m_guildID);
			m_messages.push_back(mi);

			for (auto& id : mi.m_interactableData) {
				if (id.m_type == InteractableItem::MENTION &&
					id.m_destination.size() >= 2 &&
					id.m_destination[0] == '@' &&
					id.m_destination[1] != '&') {

					if (GetProfileCache()->NeedRequestGuildMember(id.m_affected, m_guildID))
						usersToLoad.insert(id.m_affected);
				}
			}
		}
		else {
			MessageItem item = std::move(*oldMsg);
			// we will, however, need to clear some things
			item.m_messageRect = {};
			item.m_authorRect = {};
			item.m_avatarRect = {};
			item.m_message.ClearFormatting();
			item.ClearAttachmentDataRects();
			item.ClearInteractableDataRects();
			item.m_bKeepWordsUpdate = true;
			item.m_bKeepHeightRecalc = true;

			m_messages.push_back(std::move(item));
		}
	}
	uint64_t te = GetTimeUs();
	DbgPrintW("Relocation process took %lld us", te - ts);

	oldMessages.clear();

	ts = GetTimeUs();
	RecalcMessageSizes(gapCulprit == 0);
	te = GetTimeUs();
	DbgPrintW("Recalculation process took %lld us", te - ts);

	int newYMessage = 0;
	for (auto& msg : m_messages) {
		if (msg.m_msg.m_snowflake == m_firstShownMessage) {
			if (!m_bManagedByOwner && msg.m_msg.m_bIsDateGap)
				newYMessage += DATE_GAP_HEIGHT;
			break;
		}
		newYMessage += msg.m_height;
	}

	int th = m_total_height - 1;
	if (th <= 0)
		th = 0;

	scrollInfo.nPos = newYMessage - oldScrollDiff;
	scrollInfo.nMin = 0;
	scrollInfo.nMax = th;
	scrollInfo.nPage = pageHeight;

	int scrollAnyway = scrollInfo.nPos;

	if (scrollInfo.nPos < 0)
		scrollInfo.nPos = 0;

	scrollAnyway -= scrollInfo.nPos;

	SetScrollInfo(m_scroll_hwnd, SB_CTL, &scrollInfo, true);
	GetScrollInfo(m_scroll_hwnd, SB_CTL, &scrollInfo);

	m_oldPos = scrollInfo.nPos;

	if (m_messageSentTo)
	{
		// Send to the message without creating a new gap.
		m_emphasizedMessage = m_messageSentTo;
		m_firstShownMessage = m_messageSentTo;
		if (SendToMessage(m_messageSentTo, false))
			m_messageSentTo = 0;
	}
	else if (!m_bManagedByOwner)
	{
		if (scrollAnyway)
			WindowScroll(m_scrollable_hwnd, -scrollAnyway);

		InvalidateRect(m_hwnd, haveUpdateRect ? &updateRect : NULL, false);
	}

	// send request to discord if needed
	GetDiscordInstance()->RequestGuildMembers(m_guildID, usersToLoad);
}

// Thanks Raymond!
COLORREF GetBrushColor(HBRUSH brush)
{
	LOGBRUSH lbr;
	if (GetObject(brush, sizeof(lbr), &lbr) != sizeof(lbr)) {
		// Not even a brush!
		return CLR_NONE;
	}
	if (lbr.lbStyle != BS_SOLID) {
		// Not a solid color brush.
		return CLR_NONE;
	}
	return lbr.lbColor;
}

void MessageList::ProperlyResizeSubWindows()
{
	RECT rect, rect2;
	GetClientRect(m_hwnd, &rect);
	rect2 = rect;

	int scrollBarWidth = GetSystemMetrics(SM_CXVSCROLL);
	rect.right -= scrollBarWidth;
	MoveWindow(m_scrollable_hwnd, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, true);
	rect = rect2;

	rect.left = rect.right - scrollBarWidth;
	MoveWindow(m_scroll_hwnd, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, true);
}

void MessageList::HitTestAuthor(POINT pt, BOOL& hit)
{
	auto msg = FindMessageByPointAuthorRect(pt);

	if (msg == m_messages.end() || msg->m_msg.m_snowflake != m_highlightedMessage)
	{
		if (m_highlightedMessage)
		{
			auto msg2 = FindMessage(m_highlightedMessage);
			if (msg2 != m_messages.end())
			{
				msg2->m_authorHighlighted = false;
				InvalidateRect(m_scrollable_hwnd, &msg2->m_authorRect, false);
			}
			SetCursor(LoadCursor(NULL, IDC_ARROW));
		}

		m_highlightedMessage = 0;

		if (msg == m_messages.end())
			return;
	}
	
	SetCursor(LoadCursor(NULL, IDC_HAND));
	hit = TRUE;
	if (m_highlightedMessage == msg->m_msg.m_snowflake)
		return;

	m_highlightedMessage = msg->m_msg.m_snowflake;
	msg->m_authorHighlighted = true;
	InvalidateRect(m_scrollable_hwnd, &msg->m_authorRect, false);
	DbgPrintW("Hand!  Profile ID: %lld   Message ID: %lld", msg->m_msg.m_author_snowflake, msg->m_msg.m_snowflake);
}

void MessageList::HitTestAttachments(POINT pt, BOOL& hit)
{
	auto msg = FindMessageByPoint(pt);

	if (msg == m_messages.end())
	{
	_fail:
		if (m_highlightedAttachmentMessage)
		{
			auto msg2 = FindMessage(m_highlightedAttachmentMessage);
			if (msg2 != m_messages.end())
			{
				for (auto& x : msg2->m_attachmentData) {
					if (x.m_bHighlighted) {
						x.m_bHighlighted = false;

						if (!x.m_pAttachment->IsImage())
							InvalidateRect(m_scrollable_hwnd, &x.m_textRect, false);
					}
				}
			}

			SetCursor(LoadCursor(NULL, IDC_ARROW));
		}

		m_highlightedAttachment = 0;
		m_highlightedAttachmentMessage = 0;
		return;
	}

	AttachmentItem* pAttach = NULL;
	bool inText = false, inAdd = false;
	for (size_t i = 0; i < msg->m_attachmentData.size(); i++)
	{
		AttachmentItem* pData = &msg->m_attachmentData[i];

		inText = PtInRect(&pData->m_textRect, pt);
		inAdd  = PtInRect(&pData->m_addRect, pt);
		if (inText || inAdd) {
			pAttach = pData;
			break;
		}
	}

	if (!pAttach) {
		msg = m_messages.end();
		goto _fail;
	}

	SetCursor(LoadCursor(NULL, IDC_HAND));
	hit = TRUE;
	if (pAttach->m_pAttachment->m_id == m_highlightedAttachment)
		return;

	m_highlightedAttachment = pAttach->m_pAttachment->m_id;
	m_highlightedAttachmentMessage = msg->m_msg.m_snowflake;
	pAttach->m_bHighlighted = true;

	if (!pAttach->m_pAttachment->IsImage())
	{
		if (inText)
			InvalidateRect(m_scrollable_hwnd, &pAttach->m_textRect, false);
	}
	DbgPrintW("Hand!  Attachment ID: %lld   Message ID: %lld", pAttach->m_pAttachment->m_id, msg->m_msg.m_snowflake);
}

void MessageList::HitTestInteractables(POINT pt, BOOL& hit)
{
	auto msg = FindMessageByPoint(pt);

	if (msg == m_messages.end())
	{
	_fail:
		if (m_highlightedInteractableMessage)
		{
			auto msg2 = FindMessage(m_highlightedInteractableMessage);
			if (msg2 != m_messages.end())
			{
				for (auto& x : msg2->m_interactableData) {
					if (x.m_bHighlighted) {
						x.m_bHighlighted = false;
						if (x.ShouldInvalidateOnHover())
							InvalidateRect(m_scrollable_hwnd, &x.m_rect, false);
					}
				}
			}

			SetCursor(LoadCursor(NULL, IDC_ARROW));
		}

		m_highlightedInteractable = SIZE_MAX;
		m_highlightedInteractableMessage = 0;
		return;
	}

	InteractableItem* pItem = NULL;
	for (size_t i = 0; i < msg->m_interactableData.size(); i++)
	{
		InteractableItem* pData = &msg->m_interactableData[i];

		if (PtInRect(&pData->m_rect, pt)) {
			pItem = pData;
			break;
		}
	}

	if (!pItem) {
		msg = m_messages.end();
		goto _fail;
	}

	SetCursor(LoadCursor(NULL, IDC_HAND));
	hit = TRUE;
	if (pItem->m_wordIndex == m_highlightedInteractable)
		return;

	// find old interactable
	for (size_t i = 0; i < msg->m_interactableData.size(); i++) {
		InteractableItem* pData = &msg->m_interactableData[i];
		if (pData->m_wordIndex == m_highlightedInteractable) {
			pData->m_bHighlighted = false;
			if (pData->ShouldInvalidateOnHover())
				InvalidateRect(m_scrollable_hwnd, &pData->m_rect, false);
			break;
		}
	}

	m_highlightedInteractable = pItem->m_wordIndex;
	m_highlightedInteractableMessage = msg->m_msg.m_snowflake;
	pItem->m_bHighlighted = true;
	if (pItem->ShouldInvalidateOnHover())
		InvalidateRect(m_scrollable_hwnd, &pItem->m_rect, false);

	DbgPrintW("Hand!  Interactable IDX: %zu   Message ID: %lld", pItem->m_wordIndex, msg->m_msg.m_snowflake);
}

void MessageList::OpenAttachment(AttachmentItem* pItem)
{
	std::string fileName, url;
	Attachment* pAttach = pItem->m_pAttachment;
	fileName = pAttach->m_fileName;
	url = pAttach->m_actualUrl;

	// Check if the file name is a potentially dangerous download
	if (IsPotentiallyDangerousDownload(fileName))
	{
		LPCTSTR urlTStr = ConvertCppStringToTString(url);
		TCHAR buffer[4096];
		WAsnprintf(buffer, _countof(buffer), TmGetTString(IDS_DANGEROUS_DOWNLOAD), urlTStr);
		buffer[_countof(buffer) - 1] = 0;
		free((void*)urlTStr);
		int result = MessageBox(
			g_Hwnd,
			buffer,
			GetTextManager()->GetTString(IDS_DANGEROUS_DOWNLOAD_TITLE),
			MB_ICONWARNING | MB_YESNO
		);

		if (result != IDYES)
			return;
	}

	if (pAttach->IsImage())
	{
		CreateImageViewer(
			pAttach->m_proxyUrl,
			pAttach->m_actualUrl,
			pAttach->m_fileName,
			pAttach->m_width,
			pAttach->m_height
		);
		return;
	}
	
	DownloadFileDialog(GetParent(m_hwnd), url, pAttach->m_fileName);
}

void MessageList::OpenInteractable(InteractableItem* pItem)
{
	switch (pItem->m_type)
	{
		case InteractableItem::EMBED_IMAGE: {
			std::string url = pItem->m_destination;
			std::string proxyUrl = pItem->m_proxyDest;
			std::string fileName = url;
			for (size_t i = fileName.size(); i != 0; i--) {
				if (fileName[i] == '/') {
					fileName = fileName.substr(i + 1);
					break;
				}
			}
			CreateImageViewer(
				proxyUrl,
				url,
				fileName,
				pItem->m_imageWidth,
				pItem->m_imageHeight
			);
			break;
		}
		case InteractableItem::LINK:
		case InteractableItem::EMBED_LINK:
			ConfirmOpenLink(pItem->m_destination);
			break;
		case InteractableItem::MENTION: {
			if (pItem->m_destination.size() < 2)
				break;

			char mentType = pItem->m_destination[0];
			Snowflake sf = 0;
			if (pItem->m_destination[1] == '&') // roles do nothing
				break;

			sf = pItem->m_affected;
			switch (mentType) {
				case '@': {
					RECT rect;
					GetWindowRect(m_scrollable_hwnd, &rect);
					ShowProfilePopout(sf, m_guildID, rect.left + pItem->m_rect.right + 10, rect.top + pItem->m_rect.top);
					break;
				}
				case '#': {
					// find channel
					Channel* pNewChan = GetDiscordInstance()->GetChannel(sf);
					if (!pNewChan)
						break;

					Snowflake oldGuildID = m_guildID, oldChannelID = m_channelID;

					if (oldGuildID != pNewChan->m_parentGuild)
						GetDiscordInstance()->OnSelectGuild(pNewChan->m_parentGuild);

					if (oldChannelID != pNewChan->m_snowflake)
						GetDiscordInstance()->OnSelectChannel(pNewChan->m_snowflake);
				}
			}

			break;
		}
		// timestamp TODO
	}
}

void MessageList::DrawImageAttachment(HDC hdc, RECT& paintRect, AttachmentItem& attachItem, RECT& attachRect)
{
	Attachment* pAttach = attachItem.m_pAttachment;
	std::string url = pAttach->m_proxyUrl;

	RECT childAttachRect = attachRect;
	childAttachRect.right  = childAttachRect.left + pAttach->m_previewWidth;
	childAttachRect.bottom = childAttachRect.top + pAttach->m_previewHeight;

	attachItem.m_boxRect = childAttachRect;
	attachItem.m_textRect = childAttachRect;

	attachRect.bottom = attachRect.top = childAttachRect.bottom + ATTACHMENT_GAP;

	RECT intersRect;
	if (!IntersectRect(&intersRect, &paintRect, &childAttachRect))
		return;

	if (pAttach->PreviewDifferent())
	{
		bool hasQMark = false;
		for (auto ch : url) {
			if (ch == '?') {
				hasQMark = true;
				break;
			}
		}

		if (url.empty() || url[url.size() - 1] != '&' || url[url.size() - 1] != '?')
			url += hasQMark ? "&" : "?";

		url += "width=" + std::to_string(pAttach->m_previewWidth);
		url += "&height=" + std::to_string(pAttach->m_previewHeight);
	}

	GetAvatarCache()->AddImagePlace(attachItem.m_resourceID, eImagePlace::ATTACHMENTS, url, pAttach->m_id);

	HBITMAP hbm = GetAvatarCache()->GetBitmap(attachItem.m_resourceID);
	DrawBitmap(hdc, hbm, childAttachRect.left, childAttachRect.top, &childAttachRect);
}

void MessageList::DrawDefaultAttachment(HDC hdc, RECT& paintRect, AttachmentItem& attachItem, RECT& attachRect)
{
	RECT childAttachRect = attachRect;
	childAttachRect.bottom = childAttachRect.top + ATTACHMENT_HEIGHT;

	RECT intersRect;
	bool inView = IntersectRect(&intersRect, &paintRect, &childAttachRect);

	int iconSize = ScaleByDPI(32);
	int rectHeight = childAttachRect.bottom - childAttachRect.top;

	if (inView)
	{
		DrawIconEx(
			hdc,
			childAttachRect.left + 4,
			childAttachRect.top + rectHeight / 2 - iconSize / 2,
			LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_FILE)),
			iconSize,
			iconSize,
			0,
			NULL,
			DI_COMPAT | DI_NORMAL
		);
	}

	COLORREF old = CLR_NONE;
	if (attachItem.m_bHighlighted && inView)
		old = SetTextColor(hdc, RGB(0, 0, 255));

	LPCTSTR name = attachItem.m_nameText;
	RECT rcMeasure;
	SetRectEmpty(&rcMeasure);
	SelectObject(hdc, attachItem.m_bHighlighted ? g_AuthorTextUnderlinedFont : g_AuthorTextFont);
	DrawText(hdc, name, -1, &rcMeasure, DT_CALCRECT | DT_NOPREFIX);
	int width = rcMeasure.right - rcMeasure.left;

	RECT textRect = childAttachRect;
	textRect.left += iconSize + ScaleByDPI(8);
	textRect.top += (ATTACHMENT_HEIGHT - rcMeasure.bottom * 2) / 2;
	textRect.bottom = textRect.top + rcMeasure.bottom;
	textRect.left += ScaleByDPI(5);
	textRect.right = textRect.left + rcMeasure.right;

	if (inView)
		DrawText(hdc, name, -1, &textRect, DT_NOPREFIX | DT_NOCLIP);

	attachItem.m_textRect = textRect;

	if (attachItem.m_bHighlighted && inView)
		SetTextColor(hdc, old);

	LPCTSTR sizeStr = attachItem.m_sizeText;
	textRect.top += rcMeasure.bottom;
	textRect.bottom += rcMeasure.bottom;

	if (inView)
	{
		SelectObject(hdc, g_DateTextFont);
		DrawText(hdc, sizeStr, -1, &textRect, DT_NOPREFIX | DT_NOCLIP);
	}

	int dlIconSize = iconSize / 2;

	childAttachRect.right = childAttachRect.left + iconSize + iconSize + width + ScaleByDPI(20);
	attachRect.bottom = attachRect.top = childAttachRect.bottom + ATTACHMENT_GAP;

	int dlIconX = childAttachRect.right - 4 - iconSize + (iconSize - dlIconSize) / 2;
	int dlIconY = childAttachRect.top + rectHeight / 2 - dlIconSize / 2;
	attachItem.m_addRect = { dlIconX, dlIconY, dlIconX + dlIconSize, dlIconY + dlIconSize };
	
	if (inView)
	{
		DrawIconEx(hdc,
			dlIconX,
			dlIconY,
			(HICON) g_DownloadIcon,
			dlIconSize,
			dlIconSize,
			0,
			NULL,
			DI_NORMAL | DI_COMPAT
		);

		DrawEdge(hdc, &childAttachRect, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_RECT);
	}

	attachItem.m_boxRect = childAttachRect;
}

// XXX: Keep IsActionMessage and DetermineMessageData in sync, please.
bool MessageList::IsActionMessage(MessageType::eType msgType)
{
	switch (msgType)
	{
		case MessageType::USER_JOIN:
		case MessageType::CHANNEL_PINNED_MESSAGE:
		case MessageType::RECIPIENT_ADD:
		case MessageType::RECIPIENT_REMOVE:
		case MessageType::CHANNEL_NAME_CHANGE:
		case MessageType::CHANNEL_ICON_CHANGE:
		case MessageType::GAP_UP:
		case MessageType::GAP_DOWN:
		case MessageType::GAP_AROUND:
		case MessageType::CANT_VIEW_MSG_HISTORY:
		case MessageType::LOADING_PINNED_MESSAGES:
		case MessageType::NO_PINNED_MESSAGES:
			return true;
	}

	return false;
}

// This is a horrible mess!
void MessageList::DetermineMessageData(
	Snowflake guildID,                /* IN */
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
)
{
	messagePart1 = NULL;
	messagePart2 = NULL;
	messagePart3 = NULL;
	freedStringSpace = NULL;
	clickableString = NULL;
	clickableSF = 0;
	clickableLink = "";
	icon = 0;

	switch (msgType)
	{
		case MessageType::USER_JOIN:
		{
			int welcomeTextIndex = ExtractTimestamp(id) % g_WelcomeTextCount;
			int left  = g_WelcomeTextIds[welcomeTextIndex * 2 + 0];
			int right = g_WelcomeTextIds[welcomeTextIndex * 2 + 1];
			messagePart1 = left  == 0 ? TEXT("") : TmGetTString(left);
			messagePart2 = right == 0 ? TEXT("") : TmGetTString(right);
			icon = IDI_USER_JOIN;
			break;
		}

		case MessageType::CHANNEL_PINNED_MESSAGE:
		{
			messagePart1 = TEXT("");
			messagePart2 = TEXT(" pinned ");
			messagePart3 = TEXT(" to this channel.");
			clickableString = TEXT("a message");
			clickableLink = CreateMessageLink(refMsgGuildID, refMsgChannelID, refMsgMessageID);
			icon = IDI_PIN;
			break;
		}

		case MessageType::RECIPIENT_ADD:
		case MessageType::RECIPIENT_REMOVE:
		{
			messagePart1 = TEXT("");
			clickableSF = ments.size() != 1 ? 0 : *ments.begin();

			if (msgType == MessageType::RECIPIENT_ADD) {
				icon = IDI_USER_JOIN;
				messagePart2 = TEXT(" added ");
				messagePart3 = TEXT(" to the group.");
				clickableString = TEXT("who?");
			}
			else if (clickableSF == authorId) {
				icon = IDI_USER_LEAVE;
				messagePart2 = TEXT(" left the group.");
				break;
			}
			else {
				icon = IDI_USER_LEAVE;
				messagePart2 = TEXT(" removed ");
				messagePart3 = TEXT(" from the group.");
				clickableString = TEXT("who?");
			}
			
			if (clickableSF && clickableSF != authorId) {
				std::string name = GetProfileCache()->LookupProfile(clickableSF, "", "", "", false)->GetName(guildID);
				freedStringSpace = ConvertCppStringToTString(name);
				clickableString = freedStringSpace;
			}
			break;
		}

		case MessageType::CHANNEL_NAME_CHANGE:
		{
			freedStringSpace = ConvertCppStringToTString(content);

			messagePart1 = TEXT("");
			messagePart2 = TEXT(" has changed the channel name: ");
			clickableString = freedStringSpace;
			break;
		}

		case MessageType::CHANNEL_ICON_CHANGE:
		{
			messagePart1 = TEXT("");
			messagePart2 = TmGetTString(IDS_CHANGED_GROUP_ICON);
			break;
		}

		case MessageType::GAP_UP:
		case MessageType::GAP_DOWN:
		case MessageType::GAP_AROUND:
			messagePart1 = TmGetTString(IDS_LOADING_MESSAGES);
			messagePart2 = TEXT("");
			break;

		case MessageType::CANT_VIEW_MSG_HISTORY:
			messagePart1 = TmGetTString(IDS_CANT_VIEW_MESSAGE_HISTORY);
			messagePart2 = TmGetTString(IDS_WELCOME_PERIOD);
			break;

		case MessageType::LOADING_PINNED_MESSAGES:
			messagePart1 = TmGetTString(IDS_LOADING_PINNED_MESSAGES);
			messagePart2 = TEXT("");
			break;

		case MessageType::NO_PINNED_MESSAGES:
			messagePart1 = TmGetTString(IDS_NO_PINNED_MESSAGES);
			messagePart2 = TEXT("");
			break;
	}
}

static HHOOK g_MsgBoxHookTemp = NULL;

LRESULT MessageList::OpenLinkMsgBoxHook(int code, WPARAM wParam, LPARAM lParam)
{
	if (code == HC_ACTION)
	{
		CWPRETSTRUCT* cwp = (CWPRETSTRUCT*)lParam;

		if (cwp->message == WM_INITDIALOG) {
			HWND hWnd = cwp->hwnd;

			HWND item = GetDlgItem(hWnd, IDOK);
			if (item)
				SetWindowText(item, TEXT("Yep!"));
		}
		
		LRESULT res = CallNextHookEx(g_MsgBoxHookTemp, code, wParam, lParam);

		if (cwp->message == WM_NCDESTROY) {
			UnhookWindowsHookEx(g_MsgBoxHookTemp);
			g_MsgBoxHookTemp = NULL;
		}

		return res;
	}

	return CallNextHookEx(g_MsgBoxHookTemp, code, wParam, lParam);
}

void MessageList::ConfirmOpenLink(const std::string& link)
{
	bool trust = GetLocalSettings()->CheckTrustedDomain(link);

	if (!trust)
	{
		if (g_MsgBoxHookTemp) {
			UnhookWindowsHookEx(g_MsgBoxHookTemp);
			g_MsgBoxHookTemp = NULL;
		}

		g_MsgBoxHookTemp = SetWindowsHookEx(WH_CALLWNDPROCRET, MessageList::OpenLinkMsgBoxHook, NULL, GetCurrentThreadId());

		TCHAR buffer[8192];
		LPTSTR tstr = ConvertCppStringToTString(link);
		WAsnprintf(buffer, _countof(buffer), TmGetTString(IDS_LINK_CONFIRM), tstr);
		free(tstr);

		if (MessageBox(g_Hwnd, buffer, TmGetTString(IDS_LINK_CONFIRM_TITLE), MB_ICONWARNING | MB_OKCANCEL) != IDOK)
			return;
	}

	GetDiscordInstance()->LaunchURL(link);
}

COLORREF MessageList::DrawMentionBackground(HDC hdc, RECT& rc, COLORREF chosenBkColor)
{
	COLORREF colOrig = RGB(255, 232, 96);
	COLORREF color  = LerpColor(chosenBkColor, RGB(255, 232, 96), 25, 100);
	COLORREF color2 = LerpColor(chosenBkColor, RGB(255, 232, 96), 80, 100);

	RECT rcLeft = rc;
	rcLeft.right = rcLeft.left;
	rcLeft.left -= ScaleByDPI(4);
	
	eMessageStyle ms = GetLocalSettings()->GetMessageStyle();

	if (ms == MS_GRADIENT)
	{
		FillGradientColors(hdc, &rc, colOrig, CLR_NONE, false);
		FillGradientColors(hdc, &rcLeft, color2, colOrig, false);
		// note: it's all OK because we use transparent bk mode in gradient mode
	}
	else
	{
		COLORREF old = ri::SetDCBrushColor(hdc, color);
		FillRect(hdc, &rc, GetStockBrush(DC_BRUSH));

		ri::SetDCBrushColor(hdc, color2);
		FillRect(hdc, &rcLeft, GetStockBrush(DC_BRUSH));
		ri::SetDCBrushColor(hdc, old);
	}

	return color;
}

int MessageList::DrawMessageReply(HDC hdc, MessageItem& item, RECT& rc)
{
	const int pfpOffset = ScaleByDPI(PROFILE_PICTURE_SIZE_DEF + 12);
	int replyOffset = item.m_replyHeight + ScaleByDPI(5);
	RECT rcReply = rc;
	rcReply.bottom = rcReply.top + item.m_replyHeight;

	auto& refMsg = item.m_msg.m_referencedMessage;
	bool isActionMessage = IsActionMessage(refMsg.m_type);

	Snowflake authorID = refMsg.m_author_snowflake;
	Profile* pf = nullptr;
	if (authorID)
		pf = GetProfileCache()->LookupProfile(authorID, "", "", "", false);

	// Draw the corner piece
	rcReply.left -= pfpOffset;

	int iconSize    = ScaleByDPI(32);
	int iconOffset = (GetProfilePictureSize() - ScaleByDPI(2)) / 2;

	DrawIconEx(hdc, rcReply.left + iconOffset, rcReply.bottom + ScaleByDPI(5) - iconSize, g_ReplyPieceIcon, iconSize, iconSize, 0, NULL, DI_COMPAT | DI_NORMAL);

	rcReply.left += pfpOffset;

	COLORREF nameClr = CLR_NONE;
	if (authorID)
	{
		if (!isActionMessage)
		{
			int pfpSize     = ScaleByDPI(16);
			int pfpBordSize = MulDiv(GetProfileBorderRenderSize(), ScaleByDPI(16), GetProfilePictureSize());
			int pfpBordOffX = MulDiv(ScaleByDPI(6), ScaleByDPI(16), GetProfilePictureSize());
			int pfpBordOffY = MulDiv(ScaleByDPI(4), ScaleByDPI(16), GetProfilePictureSize());
			int pfpX = rcReply.left;
			int pfpY = rcReply.top + (rcReply.bottom - rcReply.top - pfpSize) / 2;
			DrawIconEx(hdc, pfpX - pfpBordOffX, pfpY - pfpBordOffY, g_ProfileBorderIcon, pfpBordSize, pfpBordSize, 0, NULL, DI_COMPAT | DI_NORMAL);
			HBITMAP hbm = GetAvatarCache()->GetBitmap(pf->m_avatarlnk);
			DrawBitmap(hdc, hbm, pfpX, pfpY, NULL, CLR_NONE, pfpSize);
			rcReply.left += pfpSize + ScaleByDPI(2);
		}

		nameClr = GetNameColor(pf, m_guildID);
	}

	if (nameClr == CLR_NONE)
		nameClr = GetSysColor(COLOR_WINDOWTEXT);

	LPCTSTR strPart1 = TEXT("");
	LPCTSTR strPart2 = item.m_replyMsg;
	LPCTSTR strPart3 = NULL;
	LPCTSTR strClick = NULL;
	LPTSTR  strFreed = NULL;
	std::string link = "";
	Snowflake mention = 0;
	std::set<Snowflake> emptyMentions;
	int icon = 0;

	if (isActionMessage)
		DetermineMessageData(
			m_guildID,
			refMsg.m_type,
			refMsg.m_snowflake,
			refMsg.m_author_snowflake,
			0, 0, 0,
			emptyMentions,
			refMsg.m_message,
			strPart1,
			strPart2,
			strPart3,
			strClick,
			link,
			mention,
			strFreed,
			icon
		);

	if (icon) {
		int iconSize = MulDiv(ScaleByDPI(16), 8, 10); // 8/10ths is the ratio of the font too
		int iconY = rcReply.top + (rcReply.bottom - rcReply.top) / 2 - iconSize / 2;
		HICON hicon = LoadIcon(g_hInstance, MAKEINTRESOURCE(icon));
		DrawIconEx(hdc, rcReply.left, iconY, hicon, iconSize, iconSize, 0, NULL, DI_COMPAT | DI_NORMAL);
		rcReply.left += iconSize + ScaleByDPI(4);
	}

	// Draw the text
	COLORREF old = SetTextColor(hdc, GetSysColor(COLOR_GRAYTEXT));
	SelectObject(hdc, g_ReplyTextFont);
	RECT rcMeasure{};

	if (strPart1 && strPart1[0] != 0)
	{
		rcMeasure.right = (rcReply.right - rcReply.left) / 3;
		int oldRight = rcReply.right;
		rcReply.right = rcReply.left + rcMeasure.right;
		DrawText(hdc, strPart1, -1, &rcMeasure, DT_NOPREFIX | DT_SINGLELINE | DT_WORD_ELLIPSIS | DT_CALCRECT);
		DrawText(hdc, strPart1, -1, &rcReply, DT_NOPREFIX | DT_SINGLELINE | DT_WORD_ELLIPSIS);
		rcReply.left += rcMeasure.right - rcMeasure.left;
		rcReply.right = oldRight;
	}

	SetTextColor(hdc, nameClr);
	DrawText(hdc, item.m_replyAuth, -1, &rcReply, DT_NOPREFIX | DT_SINGLELINE | DT_WORD_ELLIPSIS);

	rcReply.left += item.m_replyAuthorWidth;
	if (!isActionMessage)
		rcReply.left += ScaleByDPI(5);

	SetTextColor(hdc, GetSysColor(COLOR_GRAYTEXT));

	if (STRAVAILABLE(strPart2))
	{
		SetRectEmpty(&rcMeasure);
		rcMeasure.right = rcReply.right - rcReply.left;
		DrawText(hdc, strPart2, -1, &rcMeasure, DT_NOPREFIX | DT_SINGLELINE | DT_WORD_ELLIPSIS | DT_CALCRECT);
		DrawText(hdc, strPart2, -1, &rcReply,   DT_NOPREFIX | DT_SINGLELINE | DT_WORD_ELLIPSIS);
		rcReply.left += rcMeasure.right - rcMeasure.left;
	}

	if (STRAVAILABLE(strClick))
	{
		SetRectEmpty(&rcMeasure);
		rcMeasure.right = rcReply.right - rcReply.left;
		DrawText(hdc, strClick, -1, &rcMeasure, DT_NOPREFIX | DT_SINGLELINE | DT_WORD_ELLIPSIS | DT_CALCRECT);
		DrawText(hdc, strClick, -1, &rcReply,   DT_NOPREFIX | DT_SINGLELINE | DT_WORD_ELLIPSIS);
		rcReply.left += rcMeasure.right - rcMeasure.left;
	}

	if (STRAVAILABLE(strPart3))
	{
		SetRectEmpty(&rcMeasure);
		rcMeasure.right = rcReply.right - rcReply.left;
		DrawText(hdc, strPart3, -1, &rcMeasure, DT_NOPREFIX | DT_SINGLELINE | DT_WORD_ELLIPSIS | DT_CALCRECT);
		DrawText(hdc, strPart3, -1, &rcReply,   DT_NOPREFIX | DT_SINGLELINE | DT_WORD_ELLIPSIS);
		rcReply.left += rcMeasure.right - rcMeasure.left;
	}
						
	SetTextColor(hdc, old);

	if (strFreed)
		free(strFreed);

	return replyOffset;
}

COLORREF RandColor() {
	return RGB(rand(), rand(), rand());
}

void MessageList::DrawMessage(HDC hdc, MessageItem& item, RECT& msgRect, RECT& clientRect, RECT& paintRect, DrawingContext& mddc, COLORREF chosenBkColor)
{
	LPCTSTR strAuth = item.m_author;
	LPCTSTR strDate = item.m_date;
	LPCTSTR strEditDate = item.m_dateEdited;
	int height = item.m_height, authheight = item.m_authHeight, authwidth = item.m_authWidth;

	RECT rc = msgRect;
	if (!m_firstShownMessage && rc.bottom > clientRect.top && !item.m_msg.IsLoadGap())
		m_firstShownMessage = item.m_msg.m_snowflake;

	const int pfpOffset = ScaleByDPI(PROFILE_PICTURE_SIZE_DEF + 12);

	eMessageStyle mStyle = GetLocalSettings()->GetMessageStyle();

	rc.left   += ScaleByDPI(10);
	rc.top    += ScaleByDPI(10);
	rc.right  -= ScaleByDPI(10);
	rc.bottom -= ScaleByDPI(10);

	if (!m_bManagedByOwner && item.m_msg.m_bIsDateGap)
	{
		RECT dgRect = msgRect;
		dgRect.bottom = dgRect.top + DATE_GAP_HEIGHT;

		rc.top += DATE_GAP_HEIGHT;
		msgRect.top += DATE_GAP_HEIGHT;

		LPTSTR strDateGap = ConvertCppStringToTString("  " + item.m_msg.m_dateOnly + "  ");
		COLORREF oldTextClr = SetTextColor(hdc, GetSysColor(COLOR_GRAYTEXT));
		COLORREF oldPenClr = ri::SetDCPenColor(hdc, GetSysColor(COLOR_GRAYTEXT));

		auto clr = COLOR_3DFACE;
		if (mStyle == MS_FLATBR) clr = COLOR_WINDOW;
		else if (mStyle == MS_GRADIENT) {
			COLORREF c1, c2;
			c1 = GetSysColor(COLOR_WINDOW); // high
			c2 = GetSysColor(COLOR_3DFACE); // low
			if (c1 < c2) clr = COLOR_3DFACE;
			else         clr = COLOR_WINDOW;
		}

		FillRect(hdc, &dgRect, GetSysColorBrush(clr));

		RECT rcMeasure = dgRect;
		DrawText(hdc, strDateGap, -1, &rcMeasure, DT_CALCRECT | DT_NOPREFIX);
		int width = rcMeasure.right - rcMeasure.left;

		int l1 = dgRect.left + ScaleByDPI(10);
		int r1 = dgRect.right / 2 - width / 2 - 1;
		int l2 = dgRect.right / 2 + width / 2 + 1;
		int r2 = dgRect.right - ScaleByDPI(10) - 1;

		POINT old;
		HGDIOBJ oldob = SelectObject(hdc, GetStockPen(DC_PEN));
		MoveToEx(hdc, l1, (dgRect.top + dgRect.bottom) / 2, &old);
		LineTo  (hdc, r1, (dgRect.top + dgRect.bottom) / 2);
		MoveToEx(hdc, l2, (dgRect.top + dgRect.bottom) / 2, NULL);
		LineTo  (hdc, r2, (dgRect.top + dgRect.bottom) / 2);
		MoveToEx(hdc, old.x, old.y, NULL);
		SelectObject(hdc, oldob);

		DrawText(hdc, strDateGap, -1, &dgRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
		free(strDateGap);
		SetTextColor(hdc, oldTextClr);
		ri::SetDCPenColor(hdc, oldPenClr);
	}

	switch (mStyle)
	{
		case MS_3DFACE:
			DrawEdge(hdc, &msgRect, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_RECT | BF_MIDDLE);
			break;
		case MS_FLAT:
			FillRect(hdc, &msgRect, GetSysColorBrush(COLOR_3DFACE));
			break;
		case MS_FLATBR:
			FillRect(hdc, &msgRect, GetSysColorBrush(COLOR_WINDOW));
			break;
		case MS_GRADIENT:
		{
			COLORREF c1, c2;
			c1 = GetSysColor(COLOR_WINDOW); // high
			c2 = GetSysColor(COLOR_3DFACE); // low

			if (c1 < c2) {
				COLORREF tmp = c1;
				c1 = c2;
				c2 = tmp;
			}

			TRIVERTEX vt[2] = { {}, {} };
			vt[0].x = msgRect.left;
			vt[0].y = msgRect.top;
			vt[1].x = msgRect.right;
			vt[1].y = msgRect.bottom;
			vt[0].Red   = ( c1 & 0xFF) << 8;
			vt[0].Green = ((c1 >> 8) & 0xFF) << 8;
			vt[0].Blue  = ((c1 >> 16) & 0xFF) << 8;
			vt[0].Alpha = 0;
			vt[1].Red   = int( c2 & 0xFF) << 8;
			vt[1].Green = int((c2 >> 8) & 0xFF) << 8;
			vt[1].Blue  = int((c2 >> 16) & 0xFF) << 8;
			vt[1].Alpha = 0;

			GRADIENT_RECT grect;
			grect.UpperLeft = 0;
			grect.LowerRight = 1;

			ri::GradientFill(hdc, vt, 2, &grect, 1, GRADIENT_FILL_RECT_V);
			break;
		}
	}

	RECT intersRect{};
	bool inView = IntersectRect(&intersRect, &rc, &paintRect); //rc.bottom < clientRect.top;

	bool bIsCompact = IsCompact();
	bool isActionMessage = false;
	LPCTSTR actionMsgPart1 = NULL;
	LPCTSTR actionMsgPart2 = NULL;
	LPCTSTR actionMsgPart3 = NULL;
	LPCTSTR actionMsgClick = NULL;
	LPTSTR  freedString = NULL; // this string is freed when exiting
	std::string actionMsgLink = "";
	Snowflake actionMsgSF = 0;
	int icon = 0;

	isActionMessage = IsActionMessage(item.m_msg.m_type);

	if (isActionMessage)
		DetermineMessageData(
			m_guildID,
			item.m_msg.m_type,
			item.m_msg.m_snowflake,
			item.m_msg.m_author_snowflake,
			item.m_msg.m_refMessageGuild,
			item.m_msg.m_refMessageChannel,
			item.m_msg.m_refMessageSnowflake,
			item.m_msg.m_userMentions,
			item.m_msg.m_message,
			actionMsgPart1,
			actionMsgPart2,
			actionMsgPart3,
			actionMsgClick,
			actionMsgLink,
			actionMsgSF,
			freedString,
			icon);

	if (!bIsCompact && !isActionMessage)
		rc.left += pfpOffset;

	int authorOffset = 0;
	int dateOffset = 0;
	int sizePart2 = 0, sizeClick = 0, sizePart3 = 0;
	int actionIconSize = ScaleByDPI(16);

	if (isActionMessage)
	{
		SelectObject(hdc, g_MessageTextFont);
		RECT rca = rc;
		RECT rcMeasure;
		rca.right -= ScaleByDPI(75); // TODO: Figure out why I have to do this.
		rcMeasure = rca;
		DrawText(hdc, actionMsgPart1, -1, &rcMeasure, DT_CALCRECT);
		authorOffset = rcMeasure.right - rcMeasure.left + 1;
		if (icon) {
			authorOffset += actionIconSize + ScaleByDPI(4);
			rca.left += authorOffset;
		}

		if (STRAVAILABLE(actionMsgPart2)) {
			rcMeasure = rca;
			DrawText(hdc, actionMsgPart2, -1, &rcMeasure, DT_CALCRECT | DT_SINGLELINE | DT_WORD_ELLIPSIS);
			sizePart2 = rcMeasure.right - rcMeasure.left + 1;
			rca.left += sizePart2;
		}
		if (STRAVAILABLE(actionMsgClick)) {
			HGDIOBJ objold = SelectObject(hdc, g_AuthorTextFont);
			rcMeasure = rca;
			DrawText(hdc, actionMsgClick, -1, &rcMeasure, DT_CALCRECT | DT_SINGLELINE | DT_WORD_ELLIPSIS);
			sizeClick = rcMeasure.right - rcMeasure.left + 1;
			rca.left += sizeClick;
			SelectObject(hdc, objold);
		}
		if (STRAVAILABLE(actionMsgPart3)) {
			rcMeasure = rca;
			DrawText(hdc, actionMsgPart3, -1, &rcMeasure, DT_CALCRECT | DT_SINGLELINE | DT_WORD_ELLIPSIS);
			sizePart3 = rcMeasure.right - rcMeasure.left + 1;
			rca.left += sizePart3;
		}

		dateOffset = sizePart2 + sizePart3 + sizeClick + authorOffset;
	}
	else if (bIsCompact)
	{
		authorOffset = item.m_dateWidth;
	}

	// draw the reply if needed
	int replyOffset = 0;

	if (item.m_msg.m_bHaveReferencedMessage && !isActionMessage)
		replyOffset = DrawMessageReply(hdc, item, rc);

	rc.top += replyOffset;

	// draw the author text
	SelectObject(hdc, item.m_authorHighlighted ? g_AuthorTextUnderlinedFont : g_AuthorTextFont);

	RECT rcb = rc;
	DrawText(hdc, strAuth, -1, &rc, DT_CALCRECT);
	int auth_wid = rc.right - rc.left;
	int auth_hei = rc.bottom - rc.top;
	rc = rcb;

	COLORREF old = CLR_NONE;
	if (inView) {
		if (item.m_authorHighlighted) {
			old = SetTextColor(hdc, RGB(0, 0, 255));
		}
		else {
			// figure out the color of the author
			Profile* pf = GetProfileCache()->LookupProfile(item.m_msg.m_author_snowflake, "", "", "", false);
			COLORREF cr = GetNameColor(pf, m_guildID);

			if (cr != CLR_NONE)
				old = SetTextColor(hdc, cr);
		}
	}

	rc.left += authorOffset;
	rc.right += authorOffset;
	if (inView)
		DrawText(hdc, strAuth, -1, &rc, DT_NOPREFIX | DT_NOCLIP);

	if (old != CLR_NONE) {
		SetTextColor(hdc, old);
	}

	item.m_authorRect = rc;
	item.m_authorRect.bottom = item.m_authorRect.top + auth_hei;
	item.m_authorRect.right = item.m_authorRect.left + auth_wid;

	// draw the date text
	SelectObject(hdc, g_DateTextFont);

	int oldLeft = rc.left;
	if (inView) {
		if (!bIsCompact || isActionMessage)
			rc.left += authwidth + ScaleByDPI(10);
		rc.left += dateOffset;
		rc.left -= authorOffset;
		DrawText(hdc, strDate, -1, &rc, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP);
		if (item.m_msg.m_timeEdited) {
			RECT rcMeasure{};
			DrawText(hdc, strDate, -1, &rcMeasure, DT_SINGLELINE | DT_NOPREFIX | DT_CALCRECT);
			rc.left += rcMeasure.right - rcMeasure.left + ScaleByDPI(5);
			DrawText(hdc, strEditDate, -1, &rc, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP);
			rc.left -= rcMeasure.right - rcMeasure.left + ScaleByDPI(5);
		}
	}
	rc.left = oldLeft;

	rc.left -= authorOffset;
	rc.right -= authorOffset;

	// draw the message text
	SelectObject(hdc, g_MessageTextFont);

	if (!bIsCompact)
		rc.top += authheight;

	if (isActionMessage)
	{
		item.m_avatarRect = { -10000, -10000, -9999, -9999 };

		RECT rca = rc;
		if (!bIsCompact)
			rca.top -= authheight;

		int offs = 0;
		if (icon) {
			int iconY = rca.top + (rca.bottom - rca.top) / 2 - actionIconSize / 2;
			HICON hicon = LoadIcon(g_hInstance, MAKEINTRESOURCE(icon));
			DrawIconEx(hdc, rca.left, iconY, hicon, actionIconSize, actionIconSize, 0, NULL, DI_COMPAT | DI_NORMAL);
			offs = actionIconSize + ScaleByDPI(4);
		}

		rca.right = rca.left + authorOffset;
		rca.left += offs;
		if (inView) DrawText(hdc, actionMsgPart1, -1, &rca, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_WORD_ELLIPSIS);
		rca.left -= offs;
		rca.left += authorOffset + authwidth + 1;
		rca.right = rca.left + authorOffset + authwidth + dateOffset + 1;
			
		if (actionMsgPart2 && actionMsgPart2[0] != 0) {
			if (inView) {
				int oldright = rca.right;
				rca.right = rca.left + sizePart2;
				DrawText(hdc, actionMsgPart2, -1, &rca, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_WORD_ELLIPSIS);
				rca.right = oldright;
			}
			rca.left += sizePart2;
		}

		if (STRAVAILABLE(actionMsgClick))
		{
			assert(item.m_interactableData.size() == 1);
			InteractableItem& iitem = item.m_interactableData[0];

			if (actionMsgSF) {
				iitem.m_type = InteractableItem::MENTION;
				iitem.m_affected = actionMsgSF;
				iitem.m_destination = "@0";
			}
			else if (!actionMsgLink.empty()) {
				iitem.m_type = InteractableItem::LINK;
				iitem.m_destination = actionMsgLink;
			}
			else {
				iitem.m_type = InteractableItem::NONE;
				SetRectEmpty(&iitem.m_rect);
			}

			RECT rcClickable = rca;
			rcClickable.right = rcClickable.left + sizeClick;
			rca.left += sizeClick;
			iitem.m_rect = rcClickable;

			if (inView) {
				HGDIOBJ objold = SelectObject(hdc, g_AuthorTextFont);
				DrawText(hdc, actionMsgClick, -1, &rcClickable, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_WORD_ELLIPSIS);
				SelectObject(hdc, objold);
			}

			if (iitem.m_bHighlighted && inView) {
				POINT old{};
				HGDIOBJ  oldPen    = SelectObject(hdc, GetStockPen(DC_PEN));
				COLORREF oldPenClr = ri::SetDCPenColor(hdc, GetTextColor(hdc));
				MoveToEx(hdc, iitem.m_rect.left,  iitem.m_rect.bottom - 1, &old);
				LineTo  (hdc, iitem.m_rect.right, iitem.m_rect.bottom - 1);
				MoveToEx(hdc, old.x, old.y, NULL);
				ri::SetDCPenColor(hdc, oldPenClr);
				SelectObject(hdc, oldPen);
			}
		}

		if (STRAVAILABLE(actionMsgPart3)) {
			if (inView) {
				int oldright = rca.right;
				rca.right = rca.left + sizePart3;
				DrawText(hdc, actionMsgPart3, -1, &rca, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_WORD_ELLIPSIS);
				rca.right = oldright;
			}
			rca.left += sizePart3;
		}
	}
	else
	{
		if (!bIsCompact)
			rc.top += ScaleByDPI(5);

		RECT rcMsg = rc;
		rcMsg.bottom -= item.m_attachHeight + item.m_embedHeight;
		item.m_message.Layout(&mddc, Rect(W32RECT(rcMsg)), bIsCompact ? (item.m_authWidth + item.m_dateWidth + ScaleByDPI(10)) : 0);
						
		Rect ext = item.m_message.GetExtent();
		int height = std::max(item.m_authHeight, ext.Height());
		int offsetY = ((rcMsg.bottom - rcMsg.top) - ext.Height()) / 2;

		// Once laid out, also update the rectangles for the interactable items.
		const auto& words = item.m_message.GetWords();
		for (size_t i = 0; i < item.m_interactableData.size(); i++) {
			InteractableItem& iitem = item.m_interactableData[i];
			if (!iitem.TypeUpdatedFromWords())
				continue;
			const Word& word = words[iitem.m_wordIndex];
			iitem.m_rect = RectToNative(word.m_rect);
			iitem.m_rect.top += offsetY;
			iitem.m_rect.bottom += offsetY;
		}

		if (inView) {
			COLORREF oldBkColor = CLR_NONE;
			COLORREF oldTextColor = CLR_NONE;
			if (item.m_bWasMentioned) {
				COLORREF color;
				if (!bIsCompact) {
					color = DrawMentionBackground(hdc, rc, chosenBkColor);
				}
				else {
					RECT rc1, rc2;
					rc1 = rc2 = rc;

					rc1.left += item.m_authWidth + item.m_dateWidth + ScaleByDPI(10);
					rc1.bottom = rc1.top + item.m_authHeight;
					rc2.top += item.m_authHeight;

					color = DrawMentionBackground(hdc, rc1, chosenBkColor);

					if (rc2.bottom > rc2.top)
						DrawMentionBackground(hdc, rc2, chosenBkColor);
				}

				oldBkColor = SetBkColor(hdc, color);
				mddc.SetBackgroundColor(color);
			}
			else {
				mddc.SetBackgroundColor(chosenBkColor);
			}

			if (item.m_msg.m_type == MessageType::UNSENT_MESSAGE)
				oldTextColor = SetTextColor(hdc, RGB(255, 0, 0));
			
			item.m_message.Draw(&mddc, offsetY);

			if (oldBkColor != CLR_NONE)
				SetBkColor(hdc, oldBkColor);
			if (oldTextColor != CLR_NONE)
				SetTextColor(hdc, oldTextColor);

			COLORREF windowTextColor = GetSysColor(COLOR_WINDOWTEXT);
			for (size_t i = 0; i < item.m_interactableData.size(); i++) {
				InteractableItem& iitem = item.m_interactableData[i];

				if (iitem.m_bHighlighted)
				{
					POINT old{};
					HGDIOBJ oldObj = SelectObject(hdc, GetStockPen(DC_PEN));
					COLORREF oldPenClr = ri::SetDCPenColor(hdc, iitem.UseLinkColor()  ? COLOR_LINK : windowTextColor);
					MoveToEx(hdc, iitem.m_rect.left, iitem.m_rect.bottom - 1, &old);
					LineTo  (hdc, iitem.m_rect.right, iitem.m_rect.bottom - 1);
					MoveToEx(hdc, old.x, old.y, NULL);
					ri::SetDCPenColor(hdc, oldPenClr);
					SelectObject(hdc, oldObj);
				}
			}
		}

		item.m_messageRect = RectToNative(ext);

		if (bIsCompact)
			item.m_messageRect.bottom = std::max(item.m_messageRect.bottom, item.m_messageRect.top + item.m_authHeight);

		assert(ext.left != 0);

		if (!bIsCompact)
		{
			rc.left -= pfpOffset;

			int offs1 = ScaleByDPI(6), offs2 = ScaleByDPI(4), offs3 = offs1 + offs2;
			int offs4 = replyOffset;
			int szScaled = GetProfileBorderRenderSize();

			RECT pfRect = {
				msgRect.left + offs3,
				msgRect.top  + offs3 + offs4,
				msgRect.left + offs3 + GetProfilePictureSize(),
				msgRect.top  + offs3 + offs4 + GetProfilePictureSize()
			};
			item.m_avatarRect = pfRect;

			DrawIconEx(hdc, pfRect.left - offs1, pfRect.top - offs2, g_ProfileBorderIcon, szScaled, szScaled, 0, NULL, DI_NORMAL | DI_COMPAT);
			if (inView)
			{
				// draw the avatar
				Profile* pf = GetProfileCache()->LookupProfile(item.m_msg.m_author_snowflake, "", "", "", false);
				HBITMAP hbm = GetAvatarCache()->GetBitmap(pf->m_avatarlnk);
				DrawBitmap(hdc, hbm, pfRect.left, pfRect.top, &pfRect, CLR_NONE, GetProfilePictureSize());
			}
		}
		else
		{
			item.m_avatarRect = { -10000, -10000, -9999, -9999 };
		}
	}

	// draw available embeds, if any:
	RECT embedRect = item.m_messageRect;
	embedRect.bottom += ScaleByDPI(5);
	embedRect.right = msgRect.right - ScaleByDPI(10);
	embedRect.top = embedRect.bottom;

	auto& embedVec = item.m_embedData;
	size_t sz = embedVec.size();
	for (size_t i = 0; i < sz; i++)
	{
		auto& eitem = embedVec[i];

		if (eitem.m_size.cx == 0 || eitem.m_size.cy == 0)
			eitem.Measure(hdc, embedRect, bIsCompact);

		eitem.Draw(hdc, embedRect);

		for (auto& ii : item.m_interactableData) {
			if (ii.m_wordIndex - InteractableItem::EMBED_OFFSET != i)
				continue;
			if (ii.m_type == InteractableItem::EMBED_LINK) {
				switch (ii.m_placeInEmbed) {
					case EMBED_IN_TITLE:    ii.m_rect = eitem.m_titleRect;    break;
					case EMBED_IN_AUTHOR:   ii.m_rect = eitem.m_authorRect;   break;
					case EMBED_IN_PROVIDER: ii.m_rect = eitem.m_providerRect; break;
				}
			}
			if ((eitem.m_pEmbed->m_bHasImage || eitem.m_pEmbed->m_bHasThumbnail) && ii.m_type == InteractableItem::EMBED_IMAGE) {
				if (eitem.m_pEmbed->m_bHasImage)
					ii.m_rect = eitem.m_imageRect;
				else
					ii.m_rect = eitem.m_thumbnailRect;
			}
		}

		embedRect.top += eitem.m_size.cy + ScaleByDPI(5);
		embedRect.bottom = embedRect.top;
	}

	// draw available attachments, if any:
	RECT attachRect    = embedRect;
	attachRect.bottom += ScaleByDPI(5);
	attachRect.right   = msgRect.right - ScaleByDPI(10);
	attachRect.top     = attachRect.bottom;
	attachRect.bottom += ATTACHMENT_GAP;

	if (bIsCompact) attachRect.left = rc.left;

	auto& attachVec = item.m_msg.m_attachments;
	auto& attachItemVec = item.m_attachmentData;
	sz = attachVec.size();
	for (size_t i = 0; i < sz; i++)
	{
		auto& attach = attachVec[i];
		auto& attachItem = attachItemVec[i];
		switch (attach.m_contentType)
		{
			case ContentType::PNG:
			case ContentType::GIF:
			case ContentType::JPEG:
			{
				DrawImageAttachment(hdc, paintRect, attachItem, attachRect);
				break;
			}
			default:
			{
				DrawDefaultAttachment(hdc, paintRect, attachItem, attachRect);
				break;
			}
		}
	}

	bool isGap =
			item.m_msg.m_type == MessageType::GAP_DOWN
		|| item.m_msg.m_type == MessageType::GAP_UP
		|| item.m_msg.m_type == MessageType::GAP_AROUND;
	if (inView && isGap)
	{
		DbgPrintW("Loading gap message ID %lld", item.m_msg.m_anchor);

		ScrollDir::eScrollDir sd;
		switch (item.m_msg.m_type) {
			default:
			case MessageType::GAP_AROUND: sd = ScrollDir::AROUND; break;
			case MessageType::GAP_UP:     sd = ScrollDir::BEFORE; break;
			case MessageType::GAP_DOWN:   sd = ScrollDir::AFTER; break;
		}

		// gap message - load
		GetDiscordInstance()->RequestMessages(
			m_channelID,
			sd,
			item.m_msg.m_anchor,
			item.m_msg.m_snowflake
		);
	}

	if (inView)
	{
		Channel* pChan = GetDiscordInstance()->GetChannel(m_channelID);

		if (pChan && pChan->m_lastSentMsg == item.m_msg.m_snowflake && pChan->m_lastSentMsg != pChan->m_lastViewedMsg)
		{
			DbgPrintW("Acknowledging unread messages");
			GetDiscordInstance()->RequestAcknowledgeChannel(m_channelID);
		}
	}

	free((void*) freedString);
}

#include <ShellAPI.h>

LRESULT CALLBACK MessageList::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	MessageList* pThis = (MessageList*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	SCROLLINFO si;
	switch (uMsg)
	{
		case WM_NCCREATE:
		{
			CREATESTRUCT* strct = (CREATESTRUCT*)lParam;

			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)strct->lpCreateParams);

			break;
		}
		case WM_DROPFILES:
		{
			TCHAR chr[4096];
			HDROP hDrop = (HDROP)wParam;

			int amount = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);

			if (amount != 1)
			{
				MessageBox(g_Hwnd, TEXT("I currently only take one file at a time!"), TmGetTString(IDS_PROGRAM_NAME), MB_ICONWARNING | MB_OK);
				DragFinish(hDrop);
				return 0;
			}

			DragQueryFile(hDrop, 0, chr, _countof(chr));
			DbgPrintW("File dropped: %S", chr);

			if (!*chr) {
				DbgPrintW("No file name!");
				DragFinish(hDrop);
				return 0;
			}

			LPTSTR fileName = chr;
			LPTSTR fileTitle = chr;
			LPTSTR temp = chr;
			while (*temp)
			{
				if (*temp == '\\' || *temp == '/')
					fileTitle = temp + 1;

				temp++;
			}

			if (!*fileTitle) {
				DbgPrintW("No file title!  File path was %S", fileName);
				DragFinish(hDrop);
				return 0;
			}
			
			UploadDialogShowWithFileName(fileName, fileTitle);
			DragFinish(hDrop);
			return 0;
		}
		case WM_SIZE:
		{
			int oldWidth  = pThis->m_oldWidth;
			int oldHeight = pThis->m_oldHeight;
			int newWidth  = GET_X_LPARAM(lParam);
			int newHeight = GET_Y_LPARAM(lParam);
			pThis->m_oldWidth  = newWidth;
			pThis->m_oldHeight = newHeight;

			if (oldWidth != -1)
			{
				if (oldWidth != GET_X_LPARAM(lParam))
					pThis->RecalcMessageSizes();
				else
					pThis->UpdateScrollBar(0, oldHeight - newHeight, false, true);
				return 0;
			}

			break;
		}
		case WM_MOUSEWHEEL:
		{
			short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);

			si.cbSize = sizeof(si);
			si.fMask = SIF_TRACKPOS | SIF_POS | SIF_RANGE;
			GetScrollInfo(pThis->m_scroll_hwnd, SB_CTL, &si);
			si.nTrackPos = si.nPos - (zDelta / 3);
			if (si.nTrackPos < si.nMin) si.nTrackPos = si.nMin;
			if (si.nTrackPos > si.nMax) si.nTrackPos = si.nMax;
			SetScrollInfo(pThis->m_scroll_hwnd, SB_CTL, &si, false);

			wParam = SB_THUMBTRACK;
			goto _lbl;
			break;
		}
		case WM_VSCROLL:
		{
			si.cbSize = sizeof(si);
			si.fMask = SIF_ALL;

			GetScrollInfo(pThis->m_scroll_hwnd, SB_CTL, &si);

		_lbl:
			;
			int diffUpDown = 0;

			RECT rect;
			GetClientRect(hWnd, &rect);

			int scrollBarWidth = GetSystemMetrics(SM_CXVSCROLL);

			TEXTMETRIC tm;
			HDC hdc = GetDC(hWnd);
			GetTextMetrics(hdc, &tm);
			ReleaseDC(hWnd, hdc);

			int yChar = tm.tmHeight + tm.tmExternalLeading;

			switch (LOWORD(wParam))
			{
				case SB_ENDSCROLL:
					// nPos is correct
					break;
				case SB_THUMBTRACK:
					si.nPos = si.nTrackPos;
					break;
				case SB_LINEUP:
					si.nPos -= yChar;
					break;
				case SB_LINEDOWN:
					si.nPos += yChar;
					break;
				case SB_PAGEUP:
					si.nPos -= si.nPage;
					break;
				case SB_PAGEDOWN:
					si.nPos += si.nPage;
					break;
				case SB_TOP:
					si.nPos = si.nMin;
					break;
				case SB_BOTTOM:
					si.nPos = si.nMax;
					break;
			}

			SetScrollInfo(pThis->m_scroll_hwnd, SB_CTL, &si, true);
			GetScrollInfo(pThis->m_scroll_hwnd, SB_CTL, &si);

			diffUpDown = si.nPos - pThis->m_oldPos;
			pThis->m_oldPos = si.nPos;

			WindowScroll(hWnd, diffUpDown);

			break;
		}
		case WM_CONTEXTMENU:
		{
			int xPos = GET_X_LPARAM(lParam);
			int yPos = GET_Y_LPARAM(lParam);

			RECT rect = {};
			GetClientRect(hWnd, &rect);
			RECT rectParent = {};
			GetClientRect(pThis->m_hwnd, &rectParent);
			RECT rectParent2 = {};
			GetClientRect(g_Hwnd, &rectParent2);

			POINT pt;
			pt.x = xPos;
			pt.y = yPos;
			ScreenToClient(hWnd, &pt);

			// user pressed Shift+F10
			if (xPos == -1) break;

			// Which message did we right-click?
			MessageItem* pRCMsg = NULL;

			for (std::list<MessageItem>::iterator iter = pThis->m_messages.begin();
				iter != pThis->m_messages.end();
				iter++)
			{
				if (PtInRect(&iter->m_rect, pt))
				{
					pRCMsg = &(*iter);
					break;
				}
			}

			if (!pRCMsg) break;

			HMENU menu = GetSubMenu(LoadMenu(g_hInstance, MAKEINTRESOURCE(IDR_MESSAGE_CONTEXT)), 0);

			pThis->m_rightClickedMessage = pRCMsg->m_msg.m_snowflake;

			// TODO: Why does this require this hackery yet the guild lister context menu doesn't?
			MENUINFO cmi{};
			cmi.cbSize = sizeof cmi;
			cmi.fMask  = MIM_STYLE;
			ri::GetMenuInfo(menu, &cmi);
			cmi.dwStyle   |= MNS_NOTIFYBYPOS;
			ri::SetMenuInfo(menu, &cmi);

			// disable the Delete button if we're not the user
			Profile* ourPf = GetDiscordInstance()->GetProfile();
			Channel* pChan = GetDiscordInstance()->GetCurrentChannel();

			if (!pChan) break;

			bool isThisMyMessage   = pRCMsg->m_msg.m_author_snowflake == ourPf->m_snowflake;
			bool mayManageMessages = pChan->HasPermission(PERM_MANAGE_MESSAGES);

			bool mayDelete = isThisMyMessage || mayManageMessages;
			bool mayEdit   = isThisMyMessage;
			bool mayPin    = mayManageMessages;

#ifdef OLD_WINDOWS
			EnableMenuItem(menu, ID_DUMMYPOPUP_DELETEMESSAGE, mayDelete ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(menu, ID_DUMMYPOPUP_EDITMESSAGE,   mayEdit   ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(menu, ID_DUMMYPOPUP_PINMESSAGE,    mayPin    ? MF_ENABLED : MF_GRAYED);
#else
			if (!mayDelete) RemoveMenu(menu, ID_DUMMYPOPUP_DELETEMESSAGE, MF_BYCOMMAND);
			if (!mayEdit)   RemoveMenu(menu, ID_DUMMYPOPUP_EDITMESSAGE,   MF_BYCOMMAND);
			if (!mayPin)    RemoveMenu(menu, ID_DUMMYPOPUP_PINMESSAGE,    MF_BYCOMMAND);
#endif

			TrackPopupMenu(menu, TPM_RIGHTBUTTON, xPos, yPos, 0, hWnd, NULL);
			break;
		}
		case WM_MENUCOMMAND:
		{
			// NOTE!!! If you block for any reason (e.g. using a messagebox/dialog), you need to re-fetch this.
			// Otherwise a message may come in the background and delete the message you are using, among loads
			// of other things.
			MessageItem* pMsg = NULL;
			Snowflake rightClickedMessage = pThis->m_rightClickedMessage;

			for (auto iter = pThis->m_messages.rbegin(); iter != pThis->m_messages.rend(); ++iter)
			{
				if (iter->m_msg.m_snowflake == pThis->m_rightClickedMessage)
				{
					pMsg = &(*iter);
					break;
				}
			}

			pThis->m_rightClickedMessage = 0;

			if (!pMsg) break;

			int test = GetMenuItemID((HMENU)lParam, wParam);

			switch (test)
			{
				case ID_DUMMYPOPUP_COPYMESSAGELINK:
				{
					// Copy the message link.
					std::string msgLink = CreateMessageLink(pThis->m_guildID, pThis->m_channelID, pMsg->m_msg.m_snowflake);
					CopyStringToClipboard(msgLink);
					break;
				}
				case ID_DUMMYPOPUP_EDITMESSAGE:
				{
					SendMessage(g_Hwnd, WM_STARTEDITING, 0, (LPARAM) &rightClickedMessage);
					break;
				}
				case ID_DUMMYPOPUP_COPYTEXT:
				{
					CopyStringToClipboard(pMsg->m_msg.m_message);
					break;
				}
				case ID_DUMMYPOPUP_COPYID:
				{
					std::string msgID = std::to_string(rightClickedMessage);
					CopyStringToClipboard(msgID);
					break;
				}
				case ID_DUMMYPOPUP_DELETEMESSAGE:
				{
					static char buffer[8192];
					snprintf(buffer, sizeof buffer, TmGetString(IDS_CONFIRM_DELETE).c_str(), pMsg->m_msg.m_author.c_str(), pMsg->m_msg.m_dateFull.c_str(), pMsg->m_msg.m_message.c_str());
					LPCTSTR xstr = ConvertCppStringToTString(buffer);
					if (MessageBox(g_Hwnd, xstr, TmGetTString(IDS_CONFIRM_DELETE_TITLE), MB_YESNO | MB_ICONQUESTION) == IDYES)
					{
						GetDiscordInstance()->RequestDeleteMessage(pThis->m_channelID, rightClickedMessage);
					}

					free((void*)xstr);
					break;
				}
				case ID_DUMMYPOPUP_REPLY:
				{
					Snowflake sf[2];
					sf[0] = pMsg->m_msg.m_snowflake;
					sf[1] = pMsg->m_msg.m_author_snowflake;
					SendMessage(g_Hwnd, WM_STARTREPLY, 0, (LPARAM)sf);
					break;
				}
				case ID_DUMMYPOPUP_PINMESSAGE:
				{
					Channel* pChan = GetDiscordInstance()->GetCurrentChannel();
					if (!pChan)
						break;

					static char buffer[8192];
					snprintf(buffer, sizeof buffer, TmGetString(IDS_CONFIRM_PIN).c_str(), pChan->m_name.c_str(), pMsg->m_msg.m_author.c_str(), pMsg->m_msg.m_dateFull.c_str(), pMsg->m_msg.m_message.c_str());

					LPCTSTR xstr = ConvertCppStringToTString(buffer);

					if (MessageBox(g_Hwnd, xstr, TmGetTString(IDS_CONFIRM_PIN_TITLE), MB_YESNO | MB_ICONQUESTION) == IDYES)
					{

					}

					free((void*)xstr);
					break;
				}
				case ID_DUMMYPOPUP_SPEAKMESSAGE:
				{
					TextToSpeech::Speak(pMsg->m_msg.m_author + " said " + pMsg->m_msg.m_message);
					break;
				}
			}
			break;
		}
		case WM_LBUTTONDOWN:
		{
			SetFocus(hWnd);

			POINT pt = { GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam) };
			auto msg = pThis->FindMessageByPoint(pt);
			if (msg == pThis->m_messages.end())
				break;

			if (pThis->m_bManagedByOwner)
			{
				Snowflake sf = msg->m_msg.m_snowflake;
				SendMessage(GetParent(pThis->m_hwnd), WM_CLICKEDMESSAGE, 0, (LPARAM) &sf);
			}
			else
			{
				if (msg->m_msg.m_type != MessageType::GAP_UP &&
					msg->m_msg.m_type != MessageType::GAP_DOWN &&
					msg->m_msg.m_type != MessageType::GAP_AROUND &&
					msg->m_msg.m_author_snowflake != 0 &&
					(PtInRect(&msg->m_authorRect, pt) || PtInRect(&msg->m_avatarRect, pt)))
				{
					Snowflake authorSF = msg->m_msg.m_author_snowflake;

					RECT rect;
					GetWindowRect(hWnd, &rect);

					ShowProfilePopout(authorSF, pThis->m_guildID, rect.left + msg->m_authorRect.right + 10, rect.top + msg->m_authorRect.top);
				}

				for (auto& att : msg->m_attachmentData)
				{
					if (PtInRect(&att.m_textRect, pt) || PtInRect(&att.m_addRect, pt)) {
						pThis->OpenAttachment(&att);
						break;
					}
				}

				for (auto& intd : msg->m_interactableData)
				{
					if (PtInRect(&intd.m_rect, pt)) {
						pThis->OpenInteractable(&intd);
						break;
					}
				}
			}

			break;
		}
		case WM_MSGLIST_PULLDOWN:
		{
			RECT messageRect = *((PRECT)lParam);

			int height = messageRect.bottom - messageRect.top;

			// totalRect represents the total area between the top of the area and
			// the message itself.
			RECT totalRect = messageRect;
			totalRect.top = 0;

			// area to pull down from
			RECT pullDownRect = totalRect;
			pullDownRect.bottom = pullDownRect.top + messageRect.top;

			// area to refresh
			RECT refreshRect = totalRect;
			refreshRect.bottom = pullDownRect.top + height;

			if (pThis->m_bManagedByOwner)
				break;

			// do a bitblt to "pull down" the region
			HDC hdc = GetDC(hWnd);
			BitBlt(
				hdc,
				pullDownRect.left,
				pullDownRect.top + height,
				pullDownRect.right - pullDownRect.left,
				pullDownRect.bottom - pullDownRect.top,
				hdc,
				pullDownRect.left,
				pullDownRect.top,
				SRCCOPY
			);
			ReleaseDC(hWnd, hdc);
			InvalidateRect(hWnd, &refreshRect, false);
			break;
		}
		case WM_MOUSEMOVE:
		{
			POINT pt = { GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam) };
			BOOL hit = FALSE;
			pThis->HitTestAuthor(pt, hit);
			pThis->HitTestAttachments(pt, hit);
			pThis->HitTestInteractables(pt, hit);
			TRACKMOUSEEVENT tme;
			tme.cbSize = sizeof tme;
			tme.dwFlags = TME_LEAVE;
			tme.hwndTrack = hWnd;
			tme.dwHoverTime = 0;
			_TrackMouseEvent(&tme);
			if (hit)
				return TRUE;
			break;
		}
		case WM_MOUSELEAVE:
		{
			POINT pt = { -1, -1 };
			BOOL hitunused = TRUE;
			pThis->HitTestAuthor(pt, hitunused);
			pThis->HitTestAttachments(pt, hitunused);
			pThis->HitTestInteractables(pt, hitunused);
			break;
		}
		case WM_PAINT:
		{
			PAINTSTRUCT ps = {};
			RECT rect = {};
			HDC hdc = BeginPaint(hWnd, &ps);
			GetClientRect(hWnd, &rect);

			RECT &paintRect = ps.rcPaint;

			int windowHeight = rect.bottom - rect.top;
			int ScrollHeight = 0;
			SCROLLINFO si;
			si.cbSize = sizeof(si);
			si.fMask = SIF_POS | SIF_RANGE;
			GetScrollInfo(pThis->m_scroll_hwnd, SB_CTL, &si);
			ScrollHeight = si.nPos;

			RECT msgRect = rect;
			msgRect.top -= ScrollHeight;

			if (pThis->m_total_height < windowHeight && !pThis->m_bIsTopDown) {
				msgRect.top += windowHeight - pThis->m_total_height;
			}

			msgRect.bottom = msgRect.top;

			SetTextColor(hdc, GetSysColor(COLOR_MENUTEXT));

			eMessageStyle mStyle = GetLocalSettings()->GetMessageStyle();

			int oldBkMode = 0; bool oldBkModeSet = false;
			COLORREF oldBkColor = CLR_NONE;
			COLORREF chosenBkColor = CLR_NONE;

			switch (mStyle) {
				case MS_GRADIENT: {
					oldBkMode = SetBkMode(hdc, TRANSPARENT);
					oldBkModeSet = true;
					chosenBkColor = GetSysColor(COLOR_3DFACE);
					break;
				}
				case MS_FLATBR: {
					chosenBkColor = GetSysColor(COLOR_WINDOW);
					oldBkColor = SetBkColor(hdc, chosenBkColor);
					break;
				}
				default: {
					chosenBkColor = GetSysColor(COLOR_3DFACE);
					oldBkColor = SetBkColor(hdc, chosenBkColor);
					break;
				}
			}

			DrawingContext mddc(hdc);
			mddc.SetBackgroundColor(chosenBkColor);

			pThis->m_firstShownMessage = 0;
			for (std::list<MessageItem>::iterator iter = pThis->m_messages.begin();
				iter != pThis->m_messages.end();
				++iter)
			{
				bool isActionMessage = IsActionMessage(iter->m_msg.m_type);

				bool needUpdate = false;

				if (!isActionMessage) {
					if (iter->m_message.Empty())
						needUpdate = true;
				}
				else {
					if (iter->m_bNeedUpdate)
						needUpdate = true;
				}

				if (needUpdate)
					iter->Update(pThis->m_guildID);

				HGDIOBJ gdiObj = SelectObject(hdc, g_MessageTextFont);

				// measure the message text
				msgRect.bottom = msgRect.top + iter->m_height;
				iter->m_rect = msgRect;

				if (msgRect.top <= rect.bottom && msgRect.bottom > rect.top)
				{
					pThis->DrawMessage(hdc, *iter, msgRect, rect, paintRect, mddc, chosenBkColor);
				}
				else
				{
					SetRectEmpty(&iter->m_avatarRect);
					SetRectEmpty(&iter->m_authorRect);
					SetRectEmpty(&iter->m_messageRect);

					for (auto& att : iter->m_attachmentData)
					{
						SetRectEmpty(&att.m_addRect);
						SetRectEmpty(&att.m_boxRect);
						SetRectEmpty(&att.m_textRect);
					}

					for (auto& inter : iter->m_interactableData)
						SetRectEmpty(&inter.m_rect);
				}

				msgRect.top = msgRect.bottom;
				SelectObject(hdc, gdiObj);
			}

			if (oldBkModeSet)
				SetBkMode(hdc, oldBkMode);

			if (oldBkColor != CLR_NONE)
				SetBkColor(hdc, oldBkColor);

			EndPaint(hWnd, &ps);
			break;
		}
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void MessageList::InitializeClass()
{
	WNDCLASS& wc1 = g_MsgListParentClass;

	wc1.lpszClassName = T_MESSAGE_LIST_PARENT_CLASS;
	wc1.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
	wc1.style         = 0;
	wc1.lpfnWndProc   = MessageList::ParentWndProc;
	wc1.hCursor       = LoadCursor(0, IDC_ARROW);

	RegisterClass(&wc1);

	WNDCLASS& wc = g_MsgListClass;

	wc.lpszClassName = T_MESSAGE_LIST_CLASS;
	wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
	wc.style         = 0;
	wc.lpfnWndProc   = MessageList::WndProc;
	wc.hCursor       = LoadCursor(0, IDC_ARROW);

	RegisterClass(&wc);

	g_ReplyPieceIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_REPLY_PIECE)));
}

void MessageList::UpdateBackgroundBrush()
{
	int bru = COLOR_3DFACE;
	auto mode = GetLocalSettings()->GetMessageStyle();
	if (mode == MS_FLATBR)
		bru = COLOR_WINDOW;

	SetClassLongPtr(m_scrollable_hwnd, GCLP_HBRBACKGROUND, (LONG_PTR) GetSysColorBrush(bru));
}

bool MessageList::SendToMessage(Snowflake sf, bool requestIfNeeded)
{
	m_messageSentTo = sf;
	m_firstShownMessage = sf;
	auto mi = FindMessage(sf);

	if (mi != m_messages.end()) {
		int y = 0;
		for (auto iter = m_messages.begin(); iter != mi && iter != m_messages.end(); ++iter) {
			y += iter->m_height;
		}

		// centers the message
		RECT rcClient{};
		GetClientRect(m_scrollable_hwnd, &rcClient);

		SCROLLINFO si{};
		si.cbSize = sizeof(SCROLLINFO);
		si.fMask = SIF_RANGE;
		GetScrollInfo(m_scroll_hwnd, SB_CTL, &si);

		int yOffs = (rcClient.bottom - rcClient.top) / 2 - mi->m_height / 2;
		y -= yOffs;

		if (y < si.nMin)
			y = si.nMin;
		if (y > si.nMax)
			y = si.nMax;

		// Just scroll there
		si.fMask = SIF_POS;
		si.nPos = y;
		SetScrollInfo(m_scroll_hwnd, SB_CTL, &si, true);

		SendMessage(m_scrollable_hwnd, WM_VSCROLL, SB_ENDSCROLL, 0);
		return !mi->m_msg.IsLoadGap();
	}

	if (!requestIfNeeded)
		return false;

	GetDiscordInstance()->RequestMessages(m_channelID, ScrollDir::AROUND, sf, sf);
	return true;
}

void MessageList::UpdateMembers(std::set<Snowflake>& mems)
{
	RECT rcClient{};
	GetClientRect(m_hwnd, &rcClient);

	HDC hdc = GetDC(m_hwnd);
	std::vector<RECT> updates;

	for (auto& msg : m_messages)
	{
		auto iter = mems.find(msg.m_msg.m_author_snowflake);
		if (iter == mems.end())
			continue; // no need to refresh

		RECT rcRefresh = msg.m_authorRect;
		rcRefresh.left = rcClient.left;
		rcRefresh.right = rcClient.right;

		Profile* pf = GetProfileCache()->LookupProfile(*iter, "", "", "", false);
		msg.m_msg.m_author = pf->GetName(m_guildID);

		// update author text
		if (msg.m_author)
			free((void*) msg.m_author);

		msg.m_author = ConvertCppStringToTString(msg.m_msg.m_author);

		// recalculate author width and stuff
		int height = 0;
		int authHeight = 0;
		int authWidth = 0;
		int dateWidth = 0;
		int replyAuthWidth = 0;
		int replyMsgWidth = 0;
		int replyHeight = 0;
		MeasureMessage(
			hdc,
			msg.m_message,
			msg.m_author,
			msg.m_date,
			msg.m_dateEdited,
			msg.m_replyMsg,
			msg.m_replyAuth,
			msg.m_messageRect,
			height,
			authHeight,
			authWidth,
			dateWidth,
			replyMsgWidth,
			replyAuthWidth,
			replyHeight
		);

		msg.m_authWidth  = authWidth;
		msg.m_authHeight = authHeight;
		msg.m_dateWidth  = dateWidth;
		msg.m_replyAuthorWidth = replyAuthWidth;
		msg.m_replyMessageWidth = replyMsgWidth;

		if (rcRefresh.bottom < 0 || rcRefresh.top >= rcClient.bottom)
			continue;

		rcRefresh.right = rcClient.right;
		updates.push_back(rcRefresh);
	}

	ReleaseDC(m_hwnd, hdc);

	for (auto& update : updates)
		InvalidateRect(m_hwnd, &update, FALSE);
}

void MessageList::UpdateScrollBar(int addToHeight, int diffNow, bool toStart, bool update, int offsetY)
{
	int totalHeightOld = m_total_height;
	m_total_height += addToHeight;

	RECT rect;
	GetClientRect(m_hwnd, &rect);

	int pageHeight = rect.bottom - rect.top;

	int th = m_total_height - 1;
	if (th <= 0)
		th = 0;

	SCROLLINFO scrollInfo;
	scrollInfo.cbSize = sizeof scrollInfo;
	scrollInfo.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;

	GetScrollInfo(m_scroll_hwnd, SB_CTL, &scrollInfo);

	if ((!toStart && !m_bIsTopDown) || (toStart && m_bIsTopDown))
		scrollInfo.nPos += diffNow;

	scrollInfo.nMin = 0;
	scrollInfo.nMax = th;
	scrollInfo.nPage = pageHeight;

	int posNow = scrollInfo.nPos;
	SetScrollInfo(m_scroll_hwnd, SB_CTL, &scrollInfo, true);
	GetScrollInfo(m_scroll_hwnd, SB_CTL, &scrollInfo);
	m_oldPos = scrollInfo.nPos;

	if (m_bManagedByOwner)
		return;

	if (posNow != scrollInfo.nPos)
	{
		// ah screw it
		// TODO: fix it so it only redraws what's needed
		InvalidateRect(m_scrollable_hwnd, NULL, true);
		return;
	}

	if (update) {
		RECT rcScroll = rect;
		if (offsetY > 0) {
			rcScroll.bottom -= offsetY;
		} else {
			rcScroll.top -= offsetY;
		}
		int diffUpDown = toStart ? 0 : diffNow;
		ScrollWindowEx(m_scrollable_hwnd, 0, -diffUpDown, offsetY ? &rcScroll : NULL, NULL, NULL, NULL, SW_INVALIDATE);
		UpdateWindow(m_scrollable_hwnd);
	}
}

std::list<MessageItem>::iterator MessageList::FindMessage(Snowflake sf)
{
	for (auto iter = m_messages.rbegin(); iter != m_messages.rend(); ++iter)
	{
		if (iter->m_msg.m_snowflake == sf)
			return --iter.base();
	}
	return m_messages.end();
}

std::list<MessageItem>::iterator MessageList::FindMessageByPoint(POINT pt)
{
	for (auto iter = m_messages.rbegin(); iter != m_messages.rend(); ++iter)
	{
		if (PtInRect(&iter->m_rect, pt))
			return --iter.base();
	}
	return m_messages.end();
}

std::list<MessageItem>::iterator MessageList::FindMessageByPointAuthorRect(POINT pt)
{
	for (auto iter = m_messages.rbegin(); iter != m_messages.rend(); ++iter)
	{
		if (iter->m_msg.m_type != MessageType::GAP_UP &&
			iter->m_msg.m_type != MessageType::GAP_DOWN &&
			iter->m_msg.m_type != MessageType::GAP_AROUND &&
			iter->m_msg.m_author_snowflake != 0 &&
			(PtInRect(&iter->m_authorRect, pt) || PtInRect(&iter->m_avatarRect, pt)))
			return --iter.base();
	}
	return m_messages.end();
}

void MessageList::FullRecalcAndRepaint()
{
	for (auto& msg : m_messages) {
		msg.Update(m_guildID);
	}

	RecalcMessageSizes(true);
}

void MessageList::AdjustHeightInfo(const MessageItem& msg, int& height, int& textheight, int& authheight, int& replyheight, int& attachheight, int& embedheight)
{
	bool isCompact = IsCompact();
	int replyheight2 = replyheight;
	if (replyheight2)
		replyheight2 += ScaleByDPI(5);

	if (IsActionMessage(msg.m_msg.m_type))
	{
		replyheight2 = 0;
		height = replyheight2 + authheight + ScaleByDPI(20);
	}
	else if (isCompact)
	{
		height = replyheight2 + std::max(textheight, authheight) + ScaleByDPI(20);
	}
	else
	{
		height = replyheight2 + textheight + authheight + ScaleByDPI(25);
	}

	int minHeight = replyheight2 + ScaleByDPI(PROFILE_PICTURE_SIZE_DEF + 20);

	// If a date gap, add an extra X pixels
	if (!m_bManagedByOwner && msg.m_msg.m_bIsDateGap)
		height += DATE_GAP_HEIGHT, minHeight += DATE_GAP_HEIGHT;

	// also figure out embed size
	embedheight = 0;
	for (auto& emb : msg.m_embedData)
	{
		int inc = emb.m_size.cy + ScaleByDPI(5);
		height += inc;
		embedheight += inc;
	}

	// also figure out attachment size
	attachheight = 0;
	for (auto& att : msg.m_msg.m_attachments)
	{
		// XXX improve?
		int inc = 0;
		if (att.m_contentType == ContentType::BLOB)
			inc = ATTACHMENT_HEIGHT + ATTACHMENT_GAP;
		else
			inc = att.m_previewHeight + ATTACHMENT_GAP;

		height += inc;
		attachheight += inc;
	}

	if (attachheight != 0)
		height += ScaleByDPI(5);
	if (embedheight != 0)
		height += ScaleByDPI(5);

	if (!IsActionMessage(msg.m_msg.m_type) && !IsCompact() && height < minHeight)
		height = minHeight;
}

void MessageList::RecalcMessageSizes(bool update)
{
	m_total_height = 0;

	RECT msgRect2 = {};
	GetClientRect(m_scrollable_hwnd, &msgRect2);

	bool isCompact = IsCompact();

	HDC hdc = GetDC(m_scrollable_hwnd);
	time_t prevTime = 0;

	for (std::list<MessageItem>::iterator iter = m_messages.begin();
		iter != m_messages.end();
		++iter)
	{
		RECT msgRect = msgRect2;

		int height = 100, authheight = 100, authwidth = 200, textheight = 200, datewidth = 0;

		if (iter->m_message.Empty())
		{
			iter->m_bKeepHeightRecalc = false;
			iter->Update(m_guildID);
		}

		if (iter->m_bKeepHeightRecalc)
		{
			bool bIsDateGap = !m_bManagedByOwner && (prevTime / 86400 != iter->m_msg.m_dateTime / 86400);
			prevTime = iter->m_msg.m_dateTime;

			if (bIsDateGap && !iter->m_msg.m_bIsDateGap)
				iter->m_height += DATE_GAP_HEIGHT;
			if (!bIsDateGap && iter->m_msg.m_bIsDateGap)
				iter->m_height -= DATE_GAP_HEIGHT;

			iter->m_msg.m_bIsDateGap = bIsDateGap;
			iter->m_bKeepHeightRecalc = false;
		}
		else
		{
			iter->m_msg.m_bIsDateGap = !m_bManagedByOwner && (prevTime / 86400 != iter->m_msg.m_dateTime / 86400);
			prevTime = iter->m_msg.m_dateTime;
			DetermineMessageMeasurements(*iter, hdc, &msgRect);
		}

		m_total_height += iter->m_height;
	}

	ReleaseDC(m_scrollable_hwnd, hdc);
	UpdateScrollBar(0, m_total_height, false, update);
}

void MessageList::OnUpdateAttachment(Snowflake sf)
{
	// An attachment was loaded.
	for (auto& msg : m_messages)
	{
		for (auto& ai : msg.m_attachmentData)
			if (ai.m_pAttachment->m_id == sf)
				InvalidateRect(m_scrollable_hwnd, &ai.m_boxRect, false);
	}
}

void MessageList::OnUpdateEmbed(const std::string& res)
{
	// An attachment or embedded image was loaded.
	for (auto& msg : m_messages)
	{
		for (auto& ai : msg.m_attachmentData)
		{
			if (ai.m_resourceID == res)
				InvalidateRect(m_scrollable_hwnd, &ai.m_boxRect, false);
		}
		for (auto& ai : msg.m_embedData)
		{
			if (ai.m_pEmbed->m_bHasThumbnail) {
				if (ai.m_thumbnailResourceID == res) {
					DbgPrintW("Invalidating thumbnail %s", res.c_str());
					InvalidateRect(m_scrollable_hwnd, &ai.m_thumbnailRect, false);
				}
			}
			else if (ai.m_pEmbed->m_bHasImage) {
				if (ai.m_imageResourceID == res) {
					DbgPrintW("Invalidating image %s", res.c_str());
					InvalidateRect(m_scrollable_hwnd, &ai.m_imageRect, false);
				}
			}
		}
	}
}

void MessageList::OnUpdateAvatar(Snowflake sf)
{
	for (auto& msg : m_messages)
	{
		if (msg.m_msg.m_author_snowflake == sf && !IsCompact())
			InvalidateRect(m_scrollable_hwnd, &msg.m_avatarRect, false);
	}
}

void MessageList::OnFailedToSendMessage(Snowflake sf)
{
	for (auto& msg : m_messages)
	{
		if (msg.m_msg.m_snowflake != sf)
			continue;

		msg.m_msg.m_type = MessageType::UNSENT_MESSAGE;
		msg.m_msg.SetTime(msg.m_msg.m_dateTime);
		msg.m_date = NULL;
		msg.Update(m_guildID);

		RECT rcUpdate2 = msg.m_authorRect;
		rcUpdate2.right = std::max(rcUpdate2.right, msg.m_messageRect.right);
		InvalidateRect(m_hwnd, &msg.m_messageRect, FALSE);
		InvalidateRect(m_hwnd, &rcUpdate2, FALSE);
	}
}

void MessageList::DetermineMessageMeasurements(MessageItem& mi, HDC _hdc, LPRECT _rect)
{
	HDC hdc = _hdc;
	RECT rect;
	if (!_hdc) hdc = GetDC(m_scrollable_hwnd);
	if (!_rect)
		GetClientRect(m_scrollable_hwnd, &rect);
	else
		rect = *_rect;
	
	LPCTSTR strAuth = mi.m_author;
	LPCTSTR strDate = mi.m_date;
	LPCTSTR strDateEdit = mi.m_dateEdited;
	LPCTSTR strReplyMsg = mi.m_replyMsg;
	LPCTSTR strReplyAuth = mi.m_replyAuth;

	// measure the height
	MeasureMessage(
		hdc,
		mi.m_message,
		strAuth,
		strDate,
		strDateEdit,
		strReplyMsg,
		strReplyAuth,
		rect,
		mi.m_textHeight,
		mi.m_authHeight,
		mi.m_authWidth,
		mi.m_dateWidth,
		mi.m_replyMessageWidth,
		mi.m_replyAuthorWidth,
		mi.m_replyHeight
	);
	
	const bool isCompact = IsCompact();
	for (auto& embed : mi.m_embedData)
		embed.Measure(hdc, rect, isCompact);

	AdjustHeightInfo(mi, mi.m_height, mi.m_textHeight, mi.m_authHeight, mi.m_replyHeight, mi.m_attachHeight, mi.m_embedHeight);

	if (!_hdc) ReleaseDC(m_scrollable_hwnd, hdc);
}

void MessageList::EditMessage(const Message& newMsg)
{
	MessageItem mi;
	Message& copy = mi.m_msg;
	copy = newMsg; // please copy
	mi.Update(m_guildID);
	
	LPCTSTR strAuth = mi.m_author;
	LPCTSTR strDate = mi.m_date;
	LPCTSTR strDateEdit = mi.m_dateEdited;

	// measure the height
	int oldHeight = 0;

	// If the message exists, then delete it.
	bool bDeletedOldMsg = false;
	RECT oldMsgRect = {};
	for (auto iter = m_messages.rbegin(); iter != m_messages.rend(); ++iter)
	{
		if (iter->m_msg.m_snowflake == newMsg.m_snowflake)
		{
			// delete it!
			oldHeight = iter->m_height;
			oldMsgRect = iter->m_rect;
			auto msgIter = --(iter.base());
			m_messages.erase(msgIter); // sucks
			bDeletedOldMsg = true;
			break;
		}
	}

	bool canInsert = false;
	auto insertIter = m_messages.begin();
	for (; insertIter != m_messages.end(); ++insertIter)
	{
		Snowflake thisSF = insertIter->m_msg.m_snowflake;
		Snowflake nextSF = UINT64_MAX;
		auto nextiter = insertIter;
		++nextiter;
		if (nextiter != m_messages.end()) {
			nextSF = nextiter->m_msg.m_snowflake;
		}

		if (thisSF < newMsg.m_snowflake && newMsg.m_snowflake < nextSF) {
			// note: advancing because list.insert(iter) shifts elements
			// starting from iter to the right, not from iter+1
			++insertIter;
			canInsert = true;
			break;
		}
	}
	
	assert(canInsert);
	assert(bDeletedOldMsg);
	assert(insertIter != m_messages.begin());

	DetermineMessageMeasurements(mi, NULL, NULL);
	copy.m_anchor = 0;

	bool toStart = false;
	m_messages.insert(insertIter, mi);

	RECT rcClient{};
	GetClientRect(m_scrollable_hwnd, &rcClient);

	// totally new message
	UpdateScrollBar(mi.m_height - oldHeight, mi.m_height - oldHeight, toStart, true, bDeletedOldMsg ? (rcClient.bottom - oldMsgRect.top) : 0);

	if (bDeletedOldMsg)
	{
		// in case the message shrunk in size
		oldMsgRect.top += oldHeight - mi.m_height;

		// do it better
		InvalidateRect(m_hwnd, &oldMsgRect, false);
	}
}

void MessageList::MessageHeightChanged(int oldHeight, int newHeight, bool toStart)
{
	UpdateScrollBar(newHeight - oldHeight, newHeight - oldHeight, toStart);
}

void MessageList::AddMessageInternal(const Message& msg, bool toStart, bool resetAnchor)
{
	MessageItem mi;
	mi.m_msg = msg;

	mi.Update(m_guildID);

	LPCTSTR strAuth = mi.m_author;
	LPCTSTR strDate = mi.m_date;
	LPCTSTR strDateEdit = mi.m_dateEdited;

	// measure the height
	RECT msgRect = {};
	GetClientRect(m_scrollable_hwnd, &msgRect);

	int oldHeight = 0;

	// If the message has a nonce, then delete that.
	bool bDeletedNonce = false;
	RECT nonceRect = {};
	for (auto iter = m_messages.rbegin(); iter != m_messages.rend(); ++iter)
	{
		if (iter->m_msg.m_snowflake == msg.m_anchor)
		{
			// delete it!
			oldHeight = iter->m_height;
			nonceRect = iter->m_rect;
			m_messages.erase(--(iter.base())); // sucks
			bDeletedNonce = true;
			break;
		}
	}

	time_t prevDate = 0;
	if (!m_messages.empty())
	{
		if (toStart) {
			prevDate = m_messages.begin()->m_msg.m_dateTime;
		}
		else {
			prevDate = m_messages.rbegin()->m_msg.m_dateTime;
		}
	}

	if (m_bManagedByOwner && prevDate / 86400 != mi.m_msg.m_dateTime / 86400)
		mi.m_msg.m_bIsDateGap = true;

	DetermineMessageMeasurements(mi, NULL, NULL);

	if (resetAnchor)
		mi.m_msg.m_anchor = 0;

	if (toStart)
		m_messages.push_front(mi);
	else
		m_messages.push_back(mi);

	RECT rcClient{};
	GetClientRect(m_scrollable_hwnd, &rcClient);

	int offsetY = 0;
	if (bDeletedNonce) {
		if (toStart)
			offsetY = -(nonceRect.bottom - rcClient.top);
		else
			offsetY = rcClient.bottom - nonceRect.top;
	}

	UpdateScrollBar(mi.m_height - oldHeight, mi.m_height - oldHeight, toStart, true, offsetY);

	if (bDeletedNonce)
	{
		// in case the message shrunk in size
		nonceRect.top += oldHeight - mi.m_height;

		// do it better
		InvalidateRect(m_hwnd, &nonceRect, false);
	}
}

void MessageList::UpdateAllowDrop()
{
	BOOL Accept = FALSE;
	Channel* pChan = GetDiscordInstance()->GetChannel(m_channelID);

	if (pChan)
		Accept = pChan->HasPermission(PERM_SEND_MESSAGES) && pChan->HasPermission(PERM_ATTACH_FILES);
	
	DragAcceptFiles(m_scrollable_hwnd, Accept);
}

MessageList* MessageList::Create(HWND hwnd, LPRECT pRect)
{
	MessageList* newThis = new MessageList;

	int scrollBarWidth = GetSystemMetrics(SM_CXVSCROLL);
	int width = pRect->right - pRect->left, height = pRect->bottom - pRect->top;

	newThis->m_hwnd = CreateWindowEx(
		WS_EX_CLIENTEDGE, T_MESSAGE_LIST_PARENT_CLASS, NULL, WS_CHILD | WS_VISIBLE,
		pRect->left, pRect->top, width, height, hwnd, (HMENU)CID_MESSAGELIST, g_hInstance, newThis
	);

	newThis->m_scrollable_hwnd = CreateWindowEx(
		0, T_MESSAGE_LIST_CLASS, NULL, WS_CHILD | WS_VISIBLE,
		0, 0, width - 4 - scrollBarWidth, height - 4, newThis->m_hwnd, (HMENU)1, g_hInstance, newThis
	);

	newThis->m_scroll_hwnd = CreateWindowEx(
		0,
		TEXT("SCROLLBAR"),
		(PTSTR)NULL,
		WS_CHILD | WS_VISIBLE | SBS_VERT,
		width - 4 - scrollBarWidth,
		0,
		scrollBarWidth,
		height - 4,
		newThis->m_hwnd,
		NULL,
		g_hInstance,
		NULL
	);

	newThis->UpdateBackgroundBrush();

	return newThis;
}
