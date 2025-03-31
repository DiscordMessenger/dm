#include "Main.hpp"
#include "ImageLoader.hpp"

typedef void(*FreeFunction)(void*);
struct ImageFrame
{
	int FrameTime;
	uint8_t* Data;
	FreeFunction DataFree;

	ImageFrame(uint8_t* data, FreeFunction freeFunc, int frameTime = 0) :
		FrameTime(frameTime),
		Data(data),
		DataFree(freeFunc)
	{}

	ImageFrame(const ImageFrame& f) = delete;

	ImageFrame(ImageFrame&& f) noexcept {
		Data = f.Data;
		DataFree = f.DataFree;
		FrameTime = f.FrameTime;
		f.Data = nullptr;
		f.DataFree = nullptr;
	}

	~ImageFrame()
	{
		if (Data) {
			assert(DataFree);
			DataFree(Data);
		}

		Data = nullptr;
		DataFree = nullptr;
	}
};

struct Image
{
	std::vector<ImageFrame> Frames;
	int Width = 0;
	int Height = 0;
	
	Image() {}
	Image(ImageFrame&& imf, int width, int height) :
		Width(width),
		Height(height)
	{
		Frames.push_back(std::move(imf));
	}

	bool IsValid() const {
		return !Frames.empty();
	}
};

void NopFree(void* unused) {}

#ifdef WEBP_SUP

#include <webp/decode.h>

static Image DecodeWebp(const uint8_t* pData, size_t size)
{
	int width = 0, height = 0;
	uint8_t* Data = WebPDecodeBGRA(pData, size, &width, &height);
	if (!Data)
		return Image();

	return Image(ImageFrame(Data, WebPFree), width, height);
}

#else

static Image DecodeWebp(const uint8_t* pData, size_t size)
{
	return Image();
}

#endif

#ifdef STBI_SUP

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image.h>
#include <stb/stb_image_write.h>

static Image DecodeWithStbImage(const uint8_t* pData, size_t size)
{
	if (size > INT_MAX)
		return Image();

	// note: doing some work with raw STBI contexts here. don't mind it.
	/*
	stbi__context s;
	stbi__start_mem(&s, pData, (int) size); // why do you have to be an INT here...

	if (stbi__gif_test(&s)) {

	}
	*/

	int x = 0, w = 0, h = 0;
	stbi_uc* dataStbi = stbi_load_from_memory(pData, int(size), &w, &h, &x, 4);
	if (!dataStbi)
		return Image();

	// byte swap because stbi is annoying
	for (int i = 0; i < w * h; i++)
	{
		stbi_uc* pd = &dataStbi[i * 4];
		stbi_uc tmp = pd[0];
		pd[0] = pd[2];
		pd[2] = tmp;
	}

	return Image(ImageFrame(dataStbi, stbi_image_free, 0), w, h);
}

#else

static Image DecodeWithStbImage(const uint8_t* pData, size_t size)
{
	return Image();
}

#endif

HImage* ImageLoader::ConvertToBitmap(const uint8_t* pData, size_t size, bool& outHasAlphaChannel, bool loadAllFrames, int newWidth, int newHeight)
{
	outHasAlphaChannel = false;

	Image img = DecodeWebp(pData, size);

	if (!img.IsValid())
	{
		// try using stb_image instead, probably a png/gif/jpg
		img = DecodeWithStbImage(pData, size);

		if (!img.IsValid())
			return nullptr;
	}

	if (newWidth < 0)
		newWidth = GetProfilePictureSize();
	if (newHeight < 0)
		newHeight = GetProfilePictureSize();

	if (!newWidth)
		newWidth = img.Width;
	if (!newHeight)
		newHeight = img.Height;

	size_t loadedImageCount = loadAllFrames ? img.Frames.size() : 1;

	HImage* himg = new HImage;
	himg->Frames.resize(loadedImageCount);
	himg->Width = newWidth;
	himg->Height = newHeight;

	BITMAPINFO bmi{};
	BITMAPINFOHEADER& hdr = bmi.bmiHeader;
	hdr.biSizeImage = 0;
	hdr.biWidth = img.Width;
	hdr.biHeight = -img.Height;
	hdr.biPlanes = 1;
	hdr.biBitCount = 32;
	hdr.biCompression = BI_RGB;
	hdr.biSize = sizeof(BITMAPINFOHEADER);

	HDC wndHdc = GetDC(g_Hwnd);
	HDC hdc = CreateCompatibleDC(wndHdc);

	for (size_t i = 0; i < loadedImageCount; i++)
	{
		// NOTE: Have to define these extra early, because gcc is picky about initialization
		// even though I don't goddamn use these after `DoneHBM'. But these are used later
		HGDIOBJ old;
		int x, oldMode;
		BITMAPINFO bmi2;
		BITMAPINFOHEADER& hdr2 = bmi2.bmiHeader;
		uint32_t* alphaPixels;
		int width, height;
		bool needDeletePixels;

		auto& frm = img.Frames[i];

		// pre-multiply alpha
		size_t pixelCount = img.Width * img.Height;
		uint32_t* pixels = (uint32_t*)frm.Data;

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
		}

		HBITMAP hbm = NULL;

		if (!hasAlpha)
		{
			hbm = CreateCompatibleBitmap(wndHdc, newWidth, newHeight);
			HGDIOBJ old = SelectObject(hdc, hbm);

			int x = 0;
			if (newWidth != img.Width || newHeight != img.Height) {
				int oldMode = SetStretchBltMode(hdc, ri::GetHalfToneStretchMode());
				x = StretchDIBits(hdc, 0, 0, newWidth, newHeight, 0, 0, img.Width, img.Height, frm.Data, &bmi, DIB_RGB_COLORS, SRCCOPY);
				SetStretchBltMode(hdc, oldMode);
			}
			else {
				x = SetDIBits(hdc, hbm, 0, img.Height, frm.Data, &bmi, DIB_RGB_COLORS);
			}

			if (x == GDI_ERROR) {
				DbgPrintW("ConvertToBitmap failed to convert opaque image!");
				DeleteObject(hbm);
				hbm = NULL;
			}

			SelectObject(hdc, old);
			goto DoneHBM;
		}

		needDeletePixels = false;
		width = img.Width;
		height = img.Height;

		// Check if we need to stretch the bitmap.
		if (newWidth == width && newHeight == height)
		{
			// Nope, so we can apply a highly optimized scheme.
		UnstretchedScheme:
			void* pvBits = NULL;
			hbm = ri::CreateDIBSection(wndHdc, &bmi, DIB_RGB_COLORS, &pvBits, NULL, 0);

			if (!hbm) {
				DbgPrintW("ConvertToBitmap failed to convert transparent unstretched image!");
				hbm = NULL;
			}
			else {
				memcpy(pvBits, pixels, sizeof(uint32_t) * pixelCount);
				ri::CommitDIBSection(hdc, hbm, &bmi, pvBits);
			}

			if (needDeletePixels)
				delete[] pixels;

			outHasAlphaChannel = true;
			goto DoneHBM;
		}

		// Yeah, need to stretch the bitmap.  Because of that, we need to perform two stretch
		// operations and combine the bits together.
		alphaPixels = new uint32_t[pixelCount];
		for (size_t i = 0; i < pixelCount; i++) {
			uint32_t alpha = (pixels[i]) >> 24;
			alphaPixels[i] = (255 << 24) | (alpha << 16) | (alpha << 8) | alpha;
		}

		// Create the alpha channel bitmap.
		hbm = CreateCompatibleBitmap(wndHdc, newWidth, newHeight);
		old = SelectObject(hdc, hbm);
		oldMode = SetStretchBltMode(hdc, ri::GetHalfToneStretchMode());
		x = StretchDIBits(hdc, 0, 0, newWidth, newHeight, 0, 0, width, height, alphaPixels, &bmi, DIB_RGB_COLORS, SRCCOPY);
		SetStretchBltMode(hdc, oldMode);
		SelectObject(hdc, old);
		delete[] alphaPixels;
		
		size_t newPixelCount;
		uint32_t *pvAlphaBits, *pvColorBits;

		if (x == GDI_ERROR) {
			DbgPrintW("ConvertToBitmap failed to convert transparent stretched image!");
			hbm = NULL;
			goto DoneHBM;
		}

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

		// now dump the bits
		newPixelCount = newWidth * newHeight;

		pvAlphaBits = new uint32_t[newPixelCount];
		x = GetDIBits(hdc, hbm, 0, newHeight, pvAlphaBits, &bmi2, DIB_RGB_COLORS);
		if (x == 0) {
			DbgPrintW("ConvertToBitmap failed to convert transparent stretched image - GetDIBits returned 0");
			delete[] pvAlphaBits;
			DeleteBitmap(hbm);
			hbm = NULL;
			goto DoneHBM;
		}

		// Done with the alpha channel stretch.
		DeleteBitmap(hbm);

		// Create the color bitmap.
		hbm = CreateCompatibleBitmap(wndHdc, newWidth, newHeight);
		old = SelectObject(hdc, hbm);

		oldMode = SetStretchBltMode(hdc, ri::GetHalfToneStretchMode());
		x = StretchDIBits(hdc, 0, 0, newWidth, newHeight, 0, 0, width, height, pixels, &bmi, DIB_RGB_COLORS, SRCCOPY);
		SetStretchBltMode(hdc, oldMode);
		if (x == GDI_ERROR) {
			DbgPrintW("ConvertToBitmap failed to convert transparent stretched image - StretchDIBits in alpha returned 0");
			DeleteBitmap(hbm);
			hbm = NULL;
			goto DoneHBM;
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
			goto DoneHBM;
		}

		// Done with the color channel stretch.
		DeleteBitmap(hbm);

		// Clean up the original image, and perform a replacement:
		pixels = pvColorBits;
		needDeletePixels = true;
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
		goto UnstretchedScheme;

	DoneHBM:
		himg->Frames[i].Bitmap = hbm;
		himg->Frames[i].FrameTime = frm.FrameTime;
	}

	DeleteDC(hdc);
	ReleaseDC(g_Hwnd, wndHdc);
	return himg;
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

HBITMAP ImageLoader::LoadFromFile(const char* pFileName, bool& hasAlphaOut)
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
	HImage* himg = ImageLoader::ConvertToBitmap(pData, size_t(sz), hasAlpha, false, 0, 0);
	assert(himg->Frames.size() == 1);

	if (!himg->Frames[0].Bitmap) {
		delete himg;
		return NULL;
	}

	// steal the resource for ourselves (don't let the HImage destructor delete it)
	HBITMAP hbm = himg->Frames[0].Bitmap;
	himg->Frames[0].Bitmap = NULL;
	delete himg;

	hasAlphaOut = hasAlpha;
	return hbm;
}

HImageFrame::~HImageFrame()
{
	if (Bitmap)
		DeleteBitmap(Bitmap);

	Bitmap = NULL;
}

extern HImage g_defaultImage;

HImage::HImage()
{
	DbgPrintW("HImage %p constructed", this);
}

HImage::~HImage()
{
	// note: this assertion will not work when exiting !
	//assert(this != &g_defaultImage);
	DbgPrintW("HImage %p destructed", this);
}

HBITMAP HImage::WithdrawImage(size_t frameNo, int* frameLengthOut)
{
	if (frameNo >= Frames.size())
		return NULL;

	HBITMAP hbm = Frames[frameNo].Bitmap;
	if (frameLengthOut)
		*frameLengthOut = Frames[frameNo].FrameTime;

	Frames[frameNo].Bitmap = NULL;
	return hbm;
}

HBITMAP HImage::GetImage(size_t frameNo, int* frameLengthOut) const
{
	if (frameNo >= Frames.size())
		return NULL;

	if (frameLengthOut)
		*frameLengthOut = Frames[frameNo].FrameTime;

	return Frames[frameNo].Bitmap;
}

HBITMAP HImage::GetFirstFrame() const
{
	if (Frames.empty()) return NULL;
	return Frames[0].Bitmap;
}
