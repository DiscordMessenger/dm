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

private:
	std::string m_token;
	std::set<std::string> m_trustedDomains;
	eMessageStyle m_messageStyle = MS_3DFACE;
	bool m_bReplyMentionDefault = true;
};

LocalSettings* GetLocalSettings();
