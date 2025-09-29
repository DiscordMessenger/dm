#pragma once

#include "../network/HTTPClient.hpp"

class UpdateChecker
{
public:
	static std::string GetUpdateAPIURL();
	static void StartCheckingForUpdates();

private:
	static void OnRequestDone(NetRequest*);
};
