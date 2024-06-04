#pragma once

#include <ctime>
#include <string>
#include <vector>
#include "Main.hpp"
#include "AutoComplete.hpp"

class MessageEditor
{
public:
	HWND m_hwnd = NULL;
	HWND m_edit_hwnd = NULL;

private:
	HWND m_parent_hwnd = NULL;
	HWND m_send_hwnd = NULL;
	HWND m_btnUpload_hwnd = NULL;
	HWND m_mentionText_hwnd   = NULL;
	HWND m_mentionName_hwnd   = NULL;
	HWND m_mentionCheck_hwnd  = NULL;
	HWND m_mentionCancel_hwnd = NULL;
	HWND m_mentionJump_hwnd   = NULL;
	HWND m_editingMessage_hwnd = NULL;
	HWND m_autoComplete_hwnd     = NULL;
	HWND m_autoCompleteList_hwnd = NULL;
	int m_expandedBy = 0;
	int m_lineHeight = 0;
	int m_initialHeight = 0;
	int m_mentionTextWidth = 0;
	int m_replyToTextWidth = 0;
	int m_mentionAreaHeight = 0;
	int m_textLength = 0;
	bool m_bReplying = false;
	bool m_bEditing = false;
	std::vector<std::string> m_autocompleteResults;
	std::string m_replyName; // Or edited message contents
	Snowflake m_replyMessage = 0; // Or edited message ID
	COLORREF m_userNameColor = CLR_NONE;
	bool m_bWasUploadingAllowed = false;
	AutoComplete m_autoComplete;

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
	void Layout();

	Snowflake ReplyingTo() const {
		return m_replyMessage;
	}
	int ExpandedBy() const {
		return m_expandedBy;
	}

private:
	void TryToSendMessage();
	void Expand(int Amount); // Positive amount means up, negative means down.
	bool MentionRepliedUser();
	void ShowOrHideReply(bool shown);
	void ShowOrHideEdit(bool shown);
	void UpdateCommonButtonsShown();
	bool IsUploadingAllowed();
	void OnUpdateText();
	static void AutoCompleteLookup(const std::string& keyWord, char query, std::vector<AutoCompleteMatch>& matches);

public:
	static LRESULT CALLBACK EditWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static void InitializeClass();

	static MessageEditor* Create(HWND hwnd, LPRECT pRect);
};

