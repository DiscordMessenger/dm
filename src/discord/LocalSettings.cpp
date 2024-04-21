#include <fstream>
#include <nlohmann/json.h>
#include "LocalSettings.hpp"
#include "Util.hpp"
using nlohmann::json;

static LocalSettings* g_pInstance;

LocalSettings* GetLocalSettings()
{
	if (!g_pInstance)
		g_pInstance = new LocalSettings;

	return g_pInstance;
}

LocalSettings::LocalSettings()
{
	// Add some default trusted domains:
	m_trustedDomains.insert("discord.gg");
	m_trustedDomains.insert("discord.com");
	m_trustedDomains.insert("discordapp.com");
	m_trustedDomains.insert("canary.discord.com");
	m_trustedDomains.insert("canary.discordapp.com");
	m_trustedDomains.insert("cdn.discord.com");
	m_trustedDomains.insert("cdn.discordapp.com");
	m_trustedDomains.insert("media.discordapp.net");
}

bool LocalSettings::Load()
{
	std::string fileName = GetBasePath() + "/settings.json";
	std::string data = LoadEntireTextFile(fileName);

	if (data.empty())
		return false;

	m_messageStyle = MS_GRADIENT;

	json j = json::parse(data);

	// Load properties from the json object.
	if (j.contains("Token"))
		m_token = j["Token"];

	if (j.contains("MessageStyle"))
		m_messageStyle = eMessageStyle(int(j["MessageStyle"]));

	if (j.contains("TrustedDomains")) {
		for (auto& dom : j["TrustedDomains"])
			m_trustedDomains.insert(dom);
	}

	if (j.contains("ReplyMentionDefault"))
		m_bReplyMentionDefault = j["ReplyMentionDefault"];

	// TODO: what else
	return true;
}

bool LocalSettings::Save()
{
	json j;
	json trustedDomains;

	for (auto& dom : m_trustedDomains)
		trustedDomains.push_back(dom);

	j["Token"] = m_token;
	j["MessageStyle"] = int(m_messageStyle);
	j["TrustedDomains"] = trustedDomains;

	// save the file
	std::string fileName = GetBasePath() + "/settings.json";
	std::ofstream of(fileName.c_str(), std::ios::trunc);
	
	if (!of.is_open())
		return false;

	of << j.dump();
	of.close();

	return true;
}

bool LocalSettings::CheckTrustedDomain(const std::string& url)
{
	// check if the domain belongs to the trusted list
	std::string domain, resource;
	SplitURL(url, domain, resource);
	return m_trustedDomains.find(domain) != m_trustedDomains.end();
}
