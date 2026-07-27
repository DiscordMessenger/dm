#pragma once
#include <string>

namespace ContentType
{
	enum eType
	{
		BLOB,
		PNG,
		JPEG,
		GIF,
		WEBP,
		MP4,
		//...
	};

	static eType GetFromString(const std::string& str) {
		//OutputPrintf("Content type: %s", str.c_str());
		if (str == "image/png")  return PNG;
		if (str == "image/jpeg") return JPEG;
		if (str == "image/gif")  return GIF;
		if (str == "image/webp") return WEBP;

		return BLOB;
	}
}
