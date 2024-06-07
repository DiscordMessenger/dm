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

HBITMAP ImageLoader::_ConvertToBitmap(const uint8_t* pData, size_t size, bool& outHasAlphaChannel, uint32_t& topLeftPixel, int newWidth, int newHeight)
{
	outHasAlphaChannel = false;

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

	// pre-multiply alpha
	size_t pixelCount = width * height;
	uint32_t* pixels = (uint32_t*)pNewData;

	topLeftPixel = 0;

	bool hasAlpha = false;
	for (size_t i = 0; i < pixelCount; i++)
	{
		union {
			uint8_t c[4];
			uint32_t x;
		} u;

		u.x = pixels[i];
		if (u.c[3] != 0xFF) {
			hasAlpha = true;
			u.c[0] = uint8_t(int(u.c[0]) * int(u.c[3]) / 255);
			u.c[1] = uint8_t(int(u.c[1]) * int(u.c[3]) / 255);
			u.c[2] = uint8_t(int(u.c[2]) * int(u.c[3]) / 255);
			pixels[i] = u.x;
		}
		else if (!topLeftPixel) {
			topLeftPixel = u.x;
		}
	}

	BITMAPINFO bmi{};
	BITMAPINFOHEADER& hdr = bmi.bmiHeader;
	hdr.biSizeImage = 0;
	hdr.biWidth = width;
	hdr.biHeight = -height;
	hdr.biPlanes = 1;
	hdr.biBitCount = 32;
	hdr.biCompression = BI_RGB;
	hdr.biSize = sizeof(BITMAPINFOHEADER);

	HDC wndHdc = GetDC(g_Hwnd);
	HDC hdc = CreateCompatibleDC(wndHdc);
	if (!hasAlpha)
	{
		HBITMAP hbm = CreateCompatibleBitmap(wndHdc, newWidth, newHeight);
		HGDIOBJ old = SelectObject(hdc, hbm);
		outHasAlphaChannel = false;

		int x = 0;
		if (newWidth != width || newHeight != height) {
			int oldMode = SetStretchBltMode(hdc, HALFTONE);
			x = StretchDIBits(hdc, 0, 0, newWidth, newHeight, 0, 0, width, height, pNewData, &bmi, DIB_RGB_COLORS, SRCCOPY);
			SetStretchBltMode(hdc, oldMode);
		}
		else {
			x = SetDIBits(hdc, hbm, 0, height, pNewData, &bmi, DIB_RGB_COLORS);
		}

		if (x == GDI_ERROR) {
			DbgPrintW("ConvertToBitmap failed to convert opaque image!");
			DeleteObject(hbm);
			hbm = NULL;
		}

		freeFunc(pNewData);
		SelectObject(hdc, old);
		DeleteDC(hdc);
		ReleaseDC(g_Hwnd, wndHdc);
		return hbm;
	}

	// Check if we need to stretch the bitmap.
	if (newWidth == width && newHeight == height)
	{
		// Nope, so we can apply a highly optimized scheme.
	_UnstretchedScheme:
		void* pvBits = NULL;
		HBITMAP hbm = CreateDIBSection(wndHdc, &bmi, DIB_RGB_COLORS, &pvBits, NULL, 0);

		if (!hbm) {
			DbgPrintW("ConvertToBitmap failed to convert transparent unstretched image!");
			hbm = NULL;
		}
		else {
			memcpy(pvBits, pixels, sizeof(uint32_t) * pixelCount);
		}

		outHasAlphaChannel = true;
		freeFunc(pNewData);
		DeleteDC(hdc);
		ReleaseDC(g_Hwnd, wndHdc);
		return hbm;
	}

	// Yeah, need to stretch the bitmap.  Because of that, we need to perform two stretch
	// operations and combine the bits together.
	uint32_t* alphaPixels = new uint32_t[pixelCount];
	for (size_t i = 0; i < pixelCount; i++) {
		uint32_t alpha = (pixels[i]) >> 24;
		alphaPixels[i] = (255 << 24) | (alpha << 16) | (alpha << 8) | alpha;
	}

	// Create the alpha channel bitmap.
	HBITMAP hbm = CreateCompatibleBitmap(wndHdc, newWidth, newHeight);
	HGDIOBJ old = SelectObject(hdc, hbm);
	int oldMode = SetStretchBltMode(hdc, HALFTONE);
	int x = StretchDIBits(hdc, 0, 0, newWidth, newHeight, 0, 0, width, height, alphaPixels, &bmi, DIB_RGB_COLORS, SRCCOPY);
	SetStretchBltMode(hdc, oldMode);
	SelectObject(hdc, old);
	delete[] alphaPixels;

	BITMAPINFO bmi2;
	size_t newPixelCount;
	uint32_t *pvAlphaBits, *pvColorBits;

	if (x == GDI_ERROR) {
		DbgPrintW("ConvertToBitmap failed to convert transparent stretched image!");
		hbm = NULL;
	}
	else {
		ZeroMemory(&bmi2, sizeof bmi2);
		BITMAPINFOHEADER& hdr2 = bmi2.bmiHeader;
		hdr2.biSizeImage = 0;
		hdr2.biWidth = newWidth;
		hdr2.biHeight = -newHeight;
		hdr2.biPlanes = 1;
		hdr2.biBitCount = 32;
		hdr2.biCompression = BI_RGB;
		hdr2.biSize = sizeof(BITMAPINFOHEADER);
		hdr2.biClrUsed = 0;
		hdr2.biClrImportant = 0;
		hdr2.biSizeImage = 0;
		hdr2.biXPelsPerMeter = 0;
		hdr2.biYPelsPerMeter = 0;

		// now dump the bits
		newPixelCount = newWidth * newHeight;

		pvAlphaBits = new uint32_t[newPixelCount];
		x = GetDIBits(hdc, hbm, 0, newHeight, pvAlphaBits, &bmi2, DIB_RGB_COLORS);
		if (x == 0) {
			DbgPrintW("ConvertToBitmap failed to convert transparent stretched image - GetDIBits returned 0");
			delete[] pvAlphaBits;
			DeleteBitmap(hbm);
			hbm = NULL;
			goto _error;
		}

		// Done with the alpha channel stretch.
		DeleteBitmap(hbm);

		// Create the color bitmap.
		hbm = CreateCompatibleBitmap(wndHdc, newWidth, newHeight);
		old = SelectObject(hdc, hbm);

		oldMode = SetStretchBltMode(hdc, HALFTONE);
		x = StretchDIBits(hdc, 0, 0, newWidth, newHeight, 0, 0, width, height, pixels, &bmi, DIB_RGB_COLORS, SRCCOPY);
		SetStretchBltMode(hdc, oldMode);
		if (x == GDI_ERROR) {
			DbgPrintW("ConvertToBitmap failed to convert transparent stretched image - StretchDIBits in alpha returned 0");
			DeleteBitmap(hbm);
			hbm = NULL;
			goto _error;
		}

		SelectObject(hdc, old);

		// Dump that one's bits too.
		ZeroMemory(&bmi2, sizeof bmi2);
		hdr2.biSizeImage = 0;
		hdr2.biWidth = newWidth;
		hdr2.biHeight = -newHeight;
		hdr2.biPlanes = 1;
		hdr2.biBitCount = 32;
		hdr2.biCompression = BI_RGB;
		hdr2.biSize = sizeof(BITMAPINFOHEADER);
		hdr2.biClrUsed = 0;
		hdr2.biClrImportant = 0;
		hdr2.biSizeImage = 0;
		hdr2.biXPelsPerMeter = 0;
		hdr2.biYPelsPerMeter = 0;

		pvColorBits = new uint32_t[newPixelCount];
		x = GetDIBits(hdc, hbm, 0, newHeight, pvColorBits, &bmi2, DIB_RGB_COLORS);
		if (x == 0) {
			DbgPrintW("ConvertToBitmap failed to convert transparent stretched image - GetDIBits in color returned 0... %d", GetLastError());
			delete[] pvColorBits;
			DeleteBitmap(hbm);
			hbm = NULL;
			goto _error;
		}

		// Done with the color channel stretch.
		DeleteBitmap(hbm);

		// Clean up the original image, and perform a replacement:
		freeFunc(pNewData);
		pixels = pvColorBits;
		pNewData = (uint8_t*)pvColorBits;
		freeFunc = &::free;
		width = newWidth;
		height = newHeight;
		pixelCount = width * height;
		bmi.bmiHeader.biWidth = width;
		bmi.bmiHeader.biHeight = -height;

		// Mix the two together:
		for (size_t i = 0; i < newPixelCount; i++) {
			pvColorBits[i] = (pvColorBits[i] & 0xFFFFFF) | ((pvAlphaBits[i] & 0xFF) << 24);
		}

		delete[] pvAlphaBits;

		// Finally, apply the unstretched scheme.
		goto _UnstretchedScheme;
	}

_error:
	// note: you can't actually reach this part with a valid bitmap
	assert(!hbm);

	outHasAlphaChannel = false;
	freeFunc(pNewData);
	DeleteDC(hdc);
	ReleaseDC(g_Hwnd, wndHdc);
	return hbm;
}

HBITMAP ImageLoader::ConvertToBitmap(const uint8_t* pData, size_t size, bool& outHasAlphaChannel, int newWidth, int newHeight)
{
	uint32_t crap;
	return _ConvertToBitmap(pData, size, outHasAlphaChannel, crap, newWidth, newHeight);
}

static void ConvertToPNGWriteFunc(void* context, void* data, int size)
{
	auto vec = reinterpret_cast<std::vector<uint8_t>*> (context);

	size_t old_size = vec->size();
	vec->resize(old_size + size);
	memcpy(vec->data() + old_size, data, size);
}

bool ImageLoader::ConvertToPNG(std::vector<uint8_t>* outData, void* pBits, int width, int height, int widthBytes, int bpp, bool forceOpaque, bool flipVerticallyWhenSaving)
{
	uint32_t* interm_data = new uint32_t[width * height];
	uint8_t* bits_b = (uint8_t*) pBits;
	uint32_t auxBits = 0;
	if (forceOpaque)
		auxBits = 255U << 24;

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
					*(write_ptr++) = u.x | auxBits;
				}
			}
			break;
		}

		case 24: {
			uint32_t* write_ptr = interm_data;
			for (int y = 0; y < height; y++) {
				uint8_t* read_ptr = (uint8_t*)(bits_b + widthBytes * y);
				for (int x = 0; x < width; x++) {
					uint32_t rd = 255U << 24; // full alpha
					rd |= read_ptr[2];
					rd |= read_ptr[1] << 8;
					rd |= read_ptr[0] << 16;
					*(write_ptr++) = rd;
					read_ptr += 3;
				}
			}
			break;
		}

		default:
			DbgPrintW("Error, don't know how to handle this BPP");
			delete[] interm_data;
			return false;
	}

	stbi_flip_vertically_on_write(flipVerticallyWhenSaving);

	if (!stbi_write_png_to_func(ConvertToPNGWriteFunc, (void*) outData, width, height, 4, (const void*) interm_data, width * sizeof(uint32_t))) {
		delete[] interm_data;
		return false;
	}

	delete[] interm_data;
	return true;
}

HBITMAP ImageLoader::LoadFromFile(const char* pFileName, bool& hasAlphaOut, uint32_t* outTopLeftPixel)
{
	FILE* f = fopen(pFileName, "rb");
	if (!f)
		return NULL;

	fseek(f, 0, SEEK_END);
	int sz = int(ftell(f));
	fseek(f, 0, SEEK_SET);

	uint8_t* pData = new uint8_t[sz];
	fread(pData, 1, sz, f);

	fclose(f);

	bool hasAlpha = false;
	uint32_t firstPixel = 0;
	HBITMAP hbmp = ImageLoader::_ConvertToBitmap(pData, size_t(sz), hasAlpha, firstPixel, 0, 0);
	if (!hbmp)
		return NULL;

	hasAlphaOut = hasAlpha;

	if (outTopLeftPixel)
		*outTopLeftPixel = firstPixel;

	return hbmp;
}
