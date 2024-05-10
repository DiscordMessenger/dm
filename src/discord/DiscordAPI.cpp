#include "DiscordAPI.hpp"
#include "LocalSettings.hpp"

const std::string& GetDiscordAPI()
{
    return GetLocalSettings()->GetDiscordAPI();
}

const std::string& GetDiscordCDN()
{
    return GetLocalSettings()->GetDiscordCDN();
}
