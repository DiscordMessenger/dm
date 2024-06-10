#include "NotificationManager.hpp"
#include "DiscordInstance.hpp"

NotificationManager::NotificationManager(DiscordInstance* pDiscord):
	m_pDiscord(pDiscord)
{
}

void NotificationManager::OnMessageCreate(const nlohmann::json& data)
{
	const auto& author = data["author"];

	Snowflake guildId = GetSnowflake(data, "guild_id");
	Snowflake channelId = GetSnowflake(data, "channel_id");
	Snowflake messageId = GetSnowflake(data, "id");

}
