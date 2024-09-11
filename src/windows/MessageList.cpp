#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include "MessageList.hpp"
#include "TextToSpeech.hpp"
#include "ProfilePopout.hpp"
#include "ImageViewer.hpp"
#include "UploadDialog.hpp"
#include "ImageLoader.hpp"
#include "../discord/LocalSettings.hpp"

#define STRAVAILABLE(str) ((str) && (str)[0] != 0)

// N.B. WINVER<=0x0500 doesn't define it. We'll force it
#ifndef IDC_HAND
#define IDC_HAND            MAKEINTRESOURCE(32649)
#endif//IDC_HAND

#define NEW_MARKER_COLOR RGB(255,0,0)

#define ATTACHMENT_HEIGHT (ScaleByDPI(40))
#define ATTACHMENT_GAP    (ScaleByDPI(4))
#define DATE_GAP_HEIGHT   (ScaleByDPI(20))
#define PROFILE_PICTURE_GAP (PROFILE_PICTURE_SIZE + 8);

static int GetProfileBorderRenderSize()
{
	return ScaleByDPI(Supports32BitIcons() ? (PROFILE_PICTURE_SIZE_DEF + 12) : 64);
}

static void DrawImageSpecial(HDC hdc, HBITMAP hbm, RECT rect, bool hasAlpha)
{
	if (hbm == HBITMAP_LOADING)
		DrawLoadingBox(hdc, rect);
	else if (hbm == HBITMAP_ERROR || !hbm)
		DrawErrorBox(hdc, rect);
	else
		DrawBitmap(hdc, hbm, rect.left, rect.top, NULL, CLR_NONE, rect.right - rect.left, rect.bottom - rect.top, hasAlpha);
}

WNDCLASS MessageList::g_MsgListClass;

static HICON g_ReplyPieceIcon;

static const int g_WelcomeTextIds[] =
{
	IDS_WELCOME_01,
	IDS_WELCOME_02,
	IDS_WELCOME_03,
	IDS_WELCOME_04,
	IDS_WELCOME_05,
	IDS_WELCOME_06,
	IDS_WELCOME_07,
	IDS_WELCOME_08,
	IDS_WELCOME_09,
	IDS_WELCOME_10,
	IDS_WELCOME_11,
	IDS_WELCOME_12,
	IDS_WELCOME_13,
};

static const int g_WelcomeTextCount = _countof(g_WelcomeTextIds);

MessageList::MessageList()
{
	m_defaultBackgroundBrush = GetSysColorBrush(COLOR_WINDOW);
}

MessageList::~MessageList()
{
	if (m_hwnd)
	{
		BOOL b = DestroyWindow(m_hwnd);
		assert(b && "was window destroyed?");
		m_hwnd = NULL;
	}

	if (m_backgroundImage)
	{
		DeleteBitmap(m_backgroundImage);
		m_backgroundImage = NULL;
	}

	if (m_backgroundBrush)
	{
		DeleteBitmap(m_backgroundBrush);
		m_backgroundBrush = NULL;
	}
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
	if (GetLocalSettings()->ShowEmbedImages())
	{
		if (m_pEmbed->m_bHasThumbnail)
			m_thumbnailSize = EnsureMaximumSize(m_pEmbed->m_thumbnailWidth, m_pEmbed->m_thumbnailHeight, maxImgWidth, maxHeight);
		else if (m_pEmbed->m_bHasImage)
			m_imageSize = EnsureMaximumSize(m_pEmbed->m_imageWidth, m_pEmbed->m_imageHeight, maxImgWidth, maxHeight);
	}
	else
	{
		m_thumbnailSize = { 0, 0 };
		m_imageSize = { 0, 0 };
	}

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

void RichEmbedItem::Draw(HDC hdc, RECT& messageRect, MessageList* pList)
{
	RECT rc = messageRect;
	rc.right = rc.left + m_size.cx + ScaleByDPI(4);
	rc.bottom = rc.top + m_size.cy;

	assert(m_size.cx && m_size.cy);

	RECT rcGradient = rc, rcLine = rc;
	rcLine.right = rcLine.left + ScaleByDPI(4);
	rcGradient.left += ScaleByDPI(4);
	COLORREF oldColor = SetBkColor(hdc, pList->GetDarkerBackgroundColor());
	if (GetLocalSettings()->GetMessageStyle() == MS_GRADIENT) {
		FillGradientColors(hdc, &rcGradient, GetSysColor(COLOR_WINDOWFRAME), CLR_NONE, true);
	}
	else {
		COLORREF oldClr = ri::SetDCBrushColor(hdc, pList->GetDarkerBackgroundColor());
		FillRect(hdc, &rcGradient, GetStockBrush(DC_BRUSH));
		ri::SetDCBrushColor(hdc, oldClr);
	}

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
	COLORREF windowTextColor = pList->InvertIfNeeded(GetSysColor(COLOR_WINDOWTEXT));

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
	if (GetLocalSettings()->ShowEmbedImages()) {
		if (m_thumbnailSize.cy) {
			m_thumbnailResourceID = GetAvatarCache()->MakeIdentifier(m_pEmbed->m_thumbnailUrl);
			GetAvatarCache()->AddImagePlace(m_thumbnailResourceID, eImagePlace::ATTACHMENTS, m_pEmbed->m_thumbnailProxiedUrl);
			bool hasAlpha = false;
			HBITMAP hbm = GetAvatarCache()->GetBitmapSpecial(m_pEmbed->m_thumbnailUrl, hasAlpha);
			if (sizeY) sizeY += gap;
			m_thumbnailRect = { rc.left, rc.top + sizeY, rc.left + m_thumbnailSize.cx, rc.top + sizeY + m_thumbnailSize.cy };
			DrawImageSpecial(hdc, hbm, m_thumbnailRect, hasAlpha);
			sizeY += m_thumbnailSize.cy;
		}
		if (m_imageSize.cy) {
			m_imageResourceID = GetAvatarCache()->MakeIdentifier(m_pEmbed->m_imageUrl);
			GetAvatarCache()->AddImagePlace(m_imageResourceID, eImagePlace::ATTACHMENTS, m_pEmbed->m_imageProxiedUrl);
			bool hasAlpha = false;
			HBITMAP hbm = GetAvatarCache()->GetBitmapSpecial(m_pEmbed->m_imageUrl, hasAlpha);
			if (sizeY) sizeY += gap;
			m_imageRect = { rc.left, rc.top + sizeY, rc.left + m_imageSize.cx, rc.top + sizeY + m_imageSize.cy };
			DrawImageSpecial(hdc, hbm, m_imageRect, hasAlpha);
			sizeY += m_imageSize.cy;
		}
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
	bool isAuthorBot,
	const RECT& msgRect,
	int& textheight,
	int& authheight,
	int& authwidth,
	int& datewidth,
	int& replymsgwidth,
	int& replyauthwidth,
	int& replyheight,
	int placeinchain)
{
	bool bIsCompact = IsCompact();

	// measure the author text
	HGDIOBJ gdiObj = SelectObject(hdc, g_AuthorTextFont);
	RECT rcCommon1 = msgRect, rcCommon2;
	RECT rc;
	rcCommon1.right -= ScaleByDPI(20);
	rcCommon1.bottom -= ScaleByDPI(20);
	if (placeinchain == 0) {
		rcCommon2 = rcCommon1;
		if (!bIsCompact)
			rcCommon1.right -= ScaleByDPI(PROFILE_PICTURE_SIZE_DEF + 12);

		rc = rcCommon1;
		DrawText(hdc, strAuth, -1, &rc, DT_CALCRECT | DT_NOPREFIX);
		authheight = rc.bottom - rc.top, authwidth = rc.right - rc.left;
		if (authheight == 0)
			authheight = DrawText(hdc, TEXT("Wp"), -1, &rc, DT_CALCRECT | DT_NOPREFIX);
		if (isAuthorBot) authwidth += GetSystemMetrics(SM_CXSMICON) + ScaleByDPI(4);
	}
	else {
		authwidth = authheight = 0;
	}
	
	if (placeinchain == 0 && (strReplyMsg || strReplyAuth))
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

	if (bIsCompact || placeinchain == 0)
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

	if (m_msg.m_pReferencedMessage)
	{
		std::string authorStr = m_msg.m_pReferencedMessage->m_author;
		if (m_msg.m_pReferencedMessage->m_bMentionsAuthor) {
			authorStr = "@" + authorStr;
		}

		m_replyMsg = ConvertCppStringToTString(m_msg.m_pReferencedMessage->m_message);
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

static void ShiftUpRect(RECT& rc, int amount) {
	// n.b. Don't shift if covering a total area of zero
	if (rc.top == rc.bottom || rc.left == rc.right) return;
	rc.top    -= amount;
	rc.bottom -= amount;
}

static void ShiftUpDRect(Rect& rc, int amount) {
	// n.b. Don't shift if covering a total area of zero
	if (rc.top == rc.bottom || rc.left == rc.right) return;
	rc.top    -= amount;
	rc.bottom -= amount;
}

void MessageItem::ShiftUp(int amount)
{
	// Shift all the rectangles up by [amount] pixels.
	// Validation care has been taken to ensure this doesn't result in an illegal state.
	ShiftUpRect(m_rect, amount);
	ShiftUpRect(m_authorRect, amount);
	ShiftUpRect(m_refAvatarRect, amount);
	ShiftUpRect(m_messageRect, amount);
	
	for (auto& word : m_message.GetWords()) {
		ShiftUpDRect(word.m_rect, amount);
	}

	for (auto& att : m_attachmentData) {
		ShiftUpRect(att.m_addRect, amount);
		ShiftUpRect(att.m_boxRect, amount);
		ShiftUpRect(att.m_textRect, amount);
	}

	for (auto& itd : m_interactableData) {
		ShiftUpRect(itd.m_rect, amount);
	}

	for (auto& emb : m_embedData) {
		ShiftUpRect(emb.m_rect, amount);
		ShiftUpRect(emb.m_imageRect, amount);
		ShiftUpRect(emb.m_thumbnailRect, amount);
		ShiftUpRect(emb.m_titleRect, amount);
		ShiftUpRect(emb.m_authorRect, amount);
		ShiftUpRect(emb.m_providerRect, amount);
	}
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
	RECT invalidateLater{};
	bool invalidateLaterExists = false;
	int messageHeight = iter->m_height;

	// Adjust the message afterwards
	std::list<MessageItem>::iterator niter = --(iter.base()), afteriter = niter, beforeiter = niter;
	++afteriter;

	bool wasFirstMessage = niter == m_messages.begin();

	int afterMessageInitialHeight = 0;
	int afterMessageNowHeight = 0;

	if (afteriter != m_messages.end())
	{
		Snowflake sf = Snowflake(-1);
		time_t tm = 0;
		int pl = 0;
		MessageType::eType et = MessageType::DEFAULT;
		if (beforeiter != m_messages.begin()) {
			--beforeiter;
			sf = beforeiter->m_msg.m_author_snowflake;
			tm = beforeiter->m_msg.m_dateTime;
			et = beforeiter->m_msg.m_type;
			pl = beforeiter->m_placeInChain;
		}

		afterMessageInitialHeight = afteriter->m_height;

		bool shouldRecalc = false;
		if (ShouldStartNewChain(sf, tm, pl, et, *afteriter, false)) {
			afteriter->m_placeInChain = 0; // don't care about the rest to be honest
			shouldRecalc = true;
		}

		if (ShouldBeDateGap(tm, afteriter->m_msg.m_dateTime)) {
			afteriter->m_bIsDateGap = true;
		}

		if (shouldRecalc) {
			DetermineMessageMeasurements(*afteriter);
		}

		afterMessageNowHeight = afteriter->m_height;
	}

	m_messages.erase(niter); // still stupid that I have to do this decrement crap

	int deltaMessageAfter = afterMessageNowHeight - afterMessageInitialHeight;
	int pullDownAmount = messageHeight - deltaMessageAfter;
	messageRect.bottom -= deltaMessageAfter;

	UpdateScrollBar(-pullDownAmount, -pullDownAmount, false, false);

	SendMessage(m_hwnd, WM_MSGLIST_PULLDOWN, 0, (LPARAM)&messageRect);

	RECT invalRect = messageRect;
	invalRect.top = invalRect.bottom;
	invalRect.bottom += afterMessageNowHeight;
	InvalidateRect(m_hwnd, &invalRect, FALSE);
}

void MessageList::Repaint()
{
	InvalidateRect(m_hwnd, NULL, false);
}

void MessageList::ClearMessages()
{
	m_messages.clear();
	m_total_height = 0;
	UpdateScrollBar(0, 0, false);
}

void MessageList::RefetchMessages(Snowflake gapCulprit, bool causedByLoad)
{
	Channel* pChan = GetDiscordInstance()->GetChannel(m_channelID);
	if (!pChan)
		return;

	if (!causedByLoad)
		m_previousLastReadMessage = pChan->m_lastViewedMsg;

	RECT rect;
	GetClientRect(m_hwnd, &rect);

	int pageHeight = rect.bottom - rect.top;

	bool wasAnythingLoaded = false;
	for (auto& msg : m_messages) {
		wasAnythingLoaded = !msg.m_msg.IsLoadGap();
		if (wasAnythingLoaded)
			break;
	}

	SCROLLINFO scrollInfo;
	scrollInfo.cbSize = sizeof scrollInfo;
	scrollInfo.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;

	GetScrollInfo(m_hwnd, SB_VERT, &scrollInfo);

	// Find the position of the old message
	int oldYMessage = 0;
	for (auto& msg : m_messages) {
		if (msg.m_msg.m_snowflake == m_firstShownMessage) {
			if (m_bManagedByOwner && msg.m_bIsDateGap)
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
	for (auto iter = m_messages.begin(); iter != m_messages.end(); ++iter)
	{
		if (gapCulprit == iter->m_msg.m_snowflake)
		{
			updateRect = iter->m_rect;
			haveUpdateRect = true;
			break;
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

	Snowflake insertAfter = UINT64_MAX;

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
			insertAfter = std::min(insertAfter, oldMsg->m_msg.m_snowflake);

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

	if (insertAfter == UINT64_MAX) {
		insertAfter = 0;
	}

	ts = GetTimeUs();
	int repaintSize = 0;
	int scrollAdded = RecalcMessageSizes(gapCulprit == 0, repaintSize, insertAfter);
	te = GetTimeUs();
	DbgPrintW("Recalculation process took %lld us", te - ts);

	int newYMessage = 0;
	for (auto& msg : m_messages) {
		if (msg.m_msg.m_snowflake == m_firstShownMessage)
			break;
		
		newYMessage += msg.m_height;
	}

	int th = m_total_height - 1;
	if (th <= 0)
		th = 0;

	scrollInfo.nPos = newYMessage - oldScrollDiff - scrollAdded;
	scrollInfo.nMin = 0;
	scrollInfo.nMax = th;
	scrollInfo.nPage = pageHeight;

	int scrollAnyway = scrollInfo.nPos;

	if (scrollInfo.nPos < 0)
		scrollInfo.nPos = 0;

	scrollAnyway -= scrollInfo.nPos;

	BOOL eraseWhenUpdating = FALSE;

	if (haveUpdateRect)
	{
		// TODO - hacky fix
		if (m_total_height < int(scrollInfo.nPage))
			updateRect.bottom = rect.bottom, updateRect.top = rect.top, scrollAnyway = 0, eraseWhenUpdating = TRUE;
		else
			updateRect.bottom += scrollAdded + repaintSize;
	}

	if (!wasAnythingLoaded && m_previousLastReadMessage != pChan->m_lastSentMsg)
	{
		// Send them to the top of the unread section
		int y = 0;

		for (auto& msg : m_messages)
		{
			if (!msg.m_msg.IsLoadGap() && msg.m_msg.m_snowflake > m_previousLastReadMessage)
				break;

			y += msg.m_height;
		}

		DbgPrintW("Setting y pos to: %d.  Prevlastread: %lld", y, m_previousLastReadMessage);
		scrollInfo.nPos = y;
		scrollAnyway = false;
		haveUpdateRect = false;
	}

	SetScrollInfo(m_hwnd, SB_VERT, &scrollInfo, true);
	GetScrollInfo(m_hwnd, SB_VERT, &scrollInfo);

	m_oldPos = scrollInfo.nPos;
	
	if (!MayErase())
	{
		InvalidateRect(m_hwnd, NULL, FALSE);
	}
	else if (m_messageSentTo)
	{
		// Send to the message without creating a new gap.
		m_firstShownMessage = m_messageSentTo;
		FlashMessage(m_messageSentTo);
		if (SendToMessage(m_messageSentTo, false))
			m_messageSentTo = 0;
	}
	else if (!m_bManagedByOwner)
	{
		if (scrollAnyway)
			Scroll(-scrollAnyway);

		InvalidateRect(m_hwnd, haveUpdateRect ? &updateRect : NULL, eraseWhenUpdating);
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
	RECT rect;
	GetClientRect(m_hwnd, &rect);
	MoveWindow(m_hwnd, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, true);
}

bool MessageList::MayErase()
{
	return GetLocalSettings()->GetMessageStyle() != MS_IMAGE;
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
				InvalidateRect(m_hwnd, &msg2->m_authorRect, FALSE);
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
	InvalidateRect(m_hwnd, &msg->m_authorRect, FALSE);
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
							InvalidateRect(m_hwnd, &x.m_textRect, FALSE);
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
			InvalidateRect(m_hwnd, &pAttach->m_textRect, FALSE);
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
							InvalidateRect(m_hwnd, &x.m_rect, FALSE);
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
				InvalidateRect(m_hwnd, &pData->m_rect, FALSE);
			break;
		}
	}

	m_highlightedInteractable = pItem->m_wordIndex;
	m_highlightedInteractableMessage = msg->m_msg.m_snowflake;
	pItem->m_bHighlighted = true;
	if (pItem->ShouldInvalidateOnHover())
		InvalidateRect(m_hwnd, &pItem->m_rect, FALSE);

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

void MessageList::OpenInteractable(InteractableItem* pItem, MessageItem* pMsg)
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
					if (pMsg->m_msg.IsWebHook())
						break;

					RECT rect;
					GetWindowRect(m_hwnd, &rect);
					ProfilePopout::Show(sf, m_guildID, rect.left + pItem->m_rect.right + 10, rect.top + pItem->m_rect.top);
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

	bool hasAlpha = false;
	HBITMAP hbm = GetAvatarCache()->GetBitmapSpecial(attachItem.m_resourceID, hasAlpha);
	DrawImageSpecial(hdc, hbm, childAttachRect, hasAlpha);
}

void MessageList::DrawDefaultAttachment(HDC hdc, RECT& paintRect, AttachmentItem& attachItem, RECT& attachRect)
{
	RECT childAttachRect = attachRect;
	childAttachRect.bottom = childAttachRect.top + ATTACHMENT_HEIGHT;

	RECT intersRect;
	bool inView = IntersectRect(&intersRect, &paintRect, &childAttachRect);

	int iconSize = ScaleByDPI(32);
	int rectHeight = childAttachRect.bottom - childAttachRect.top;

	COLORREF old = CLR_NONE;
	if (attachItem.m_bHighlighted && inView)
		old = SetTextColor(hdc, RGB(0, 0, 255));
	else
		old = SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));

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

	childAttachRect.right = childAttachRect.left + iconSize + iconSize + width + ScaleByDPI(20);
	attachRect.bottom = attachRect.top = childAttachRect.bottom + ATTACHMENT_GAP;

	if (inView)
	{
		DrawEdge(hdc, &childAttachRect, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_RECT | BF_MIDDLE);
		DrawText(hdc, name, -1, &textRect, DT_NOPREFIX | DT_NOCLIP);
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

	attachItem.m_textRect = textRect;

	uint32_t oc = SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
	if (old == CLR_NONE)
		old = oc;

	LPCTSTR sizeStr = attachItem.m_sizeText;
	textRect.top += rcMeasure.bottom;
	textRect.bottom += rcMeasure.bottom;

	if (inView)
	{
		SelectObject(hdc, g_DateTextFont);
		DrawText(hdc, sizeStr, -1, &textRect, DT_NOPREFIX | DT_NOCLIP);
	}

	if (old != CLR_NONE)
		SetTextColor(hdc, old);

	int dlIconSize = iconSize / 2;

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
		case MessageType::NO_NOTIFICATIONS:
		case MessageType::CHANNEL_HEADER:
		case MessageType::STAGE_START:
		case MessageType::STAGE_END:
		case MessageType::STAGE_SPEAKER:
		case MessageType::STAGE_TOPIC:
		case MessageType::GUILD_BOOST:
		case MessageType::GUILD_BOOST_TIER_1:
		case MessageType::GUILD_BOOST_TIER_2:
		case MessageType::GUILD_BOOST_TIER_3:
			return true;
	}

	return false;
}

bool MessageList::IsClientSideMessage(MessageType::eType msgType)
{
	switch (msgType)
	{
		case MessageType::GAP_UP:
		case MessageType::GAP_DOWN:
		case MessageType::GAP_AROUND:
		case MessageType::CANT_VIEW_MSG_HISTORY:
		case MessageType::LOADING_PINNED_MESSAGES:
		case MessageType::NO_PINNED_MESSAGES:
		case MessageType::NO_NOTIFICATIONS:
		case MessageType::CHANNEL_HEADER:
			return true;
	}

	return false;
}

// This is a horrible mess!
void MessageList::DetermineMessageData(
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
			icon = IDI_USER_JOIN;

			int welcomeTextIndex = ExtractTimestamp(id) % g_WelcomeTextCount;

			LPCTSTR tstr = TmGetTString(g_WelcomeTextIds[welcomeTextIndex]);
			size_t slen = _tcslen(tstr);
			freedStringSpace = (LPTSTR) calloc(slen + 1, sizeof(TCHAR));
			if (!freedStringSpace) {
				messagePart1 = TEXT("Yer' probably outta memory, duuuude! But ");
				messagePart2 = TEXT("joined.");
				break;
			}

			_tcscpy(freedStringSpace, tstr);

			messagePart1 = freedStringSpace;
			messagePart2 = TEXT(" Oh no!");

			// find the dollar sign, place a null character and set part 2 to the right
			for (size_t i = 0; i < slen; i++) {
				if (freedStringSpace[i] == (TCHAR) '$') {
					freedStringSpace[i] =  (TCHAR) '\0';
					messagePart2 = &freedStringSpace[i + 1];
					break;
				}
			}

			break;
		}

		case MessageType::CHANNEL_HEADER:
		{
			Channel* pChan = GetDiscordInstance()->GetChannel(channelID);
			if (!pChan) {
				messagePart1 = TEXT("Unknown channel");
				break;
			}

			if (pChan->m_channelType == Channel::DM) {
				messagePart1 = TmGetTString(IDS_WELCOME_TO_START_DM_1);
				messagePart2 = TmGetTString(IDS_WELCOME_TO_START_DM_2);
				break;
			}

			messagePart1 = TmGetTString(IDS_WELCOME_TO_START_1);

			if (pChan->m_channelType == Channel::GROUPDM) {
				messagePart2 = TmGetTString(IDS_WELCOME_TO_START_GROUP);
			}
			else {
				messagePart2 = TmGetTString(IDS_WELCOME_TO_START_2);
			}

			break;
		}

		case MessageType::CHANNEL_PINNED_MESSAGE:
		{
			messagePart1 = TEXT("");
			messagePart2 = TmGetTString(IDS_PINNED_1);
			messagePart3 = TmGetTString(IDS_PINNED_3);
			clickableString = TmGetTString(IDS_PINNED_2);
			clickableLink = CreateMessageLink(refMsgGuildID, refMsgChannelID, refMsgMessageID);
			icon = IDI_PIN;
			break;
		}

		case MessageType::GUILD_BOOST:
		{
			messagePart1 = TEXT("");
			messagePart2 = TmGetTString(IDS_BOOSTED_GUILD);
			icon = IDI_BOOST;
			break;
		}

		case MessageType::GUILD_BOOST_TIER_1:
		case MessageType::GUILD_BOOST_TIER_2:
		case MessageType::GUILD_BOOST_TIER_3:
		{
			std::string guildName = "[unknown guild]";
			Guild* pGld = GetDiscordInstance()->GetGuild(guildID);
			if (pGld)
				guildName = pGld->m_name;

			freedStringSpace = ConvertCppStringToTString(TmGetString(IDS_BOOSTED_GUILD) + guildName + TmGetString(IDS_BOOSTED_ACHIEVED));

			messagePart1 = TEXT("");
			messagePart2 = freedStringSpace;
			messagePart3 = TEXT("!");

			int tier = int(msgType) - int(MessageType::GUILD_BOOST_TIER_1);
			clickableString = TmGetTString(IDS_BOOSTED_TIER_1 + tier);

			icon = IDI_BOOST;
			break;
		}

		case MessageType::STAGE_START:
		{
			freedStringSpace = ConvertCppStringToTString(content);
			messagePart1 = TEXT("");
			messagePart2 = TmGetTString(IDS_STAGE_STARTED);
			clickableString = freedStringSpace;
			break;
		}

		case MessageType::STAGE_END:
		{
			freedStringSpace = ConvertCppStringToTString(content);
			messagePart1 = TEXT("");
			messagePart2 = TmGetTString(IDS_STAGE_ENDED);
			clickableString = freedStringSpace;
			break;
		}

		case MessageType::STAGE_TOPIC:
		{
			freedStringSpace = ConvertCppStringToTString(content);
			messagePart1 = TEXT("");
			messagePart2 = TmGetTString(IDS_STAGE_TOPIC);
			clickableString = freedStringSpace;
			break;
		}

		case MessageType::STAGE_SPEAKER:
		{
			messagePart1 = TEXT("");
			messagePart2 = TmGetTString(IDS_STAGE_SPEAKER);
			break;
		}

		case MessageType::RECIPIENT_ADD:
		case MessageType::RECIPIENT_REMOVE:
		{
			messagePart1 = TEXT("");
			clickableSF = ments.size() != 1 ? 0 : *ments.begin();

			if (msgType == MessageType::RECIPIENT_ADD) {
				icon = IDI_USER_JOIN;
				messagePart2 = TmGetTString(IDS_GROUP_JOIN_1);
				messagePart3 = TmGetTString(IDS_GROUP_JOIN_2);
				clickableString = TEXT("who?");
			}
			else if (clickableSF == authorId) {
				icon = IDI_USER_LEAVE;
				messagePart2 = TmGetTString(IDS_GROUP_SELF_REMOVE);
				break;
			}
			else {
				icon = IDI_USER_LEAVE;
				messagePart2 = TmGetTString(IDS_GROUP_REMOVE_1);
				messagePart3 = TmGetTString(IDS_GROUP_REMOVE_2);
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
			messagePart2 = TmGetTString(IDS_CHANNEL_RENAME);
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

		case MessageType::NO_NOTIFICATIONS:
			messagePart1 = TmGetTString(IDS_NO_NOTIFICATIONS);
			messagePart2 = TEXT("");
			break;
	}
}

void MessageList::ConfirmOpenLink(const std::string& link)
{
	bool trust = GetLocalSettings()->CheckTrustedDomain(link);

	if (!trust)
	{
		static TCHAR buffer[8192];
		LPTSTR tstr = ConvertCppStringToTString(link);
		WAsnprintf(buffer, _countof(buffer), TmGetTString(IDS_LINK_CONFIRM), tstr);
		buffer[_countof(buffer) - 1] = 0;
		free(tstr);

		if (MessageBoxHooked(g_Hwnd, buffer, TmGetTString(IDS_HOLD_UP_CONFIRM), MB_ICONWARNING | MB_OKCANCEL, IDOK, TmGetTString(IDS_EXCITED_YES)) != IDOK)
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
	const int replyOffset = item.m_replyHeight + ScaleByDPI(5);
	RECT rcReply = rc;
	rcReply.bottom = rcReply.top + item.m_replyHeight;

	auto& refMsg = *item.m_msg.m_pReferencedMessage;
	const bool isActionMessage = IsActionMessage(refMsg.m_type);
	const bool isCompact = IsCompact();

	Snowflake authorID = refMsg.m_author_snowflake;

	// Draw the corner piece
	const int offset2 = isCompact ? 0 : pfpOffset;
	rcReply.left -= offset2;

	const int iconSize    = ScaleByDPI(32);
	const int iconOffset = (GetProfilePictureSize() - ScaleByDPI(2)) / 2;
	const int offset3 = isCompact ? iconSize : 0;

	if (isCompact) {
		HRGN rgn = CreateRectRgn(
			rcReply.left + iconOffset,
			rcReply.bottom + ScaleByDPI(5) - iconSize,
			rcReply.left + iconOffset + offset2 + offset3 - ScaleByDPI(20),
			rcReply.bottom + ScaleByDPI(5)
		);
		SelectClipRgn(hdc, rgn);
		DrawIconEx(hdc, rcReply.left + iconOffset, rcReply.bottom + ScaleByDPI(5) - iconSize, g_ReplyPieceIcon, iconSize, iconSize, 0, NULL, DI_COMPAT | DI_NORMAL);
		SelectClipRgn(hdc, NULL);
		DeleteRgn(rgn);
	}
	else {
		DrawIconEx(hdc, rcReply.left + iconOffset, rcReply.bottom + ScaleByDPI(5) - iconSize, g_ReplyPieceIcon, iconSize, iconSize, 0, NULL, DI_COMPAT | DI_NORMAL);
	}

	rcReply.left += offset2 + offset3;

	COLORREF nameClr = CLR_NONE;
	if (authorID)
	{
		if (!isCompact && !isActionMessage)
		{
			int pfpSize     = ScaleByDPI(16);
			int pfpBordSize = MulDiv(GetProfileBorderRenderSize(), ScaleByDPI(16), GetProfilePictureSize());
			int pfpBordOffX = MulDiv(ScaleByDPI(6), ScaleByDPI(16), GetProfilePictureSize());
			int pfpBordOffY = MulDiv(ScaleByDPI(4), ScaleByDPI(16), GetProfilePictureSize());
			int pfpX = rcReply.left;
			int pfpY = rcReply.top + (rcReply.bottom - rcReply.top - pfpSize) / 2;
			DrawIconEx(hdc, pfpX - pfpBordOffX, pfpY - pfpBordOffY, g_ProfileBorderIcon, pfpBordSize, pfpBordSize, 0, NULL, DI_COMPAT | DI_NORMAL);
			bool hasAlpha = false;
			GetAvatarCache()->AddImagePlace(refMsg.m_avatar, eImagePlace::AVATARS, refMsg.m_avatar, refMsg.m_author_snowflake);
			HBITMAP hbm = GetAvatarCache()->GetBitmap(refMsg.m_avatar, hasAlpha);

			RECT& raRect = item.m_refAvatarRect;
			raRect.left   = pfpX;
			raRect.top    = pfpY;
			raRect.right  = raRect.left + pfpSize;
			raRect.bottom = raRect.top  + pfpSize;
			DrawBitmap(hdc, hbm, pfpX, pfpY, NULL, CLR_NONE, pfpSize, 0, hasAlpha);
			rcReply.left += pfpSize + ScaleByDPI(2);
		}
		else
		{
			SetRectEmpty(&item.m_refAvatarRect);
		}

		if (!refMsg.m_webhook_id) {
			Profile* pf = GetProfileCache()->LookupProfile(authorID, "", "", "", false);
			if (pf)
				nameClr = GetNameColor(pf, m_guildID);
		}
	}

	if (nameClr == CLR_NONE)
		nameClr = InvertIfNeeded(GetSysColor(COLOR_WINDOWTEXT));

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
			m_channelID,
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

void MessageList::RequestMarkRead()
{
	GetDiscordInstance()->RequestAcknowledgeChannel(m_channelID);

	GetNotificationManager()->MarkNotificationsRead(m_channelID);
}

void MessageList::DrawMessage(HDC hdc, MessageItem& item, RECT& msgRect, RECT& clientRect, RECT& paintRect, DrawingContext& mddc, COLORREF chosenBkColor, bool bDrawNewMarker)
{
	LPCTSTR strAuth = item.m_author;
	LPCTSTR strDate = item.m_date;
	LPCTSTR strEditDate = item.m_dateEdited;
	int height = item.m_height, authheight = item.m_authHeight, authwidth = item.m_authWidth;
	const eMessageStyle mStyle = GetLocalSettings()->GetMessageStyle();

	bool isFlashed = (m_emphasizedMessage == item.m_msg.m_snowflake) && (m_flash_counter % 2 != 0);

	COLORREF chosenTextColor = GetSysColor(COLOR_WINDOWTEXT);
	if (item.m_msg.m_bRead)
		chosenTextColor = GetSysColor(COLOR_3DSHADOW);

	COLORREF textColor = InvertIfNeeded(chosenTextColor), bkgdColor = CLR_NONE;

	RECT rc = msgRect;
	if (!m_firstShownMessage && rc.bottom > clientRect.top && !item.m_msg.IsLoadGap())
		m_firstShownMessage = item.m_msg.m_snowflake;

	const int pfpOffset = ScaleByDPI(PROFILE_PICTURE_SIZE_DEF + 12);
	const bool isChainBegin = item.m_placeInChain == 0;
	const bool isChainCont = item.m_placeInChain != 0;

	rc.left += ScaleByDPI(10);
	rc.right -= ScaleByDPI(10);
	rc.bottom -= ScaleByDPI(10);
	if (isChainBegin)
		rc.top += ScaleByDPI(10);

	if (!m_bManagedByOwner && item.m_bIsDateGap && isChainBegin)
	{
		RECT dgRect = msgRect;
		dgRect.bottom = dgRect.top + DATE_GAP_HEIGHT;

		rc.top += DATE_GAP_HEIGHT;
		msgRect.top += DATE_GAP_HEIGHT;

		LPTSTR strDateGap = ConvertCppStringToTString("  " + item.m_msg.m_dateOnly + "  ");

		COLORREF clrText = InvertIfNeeded(bDrawNewMarker ? NEW_MARKER_COLOR : GetSysColor(COLOR_GRAYTEXT));
		COLORREF oldTextClr = SetTextColor(hdc, clrText);
		COLORREF oldPenClr = ri::SetDCPenColor(hdc, clrText);

		if (mStyle != MS_IMAGE)
		{
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
		}
		else
		{
			FillRect(hdc, &dgRect, m_backgroundBrush);
		}

		RECT rcMeasure = dgRect;
		DrawText(hdc, strDateGap, -1, &rcMeasure, DT_CALCRECT | DT_NOPREFIX);
		int width = rcMeasure.right - rcMeasure.left;

		int l1 = dgRect.left + ScaleByDPI(10);
		int r1 = dgRect.right / 2 - width / 2 - 1;
		int l2 = dgRect.right / 2 + width / 2 + 1;
		int r2 = dgRect.right - ScaleByDPI(10) - 1;

		HGDIOBJ oldFont = NULL;
		if (bDrawNewMarker)
			oldFont = SelectObject(hdc, g_AuthorTextFont);

		POINT old;
		HGDIOBJ oldob = SelectObject(hdc, GetStockPen(DC_PEN));
		MoveToEx(hdc, l1, (dgRect.top + dgRect.bottom) / 2, &old);
		LineTo(hdc, r1, (dgRect.top + dgRect.bottom) / 2);
		MoveToEx(hdc, l2, (dgRect.top + dgRect.bottom) / 2, NULL);
		LineTo(hdc, r2, (dgRect.top + dgRect.bottom) / 2);
		MoveToEx(hdc, old.x, old.y, NULL);
		SelectObject(hdc, oldob);

		DrawText(hdc, strDateGap, -1, &dgRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
		free(strDateGap);

		if (bDrawNewMarker)
		{
			int iconSize = ScaleByDPI(32);
			if (iconSize > 32) iconSize = 48;
			if (iconSize < 32) iconSize = 32;
			HICON hic = (HICON)LoadImage(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_NEW_INLINE)), IMAGE_ICON, iconSize, iconSize, LR_SHARED | LR_CREATEDIBSECTION);
			DrawIconEx(hdc, dgRect.right - iconSize, dgRect.top + (dgRect.bottom - dgRect.top - iconSize) / 2 + 1, hic, iconSize, iconSize, 0, NULL, DI_COMPAT | DI_NORMAL);
		}

		if (oldFont) SelectObject(hdc, oldFont);
		SetTextColor(hdc, oldTextClr);
		ri::SetDCPenColor(hdc, oldPenClr);
		bDrawNewMarker = false;
	}

	if (isFlashed) {
		bkgdColor = GetDarkerBackgroundColor();
		COLORREF crOld = ri::SetDCBrushColor(hdc, bkgdColor);
		FillRect(hdc, &msgRect, GetStockBrush(DC_BRUSH));
		ri::SetDCBrushColor(hdc, crOld);
	}
	switch (mStyle)
	{
		case MS_3DFACE: {
			RECT rect2 = msgRect;
			int edgeFlags = BF_LEFT | BF_RIGHT | BF_BOTTOM;
			if (!isFlashed)   edgeFlags |= BF_MIDDLE;
			if (isChainBegin) edgeFlags |= BF_TOP;
			else rect2.top -= 4;

			if (!isFlashed)
				bkgdColor = GetSysColor(COLOR_3DFACE);

			if (item.m_msg.m_type != MessageType::CHANNEL_HEADER) {
				DrawEdge(hdc, &rect2, BDR_RAISED, edgeFlags);
			}
			else if (edgeFlags & BF_MIDDLE) {
				FillRect(hdc, &rect2, GetSysColorBrush(COLOR_3DFACE));
			}
			break;
		}
		case MS_FLAT: {
			if (!isFlashed) {
				bkgdColor = GetSysColor(COLOR_3DFACE);
				FillRect(hdc, &msgRect, GetSysColorBrush(COLOR_3DFACE));
			}
			break;
		}
		case MS_FLATBR: {
			if (!isFlashed) {
				bkgdColor = GetSysColor(COLOR_WINDOW);
				FillRect(hdc, &msgRect, GetSysColorBrush(COLOR_WINDOW));
			}
			break;
		}
		case MS_GRADIENT:
		{
			if (isFlashed)
				break;

			COLORREF c1, c2;
			c1 = GetSysColor(COLOR_WINDOW); // high
			c2 = GetSysColor(COLOR_3DFACE); // low
			bool swapped = false;

			if (c1 < c2) {
				COLORREF tmp = c1;
				c1 = c2;
				c2 = tmp;
				swapped = true;
			}

			// note: Intentionally perform the swap again
			if (item.m_msg.m_type == MessageType::CHANNEL_HEADER) {
				COLORREF tmp = c1;
				c1 = c2;
				c2 = tmp;
				swapped = !swapped;
			}

			bkgdColor = c2;

			if (isChainCont) {
				// Just render a flat background using the bottom color
				FillRect(hdc, &msgRect, GetSysColorBrush(swapped ? COLOR_WINDOW : COLOR_3DFACE));
				break;
			}

			TRIVERTEX vt[2] = { {}, {} };
			vt[0].x = msgRect.left;
			vt[0].y = msgRect.top;
			vt[1].x = msgRect.right;
			vt[1].y = msgRect.bottom;
			vt[0].Red = (c1 & 0xFF) << 8;
			vt[0].Green = ((c1 >> 8) & 0xFF) << 8;
			vt[0].Blue = ((c1 >> 16) & 0xFF) << 8;
			vt[0].Alpha = 0;
			vt[1].Red = int(c2 & 0xFF) << 8;
			vt[1].Green = int((c2 >> 8) & 0xFF) << 8;
			vt[1].Blue = int((c2 >> 16) & 0xFF) << 8;
			vt[1].Alpha = 0;

			GRADIENT_RECT grect;
			grect.UpperLeft = 0;
			grect.LowerRight = 1;

			ri::GradientFill(hdc, vt, 2, &grect, 1, GRADIENT_FILL_RECT_V);
			break;
		}
	}

	COLORREF crOldTextColor = SetTextColor(hdc, textColor);
	COLORREF crOldBkgdColor = SetBkColor  (hdc, bkgdColor);

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
			m_channelID,
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
	int oldMode = 0;
	bool restoreOldMode = false;

	if (isActionMessage)
	{
		if (item.m_msg.m_type == MessageType::CHANNEL_HEADER)
		{
			if (Supports32BitIcons() && GetDeviceCaps(hdc, BITSPIXEL) >= 16)
			{
				restoreOldMode = true;
				oldMode = SetBkMode(hdc, TRANSPARENT);

				HICON hics[5];
				int sz = ScaleByDPI(64);

				for (int i = 0; i < 5; i++) {
					hics[i] = (HICON)LoadImage(g_hInstance, MAKEINTRESOURCE(IDI_HEADER_1 + i), IMAGE_ICON, sz, sz, LR_SHARED | LR_CREATEDIBSECTION);
				}

				// Render on the left side.
				int x = 0;
				for (int i = 0; i < 2; i++) {
					DrawIconEx(hdc, x, rc.top, hics[i], sz, sz, 0, NULL, DI_NORMAL | DI_COMPAT);
					x += sz;
				}

				x = rc.right;
				for (int i = 4; i >= 2; i--) {
					x -= sz;
					DrawIconEx(hdc, x, rc.top, hics[i], sz, sz, 0, NULL, DI_NORMAL | DI_COMPAT);
				}
			}

			rc.top = rc.bottom - item.m_authHeight;
		}

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

	if (item.m_msg.m_pReferencedMessage != nullptr && !isActionMessage && isChainBegin)
		replyOffset = DrawMessageReply(hdc, item, rc);

	rc.top += replyOffset;

	if (isChainBegin)
	{
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
		{
			DrawText(hdc, strAuth, -1, &rc, DT_NOPREFIX | DT_NOCLIP);
			if (item.m_msg.m_bIsAuthorBot)
			{
				int sm = GetSystemMetrics(SM_CXSMICON);
				HICON hic = (HICON)LoadImage(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_BOT)), IMAGE_ICON, sm, sm, LR_SHARED);
				DrawIconEx(hdc, rc.left + auth_wid + ScaleByDPI(4), rc.top + (auth_hei - sm) / 2, hic, sm, sm, 0, NULL, DI_COMPAT | DI_NORMAL);
			}
		}

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

		if (!bIsCompact)
			rc.top += authheight;
	}

	// draw the message text
	SelectObject(hdc, g_MessageTextFont);

	if (isActionMessage)
	{
		item.m_avatarRect = { -10000, -10000, -9999, -9999 };

		RECT rca = rc;
		if (!bIsCompact)
			rca.top -= authheight;

		int offs = 0;
		if (icon) {
			int iconY = rca.top + (rca.bottom - rca.top) / 2 - actionIconSize / 2;
			HICON hicon = (HICON) LoadImage(g_hInstance, MAKEINTRESOURCE(icon), IMAGE_ICON, actionIconSize, actionIconSize, LR_SHARED | LR_CREATEDIBSECTION);
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
		if (!bIsCompact && isChainBegin)
			rc.top += ScaleByDPI(5);

		RECT rcMsg = rc;
		rcMsg.bottom -= item.m_attachHeight + item.m_embedHeight;
		if (item.m_attachHeight)
			rcMsg.bottom -= ATTACHMENT_GAP;
		if (item.m_embedHeight)
			rcMsg.bottom -= ScaleByDPI(5);

		item.m_message.Layout(&mddc, Rect(W32RECT(rcMsg)), bIsCompact ? (item.m_authWidth + item.m_dateWidth + ScaleByDPI(10)) : 0);
		
		Rect ext = item.m_message.GetExtent();
		int height = std::max(item.m_authHeight, ext.Height());
		int offsetY = ((rcMsg.bottom - rcMsg.top) - ext.Height()) / 2;
		
		item.m_messageRect = rcMsg;

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
			else if (item.m_msg.m_type == MessageType::SENDING_MESSAGE)
				oldTextColor = SetTextColor(hdc, GetSysColor(COLOR_GRAYTEXT));
			
			item.m_message.Draw(&mddc, offsetY);

			if (oldBkColor != CLR_NONE)
				SetBkColor(hdc, oldBkColor);
			if (oldTextColor != CLR_NONE)
				SetTextColor(hdc, oldTextColor);

			COLORREF windowTextColor = InvertIfNeeded(GetSysColor(COLOR_WINDOWTEXT));
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

		if (bIsCompact)
			item.m_messageRect.bottom = std::max(item.m_messageRect.bottom, item.m_messageRect.top + item.m_authHeight);

		assert(ext.left != 0);

		if (!bIsCompact && isChainBegin)
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
				bool hasAlpha = false;
				GetAvatarCache()->AddImagePlace(item.m_msg.m_avatar, eImagePlace::AVATARS, item.m_msg.m_avatar, item.m_msg.m_author_snowflake);
				HBITMAP hbm = GetAvatarCache()->GetBitmap(item.m_msg.m_avatar, hasAlpha);
				DrawBitmap(hdc, hbm, pfRect.left, pfRect.top, &pfRect, CLR_NONE, GetProfilePictureSize(), 0, hasAlpha);
			}
		}
		else
		{
			item.m_avatarRect = { 0, 0, 0, 0 };
		}
	}

	if (restoreOldMode)
		SetBkMode(hdc, oldMode);

	// draw available embeds, if any:
	RECT embedRect = item.m_messageRect;
	auto& embedVec = item.m_embedData;
	size_t sz = embedVec.size();

	if (!GetLocalSettings()->ShowEmbedContent())
		sz = 0;

	embedRect.right = msgRect.right - ScaleByDPI(10);
	embedRect.top = embedRect.bottom;

	if (bIsCompact) embedRect.left = rc.left;

	for (size_t i = 0; i < sz; i++)
	{
		auto& eitem = embedVec[i];

		if (eitem.m_size.cx == 0 || eitem.m_size.cy == 0)
			eitem.Measure(hdc, embedRect, bIsCompact);

		eitem.Draw(hdc, embedRect, this);

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

	if (sz)
		embedRect.bottom += ScaleByDPI(5);

	// draw available attachments, if any:
	RECT attachRect = embedRect;
	auto& attachVec = item.m_msg.m_attachments;
	auto& attachItemVec = item.m_attachmentData;
	sz = attachVec.size();

	attachRect.right   = msgRect.right - ScaleByDPI(10);
	attachRect.top     = attachRect.bottom;

	if (bIsCompact) attachRect.left = rc.left;

	for (size_t i = 0; i < sz; i++)
	{
		auto& attach = attachVec[i];
		auto& attachItem = attachItemVec[i];
		switch (attach.m_contentType)
		{
			case ContentType::PNG:
			case ContentType::GIF:
			case ContentType::JPEG:
			case ContentType::WEBP:
			{
				if (GetLocalSettings()->ShowAttachmentImages())
				{
					DrawImageAttachment(hdc, paintRect, attachItem, attachRect);
					break;
				}
				// FALLTHROUGH
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

	SetTextColor(hdc, crOldTextColor);
	SetBkColor  (hdc, crOldBkgdColor);

	free((void*) freedString);

	if (bDrawNewMarker)
	{
		// Draw the unread marker.
		HGDIOBJ oldPen = SelectObject(hdc, GetStockObject(DC_PEN));
		COLORREF oldClr = ri::SetDCPenColor(hdc, RGB(255, 0, 0));

		POINT ptOld{};
		MoveToEx(hdc, msgRect.left, msgRect.top, &ptOld);
		LineTo(hdc, msgRect.right, msgRect.top);
		MoveToEx(hdc, ptOld.x, ptOld.y, NULL);

		int iconSize = ScaleByDPI(32);
		if (iconSize > 32) iconSize = 48;
		if (iconSize < 32) iconSize = 32;
		HICON hic = (HICON)LoadImage(g_hInstance, MAKEINTRESOURCE(DMIC(IDI_NEW)), IMAGE_ICON, iconSize, iconSize, LR_SHARED | LR_CREATEDIBSECTION);
		DrawIconEx(hdc, msgRect.right - iconSize, msgRect.top, hic, iconSize, iconSize, 0, NULL, DI_COMPAT | DI_NORMAL);

		ri::SetDCPenColor(hdc, oldClr);
		SelectObject(hdc, oldPen);
	}
}

void MessageList::PaintBackground(HDC hdc, RECT& paintRect, RECT& rcClient)
{
	if (GetLocalSettings()->GetMessageStyle() != MS_IMAGE)
		return;

	if (!m_backgroundBrush)
		return;

	FillRect(hdc, &paintRect, m_backgroundBrush);

	if (m_backgroundImage)
	{
		BITMAP bm{};
		GetObject(m_backgroundImage, (int)sizeof bm, &bm);

		int x = 0, y = 0;
		// x axis
		switch (GetLocalSettings()->GetImageAlignment())
		{
			case ALIGN_UPPER_LEFT:
			case ALIGN_MIDDLE_LEFT:
			case ALIGN_LOWER_LEFT:  x = 0; break;
			case ALIGN_UPPER_CENTER:
			case ALIGN_CENTER:
			case ALIGN_LOWER_CENTER:  x = rcClient.left + (rcClient.right - rcClient.left - bm.bmWidth) / 2; break;
			case ALIGN_UPPER_RIGHT:
			case ALIGN_MIDDLE_RIGHT:
			case ALIGN_LOWER_RIGHT:  x = rcClient.right - bm.bmWidth; break;
		}
		// y axis
		switch (GetLocalSettings()->GetImageAlignment())
		{
			case ALIGN_UPPER_LEFT:
			case ALIGN_UPPER_CENTER:
			case ALIGN_UPPER_RIGHT:  y = 0; break;
			case ALIGN_MIDDLE_LEFT:
			case ALIGN_CENTER:
			case ALIGN_MIDDLE_RIGHT: y = rcClient.top + (rcClient.bottom - rcClient.top - bm.bmHeight) / 2; break;
			case ALIGN_LOWER_LEFT:
			case ALIGN_LOWER_CENTER:
			case ALIGN_LOWER_RIGHT:  y = rcClient.bottom - bm.bmHeight; break;
		}

		RECT rcImg = { x, y, x + bm.bmWidth, y + bm.bmHeight };
		RECT dummy;
		if (!IntersectRect(&dummy, &rcImg, &paintRect))
			return;

		DrawBitmap(hdc, m_backgroundImage, x, y, NULL, CLR_NONE, 0, 0, m_bBackgroundHasAlpha);
	}
}

void MessageList::Paint(HDC hdc, RECT& paintRect)
{
	RECT rect = {};
	GetClientRect(m_hwnd, &rect);
	PaintBackground(hdc, paintRect, rect);
	int windowHeight = rect.bottom - rect.top;
	int ScrollHeight = 0;
	SCROLLINFO si;
	si.cbSize = sizeof(si);
	si.fMask = SIF_POS | SIF_RANGE;
	GetScrollInfo(m_hwnd, SB_VERT, &si);
	ScrollHeight = si.nPos;

	RECT msgRect = rect;
	msgRect.top -= ScrollHeight;

	if (m_total_height < windowHeight && !m_bIsTopDown) {
		msgRect.top += windowHeight - m_total_height;
	}

	msgRect.bottom = msgRect.top;

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
		case MS_IMAGE: {
			oldBkMode = SetBkMode(hdc, TRANSPARENT);
			oldBkModeSet = true;
			chosenBkColor = m_backgroundColor;
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
	mddc.SetInvertTextColor(InvertIfNeeded(GetSysColor(COLOR_WINDOWTEXT)));

	Snowflake lastDrawnMessage = 0;
	Snowflake lastKnownMessage = 0;
	bool isLastKnownMessageGap = false;

	int unreadMarkerState = 0;
	m_firstShownMessage = 0;
	for (std::list<MessageItem>::iterator iter = m_messages.begin();
		iter != m_messages.end();
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
			iter->Update(m_guildID);

		HGDIOBJ gdiObj = SelectObject(hdc, g_MessageTextFont);

		// measure the message text
		msgRect.bottom = msgRect.top + iter->m_height;
		iter->m_rect = msgRect;

		lastKnownMessage = iter->m_msg.m_snowflake;
		isLastKnownMessageGap = iter->m_msg.IsLoadGap();

		bool bDraw = msgRect.top <= rect.bottom && msgRect.bottom > rect.top;
		bool bDrawNewMarker = false;

		// Observation: We increment unreadMarkerState in this if condition.  If the conditions before aren't true, the variable
		// isn't incremented. The variable is incremented regardless of bDraw. We just need it to increase when we stumble across
		// the oldest unread message to know whether and where we should draw the unread marker.
		if (m_previousLastReadMessage &&
			iter->m_msg.m_snowflake > m_previousLastReadMessage &&
			!iter->m_msg.IsLoadGap() &&
			unreadMarkerState++ == 0 &&
			bDraw) {
			bDrawNewMarker = true;
		}

		if (bDraw)
		{
			DrawMessage(hdc, *iter, msgRect, rect, paintRect, mddc, chosenBkColor, bDrawNewMarker);
			lastDrawnMessage = iter->m_msg.m_snowflake;
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

	Channel* pChan = GetDiscordInstance()->GetChannel(m_channelID);

	if (pChan &&
		lastDrawnMessage == lastKnownMessage &&
		!isLastKnownMessageGap &&
		pChan->m_lastSentMsg != pChan->m_lastViewedMsg &&
		m_bAcknowledgeNow) {
		RequestMarkRead();
		pChan->m_lastViewedMsg = pChan->m_lastSentMsg;
	}

	if (oldBkModeSet)
		SetBkMode(hdc, oldBkMode);

	if (oldBkColor != CLR_NONE)
		SetBkColor(hdc, oldBkColor);
}

LRESULT CALLBACK MessageList::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	MessageList* pThis = (MessageList*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if (!pThis && uMsg != WM_NCCREATE)
		return DefWindowProc(hWnd, uMsg, wParam, lParam);

	SCROLLINFO si{};
	switch (uMsg)
	{
		case WM_NCCREATE:
		{
			CREATESTRUCT* strct = (CREATESTRUCT*)lParam;

			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)strct->lpCreateParams);

			break;
		}
		case WM_DESTROY:
		{
			SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR) NULL);
			break;
		}
		case WM_TIMER:
		{
			// Flash timer
			Snowflake flashedMsg = pThis->m_emphasizedMessage;
			if (!flashedMsg)
				break;

			pThis->m_flash_counter--;
			if (pThis->m_flash_counter <= 0) {
				pThis->m_emphasizedMessage = 0;
				KillTimer(hWnd, pThis->m_flash_timer);
				pThis->m_flash_timer = 0;
			}

			auto itMsg = pThis->FindMessage(flashedMsg);
			if (itMsg == pThis->m_messages.end())
				break;

			InvalidateRect(hWnd, &itMsg->m_rect, pThis->MayErase());
			break;
		}
		case WM_DROPFILES:
		{
			TCHAR chr[4096];
			HDROP hDrop = (HDROP)wParam;

			int amount = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);

			if (amount != 1)
			{
				MessageBox(g_Hwnd, TmGetTString(IDS_CANT_UPLOAD_SEVERAL), TmGetTString(IDS_PROGRAM_NAME), MB_ICONWARNING | MB_OK);
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
				int rpsUnused = 0;
				if (oldWidth != GET_X_LPARAM(lParam)) {
					si.cbSize = sizeof(si);
					si.fMask = SIF_TRACKPOS | SIF_POS | SIF_RANGE | SIF_PAGE;
					GetScrollInfo(hWnd, SB_VERT, &si);

					bool retrack = si.nPos < si.nMax - int(si.nPage) - 10;
					int position = 0, ypos = 0, ypos2 = 0;
					Snowflake sf = 0;

					if (retrack)
					{
						position = si.nPos;

						// find the old offset from the top of the messages
						for (auto& msg : pThis->m_messages) {
							if (ypos + msg.m_height >= position) {
								sf = msg.m_msg.m_snowflake;
								break;
							}
							ypos += msg.m_height;
						}
					}

					// recalculate
					pThis->RecalcMessageSizes(true, rpsUnused, 0);

					if (retrack) {
						// find the new offset from the top of the messages
						for (auto& msg : pThis->m_messages) {
							if (msg.m_msg.m_snowflake == sf)
								break;

							ypos2 += msg.m_height;
						}

						// recalculate the range and page
						int th = pThis->m_total_height - 1;
						if (th <= 0)
							th = 0;

						si.nMin = 0;
						si.nMax = th;
						si.nPage = newHeight;

						// scroll to the old position plus the offset
						si.nTrackPos = position + ypos2 - ypos;
						if (si.nTrackPos < si.nMin) si.nTrackPos = si.nMin;
						if (si.nTrackPos > si.nMax) si.nTrackPos = si.nMax;
						wParam = SB_THUMBTRACK;
						goto _lbl;
					}
				}
				else {
					pThis->UpdateScrollBar(0, oldHeight - newHeight, false, true);
				}

				(void) rpsUnused;
				return 0;
			}

			break;
		}
		case WM_MOUSEWHEEL:
		{
			short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);

			si.cbSize = sizeof(si);
			si.fMask = SIF_TRACKPOS | SIF_POS | SIF_RANGE;
			GetScrollInfo(pThis->m_hwnd, SB_VERT, &si);
			si.nTrackPos = si.nPos - (zDelta / 3);
			if (si.nTrackPos < si.nMin) si.nTrackPos = si.nMin;
			if (si.nTrackPos > si.nMax) si.nTrackPos = si.nMax;
			SetScrollInfo(pThis->m_hwnd, SB_VERT, &si, false);

			wParam = SB_THUMBTRACK;
			goto _lbl;
		}
		case WM_GESTURE:
		{
			HandleGestureMessage(hWnd, uMsg, wParam, lParam, 3.0f);
			break;
		}
		case WM_VSCROLL:
		{
			si.cbSize = sizeof(si);
			si.fMask = SIF_ALL;

			GetScrollInfo(pThis->m_hwnd, SB_VERT, &si);

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

			SetScrollInfo(pThis->m_hwnd, SB_VERT, &si, true);
			GetScrollInfo(pThis->m_hwnd, SB_VERT, &si);

			diffUpDown = si.nPos - pThis->m_oldPos;
			pThis->m_oldPos = si.nPos;

			pThis->Scroll(diffUpDown);

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

			if (IsClientSideMessage(pRCMsg->m_msg.m_type)) break;

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
			bool maySpeak  = !IsActionMessage(pRCMsg->m_msg.m_type) && !pRCMsg->m_msg.m_message.empty();

#ifdef OLD_WINDOWS
			EnableMenuItem(menu, ID_DUMMYPOPUP_DELETEMESSAGE, mayDelete ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(menu, ID_DUMMYPOPUP_EDITMESSAGE,   mayEdit   ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(menu, ID_DUMMYPOPUP_PINMESSAGE,    mayPin    ? MF_ENABLED : MF_GRAYED);
			EnableMenuItem(menu, ID_DUMMYPOPUP_SPEAKMESSAGE,  maySpeak  ? MF_ENABLED : MF_GRAYED);
#else
			if (!mayDelete) RemoveMenu(menu, ID_DUMMYPOPUP_DELETEMESSAGE, MF_BYCOMMAND);
			if (!mayEdit)   RemoveMenu(menu, ID_DUMMYPOPUP_EDITMESSAGE,   MF_BYCOMMAND);
			if (!mayPin)    RemoveMenu(menu, ID_DUMMYPOPUP_PINMESSAGE,    MF_BYCOMMAND);
			if (!maySpeak)  RemoveMenu(menu, ID_DUMMYPOPUP_SPEAKMESSAGE,  MF_BYCOMMAND);
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
			Snowflake messageBeforeRightClickedMessage = 0;

			for (auto iter = pThis->m_messages.rbegin(); iter != pThis->m_messages.rend(); ++iter)
			{
				if (iter->m_msg.m_snowflake == pThis->m_rightClickedMessage)
				{
					pMsg = &(*iter);

					++iter;
					if (iter != pThis->m_messages.rend())
						messageBeforeRightClickedMessage = iter->m_msg.m_snowflake;
					break;
				}
			}

			pThis->m_rightClickedMessage = 0;

			if (!pMsg) break;

			int test = GetMenuItemID((HMENU)lParam, wParam);

			switch (test)
			{
				case ID_DUMMYPOPUP_MARKUNREAD:
				{
					if (messageBeforeRightClickedMessage == 0) {
						uint64_t timeStamp = rightClickedMessage >> 22;
						timeStamp--; // in milliseconds since Discord epoch - irrelevant because we just want to take ONE millisecond
						messageBeforeRightClickedMessage = timeStamp << 22;
					}

					assert(messageBeforeRightClickedMessage < rightClickedMessage);

					GetDiscordInstance()->RequestAcknowledgeMessages(pThis->m_channelID, messageBeforeRightClickedMessage);

					// Block acknowledgements until we switch to the channel again.
					pThis->m_bAcknowledgeNow = false;
					break;
				}
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
						// TODO
					}

					free((void*)xstr);
					break;
				}
				case ID_DUMMYPOPUP_SPEAKMESSAGE:
				{
					if (IsActionMessage(pMsg->m_msg.m_type))
						break;
					
					std::string action = " said ";
					if (pMsg->m_msg.m_type == MessageType::REPLY &&
						pMsg->m_msg.m_pReferencedMessage != nullptr)
						action = " replied to " + pMsg->m_msg.m_pReferencedMessage->m_author + " ";

					TextToSpeech::Speak(pMsg->m_msg.m_author + action + GetDiscordInstance()->ReverseMentions(pMsg->m_msg.m_message, pThis->m_guildID, true));
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

					if (!msg->m_msg.IsWebHook())
						ProfilePopout::Show(authorSF, pThis->m_guildID, rect.left + msg->m_authorRect.right + 10, rect.top + msg->m_authorRect.top);
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
						pThis->OpenInteractable(&intd, &*msg);
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

			if (!MayErase())
			{
				refreshRect.bottom = pullDownRect.bottom + height;
				InvalidateRect(hWnd, NULL, FALSE);
				break;
			}

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

			SCROLLINFO si;
			ZeroMemory(&si, sizeof(SCROLLINFO));
			si.cbSize = sizeof(SCROLLINFO);
			si.fMask = SIF_PAGE;
			GetScrollInfo(hWnd, SB_VERT, &si);

			InvalidateRect(hWnd, &refreshRect, pThis->MayErase() && int(si.nPage) > pThis->m_total_height);

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
		case WM_ERASEBKGND:
		{
			if (GetLocalSettings()->GetMessageStyle() == MS_IMAGE)
				return 1;

			break;
		}
		case WM_PAINT:
		{
			PAINTSTRUCT ps = {};
			HDC hdc = BeginPaint(hWnd, &ps);
			RECT &paintRect = ps.rcPaint;

			if (GetLocalSettings()->GetMessageStyle() == MS_IMAGE)
			{
				// Create a DC that we blit to, and then only the finished result goes on screen.
				RECT rcClient{};
				GetClientRect(hWnd, &rcClient);

				int prWidth = paintRect.right - paintRect.left, prHeight = paintRect.bottom - paintRect.top;
				
				// TODO: Surely there's a better way. Surely there's a way to only create paintRect.width * paintRect.height
				// sized bitmap, and somehow let Windows know that that's the offset we want. But I don't know of that way,
				// so this'll do for now.
				HBITMAP hbm = CreateCompatibleBitmap(hdc, prWidth, prHeight);
				HDC hdcMem = CreateCompatibleDC(hdc);
				HGDIOBJ old = SelectObject(hdcMem, hbm);

				POINT oldOrg{};
				SIZE oldSize{};
				SetViewportOrgEx(hdcMem, -paintRect.left, -paintRect.top, &oldOrg);
				//SetViewportExtEx(hdcMem, rcClient.right-rcClient.left,rcClient.bottom-rcClient.top, &oldSize);

				// Paint on this object
				pThis->Paint(hdcMem, paintRect);

				SetViewportOrgEx(hdcMem, oldOrg.x, oldOrg.y, NULL);
				//SetViewportExtEx(hdcMem, oldSize.cx, oldSize.cy, NULL);

				// Ok, now flush to the main screen
				BitBlt(hdc, paintRect.left, paintRect.top, prWidth, prHeight, hdcMem, 0, 0, SRCCOPY);

				// And dispose of the evidence
				DeleteDC(hdcMem);
				DeleteBitmap(hbm);
			}
			else
			{
				pThis->Paint(hdc, paintRect);
			}

			EndPaint(hWnd, &ps);
			break;
		}
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void MessageList::InitializeClass()
{
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

	ReloadBackground();

	if (mode == MS_IMAGE)
		SetClassLongPtr(m_hwnd, GCLP_HBRBACKGROUND, (LONG_PTR) m_backgroundBrush);
	else
		SetClassLongPtr(m_hwnd, GCLP_HBRBACKGROUND, (LONG_PTR) GetSysColorBrush(bru));
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
		GetClientRect(m_hwnd, &rcClient);

		SCROLLINFO si{};
		si.cbSize = sizeof(SCROLLINFO);
		si.fMask = SIF_RANGE;
		GetScrollInfo(m_hwnd, SB_VERT, &si);

		int yOffs = (rcClient.bottom - rcClient.top) / 2 - mi->m_height / 2;
		y -= yOffs;

		if (y < si.nMin)
			y = si.nMin;
		if (y > si.nMax)
			y = si.nMax;

		// Just scroll there
		si.fMask = SIF_POS;
		si.nPos = y;
		SetScrollInfo(m_hwnd, SB_VERT, &si, true);

		FlashMessage(mi->m_msg.m_snowflake);
		SendMessage(m_hwnd, WM_VSCROLL, SB_ENDSCROLL, 0);
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
		if (msg.m_msg.m_author_snowflake == 0 || msg.m_msg.IsWebHook())
			continue;

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
			msg.m_msg.m_bIsAuthorBot,
			msg.m_messageRect,
			height,
			authHeight,
			authWidth,
			dateWidth,
			replyMsgWidth,
			replyAuthWidth,
			replyHeight,
			msg.m_placeInChain
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

void MessageList::UpdateScrollBar(int addToHeight, int diffNow, bool toStart, bool update, int offsetY, bool addingMessage)
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

	GetScrollInfo(m_hwnd, SB_VERT, &scrollInfo);

	bool scroll = true;

	// Check if the current position is at the bottom.
	if (scrollInfo.nPos != totalHeightOld - pageHeight && totalHeightOld > pageHeight && addingMessage) {
		// No, so don't scroll.
		scroll = false;
		diffNow = 0;
		offsetY = 0;
	}

	if (scroll)
	{
		if ((!toStart && !m_bIsTopDown) || (toStart && m_bIsTopDown))
			scrollInfo.nPos += diffNow;
	}

	scrollInfo.nMin = 0;
	scrollInfo.nMax = th;
	scrollInfo.nPage = pageHeight;

	int posNow = scrollInfo.nPos;
	SetScrollInfo(m_hwnd, SB_VERT, &scrollInfo, true);
	GetScrollInfo(m_hwnd, SB_VERT, &scrollInfo);
	m_oldPos = scrollInfo.nPos;

	if (m_bManagedByOwner)
		return;

	if (update)
	{
		if (posNow != scrollInfo.nPos)
		{
			// ah screw it
			// TODO: fix it so it only redraws what's needed
			InvalidateRect(m_hwnd, NULL, MayErase());
			return;
		}

		RECT rcScroll = rect;
		if (offsetY > 0) {
			rcScroll.bottom -= offsetY;
		} else {
			rcScroll.top -= offsetY;
		}

		int diffUpDown = toStart ? 0 : diffNow;
		Scroll(diffUpDown, offsetY ? &rcScroll : NULL);
	}
}

void MessageList::FlashMessage(Snowflake msg)
{
	auto itMsg = FindMessage(msg);
	if (itMsg == m_messages.end())
		return;
	
	// If a flash process has already started, end it.
	if (IsFlashingMessage())
		EndFlashMessage();

	// Start flashing
	m_flash_counter = 5;
	m_emphasizedMessage = msg;
	m_flash_timer = SetTimer(m_hwnd, 0, 250, NULL);
	if (!m_flash_timer) {
		DbgPrintW("Failed to flash message %lld", msg);
		m_flash_counter = 0;
		m_emphasizedMessage = 0;
		return;
	}

	InvalidateRect(m_hwnd, &itMsg->m_rect, FALSE);
}

bool MessageList::IsFlashingMessage() const
{
	return m_flash_timer != 0;
}

void MessageList::EndFlashMessage()
{
	// Find the message that is being flashed.
	auto itMsg = FindMessage(m_emphasizedMessage);
	m_emphasizedMessage = 0;

	// Check if the message is rendered normally
	bool wasFlipped = m_flash_timer % 2 != 0;
	m_flash_timer = 0;
	if (wasFlipped != 0) {
		// No, so need to invalidate it.
		if (itMsg != m_messages.end())
			InvalidateRect(m_hwnd, &itMsg->m_rect, FALSE);
	}

	KillTimer(m_hwnd, m_flash_timer);
	m_flash_timer = 0;
}

void MessageList::ReloadBackground()
{
	UnloadBackground();

	if (GetLocalSettings()->GetMessageStyle() != MS_IMAGE)
		return;

	const auto& str = GetLocalSettings()->GetImageBackgroundFileName();
	if (str.empty())
	{
		m_backgroundBrush = m_defaultBackgroundBrush;
		m_backgroundImage = NULL;
		m_bDontDeleteBackgroundBrush = true;
		return;
	}

	m_backgroundImage = ImageLoader::LoadFromFile(str.c_str(), m_bBackgroundHasAlpha);

	if (m_bBackgroundHasAlpha)
	{
		m_backgroundColor = GetSysColor(COLOR_WINDOW);
		m_backgroundBrush = GetSysColorBrush(COLOR_WINDOW);
		m_bDontDeleteBackgroundBrush = true;
	}
	else
	{
		// sample a pixel depending on the alignment
		BITMAP bm{};
		GetObject(m_backgroundImage, sizeof bm, &bm);
		int xSamp = 0, ySamp = 0;

		auto align = GetLocalSettings()->GetImageAlignment();
		if (align == ALIGN_UPPER_LEFT || align == ALIGN_MIDDLE_LEFT || align == ALIGN_LOWER_LEFT)
			xSamp = bm.bmWidth - 1;
		if (align == ALIGN_UPPER_LEFT || align == ALIGN_UPPER_CENTER || align == ALIGN_UPPER_RIGHT)
			ySamp = bm.bmHeight - 1;

		HDC hdc = GetDC(m_hwnd);
		HDC hdc2 = CreateCompatibleDC(hdc);
		HGDIOBJ old = SelectObject(hdc2, m_backgroundImage);
		m_backgroundColor = GetPixel(hdc2, xSamp, ySamp);
		SelectObject(hdc2, old);
		DeleteDC(hdc2);
		ReleaseDC(m_hwnd, hdc);

		m_backgroundBrush = CreateSolidBrush(m_backgroundColor);
		m_bDontDeleteBackgroundBrush = false;
	}

	m_bInvertTextColors = IsColorDark(m_backgroundColor) ^ (!IsColorDark(GetSysColor(COLOR_WINDOWTEXT)));
}

void MessageList::UnloadBackground()
{
	if (m_backgroundImage)
		DeleteBitmap(m_backgroundImage);
	m_backgroundImage = NULL;
	
	if (m_backgroundBrush && !m_bDontDeleteBackgroundBrush)
		DeleteBrush(m_backgroundBrush);
	m_backgroundBrush = NULL;
	m_bDontDeleteBackgroundBrush = true;

	m_bInvertTextColors = false;
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

COLORREF MessageList::GetDarkerBackgroundColor() const
{
	COLORREF bgColor;
	auto style = GetLocalSettings()->GetMessageStyle();
	if (style == MS_IMAGE)
		bgColor = m_backgroundColor;
	else if (style == MS_FLATBR)
		bgColor = GetSysColor(COLOR_WINDOW);
	else
		bgColor = GetSysColor(COLOR_3DFACE);

	COLORREF hint = IsColorDark(bgColor) ? 0xFFFFFF : 0x000000;

	return LerpColor(hint, bgColor, 90, 100);
}

void MessageList::FullRecalcAndRepaint()
{
	for (auto& msg : m_messages) {
		msg.Update(m_guildID);
	}

	int repaintSizeUnused = 0;
	RecalcMessageSizes(true, repaintSizeUnused, 0);
}

void MessageList::AdjustHeightInfo(const MessageItem& msg, int& height, int& textheight, int& authheight, int& replyheight, int& attachheight, int& embedheight)
{
	bool isCompact = IsCompact();
	int replyheight2 = replyheight;
	if (replyheight2)
		replyheight2 += ScaleByDPI(5);

	bool isChainCont = msg.m_placeInChain != 0;
	if (isChainCont)
	{
		authheight = 0;
	}

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
		int pad = 25;
		if (!authheight) pad = 20;
		if (isChainCont) pad = 10;
		height = replyheight2 + textheight + authheight + ScaleByDPI(pad);
	}

	int minHeight = replyheight2 + ScaleByDPI(PROFILE_PICTURE_SIZE_DEF + 20);

	// If a date gap, add an extra X pixels
	if (!m_bManagedByOwner && msg.m_bIsDateGap)
		height += DATE_GAP_HEIGHT, minHeight += DATE_GAP_HEIGHT;

	// also figure out embed size
	embedheight = 0;
	if (GetLocalSettings()->ShowEmbedContent())
	{
		for (auto& emb : msg.m_embedData)
		{
			int inc = emb.m_size.cy + ScaleByDPI(5);
			height += inc;
			embedheight += inc;
		}
	}

	// also figure out attachment size
	attachheight = 0;
	for (auto& att : msg.m_msg.m_attachments)
	{
		// XXX improve?
		int inc = 0;
		if (!GetLocalSettings()->ShowAttachmentImages() || !att.IsImage())
			inc = ATTACHMENT_HEIGHT + ATTACHMENT_GAP;
		else
			inc = att.m_previewHeight + ATTACHMENT_GAP;

		height += inc;
		attachheight += inc;
	}

	if (embedheight != 0)
		embedheight -= ScaleByDPI(5);
	if (attachheight != 0)
		attachheight -= ATTACHMENT_GAP;

	if (attachheight != 0)
		height += ScaleByDPI(5);
	if (embedheight != 0)
		height += ScaleByDPI(5);

	if (!IsActionMessage(msg.m_msg.m_type) && !IsCompact() && !isChainCont && height < minHeight)
		height = minHeight;

	if (msg.m_msg.m_type == MessageType::CHANNEL_HEADER) {
		height = ScaleByDPI(64);
	}
}

bool MessageList::ShouldBeDateGap(time_t oldTime, time_t newTime)
{
	return !m_bManagedByOwner && (oldTime / 86400 != newTime / 86400);
}

bool MessageList::ShouldStartNewChain(Snowflake prevAuthor, time_t prevTime, int prevPlaceInChain, MessageType::eType prevType, const MessageItem& item, bool ifChainTooLongToo)
{
	return m_bManagedByOwner || (
		(prevPlaceInChain >= 10 && ifChainTooLongToo) ||
		prevAuthor != item.m_msg.m_author_snowflake ||
		prevTime + 15 * 60 < item.m_msg.m_dateTime ||
		item.m_msg.IsLoadGap() ||
		item.m_msg.m_pReferencedMessage != nullptr ||
		item.m_msg.m_type == MessageType::REPLY ||
		IsActionMessage(prevType) ||
		IsActionMessage(item.m_msg.m_type) ||
		ShouldBeDateGap(prevTime, item.m_msg.m_dateTime)
	);
}

int MessageList::RecalcMessageSizes(bool update, int& repaintSize, Snowflake addedMessagesBeforeThisID)
{
	repaintSize = 0;
	m_total_height = 0;

	RECT msgRect2 = {};
	GetClientRect(m_hwnd, &msgRect2);

	bool isCompact = IsCompact();

	HDC hdc = GetDC(m_hwnd);
	time_t prevTime = 0;
	Snowflake prevAuthor = Snowflake(-1);
	MessageType::eType prevType = MessageType::DEFAULT;
	int prevPlaceInChain = 0;
	int subScroll = 0;

	for (std::list<MessageItem>::iterator iter = m_messages.begin();
		iter != m_messages.end();
		++iter)
	{
		RECT msgRect = msgRect2;

		int height = 100, authheight = 100, authwidth = 200, textheight = 200, datewidth = 0;

		if (iter->m_message.Empty())
		{
			// HACK
			if (iter->m_msg.m_snowflake < addedMessagesBeforeThisID || addedMessagesBeforeThisID == 0) {
				iter->m_bKeepHeightRecalc = false;
				iter->Update(m_guildID);
			}
		}

		bool modifyChainOrder = addedMessagesBeforeThisID == 0 || iter->m_msg.m_snowflake <= addedMessagesBeforeThisID;

		bool bIsDateGap = ShouldBeDateGap(prevTime, iter->m_msg.m_dateTime);
		bool startNewChain = isCompact || ShouldStartNewChain(prevAuthor, prevTime, prevPlaceInChain, prevType, *iter, modifyChainOrder);

		bool msgOldIsDateGap = iter->m_bIsDateGap;
		bool msgOldWasChainBeg = iter->m_placeInChain == 0;

		iter->m_bIsDateGap = bIsDateGap;

		if (modifyChainOrder) {
			iter->m_placeInChain = startNewChain ? 0 : 1 + prevPlaceInChain;
		}
		else {
			startNewChain = msgOldWasChainBeg;
		}

		if (iter->m_bKeepHeightRecalc)
		{
			// XXX - this is the new chain height - maybe a better formula exists?
			int newChainHeight = iter->m_authHeight + ScaleByDPI(15);
			int oldHeight = iter->m_height;

			if (!startNewChain && msgOldWasChainBeg)
				iter->m_height -= newChainHeight;

			if (startNewChain && !msgOldWasChainBeg) {
				assert(!"This condition shouldn't even be reached");
				iter->m_height += newChainHeight;
			}

			if (bIsDateGap && !msgOldIsDateGap)
				iter->m_height += DATE_GAP_HEIGHT;
			if (!bIsDateGap && msgOldIsDateGap)
				iter->m_height -= DATE_GAP_HEIGHT;

			if (oldHeight != iter->m_height)
			{
				int diff = iter->m_height - oldHeight;
				subScroll -= diff;

				// This situation can pretty much only happen when scrolling
				// up to load new messages, so do this
				assert(repaintSize == 0);
				repaintSize = iter->m_height;
			}

			iter->m_bKeepHeightRecalc = false;
		}
		else
		{
			DetermineMessageMeasurements(*iter, hdc, &msgRect);
		}

		prevPlaceInChain = iter->m_placeInChain;
		prevAuthor = iter->m_msg.m_author_snowflake;
		prevTime = iter->m_msg.m_dateTime;
		prevType = iter->m_msg.m_type;

		m_total_height += iter->m_height;
	}

	ReleaseDC(m_hwnd, hdc);
	UpdateScrollBar(0, m_total_height, false, update, 0, false);
	return subScroll;
}

void MessageList::OnUpdateAttachment(Snowflake sf)
{
	// An attachment was loaded.
	for (auto& msg : m_messages)
	{
		for (auto& ai : msg.m_attachmentData)
			if (ai.m_pAttachment->m_id == sf)
				InvalidateRect(m_hwnd, &ai.m_boxRect, false);
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
				InvalidateRect(m_hwnd, &ai.m_boxRect, false);
		}
		for (auto& ai : msg.m_embedData)
		{
			if (ai.m_pEmbed->m_bHasThumbnail) {
				if (ai.m_thumbnailResourceID == res) {
					DbgPrintW("Invalidating thumbnail %s", res.c_str());
					InvalidateRect(m_hwnd, &ai.m_thumbnailRect, false);
				}
			}
			else if (ai.m_pEmbed->m_bHasImage) {
				if (ai.m_imageResourceID == res) {
					DbgPrintW("Invalidating image %s", res.c_str());
					InvalidateRect(m_hwnd, &ai.m_imageRect, false);
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
			InvalidateRect(m_hwnd, &msg.m_avatarRect, false);

		if (msg.m_msg.m_pReferencedMessage != nullptr &&
			msg.m_msg.m_pReferencedMessage->m_author_snowflake == sf && !IsCompact())
			InvalidateRect(m_hwnd, &msg.m_refAvatarRect, false);
	}
}

void MessageList::InvalidateEmote(void* context, const Rect& rc)
{
	HRGN invRgn = CreateRectRgn(W32RECT(rc));
	UnionRgn((HRGN)context, (HRGN)context, invRgn);
	DeleteRgn(invRgn);
}

void MessageList::OnUpdateEmoji(Snowflake sf)
{
	HRGN rgn = CreateRectRgn(0, 0, 0, 0);

	// TODO: Only invalidate the emoji that were updated
	for (auto& msg : m_messages)
	{
		if (msg.m_message.IsFormatted())
			msg.m_message.RunForEachCustomEmote(&InvalidateEmote, (void*) rgn);
	}

	InvalidateRgn(m_hwnd, rgn, FALSE);
	DeleteRgn(rgn);
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
	if (!_hdc) hdc = GetDC(m_hwnd);
	if (!_rect)
		GetClientRect(m_hwnd, &rect);
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
		mi.m_msg.m_bIsAuthorBot,
		rect,
		mi.m_textHeight,
		mi.m_authHeight,
		mi.m_authWidth,
		mi.m_dateWidth,
		mi.m_replyMessageWidth,
		mi.m_replyAuthorWidth,
		mi.m_replyHeight,
		mi.m_placeInChain
	);
	
	const bool isCompact = IsCompact();

	if (GetLocalSettings()->ShowEmbedContent()) {
		for (auto& embed : mi.m_embedData)
			embed.Measure(hdc, rect, isCompact);
	}

	AdjustHeightInfo(mi, mi.m_height, mi.m_textHeight, mi.m_authHeight, mi.m_replyHeight, mi.m_attachHeight, mi.m_embedHeight);

	if (!_hdc) ReleaseDC(m_hwnd, hdc);
}

void MessageList::EditMessage(const Message& newMsg)
{
	MessageItem mi;
	mi.m_msg = newMsg; // please copy
	mi.Update(m_guildID);
	
	LPCTSTR strAuth = mi.m_author;
	LPCTSTR strDate = mi.m_date;
	LPCTSTR strDateEdit = mi.m_dateEdited;

	// measure the height
	int oldHeight = 0;

	// If the message exists, then delete it.
	// TODO: Why are we doing this instead of just replacing the message in-place?
	bool bDeletedOldMsg = false;
	int  placeInChainOld = -1;
	bool wasDateGap = false;
	RECT oldMsgRect = {};
	for (auto iter = m_messages.rbegin(); iter != m_messages.rend(); ++iter)
	{
		if (iter->m_msg.m_snowflake == newMsg.m_snowflake)
		{
			// delete it!
			oldHeight = iter->m_height;
			oldMsgRect = iter->m_rect;
			wasDateGap = iter->m_bIsDateGap;
			placeInChainOld = iter->m_placeInChain;
			auto msgIter = --(iter.base());
			m_messages.erase(msgIter); // sucks
			bDeletedOldMsg = true;
			break;
		}
	}

	bool canInsert = false;
	bool firstTime = true;
	auto insertIter = m_messages.end();
	while (firstTime || insertIter != m_messages.end())
	{
		Snowflake thisSF = 0, nextSF = UINT64_MAX;

		if (!firstTime)
			thisSF = insertIter->m_msg.m_snowflake;

		auto nextiter = insertIter;
		if (firstTime)
			nextiter = m_messages.begin();
		else
			++nextiter;

		if (nextiter != m_messages.end()) {
			nextSF = nextiter->m_msg.m_snowflake;
		}

		if (thisSF < newMsg.m_snowflake && newMsg.m_snowflake < nextSF) {
			// note: using nextIter because list.insert(iter) shifts elements
			// starting from iter to the right, not from iter+1. We want to insert
			// AFTER `insertIter`.
			insertIter = nextiter;
			canInsert = true;
			break;
		}

		if (firstTime)
			insertIter = m_messages.begin();
		else
			++insertIter;

		firstTime = false;
	}
	
	assert(canInsert);
	assert(bDeletedOldMsg);

	mi.m_bIsDateGap = wasDateGap;
	mi.m_placeInChain = placeInChainOld;

	DetermineMessageMeasurements(mi, NULL, NULL);
	mi.m_msg.m_anchor = 0;

	bool toStart = false;
	m_messages.insert(insertIter, mi);

	RECT rcClient{};
	GetClientRect(m_hwnd, &rcClient);

	// totally new message
	UpdateScrollBar(mi.m_height - oldHeight, mi.m_height - oldHeight, toStart, true, bDeletedOldMsg ? (rcClient.bottom - oldMsgRect.top) : 0, true);

	if (bDeletedOldMsg)
	{
		// in case the message shrunk in size
		oldMsgRect.top += oldHeight - mi.m_height;

		// do it better
		InvalidateRect(m_hwnd, &oldMsgRect, false);
	}
}

void MessageList::SetLastViewedMessage(Snowflake sf, bool refreshItAlso)
{
	if (m_previousLastReadMessage == sf)
		return;

	bool notExactMatch = false;
	auto itm = m_messages.begin();
	for (; itm != m_messages.end(); ++itm) {
		if (itm->m_msg.m_snowflake >= sf) {
			notExactMatch = itm->m_msg.m_snowflake != sf;
			break;
		}
	}

	Snowflake old = m_previousLastReadMessage;
	m_previousLastReadMessage = sf;
	if (old != 0)
	{
		// find that message's iter...
		auto it = m_messages.rbegin();
		auto it2 = m_messages.end();
		for (; it != m_messages.rend(); ++it) {
			if (it->m_msg.m_snowflake == old) {
				it2 = it.base(); // returns the NEXT element.
				break;
			}
		}

		if (it2 != m_messages.end()) {
			// refresh JUST the top part
			RECT rcMsg = it2->m_rect;
			rcMsg.bottom = rcMsg.top + ScaleByDPI(15); // about the height of the NEW marker
			InvalidateRect(m_hwnd, &rcMsg, FALSE);
		}
	}

	// check if this message is the last one
	if (!notExactMatch && itm != m_messages.end()) {
		++itm;
		if (itm == m_messages.end())
			// yeah, so don't ACTUALLY update - that'll be done on refresh.
			return;
	}

	if (refreshItAlso)
	{
		// TODO: fix bug where first message isn't actually properly refreshed?
		if (itm != m_messages.end()) {
			// refresh, again, JUST the top part
			RECT rcMsg = itm->m_rect;
			rcMsg.bottom = rcMsg.top + ScaleByDPI(15) + (itm->m_bIsDateGap ? DATE_GAP_HEIGHT : 0); // about the height of the NEW marker
			InvalidateRect(m_hwnd, &rcMsg, FALSE);
		}
	}
}

void MessageList::Scroll(int amount, RECT* rcClip, bool shiftAllRects)
{
	// amount - Amount of pixels to scroll UP
	if (shiftAllRects)
	{
		for (auto& msg : m_messages)
			msg.ShiftUp(amount);
	}
	
	if (GetLocalSettings()->GetMessageStyle() == MS_IMAGE)
	{
		// nope, just invalidate
		InvalidateRect(m_hwnd, NULL, FALSE);
		return;
	}

	// Note. In the case that any repainting is done, we perform the shifts
	// BEFORE repainting to avoid some weird glitches caused by a shift occuring
	// twice.
	ScrollWindowEx(m_hwnd, 0, -amount, rcClip, NULL, NULL, NULL, SW_INVALIDATE);
	UpdateWindow(m_hwnd);
}

void MessageList::MessageHeightChanged(int oldHeight, int newHeight, bool toStart)
{
	UpdateScrollBar(newHeight - oldHeight, newHeight - oldHeight, toStart);
}

void MessageList::AddMessageInternal(const Message& msg, bool toStart, bool updateLastViewedMessage, bool resetAnchor)
{
	MessageItem mi;
	mi.m_msg = msg;

	mi.Update(m_guildID);

	LPCTSTR strAuth = mi.m_author;
	LPCTSTR strDate = mi.m_date;
	LPCTSTR strDateEdit = mi.m_dateEdited;

	// measure the height
	RECT msgRect = {};
	GetClientRect(m_hwnd, &msgRect);

	int oldHeight = 0;

	// If the message has a nonce, then delete it.
	if (msg.m_anchor)
		DeleteMessage(msg.m_anchor);

	Snowflake prevAuthor = Snowflake(-1);
	time_t prevDate = 0;
	int prevPlaceInChain = -1;
	MessageType::eType prevType = MessageType::DEFAULT;
	if (!m_messages.empty())
	{
		MessageItem* item = nullptr;
		if (toStart)
			item = &*m_messages.begin();
		else
			item = &*m_messages.rbegin();

		if (item) {
			prevAuthor = item->m_msg.m_author_snowflake;
			prevDate = item->m_msg.m_dateTime;
			prevType = item->m_msg.m_type;
			prevPlaceInChain = item->m_placeInChain;
		}
	}

	if (ShouldBeDateGap(prevDate, mi.m_msg.m_dateTime))
		mi.m_bIsDateGap = true;

	if (prevPlaceInChain < 0 || ShouldStartNewChain(prevAuthor, prevDate, prevPlaceInChain, prevType, mi, true))
		mi.m_placeInChain = 0;
	else
		mi.m_placeInChain = prevPlaceInChain + 1;

	DetermineMessageMeasurements(mi, NULL, NULL);

	if (resetAnchor)
		mi.m_msg.m_anchor = 0;

	Snowflake lastMessageId = 0;
	if (!m_messages.empty())
		lastMessageId = m_messages.rbegin()->m_msg.m_snowflake;

	if (toStart)
		m_messages.push_front(mi);
	else
		m_messages.push_back(mi);

	if (updateLastViewedMessage) {
		SetLastViewedMessage(mi.m_msg.m_snowflake, false);
	}

	RECT rcClient{};
	GetClientRect(m_hwnd, &rcClient);

	UpdateScrollBar(mi.m_height - oldHeight, mi.m_height - oldHeight, toStart, true, 0, true);

	bool needUpdateAboveMessage = GetLocalSettings()->GetMessageStyle() == MS_3DFACE;
	if (needUpdateAboveMessage && mi.m_placeInChain != 0) {
		RECT rcUpdate = rcClient;
		rcUpdate.top = rcUpdate.bottom - 2;
		InvalidateRect(m_hwnd, &rcUpdate, FALSE);
	}
}

void MessageList::UpdateAllowDrop()
{
	BOOL Accept = FALSE;
	Channel* pChan = GetDiscordInstance()->GetChannel(m_channelID);

	if (pChan)
		Accept = pChan->HasPermission(PERM_SEND_MESSAGES) && pChan->HasPermission(PERM_ATTACH_FILES);
	
	DragAcceptFiles(m_hwnd, Accept);
}

MessageList* MessageList::Create(HWND hwnd, LPRECT pRect)
{
	MessageList* newThis = new MessageList;
	int width = pRect->right - pRect->left, height = pRect->bottom - pRect->top;

	newThis->m_hwnd = CreateWindowEx(
		WS_EX_CLIENTEDGE, T_MESSAGE_LIST_CLASS, NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL,
		pRect->left, pRect->top, width, height, hwnd, (HMENU)CID_MESSAGELIST, g_hInstance, newThis
	);

	newThis->UpdateBackgroundBrush();

	return newThis;
}
