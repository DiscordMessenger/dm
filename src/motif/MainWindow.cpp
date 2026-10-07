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

static Widget MakeScrolledList(Widget parent, const char* name, Arg* args, int n)
{
	XtSetArg(args[n], XmNselectionPolicy, XmBROWSE_SELECT); n++;
	XtSetArg(args[n], XmNscrollBarDisplayPolicy, XmAS_NEEDED); n++;
	XtSetArg(args[n], XmNlistSizePolicy, XmCONSTANT); n++;
	Widget list = XmCreateScrolledList(parent, (char*) name, args, n);
	XtManageChild(list);
	return list;
}

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

	Arg args[16];
	int n;

	// guilds, on the left
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNtopOffset, 4); n++;
	XtSetArg(args[n], XmNleftOffset, 4); n++;
	XtSetArg(args[n], XmNbottomOffset, 4); n++;
	XtSetArg(args[n], XmNwidth, 170); n++;
	m_guildList = MakeScrolledList(m_form, "guilds", args, n);
	XtAddCallback(m_guildList, XmNbrowseSelectionCallback, GuildSelectCB, this);

	// channels
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, XtParent(m_guildList)); n++;
	XtSetArg(args[n], XmNtopOffset, 4); n++;
	XtSetArg(args[n], XmNleftOffset, 4); n++;
	XtSetArg(args[n], XmNbottomOffset, 4); n++;
	XtSetArg(args[n], XmNwidth, 190); n++;
	m_channelList = MakeScrolledList(m_form, "channels", args, n);
	XtAddCallback(m_channelList, XmNbrowseSelectionCallback, ChannelSelectCB, this);

	// members, on the right
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNtopOffset, 4); n++;
	XtSetArg(args[n], XmNrightOffset, 4); n++;
	XtSetArg(args[n], XmNbottomOffset, 4); n++;
	XtSetArg(args[n], XmNwidth, 170); n++;
	m_memberList = MakeScrolledList(m_form, "members", args, n);
	m_memberPane = XtParent(m_memberList);

	// the middle: header, messages, editor, status
	m_header = XtVaCreateManagedWidget("header", xmLabelWidgetClass, m_form,
		XmNtopAttachment, XmATTACH_FORM,
		XmNleftAttachment, XmATTACH_WIDGET,
		XmNleftWidget, XtParent(m_channelList),
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
		XmNleftWidget, XtParent(m_channelList),
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
	XtSetArg(args[n], XmNleftWidget, XtParent(m_channelList)); n++;
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
		XmNleftWidget, XtParent(m_channelList),
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
			self->m_messages->Relayout();
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

static void SetListItems(Widget list, const std::vector<std::string>& rows, int selected)
{
	std::vector<XmString> items;
	for (auto& r : rows)
		items.push_back(MakeXmString(r));
	XtVaSetValues(list, XmNitems, items.empty() ? NULL : items.data(), XmNitemCount, (int) items.size(), NULL);
	for (auto& x : items)
		XmStringFree(x);
	if (selected >= 0) {
		XmListSelectPos(list, selected + 1, False);
		XmListSetKbdItemPos(list, selected + 1);
		int top = 0, visible = 0;
		XtVaGetValues(list, XmNtopItemPosition, &top, XmNvisibleItemCount, &visible, NULL);
		if (selected + 1 < top || selected + 1 >= top + visible)
			XmListSetPos(list, std::max(1, selected + 1 - visible / 2));
	}
	else
		XmListDeselectAllItems(list);
}

void MainWindow::UpdateGuildList()
{
	DiscordInstance* pInst = GetDiscordInstance();
	std::vector<Snowflake> ids;
	pInst->GetGuildIDsOrdered(ids, true);

	std::vector<std::string> rows;
	m_guildRows.clear();
	int selected = -1;
	bool inFolder = false;
	for (Snowflake sf : ids)
	{
		if (sf == 1)
			continue; // the UI gap after Direct Messages
		if (sf & BIT_FOLDER) {
			if (sf == BIT_FOLDER) {
				inFolder = false;
				continue;
			}
			inFolder = true;
			rows.push_back("[" + pInst->GetGuildFolderName(sf & ~BIT_FOLDER) + "]");
			m_guildRows.push_back(BIT_FOLDER);
			continue;
		}
		Guild* pGuild = pInst->GetGuild(sf);
		std::string name = sf == 0 ? GetFrontend()->GetDirectMessagesText() : (pGuild ? pGuild->m_name : "?");
		if (sf == pInst->GetCurrentGuildID())
			selected = (int) rows.size();
		rows.push_back((inFolder ? "   " : "") + name);
		m_guildRows.push_back(sf);
	}
	SetListItems(m_guildList, rows, selected);
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
	std::vector<std::string> rows;
	m_channelRows.clear();
	int selected = -1;

	if (pGuild && !pGuild->m_bChannelsLoaded) {
		pGuild->RequestFetchChannels();
		rows.push_back(GetFrontend()->GetPleaseWaitText());
		m_channelRows.push_back(0);
	}
	else if (pGuild)
	{
		std::vector<const Channel*> chans;
		for (auto& ch : pGuild->m_channels)
			chans.push_back(&ch);
		std::stable_sort(chans.begin(), chans.end(), [](const Channel* a, const Channel* b) { return *a < *b; });

		auto addChannel = [&](const Channel* ch, bool indent) {
			if (!ch->HasPermissionConst(PERM_VIEW_CHANNEL))
				return;
			std::string mark;
			if (ch->WasMentioned())
				mark = " (" + std::to_string(ch->m_mentionCount) + ")";
			else if (ch->HasUnreadMessages())
				mark = " *";
			std::string prefix = ch->IsDM() ? "@ " : IsTextChannel(*ch) ? "# " : "~ ";
			if (ch->m_channelType == Channel::GROUPDM)
				prefix = "@@ ";
			if (ch->m_snowflake == pInst->GetCurrentChannelID())
				selected = (int) rows.size();
			rows.push_back((indent ? "  " : "") + prefix + ch->m_name + mark);
			m_channelRows.push_back(IsTextChannel(*ch) ? ch->m_snowflake : 0);
		};

		// channels outside categories first, then each category's
		for (const Channel* ch : chans)
			if (!ch->IsCategory() && ch->m_parentCateg == 0)
				addChannel(ch, false);
		for (const Channel* cat : chans)
		{
			if (!cat->IsCategory())
				continue;
			size_t headerRow = rows.size();
			rows.push_back(Utf8ToLatin1(cat->m_name).empty() ? "?" : cat->m_name);
			m_channelRows.push_back(0);
			size_t before = rows.size();
			for (const Channel* ch : chans)
				if (!ch->IsCategory() && ch->m_parentCateg == cat->m_snowflake)
					addChannel(ch, true);
			if (rows.size() == before) {
				// an empty (or wholly hidden) category is not shown
				rows.erase(rows.begin() + headerRow);
				m_channelRows.erase(m_channelRows.begin() + headerRow);
			}
			else {
				// upper-case category names, as Discord shows them
				std::string& h = rows[headerRow];
				for (auto& c : h)
					if (c >= 'a' && c <= 'z') c -= 32;
			}
		}
	}

	SetListItems(m_channelList, rows, selected);
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

void MainWindow::UpdateMemberList()
{
	DiscordInstance* pInst = GetDiscordInstance();
	Guild* pGuild = pInst->GetCurrentGuild();
	std::vector<std::string> rows;
	if (pGuild && pGuild->m_snowflake != 0)
	{
		for (Snowflake sf : pGuild->m_members)
		{
			GuildMember* gm = pGuild->GetGuildMember(sf);
			if (!gm)
				continue;
			if (gm->m_bIsGroup) {
				if (gm->m_groupCount)
					rows.push_back(pGuild->GetGroupName(gm->m_groupId) + " - " + std::to_string(gm->m_groupCount));
				continue;
			}
			Profile* p = GetProfileCache()->LookupProfile(gm->m_user, "", "", "", false);
			rows.push_back("  " + (p ? p->GetName(pGuild->m_snowflake) : std::string("?")));
		}
	}
	SetListItems(m_memberList, rows, -1);
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

void MainWindow::GuildSelectCB(Widget, XtPointer client, XtPointer call)
{
	MainWindow* self = (MainWindow*) client;
	XmListCallbackStruct* cbs = (XmListCallbackStruct*) call;
	int row = cbs->item_position - 1;
	if (row < 0 || row >= (int) self->m_guildRows.size())
		return;
	Snowflake sf = self->m_guildRows[row];
	if (sf & BIT_FOLDER) {
		// folders are headings: keep the current guild selected
		self->UpdateGuildList();
		return;
	}
	GetDiscordInstance()->OnSelectGuild(sf);
}

void MainWindow::ChannelSelectCB(Widget, XtPointer client, XtPointer call)
{
	MainWindow* self = (MainWindow*) client;
	XmListCallbackStruct* cbs = (XmListCallbackStruct*) call;
	int row = cbs->item_position - 1;
	if (row < 0 || row >= (int) self->m_channelRows.size())
		return;
	Snowflake sf = self->m_channelRows[row];
	if (!sf) {
		// a category or a voice channel
		self->UpdateChannelList();
		return;
	}
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
