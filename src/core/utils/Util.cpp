#include <cstdio>
#include <cassert>
#include <cstdarg>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include "Util.hpp"

#include "../../discord/Frontend.hpp"

std::string g_BasePath = "";
std::string g_ProgramNamePath = "";

void SetProgramNamePath(const std::string& programName)
{
	g_ProgramNamePath = programName;
}

void SetBasePath(const std::string& path)
{
	g_BasePath = path;
	if (!path.empty() && path[path.size() - 1] != '\\')
		g_BasePath += '\\';
}

std::string GetBasePath()
{
	return g_BasePath + g_ProgramNamePath;
}

std::string GetCachePath()
{
	return g_BasePath + g_ProgramNamePath + "\\cache";
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

int StringCompareCaseInsens(const char* s1, const char* s2)
{
	while (tolower(*s1) == tolower(*s2)) {
		if (*s1 == '\0') break;
		s1++, s2++;
	}

	return tolower(*s1) - tolower(*s2);
}

int StringCompareCaseInsensLimited(const char* s1, const char* s2, size_t n)
{
	while (n > 0 && tolower(*s1) == tolower(*s2)) {
		n--;
		if (*s1 == '\0') break;
		s1++, s2++;
	}

	if (n == 0)
		return 0;
	return tolower(*s1) - tolower(*s2);
}

bool BeginsWith(const std::string& what, const std::string& with)
{
	if (what.size() < with.size())
		return false;

	return strncmp(what.c_str(), with.c_str(), with.size()) == 0;
}

bool BeginsWithCaseInsens(const std::string& what, const std::string& with)
{
	if (what.size() < with.size())
		return false;

	return StringCompareCaseInsensLimited(what.c_str(), with.c_str(), with.size()) == 0;
}

bool EndsWith(const std::string& what, const std::string& with)
{
	if (what.size() < with.size())
		return false;

	size_t diff = what.size() - with.size();
	return strcmp(what.c_str() + diff, with.c_str()) == 0;
}

bool EndsWithCaseInsens(const std::string& what, const std::string& with)
{
	if (what.size() < with.size())
		return false;

	size_t diff = what.size() - with.size();
	return StringCompareCaseInsens(what.c_str() + diff, with.c_str()) == 0;
}

bool IsPotentiallyDangerousDownload(const std::string& filename)
{
	return
		EndsWithCaseInsens(filename, ".exe") ||
		EndsWithCaseInsens(filename, ".bat") ||
		EndsWithCaseInsens(filename, ".com") ||
		EndsWithCaseInsens(filename, ".vbs") ||
		EndsWithCaseInsens(filename, ".cmd") ||
		EndsWithCaseInsens(filename, ".scr");
}

int64_t ExtractTimestamp(Snowflake sf)
{
	return (sf >> 22) + 1420070400000;
}

std::string GetFieldSafe(const nlohmann::json& j, const std::string& key)
{
	if (j.contains(key) && !j[key].is_null())
		return j[key];

	return "";
}

int GetFieldSafeInt(const nlohmann::json& j, const std::string& key)
{
	if (j.contains(key) && j[key].is_number_integer())
		return j[key];

	return 0;
}

std::string FormatDiscrim(int discrim)
{
	char chr[16];
	snprintf(chr, sizeof chr, "%04d", discrim);
	return std::string(chr);
}

std::string GetGlobalName(const nlohmann::json& j)
{
	if (j.contains("global_name") && !j["global_name"].is_null())
		return GetFieldSafe(j, "global_name");
	else
		return GetFieldSafe(j, "username");
}

std::string GetUsername(const nlohmann::json& j)
{
	std::string username = GetFieldSafe(j, "username");
	int discrim = int(GetIntFromString(GetFieldSafe(j, "discriminator")));

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

bool GetFieldSafeBool(const nlohmann::json& j, const std::string& key, bool default1)
{
	if (j.contains(key) && j[key].is_boolean())
		return j[key];
	return default1;
}

Snowflake GetSnowflakeFromJsonObject(const nlohmann::json& j) {
	if (j.is_number_integer())
		return Snowflake(int64_t(j));
	if (j.is_number_unsigned())
		return Snowflake(uint64_t(j));
	if (j.is_string())
		return Snowflake(GetIntFromString(j));
	return 0;
}

Snowflake GetSnowflake(const nlohmann::json& j, const std::string& key)
{
	auto ji = j.find(key);
	if (ji == j.end())
		return 0;

	return GetSnowflakeFromJsonObject(ji.value());
}

using Json = nlohmann::json;

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
#ifdef _WIN32
	extern time_t MakeGMTime(const tm* ptime);
	time_t t = MakeGMTime(&ptime);
	//time_t t = _mkgmtime(&ptime);
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

std::string FormatTimestampTimeShort(time_t time)
{
	struct tm ptime { 0 };
	ptime = *localtime(&time);
	char buffer[256];
	strftime(buffer, sizeof buffer, GetFrontend()->GetFormatTimestampTimeShort().c_str(), &ptime);
	return std::string(buffer);
}

std::string FormatTimestampTimeLong(time_t time)
{
	struct tm ptime { 0 };
	ptime = *localtime(&time);
	char buffer[256];
	strftime(buffer, sizeof buffer, GetFrontend()->GetFormatTimestampTimeLong().c_str(), &ptime);
	return std::string(buffer);
}

std::string FormatTimestampDateShort(time_t time)
{
	struct tm ptime { 0 };
	ptime = *localtime(&time);
	char buffer[256];
	strftime(buffer, sizeof buffer, GetFrontend()->GetFormatTimestampDateShort().c_str(), &ptime);
	return std::string(buffer);
}

std::string FormatTimestampDateLong(time_t time)
{
	struct tm ptime { 0 };
	ptime = *localtime(&time);
	char buffer[256];
	strftime(buffer, sizeof buffer, GetFrontend()->GetFormatTimestampDateLong().c_str(), &ptime);
	return std::string(buffer);
}

std::string FormatTimestampDateLongTimeShort(time_t time)
{
	struct tm ptime { 0 };
	ptime = *localtime(&time);
	char buffer[256];
	strftime(buffer, sizeof buffer, GetFrontend()->GetFormatTimestampDateLongTimeShort().c_str(), &ptime);
	return std::string(buffer);
}

std::string FormatTimestampDateLongTimeLong(time_t time)
{
	struct tm ptime { 0 };
	ptime = *localtime(&time);
	char buffer[256];
	strftime(buffer, sizeof buffer, GetFrontend()->GetFormatTimestampDateLongTimeLong().c_str(), &ptime);
	return std::string(buffer);
}

std::string FormatTimestampRelative(time_t t)
{
	time_t ct = time(NULL);

	auto diff = ct - t;
	bool future = diff < 0;
	diff = abs(diff);
	
	auto pfx = future ? "in " : "";
	auto sfx = future ? "" : " ago";

	if (diff >= 365 * 24 * 60 * 60) {
		// years
		diff = round(diff / double(365 * 24 * 60 * 60));
		return pfx + std::to_string(diff) + (diff == 1 ? " year" : " years") + sfx;
	}

	if (diff >= 30 * 24 * 60 * 60) {
		// months - TODO, just an approximation
		diff = round(diff / double(30 * 24 * 60 * 60));
		return pfx + std::to_string(diff) + (diff == 1 ? " month" : " months") + sfx;
	}

	if (diff >= 24 * 60 * 60) {
		// days
		diff = round(diff / double(24 * 60 * 60));
		return pfx + std::to_string(diff) + (diff == 1 ? " day" : " days") + sfx;
	}

	if (diff >= 60 * 60) {
		// hours
		diff = round(diff / double(60 * 60));
		return pfx + std::to_string(diff) + (diff == 1 ? " hour" : " hours") + sfx;
	}

	if (diff >= 60) {
		// minutes
		diff = round(diff / double(60));
		return pfx + std::to_string(diff) + (diff == 1 ? " minute" : " minutes") + sfx;
	}

	return pfx + std::to_string(diff) + (diff == 1 ? " second" : " seconds") + sfx;
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
	return 1.10f;
}

// C/C++ macro memes
#define STRINGIFY2(x) #x
#define STRINGIFY(x) STRINGIFY2(x)

const char* GetAppVersionString()
{
#ifdef GIT_COMMIT_HASH
	return "V1.10 [Nightly " STRINGIFY(GIT_COMMIT_HASH) "]";
#else
	return "V1.10";
#endif
}

std::string FormatDuration(int seconds)
{
	int minutes = seconds / 60;
	seconds %= 60;
	int hours   = minutes / 60;
	minutes %= 60;
	int days    = hours / 24;
	hours %= 24;

	std::string result;
	if (days) {
		result += std::to_string(days);
		if (days == 1) result += " day";
		else           result += " days";
	}
	if (hours) {
		if (!result.empty()) result += ", ";
		result += std::to_string(hours);
		if (hours == 1) result += " hr";
		else            result += " hrs";
	}
	if (minutes) {
		if (!result.empty()) result += ", ";
		result += std::to_string(minutes);
		if (minutes == 1) result += " min";
		else              result += " mins";
	}
	if (seconds) {
		if (!result.empty()) result += ", ";
		result += std::to_string(seconds);
		if (seconds == 1) result += " sec";
		else              result += " secs";
	}
	if (result.empty()) {
		result = "0 secs";
	}

	return result;
}

std::string Format(const char* fmt, ...)
{
	va_list vl;
	va_start(vl, fmt);

	char pos[2048];
	vsnprintf(pos, sizeof pos, fmt, vl);
	pos[sizeof pos - 1] = 0;

	va_end(vl);

	return std::string(pos);
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
