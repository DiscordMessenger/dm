#pragma once
#include <string>

enum eActiveStatus
{
	STATUS_OFFLINE,
	STATUS_ONLINE,
	STATUS_IDLE,
	STATUS_DND,
	STATUS_MAX, // MUST be the last element!
};

static eActiveStatus GetStatusFromString(const std::string& str) {
	eActiveStatus st = STATUS_OFFLINE;
	if (str == "idle")   st = STATUS_IDLE;
	if (str == "dnd")    st = STATUS_DND;
	if (str == "online") st = STATUS_ONLINE;
	return st;
}
