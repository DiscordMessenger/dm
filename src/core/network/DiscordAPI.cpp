#include "DiscordAPI.hpp"
#include "../config/LocalSettings.hpp"

const std::string& GetDiscordAPI()
{
    return GetLocalSettings()->GetDiscordAPI();
}

const std::string& GetDiscordCDN()
{
    return GetLocalSettings()->GetDiscordCDN();
}
