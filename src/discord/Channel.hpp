#pragma once

#include <string>
#include <vector>
#include "Snowflake.hpp"
#include "Permissions.hpp"

// Read state object "flags" member.  Only seems to have this.
#define RSTATE_FLAG_HAS_LASTVIEWED (1 << 0)

struct Channel
{
	enum eChannelType
	{
		TEXT,
		DM,
		VOICE,
		GROUPDM,
		CATEGORY,
		NEWS,
		STORE,
		NEWSTHREAD = 10,
		PUBTHREAD,
		PRIVTHREAD,
		STAGEVOICE,
		DIRECTORY,
		FORUM,
		MEDIA,
	};

	eChannelType m_channelType = TEXT;
	Snowflake m_snowflake = 0;
	Snowflake m_lastSentMsg = 0; // The message that was sent last.
	Snowflake m_lastViewedMsg = 0; // The last message that was read in this channel.
	Snowflake m_parentCateg = 0;
	Snowflake m_parentGuild = 0;
	Snowflake m_recipient = 0; // valid only for DM channels
	std::string m_name = "";
	std::string m_topic = "";
	std::string m_avatarLnk = ""; // valid only for DM channels
	int m_pos = 0;
	int m_recipientCount = 0; // valid only for GROUPDM channels
	std::map<Snowflake, Overwrite> m_overwrites;
	uint64_t m_currentUserPerms = 0;
	bool m_bCurrentUserPermsCalculated = false;
	int m_mentionCount = 0;

	// The "last_viewed" field in the read state object.  Appears to be a sequence number.
	// Passed to POST(*/messages/$LAST_MESSAGE_HERE/ack).  -1 if doesn't exist
	int m_lastViewedNum = 0;

	bool operator<(const Channel& b) const
	{
		// TODO: this is a mess, fix it.
		bool isDm = m_channelType == DM || m_channelType == GROUPDM;
		bool isOthDm = b.m_channelType == DM || b.m_channelType == GROUPDM;

		if (IsDM() && !b.IsDM()) return true;
		if (!IsDM() && b.IsDM()) return false;

		if (m_channelType == CATEGORY && b.m_channelType != CATEGORY) return true;
		if (m_channelType != CATEGORY && b.m_channelType == CATEGORY) return false;

		if (m_channelType == CATEGORY) {
			if (m_pos != b.m_pos)                 return m_pos < b.m_pos;
			if (m_parentCateg != b.m_parentCateg) return m_parentCateg < b.m_parentCateg;
		}
		else if (IsDM()) {
			if (m_lastSentMsg != b.m_lastSentMsg) return m_lastSentMsg > b.m_lastSentMsg;
			if (m_pos != b.m_pos)                 return m_pos < b.m_pos;
			if (m_parentCateg != b.m_parentCateg) return m_parentCateg < b.m_parentCateg;
		}
		else {
			if (m_parentCateg != b.m_parentCateg) return m_parentCateg < b.m_parentCateg;
			if (m_pos != b.m_pos)                 return m_pos < b.m_pos;
		}

		if (m_snowflake != b.m_snowflake)     return m_snowflake > b.m_snowflake;
		return false;
	}

	Channel() {}
	Channel(uint64_t sf, const std::string& st, eChannelType ct) :m_channelType(ct), m_snowflake(sf), m_name(st) {}

	uint64_t ComputePermissionOverwrites(Snowflake Member, uint64_t BasePermissions) const;

	// Check if the current user has permission to do something.
	bool HasPermission(uint64_t Permission);
	bool HasPermissionConst(uint64_t Permission) const;

	bool WasMentioned() const {
		return m_mentionCount > 0;
	}

	bool HasUnreadMessages() const {
		return m_lastViewedMsg != 0 && m_lastViewedMsg < m_lastSentMsg;
	}

	bool IsCategory() const {
		return m_channelType == CATEGORY;
	}

	bool IsDM() const {
		auto t = m_channelType;
		return t == DM || t == GROUPDM;
	}

	bool IsVoice() const {
		auto t = m_channelType;
		return t == VOICE || t == STAGEVOICE;
	}

	bool IsText() const {
		return !IsVoice() && !IsDM() && !IsCategory();
	}

	std::string GetTypeSymbol() const {
		switch (m_channelType) {
			case DM:    return "@";
			case VOICE:
			case NEWS:
			case TEXT:  return "#";
			default:    return "";
		}
	}
};
