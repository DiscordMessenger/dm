#include "ChatView.hpp"
#include "DiscordInstance.hpp"

Guild* ChatView::GetCurrentGuild()
{
	return GetDiscordInstance()->GetGuild(m_guildID);
}

Channel* ChatView::GetCurrentChannel()
{
	auto guild = GetCurrentGuild();
	if (!guild)
		return nullptr;

	return guild->GetChannel(m_channelID);
}
