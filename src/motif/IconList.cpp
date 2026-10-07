#include "IconList.hpp"

#include <algorithm>

#include <X11/keysym.h>
#include <Xm/DrawingA.h>
#include <Xm/Form.h>
#include <Xm/ScrollBar.h>

#include "Fonts.hpp"
#include "Theme.hpp"

static const int PAD = 6;

static Rgb DiscColor(Snowflake seed)
{
	static const Rgb colors[] = { 0x5865f2, 0x3ba55c, 0xfaa61a, 0xed4245, 0xeb459e, 0x747f8d, 0x2d8f9e, 0x9b59b6 };
	return colors[(seed >> 22) % (sizeof colors / sizeof colors[0])];
}

// Up to two initials of a name, as Discord shows guilds without icons.
static std::string Initials(const std::string& s)
{
	std::string out;
	bool start = true;
	const char* p = s.c_str();
	const char* end = p + s.size();
	while (p < end && out.size() < 8)
	{
		const char* c0 = p;
		unsigned cp = DecodeUtf8(p, end);
		bool space = cp == ' ' || cp == '-' || cp == '_';
		if (start && !space) {
			out.append(c0, p - c0);
			if (out.size() >= 2 && (unsigned char) out[0] < 0x80)
				break;
		}
		start = space;
	}
	return out;
}

IconList::IconList(Widget parent, const char* name, const PixelFormat& fmt, int iconSize, bool darker) :
	m_fmt(fmt), m_iconSize(iconSize), m_darker(darker)
{
	m_form = XtVaCreateWidget(name, xmFormWidgetClass, parent, NULL);
	m_scroll = XtVaCreateManagedWidget("scroll", xmScrollBarWidgetClass, m_form,
		XmNorientation, XmVERTICAL,
		XmNtopAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		XmNminimum, 0,
		XmNmaximum, 1,
		XmNsliderSize, 1,
		NULL);
	m_area = XtVaCreateManagedWidget("area", xmDrawingAreaWidgetClass, m_form,
		XmNtopAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNleftAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_WIDGET,
		XmNrightWidget, m_scroll,
		XmNwidth, 160,
		XmNheight, 300,
		XmNresizePolicy, XmRESIZE_NONE,
		XmNtraversalOn, True,
		NULL);
	XtAddCallback(m_area, XmNexposeCallback, ExposeCB, this);
	XtAddCallback(m_area, XmNresizeCallback, ResizeCB, this);
	XtAddCallback(m_area, XmNinputCallback, InputCB, this);
	XtAddCallback(m_scroll, XmNvalueChangedCallback, ScrollCB, this);
	XtAddCallback(m_scroll, XmNdragCallback, ScrollCB, this);
	XtAddEventHandler(m_area, FocusChangeMask, False, FocusEH, this);
	XtManageChild(m_form);
}

int IconList::RowHeight(const IconRow& r) const
{
	int px = GetTextSize() - 1;
	switch (r.type) {
		case IconRow::SPACE:  return 8;
		case IconRow::HEADER: return Fonts::LineHeight(FS_BOLD, px - 2) + 14;
		default:              return std::max(m_iconSize + 8, Fonts::LineHeight(FS_REGULAR, px) + 10);
	}
}

void IconList::Layout()
{
	m_tops.resize(m_rows.size());
	int y = 4;
	for (size_t i = 0; i < m_rows.size(); i++) {
		m_tops[i] = y;
		y += RowHeight(m_rows[i]);
	}
	m_contentHeight = y + 4;
	UpdateScrollbar();
}

void IconList::SetRows(const std::vector<IconRow>& rows, Snowflake selected)
{
	Snowflake cursorId = m_cursor >= 0 && m_cursor < (int) m_rows.size() ? m_rows[m_cursor].id : 0;
	m_rows = rows;
	m_selected = m_cursor = -1;
	for (size_t i = 0; i < m_rows.size(); i++) {
		if (m_rows[i].type == IconRow::ITEM && m_rows[i].selectable && m_rows[i].id == selected && m_selected < 0)
			m_selected = (int) i;
		if (cursorId && m_rows[i].id == cursorId && m_cursor < 0)
			m_cursor = (int) i;
	}
	if (m_cursor < 0)
		m_cursor = m_selected;
	Layout();
	SetScroll(m_scrollY);
	if (m_selected >= 0)
		EnsureVisible(m_selected);
}

void IconList::UpdateScrollbar()
{
	int maxv = std::max(m_contentHeight, m_viewH);
	XtVaSetValues(m_scroll,
		XmNmaximum, maxv,
		XmNsliderSize, std::max(1, std::min(m_viewH, maxv)),
		XmNvalue, std::min(std::max(0, m_scrollY), maxv - std::min(m_viewH, maxv)),
		XmNincrement, 30,
		XmNpageIncrement, std::max(30, m_viewH - 30),
		NULL);
}

void IconList::SetScroll(int y)
{
	m_scrollY = std::min(std::max(0, y), std::max(0, m_contentHeight - m_viewH));
	UpdateScrollbar();
	Repaint();
}

void IconList::EnsureVisible(int row)
{
	if (row < 0 || row >= (int) m_rows.size())
		return;
	int top = m_tops[row], bottom = top + RowHeight(m_rows[row]);
	if (top < m_scrollY)
		SetScroll(top - 4);
	else if (bottom > m_scrollY + m_viewH)
		SetScroll(bottom - m_viewH + 4);
}

int IconList::RowAt(int y) const
{
	int cy = y + m_scrollY;
	for (size_t i = 0; i < m_rows.size(); i++)
		if (cy >= m_tops[i] && cy < m_tops[i] + RowHeight(m_rows[i]))
			return (int) i;
	return -1;
}

void IconList::MoveCursor(int delta)
{
	if (m_rows.empty())
		return;
	int i = m_cursor < 0 ? (delta > 0 ? -1 : (int) m_rows.size()) : m_cursor;
	int step = delta > 0 ? 1 : -1;
	int remaining = std::abs(delta);
	int last = m_cursor;
	while (remaining > 0) {
		i += step;
		if (i < 0 || i >= (int) m_rows.size())
			break;
		if (m_rows[i].type == IconRow::ITEM && m_rows[i].selectable) {
			last = i;
			remaining--;
		}
	}
	if (last >= 0) {
		m_cursor = last;
		EnsureVisible(m_cursor);
		Repaint();
	}
}

void IconList::Activate(int row)
{
	if (row < 0 || row >= (int) m_rows.size())
		return;
	const IconRow& r = m_rows[row];
	if (r.type != IconRow::ITEM || !r.selectable)
		return;
	m_cursor = row;
	Repaint();
	if (m_onSelect)
		m_onSelect(r.id); // may replace the rows
}

void IconList::ExposeCB(Widget, XtPointer client, XtPointer call)
{
	XmDrawingAreaCallbackStruct* cbs = (XmDrawingAreaCallbackStruct*) call;
	if (cbs && cbs->event && cbs->event->type == Expose && cbs->event->xexpose.count > 0)
		return;
	((IconList*) client)->Repaint();
}

void IconList::ResizeCB(Widget w, XtPointer client, XtPointer)
{
	IconList* self = (IconList*) client;
	Dimension width = 0, height = 0;
	XtVaGetValues(w, XmNwidth, &width, XmNheight, &height, NULL);
	self->m_viewW = width;
	self->m_viewH = height;
	self->SetScroll(self->m_scrollY);
}

void IconList::ScrollCB(Widget, XtPointer client, XtPointer call)
{
	IconList* self = (IconList*) client;
	self->m_scrollY = ((XmScrollBarCallbackStruct*) call)->value;
	self->Repaint();
}

void IconList::FocusEH(Widget, XtPointer client, XEvent* ev, Boolean*)
{
	IconList* self = (IconList*) client;
	self->m_focused = ev->type == FocusIn;
	self->Repaint();
}

void IconList::InputCB(Widget w, XtPointer client, XtPointer call)
{
	IconList* self = (IconList*) client;
	XEvent* ev = ((XmDrawingAreaCallbackStruct*) call)->event;
	if (!ev)
		return;

	if (ev->type == ButtonPress)
	{
		switch (ev->xbutton.button) {
			case Button4: self->SetScroll(self->m_scrollY - 60); return;
			case Button5: self->SetScroll(self->m_scrollY + 60); return;
			case Button1:
				XmProcessTraversal(w, XmTRAVERSE_CURRENT);
				self->Activate(self->RowAt(ev->xbutton.y));
				return;
		}
		return;
	}

	if (ev->type == KeyPress)
	{
		KeySym ks = XLookupKeysym(&ev->xkey, 0);
		int page = std::max(1, self->m_viewH / std::max(1, self->m_iconSize + 8) - 1);
		switch (ks) {
			case XK_Up:    case XK_KP_Up:    self->MoveCursor(-1); break;
			case XK_Down:  case XK_KP_Down:  self->MoveCursor(1); break;
			case XK_Prior: case XK_KP_Prior: self->MoveCursor(-page); break;
			case XK_Next:  case XK_KP_Next:  self->MoveCursor(page); break;
			case XK_Home:  case XK_KP_Home:  self->MoveCursor(-100000); break;
			case XK_End:   case XK_KP_End:   self->MoveCursor(100000); break;
			case XK_Return: case XK_KP_Enter: case XK_space:
				self->Activate(self->m_cursor);
				break;
		}
	}
}

void IconList::PaintRow(const IconRow& r, int y, int h, bool selected, bool cursor)
{
	const Palette& p = GetPalette();
	Canvas& c = m_canvas;
	int px = GetTextSize() - 1;
	Rgb bg = m_darker ? p.guildBg : p.listBg;

	if (r.type == IconRow::SPACE) {
		c.HLine(12, y + h / 2, m_viewW - 24, LerpRgb(bg, p.listMuted, 1, 3));
		return;
	}

	if (r.type == IconRow::HEADER) {
		int hpx = px - 2;
		std::string t = Fonts::Elide(r.text, FS_BOLD, hpx, m_viewW - 12 - r.indent);
		Fonts::Draw(c, 8 + r.indent, y + h - 6 - Fonts::Descent(FS_BOLD, hpx), t, FS_BOLD, hpx, p.listHeader);
		return;
	}

	if (selected)
		c.FillRounded(4, y + 1, m_viewW - 8, h - 2, 4, p.selBg);
	if (cursor && m_focused)
		c.Frame(4, y + 1, m_viewW - 8, h - 2, p.listMuted);
	if (r.unread && !selected)
		c.FillRounded(0, y + h / 2 - 4, 4, 8, 2, p.unread);

	// the icon
	int x = 10 + r.indent;
	int s = m_iconSize;
	int iy = y + (h - s) / 2;
	bool drewIcon = false;
	if (r.hasImage) {
		const Image* img = ImageCache::Get(r.imageKind, r.imagePlace, r.imageSf, s, s);
		if (img) {
			if (r.roundImage)
				c.BlendArgbCircle(x + (s - img->w) / 2, iy + (s - img->h) / 2, img->px.data(), img->w, img->h, img->w);
			else
				c.BlendArgb(x + (s - img->w) / 2, iy + (s - img->h) / 2, img->px.data(), img->w, img->h, img->w);
			drewIcon = true;
		}
	}
	if (!drewIcon && (r.initials || r.hasImage)) {
		std::vector<uint32_t> disc((size_t) s * s, 0xff000000u | DiscColor(r.colorSeed ? r.colorSeed : r.id));
		c.BlendArgbCircle(x, iy, disc.data(), s, s, s);
		std::string ini = Initials(r.text);
		int ipx = std::max(8, s * 2 / 5);
		std::string fit = Fonts::Elide(ini, FS_BOLD, ipx, s - 2);
		int iw = Fonts::Measure(fit, FS_BOLD, ipx);
		Fonts::Draw(c, x + (s - iw) / 2, iy + s / 2 + Fonts::Ascent(FS_BOLD, ipx) / 2 - 1, fit, FS_BOLD, ipx, 0xffffff);
		drewIcon = true;
	}
	if (!drewIcon && !r.glyph.empty()) {
		int gpx = px + 2;
		int gw = Fonts::Measure(r.glyph, FS_BOLD, gpx);
		Fonts::Draw(c, x + (s - gw) / 2, y + h / 2 + Fonts::Ascent(FS_BOLD, gpx) / 2 - 1, r.glyph, FS_BOLD, gpx,
			selected ? p.selFg : p.listMuted);
		drewIcon = true;
	}
	if (r.status >= 0 && drewIcon) {
		// the presence dot, ringed with the background
		int d = std::max(8, s / 3);
		Rgb ring = selected ? p.selBg : bg;
		Rgb col = r.status == 1 ? p.online : r.status == 2 ? p.idle : r.status == 3 ? p.dnd : p.offline;
		std::vector<uint32_t> outer((size_t) (d + 4) * (d + 4), 0xff000000u | ring);
		std::vector<uint32_t> inner((size_t) d * d, 0xff000000u | col);
		c.BlendArgbCircle(x + s - d + 1 - 2, iy + s - d + 1 - 2, outer.data(), d + 4, d + 4, d + 4);
		c.BlendArgbCircle(x + s - d + 1, iy + s - d + 1, inner.data(), d, d, d);
	}
	int tx = drewIcon ? x + s + 8 : x + 2;

	// the badge, then the text in what is left
	int right = m_viewW - 8;
	if (r.mentions > 0) {
		std::string n = r.mentions > 99 ? "99+" : std::to_string(r.mentions);
		int bpx = px - 2;
		int bw = std::max(Fonts::Measure(n, FS_BOLD, bpx) + 10, 18);
		int bh = Fonts::LineHeight(FS_BOLD, bpx) + 2;
		c.FillRounded(right - bw, y + (h - bh) / 2, bw, bh, bh / 2, p.badge);
		Fonts::Draw(c, right - bw + (bw - Fonts::Measure(n, FS_BOLD, bpx)) / 2,
			y + (h - bh) / 2 + 1 + Fonts::Ascent(FS_BOLD, bpx), n, FS_BOLD, bpx, p.badgeFg);
		right -= bw + 6;
	}

	FontStyle st = (r.unread || selected) ? FS_BOLD : FS_REGULAR;
	Rgb fg = r.textColor ? r.textColor : selected ? p.selFg : r.unread ? p.unread : r.dim ? p.listMuted : p.listFg;
	std::string t = Fonts::Elide(r.text, st, px, std::max(10, right - tx));
	Fonts::Draw(c, tx, y + h / 2 + Fonts::Ascent(st, px) / 2 - 1, t, st, px, fg);
}

void IconList::Repaint()
{
	if (!XtIsRealized(m_area))
		return;
	Display* dpy = XtDisplay(m_area);
	Window win = XtWindow(m_area);
	if (!m_gc)
		m_gc = XCreateGC(dpy, win, 0, NULL);

	Dimension width = 0, height = 0;
	XtVaGetValues(m_area, XmNwidth, &width, XmNheight, &height, NULL);
	if (width != m_viewW || height != m_viewH) {
		m_viewW = width;
		m_viewH = height;
		UpdateScrollbar();
	}
	if (m_canvas.Width() != m_viewW || m_canvas.Height() != m_viewH)
		m_canvas.Resize(m_viewW, m_viewH);

	const Palette& p = GetPalette();
	m_canvas.Fill(0, 0, m_viewW, m_viewH, m_darker ? p.guildBg : p.listBg);
	for (size_t i = 0; i < m_rows.size(); i++) {
		int h = RowHeight(m_rows[i]);
		int y = m_tops[i] - m_scrollY;
		if (y + h < 0 || y > m_viewH)
			continue;
		PaintRow(m_rows[i], y, h, (int) i == m_selected, (int) i == m_cursor);
	}
	m_canvas.Present(m_fmt, win, m_gc, 0, 0, m_viewW, m_viewH, 0, 0);
}
