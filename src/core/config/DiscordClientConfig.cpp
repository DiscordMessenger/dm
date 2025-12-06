#include "DiscordClientConfig.hpp"
#include <boost/base64/base64.hpp>

// TODO: Fetch some of these properties from the system (e.g. system locale)

// This configuration simulates a Microsoft Edge web client on Microsoft Windows 10.

DiscordClientConfig* GetClientConfig()
{
	static DiscordClientConfig s_config;
	return &s_config;
}

DiscordClientConfig::DiscordClientConfig()
{
	// Setup here -- These are dumped from my discord canary client
	m_osArch = m_appArch = "x64"; // not actually
	m_os = "Windows";
	m_browser = "Discord Client";
	m_systemLocale = "en-US";
	m_timeZone = "Europe/Bucharest"; // TODO: UTC here
	m_browserVersion = "37.6.0";
	m_browserVersionSimple = "125";
	m_webKitVersion = "537.36";
	m_osVersion = "10.0.22621";
	m_osSdkVersion = "22621";
	m_releaseChannel = "canary";
	m_clientBuildNumber = 476782;
	m_nativeBuildNumber = 72830;
	m_clientVersion = "1.0.777";
	m_osVersionSimple = "10";
	m_chromeVersion = "138.0.7204.251";

	m_browserUserAgent =
		"Mozilla/5.0 (Windows NT "
		+ m_osVersionSimple
		+ ".0; Win64; x64) AppleWebKit/"
		+ m_webKitVersion
		+ " (KHTML, like Gecko) discord/"
		+ m_clientVersion
		+ " Chrome/"
		+ m_chromeVersion
		+ " Electron/"
		+ m_browserVersion
		+ " Safari/"
		+ m_webKitVersion;

	m_secChUa = "\"Not?A_Brand\";v=\"99\", \"Chromium\";v=\"130\"";

	// Ok, now serialize it
	nlohmann::json j = Serialize();

	std::string str = j.dump();
	m_serializedJsonBlob = str;

	char* buffer = new char[base64::encoded_size(str.size()) + 1];
	size_t sz = base64::encode(buffer, str.data(), str.size());
	std::string dataToSend(buffer, sz);
	delete[] buffer;

	m_serializedBase64Blob = dataToSend;
}

nlohmann::json DiscordClientConfig::Serialize() const
{
	nlohmann::json j;
	j["app_arch"] = m_appArch;
	j["browser"] = m_browser;
	j["browser_user_agent"] = m_browserUserAgent;
	j["browser_version"] = m_browserVersion;
	j["client_build_number"] = m_clientBuildNumber;
	j["client_event_source"] = nullptr;
	j["client_version"] = m_clientVersion;
	j["has_client_mods"] = false; // this whole thing is a mod isn't it?
	j["native_build_number"] = m_nativeBuildNumber;
	j["os"] = m_os;
	j["os_arch"] = m_osArch;
	j["os_sdk_version"] = m_osSdkVersion;
	j["os_version"] = m_osVersion;
	j["release_channel"] = m_releaseChannel;
	j["system_locale"] = m_systemLocale;
	return j;
}

const std::string& DiscordClientConfig::GetSerializedJsonBlob() const
{
	return m_serializedJsonBlob;
}

const std::string& DiscordClientConfig::GetSerializedBase64Blob() const
{
	return m_serializedBase64Blob;
}

const std::string& DiscordClientConfig::GetUserAgent() const
{
	return m_browserUserAgent;
}

const std::string& DiscordClientConfig::GetSecChUa() const
{
	return m_secChUa;
}

const std::string& DiscordClientConfig::GetLocale() const
{
	return m_systemLocale;
}

const std::string& DiscordClientConfig::GetTimezone() const
{
	return m_timeZone;
}

const std::string& DiscordClientConfig::GetOS() const
{
	return m_os;
}
