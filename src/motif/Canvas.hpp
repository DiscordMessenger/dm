#pragma once

#include <cstdint>
#include <vector>
#include <X11/Xlib.h>

// Colours are 0x00RRGGBB.
typedef uint32_t Rgb;

inline Rgb MakeRgb(int r, int g, int b) { return (Rgb) ((r << 16) | (g << 8) | b); }
inline int RgbR(Rgb c) { return (c >> 16) & 0xff; }
inline int RgbG(Rgb c) { return (c >> 8) & 0xff; }
inline int RgbB(Rgb c) { return c & 0xff; }
Rgb LerpRgb(Rgb a, Rgb b, int num, int den);

// How client-side pixels become pixels of one X visual: any TrueColor
// layout, or a colour cube in a PseudoColor colormap (ordered dithering).
class PixelFormat
{
public:
	bool Init(Display* dpy, Visual* visual, int depth, Colormap cmap);

	Display* GetDisplay() const { return m_dpy; }
	Visual* GetVisual() const { return m_visual; }
	int GetDepth() const { return m_depth; }

	unsigned long PixelOf(Rgb c) const;

	// Converts w x h pixels (stride in pixels) into a new XImage, dithered
	// when the visual needs it.  Free with XDestroyImage.
	XImage* MakeImage(const Rgb* px, int stride, int w, int h, int originX, int originY) const;

private:
	Display* m_dpy = nullptr;
	Visual* m_visual = nullptr;
	int m_depth = 0;
	bool m_trueColor = false;
	int m_shift[3] = { 0, 0, 0 }; // left shift of the 8-bit value's top bit
	int m_bits[3] = { 0, 0, 0 };
	unsigned long m_mask[3] = { 0, 0, 0 };
	// colour cube for PseudoColor
	int m_levels = 0;
	std::vector<unsigned long> m_cube;
};

// A client-side RGB image that the UI draws into, then shows in a window.
class Canvas
{
public:
	void Resize(int w, int h);
	int Width() const { return m_w; }
	int Height() const { return m_h; }
	Rgb* Pixels() { return m_px.data(); }

	// Drawing is limited to the clip rectangle.
	void SetClip(int x, int y, int w, int h);
	void ClearClip();

	void Fill(int x, int y, int w, int h, Rgb c);
	void FillRounded(int x, int y, int w, int h, int radius, Rgb c);
	void Frame(int x, int y, int w, int h, Rgb c);
	void HLine(int x, int y, int w, Rgb c) { Fill(x, y, w, 1, c); }
	void VLine(int x, int y, int h, Rgb c) { Fill(x, y, 1, h, c); }

	// An 8-bit coverage mask (stride bytes per row) drawn in colour c.
	void BlendMask(int x, int y, const uint8_t* mask, int w, int h, int stride, Rgb c);

	// ARGB pixels (alpha in the top byte) blended over the canvas.
	void BlendArgb(int x, int y, const uint32_t* px, int w, int h, int stride);
	// The same inside a circle of the image's size (avatars).
	void BlendArgbCircle(int x, int y, const uint32_t* px, int w, int h, int stride);

	// Shows the rectangle (x, y, w, h) of the canvas at (dx, dy) in a
	// drawable.
	void Present(const PixelFormat& fmt, Drawable d, GC gc, int x, int y, int w, int h, int dx, int dy) const;

private:
	bool ClipRect(int& x, int& y, int& w, int& h) const;

	int m_w = 0, m_h = 0;
	std::vector<Rgb> m_px;
	int m_cx0 = 0, m_cy0 = 0, m_cx1 = 0, m_cy1 = 0;
};
