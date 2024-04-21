#pragma once
#include <string>

void CreateImageViewer(const std::string& proxyURL, const std::string& url, const std::string& fileName, int width, int height);
bool RegisterImageViewerClass();
void KillImageViewer();
