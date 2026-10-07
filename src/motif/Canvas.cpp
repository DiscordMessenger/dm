#include "Canvas.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <X11/Xutil.h>

Rgb LerpRgb(Rgb a, Rgb b, int num, int den)
{
	int r = RgbR(a) + (RgbR(b) - RgbR(a)) * num / den;
	int g = RgbG(a) + (RgbG(b) - RgbG(a)) * num / den;
	int bl = RgbB(a) + (RgbB(b) - RgbB(a)) * num / den;
	return MakeRgb(r, g, bl);
}

// 4x4 ordered dither thresholds, 0..15
static const int g_bayer[4][4] = {
	{  0,  8,  2, 10 },
	{ 12,  4, 14,  6 },
	{  3, 11,  1,  9 },
	{ 15,  7, 13,  5 },
};

static int HighBit(unsigned long m)
{
	int b = -1;
	while (m) { b++; m >>= 1; }
	return b;
}

static int BitCount(unsigned long m)
{
	int n = 0;
	while (m) { n += m & 1; m >>= 1; }
	return n;
}

bool PixelFormat::Init(Display* dpy, Visual* visual, int depth, Colormap cmap)
{
	m_dpy = dpy;
	m_visual = visual;
	m_depth = depth;
	m_trueColor = visual->c_class == TrueColor || visual->c_class == DirectColor;

	if (m_trueColor)
	{
		unsigned long masks[3] = { visual->red_mask, visual->green_mask, visual->blue_mask };
		for (int i = 0; i < 3; i++) {
			m_mask[i] = masks[i];
			m_bits[i] = BitCount(masks[i]);
			m_shift[i] = HighBit(masks[i]); // position of the top bit
		}
		return true;
	}

	// PseudoColor / StaticColor / GrayScale: a colour cube, as big as the
	// colormap lets us have.
	for (int levels = 6; levels >= 2; levels--)
	{
		std::vector<unsigned long> cube;
		bool ok = true;
		for (int r = 0; r < levels && ok; r++)
		for (int g = 0; g < levels && ok; g++)
		for (int b = 0; b < levels && ok; b++)
		{
			XColor xc;
			xc.red   = (unsigned short) (r * 65535 / (levels - 1));
			xc.green = (unsigned short) (g * 65535 / (levels - 1));
			xc.blue  = (unsigned short) (b * 65535 / (levels - 1));
			xc.flags = DoRed | DoGreen | DoBlue;
			if (!XAllocColor(dpy, cmap, &xc))
				ok = false;
			else
				cube.push_back(xc.pixel);
		}
		if (ok) {
			m_levels = levels;
			m_cube.swap(cube);
			return true;
		}
		if (!cube.empty())
			XFreeColors(dpy, cmap, cube.data(), (int) cube.size(), 0);
	}

	// Nothing could be allocated: black and white.
	m_levels = 2;
	m_cube.assign(8, BlackPixel(dpy, DefaultScreen(dpy)));
	m_cube[7] = WhitePixel(dpy, DefaultScreen(dpy));
	return false;
}

unsigned long PixelFormat::PixelOf(Rgb c) const
{
	int v[3] = { RgbR(c), RgbG(c), RgbB(c) };
	if (m_trueColor) {
		unsigned long p = 0;
		for (int i = 0; i < 3; i++) {
			int s = m_shift[i] - 7;
			unsigned long x = s >= 0 ? (unsigned long) v[i] << s : (unsigned long) v[i] >> -s;
			p |= x & m_mask[i];
		}
		return p;
	}
	int n = m_levels;
	int r = (v[0] * (n - 1) + 127) / 255, g = (v[1] * (n - 1) + 127) / 255, b = (v[2] * (n - 1) + 127) / 255;
	return m_cube[(r * n + g) * n + b];
}

XImage* PixelFormat::MakeImage(const Rgb* px, int stride, int w, int h, int originX, int originY) const
{
	XImage* img = XCreateImage(m_dpy, m_visual, m_depth, ZPixmap, 0, NULL, w, h, 32, 0);
	if (!img)
		return NULL;
	img->data = (char*) malloc((size_t) img->bytes_per_line * h);
	if (!img->data) {
		XDestroyImage(img);
		return NULL;
	}

	bool hostMSB;
	{
		uint16_t probe = 1;
		hostMSB = *(uint8_t*) &probe == 0;
	}
	bool nativeOrder = (img->byte_order == MSBFirst) == hostMSB;

	if (m_trueColor && m_bits[0] >= 8 && m_bits[1] >= 8 && m_bits[2] >= 8 &&
		img->bits_per_pixel == 32 && nativeOrder)
	{
		// 24-bit colour: no dithering, one store per pixel.
		int sr = m_shift[0] - 7, sg = m_shift[1] - 7, sb = m_shift[2] - 7;
		for (int y = 0; y < h; y++) {
			const Rgb* s = px + (size_t) y * stride;
			uint32_t* d = (uint32_t*) (img->data + (size_t) y * img->bytes_per_line);
			for (int x = 0; x < w; x++) {
				Rgb c = s[x];
				uint32_t r = RgbR(c), g = RgbG(c), b = RgbB(c);
				d[x] = (sr >= 0 ? r << sr : r >> -sr) | (sg >= 0 ? g << sg : g >> -sg) | (sb >= 0 ? b << sb : b >> -sb);
			}
		}
		return img;
	}

	for (int y = 0; y < h; y++)
	{
		const Rgb* s = px + (size_t) y * stride;
		for (int x = 0; x < w; x++)
		{
			Rgb c = s[x];
			int t = g_bayer[(y + originY) & 3][(x + originX) & 3]; // 0..15
			int v[3] = { RgbR(c), RgbG(c), RgbB(c) };
			unsigned long p = 0;

			if (m_trueColor) {
				for (int i = 0; i < 3; i++) {
					int bits = m_bits[i];
					int q = v[i];
					if (bits < 8) {
						// ordered dither between the two nearest levels
						int top = (1 << bits) - 1;
						int f = q * top * 16 / 255;
						q = f / 16 + ((f & 15) > t ? 1 : 0);
						if (q > top) q = top;
						p |= ((unsigned long) q << (m_shift[i] - bits + 1)) & m_mask[i];
					}
					else {
						int sft = m_shift[i] - 7;
						p |= (sft >= 0 ? (unsigned long) q << sft : (unsigned long) q >> -sft) & m_mask[i];
					}
				}
			}
			else {
				int n = m_levels;
				int idx[3];
				for (int i = 0; i < 3; i++) {
					// scale to 0..(n-1)*16, then threshold
					int f = v[i] * (n - 1) * 16 / 255;
					int lo = f / 16;
					idx[i] = lo + ((f & 15) > t ? 1 : 0);
					if (idx[i] > n - 1) idx[i] = n - 1;
				}
				p = m_cube[(idx[0] * n + idx[1]) * n + idx[2]];
			}
			XPutPixel(img, x, y, p);
		}
	}
	return img;
}

void Canvas::Resize(int w, int h)
{
	if (w < 1) w = 1;
	if (h < 1) h = 1;
	m_w = w;
	m_h = h;
	m_px.assign((size_t) w * h, 0xffffff);
	ClearClip();
}

void Canvas::SetClip(int x, int y, int w, int h)
{
	m_cx0 = std::max(0, x);
	m_cy0 = std::max(0, y);
	m_cx1 = std::min(m_w, x + w);
	m_cy1 = std::min(m_h, y + h);
}

void Canvas::ClearClip()
{
	m_cx0 = m_cy0 = 0;
	m_cx1 = m_w;
	m_cy1 = m_h;
}

bool Canvas::ClipRect(int& x, int& y, int& w, int& h) const
{
	int x1 = std::min(x + w, m_cx1), y1 = std::min(y + h, m_cy1);
	x = std::max(x, m_cx0);
	y = std::max(y, m_cy0);
	w = x1 - x;
	h = y1 - y;
	return w > 0 && h > 0;
}

void Canvas::Fill(int x, int y, int w, int h, Rgb c)
{
	if (!ClipRect(x, y, w, h))
		return;
	for (int j = 0; j < h; j++) {
		Rgb* d = &m_px[(size_t) (y + j) * m_w + x];
		std::fill(d, d + w, c);
	}
}

void Canvas::FillRounded(int x, int y, int w, int h, int radius, Rgb c)
{
	if (radius * 2 > h) radius = h / 2;
	if (radius * 2 > w) radius = w / 2;
	for (int j = 0; j < h; j++)
	{
		int inset = 0;
		int dy = j < radius ? radius - j - 1 : (j >= h - radius ? j - (h - radius) : -1);
		if (dy >= 0) {
			// how far the circle's edge is from the corner at this row
			int r2 = radius * radius, k = 0;
			while (k < radius && (radius - k - 1) * (radius - k - 1) + dy * dy >= r2)
				k++;
			inset = k;
		}
		Fill(x + inset, y + j, w - 2 * inset, 1, c);
	}
}

void Canvas::Frame(int x, int y, int w, int h, Rgb c)
{
	Fill(x, y, w, 1, c);
	Fill(x, y + h - 1, w, 1, c);
	Fill(x, y, 1, h, c);
	Fill(x + w - 1, y, 1, h, c);
}

static inline Rgb Blend(Rgb dst, Rgb src, int a)
{
	if (a >= 255) return src;
	if (a <= 0) return dst;
	int ia = 255 - a;
	int r = (RgbR(src) * a + RgbR(dst) * ia + 127) / 255;
	int g = (RgbG(src) * a + RgbG(dst) * ia + 127) / 255;
	int b = (RgbB(src) * a + RgbB(dst) * ia + 127) / 255;
	return MakeRgb(r, g, b);
}

void Canvas::BlendMask(int x, int y, const uint8_t* mask, int w, int h, int stride, Rgb c)
{
	int cx = x, cy = y, cw = w, ch = h;
	if (!ClipRect(cx, cy, cw, ch))
		return;
	for (int j = 0; j < ch; j++) {
		const uint8_t* m = mask + (size_t) (cy - y + j) * stride + (cx - x);
		Rgb* d = &m_px[(size_t) (cy + j) * m_w + cx];
		for (int i = 0; i < cw; i++)
			if (m[i])
				d[i] = Blend(d[i], c, m[i]);
	}
}

void Canvas::BlendArgb(int x, int y, const uint32_t* px, int w, int h, int stride)
{
	int cx = x, cy = y, cw = w, ch = h;
	if (!ClipRect(cx, cy, cw, ch))
		return;
	for (int j = 0; j < ch; j++) {
		const uint32_t* s = px + (size_t) (cy - y + j) * stride + (cx - x);
		Rgb* d = &m_px[(size_t) (cy + j) * m_w + cx];
		for (int i = 0; i < cw; i++)
			d[i] = Blend(d[i], s[i] & 0xffffff, (int) (s[i] >> 24));
	}
}

void Canvas::BlendArgbCircle(int x, int y, const uint32_t* px, int w, int h, int stride)
{
	int cx = x, cy = y, cw = w, ch = h;
	if (!ClipRect(cx, cy, cw, ch))
		return;
	// coverage of a circle inscribed in w x h, 4x4 supersampled at the edge
	float rx = w / 2.0f, ry = h / 2.0f;
	for (int j = 0; j < ch; j++) {
		int sy = cy - y + j;
		const uint32_t* s = px + (size_t) sy * stride + (cx - x);
		Rgb* d = &m_px[(size_t) (cy + j) * m_w + cx];
		for (int i = 0; i < cw; i++) {
			int sx = cx - x + i;
			int inside = 0;
			for (int sj = 0; sj < 4; sj++)
			for (int si = 0; si < 4; si++) {
				float fx = (sx + (si + 0.5f) / 4 - rx) / rx;
				float fy = (sy + (sj + 0.5f) / 4 - ry) / ry;
				inside += fx * fx + fy * fy <= 1.0f;
			}
			int a = (int) (s[i] >> 24) * inside / 16;
			d[i] = Blend(d[i], s[i] & 0xffffff, a);
		}
	}
}

void Canvas::Present(const PixelFormat& fmt, Drawable dr, GC gc, int x, int y, int w, int h, int dx, int dy) const
{
	if (x < 0) { w += x; dx -= x; x = 0; }
	if (y < 0) { h += y; dy -= y; y = 0; }
	if (x + w > m_w) w = m_w - x;
	if (y + h > m_h) h = m_h - y;
	if (w <= 0 || h <= 0)
		return;

	// In bands, so a full-window update never needs one huge image.
	const int band = 64;
	for (int by = 0; by < h; by += band)
	{
		int bh = std::min(band, h - by);
		XImage* img = fmt.MakeImage(&m_px[(size_t) (y + by) * m_w + x], m_w, w, bh, dx, dy + by);
		if (!img)
			return;
		XPutImage(fmt.GetDisplay(), dr, gc, img, 0, 0, dx, dy + by, w, bh);
		XDestroyImage(img);
	}
}
