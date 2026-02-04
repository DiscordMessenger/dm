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
		AVI,
		MOV,
		WMV,
		//...
	};

	static eType GetFromString(const std::string& str) {
		//OutputPrintf("Content type: %s", str.c_str());
		if (str == "image/png")  return PNG;
		if (str == "image/jpeg") return JPEG;
		if (str == "image/gif")  return GIF;
		if (str == "image/webp") return WEBP;

		if (str == "video/mp4") return MP4;
		if (str == "video/x-msvideo") return AVI;
		if (str == "video/quicktime") return MOV;
		if (str == "video/x-ms-wmv") return WMV;

		return BLOB;
	}
}
