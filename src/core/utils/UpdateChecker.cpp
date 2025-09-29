#include "UpdateChecker.hpp"
#include "../../core/Frontend.hpp"
#include "Util.hpp"
#include <nlohmann/json.h>

std::string UpdateChecker::GetUpdateAPIURL()
{
	return "https://api.github.com/repos/DiscordMessenger/dm/releases/latest";
}

void UpdateChecker::StartCheckingForUpdates()
{
	GetHTTPClient()->PerformRequest(true, NetRequest::GET, GetUpdateAPIURL(), 0, 0, "", "", "", OnRequestDone);
}

#ifdef _MSC_VER
#define IS_MINGW false
#else
#define IS_MINGW true
#endif

void UpdateChecker::OnRequestDone(NetRequest* pReq)
{
	if (pReq->result != HTTP_OK) {
		GetFrontend()->OnFailedToCheckForUpdates(pReq->result, pReq->response);
		return;
	}

	nlohmann::json j = nlohmann::json::parse(pReq->response);

	// This is specific to the GitHub API.  If moving away from GitHub, redo this:
	std::string tagName = j["tag_name"];
	std::string downloadUrl = "";

	const bool isMinGW = IS_MINGW;

	//
	// GUIDE TO MAKING A RELEASE: (if iProgramInCpp ever forgets how to, or you wanna make a fork)
	//
	// 1. Give it a tag that is the exact version of the app. ("v1.01" for V1.01, for instance)
	// 2. Upload two files: DiscordMessenger-V$.$$-MinGW.zip and DiscordMessenger-V$.$$-MSVC.zip
	//    (note, they are case sensitive!)
	// 3. Publish the release
	// 4. Enjoy!
	//

	for (auto& asset : j["assets"])
	{
		std::string url = asset["browser_download_url"];

		bool isTheOne;
		if (isMinGW)
			isTheOne = strstr(url.c_str(), "MinGW") != nullptr;
		else
			isTheOne = strstr(url.c_str(), "MSVC") != nullptr;
		
		if (isTheOne) {
			downloadUrl = url;
			break;
		}
	}

	if (tagName.empty()) {
		GetFrontend()->OnFailedToCheckForUpdates(-2, "Latest release's tag name is empty.");
		return;
	}

	if (downloadUrl.empty()) {
		GetFrontend()->OnFailedToCheckForUpdates(-2, std::string("Latest release doesn't have a valid ") + (isMinGW ? "MinGW" : "MSVC") + " download.");
		return;
	}

	if (tagName[0] != 'v') {
		GetFrontend()->OnFailedToCheckForUpdates(-3, "Latest release's tag name is invalid.");
		return;
	}

	tagName[0] = 'V';

	// Ok, now parse it as a float
	float f = std::strtof(tagName.c_str() + 1, NULL);

	if (f > GetAppVersion()) {
		// Ta-da! We have found an update.
		GetFrontend()->OnUpdateAvailable(downloadUrl, tagName);
	}
}
