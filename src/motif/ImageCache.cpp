#include "ImageCache.hpp"

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <list>
#include <map>
#include <sys/stat.h>

#include <md5/MD5.h>

#include "Frontend.hpp"
#include "network/DiscordAPI.hpp"
#include "network/DiscordRequest.hpp"
#include "network/HTTPClient.hpp"
#include "utils/Util.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_GIF
#define STBI_NO_STDIO
#include <stb/stb_image.h>

#ifndef DISABLE_WEBP
#include <webp/decode.h>
#endif

// Images kept decoded in memory; the least recently used go first.
static const size_t MAX_IMAGES = 400;

namespace
{
	enum State { LOADING, READY, FAILED };

	struct Entry
	{
		State state = LOADING;
		Image image;
		std::list<std::string>::iterator lru;
		bool inLru = false;
	};

	// One download serves every size wanted of a source.
	struct Source
	{
		std::string url;
		std::vector<std::pair<int, int>> sizes;
		bool requested = false;
	};

	std::map<std::string, Entry> g_entries;     // key: source id + size
	std::map<std::string, Source> g_sources;    // key: source id
	std::list<std::string> g_lru;               // front: most recent
	std::function<void()> g_changed;
	bool g_changedPending = false;

	int NearestPowerOfTwo(int x)
	{
		int p = 16;
		while (p < x && p < 4096)
			p <<= 1;
		return p;
	}

	std::string SourceURL(ImageCache::Kind kind, const std::string& place, Snowflake sf, int size)
	{
		int px = NearestPowerOfTwo(std::max(size * 2, 32)); // some headroom for the scaling
		switch (kind) {
			case ImageCache::AVATAR:
				return GetDiscordCDN() + "avatars/" + std::to_string(sf) + "/" + place + ".png?size=" + std::to_string(px);
			case ImageCache::ICON:
				return GetDiscordCDN() + "icons/" + std::to_string(sf) + "/" + place + ".png?size=" + std::to_string(px);
			case ImageCache::EMOJI:
				return GetDiscordCDN() + "emojis/" + std::to_string(sf) + ".png?size=" + std::to_string(px);
			case ImageCache::DEFAULT_AVATAR:
				return GetDiscordCDN() + "embed/avatars/" + std::to_string((sf >> 22) % 6) + ".png";
			case ImageCache::URL:
			default:
				return place;
		}
	}

	std::string SourceId(ImageCache::Kind kind, const std::string& place, Snowflake sf)
	{
		std::string what = std::to_string((int) kind) + ":" + place + ":" + std::to_string(sf);
		return MD5(what).finalize().hexdigest();
	}

	std::string EntryKey(const std::string& id, int w, int h)
	{
		return id + "@" + std::to_string(w) + "x" + std::to_string(h);
	}

	std::string CacheFile(const std::string& id)
	{
		return GetCachePath() + "/" + id;
	}

	void Touch(const std::string& key, Entry& e)
	{
		if (e.inLru)
			g_lru.erase(e.lru);
		g_lru.push_front(key);
		e.lru = g_lru.begin();
		e.inLru = true;

		while (g_lru.size() > MAX_IMAGES) {
			std::string old = g_lru.back();
			g_lru.pop_back();
			g_entries.erase(old);
		}
	}

	// Area-averaging scale into a w x h box, keeping the aspect ratio when
	// fit is set; alpha-weighted so transparent edges do not darken.
	void Scale(const Image& src, int w, int h, bool fit, Image& out)
	{
		if (fit && src.w > 0 && src.h > 0) {
			double s = std::min((double) w / src.w, (double) h / src.h);
			if (s > 1.0) s = 1.0; // never enlarge previews
			w = std::max(1, (int) (src.w * s + 0.5));
			h = std::max(1, (int) (src.h * s + 0.5));
		}
		out.w = w;
		out.h = h;
		out.px.assign((size_t) w * h, 0);
		if (src.w == w && src.h == h) {
			out.px = src.px;
			return;
		}
		for (int y = 0; y < h; y++)
		{
			int y0 = y * src.h / h, y1 = std::max(y0 + 1, (y + 1) * src.h / h);
			for (int x = 0; x < w; x++)
			{
				int x0 = x * src.w / w, x1 = std::max(x0 + 1, (x + 1) * src.w / w);
				uint64_t a = 0, r = 0, g = 0, b = 0;
				int n = 0;
				for (int sy = y0; sy < y1 && sy < src.h; sy++)
				for (int sx = x0; sx < x1 && sx < src.w; sx++) {
					uint32_t p = src.px[(size_t) sy * src.w + sx];
					uint32_t pa = p >> 24;
					a += pa;
					r += ((p >> 16) & 0xff) * pa;
					g += ((p >> 8) & 0xff) * pa;
					b += (p & 0xff) * pa;
					n++;
				}
				if (!n || !a)
					continue;
				out.px[(size_t) y * w + x] = (uint32_t) ((a / n) << 24) |
					(uint32_t) ((r / a) << 16) | (uint32_t) ((g / a) << 8) | (uint32_t) (b / a);
			}
		}
	}

	void NotifyChanged()
	{
		if (!g_changed)
			return;
		g_changed();
	}

	// Fills every entry waiting for this source, from the encoded data.
	// Data that does not decode fails them only when final (a download);
	// a bad cache file just means downloading it again.
	bool Deliver(const std::string& id, const uint8_t* data, size_t size, bool final)
	{
		auto sit = g_sources.find(id);
		if (sit == g_sources.end())
			return false;

		Image full;
		bool ok = ImageCache::Decode(data, size, full);
		if (!ok && !final)
			return false;
		for (auto& sz : sit->second.sizes)
		{
			std::string key = EntryKey(id, sz.first, sz.second);
			Entry& e = g_entries[key];
			if (!ok) {
				e.state = FAILED;
				continue;
			}
			bool fit = sz.first != sz.second || full.w != full.h;
			Scale(full, sz.first, sz.second, fit, e.image);
			e.state = READY;
			Touch(key, e);
		}
		sit->second.sizes.clear();
		return ok;
	}

	bool ReadFile(const std::string& path, std::string& out)
	{
		FILE* f = fopen(path.c_str(), "rb");
		if (!f)
			return false;
		char buf[65536];
		size_t n;
		while ((n = fread(buf, 1, sizeof buf, f)) > 0)
			out.append(buf, n);
		fclose(f);
		return !out.empty();
	}
}

bool ImageCache::Decode(const uint8_t* data, size_t size, Image& out)
{
	if (!data || size < 12)
		return false;

#ifndef DISABLE_WEBP
	if (!memcmp(data, "RIFF", 4) && !memcmp(data + 8, "WEBP", 4))
	{
		int w = 0, h = 0;
		uint8_t* rgba = WebPDecodeRGBA(data, size, &w, &h);
		if (!rgba)
			return false;
		out.w = w;
		out.h = h;
		out.px.resize((size_t) w * h);
		for (size_t i = 0; i < out.px.size(); i++) {
			const uint8_t* p = rgba + i * 4;
			out.px[i] = ((uint32_t) p[3] << 24) | ((uint32_t) p[0] << 16) | ((uint32_t) p[1] << 8) | p[2];
		}
		WebPFree(rgba);
		return true;
	}
#endif

	int w = 0, h = 0, comp = 0;
	uint8_t* rgba = stbi_load_from_memory(data, (int) size, &w, &h, &comp, 4);
	if (!rgba)
		return false;
	out.w = w;
	out.h = h;
	out.px.resize((size_t) w * h);
	for (size_t i = 0; i < out.px.size(); i++) {
		const uint8_t* p = rgba + i * 4;
		out.px[i] = ((uint32_t) p[3] << 24) | ((uint32_t) p[0] << 16) | ((uint32_t) p[1] << 8) | p[2];
	}
	stbi_image_free(rgba);
	return true;
}

const Image* ImageCache::Get(Kind kind, const std::string& place, Snowflake sf, int w, int h)
{
	if (w <= 0 || h <= 0 || (kind == URL && place.empty()))
		return nullptr;

	std::string id = SourceId(kind, place, sf);
	std::string key = EntryKey(id, w, h);
	auto it = g_entries.find(key);
	if (it != g_entries.end()) {
		if (it->second.state == READY) {
			Touch(key, it->second);
			return &it->second.image;
		}
		return nullptr;
	}

	g_entries[key].state = LOADING;
	Source& src = g_sources[id];
	src.sizes.push_back(std::make_pair(w, h));
	if (src.url.empty())
		src.url = SourceURL(kind, place, sf, std::max(w, h));

	// on disk already?
	std::string data;
	if (ReadFile(CacheFile(id), data)) {
		if (Deliver(id, (const uint8_t*) data.data(), data.size(), false)) {
			Entry& e = g_entries[key];
			return e.state == READY ? &e.image : nullptr;
		}
		remove(CacheFile(id).c_str());
	}

	if (!src.requested) {
		src.requested = true;
		GetHTTPClient()->PerformRequest(
			false,
			NetRequest::GET,
			src.url,
			kind == URL ? DiscordRequest::IMAGE_ATTACHMENT : DiscordRequest::IMAGE,
			sf,
			"",
			"",
			id
		);
	}
	return nullptr;
}

bool ImageCache::Failed(Kind kind, const std::string& place, Snowflake sf, int w, int h)
{
	auto it = g_entries.find(EntryKey(SourceId(kind, place, sf), w, h));
	return it != g_entries.end() && it->second.state == FAILED;
}

void ImageCache::Downloaded(const std::string& id, const uint8_t* data, size_t size)
{
	auto sit = g_sources.find(id);
	if (sit != g_sources.end())
		sit->second.requested = false;

	if (Deliver(id, data, size, true)) {
		// keep it for the next run
		std::string path = CacheFile(id), tmp = path + ".new";
		FILE* f = fopen(tmp.c_str(), "wb");
		if (f) {
			bool ok = fwrite(data, 1, size, f) == size;
			ok = fclose(f) == 0 && ok;
			if (!ok || rename(tmp.c_str(), path.c_str()) != 0)
				remove(tmp.c_str());
		}
	}
	NotifyChanged();
}

void ImageCache::DownloadFailed(const std::string& id)
{
	auto sit = g_sources.find(id);
	if (sit == g_sources.end())
		return;
	sit->second.requested = false;
	for (auto& sz : sit->second.sizes)
		g_entries[EntryKey(id, sz.first, sz.second)].state = FAILED;
	sit->second.sizes.clear();
	NotifyChanged();
}

void ImageCache::SetChangedCallback(std::function<void()> fn)
{
	g_changed = fn;
}
