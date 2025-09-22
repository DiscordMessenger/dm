#pragma once

#include "Main.hpp"
#include "IChannelView.hpp"

#define C_MAX_CHANNEL_MEMBER_LEN (64)

#define T_CHANNEL_VIEW_CONTAINER_CLASS2 TEXT("ChannelViewContainerOLD")

class ChannelViewOld : public IChannelView
{
private:
	struct ChannelMember {
		Snowflake m_category;
		Snowflake m_id;
		Channel::eChannelType m_type;
		std::string m_name;
		int m_categIndex;
		int m_pos;
		Snowflake m_lastMessageID;

		bool IsDM() const
		{
			return m_type == Channel::DM || m_type == Channel::GROUPDM;
		}

		bool IsCategory() const
		{
			return m_type == Channel::CATEGORY;
		}

		bool IsVoice() const
		{
			return m_type == Channel::VOICE || m_type == Channel::STAGEVOICE;
		}

		bool operator<(const ChannelMember& other) const
		{
			if (IsDM()) {
				// the other is probably a DM too
				if (m_lastMessageID != other.m_lastMessageID)
					return m_lastMessageID > other.m_lastMessageID;
			}

			// sort by which category we're in
			if (m_categIndex != other.m_categIndex)
				return m_categIndex < other.m_categIndex;
			
			if (m_category != other.m_category)
				return m_category < other.m_category;

			// within this category group, put the category first
			if (IsCategory() && !other.IsCategory()) return true;
			if (!IsCategory() && other.IsCategory()) return false;

			// voice channels are always below text channels
			if (IsVoice() && !other.IsVoice()) return false;
			if (!IsVoice() && other.IsVoice()) return true;

			// within each category, sort by position
			if (!IsCategory() && !other.IsCategory() && m_pos != other.m_pos)
				return m_pos < other.m_pos;

			return m_id < other.m_id;
		}
	};

public:
	static ChannelViewOld* Create(HWND hwnd, LPRECT rect);
	static void InitializeClass();

public:
	void ClearChannels() override;
	void OnUpdateAvatar(Snowflake sf) override;
	void OnUpdateSelectedChannel(Snowflake sf) override;
	void UpdateAcknowledgement(Snowflake sf) override;
	void SetMode(bool listMode) override;
	void AddChannel(const Channel& channel) override;
	void RemoveCategoryIfNeeded(const Channel& channel) override;
	void CommitChannels() override;
	HWND GetListHWND() override { return m_listHwnd; }
	HWND GetTreeHWND() override { return m_listHwnd; }

private:
	HWND m_hwndParent = NULL;
	HWND m_listHwnd = NULL;
	Snowflake m_currentChannel = 0;
	std::vector<ChannelMember> m_items;
	std::map<Snowflake, int> m_idToIdx;
	int m_nextCategIndex = 0;

private:
	static WNDCLASS g_ChannelViewLegacyClass;
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};
