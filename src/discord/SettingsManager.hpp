#pragma once

#include <vector>
#include <protobuf/Protobuf.hpp>
#include "Snowflake.hpp"
#include "ActiveStatus.hpp"

// thanks https://github.com/discord-net/Discord.Net/blob/dev/src/Discord.Net.Core/Entities/Activities/ActivityType.cs
enum eActivityType
{
	ACTIVITY_PLAYING,
	ACTIVITY_STREAMING,
	ACTIVITY_LISTENING,
	ACTIVITY_WATCHING,
	ACTIVITY_CUSTOM_STATUS,
	ACTIVITY_COMPETING,
};

enum eExplicitFilter
{
	FILTER_NONE = 0,
	FILTER_EXCEPTFRIENDS,
	FILTER_TOTAL,
};

// Thanks a lot to https://github.com/dolfies/discord-protos/blob/master/discord_protos/PreloadedUserSettings.proto
namespace Settings {
	enum {
		FIELD_TEXT_AND_IMAGES = 6,
		FIELD_PRIVACY = 8,
		FIELD_ACTIVITY = 11,
		FIELD_GUILD_FOLDERS = 14,
	};

	namespace TextAndImages
	{
		enum {
			FIELD_DISPLAY_COMPACT = 17,
			FIELD_EXPLICIT_FILTER = 19,
		};

		namespace DisplayCompact
		{
			enum {
				FIELD_VALUE = 1,
			};
		}

		namespace ExplicitFilter
		{
			enum {
				FIELD_VALUE = 1,
			};
		}
	}

	namespace Privacy
	{
		enum {
			FIELD_DM_BLOCK_GUILDS = 3,
			FIELD_DM_BLOCK_DEFAULT = 4, // default when joining new guilds
		};
	}

	namespace Activity {
		enum {
			FIELD_INDICATOR = 1,
			FIELD_CUSTOM_STATUS = 2,
		};
		namespace Indicator {
			enum {
				FIELD_STATE = 1,
			};
		}
		namespace CustomStatus {
			enum {
				FIELD_TEXT = 1,
				FIELD_EMOJI = 3,
				FIELD_EXPIRY = 4,
			};
		}
	}

	namespace GuildFolders {
		enum {
			FIELD_ITEMS = 1,
		};
		namespace Item {
			enum {
				FIELD_GUILD_IDS = 1,
				FIELD_ID = 2,
				FIELD_NAME = 3,
				FIELD_COLOR = 4,
			};
		}
	}
}

class SettingsManager
{
public:
	SettingsManager();
	~SettingsManager();

	// This is a versatile method. It can not only load complete data,
	// but it can also load diffs, which will be applied on top of the data.
	void LoadData(const uint8_t* data, size_t sz);

	void LoadData(std::vector<uint8_t>& data) {
		LoadData(data.data(), data.size());
	}

	void LoadDataBase64(const std::string& str);

	void FlushSettings();

public: // SETTINGS
	void SetOnlineIndicator(eActiveStatus status);
	eActiveStatus GetOnlineIndicator();

	void SetCustomStatus(const std::string& text, const std::string& emoji, uint64_t timeExpiry);
	std::string GetCustomStatusText();
	std::string GetCustomStatusEmoji();
	uint64_t GetCustomStatusExpiry();

	void SetExplicitFilter(eExplicitFilter filter);
	eExplicitFilter GetExplicitFilter();

	void SetGuildDMBlocklist(const std::vector<Snowflake>& guilds);
	void GetGuildDMBlocklist(std::vector<Snowflake>& guilds);
	
	void SetDMBlockDefault(bool b);
	bool GetDMBlockDefault();

	void SetMessageCompact(bool b); // #IRC
	bool GetMessageCompact();

	std::vector<Snowflake> GetGuildFolders();
	void GetGuildFoldersEx(std::map<Snowflake, std::string>& folders, std::vector<std::pair<Snowflake, Snowflake>>& guilds);

private:
	Protobuf::DecodeHint* CreateHint();

	std::unique_ptr<Protobuf::ObjectBaseMessage> m_pSettingsMessage = nullptr;
};

SettingsManager* GetSettingsManager();

