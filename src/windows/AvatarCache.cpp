#include <md5/MD5.h>
#include "AvatarCache.hpp"
#include "WinUtils.hpp"
#include "ImageLoader.hpp"

#define MAX_BITMAPS_KEEP_LOADED (256)

//#define DISABLE_AVATAR_LOADING_FOR_DEBUGGING

static int NearestPowerOfTwo(int x) {
	if (x > (1 << 30))
		return 1 << 30;

	if (x < 2)
		return 2;

	int i;
	for (i = 1; i < x; i <<= 1);
	return i;
}

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

std::string AvatarCache::AddImagePlace(const std::string& resource, eImagePlace ip, const std::string& place, Snowflake sf, int sizeOverride)
{
	std::string id = MakeIdentifier(resource);
	m_imagePlaces[id] = { ip, sf, place, id, sizeOverride };
	return id;
}

void AvatarCache::SetImage(const std::string& resource, HImage* him, bool hasAlpha)
{
	std::string id = MakeIdentifier(resource);

	// Remove the least recently used bitmap until the maximum amount is loaded.
	while (m_profileToBitmap.size() > MAX_BITMAPS_KEEP_LOADED)
	{
		if (!TrimBitmap())
			break;
	}

	AgeBitmaps();

	auto iter = m_profileToBitmap.find(id);
	if (iter != m_profileToBitmap.end())
	{
		DeleteImageIfNeeded(iter->second.m_image);
		iter->second.m_image = him;
		iter->second.m_age = 0;
		iter->second.m_bHasAlpha = hasAlpha;
		return;
	}

	m_profileToBitmap[id] = BitmapObject(him, 0, hasAlpha);
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

//extern HBITMAP GetDefaultBitmap(); // main.cpp
extern HImage* GetDefaultImage(); // main.cpp

HImage* AvatarCache::GetImageSpecial(const std::string& resource, bool& hasAlphaOut)
{
	std::string id = MakeIdentifier(resource);

	auto iter = m_profileToBitmap.find(id);
	if (iter != m_profileToBitmap.end()) {
		iter->second.m_age = 0;
		hasAlphaOut = iter->second.m_bHasAlpha;
		return iter->second.m_image;
	}

	auto iterIP = m_imagePlaces.find(id);
	if (iterIP == m_imagePlaces.end()) {
		// this shouldn't happen.  Just set to default
		DbgPrintW("Could not load resource %s, no image place was registered", id.c_str());
		SetImage(id, HIMAGE_LOADING, false);
		return GetImageSpecial(id, hasAlphaOut);
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
		if (!f) {
			SetImage(id, HIMAGE_ERROR, false);
			return GetImageSpecial(id, hasAlphaOut);
		}

		fseek(f, 0, SEEK_END);
		int sz = int(ftell(f));
		fseek(f, 0, SEEK_SET);

		uint8_t* pData = new uint8_t[sz];
		fread(pData, 1, sz, f);

		fclose(f);

		int nsz = pla == eImagePlace::ATTACHMENTS ? 0 : -1;
		bool hasAlpha = false;
		HImage* himg = ImageLoader::ConvertToBitmap(pData, size_t(sz), hasAlpha, false, nsz, nsz);

		if (himg && himg->IsValid())
		{
			SetImage(id, himg, hasAlpha);
			hasAlphaOut = hasAlpha;
			return himg;
		}

		SAFE_DELETE(himg);

		// just return the default...
		DbgPrintW("Image %s could not be decoded!", id.c_str());
#endif
		SetImage(id, HIMAGE_ERROR, false);
		return GetImageSpecial(id, hasAlphaOut);
	}

	// Could not find it in the cache, so request it from discord
	SetImage(id, HIMAGE_LOADING, false);

	if (iterIP->second.place.empty()) {
		DbgPrintW("Image %s could not be fetched!  Place is empty", id.c_str());
		return GetImageSpecial(id, hasAlphaOut);
	}

	std::string url = iterIP->second.GetURL();

	if (!url.empty())
	{
		// if not inserted already
		if (!m_loadingResources.insert(url).second)
			return GetImageSpecial(id, hasAlphaOut);

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

	return GetImageSpecial(id, hasAlphaOut);
}

HImage* AvatarCache::GetImageNullable(const std::string& resource, bool& hasAlphaOut)
{
	HImage* him = GetImageSpecial(resource, hasAlphaOut);

	if (him == HIMAGE_ERROR || him == HIMAGE_LOADING)
		him = NULL;

	return him;
}

HImage* AvatarCache::GetImage(const std::string& resource, bool& hasAlphaOut)
{
	hasAlphaOut = false;
	HImage* him = GetImageSpecial(resource, hasAlphaOut);

	if (him == HIMAGE_ERROR || him == HIMAGE_LOADING || !him)
		him = GetDefaultImage();

	return him;
}

void AvatarCache::WipeBitmaps()
{
	for (auto b : m_profileToBitmap)
		DeleteImageIfNeeded(b.second.m_image);

	m_profileToBitmap.clear();
	m_imagePlaces.clear();
}

void AvatarCache::EraseBitmap(const std::string& resource)
{
	auto iter = m_profileToBitmap.find(resource);
	if (iter == m_profileToBitmap.end())
		return;

	DeleteImageIfNeeded(iter->second.m_image);
	m_profileToBitmap.erase(iter);
}

bool AvatarCache::TrimBitmap()
{
	int maxAge = 0;
	std::string rid = "";

	for (auto &b : m_profileToBitmap) {
		if (maxAge < b.second.m_age &&
			b.second.m_image != GetDefaultImage() &&
			b.second.m_image != HIMAGE_ERROR &&
			b.second.m_image != HIMAGE_LOADING) {
			maxAge = b.second.m_age;
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
	DeleteImageIfNeeded(iter->second.m_image);
	m_profileToBitmap.erase(iter);

	return true;
}

int AvatarCache::TrimBitmaps(int count)
{
	if (count < 0)
		return 0;

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
		b.second.m_age++;
	}
}

void AvatarCache::ClearProcessingRequests()
{
	m_loadingResources.clear();
}

void AvatarCache::DeleteImageIfNeeded(HImage* him)
{
	if (him && him != HIMAGE_LOADING && him != HIMAGE_ERROR && him != GetDefaultImage())
		delete him;
}

std::string ImagePlace::GetURL() const
{
	bool bIsAttachment = false;
	bool useOnlySnowflake = false;
	bool dontAppendSize = false;
	std::string path = "";
	switch (type)
	{
		case eImagePlace::AVATARS:
			path = "avatars";
			break;
		case eImagePlace::ICONS:
			path = "icons";
			break;
		case eImagePlace::CHANNEL_ICONS:
			path = "channel-icons";
			break;
		case eImagePlace::EMOJIS:
			path = "emojis";
			useOnlySnowflake = true;
			break;
		case eImagePlace::ATTACHMENTS:
			path = "z";
			bIsAttachment = true;
			dontAppendSize = true;
			break;
	}

	if (path.empty()) {
		DbgPrintW("Image %s could not be downloaded! Path is empty!", key.c_str());
		return "";
	}

	if (bIsAttachment)
		return place;

	std::string url = GetDiscordCDN() + path + "/" + std::to_string(sf);

	if (!useOnlySnowflake)
		url += "/" + place;

	// Add in the webp / png at the end.
	url += 
#ifdef WEBP_SUP
		".webp";
#else
		".png";
#endif // WEBP_SUB

	// Also the size should reflect the active profile picture size.
	if (!dontAppendSize)
	{
		int size = NearestPowerOfTwo(sizeOverride ? sizeOverride : GetProfilePictureSize());
		// actually increase it a bit to increase quality
		if (size < 128)
			size = 128;

		url += "?size=" + std::to_string(size);
	}

	return url;
}
