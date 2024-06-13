#pragma once

#include <nlohmann/json.h>
#include <string>

class DiscordClientConfig
{
public:
	DiscordClientConfig();

	const std::string& GetUserAgent() const;
	const std::string& GetSecChUa() const;
	const std::string& GetLocale() const;
	const std::string& GetTimezone() const;
	const std::string& GetOS() const;

	nlohmann::json Serialize() const;
	const std::string& GetSerializedJsonBlob() const;
	const std::string& GetSerializedBase64Blob() const;

private:
	std::string m_os;
	std::string m_browser;
	std::string m_device;
	std::string m_systemLocale;
	std::string m_browserUserAgent;
	std::string m_browserVersion;
	std::string m_osVersion;
	std::string m_referrer;
	std::string m_referringDomain;
	std::string m_referrerCurrent;
	std::string m_referringDomainCurrent;
	std::string m_releaseChannel;
	int m_clientBuildNumber;
	int m_designId;

	// not part of the actual header itself, just the user agent
	std::string m_webKitVersion;


	std::string m_browserVersionSimple;
	std::string m_secChUa;
	std::string m_timeZone;
	std::string m_serializedJsonBlob;
	std::string m_serializedBase64Blob;
};

DiscordClientConfig* GetClientConfig();
