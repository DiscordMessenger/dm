#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "models/Snowflake.hpp"

// A decoded image, scaled to the size it is shown at: ARGB pixels, alpha in
// the top byte (not premultiplied).
struct Image
{
	int w = 0, h = 0;
	std::vector<uint32_t> px;
};

// Avatars, server icons, emoji and attachment previews: from memory, from
// the cache directory, or downloaded (and then kept there).
namespace ImageCache
{
	enum Kind
	{
		AVATAR,      // place = avatar hash, sf = user
		ICON,        // place = icon hash, sf = guild
		EMOJI,       // sf = emoji
		DEFAULT_AVATAR, // sf = user (Discord's coloured default avatars)
		URL,         // place = the URL (attachments, embed images)
	};

	// The image for (kind, place, sf) at w x h (aspect kept inside that box
	// for URL images; avatars and icons are square), or null while it loads
	// or when it failed.  The first call starts the download.
	const Image* Get(Kind kind, const std::string& place, Snowflake sf, int w, int h);

	// Whether that image failed to load.
	bool Failed(Kind kind, const std::string& place, Snowflake sf, int w, int h);

	// Called on the UI thread when downloaded data arrives (the frontend's
	// OnAttachmentDownloaded / OnAttachmentFailed); key is the request's
	// additional data.
	void Downloaded(const std::string& key, const uint8_t* data, size_t size);
	void DownloadFailed(const std::string& key);

	// Runs (on the UI thread, soon after) when images became available.
	void SetChangedCallback(std::function<void()> fn);

	// Decodes PNG, JPEG, GIF (first frame) and WebP.
	bool Decode(const uint8_t* data, size_t size, Image& out);
}
