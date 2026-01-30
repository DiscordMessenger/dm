#pragma once

namespace DiscordRequest
{
	enum eDiscordRequest
	{
		NOTHING,
		PROFILE,
		GUILDS,
		MESSAGES,
		GUILD,
		IMAGE,
		GATEWAY, // on init, fetches the Websocket gateway address
		TYPING,
		IMAGE_ATTACHMENT,
		DELETE_MESSAGE,
		ACK,
		PINS,
		PIN_MESSAGE,
		UNPIN_MESSAGE,
		MESSAGE_CREATE,
		UPLOAD_ATTACHMENT,
		UPLOAD_ATTACHMENT_2,
		LEAVE_GUILD,
		ACK_BULK,
		USER_NOTE,
		SET_USER_NOTE,
	};
};
