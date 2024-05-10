#pragma once

#include <string>
#include <set>

enum eMessageStyle
{
	MS_3DFACE,
	MS_FLAT,
	MS_GRADIENT,
	MS_FLATBR,
};

class LocalSettings
{
public:
	LocalSettings();

	bool Load();
	bool Save();
	std::string GetToken() const {
		return m_token;
	}
	void SetToken(const std::string& str) {
		m_token = str;
	}
	eMessageStyle GetMessageStyle() const {
		return m_messageStyle;
	}
	void SetMessageStyle(eMessageStyle ms) {
		m_messageStyle = ms;
	}
	bool CheckTrustedDomain(const std::string& url);
	
	bool ReplyMentionByDefault() const {
		return m_bReplyMentionDefault;
	}
	void SetReplyMentionByDefault(bool b) {
		m_bReplyMentionDefault = b;
	}
	void SetWindowSize(int w, int h) {
		m_width = w; m_height = h;
	}
	void GetWindowSize(int& w, int& h) {
		w = m_width; h = m_height;
	}
	void SetSaveWindowSize(bool b) {
		m_bSaveWindowSize = b;
	}
	bool GetSaveWindowSize() const {
		return m_bSaveWindowSize;
	}
	void SetStartMaximized(bool b) {
		m_bStartMaximized = b;
	}
	bool GetStartMaximized() const {
		return m_bStartMaximized;
	}
	bool IsFirstStart() const {
		return m_bIsFirstStart;
	}
	const std::string& GetDiscordAPI() const {
		return m_discordApi;
	}
	void SetDiscordAPI(const std::string& str) {
		m_discordApi = str;
	}
	const std::string& GetDiscordCDN() const {
		return m_discordCdn;
	}
	void SetDiscordCDN(const std::string& str) {
		m_discordCdn = str;
	}

private:
	std::string m_token;
	std::string m_discordApi;
	std::string m_discordCdn;
	std::set<std::string> m_trustedDomains;
	eMessageStyle m_messageStyle = MS_GRADIENT;
	bool m_bReplyMentionDefault = true;
	bool m_bSaveWindowSize = false;
	bool m_bStartMaximized = false;
	bool m_bIsFirstStart = false;
	int m_width = 1000;
	int m_height = 700;
};

LocalSettings* GetLocalSettings();
