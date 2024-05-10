#pragma once

#include <windows.h>
#include <cstdint>

void UploadDialogShow2();
void UploadDialogShow();
void UploadDialogShowWithFileName(LPCTSTR fileName, LPCTSTR fileTitle);
void UploadDialogShowWithFileData(uint8_t* fileData, size_t fileSize, LPCTSTR fileTitle);
