#pragma once

#include "Main.hpp"
#include "IChannelView.hpp"

#define C_MAX_CHANNEL_MEMBER_LEN (64)

#define T_CHANNEL_VIEW_CONTAINER_CLASS TEXT("ChannelViewContainer")

class ChannelView : public IChannelView
{
public:
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
	~ChannelView() override;

	void ClearChannels() override;
	void UpdateAcknowledgement(Snowflake sf) override;
	void AddChannel(const Channel& ch) override;
	void RemoveCategoryIfNeeded(const Channel& ch) override;
	void OnUpdateSelectedChannel(Snowflake newCh) override;
	void SetMode(bool listMode) override;
	void OnUpdateAvatar(Snowflake sf) override;
	HWND GetListHWND() override { return m_listHwnd; }
	HWND GetTreeHWND() override { return m_treeHwnd; }
	void CommitChannels() override {}

public:
	void SetItemIcon(HTREEITEM hItem, int icon);
	bool OnNotifyTree(LRESULT& lres, WPARAM wParam, LPARAM lParam);
	bool OnNotifyList(LRESULT& lres, WPARAM wParam, LPARAM lParam);


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

