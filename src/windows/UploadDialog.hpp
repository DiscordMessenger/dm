#pragma once

#include <windows.h>
#include <cstdint>
#include "../discord/Snowflake.hpp"

void UploadDialogShow2(HWND hWnd, Snowflake channelID);
void UploadDialogShow(HWND hWnd, Snowflake channelID);
void UploadDialogShowWithFileName(HWND hWnd, Snowflake channelID, LPCTSTR fileName, LPCTSTR fileTitle);
void UploadDialogShowWithFileData(HWND hWnd, Snowflake channelID, uint8_t* fileData, size_t fileSize, LPCTSTR fileTitle);
