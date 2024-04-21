#include "Main.hpp"
#include "ImageLoader.hpp"

typedef void(*FreeFunction)(void*);

void NopFree(void* unused) {}

#ifdef WEBP_SUP

#include <webp/decode.h>

static uint8_t* DecodeWebp(FreeFunction* pFreeFunc, const uint8_t* pData, size_t size, int* width, int* height)
{
	*pFreeFunc = WebPFree;
	return WebPDecodeBGRA(pData, size, width, height);
}

#else

static uint8_t* DecodeWebp(FreeFunction* pFreeFunc, const uint8_t* pData, size_t size, int* width, int* height)
{
	*pFreeFunc = NopFree;
	return nullptr;
}

#endif

#ifdef STBI_SUP

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

static uint8_t* DecodeWithStbImage(FreeFunction* pFreeFunc, const uint8_t* pData, size_t size, int* width, int* height)
{
	int x;
	stbi_uc* dataStbi = stbi_load_from_memory(pData, int(size), width, height, &x, 4);
	if (!*width || !*height)
		return nullptr;

	int w = *width, h = *height;
	// byte swap because stbi is annoying
	for (int i = 0; i < w * h; i++)
	{
		stbi_uc* pd = &dataStbi[i * 4];
		stbi_uc tmp = pd[0];
		pd[0] = pd[2];
		pd[2] = tmp;
	}

	*pFreeFunc = stbi_image_free;
	return dataStbi;
}

#else

static uint8_t* DecodeWithStbImage(FreeFunction* pFreeFunc, const uint8_t* pData, size_t size, int* width, int* height)
{
	*pFreeFunc = NopFree;
	return nullptr;
}

#endif

HBITMAP ImageLoader::ConvertToBitmap(const uint8_t* pData, size_t size, int newWidth, int newHeight)
{
	FreeFunction freeFunc = NopFree;
	int width = 0, height = 0;
	uint8_t* pNewData = DecodeWebp(&freeFunc, pData, size, &width, &height);

	if (!pNewData)
	{
		// try using stb_image instead, probably a png/gif/jpg
		pNewData = DecodeWithStbImage(&freeFunc, pData, size, &width, &height);

		if (!pNewData)
			return NULL;
	}

	if (newWidth < 0)
		newWidth = GetProfilePictureSize();
	if (newHeight < 0)
		newHeight = GetProfilePictureSize();

	if (!newWidth)
		newWidth = width;
	if (!newHeight)
		newHeight = height;

	HDC wndHdc = GetDC(g_Hwnd);
	HDC hdc = CreateCompatibleDC(wndHdc);
	HBITMAP hbm = CreateCompatibleBitmap(wndHdc, newWidth, newHeight);
	HGDIOBJ old = SelectObject(hdc, hbm);

	BITMAPINFO bmi;
	memset(&bmi, 0, sizeof bmi);

	BITMAPINFOHEADER& hdr = bmi.bmiHeader;

	hdr.biSizeImage = 0;
	hdr.biWidth = width;
	hdr.biHeight = -height;
	hdr.biPlanes = 1;
	hdr.biBitCount = 32;
	hdr.biCompression = BI_RGB;
	hdr.biSize = sizeof(BITMAPINFOHEADER);
	hdr.biXPelsPerMeter = 0;
	hdr.biYPelsPerMeter = 0;
	hdr.biClrUsed = 0;
	hdr.biClrImportant = 0;

	int x;
	if (newWidth != width || newHeight != height)
	{
		// This is the best stretch mode.
		SetStretchBltMode(hdc, HALFTONE);
		x = StretchDIBits(hdc, 0, 0, newWidth, newHeight, 0, 0, width, height, pNewData, &bmi, DIB_RGB_COLORS, SRCCOPY);
	}
	else
	{
		x = SetDIBits(hdc, hbm, 0, height, pNewData, &bmi, DIB_RGB_COLORS);
	}

	SelectObject(hdc, old);

	freeFunc(pNewData);

	ReleaseDC(g_Hwnd, wndHdc);

	if (x == GDI_ERROR)
	{
		// it failed...
		OutputDebugStringA("ConvertToBitmap failed!");
		DeleteObject(hbm);
		DeleteDC(hdc);
		hbm = NULL;
	}

	static int i = 0;
	DbgPrintW("Loading bitmap.. %d", ++i);

	DeleteDC (hdc);

	return hbm;
}
