#include "ChatView.hpp"
#include "DiscordInstance.hpp"
#include "Frontend.hpp"

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

void ChatView::OnSelectChannel(Snowflake sf, bool bSendSubscriptionUpdate)
{
	if (m_channelID == sf)
		return;
	
	// check if there are any channels and select the first (for now)
	Guild* pGuild = GetDiscordInstance()->GetGuild(m_guildID);
	if (!pGuild) return;

	Channel* pChan = pGuild->GetChannel(sf);

	if (!pChan)
	{
		if (sf != 0)
			return;
	}
	else if (pChan->m_channelType == Channel::CATEGORY)
		return;

	// Check if we have permission to view the channel.
	auto& denyList = GetDiscordInstance()->m_channelDenyList;

	if (denyList.find(sf) != denyList.end() ||
		(pChan && !pChan->HasPermission(PERM_VIEW_CHANNEL)))
	{
		GetFrontend()->OnCantViewChannel(pChan->m_name);
		return;
	}

	GetDiscordInstance()->m_channelHistory.AddToHistory(m_channelID);
	
	m_channelID = sf;
	pGuild->m_currentChannel = sf;

	GetFrontend()->UpdateSelectedChannel(m_viewID);

	if (!GetCurrentChannel() || !GetCurrentGuild())
		return;

	// send an update subscriptions message
	if (bSendSubscriptionUpdate)
		GetDiscordInstance()->UpdateSubscriptions();
}

void ChatView::OnSelectGuild(Snowflake sf, Snowflake chan)
{
	if (m_guildID == sf)
	{
		if (chan)
			OnSelectChannel(chan);

		return;
	}

	GetDiscordInstance()->m_channelHistory.AddToHistory(m_channelID);

	// select the guild
	m_guildID = sf;

	// check if there are any channels and select the first (for now)
	Guild* pGuild = GetDiscordInstance()->GetGuild(sf);
	if (!pGuild)
		return;

	GetFrontend()->UpdateSelectedGuild(m_viewID);

	if (pGuild->m_bChannelsLoaded && pGuild->m_channels.size())
	{
		// Determine the first channel we should load.
		// TODO: Note, unfortunately this isn't really the right order, but it works for now.
		if (!chan && pGuild->m_currentChannel == 0)
		{
			for (auto& ch : pGuild->m_channels)
			{
				if (ch.HasPermission(PERM_VIEW_CHANNEL) && ch.m_channelType != Channel::CATEGORY) {
					pGuild->m_currentChannel = ch.m_snowflake;
					break;
				}
			}
		}

		if (!chan)
			chan = pGuild->m_currentChannel;

		OnSelectChannel(chan, false);
	}
	else
	{
		OnSelectChannel(0);
	}

	GetDiscordInstance()->UpdateSubscriptions();
}
