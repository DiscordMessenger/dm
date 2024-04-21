#pragma once

#include <ctime>
#include <string>
#include <vector>
#include "Main.hpp"

struct TypingUser
{
	Snowflake m_key;
	std::string m_name;
	uint64_t m_startTimeMS;
};

class MessageEditor
{
public:
	HWND m_hwnd = NULL;
	HWND m_edit_hwnd = NULL;
	HWND m_send_hwnd = NULL;
	HWND m_btnUpload_hwnd = NULL;
	HWND m_mentionText_hwnd   = NULL;
	HWND m_mentionName_hwnd   = NULL;
	HWND m_mentionCheck_hwnd  = NULL;
	HWND m_mentionCancel_hwnd = NULL;
	HWND m_mentionJump_hwnd   = NULL;
	HWND m_editingMessage_hwnd = NULL;
	RECT m_typing_status_rect;
	RECT m_typing_animation_rect;
	std::vector<TypingUser> m_typingUsers;
	UINT_PTR m_timerExpireEvent;
	UINT_PTR m_timerAnimEvent;
	int m_anim_frame_number = 0;
	int m_expandedBy = 0;
	int m_lineHeight = 0;
	int m_initialHeight = 0;
	int m_mentionTextWidth = 0;
	int m_replyToTextWidth = 0;
	int m_mentionAreaHeight = 0;
	bool m_bReplying = false;
	bool m_bEditing = false;
	std::string m_replyName; // Or edited message contents
	Snowflake m_replyMessage = 0; // Or edited message ID
	COLORREF m_userNameColor = CLR_NONE;
	bool m_bWasUploadingAllowed = false;

	static WNDPROC m_editWndProc;
	static bool m_shiftHeld;
	
public:
	MessageEditor();
	~MessageEditor();

	void UpdateTextBox();
	void AddTypingName(Snowflake sf, time_t startTime, const std::string& name);
	void RemoveTypingName(Snowflake sf);
	void ClearTypers();
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
	void RemoveTypingNameInternal(Snowflake sf);
	void SetExpiryTimerIn(int ms);
	void StartAnimation();
	void StopAnimation();
	int GetNextExpiryTime();
	void OnExpiryTick();
	void OnAnimationTick();
	void TryToSendMessage();
	void Expand(int Amount); // Positive amount means up, negative means down.
	bool MentionRepliedUser();
	void ShowOrHideReply(bool shown);
	void ShowOrHideEdit(bool shown);
	void UpdateCommonButtonsShown();
	bool IsUploadingAllowed();

public:
	static WNDCLASS g_ProfileViewClass;

	static LRESULT CALLBACK EditWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static void InitializeClass();

	static MessageEditor* Create(HWND hwnd, LPRECT pRect);
};

