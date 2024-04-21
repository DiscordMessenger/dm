#pragma once
#define WIN32_LEAN_AND_MEAN
#include <string>
#include <Windows.h>
#include "../discord/Snowflake.hpp"

// GET Request: discordapi/channels/$CHANNEL_ID/pins
// RESPONSE: Array of message objects

void PmvInitialize(HWND hWnd);

void PmvOnLoadedPins(Snowflake channelID, const std::string& data);

void PmvCreate(Snowflake channelID, Snowflake guildID, int x, int y, bool rightJustify = false);

bool PmvIsActive();
