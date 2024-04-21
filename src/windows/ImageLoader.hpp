#pragma once

#include <Windows.h>
#include <cstdint>

class ImageLoader
{
public:
	static HBITMAP ConvertToBitmap(const uint8_t* pData, size_t size, int width = 0, int height = 0);
};

