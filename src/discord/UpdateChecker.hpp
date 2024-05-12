#pragma once

#include "Frontend.hpp"
#include "HTTPClient.hpp"

class UpdateChecker
{
public:
	static std::string GetUpdateAPIURL();
	static void StartCheckingForUpdates();
	static void DownloadUpdate(const std::string& url);

private:
	static void OnRequestDone(NetRequest*);
};
