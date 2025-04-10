#include "GameScanner.hpp"
#include "Main.hpp"

using json = nlohmann::json;

std::unordered_map<Snowflake, GameItem> GameScanner::m_games;
std::unordered_map<uint32_t, Snowflake> GameScanner::m_executableHashToGameId;

bool GameScanner::HasDetectableCache()
{
	std::string filePath = GetBasePath() + "/detectable.json";
	LPTSTR tstr = ConvertCppStringToTString(filePath);

	DWORD attributes = GetFileAttributes(tstr);
	free(tstr);
	return attributes != INVALID_FILE_ATTRIBUTES && (~attributes & FILE_ATTRIBUTE_DIRECTORY);
}

void GameScanner::DownloadDetectables()
{
}

void GameScanner::LoadDetectableCache()
{
	std::string filePath = GetBasePath() + "/detectable.json";
	LPTSTR tstr = ConvertCppStringToTString(filePath);
	// open up the file and save it
	HANDLE hFile = CreateFile(tstr, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	free(tstr);

	if (hFile == INVALID_HANDLE_VALUE) {
		DbgPrintW("Error, cannot open %s, error %d", filePath.c_str(), GetLastError());
		return;
	}

	DWORD fileSizeHigh = 0;
	DWORD fileSize = GetFileSize(hFile, &fileSizeHigh);
	if (fileSize > 8000000 || fileSizeHigh) {
		DbgPrintW("Error, detectable is too big");
		CloseHandle(hFile);
		return;
	}

	std::string data;
	data.resize(fileSize);

	DWORD bytesRead = 0;
	BOOL result = ReadFile(hFile, (void*) data.data(), fileSize, &bytesRead, NULL);

	CloseHandle(hFile);

	if (!result) {
		DbgPrintW("Cannot read from %s, error %d", filePath.c_str(), GetLastError());
		return;
	}

	LoadData(data);
}

void GameScanner::DetectableDownloaded(const std::string& detectable)
{
	std::string filePath = GetBasePath() + "/detectable.json";

	LPTSTR tstr = ConvertCppStringToTString(filePath);
	// open up the file and save it
	HANDLE hFile = CreateFile(tstr, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	free(tstr);

	if (hFile == INVALID_HANDLE_VALUE) {
		DbgPrintW("Error, cannot open %s, error %d", filePath.c_str(), GetLastError());
		return;
	}

	DWORD bytesWritten = 0;
	BOOL result = WriteFile(hFile, detectable.c_str(), (DWORD) detectable.size(), &bytesWritten, NULL);

	CloseHandle(hFile);

	if (!result || bytesWritten != (DWORD) detectable.size()) {
		DbgPrintW("Cannot write all of the bytes to %s, error %d", filePath.c_str(), GetLastError());
		return;
	}

	LoadData(detectable);
}

void GameScanner::LoadData(const std::string& data)
{
	DbgPrintW("Loading game scanner data...");

	// TODO: This might run out of memory on some systems.  Maybe stream it instead?

	json j = json::parse(data);
	for (auto& game : j)
	{
		// Fields:
		//
		// aliases
		// executables
		// hook
		// id
		// name
		// overlay
		// overlay_compatibility_hook
		// overlay_warn
		// themes
	}

	DbgPrintW("Loaded game scanner data.");
}
