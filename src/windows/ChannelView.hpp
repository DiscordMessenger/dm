#pragma once

#include "Main.hpp"

#define C_MAX_CHANNEL_MEMBER_LEN (64)

#define T_CHANNEL_VIEW_CONTAINER_CLASS TEXT("ChannelViewContainer")

class ChannelView
{
public:
	HWND m_hwnd = NULL;
	HWND m_treeHwnd = NULL;
	HWND m_listHwnd = NULL;

private:
	struct ChannelMember
	{
		TCHAR str[C_MAX_CHANNEL_MEMBER_LEN];
		Snowflake m_snowflake;
		HTREEITEM m_hItem;        // valid only in tree mode
		int m_childCount;
	};

	HWND m_hwndParent;
	std::vector<ChannelMember> m_channels;
	Snowflake m_currentChannel = 0;
	bool m_bListMode = false;

	bool TreeMode() const { return !m_bListMode; }
	bool ListMode() const { return m_bListMode; }

	// Tree specific
	int m_level = 1;
	std::map<Snowflake, int> m_idToIdx;
	int m_nCategoryExpandIcon,
		m_nCategoryCollapseIcon,
		m_nChannelIcon,
		m_nForumIcon,
		m_nVoiceIcon,
		m_nDmIcon,
		m_nGroupDmIcon,
		m_nChannelDotIcon,
		m_nChannelRedIcon;
	std::map<int, HTREEITEM> m_hPrev;

	// List specific
	WNDPROC m_origListWndProc = NULL;
	int m_hotItem = -1;

public:
	~ChannelView();

	void ClearChannels();
	void UpdateAcknowledgement(Snowflake sf);
	void AddChannel(const Channel& ch);
	void RemoveCategoryIfNeeded(const Channel& ch);
	bool OnNotifyTree(LRESULT& lres, WPARAM wParam, LPARAM lParam);
	bool OnNotifyList(LRESULT& lres, WPARAM wParam, LPARAM lParam);
	void SetItemIcon(HTREEITEM hItem, int icon);
	void OnUpdateSelectedChannel(Snowflake newCh);
	void SetMode(bool listMode);

private:
	int GetIcon(const Channel& ch, bool bIsExpanded);
	void ResetTree();
	void SetPrevious(int parentIndex, HTREEITEM hPrev);
	bool InitTreeView();
	bool InitListView();
	HTREEITEM GetPrevious(int parentIndex);
	HTREEITEM AddItemToTree(HWND hwndTV, LPTSTR lpszItem, HTREEITEM hParent, int nIndex, int nParentIndex, int nIcon);

public:
	static WNDCLASS g_ChannelViewClass;

	static ChannelView* Create(HWND hwnd, LPRECT rect);
	static void InitializeClass();
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK ListWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

