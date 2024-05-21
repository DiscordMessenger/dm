#pragma once

#include <string>
#include <nlohmann/json.h>
#include "Snowflake.hpp"

#ifdef _DEBUG
#define USE_DEBUG_PRINTS
#endif

std::string GetBasePath();
std::string GetCachePath();
void SetBasePath(const std::string& appDataPath);
uint64_t HashStringLong(const char* str, int len);
std::string CombineNicely(Snowflake sf, std::string avkey);
bool ReadEntireFile(const std::string& fileName, char** data, size_t* size, bool readBinary);
std::string LoadEntireTextFile(const std::string& fileName);
std::string HttpEncodeString(std::string str);
std::string GetSizeString(size_t sz);
int StringCompareCaseInsens(const char* s1, const char* s2);
bool EndsWith(const std::string& what, const std::string& with);
bool EndsWithCaseInsens(const std::string& what, const std::string& with);
bool IsPotentiallyDangerousDownload(const std::string& filename);
int64_t ExtractTimestamp(Snowflake sf);
const uint64_t GetTimeMs() noexcept;
const uint64_t GetTimeUs() noexcept;
Snowflake GetSnowflakeFromJsonObject(nlohmann::json& j);
Snowflake GetSnowflake(nlohmann::json& j, const std::string& key);
int64_t GetIntFromString(const std::string& str);
std::string FormatDiscrim(int discrim);
std::string GetGlobalName(nlohmann::json& j);
std::string GetUsername(nlohmann::json& j);
int GetFieldSafeInt(nlohmann::json& j, const std::string& key);
bool GetFieldSafeBool(nlohmann::json& j, const std::string& key, bool default1);
std::string GetFieldSafe(nlohmann::json& j, const std::string& key);
std::string GetMonthName(int mon);
const char* GetDaySuffix(int day);
time_t ParseTime(const std::string& iso8601);
std::string FormatDate(time_t time); // January 1, 1970
std::string FormatTimeLong(time_t time, bool relativity = false); // relativity=true means "Today at" and "Yesterday at" show
std::string FormatTimeShort(time_t time);
std::string FormatTimeShorter(time_t time);
void SplitURL(const std::string& url, std::string& domainOut, std::string& resourceOut);
std::string CreateMessageLink(Snowflake guild, Snowflake channel, Snowflake message);
float CompareFuzzy(const std::string& item, const char* query); // returns a "closeness" factor, =0 if no match, >0 if match. The closeness is used for sorting matches
float GetAppVersion();
std::string GetAppVersionString();

#ifdef USE_DEBUG_PRINTS
void DbgPrintF(const char* fmt, ...);
#else
#define DbgPrintF(...)
#endif
