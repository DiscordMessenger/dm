#pragma once

#include <windows.h>
#include <cstdint>
#include <vector>

class ImageLoader
{
public:
	// outHasAlphaChannel - If HBITMAP is valid, this value means whether or not the alpha channel is valid.  If
	// false, you shouldn't use AlphaBlend because the channel is most likely filled with zero, which is bad.
	static HBITMAP ConvertToBitmap(const uint8_t* pData, size_t size, bool& outHasAlphaChannel, int width = 0, int height = 0);

	static HBITMAP LoadFromFile(const char* pFileName, bool& outHasAlphaChannel);

	static bool ConvertToPNG(std::vector<uint8_t>* outData, void* pBits, int width, int height, int widthBytes, int bpp, bool forceOpaque, bool flipVerticallyWhenSaving);
};
