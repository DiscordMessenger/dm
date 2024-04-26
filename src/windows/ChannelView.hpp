#pragma once

#include "Main.hpp"

#define C_MAX_CHANNEL_MEMBER_LEN (64)

class ChannelView
{
public:
	HWND m_hwnd;

private:
	struct ChannelMember
	{
		TCHAR str[C_MAX_CHANNEL_MEMBER_LEN];
		Snowflake m_snowflake;
		HTREEITEM m_hItem;
		int m_childCount;
	};

	int m_level = 1;

	std::map<Snowflake, int> m_idToIdx;
	std::vector<ChannelMember> m_channels;

	HWND m_hwndParent;

	int m_nCategoryIcon,
		m_nChannelIcon,
		m_nForumIcon,
		m_nVoiceIcon,
		m_nDmIcon,
		m_nGroupDmIcon,
		m_nChannelDotIcon,
		m_nChannelRedIcon;

	std::map<int, HTREEITEM> m_hPrev;

public:
	~ChannelView();

	void ClearChannels();
	void UpdateAcknowledgement(Snowflake sf);
	void AddChannel(const Channel& ch);
	void RemoveCategoryIfNeeded(const Channel& ch);
	void OnNotify(WPARAM wParam, LPARAM lParam);

private:
	int GetIcon(const Channel& ch);
	void ResetTree();
	void SetPrevious(int parentIndex, HTREEITEM hPrev);
	bool InitTreeViewImageLists();
	HTREEITEM GetPrevious(int parentIndex);
	HTREEITEM AddItemToTree(HWND hwndTV, LPTSTR lpszItem, HTREEITEM hParent, int nIndex, int nParentIndex, int nIcon);

public:
	static int g_ChannelViewId;

	static ChannelView* Create(HWND hwnd, LPRECT rect);
};

