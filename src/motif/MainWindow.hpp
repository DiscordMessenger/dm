#pragma once

#include <map>
#include <string>
#include <vector>

#include "Xm.hpp"

#include "models/Snowflake.hpp"
#include "Canvas.hpp"

class MessageView;
class IconList;

// The main window: guilds, channels, the message view with its editor and
// header, the member list, a status line and the menus.
class MainWindow
{
public:
	MainWindow(Widget toplevel, const PixelFormat& fmt);

	Widget GetShell() const { return m_shell; }
	MessageView* GetMessageView() const { return m_messages; }

	void UpdateGuildList();
	void UpdateSelectedGuild();
	void UpdateChannelList();
	void UpdateSelectedChannel();
	void UpdateMemberList();
	void UpdateHeader();
	void UpdateTitle();

	void OnTyping(Snowflake user, Snowflake guild, Snowflake channel, time_t when);
	void OnStopTyping(Snowflake channel, Snowflake user);
	void SetStatus(const std::string& text);

	// Shows a modal error box.
	void ShowError(const std::string& text);

	bool IsIconic() const;

	// Images arrived: repaint what may show them.
	void OnImagesChanged();

	// Sample rows for --demo.
	void ShowDemoLists();

private:
	void OnGuildPicked(Snowflake sf);
	void OnChannelPicked(Snowflake sf);
	static void ListRepaintCB(XtPointer, XtIntervalId*);
	static void SendCB(Widget, XtPointer, XtPointer);
	static void EditorChangedCB(Widget, XtPointer, XtPointer);
	static void MenuCB(Widget, XtPointer, XtPointer);
	static void TypingTimerCB(XtPointer, XtIntervalId*);

	void BuildMenus(Widget menubar);
	void SendFromEditor();
	void UpdateTypingStatus();

	Widget m_shell, m_main, m_form;
	Widget m_guildList, m_channelList, m_memberList;
	Widget m_header, m_editor, m_sendButton, m_status;
	Widget m_memberPane;
	MessageView* m_messages;
	IconList* m_guilds;
	IconList* m_channels;
	IconList* m_members;
	XtIntervalId m_listRepaintTimer = 0;


	// typing: channel -> user -> when it expires
	std::map<Snowflake, std::map<Snowflake, time_t>> m_typing;
	XtIntervalId m_typingTimer = 0;
	time_t m_lastTypingSent = 0;
	std::string m_statusText;
	bool m_memberListShown = true;
};

MainWindow* GetMainWindow();
