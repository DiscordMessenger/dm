#include "Xm.hpp"
#include "MainWindow.hpp"

#include <algorithm>
#include <cstring>
#include <ctime>

#include <X11/Xutil.h>
#include <Xm/CascadeB.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/List.h>
#include <Xm/MainW.h>
#include <Xm/MessageB.h>
#include <Xm/PushB.h>
#include <Xm/RowColumn.h>
#include <Xm/Separator.h>
#include <Xm/Text.h>
#include <Xm/ToggleB.h>

#include "DiscordInstance.hpp"
#include "Frontend.hpp"
#include "config/LocalSettings.hpp"
#include "state/ProfileCache.hpp"
#include "MessageView.hpp"
#include "Theme.hpp"
#include "IconList.hpp"
#include "models/ActiveStatus.hpp"

static MainWindow* g_pMainWindow;

MainWindow* GetMainWindow()
{
	return g_pMainWindow;
}

// Menu items, as client data of the push buttons
enum
{
	MI_RECONNECT = 1,
	MI_LOGOUT,
	MI_QUIT,
	MI_BIGGER,
	MI_SMALLER,
	MI_MEMBERS,
	MI_MARKREAD,
	MI_ABOUT,
};

void RequestLogout();   // Main.cpp
void RequestReconnect(); // Main.cpp
int AddVisualArgs(Arg* args, int n); // Main.cpp


MainWindow::MainWindow(Widget toplevel, const PixelFormat& fmt)
{
	g_pMainWindow = this;
	m_shell = toplevel;

	m_main = XtVaCreateManagedWidget("main", xmMainWindowWidgetClass, toplevel, NULL);

	Widget menubar = XmCreateMenuBar(m_main, (char*) "menubar", NULL, 0);
	BuildMenus(menubar);
	XtManageChild(menubar);

	m_form = XtVaCreateWidget("form", xmFormWidgetClass, m_main,
		XmNwidth, 1000,
		XmNheight, 680,
		NULL);
	InitPalette(m_form);

	Arg args[16];
	int n;

	// guilds on the left, then channels; members on the right
	m_guilds = new IconList(m_form, "guilds", fmt, 28, true);
	m_guildList = m_guilds->GetWidget();
	XtVaSetValues(m_guildList,
		XmNtopAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNleftAttachment, XmATTACH_FORM,
		XmNtopOffset, 4, XmNleftOffset, 4, XmNbottomOffset, 4,
		XmNwidth, 200,
		NULL);
	m_guilds->SetSelectCallback([this](Snowflake sf) { OnGuildPicked(sf); });

	m_channels = new IconList(m_form, "channels", fmt, 22, false);
	m_channelList = m_channels->GetWidget();
	XtVaSetValues(m_channelList,
		XmNtopAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNleftAttachment, XmATTACH_WIDGET,
		XmNleftWidget, m_guildList,
		XmNtopOffset, 4, XmNleftOffset, 2, XmNbottomOffset, 4,
		XmNwidth, 210,
		NULL);
	m_channels->SetSelectCallback([this](Snowflake sf) { OnChannelPicked(sf); });

	m_members = new IconList(m_form, "members", fmt, 24, false);
	m_memberList = m_members->GetWidget();
	XtVaSetValues(m_memberList,
		XmNtopAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		XmNtopOffset, 4, XmNrightOffset, 4, XmNbottomOffset, 4,
		XmNwidth, 200,
		NULL);
	m_memberPane = m_memberList;
	m_members->SetSelectCallback([](Snowflake) {});

	// the middle: header, messages, editor, status
	m_header = XtVaCreateManagedWidget("header", xmLabelWidgetClass, m_form,
		XmNtopAttachment, XmATTACH_FORM,
		XmNleftAttachment, XmATTACH_WIDGET,
		XmNleftWidget, m_channelList,
		XmNrightAttachment, XmATTACH_WIDGET,
		XmNrightWidget, m_memberPane,
		XmNtopOffset, 6,
		XmNleftOffset, 6,
		XmNrightOffset, 6,
		XmNalignment, XmALIGNMENT_BEGINNING,
		NULL);

	m_status = XtVaCreateManagedWidget("status", xmLabelWidgetClass, m_form,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNleftAttachment, XmATTACH_WIDGET,
		XmNleftWidget, m_channelList,
		XmNrightAttachment, XmATTACH_WIDGET,
		XmNrightWidget, m_memberPane,
		XmNbottomOffset, 4,
		XmNleftOffset, 6,
		XmNrightOffset, 6,
		XmNalignment, XmALIGNMENT_BEGINNING,
		NULL);

	m_sendButton = XtVaCreateManagedWidget("Send", xmPushButtonWidgetClass, m_form,
		XmNbottomAttachment, XmATTACH_WIDGET,
		XmNbottomWidget, m_status,
		XmNrightAttachment, XmATTACH_WIDGET,
		XmNrightWidget, m_memberPane,
		XmNbottomOffset, 4,
		XmNrightOffset, 6,
		NULL);
	XtAddCallback(m_sendButton, XmNactivateCallback, SendCB, this);

	n = 0;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNbottomWidget, m_status); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, m_channelList); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNrightWidget, m_sendButton); n++;
	XtSetArg(args[n], XmNbottomOffset, 4); n++;
	XtSetArg(args[n], XmNleftOffset, 6); n++;
	XtSetArg(args[n], XmNrightOffset, 6); n++;
	XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT); n++;
	XtSetArg(args[n], XmNrows, 3); n++;
	XtSetArg(args[n], XmNwordWrap, True); n++;
	XtSetArg(args[n], XmNscrollHorizontal, False); n++;
	m_editor = XmCreateScrolledText(m_form, (char*) "editor", args, n);
	XtManageChild(m_editor);
	// Return sends; Shift+Return starts a new line.
	XtOverrideTranslations(m_editor, XtParseTranslationTable(
		"Shift<Key>Return: newline()\n"
		"<Key>Return: activate()\n"
		"<Key>KP_Enter: activate()"));
	XtAddCallback(m_editor, XmNactivateCallback, SendCB, this);
	XtAddCallback(m_editor, XmNvalueChangedCallback, EditorChangedCB, this);

	m_messages = new MessageView(m_form, fmt);
	XtVaSetValues(m_messages->GetWidget(),
		XmNtopAttachment, XmATTACH_WIDGET,
		XmNtopWidget, m_header,
		XmNbottomAttachment, XmATTACH_WIDGET,
		XmNbottomWidget, XtParent(m_editor),
		XmNleftAttachment, XmATTACH_WIDGET,
		XmNleftWidget, m_channelList,
		XmNrightAttachment, XmATTACH_WIDGET,
		XmNrightWidget, m_memberPane,
		XmNtopOffset, 6,
		XmNbottomOffset, 6,
		XmNleftOffset, 6,
		XmNrightOffset, 6,
		NULL);

	XtManageChild(m_form);
	XmMainWindowSetAreas(m_main, menubar, NULL, NULL, NULL, m_form);

	SetStatus("");
	UpdateHeader();
}

void MainWindow::BuildMenus(Widget menubar)
{
	struct Item { const char* label; int id; char mnemonic; };
	struct Menu { const char* label; char mnemonic; std::vector<Item> items; };
	std::vector<Menu> menus = {
		{ "File", 'F', {
			{ "Reconnect to Discord", MI_RECONNECT, 'R' },
			{ "Mark Channel Read", MI_MARKREAD, 'M' },
			{ "-", 0, 0 },
			{ "Log Out", MI_LOGOUT, 'L' },
			{ "Quit", MI_QUIT, 'Q' },
		} },
		{ "View", 'V', {
			{ "Larger Text", MI_BIGGER, 'L' },
			{ "Smaller Text", MI_SMALLER, 'S' },
			{ "-", 0, 0 },
			{ "Member List", MI_MEMBERS, 'M' },
		} },
		{ "Help", 'H', {
			{ "About Discord Messenger", MI_ABOUT, 'A' },
		} },
	};

	for (auto& menu : menus)
	{
		Arg args[4];
		int n = AddVisualArgs(args, 0);
		Widget pulldown = XmCreatePulldownMenu(menubar, (char*) "pulldown", args, n);
		Widget cascade = XtVaCreateManagedWidget(menu.label, xmCascadeButtonWidgetClass, menubar,
			XmNsubMenuId, pulldown,
			XmNmnemonic, (KeySym) menu.mnemonic,
			NULL);
		if (!strcmp(menu.label, "Help"))
			XtVaSetValues(menubar, XmNmenuHelpWidget, cascade, NULL);

		for (auto& item : menu.items)
		{
			if (!strcmp(item.label, "-")) {
				XtVaCreateManagedWidget("sep", xmSeparatorWidgetClass, pulldown, NULL);
				continue;
			}
			Widget b;
			if (item.id == MI_MEMBERS) {
				b = XtVaCreateManagedWidget(item.label, xmToggleButtonWidgetClass, pulldown,
					XmNset, True, XmNmnemonic, (KeySym) item.mnemonic, NULL);
				XtAddCallback(b, XmNvalueChangedCallback, MenuCB, (XtPointer) (long) item.id);
			}
			else {
				b = XtVaCreateManagedWidget(item.label, xmPushButtonWidgetClass, pulldown,
					XmNmnemonic, (KeySym) item.mnemonic, NULL);
				XtAddCallback(b, XmNactivateCallback, MenuCB, (XtPointer) (long) item.id);
			}
		}
	}
}

void MainWindow::MenuCB(Widget w, XtPointer client, XtPointer)
{
	MainWindow* self = g_pMainWindow;
	switch ((int) (long) client)
	{
		case MI_RECONNECT:
			RequestReconnect();
			break;
		case MI_LOGOUT:
			RequestLogout();
			break;
		case MI_QUIT:
			GetFrontend()->RequestQuit();
			break;
		case MI_BIGGER:
		case MI_SMALLER:
			SetTextSize(GetTextSize() + ((int) (long) client == MI_BIGGER ? 1 : -1));
			SaveMotifConfig();
			self->m_messages->Relayout();
			self->UpdateGuildList();
			self->UpdateChannelList();
			self->UpdateMemberList();
			break;
		case MI_MEMBERS:
			self->m_memberListShown = XmToggleButtonGetState(w);
			if (self->m_memberListShown) {
				XtManageChild(self->m_memberPane);
				XtVaSetValues(self->m_messages->GetWidget(), XmNrightAttachment, XmATTACH_WIDGET, XmNrightWidget, self->m_memberPane, NULL);
				XtVaSetValues(self->m_header, XmNrightAttachment, XmATTACH_WIDGET, XmNrightWidget, self->m_memberPane, NULL);
				XtVaSetValues(self->m_sendButton, XmNrightAttachment, XmATTACH_WIDGET, XmNrightWidget, self->m_memberPane, NULL);
				XtVaSetValues(self->m_status, XmNrightAttachment, XmATTACH_WIDGET, XmNrightWidget, self->m_memberPane, NULL);
			}
			else {
				XtUnmanageChild(self->m_memberPane);
				XtVaSetValues(self->m_messages->GetWidget(), XmNrightAttachment, XmATTACH_FORM, NULL);
				XtVaSetValues(self->m_header, XmNrightAttachment, XmATTACH_FORM, NULL);
				XtVaSetValues(self->m_sendButton, XmNrightAttachment, XmATTACH_FORM, NULL);
				XtVaSetValues(self->m_status, XmNrightAttachment, XmATTACH_FORM, NULL);
			}
			break;
		case MI_MARKREAD: {
			DiscordInstance* pInst = GetDiscordInstance();
			if (pInst && pInst->GetCurrentChannelID())
				pInst->RequestAcknowledgeChannel(pInst->GetCurrentChannelID());
			break;
		}
		case MI_ABOUT:
			self->ShowError("Discord Messenger for IRIX\n\nA Discord-compatible messenger by iProgramInCpp and contributors,\nported to IRIX with Motif.\n\nNote: third-party clients are against Discord's terms of service.");
			break;
	}
}

void MainWindow::ShowError(const std::string& text)
{
	Arg args[8];
	int n = 0;
	XmString msg = XmStringCreateLtoR((char*) Utf8ToLatin1(text).c_str(), (char*) XmFONTLIST_DEFAULT_TAG);
	XtSetArg(args[n], XmNmessageString, msg); n++;
	XtSetArg(args[n], XmNdialogStyle, XmDIALOG_FULL_APPLICATION_MODAL); n++;
	XtSetArg(args[n], XmNtitle, "Discord Messenger"); n++;
	n = AddVisualArgs(args, n);
	Widget dlg = XmCreateInformationDialog(m_shell, (char*) "message", args, n);
	XmStringFree(msg);
	XtUnmanageChild(XmMessageBoxGetChild(dlg, XmDIALOG_CANCEL_BUTTON));
	XtUnmanageChild(XmMessageBoxGetChild(dlg, XmDIALOG_HELP_BUTTON));
	XtAddCallback(dlg, XmNokCallback, [](Widget w, XtPointer, XtPointer) { XtDestroyWidget(XtParent(w)); }, NULL);
	XtManageChild(dlg);
}

bool MainWindow::IsIconic() const
{
	if (!XtIsRealized(m_shell))
		return false;
	XWindowAttributes wa;
	XGetWindowAttributes(XtDisplay(m_shell), XtWindow(m_shell), &wa);
	return wa.map_state != IsViewable;
}

void MainWindow::UpdateGuildList()
{
	DiscordInstance* pInst = GetDiscordInstance();
	std::vector<Snowflake> ids;
	pInst->GetGuildIDsOrdered(ids, true);

	std::vector<IconRow> rows;
	bool inFolder = false;
	for (Snowflake sf : ids)
	{
		if (sf == 1) {
			// the gap after Direct Messages
			IconRow r;
			r.type = IconRow::SPACE;
			rows.push_back(r);
			continue;
		}
		if (sf & BIT_FOLDER) {
			inFolder = sf != BIT_FOLDER;
			if (inFolder) {
				IconRow r;
				r.type = IconRow::HEADER;
				r.text = pInst->GetGuildFolderName(sf & ~BIT_FOLDER);
				rows.push_back(r);
			}
			continue;
		}
		IconRow r;
		r.id = sf;
		r.indent = inFolder ? 8 : 0;
		if (sf == 0) {
			r.text = GetFrontend()->GetDirectMessagesText();
			r.glyph = "@";
		}
		else {
			Guild* pGuild = pInst->GetGuild(sf);
			if (!pGuild)
				continue;
			r.text = pGuild->m_name;
			r.initials = true;
			if (!pGuild->m_avatarlnk.empty()) {
				r.hasImage = true;
				r.imageKind = ImageCache::ICON;
				r.imagePlace = pGuild->m_avatarlnk;
				r.imageSf = sf;
			}
			// unread: any channel the user can see with newer messages
			int mentions = 0;
			bool unread = false;
			for (auto& ch : pGuild->m_channels) {
				mentions += ch.m_mentionCount;
				if (ch.HasUnreadMessages() && ch.HasPermissionConst(PERM_VIEW_CHANNEL) &&
					!pInst->IsChannelMuted(sf, ch.m_snowflake))
					unread = true;
			}
			r.unread = unread;
			r.mentions = mentions;
		}
		rows.push_back(r);
	}
	m_guilds->SetRows(rows, pInst->GetCurrentGuildID());
}

void MainWindow::UpdateSelectedGuild()
{
	UpdateGuildList();
	UpdateChannelList();
	UpdateMemberList();
	UpdateHeader();
	UpdateTitle();
}

static bool IsTextChannel(const Channel& ch)
{
	switch (ch.m_channelType) {
		case Channel::TEXT: case Channel::DM: case Channel::GROUPDM: case Channel::NEWS:
		case Channel::NEWSTHREAD: case Channel::PUBTHREAD: case Channel::PRIVTHREAD:
			return true;
		default:
			return false;
	}
}

void MainWindow::UpdateChannelList()
{
	DiscordInstance* pInst = GetDiscordInstance();
	Guild* pGuild = pInst->GetCurrentGuild();
	std::vector<IconRow> rows;

	if (pGuild && !pGuild->m_bChannelsLoaded) {
		pGuild->RequestFetchChannels();
		IconRow r;
		r.type = IconRow::HEADER;
		r.text = GetFrontend()->GetPleaseWaitText();
		rows.push_back(r);
	}
	else if (pGuild)
	{
		std::vector<const Channel*> chans;
		for (auto& ch : pGuild->m_channels)
			chans.push_back(&ch);
		std::stable_sort(chans.begin(), chans.end(), [](const Channel* a, const Channel* b) { return *a < *b; });

		auto addChannel = [&](const Channel* ch) {
			if (!ch->HasPermissionConst(PERM_VIEW_CHANNEL))
				return;
			IconRow r;
			r.id = ch->m_snowflake;
			r.text = ch->m_name;
			r.mentions = ch->m_mentionCount;
			r.unread = ch->HasUnreadMessages() && !pInst->IsChannelMuted(pGuild->m_snowflake, ch->m_snowflake);
			r.selectable = IsTextChannel(*ch);
			r.dim = !r.selectable;
			if (ch->m_channelType == Channel::DM) {
				Snowflake who = ch->m_recipients.empty() ? 0 : ch->m_recipients[0];
				r.hasImage = true;
				r.imageKind = ch->m_avatarLnk.empty() ? ImageCache::DEFAULT_AVATAR : ImageCache::AVATAR;
				r.imagePlace = ch->m_avatarLnk;
				r.imageSf = who;
				r.colorSeed = who;
				Profile* pf = who ? GetProfileCache()->LookupProfile(who, "", "", "", false) : nullptr;
				r.status = pf ? (int) pf->m_activeStatus : -1;
			}
			else if (ch->m_channelType == Channel::GROUPDM) {
				r.initials = true;
				if (!ch->m_avatarLnk.empty()) {
					r.hasImage = true;
					r.imageKind = ImageCache::CHANNEL_ICON;
					r.imagePlace = ch->m_avatarLnk;
					r.imageSf = ch->m_snowflake;
				}
			}
			else if (ch->m_channelType == Channel::VOICE || ch->m_channelType == Channel::STAGEVOICE)
				r.glyph = "\xe2\x99\xaa"; // a note: voice
			else if (ch->m_channelType == Channel::FORUM || ch->m_channelType == Channel::MEDIA)
				r.glyph = "\xe2\x96\xa4";
			else
				r.glyph = "#";
			rows.push_back(r);
		};

		// channels outside categories first, then each category's
		for (const Channel* ch : chans)
			if (!ch->IsCategory() && ch->m_parentCateg == 0)
				addChannel(ch);
		for (const Channel* cat : chans)
		{
			if (!cat->IsCategory())
				continue;
			IconRow h;
			h.type = IconRow::HEADER;
			h.text = cat->m_name;
			for (auto& c : h.text)
				if (c >= 'a' && c <= 'z') c -= 32;
			size_t before = rows.size();
			rows.push_back(h);
			for (const Channel* ch : chans)
				if (!ch->IsCategory() && ch->m_parentCateg == cat->m_snowflake)
					addChannel(ch);
			if (rows.size() == before + 1)
				rows.pop_back(); // an empty (or wholly hidden) category
		}
	}

	m_channels->SetRows(rows, pInst->GetCurrentChannelID());
}

void MainWindow::UpdateSelectedChannel()
{
	DiscordInstance* pInst = GetDiscordInstance();
	UpdateChannelList();
	UpdateHeader();
	UpdateTitle();
	UpdateTypingStatus();

	m_messages->SetChannel(pInst->GetCurrentGuildID(), pInst->GetCurrentChannelID());
	if (pInst->GetCurrentChannel())
		pInst->HandledChannelSwitch();
	m_messages->Refresh();
}

// The colour of the member's highest coloured role, or 0.
Rgb RoleColor(Snowflake user, Snowflake guild)
{
	Guild* pGuild = GetDiscordInstance()->GetGuild(guild);
	if (!pGuild || !guild)
		return 0;
	Profile* pf = GetProfileCache()->LookupProfile(user, "", "", "", false);
	if (!pf)
		return 0;
	auto gm = pf->m_guildMembers.find(guild);
	if (gm == pf->m_guildMembers.end())
		return 0;
	int bestPos = -1;
	Rgb best = 0;
	for (Snowflake role : gm->second.m_roles) {
		auto it = pGuild->m_roles.find(role);
		if (it == pGuild->m_roles.end())
			continue;
		if (it->second.m_colorOriginal && it->second.m_position > bestPos) {
			bestPos = it->second.m_position;
			best = (Rgb) it->second.m_colorOriginal;
		}
	}
	return best;
}

void MainWindow::UpdateMemberList()
{
	DiscordInstance* pInst = GetDiscordInstance();
	Guild* pGuild = pInst->GetCurrentGuild();
	std::vector<IconRow> rows;
	if (pGuild && pGuild->m_snowflake != 0)
	{
		for (Snowflake sf : pGuild->m_members)
		{
			GuildMember* gm = pGuild->GetGuildMember(sf);
			if (!gm)
				continue;
			if (gm->m_bIsGroup) {
				if (gm->m_groupCount) {
					IconRow h;
					h.type = IconRow::HEADER;
					h.text = pGuild->GetGroupName(gm->m_groupId) + " \xe2\x80\x94 " + std::to_string(gm->m_groupCount);
					rows.push_back(h);
				}
				continue;
			}
			Profile* p = GetProfileCache()->LookupProfile(gm->m_user, "", "", "", false);
			IconRow r;
			r.id = gm->m_user;
			r.text = p ? p->GetName(pGuild->m_snowflake) : std::string("?");
			r.hasImage = true;
			r.colorSeed = gm->m_user;
			std::string av = !gm->m_avatar.empty() ? gm->m_avatar : (p ? p->m_avatarlnk : "");
			r.imageKind = av.empty() ? ImageCache::DEFAULT_AVATAR : ImageCache::AVATAR;
			r.imagePlace = av;
			r.imageSf = gm->m_user;
			r.status = p ? (int) p->m_activeStatus : -1;
			r.dim = p && p->m_activeStatus == STATUS_OFFLINE;
			r.textColor = RoleColor(gm->m_user, pGuild->m_snowflake);
			rows.push_back(r);
		}
	}
	m_members->SetRows(rows, 0);
}

void MainWindow::ShowDemoLists()
{
	auto item = [](Snowflake id, const char* text) { IconRow r; r.id = id; r.text = text; return r; };
	auto header = [](const char* text) { IconRow r; r.type = IconRow::HEADER; r.text = text; return r; };
	std::vector<IconRow> g;
	IconRow dm = item(1, "Direct Messages"); dm.glyph = "@"; g.push_back(dm);
	IconRow sp; sp.type = IconRow::SPACE; g.push_back(sp);
	const char* names[] = { "Silicon Graphics User Group", "Vintage Computer CH", "Demoscene", "IRIX Network \xe2\x9c\xa8", "Octane Owners" };
	for (int i = 0; i < 5; i++) {
		IconRow r = item(100 + i, names[i]);
		r.initials = true;
		r.colorSeed = (Snowflake) (i * 3 + 1) << 22;
		r.unread = i == 1;
		r.mentions = i == 2 ? 3 : 0;
		if (i == 0) {
			r.hasImage = true;
			r.imageKind = ImageCache::DEFAULT_AVATAR;
			r.imageSf = (Snowflake) 2 << 22;
		}
		g.push_back(r);
	}
	g.push_back(header("Folder"));
	IconRow f = item(200, "Tezro Fans"); f.initials = true; f.indent = 8; g.push_back(f);
	m_guilds->SetRows(g, 100);

	std::vector<IconRow> c;
	c.push_back(header("INFORMATION"));
	IconRow c1 = item(300, "rules"); c1.glyph = "#"; c.push_back(c1);
	IconRow c2 = item(301, "announcements \xf0\x9f\x93\xa2"); c2.glyph = "#"; c2.unread = true; c.push_back(c2);
	c.push_back(header("TEXT CHANNELS"));
	IconRow c3 = item(302, "general"); c3.glyph = "#"; c.push_back(c3);
	IconRow c4 = item(303, "marketplace"); c4.glyph = "#"; c4.mentions = 2; c4.unread = true; c.push_back(c4);
	IconRow c5 = item(304, "Lounge"); c5.glyph = "\xe2\x99\xaa"; c5.selectable = false; c5.dim = true; c.push_back(c5);
	m_channels->SetRows(c, 302);

	std::vector<IconRow> m;
	m.push_back(header("Online \xe2\x80\x94 3"));
	const char* who[] = { "Ada", "Grace", "Dennis" };
	Rgb colors[] = { 0xe91e63, 0x3498db, 0 };
	for (int i = 0; i < 3; i++) {
		IconRow r = item(400 + i, who[i]);
		r.hasImage = true;
		r.imageKind = ImageCache::DEFAULT_AVATAR;
		r.imageSf = (Snowflake) (i + 3) << 22;
		r.status = i + 1;
		r.textColor = colors[i];
		m.push_back(r);
	}
	m.push_back(header("Offline \xe2\x80\x94 1"));
	IconRow off = item(410, "Linus");
	off.hasImage = true; off.imageKind = ImageCache::DEFAULT_AVATAR; off.imageSf = (Snowflake) 7 << 22;
	off.status = 0; off.dim = true;
	m.push_back(off);
	m_members->SetRows(m, 0);
}

void MainWindow::OnImagesChanged()
{
	m_messages->ImagesChanged();
	if (!m_listRepaintTimer)
		m_listRepaintTimer = XtAppAddTimeOut(XtWidgetToApplicationContext(m_shell), 100, ListRepaintCB, this);
}

void MainWindow::ListRepaintCB(XtPointer client, XtIntervalId*)
{
	MainWindow* self = (MainWindow*) client;
	self->m_listRepaintTimer = 0;
	self->m_guilds->Repaint();
	self->m_channels->Repaint();
	self->m_members->Repaint();
}

void MainWindow::UpdateHeader()
{
	DiscordInstance* pInst = GetDiscordInstance();
	Channel* pChan = pInst ? pInst->GetCurrentChannel() : nullptr;
	std::string text;
	if (pChan) {
		text = (pChan->IsDM() ? "@" : "#") + pChan->m_name;
		if (!pChan->m_topic.empty()) {
			std::string topic = pChan->m_topic;
			std::replace(topic.begin(), topic.end(), '\n', ' ');
			if (topic.size() > 120)
				topic = topic.substr(0, 117) + "...";
			text += "   |   " + topic;
		}
	}
	XmString xs = MakeXmString(text.empty() ? std::string(" ") : text);
	XtVaSetValues(m_header, XmNlabelString, xs, NULL);
	XmStringFree(xs);
}

void MainWindow::UpdateTitle()
{
	DiscordInstance* pInst = GetDiscordInstance();
	Channel* pChan = pInst ? pInst->GetCurrentChannel() : nullptr;
	std::string title = "Discord Messenger";
	if (pChan)
		title = (pChan->IsDM() ? "@" : "#") + pChan->m_name + " - " + title;
	std::string l1 = Utf8ToLatin1(title);
	XtVaSetValues(m_shell, XmNtitle, l1.c_str(), XmNiconName, "Discord", NULL);
}

void MainWindow::SetStatus(const std::string& text)
{
	m_statusText = text;
	UpdateTypingStatus();
}

void MainWindow::UpdateTypingStatus()
{
	DiscordInstance* pInst = GetDiscordInstance();
	std::string text = m_statusText;
	if (pInst)
	{
		auto it = m_typing.find(pInst->GetCurrentChannelID());
		if (it != m_typing.end() && !it->second.empty())
		{
			std::vector<std::string> names;
			for (auto& t : it->second) {
				Profile* p = GetProfileCache()->LookupProfile(t.first, "", "", "", false);
				names.push_back(p ? p->GetName(pInst->GetCurrentGuildID()) : "Someone");
			}
			std::string who;
			if (names.size() > 3)
				who = "Several people are typing...";
			else {
				for (size_t i = 0; i < names.size(); i++)
					who += (i ? (i + 1 == names.size() ? " and " : ", ") : "") + names[i];
				who += names.size() == 1 ? " is typing..." : " are typing...";
			}
			text = who;
		}
	}
	XmString xs = MakeXmString(text.empty() ? std::string(" ") : text);
	XtVaSetValues(m_status, XmNlabelString, xs, NULL);
	XmStringFree(xs);
}

void MainWindow::OnTyping(Snowflake user, Snowflake guild, Snowflake channel, time_t when)
{
	DiscordInstance* pInst = GetDiscordInstance();
	if (user == pInst->GetUserID())
		return;
	m_typing[channel][user] = time(NULL) + 10;
	UpdateTypingStatus();
	if (!m_typingTimer)
		m_typingTimer = XtAppAddTimeOut(XtWidgetToApplicationContext(m_shell), 1000, TypingTimerCB, this);
}

void MainWindow::OnStopTyping(Snowflake channel, Snowflake user)
{
	auto it = m_typing.find(channel);
	if (it == m_typing.end())
		return;
	it->second.erase(user);
	UpdateTypingStatus();
}

void MainWindow::TypingTimerCB(XtPointer client, XtIntervalId*)
{
	MainWindow* self = (MainWindow*) client;
	self->m_typingTimer = 0;
	time_t now = time(NULL);
	bool any = false;
	for (auto& ch : self->m_typing) {
		for (auto it = ch.second.begin(); it != ch.second.end(); ) {
			if (it->second <= now)
				it = ch.second.erase(it);
			else {
				++it;
				any = true;
			}
		}
	}
	self->UpdateTypingStatus();
	if (any)
		self->m_typingTimer = XtAppAddTimeOut(XtWidgetToApplicationContext(self->m_shell), 1000, TypingTimerCB, self);
}

void MainWindow::OnGuildPicked(Snowflake sf)
{
	GetDiscordInstance()->OnSelectGuild(sf);
}

void MainWindow::OnChannelPicked(Snowflake sf)
{
	GetDiscordInstance()->OnSelectChannel(sf);
}

void MainWindow::SendCB(Widget, XtPointer client, XtPointer)
{
	((MainWindow*) client)->SendFromEditor();
}

void MainWindow::EditorChangedCB(Widget, XtPointer client, XtPointer)
{
	MainWindow* self = (MainWindow*) client;
	DiscordInstance* pInst = GetDiscordInstance();
	if (!pInst || !pInst->GetCurrentChannelID())
		return;
	char* text = XmTextGetString(self->m_editor);
	bool empty = !text || !*text;
	XtFree(text);
	time_t now = time(NULL);
	if (!empty && now - self->m_lastTypingSent >= 8) {
		self->m_lastTypingSent = now;
		pInst->Typing();
	}
}

void MainWindow::SendFromEditor()
{
	DiscordInstance* pInst = GetDiscordInstance();
	char* raw = XmTextGetString(m_editor);
	std::string text = raw ? raw : "";
	XtFree(raw);

	// trim
	size_t a = text.find_first_not_of(" \t\n"), b = text.find_last_not_of(" \t\n");
	if (a == std::string::npos || !pInst || !pInst->GetCurrentChannelID())
		return;
	text = text.substr(a, b - a + 1);

	std::string utf8 = Latin1ToUtf8(text);
	Snowflake tempSf = 0;
	if (pInst->SendMessageToCurrentChannel(utf8, tempSf)) {
		XmTextSetString(m_editor, (char*) "");
		m_lastTypingSent = 0;
		m_messages->ScrollToBottom();
	}
}
