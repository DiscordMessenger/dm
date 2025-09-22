#include "ChatWindow.hpp"
#include "../discord/DiscordInstance.hpp"

ChatWindow::ChatWindow()
{
	m_chatView = std::make_shared<ChatView>();

	if (GetDiscordInstance())
		GetDiscordInstance()->RegisterView(m_chatView);
}

ChatWindow::~ChatWindow()
{
	if (GetDiscordInstance())
		GetDiscordInstance()->UnregisterView(m_chatView);
}

ChatViewPtr ChatWindow::GetChatView() const
{
	return m_chatView;
}

Snowflake ChatWindow::GetCurrentGuildID() const
{
	return m_chatView->GetCurrentGuildID();
}

Snowflake ChatWindow::GetCurrentChannelID() const
{
	return m_chatView->GetCurrentChannelID();
}

Guild* ChatWindow::GetCurrentGuild()
{
	return m_chatView->GetCurrentGuild();
}

Channel* ChatWindow::GetCurrentChannel()
{
	return m_chatView->GetCurrentChannel();
}

void ChatWindow::SetCurrentGuildID(Snowflake sf)
{
	m_chatView->SetCurrentGuildID(sf);
}

void ChatWindow::SetCurrentChannelID(Snowflake sf)
{
	m_chatView->SetCurrentChannelID(sf);
}
