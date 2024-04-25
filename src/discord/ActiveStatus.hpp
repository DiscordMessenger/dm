#pragma once
#include <string>

enum eActiveStatus
{
	STATUS_ONLINE,
	STATUS_IDLE,
	STATUS_DND,
	STATUS_OFFLINE,
	STATUS_MAX, // MUST be the last element!
};

static eActiveStatus GetStatusFromString(const std::string& str) {
	eActiveStatus st = STATUS_ONLINE;
	if (str == "idle")      st = STATUS_IDLE;
	if (str == "dnd")       st = STATUS_DND;
	if (str == "offline")   st = STATUS_OFFLINE;
	if (str == "invisible") st = STATUS_OFFLINE;
	return st;
}
