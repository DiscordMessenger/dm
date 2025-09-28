#pragma once

#include <iprogsjson.hpp>
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

	iprog::JsonObject Serialize() const;
	const std::string& GetSerializedJsonBlob() const;
	const std::string& GetSerializedBase64Blob() const;

private:
	std::string m_os;
	std::string m_browser;
	std::string m_systemLocale;
	std::string m_browserUserAgent;
	std::string m_browserVersion;
	std::string m_clientVersion;
	std::string m_osVersion;
	std::string m_osSdkVersion;
	std::string m_releaseChannel;
	std::string m_osArch;
	std::string m_appArch;
	int m_clientBuildNumber;
	int m_nativeBuildNumber;

	// not part of the actual header itself, just the user agent
	std::string m_webKitVersion;


	std::string m_browserVersionSimple;
	std::string m_osVersionSimple;
	std::string m_chromeVersion;
	std::string m_secChUa;
	std::string m_timeZone;
	std::string m_serializedJsonBlob;
	std::string m_serializedBase64Blob;
};

DiscordClientConfig* GetClientConfig();
