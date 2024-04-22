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
	m_bIsFirstStart = false;

	std::string fileName = GetBasePath() + "/settings.json";
	std::string data = LoadEntireTextFile(fileName);

	if (data.empty()) {
		m_bIsFirstStart = true;
		return false;
	}

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

	if (j.contains("SaveWindowSize"))
		m_bSaveWindowSize = j["SaveWindowSize"];

	if (j.contains("StartMaximized"))
		m_bStartMaximized = j["StartMaximized"];

	if (m_bSaveWindowSize)
	{
		if (j.contains("WindowWidth"))
			m_width = j["WindowWidth"];

		if (j.contains("WindowHeight"))
			m_height = j["WindowHeight"];

		if (m_width < 900)
			m_width = 900;
		if (m_height < 600)
			m_height = 600;
	}
	
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
	j["ReplyMentionDefault"] = m_bReplyMentionDefault;
	j["StartMaximized"] = m_bStartMaximized;
	j["SaveWindowSize"] = m_bSaveWindowSize;
	if (m_bSaveWindowSize) {
		j["WindowWidth"] = m_width;
		j["WindowHeight"] = m_height;
	}

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
