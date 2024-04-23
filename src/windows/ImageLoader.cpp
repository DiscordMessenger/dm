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
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image.h>
#include <stb/stb_image_write.h>

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

static void ConvertToPNGWriteFunc(void* context, void* data, int size)
{
	auto vec = reinterpret_cast<std::vector<uint8_t>*> (context);

	size_t old_size = vec->size();
	vec->resize(old_size + size);
	memcpy(vec->data() + old_size, data, size);
}

bool ImageLoader::ConvertToPNG(std::vector<uint8_t>* outData, void* pBits, int width, int height, int widthBytes, int bpp)
{
	uint32_t* interm_data = new uint32_t[width * height];
	uint8_t* bits_b = (uint8_t*) pBits;

	switch (bpp) {
		case 32: {
			uint32_t* write_ptr = interm_data;
			for (int y = 0; y < height; y++) {
				uint32_t* read_ptr = (uint32_t*)(bits_b + widthBytes * y);

				for (int x = 0; x < width; x++) {
					union {
						uint8_t b[4];
						uint32_t x;
					} u;
					u.x = *(read_ptr++);
					// ugh
					uint8_t tmp = u.b[0];
					u.b[0] = u.b[2];
					u.b[2] = tmp;
					*(write_ptr++) = u.x;
				}
			}
			break;
		}

		case 24: {
			uint32_t* write_ptr = interm_data;
			for (int y = 0; y < height; y++) {
				uint8_t* read_ptr = (uint8_t*)(bits_b + widthBytes * y);
				for (int x = 0; x < width; x++) {
					uint32_t rd = 255 << 16; // full alpha
					rd |= read_ptr[2];
					rd |= read_ptr[1] << 8;
					rd |= read_ptr[0] << 16;
					*(write_ptr++) = rd;
				}
			}
			break;
		}

		default:
			DbgPrintW("Error, don't know how to handle this BPP");
			delete[] interm_data;
			return false;
	}

	if (!stbi_write_png_to_func(ConvertToPNGWriteFunc, (void*) outData, width, height, 4, (const void*) interm_data, width * sizeof(uint32_t))) {
		delete[] interm_data;
		return false;
	}

	delete[] interm_data;
	return true;
}
