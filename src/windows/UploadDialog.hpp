#pragma once

#include <windows.h>
#include <cstdint>
#include "../discord/Snowflake.hpp"

void UploadDialogShow2(Snowflake channelID);
void UploadDialogShow(Snowflake channelID);
void UploadDialogShowWithFileName(Snowflake channelID, LPCTSTR fileName, LPCTSTR fileTitle);
void UploadDialogShowWithFileData(Snowflake channelID, uint8_t* fileData, size_t fileSize, LPCTSTR fileTitle);
