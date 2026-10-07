#include "Xm.hpp"
#include "MessageView.hpp"

#include <algorithm>
#include <cstdio>
#include <ctime>

#include <Xm/DrawingA.h>
#include <Xm/Form.h>
#include <Xm/ScrollBar.h>

#include "DiscordInstance.hpp"
#include "Frontend.hpp"
#include "state/MessageCache.hpp"
#include "Theme.hpp"
#include "ImageCache.hpp"

// Geometry, in pixels
static const int MARGIN = 16;        // left and right
static const int AVATAR = 36;
static const int TEXT_X = MARGIN + AVATAR + 12;
static const int GROUP_GAP = 14;     // above a message with a header
static const int LINE_GAP = 2;       // above a grouped message
static const int DATE_SEP = 30;
static const int GROUP_SECONDS = 7 * 60;

struct ItemLink
{
	Rect rect; // item coordinates
	std::string url;
};

// Per-item layout that only this file needs.
struct MessageView::ItemExtra
{
	std::string dateSep;   // non-empty: a day separator above the item
	std::vector<ItemLink> links;
	std::vector<std::unique_ptr<FormattedText>> embedTexts;
	std::vector<int> embedTops;
	std::vector<int> embedHeights;
	// images: where they go (item coordinates) and what to fetch
	struct Pic { Rect rect; std::string url; };
	std::vector<Pic> attachPics;       // one per attachment; empty url: a file line
	std::vector<Pic> embedThumbs;      // one per embed; empty url: none
	std::vector<Pic> embedImages;      // one per embed; empty url: none
	int attachTop = 0;
	int replyTop = 0;
	int headerTop = 0;
};


static std::string FormatSize(int bytes)
{
	char buf[64];
	if (bytes >= 1024 * 1024)
		snprintf(buf, sizeof buf, "%.1f MB", bytes / (1024.0 * 1024.0));
	else if (bytes >= 1024)
		snprintf(buf, sizeof buf, "%.0f KB", bytes / 1024.0);
	else
		snprintf(buf, sizeof buf, "%d bytes", bytes);
	return buf;
}

// A media proxy URL asking for the w x h preview, when that differs from
// the original size.
static std::string PreviewURL(const std::string& base, int w, int h, int origW, int origH)
{
	if (base.empty() || (w == origW && h == origH))
		return base;
	return base + (base.find('?') == std::string::npos ? "?" : "&") +
		"width=" + std::to_string(w) + "&height=" + std::to_string(h);
}

// w x h scaled to fit in maxW x maxH (never enlarged).
static void FitBox(int& w, int& h, int maxW, int maxH)
{
	if (w <= 0 || h <= 0) {
		w = maxW;
		h = maxH / 2;
		return;
	}
	if (w > maxW) { h = h * maxW / w; w = maxW; }
	if (h > maxH) { w = w * maxH / h; h = maxH; }
	if (w < 1) w = 1;
	if (h < 1) h = 1;
}

static std::string DayOf(time_t t)
{
	struct tm tmv = *localtime(&t);
	char buf[64];
	strftime(buf, sizeof buf, "%A, %e %B %Y", &tmv);
	return buf;
}

static int DayNumber(time_t t)
{
	struct tm tmv = *localtime(&t);
	return tmv.tm_year * 400 + tmv.tm_yday;
}

// The one-line text of a system message, or "" for ordinary messages.
static std::string SystemText(const Message& m, const std::string& channelName)
{
	using namespace MessageType;
	switch (m.m_type)
	{
		case USER_JOIN:              return m.m_author + " joined the server.";
		case CHANNEL_PINNED_MESSAGE: return m.m_author + " pinned a message to this channel.";
		case RECIPIENT_ADD:          return m.m_author + " added someone to the group.";
		case RECIPIENT_REMOVE:       return m.m_author + " removed someone from the group.";
		case CALL:                   return m.m_author + " started a call.";
		case CHANNEL_NAME_CHANGE:    return m.m_author + " changed the channel name: " + m.m_message;
		case CHANNEL_ICON_CHANGE:    return m.m_author + " changed the channel icon.";
		case GUILD_BOOST:
		case GUILD_BOOST_TIER_1:
		case GUILD_BOOST_TIER_2:
		case GUILD_BOOST_TIER_3:     return m.m_author + " boosted the server!";
		case CHANNEL_FOLLOW_ADD:     return m.m_author + " added " + m.m_message + " to this channel.";
		case THREAD_CREATED:         return m.m_author + " started a thread: " + m.m_message;
		case STAGE_START:            return m.m_author + " started the stage.";
		case STAGE_END:              return m.m_author + " ended the stage.";
		case CANT_VIEW_MSG_HISTORY:  return "You do not have permission to read the message history of #" + channelName + ".";
		case CHANNEL_HEADER:         return "This is the start of #" + channelName + ".";
		case GAP_UP:
		case GAP_DOWN:
		case GAP_AROUND:             return "Loading messages\xe2\x80\xa6";
		default:                     return "";
	}
}

MessageView::MessageView(Widget parent, const PixelFormat& fmt) : m_fmt(fmt)
{
	m_form = XtVaCreateWidget("messageView", xmFormWidgetClass, parent, NULL);

	m_scroll = XtVaCreateManagedWidget("messageScroll", xmScrollBarWidgetClass, m_form,
		XmNorientation, XmVERTICAL,
		XmNtopAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		XmNminimum, 0,
		XmNmaximum, 1,
		XmNsliderSize, 1,
		NULL);

	m_area = XtVaCreateManagedWidget("messageArea", xmDrawingAreaWidgetClass, m_form,
		XmNtopAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNleftAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_WIDGET,
		XmNrightWidget, m_scroll,
		XmNwidth, 500,
		XmNheight, 400,
		XmNresizePolicy, XmRESIZE_NONE,
		XmNtraversalOn, False,
		NULL);

	XtAddCallback(m_area, XmNexposeCallback, ExposeCB, this);
	XtAddCallback(m_area, XmNresizeCallback, ResizeCB, this);
	XtAddCallback(m_scroll, XmNvalueChangedCallback, ScrollCB, this);
	XtAddCallback(m_scroll, XmNdragCallback, ScrollCB, this);
	XtAddEventHandler(m_area, ButtonPressMask, False, InputEH, this);

	XtManageChild(m_form);
	ApplyTheme(m_ctx);
}

int MessageView::ContentWidth() const
{
	return std::max(200, m_viewW);
}

void MessageView::SetChannel(Snowflake guild, Snowflake channel)
{
	m_guild = guild;
	m_channel = channel;
	m_items.clear();
	m_requestedGaps.clear();
	m_scrollY = 0;
	m_stickToBottom = true;
	Refresh();
}

void MessageView::Refresh()
{
	// remember what is on screen: the first message whose top is visible
	Snowflake anchor = 0;
	int anchorOffset = 0;
	bool atBottom = m_stickToBottom || m_scrollY + m_viewH >= m_contentHeight - 4;
	for (auto& it : m_items) {
		if (it.y + it.height > m_scrollY && !it.msg->IsLoadGap()) {
			anchor = it.msg->m_snowflake;
			anchorOffset = it.y - m_scrollY;
			break;
		}
	}

	Rebuild();
	LayoutAll();

	if (atBottom) {
		ScrollToBottom();
		return;
	}

	for (auto& it : m_items) {
		if (it.msg->m_snowflake == anchor) {
			SetScroll(it.y - anchorOffset);
			return;
		}
	}
	SetScroll(m_scrollY);
}

void MessageView::Relayout()
{
	ApplyTheme(m_ctx);
	for (auto& it : m_items) {
		it.text.Clear();
		it.laidOutWidth = -1;
	}
	Refresh();
}

void MessageView::Rebuild()
{
	std::list<MessagePtr> msgs;
	if (m_channel)
		GetMessageCache()->GetLoadedMessages(m_channel, m_guild, msgs);

	DiscordInstance* pInst = GetDiscordInstance();
	Channel* pChan = pInst ? pInst->GetChannelGlobally(m_channel) : nullptr;
	std::string chanName = pChan ? pChan->m_name : "";

	// keep the laid-out items of messages that did not change
	std::map<Snowflake, Item*> old;
	for (auto& it : m_items)
		old[it.msg->m_snowflake] = &it;

	std::list<Item> items;
	const Message* prev = nullptr;
	int prevDay = -1;
	for (auto& mp : msgs)
	{
		items.emplace_back();
		Item& item = items.back();
		auto o = old.find(mp->m_snowflake);
		bool reuse = o != old.end() && o->second->msg == mp;

		item.msg = mp;
		item.systemText = SystemText(*mp, chanName);
		item.systemLine = !item.systemText.empty();

		bool isGap = mp->IsLoadGap();
		int day = isGap ? prevDay : DayNumber(mp->m_dateTime);
		item.extra = std::make_shared<ItemExtra>();
		ItemExtra& ex = *item.extra;
		if (!isGap && day != prevDay && mp->m_dateTime)
			ex.dateSep = DayOf(mp->m_dateTime);

		item.grouped = prev && !item.systemLine && !prev->IsLoadGap() &&
			ex.dateSep.empty() &&
			SystemText(*prev, chanName).empty() &&
			prev->m_author_snowflake == mp->m_author_snowflake &&
			prev->m_author == mp->m_author &&
			!mp->IsReply() &&
			mp->m_dateTime - prev->m_dateTime < GROUP_SECONDS;

		if (reuse && !o->second->text.Empty()) {
			// the formatted text parses once per message
			std::swap(item.text, o->second->text);
			std::swap(item.interactables, o->second->interactables);
			std::swap(item.reply, o->second->reply);
		}

		if (!isGap)
			prevDay = day;
		prev = mp.get();
	}

	m_items.swap(items);
}

void MessageView::LayoutAll()
{
	int width = ContentWidth();
	int y = 8;
	for (auto& it : m_items) {
		LayoutItem(it, width);
		it.y = y;
		y += it.height;
	}
	m_contentHeight = y + 12;
	UpdateScrollbar();
}

void MessageView::LayoutItem(Item& item, int width)
{
	ItemExtra& ex = *item.extra;
	const Message& m = *item.msg;
	int right = width - MARGIN;
	int y = 0;

	if (!ex.dateSep.empty())
		y += DATE_SEP;

	if (item.systemLine) {
		y += m.IsLoadGap() ? 8 : 6;
		ex.headerTop = y;
		y += Fonts::LineHeight(FS_ITALIC, m_ctx.px) + (m.IsLoadGap() ? 8 : 6);
		item.height = y;
		return;
	}

	y += item.grouped ? LINE_GAP : GROUP_GAP;

	// reply: one small line above the header
	if (m.IsReply() && !item.grouped) {
		ex.replyTop = y;
		if (!item.reply) {
			item.reply.reset(new FormattedText);
			item.reply->SetDefaultStyle(WORD_ITALIC | WORD_SMALLER);
			item.reply->SetAllowBiggerText(false);
			item.reply->SetMessage(m.m_pReferencedMessage->m_message);
			std::vector<InteractableItem> ignored;
			GetDiscordInstance()->ResolveLinks(item.reply.get(), ignored, m_guild);
		}
		y += Fonts::LineHeight(FS_ITALIC, m_ctx.px - 2) + 4;
	}

	if (!item.grouped) {
		ex.headerTop = y;
		y += Fonts::LineHeight(FS_BOLD, m_ctx.px) + 2;
	}

	// the text
	item.textTop = y;
	if (item.text.Empty() && !m.m_message.empty()) {
		item.text.SetMessage(m.m_message);
		item.interactables.clear();
		GetDiscordInstance()->ResolveLinks(&item.text, item.interactables, m_guild);
	}
	if (!item.text.Empty()) {
		item.text.Layout(&m_ctx, Rect(TEXT_X, y, right, y + 100000));
		Rect ext = item.text.GetExtent();
		y = std::max(y, ext.bottom);
		item.laidOutWidth = width;
	}

	// attachments: images as previews, other files a line each
	ex.attachTop = y;
	ex.links.clear();
	ex.attachPics.clear();
	int lh = Fonts::LineHeight(FS_REGULAR, m_ctx.px) + 4;
	for (auto& att : m.m_attachments) {
		if (att.IsImage() && att.m_width > 0 && att.m_height > 0) {
			int w = att.m_previewWidth > 0 ? att.m_previewWidth : att.m_width;
			int h = att.m_previewHeight > 0 ? att.m_previewHeight : att.m_height;
			FitBox(w, h, std::min(300, right - TEXT_X), 300);
			y += 4;
			Rect r(TEXT_X, y, TEXT_X + w, y + h);
			ex.attachPics.push_back({ r, PreviewURL(att.m_proxyUrl, w, h, att.m_width, att.m_height) });
			ex.links.push_back(ItemLink{ r, att.m_actualUrl });
			y += h + 4;
			continue;
		}
		ex.attachPics.push_back({ Rect(TEXT_X, y, right, y + lh), "" });
		ex.links.push_back(ItemLink{ Rect(TEXT_X, y, right, y + lh), att.m_actualUrl });
		y += lh;
	}

	// embeds: a bar, the title, the text
	ex.embedTexts.clear();
	ex.embedTops.clear();
	ex.embedHeights.clear();
	ex.embedThumbs.clear();
	ex.embedImages.clear();
	const int THUMB = 80;
	for (auto& em : m.m_embeds)
	{
		y += 4;
		int top = y;
		int boxRight = std::min(right, TEXT_X + 520);
		// a thumbnail sits at the top right, beside the text
		bool thumb = em.m_bHasThumbnail && !em.m_thumbnailProxiedUrl.empty() && !em.m_bHasImage;
		int textRight = thumb ? boxRight - THUMB - 12 : boxRight;
		y += 6;
		if (!em.m_providerName.empty())
			y += Fonts::LineHeight(FS_REGULAR, m_ctx.px - 3) + 2;
		if (!em.m_authorName.empty())
			y += Fonts::LineHeight(FS_BOLD, m_ctx.px - 2) + 2;
		if (!em.m_title.empty())
			y += Fonts::LineHeight(FS_BOLD, m_ctx.px) + 2;

		std::unique_ptr<FormattedText> ft;
		std::string body = em.m_description;
		for (auto& f : em.m_fields)
			body += (body.empty() ? "" : "\n") + std::string("**") + f.m_title + "**\n" + f.m_value;
		if (!body.empty()) {
			ft.reset(new FormattedText);
			ft->SetMessage(body);
			ft->Layout(&m_ctx, Rect(TEXT_X + 12, y, textRight, y + 100000));
			y = std::max(y, ft->GetExtent().bottom);
		}

		ItemExtra::Pic img, th;
		if (thumb) {
			int w = em.m_thumbnailWidth, h = em.m_thumbnailHeight;
			FitBox(w, h, THUMB, THUMB);
			th.rect = Rect(boxRight - 8 - w, top + 8, boxRight - 8, top + 8 + h);
			th.url = PreviewURL(em.m_thumbnailProxiedUrl, w, h, em.m_thumbnailWidth, em.m_thumbnailHeight);
			y = std::max(y, th.rect.bottom);
		}
		std::string imgUrl = em.m_bHasImage ? em.m_imageProxiedUrl :
			(em.m_bHasThumbnail && !thumb ? em.m_thumbnailProxiedUrl : "");
		if (!imgUrl.empty()) {
			int ow = em.m_bHasImage ? em.m_imageWidth : em.m_thumbnailWidth;
			int oh = em.m_bHasImage ? em.m_imageHeight : em.m_thumbnailHeight;
			int w = ow, h = oh;
			FitBox(w, h, std::min(300, boxRight - TEXT_X - 24), 300);
			y += 6;
			img.rect = Rect(TEXT_X + 12, y, TEXT_X + 12 + w, y + h);
			img.url = PreviewURL(imgUrl, w, h, ow, oh);
			y += h;
		}
		ex.embedThumbs.push_back(th);
		ex.embedImages.push_back(img);

		if (!em.m_footerText.empty())
			y += Fonts::LineHeight(FS_REGULAR, m_ctx.px - 3) + 4;
		y += 6;
		ex.embedTops.push_back(top);
		ex.embedHeights.push_back(y - top);
		ex.embedTexts.push_back(std::move(ft));
		if (!em.m_url.empty())
			ex.links.push_back(ItemLink{ Rect(TEXT_X, top, std::min(right, TEXT_X + 520), top + 40), em.m_url });
	}

	y += 2;
	item.height = y;
}

void MessageView::UpdateScrollbar()
{
	int maxv = std::max(m_contentHeight, m_viewH);
	int value = std::min(std::max(0, m_scrollY), maxv - m_viewH);
	XtVaSetValues(m_scroll,
		XmNmaximum, maxv,
		XmNsliderSize, std::max(1, std::min(m_viewH, maxv)),
		XmNvalue, value,
		XmNincrement, 40,
		XmNpageIncrement, std::max(40, m_viewH - 40),
		NULL);
}

void MessageView::SetScroll(int y)
{
	int maxY = std::max(0, m_contentHeight - m_viewH);
	m_scrollY = std::min(std::max(0, y), maxY);
	m_stickToBottom = m_scrollY >= maxY;
	UpdateScrollbar();
	Paint();
}

void MessageView::ScrollToBottom()
{
	SetScroll(m_contentHeight);
	m_stickToBottom = true;
}

void MessageView::ExposeCB(Widget, XtPointer client, XtPointer call)
{
	MessageView* self = (MessageView*) client;
	XmDrawingAreaCallbackStruct* cbs = (XmDrawingAreaCallbackStruct*) call;
	if (cbs && cbs->event && cbs->event->type == Expose && cbs->event->xexpose.count > 0)
		return;
	self->Paint();
}

void MessageView::ResizeCB(Widget w, XtPointer client, XtPointer)
{
	MessageView* self = (MessageView*) client;
	Dimension width = 0, height = 0;
	XtVaGetValues(w, XmNwidth, &width, XmNheight, &height, NULL);
	bool widthChanged = width != self->m_viewW;
	self->m_viewW = width;
	self->m_viewH = height;
	if (widthChanged)
		self->LayoutAll();
	if (self->m_stickToBottom)
		self->ScrollToBottom();
	else
		self->SetScroll(self->m_scrollY);
}

void MessageView::ScrollCB(Widget, XtPointer client, XtPointer call)
{
	MessageView* self = (MessageView*) client;
	XmScrollBarCallbackStruct* cbs = (XmScrollBarCallbackStruct*) call;
	self->m_scrollY = cbs->value;
	self->m_stickToBottom = self->m_scrollY >= self->m_contentHeight - self->m_viewH;
	self->Paint();
}

void MessageView::InputEH(Widget, XtPointer client, XEvent* ev, Boolean*)
{
	MessageView* self = (MessageView*) client;
	if (ev->type != ButtonPress)
		return;
	int step = 3 * (Fonts::LineHeight(FS_REGULAR, self->m_ctx.px) + 2);
	switch (ev->xbutton.button) {
		case Button1: self->OnClick(ev->xbutton.x, ev->xbutton.y); break;
		case Button4: self->SetScroll(self->m_scrollY - step); break;
		case Button5: self->SetScroll(self->m_scrollY + step); break;
	}
}

static bool Inside(const Rect& r, int x, int y)
{
	return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
}

void MessageView::OnClick(int x, int y)
{
	int cy = y + m_scrollY;
	for (auto& it : m_items)
	{
		if (cy < it.y || cy >= it.y + it.height)
			continue;
		int iy = cy - it.y;

		auto& words = it.text.GetWords();
		for (auto& ii : it.interactables) {
			if (ii.m_type != InteractableItem::LINK || ii.m_wordIndex >= words.size())
				continue;
			if (Inside(words[ii.m_wordIndex].m_rect, x, iy)) {
				GetFrontend()->LaunchURL(ii.m_destination);
				return;
			}
		}
		for (auto& l : it.extra->links) {
			if (Inside(l.rect, x, iy) && !l.url.empty()) {
				GetFrontend()->LaunchURL(l.url);
				return;
			}
		}
		return;
	}
}

void MessageView::RequestVisibleGaps()
{
	for (auto& it : m_items)
	{
		if (it.y + it.height < m_scrollY || it.y > m_scrollY + m_viewH)
			continue;
		const Message& m = *it.msg;
		if (!m.IsLoadGap() || m_requestedGaps.count(m.m_snowflake))
			continue;

		m_requestedGaps.insert(m.m_snowflake);
		ScrollDir::eScrollDir sd = m.m_type == MessageType::GAP_UP ? ScrollDir::BEFORE :
			m.m_type == MessageType::GAP_DOWN ? ScrollDir::AFTER : ScrollDir::AROUND;
		GetDiscordInstance()->RequestMessages(m_channel, sd, m.m_anchor, m.m_snowflake);
	}
}

Rgb RoleColor(Snowflake user, Snowflake guild); // MainWindow.cpp

static Rgb AvatarColor(Snowflake sf)
{
	static const Rgb colors[] = { 0x5865f2, 0x3ba55c, 0xfaa61a, 0xed4245, 0xeb459e, 0x747f8d, 0x2d8f9e, 0x9b59b6 };
	return colors[(sf >> 22) % (sizeof colors / sizeof colors[0])];
}

void MessageView::DrawPicture(const Rect& r, const std::string& url, int top, const std::string& label)
{
	Canvas& c = m_canvas;
	int w = r.Width(), h = r.Height();
	int x = r.left, y = r.top + top;
	if (y + h < 0 || y > m_viewH)
		return; // not on screen: not fetched either
	const Image* img = ImageCache::Get(ImageCache::URL, url, 0, w, h);
	if (img) {
		// opaque images on the background; centred in their box
		c.BlendArgb(x + (w - img->w) / 2, y + (h - img->h) / 2, img->px.data(), img->w, img->h, img->w);
		return;
	}
	c.Fill(x, y, w, h, m_ctx.codeBg);
	c.Frame(x, y, w, h, m_ctx.codeFrame);
	bool failed = ImageCache::Failed(ImageCache::URL, url, 0, w, h);
	std::string text = failed ? (label.empty() ? std::string("Image not available") : label) : std::string("Loading\xe2\x80\xa6");
	int spx = m_ctx.px - 2;
	text = Fonts::Elide(text, FS_ITALIC, spx, w - 8);
	int tw = Fonts::Measure(text, FS_ITALIC, spx);
	if (h > Fonts::LineHeight(FS_ITALIC, spx))
		Fonts::Draw(c, x + (w - tw) / 2, y + h / 2 + Fonts::Ascent(FS_ITALIC, spx) / 2, text, FS_ITALIC, spx, m_ctx.muted);
}

void MessageView::ImagesChanged()
{
	// Downloads come in bursts: repaint once for each burst.
	if (m_repaintTimer)
		return;
	m_repaintTimer = XtAppAddTimeOut(XtWidgetToApplicationContext(m_area), 80, RepaintTimerCB, this);
}

void MessageView::RepaintTimerCB(XtPointer client, XtIntervalId*)
{
	MessageView* self = (MessageView*) client;
	self->m_repaintTimer = 0;
	self->Paint();
}

void MessageView::PaintItem(Item& item, int top)
{
	Canvas& c = m_canvas;
	ItemExtra& ex = *item.extra;
	const Message& m = *item.msg;
	int right = m_viewW - MARGIN;

	if (!ex.dateSep.empty()) {
		int mid = top + DATE_SEP / 2 + 2;
		int tw = Fonts::Measure(ex.dateSep, FS_BOLD, m_ctx.px - 3);
		int cx = (m_viewW - tw) / 2;
		c.HLine(MARGIN, mid, cx - MARGIN - 8, m_ctx.codeFrame);
		c.HLine(cx + tw + 8, mid, right - (cx + tw + 8), m_ctx.codeFrame);
		Fonts::Draw(c, cx, mid + 4, ex.dateSep, FS_BOLD, m_ctx.px - 3, m_ctx.muted);
	}

	if (item.systemLine) {
		int y = top + ex.headerTop + Fonts::Ascent(FS_ITALIC, m_ctx.px);
		if (m.IsLoadGap()) {
			int tw = Fonts::Measure(item.systemText, FS_ITALIC, m_ctx.px);
			Fonts::Draw(c, (m_viewW - tw) / 2, y, item.systemText, FS_ITALIC, m_ctx.px, m_ctx.muted);
		}
		else {
			Fonts::Draw(c, TEXT_X - 22, y, "\xe2\x86\x92", FS_BOLD, m_ctx.px, 0x3ba55c);
			Fonts::Draw(c, TEXT_X, y, Fonts::Elide(item.systemText, FS_ITALIC, m_ctx.px, right - TEXT_X), FS_ITALIC, m_ctx.px, m_ctx.muted);
		}
		return;
	}

	bool pending = m.m_type == MessageType::SENDING_MESSAGE || m.m_type == MessageType::UNSENT_MESSAGE;

	if (m.IsReply() && !item.grouped && m.m_pReferencedMessage) {
		int y = top + ex.replyTop;
		int spx = m_ctx.px - 2;
		int asc = Fonts::Ascent(FS_ITALIC, spx);
		// the reply's elbow from the avatar column
		c.VLine(MARGIN + AVATAR / 2, y + asc / 2, Fonts::LineHeight(FS_ITALIC, spx), m_ctx.quoteBar);
		c.HLine(MARGIN + AVATAR / 2, y + asc / 2, TEXT_X - 6 - (MARGIN + AVATAR / 2), m_ctx.quoteBar);
		std::string who = "@" + m.m_pReferencedMessage->m_author + " ";
		int x = TEXT_X;
		x += Fonts::Draw(c, x, y + asc, who, FS_BOLD, spx, m_ctx.muted);
		std::string snippet = m.m_pReferencedMessage->m_message;
		std::replace(snippet.begin(), snippet.end(), '\n', ' ');
		if (snippet.empty())
			snippet = m.m_pReferencedMessage->m_bHasAttachments ? "Click to see attachment" : "Original message was deleted";
		Fonts::Draw(c, x, y + asc, Fonts::Elide(snippet, FS_ITALIC, spx, right - x), FS_ITALIC, spx, m_ctx.muted);
	}

	if (!item.grouped)
	{
		int y = top + ex.headerTop;
		// the avatar; a coloured disc with the initial until it arrives
		const Image* av = m.m_avatar.empty() ?
			ImageCache::Get(ImageCache::DEFAULT_AVATAR, "", m.m_author_snowflake, AVATAR, AVATAR) :
			ImageCache::Get(ImageCache::AVATAR, m.m_avatar, m.m_author_snowflake, AVATAR, AVATAR);
		Rgb ac = AvatarColor(m.m_author_snowflake);
		std::vector<uint32_t> disc((size_t) AVATAR * AVATAR, 0xff000000u | ac);
		if (av) {
			c.BlendArgbCircle(MARGIN + (AVATAR - av->w) / 2, y + (AVATAR - av->h) / 2, av->px.data(), av->w, av->h, av->w);
		}
		else {
			c.BlendArgbCircle(MARGIN, y, disc.data(), AVATAR, AVATAR, AVATAR);
		}
		if (!av && !m.m_author.empty()) {
			const char* p = m.m_author.c_str();
			const char* end = p + m.m_author.size();
			DecodeUtf8(p, end);
			std::string initial(m.m_author.c_str(), p - m.m_author.c_str());
			int iw = Fonts::Measure(initial, FS_BOLD, 17);
			Fonts::Draw(c, MARGIN + (AVATAR - iw) / 2, y + AVATAR / 2 + 6, initial, FS_BOLD, 17, 0xffffff);
		}

		int asc = Fonts::Ascent(FS_BOLD, m_ctx.px);
		int x = TEXT_X;
		std::string when = m.m_dateFull.empty() ? m.m_dateCompact : m.m_dateFull;
		if (!m.m_editedText.empty())
			when += "  (edited)";
		int whenW = Fonts::Measure(when, FS_REGULAR, m_ctx.px - 3);
		Rgb nameColor = RoleColor(m.m_author_snowflake, m_guild);
		std::string name = Fonts::Elide(m.m_author, FS_BOLD, m_ctx.px, std::max(40, right - x - whenW - 60));
		x += Fonts::Draw(c, x, y + asc, name, FS_BOLD, m_ctx.px, nameColor ? nameColor : m_ctx.fg) + 8;
		if (m.m_bIsAuthorBot || m.IsWebHook()) {
			int bw = Fonts::Measure("BOT", FS_BOLD, m_ctx.px - 4) + 8;
			c.FillRounded(x, y + 2, bw, asc, 3, 0x5865f2);
			Fonts::Draw(c, x + 4, y + asc - 1, "BOT", FS_BOLD, m_ctx.px - 4, 0xffffff);
			x += bw + 6;
		}
		// the time on the right, as Abaddon and IRC clients put it
		Fonts::Draw(c, right - whenW, y + asc, when, FS_REGULAR, m_ctx.px - 3, m_ctx.muted);
	}

	if (!item.text.Empty()) {
		Rgb fg = m_ctx.fg;
		if (pending)
			m_ctx.fg = m_ctx.muted;
		item.text.Draw(&m_ctx, top);
		m_ctx.fg = fg;
	}

	// attachments
	int ay = top + ex.attachTop;
	int lh = Fonts::LineHeight(FS_REGULAR, m_ctx.px) + 4;
	for (size_t ai = 0; ai < m.m_attachments.size(); ai++) {
		const Attachment& att = m.m_attachments[ai];
		if (ai < ex.attachPics.size() && !ex.attachPics[ai].url.empty()) {
			DrawPicture(ex.attachPics[ai].rect, ex.attachPics[ai].url, top, att.m_fileName);
			continue;
		}
		ay = top + (ai < ex.attachPics.size() ? ex.attachPics[ai].rect.top : ay - top);
		int asc = Fonts::Ascent(FS_REGULAR, m_ctx.px);
		// a page with a folded corner
		int ih = asc + 1, iw = ih * 3 / 4, iy = ay + 3, fold = iw / 3;
		c.Fill(TEXT_X, iy, iw, ih, 0xffffff);
		c.HLine(TEXT_X, iy, iw - fold, m_ctx.muted);
		c.VLine(TEXT_X, iy, ih, m_ctx.muted);
		c.HLine(TEXT_X, iy + ih - 1, iw, m_ctx.muted);
		c.VLine(TEXT_X + iw - 1, iy + fold, ih - fold, m_ctx.muted);
		for (int k = 0; k <= fold; k++)
			c.Fill(TEXT_X + iw - fold - 1 + k, iy + k, 1, 1, m_ctx.muted);
		c.VLine(TEXT_X + iw - fold - 1, iy, fold, m_ctx.muted);
		c.HLine(TEXT_X + iw - fold - 1, iy + fold, fold + 1, m_ctx.muted);
		int x = TEXT_X + iw + 6;
		int nameW = Fonts::Draw(c, x, ay + asc + 2, att.m_fileName, FS_REGULAR, m_ctx.px, m_ctx.link);
		c.HLine(x, ay + asc + 4, nameW, m_ctx.link);
		x += nameW;
		Fonts::Draw(c, x + 8, ay + asc + 2, "(" + FormatSize(att.m_size) + ")", FS_REGULAR, m_ctx.px - 3, m_ctx.muted);
		ay += lh;
	}

	// embeds
	for (size_t i = 0; i < m.m_embeds.size() && i < ex.embedTops.size(); i++)
	{
		const RichEmbed& em = m.m_embeds[i];
		int y = top + ex.embedTops[i];
		int w = std::min(right, TEXT_X + 520) - TEXT_X;
		c.Fill(TEXT_X, y, w, ex.embedHeights[i], m_ctx.codeBg);
		c.Fill(TEXT_X, y, 4, ex.embedHeights[i], em.m_color ? (Rgb) em.m_color : m_ctx.codeFrame);
		int x = TEXT_X + 12;
		y += 6;
		if (!em.m_providerName.empty()) {
			int spx = m_ctx.px - 3;
			Fonts::Draw(c, x, y + Fonts::Ascent(FS_REGULAR, spx), Fonts::Elide(em.m_providerName, FS_REGULAR, spx, w - 16), FS_REGULAR, spx, m_ctx.muted);
			y += Fonts::LineHeight(FS_REGULAR, spx) + 2;
		}
		if (!em.m_authorName.empty()) {
			int spx = m_ctx.px - 2;
			Fonts::Draw(c, x, y + Fonts::Ascent(FS_BOLD, spx), Fonts::Elide(em.m_authorName, FS_BOLD, spx, w - 16), FS_BOLD, spx, m_ctx.fg);
			y += Fonts::LineHeight(FS_BOLD, spx) + 2;
		}
		if (!em.m_title.empty()) {
			Fonts::Draw(c, x, y + Fonts::Ascent(FS_BOLD, m_ctx.px), Fonts::Elide(em.m_title, FS_BOLD, m_ctx.px, w - 16), FS_BOLD, m_ctx.px,
				em.m_url.empty() ? m_ctx.fg : m_ctx.link);
		}
		if (ex.embedTexts[i])
			ex.embedTexts[i]->Draw(&m_ctx, top);
		if (i < ex.embedThumbs.size() && !ex.embedThumbs[i].url.empty())
			DrawPicture(ex.embedThumbs[i].rect, ex.embedThumbs[i].url, top, "");
		if (i < ex.embedImages.size() && !ex.embedImages[i].url.empty())
			DrawPicture(ex.embedImages[i].rect, ex.embedImages[i].url, top, "");
		if (!em.m_footerText.empty()) {
			int spx = m_ctx.px - 3;
			int fy = top + ex.embedTops[i] + ex.embedHeights[i] - 6 - Fonts::Descent(FS_REGULAR, spx);
			Fonts::Draw(c, x, fy, Fonts::Elide(em.m_footerText, FS_REGULAR, spx, w - 16), FS_REGULAR, spx, m_ctx.muted);
		}
	}
}

void MessageView::Paint()
{
	if (!XtIsRealized(m_area))
		return;

	Display* dpy = XtDisplay(m_area);
	Window win = XtWindow(m_area);
	if (!m_gc)
		m_gc = XCreateGC(dpy, win, 0, NULL);

	// the first exposure can come before any resize callback
	Dimension width = 0, height = 0;
	XtVaGetValues(m_area, XmNwidth, &width, XmNheight, &height, NULL);
	if (width != m_viewW || height != m_viewH) {
		bool widthChanged = width != m_viewW;
		m_viewW = width;
		m_viewH = height;
		if (widthChanged)
			LayoutAll();
		if (m_stickToBottom) {
			int maxY = std::max(0, m_contentHeight - m_viewH);
			m_scrollY = maxY;
		}
		UpdateScrollbar();
	}

	if (m_canvas.Width() != m_viewW || m_canvas.Height() != m_viewH)
		m_canvas.Resize(m_viewW, m_viewH);

	m_canvas.Fill(0, 0, m_viewW, m_viewH, m_ctx.bg);
	m_ctx.canvas = &m_canvas;

	if (!m_channel) {
		const char* hint = "Pick a channel on the left.";
		int tw = Fonts::Measure(hint, FS_ITALIC, m_ctx.px + 2);
		Fonts::Draw(m_canvas, (m_viewW - tw) / 2, m_viewH / 2, hint, FS_ITALIC, m_ctx.px + 2, m_ctx.muted);
	}

	for (auto& it : m_items) {
		int top = it.y - m_scrollY;
		if (top + it.height < 0 || top > m_viewH)
			continue;
		PaintItem(it, top);
	}

	m_ctx.canvas = nullptr;
	m_canvas.Present(m_fmt, win, m_gc, 0, 0, m_viewW, m_viewH, 0, 0);
	RequestVisibleGaps();
}
