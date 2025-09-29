#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <unordered_map>
#include <set>

#include "../core/models/Snowflake.hpp"
#include "ImageLoader.hpp"

enum class eImagePlace
{
	NONE,
	AVATARS,       // profile avatars
	ICONS,         // server icons
	ATTACHMENTS,   // attachments
	CHANNEL_ICONS, // channel icons
	EMOJIS,        // emojis
};

struct ImagePlace {
	eImagePlace type = eImagePlace::NONE;
	Snowflake sf = 0;
	std::string place;
	std::string key;
	int sizeOverride = 0;
	
	ImagePlace() {}
	ImagePlace(eImagePlace t, Snowflake s, const std::string& p, const std::string& k, int so):
		type(t), sf(s), place(p), key(k), sizeOverride(so) {}

	void SetSizeOverride(int so) { sizeOverride = so; }
	bool IsAttachment() const { return type == eImagePlace::ATTACHMENTS; }
	std::string GetURL() const;
};

#define HIMAGE_LOADING ((HImage*) (uintptr_t) 0xDDCCBBAA)
#define HIMAGE_ERROR   ((HImage*) (uintptr_t) 0xDDCCBBAB)

// Kind of a misnomer as it also handles attachment resources.
class AvatarCache
{
private:
	struct BitmapObject
	{
		HImage* m_image = nullptr;
		int m_age = 0;
		bool m_bHasAlpha = false;

		BitmapObject() {}
		BitmapObject(HImage* him, int age, bool hasAlpha) : m_image(him), m_age(age), m_bHasAlpha(hasAlpha) {}

		~BitmapObject() {
		}
	};

protected:
	friend class Frontend_Win32;

	// Set the bitmap associated with the resource ID.
	// Note, after this, hbm is owned by the profile bitmap handler, so you shouldn't delete it
	void SetImage(const std::string& resource, HImage* him, bool hasAlpha);

public:
	// Create a 32-character identifier based on the resource name.  If a 32 character
	// GUID was provided, return it, otherwise perform the MD5 hash of the string.
	std::string MakeIdentifier(const std::string& resource);

	// Let the avatar cache know where resource with the specified ID is located.
	// Returns the resource ID to avoid further md5 hashes on the same content.
	std::string AddImagePlace(const std::string& resource, eImagePlace ip, const std::string& place, Snowflake sf = 0, int sizeOverride = 0);

	// Get the type of the resource with the specified ID.
	ImagePlace GetPlace(const std::string& resource);

	// Let the avatar cache know that the resource was loaded.
	void LoadedResource(const std::string& resource);

	// Get the bitmap associated with the resource.  If it isn't loaded, request it, and return special bitmap handles.
	HImage* GetImageSpecial(const std::string& resource, bool& hasAlphaOut);

	// Get the bitmap associated with the resource.  If it isn't loaded, request it, and return NULL.
	HImage* GetImageNullable(const std::string& resource, bool& hasAlphaOut);

	// Get the bitmap associated with the resource.  If it isn't loaded, request it, and return a default.
	HImage* GetImage(const std::string& resource, bool& hasAlphaOut);

	// Delete all bitmaps.
	void WipeBitmaps();

	// Erase the bitmap with the specified ID.  Make sure no one
	// else is holding a reference to it.
	void EraseBitmap(const std::string& resource);

	// Trim a single bitmap slot.  This makes room for another.
	bool TrimBitmap();

	// Trim X bitmap slots.  This makes room for others.
	int  TrimBitmaps(int num = 1);

	// Age all loaded bitmaps.  This is used to determine which ones are least frequent and ripe to purge.
	void AgeBitmaps();

	// Clear the processing requests set.  These requests will never be fulfilled.
	void ClearProcessingRequests();
	
private:
	// Cache for MakeIdentifier.  MD5 hashes aren't too cheap.
	std::unordered_map<std::string, std::string> m_resourceNameToID;

	// The value has two items: the bitmap itself and an 'age'. From time to time,
	// bitmaps are 'aged'; GetBitmap resets the age of the specific bitmap to zero.
	std::unordered_map<std::string, BitmapObject> m_profileToBitmap;

	// The place where the resource with the specified ID can be found.
	std::unordered_map<std::string, ImagePlace> m_imagePlaces;

	// A list of resources pending load.
	std::set<std::string> m_loadingResources;

	// Delete the image if it isn't the default one.
	static void DeleteImageIfNeeded(HImage* hbm);
};

AvatarCache* GetAvatarCache();
