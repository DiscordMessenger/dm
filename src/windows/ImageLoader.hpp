#pragma once

#include <windows.h>
#include <cstdint>
#include <vector>

class ImageLoader
{
public:
	static HBITMAP ConvertToBitmap(const uint8_t* pData, size_t size, int width = 0, int height = 0);
	static bool ConvertToPNG(std::vector<uint8_t>* outData, void* pBits, int width, int height, int widthBytes, int bpp);
};

