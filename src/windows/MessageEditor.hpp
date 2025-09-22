#pragma once

#include <ctime>
#include <string>
#include <vector>
#include "Main.hpp"
#include "AutoComplete.hpp"

class MessageList;

class MessageEditor
{
public:
	HWND m_hwnd      = NULL;
	HWND m_edit_hwnd = NULL;
	ChatWindow* m_pParent = nullptr;

private:
	HWND m_parent_hwnd         = NULL;
	HWND m_send_hwnd           = NULL;
	HWND m_btnUpload_hwnd      = NULL;
	HWND m_mentionText_hwnd    = NULL;
	HWND m_mentionName_hwnd    = NULL;
	HWND m_mentionCheck_hwnd   = NULL;
	HWND m_mentionCancel_hwnd  = NULL;
	HWND m_mentionJump_hwnd    = NULL;
	HWND m_editingMessage_hwnd = NULL;
	HWND m_jumpPresent_hwnd    = NULL;
	HWND m_jumpPresLbl_hwnd    = NULL;
	int m_expandedBy = 0;
	int m_lineHeight = 0;
	int m_initialHeight = 0;
	int m_mentionTextWidth = 0;
	int m_replyToTextWidth = 0;
	int m_cancelTextWidth = 0;
	int m_jumpTextWidth = 0;
	int m_uploadTextWidth = 0;
	int m_mentionAreaHeight = 0;
	int m_textLength = 0;
	int m_jumpPresentWidth = 0;
	int m_jumpPresentHeight = 0;
	bool m_bReplying = false;
	bool m_bEditing = false;
	bool m_bBrowsingPast = false;
	std::string m_replyName; // Or edited message contents
	Snowflake m_replyMessage = 0; // Or edited message ID
	COLORREF m_userNameColor = CLR_NONE;
	bool m_bWasUploadingAllowed = false;

	Snowflake m_guildID = 0;
	Snowflake m_channelID = 0;

	AutoComplete m_autoComplete;
	bool m_bDidMemberLookUpRecently = false;
	Snowflake m_previousQueriesActiveOnGuild = 0;
	uint64_t m_lastRemoteQuery = 0;
	std::set<std::string> m_previousQueries;
	MessageList* m_pMessageList;

	static WNDPROC m_editWndProc;
	static bool m_shiftHeld;
	
public:
	MessageEditor();
	~MessageEditor();

	void UpdateTextBox();
	void StartReply(Snowflake messageID, Snowflake authorID);
	void StopReply();
	void StartEdit(Snowflake messageID);
	void StopEdit();
	void StartBrowsingPast();
	void StopBrowsingPast();
	void Layout();
	void OnLoadedMemberChunk();
	void SetMessageList(MessageList* messageList);

	Snowflake ReplyingTo() const {
		return m_replyMessage;
	}
	int ExpandedBy() const {
		return m_expandedBy;
	}
	void SetJumpPresentHeight(int x) {
		m_jumpPresentHeight = x;
	}

	Snowflake GetCurrentGuildID() const { return m_guildID; }
	Snowflake GetCurrentChannelID() const { return m_channelID; }

	void SetGuildID(Snowflake sf) { m_guildID = sf; }
	void SetChannelID(Snowflake sf) { m_channelID = sf; }

	Guild* GetCurrentGuild();
	Channel* GetCurrentChannel();

private:
	void TryToSendMessage();
	void Expand(int Amount); // Positive amount means up, negative means down.
	bool MentionRepliedUser();
	void ShowOrHideReply(bool shown);
	void ShowOrHideEdit(bool shown);
	void ShowOrHideJumpPresent(bool shown);
	void UpdateCommonButtonsShown();
	bool IsUploadingAllowed();
	void OnUpdateText();
	void AutoCompleteLookup(const std::string& keyWord, char query, std::vector<AutoCompleteMatch>& matches);
	static void _AutoCompleteLookup(void* context, const std::string& keyWord, char query, std::vector<AutoCompleteMatch>& matches);

public:
	static LRESULT CALLBACK EditWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static void InitializeClass();

	static MessageEditor* Create(ChatWindow* parent, LPRECT pRect);
};

