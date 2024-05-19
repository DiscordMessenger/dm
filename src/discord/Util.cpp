#include <cstdio>
#include <cassert>
#include <cstdarg>
#include <sstream>
#include <iomanip>
#include <chrono>
#include "Util.hpp"
#include "Frontend.hpp"

std::string g_BasePath = "";

void SetBasePath(const std::string& path)
{
	g_BasePath = path;
	if (!path.empty() && path[path.size() - 1] != '\\')
		g_BasePath += '\\';

	g_BasePath += "DiscordMessenger";
}

std::string GetBasePath()
{
	return g_BasePath;
}

std::string GetCachePath()
{
	return g_BasePath + "\\cache";
}

unsigned long long BitMix(uint64_t lol)
{
	lol ^= lol >> 33;
	lol *= 0xff51afd7ed558ccdLL;
	lol ^= lol >> 33;
	lol *= 0xc4ceb9fe1a85ec53LL;
	lol ^= lol >> 33;
	return lol;
}

uint64_t HashStringLong(const char* str, int len)
{
	uint64_t startHash = 0x5555AAAA5555AAAA;
	if (len == 0) len = int(strlen(str));
	for (int i = 0; i < len - 7; i += 8)
	{
		uint64_t lol = *(uint64_t*)&str[i];
		lol = BitMix(lol);
		startHash ^= lol;
	}
	int remStart = len / 8;
	remStart *= 8;
	uint64_t tbh = 0;
	for (int i = remStart; i < len; i++)
	{
		tbh |= (1LL << (8LL * (i - remStart))) * str[i];
	}
	tbh = BitMix(tbh);
	startHash ^= tbh;
	return startHash;
}

std::string CombineNicely(Snowflake sf, std::string avkey)
{
	std::string str = std::to_string(sf) + "_" + avkey;

	return std::to_string(HashStringLong(str.c_str(), 0));
}

bool ReadEntireFile(const std::string& fileName, char** data, size_t* size, bool readBinary)
{
	FILE* f = fopen(fileName.c_str(), readBinary ? "rb" : "r");
	if (!f)
	{
		*data = nullptr;
		*size = 0;
		return false;
	}

	fseek(f, 0, SEEK_END);
	int sz = int(ftell(f));
	fseek(f, 0, SEEK_SET);

	assert(sizeof(char) == 1);
	char* pData = new char[sz];
	*size = fread(pData, 1, sz, f);

	fclose(f);

	*data = pData;
	return true;
}

std::string LoadEntireTextFile(const std::string& fileName)
{
	char* data = nullptr;
	size_t size = 0;
	if (!ReadEntireFile(fileName, &data, &size, false))
		return "";

	std::string str(data, size);
	delete[] data;
	return str;
}

std::string HttpEncodeString(std::string str)
{
	std::stringstream strstr;

	for (char c : str)
	{
		if (strchr("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ*<>-_.", c) != NULL)
			strstr << c;
		else if (c == ' ')
			strstr << '+';
		else
			strstr << "%" << std::hex << (int)c << std::dec;
	}

	return strstr.str();
}

std::string GetSizeString(size_t sz)
{
	size_t lower1024 = 0;
	int unitIndex = 0;
	while (sz >= 1024) {
		unitIndex++;
		lower1024 = sz & 0x3FF;
		sz >>= 10;
	}

	static const char* lutUnit[] = { "bytes","kB","MB","GB","TB","PB","EB","YB" };

	std::stringstream ss;
	ss << sz;
	if (lower1024)
		ss << "." << std::setw(2) << std::setfill('0') << (lower1024 * 1000 / 1024 / 10);
	ss << " " << lutUnit[unitIndex];

	return ss.str();
}

bool EndsWith(const std::string& what, const std::string& with)
{
	if (what.size() < with.size())
		return false;

	size_t diff = what.size() - with.size();
	return strcmp(what.c_str() + diff, with.c_str()) == 0;
}

bool IsPotentiallyDangerousDownload(const std::string& filename)
{
	return
		EndsWith(filename, ".exe") ||
		EndsWith(filename, ".bat") ||
		EndsWith(filename, ".com") ||
		EndsWith(filename, ".vbs") ||
		EndsWith(filename, ".cmd") ||
		EndsWith(filename, ".scr");
}

int64_t ExtractTimestamp(Snowflake sf)
{
	return (sf >> 22) + 1420070400000;
}

using Json = nlohmann::json;

std::string GetFieldSafe(Json& j, const std::string& key)
{
	if (j.contains(key) && !j[key].is_null())
		return j[key];

	return "";
}

int GetFieldSafeInt(Json& j, const std::string& key)
{
	if (j.contains(key) && !j[key].is_null())
		return j[key];

	return 0;
}

std::string FormatDiscrim(int discrim)
{
	char chr[16];
	snprintf(chr, sizeof chr, "%04d", discrim);
	return std::string(chr);
}

std::string GetGlobalName(Json& j)
{
	if (j.contains("global_name") && !j["global_name"].is_null())
		return j["global_name"];
	else
		return j["username"];
}

std::string GetUsername(nlohmann::json& j)
{
	std::string username = GetFieldSafe(j, "username");
	int discrim = GetIntFromString(GetFieldSafe(j, "discriminator"));

	if (discrim > 0)
		username += "#" + FormatDiscrim(discrim);

	return username;
}

int64_t GetIntFromString(const std::string& str)
{
	std::stringstream sstr(str);
	int64_t t = 0;
	if (!(sstr >> t))
		return 0;
	return t;
}

bool GetFieldSafeBool(nlohmann::json& j, const std::string& key, bool default1)
{
	if (j.contains(key) && j.is_boolean())
		return j["key"];
	return default1;
}

Snowflake GetSnowflakeFromJsonObject(Json& j) {
	if (j.is_number_integer())
		return Snowflake(int64_t(j));
	if (j.is_number_unsigned())
		return Snowflake(uint64_t(j));
	if (j.is_string())
		return Snowflake(GetIntFromString(j));
	return 0;
}

Snowflake GetSnowflake(Json& j, const std::string& key)
{
	return GetSnowflakeFromJsonObject(j[key]);
}

const uint64_t GetTimeMs() noexcept
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
const uint64_t GetTimeUs() noexcept
{
	return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

std::string GetMonthName(int mon)
{
	return GetFrontend()->GetMonthName(mon);
}

const char* GetDaySuffix(int day)
{
	if (day == 1 || (day > 20 && day % 10 == 1)) return "st";
	if (day == 2 || (day > 20 && day % 10 == 2)) return "nd";
	if (day == 3 || (day > 20 && day % 10 == 3)) return "rd";
	return "th";
}

time_t ParseTime(const std::string& iso8601)
{
	// tokenize date
	std::vector<std::string> dateTokens;
	std::string currentToken = "";
	bool encounteredT = false;
	for (auto c : iso8601)
	{
		if (c == 'T' || c == 'Z' || c == '+' || (c == '-' && encounteredT)) {
			dateTokens.push_back(currentToken);
			currentToken.clear();
		}
		if (c == 'T') encounteredT = true;
		currentToken += c;
	}

	if (!currentToken.empty())
		dateTokens.push_back(currentToken);

	struct tm ptime { 0 };

	// Date string format: yyyy-mm-ddThh:mm:ss.wwwzzz+oo:pp
	// w - millisecond, z - microsecond (0)
	// oo - offset hr
	// pp - offset min

	for (auto& tk : dateTokens)
	{
		if (tk.empty())
			continue;

		if (tk[0] == 'T') {
			// time mode
			int h = 0, m = 0, s = 0;
			sscanf(tk.c_str() + 1, "%d:%d:%d", &h, &m, &s);
			ptime.tm_hour = h;
			ptime.tm_min  = m;
			ptime.tm_sec  = s;
			continue;
		}

		if (tk[0] == '+' || tk[0] == '-') {
			// time zone offset mode TODO
			continue;
		}

		if (isdigit(tk[0])) {
			// date mode
			int y = 0, m = 0, d = 0;
			sscanf(tk.c_str(), "%d-%d-%d", &y, &m, &d);
			ptime.tm_year = y - 1900;
			ptime.tm_mon  = m - 1;
			ptime.tm_mday = d;
			continue;
		}

		assert(!"unknown date mode");
	}

	// Convert to time_t
	// XXX timegm on linux
#ifdef MINGW_SPECIFIC_HACKS
	time_t t = mktime(&ptime);
#elif _WIN32
	time_t t = _mkgmtime(&ptime);
#else
	time_t t = timegm(&ptime);
#endif
	return t;
}

std::string FormatDate(time_t time)
{
	struct tm ptime { 0 };
	ptime = *localtime(&time);
	char buff[2048];
	snprintf(
		buff,
		sizeof buff,
		GetFrontend()->GetFormatDateOnlyText().c_str(),
		GetMonthName(ptime.tm_mon).c_str(),
		ptime.tm_mday,
		GetDaySuffix(ptime.tm_mday),
		ptime.tm_year + 1900
	);
	buff[sizeof buff - 1] = 0;
	return std::string(buff);
}

std::string FormatTimeLong(time_t time, bool relativity)
{
	// Full time: [date noun] at [time] OR [date] [time]
	struct tm ptime { 0 };
	ptime = *localtime(&time);
	char buff[2048];
	bool istoday = false, isyday = false;
	if (relativity) {
		time_t todaytime = ::time(NULL);
		struct tm todaytm = *localtime(&todaytime); // fetch today's time
		todaytime -= 86400;
		struct tm ydaytm = *localtime(&todaytime); // fetch yesterday's time
		istoday = todaytm.tm_yday == ptime.tm_yday && todaytm.tm_year == ptime.tm_year;
		isyday  = todaytm.tm_yday == ptime.tm_yday && ydaytm.tm_year  == ptime.tm_year;
	}

	/**/ if (istoday) strftime(buff, sizeof buff, GetFrontend()->GetTodayAtText().c_str(), &ptime);
	else if (isyday)  strftime(buff, sizeof buff, GetFrontend()->GetYesterdayAtText().c_str(), &ptime);
	else              strftime(buff, sizeof buff, GetFrontend()->GetFormatTimeLongText().c_str(), &ptime);
	buff[sizeof buff - 1] = 0;

	return std::string(buff);
}

std::string FormatTimeShort(time_t time)
{
	// Compact time: dd/mm hh:mm
	struct tm ptime { 0 };
	ptime = *localtime(&time);
	char buff[2048];
	strftime(buff, sizeof buff, GetFrontend()->GetFormatTimeShortText().c_str(), &ptime);
	buff[sizeof buff - 1] = 0;
	return std::string(buff);
}

std::string FormatTimeShorter(time_t time)
{
	// Compact time: dd/mm hh:mm
	struct tm ptime { 0 };
	ptime = *localtime(&time);
	char buff[2048];
	strftime(buff, sizeof buff, GetFrontend()->GetFormatTimeShorterText().c_str(), &ptime);
	buff[sizeof buff - 1] = 0;
	return std::string(buff);
}

void SplitURL(const std::string& url, std::string& domainOut, std::string& resourceOut)
{
	const char* urlc = url.c_str();

	// skip the http:// prefix
	/**/ if (strncmp(urlc, "http://", 7) == 0) urlc += 7;
	else if (strncmp(urlc, "https://", 8) == 0) urlc += 8;

	const char* slashplace = strchr(urlc, '/');

	std::string domain;
	if (slashplace) {
		domainOut = std::string(urlc, slashplace - urlc);
		resourceOut = std::string(slashplace);
	}
	else {
		domainOut = std::string(urlc);
		resourceOut = "";
	}
}

float CompareFuzzy(const std::string& item, const char* query)
{
	if (*query == '\0')
		return 0.0f;

	size_t matched, idx;
	for (idx = 0, matched = 0; idx < item.size() && query[matched] != 0; idx++)
	{
		if (tolower(item[idx]) == tolower(query[matched]))
			matched++;
	}

	if (query[matched] != '\0')
		return 0.0f;

	float res = float(matched) / float(idx);
	if (res < 0.0001f)
		res = 0.0001f;

	return res;
}

float GetAppVersion()
{
	return 1.02f;
}

std::string GetAppVersionString()
{
	return "V1.02";
}

#ifdef USE_DEBUG_PRINTS
void DbgPrintF(const char* fmt, ...)
{
	va_list vl;
	va_start(vl, fmt);
	GetFrontend()->DebugPrint(fmt, vl);
	va_end(vl);
}
#endif
