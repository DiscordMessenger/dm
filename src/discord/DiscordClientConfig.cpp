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
	// Setup here
	m_os = "Windows";
	m_browser = "Chrome";
	m_systemLocale = "en-US";
	m_timeZone = "Europe/Bucharest"; // TODO: UTC here
	m_browserVersion = "124.0.0.0";
	m_browserVersionSimple = "124";
	m_webKitVersion = "537.36";
	m_osVersion = "10";
	m_releaseChannel = "stable";
	m_clientBuildNumber = 291963;
	m_device = "";
	m_referrer = "";
	m_referrerCurrent = "";
	m_referringDomain = "";
	m_referringDomainCurrent = "";
	m_designId = 0;

	m_browserUserAgent =
		"Mozilla/5.0 (Windows NT "
		+ m_osVersion
		+ ".0; Win64; x64) AppleWebKit/"
		+ m_webKitVersion
		+ " (KHTML, like Gecko) Chrome/"
		+ m_browserVersion
		+ " Safari/"
		+ m_webKitVersion
		+ " Edg/"
		+ m_browserVersion;

	m_secChUa = "\"Chromium\";v=\"" + m_browserVersionSimple + "\", \"Microsoft Edge\";v=\"124\", \"Not-A.Brand\";v=\"99\"";

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
	j["os"] = m_os;
	j["browser"] = m_browser;
	j["device"] = m_device;
	j["system_locale"] = m_systemLocale;
	j["browser_user_agent"] = m_browserUserAgent;
	j["browser_version"] = m_browserVersion;
	j["os_version"] = m_osVersion;
	j["referrer"] = m_referrer;
	j["referring_domain"] = m_referringDomain;
	j["referrer_current"] = m_referrerCurrent;
	j["referring_domain_current"] = m_referringDomainCurrent;
	j["release_channel"] = m_releaseChannel;
	j["client_build_number"] = m_clientBuildNumber;
	j["client_event_source"] = nullptr;
	j["design_id"] = m_designId;
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
