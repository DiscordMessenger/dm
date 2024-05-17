#include <md5/MD5.h>
#include "AvatarCache.hpp"
#include "WinUtils.hpp"
#include "ImageLoader.hpp"

#define MAX_BITMAPS_KEEP_LOADED (256)

//#define DISABLE_AVATAR_LOADING_FOR_DEBUGGING

static AvatarCache s_AvatarCacheSingleton;
AvatarCache* GetAvatarCache() {
	return &s_AvatarCacheSingleton;
}

std::string AvatarCache::MakeIdentifier(const std::string& resource)
{
	if (!m_resourceNameToID[resource].empty())
		return m_resourceNameToID[resource];

	if (resource.size() == 32) {
		bool isAlreadyResource = true;
		for (auto chr : resource) {
			if (!(chr >= '0' && chr <= '9' || chr >= 'a' && chr <= 'f')) {
				isAlreadyResource = false;
				break;
			}
		}
		if (isAlreadyResource)
			return resource;
	}

	m_resourceNameToID[resource] = MD5(resource).finalize().hexdigest();
	return m_resourceNameToID[resource];
}

void AvatarCache::AddImagePlace(const std::string& resource, eImagePlace ip, const std::string& place, Snowflake sf)
{
	m_imagePlaces[MakeIdentifier(resource)] = { ip, sf, place, MakeIdentifier(resource) };
}

void AvatarCache::SetBitmap(const std::string& resource, HBITMAP hbm)
{
	std::string id = MakeIdentifier(resource);

	// Remove the least recently used bitmap until the maximum amount is loaded.
	while (m_profileToBitmap.size() > MAX_BITMAPS_KEEP_LOADED)
	{
		if (!TrimBitmap())
			break;
	}

	auto iter = m_profileToBitmap.find(id);
	if (iter != m_profileToBitmap.end())
	{
		DeleteBitmapIfNeeded(iter->second.first);
		iter->second.first = hbm;
		iter->second.second = 0;
		return;
	}

	m_profileToBitmap[id] = std::make_pair(hbm, 0);
}

ImagePlace AvatarCache::GetPlace(const std::string& resource)
{
	std::string id = MakeIdentifier(resource);
	return m_imagePlaces[id];
}

void AvatarCache::LoadedResource(const std::string& resource)
{
	std::string id = MakeIdentifier(resource);
	auto iter = m_loadingResources.find(id);
	if (iter != m_loadingResources.end())
		m_loadingResources.erase(iter);
}

#include "Main.hpp"
#include "NetworkerThread.hpp"

extern HBITMAP GetDefaultBitmap(); // main.cpp

HBITMAP AvatarCache::GetBitmapSpecial(const std::string& resource)
{
	std::string id = MakeIdentifier(resource);

	auto iter = m_profileToBitmap.find(id);
	if (iter != m_profileToBitmap.end()) {
		iter->second.second = 0;
		return iter->second.first;
	}

	auto iterIP = m_imagePlaces.find(id);
	if (iterIP == m_imagePlaces.end()) {
		// this shouldn't happen.  Just set to default
		DbgPrintW("Could not load resource %s, no image place was registered", id.c_str());
		SetBitmap(id, HBITMAP_LOADING);
		return GetBitmapSpecial(id);
	}

	eImagePlace pla = iterIP->second.type;

	// Check if we have already downloaded a cached version.
	std::string final_path = GetCachePath() + "\\" + id;
	if (FileExists(final_path))
	{
#ifndef DISABLE_AVATAR_LOADING_FOR_DEBUGGING
		DbgPrintW("Loading image %s", final_path.c_str());
		// load that instead.
		FILE* f = fopen(final_path.c_str(), "rb");
		fseek(f, 0, SEEK_END);
		int sz = int(ftell(f));
		fseek(f, 0, SEEK_SET);

		uint8_t* pData = new uint8_t[sz];
		fread(pData, 1, sz, f);

		fclose(f);

		int nsz = pla == eImagePlace::ATTACHMENTS ? 0 : -1;
		HBITMAP hbmp = ImageLoader::ConvertToBitmap(pData, size_t(sz), nsz, nsz);
		if (hbmp) {
			SetBitmap(id, hbmp);
			return hbmp;
		}

		// just return the default...
		DbgPrintW("Image %s could not be decoded!", id.c_str());
#endif
		SetBitmap(id, HBITMAP_ERROR);
		return GetBitmapSpecial(id);
	}

	// Could not find it in the cache, so request it from discord
	SetBitmap(id, HBITMAP_LOADING);

	if (iterIP->second.place.empty()) {
		DbgPrintW("Image %s could not be fetched!  Place is empty", id.c_str());
		return GetBitmapSpecial(id);
	}

	std::string url = iterIP->second.GetURL();

	if (!url.empty())
	{
		// if not inserted already
		if (!m_loadingResources.insert(url).second)
			return GetBitmapSpecial(id);

#ifdef DISABLE_AVATAR_LOADING_FOR_DEBUGGING
		GetFrontend()->OnAttachmentFailed(!iterIP->second.IsAttachment(), id);
#else
		// send a request to the networker thread to grab the profile picture
		GetHTTPClient()->PerformRequest(
			false,
			NetRequest::GET,
			url,
			iterIP->second.IsAttachment() ? DiscordRequest::IMAGE_ATTACHMENT : DiscordRequest::IMAGE,
			uint64_t(iterIP->second.sf),
			"",
			GetDiscordToken(),
			id
		);
#endif
	}
	else
	{
		DbgPrintW("Image %s could not be downloaded! URL is empty!", id.c_str());
	}

	return GetBitmapSpecial(id);
}

HBITMAP AvatarCache::GetBitmapNullable(const std::string& resource)
{
	HBITMAP hbm = GetBitmapSpecial(resource);

	if (hbm == HBITMAP_ERROR || hbm == HBITMAP_LOADING)
		hbm = NULL;

	return hbm;
}

HBITMAP AvatarCache::GetBitmap(const std::string& resource)
{
	HBITMAP hbm = GetBitmapSpecial(resource);

	if (hbm == HBITMAP_ERROR || hbm == HBITMAP_LOADING || !hbm)
		hbm = GetDefaultBitmap();

	return hbm;
}

void AvatarCache::WipeBitmaps()
{
	for (auto b : m_profileToBitmap)
		DeleteBitmapIfNeeded(b.second.first);

	m_profileToBitmap.clear();
	m_imagePlaces.clear();
}

void AvatarCache::EraseBitmap(const std::string& resource)
{
	auto iter = m_profileToBitmap.find(resource);
	if (iter == m_profileToBitmap.end())
		return;

	DeleteBitmapIfNeeded(iter->second.first);
	m_profileToBitmap.erase(iter);
}

bool AvatarCache::TrimBitmap()
{
	int maxAge = 0;
	std::string rid = "";

	for (auto &b : m_profileToBitmap) {
		if (maxAge < b.second.second &&
			b.second.first != GetDefaultBitmap() &&
			b.second.first != HBITMAP_ERROR &&
			b.second.first != HBITMAP_LOADING) {
			maxAge = b.second.second;
			rid    = b.first;
		}
	}

	if (rid.empty()) return false;

	auto iter = m_profileToBitmap.find(rid);
	assert(iter != m_profileToBitmap.end());

	auto iter2 = m_loadingResources.find(m_imagePlaces[iter->first].GetURL());

	if (iter2 != m_loadingResources.end())
		m_loadingResources.erase(iter2);

	DbgPrintW("Deleting bitmap %s", rid.c_str());
	DeleteBitmapIfNeeded(iter->second.first);
	m_profileToBitmap.erase(iter);

	return true;
}

int AvatarCache::TrimBitmaps(int count)
{
	int trimCount = 0;
	for (int i = 0; i < count; i++) {
		if (!TrimBitmap())
			return trimCount;
		trimCount++;
	}
	return trimCount;
}

void AvatarCache::AgeBitmaps()
{
	for (auto &b : m_profileToBitmap) {
		b.second.second++;
	}
}

void AvatarCache::ClearProcessingRequests()
{
	m_loadingResources.clear();
}

void AvatarCache::DeleteBitmapIfNeeded(HBITMAP hbm)
{
	if (hbm && hbm != HBITMAP_LOADING && hbm != HBITMAP_ERROR && hbm != GetDefaultBitmap())
	{
		BOOL result = DeleteObject(hbm);
		assert(result);
	}
}

std::string ImagePlace::GetURL() const
{
	bool bIsAttachment = false;
	std::string path = "";
	switch (type)
	{
		case eImagePlace::AVATARS:
			path = "avatars";
			break;
		case eImagePlace::ICONS:
			path = "icons";
			break;
		case eImagePlace::ATTACHMENTS:
			path = "z";
			bIsAttachment = true;
			break;
	}

	if (path.empty()) {
		DbgPrintW("Image %s could not be downloaded! Path is empty!", key.c_str());
		return "";
	}

	if (bIsAttachment)
		return place;

	return GetDiscordCDN() + path + "/" + std::to_string(sf) + "/" + place
#ifdef WEBP_SUP
		+ ".webp";
#else
		+ ".png";
#endif // WEBP_SUB
}
