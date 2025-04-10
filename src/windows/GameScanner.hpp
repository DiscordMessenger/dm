#pragma once

#include <string>
#include <unordered_map>
#include "../discord/Snowflake.hpp"

struct GameItem
{
	Snowflake m_id;
};

class GameScanner
{
public:
	static bool HasDetectableCache();
	static void DownloadDetectables();
	static void LoadDetectableCache();

protected:
	friend class Frontend_Win32;
	static void DetectableDownloaded(const std::string& detectable);

private:
	static void LoadData(const std::string& data);

	static std::unordered_map<Snowflake, GameItem> m_games;
	static std::unordered_map<uint32_t, Snowflake> m_executableHashToGameId;
};
