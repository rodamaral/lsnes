#include "platform/wxwidgets/textrender.hpp"
#include "fonts/wrapper.hpp"
#include "library/utf8.hpp"
#include <wx/dc.h>
#include <wx/dcclient.h>
#include <wx/image.h>

extern const uint32_t text_framebuffer::element::white = 0xFFFFFF;
extern const uint32_t text_framebuffer::element::black = 0x000000;

text_framebuffer::text_framebuffer()
{
	width = 0;
	height = 0;
}

text_framebuffer::text_framebuffer(size_t w, size_t h)
{
	width = 0;
	height = 0;
	set_size(w, h);
}

void text_framebuffer::set_size(size_t w, size_t h)
{
	if(w == width && h == height)
		return;
	std::vector<element> newfb;
	newfb.resize(w * h);
	for(size_t i = 0; i < h && i < height; i++)
			for(size_t j = 0; j < w && j < width; j++)
				newfb[i * w + j] = buffer[i * width + j];
	buffer.swap(newfb);
	width = w;
	height = h;
}

std::pair<size_t, size_t> text_framebuffer::get_cell()
{
	return std::make_pair(8, 16);
}

std::pair<size_t, size_t> text_framebuffer::get_pixels()
{
	auto x = get_cell();
	return std::make_pair(x.first * width, x.second * height);
}

void text_framebuffer::render(char* tbuffer)
{
	uint32_t dummy[8] = {0};
	size_t stride = 24 * width;
	size_t cellstride = 24;
	for(size_t y = 0; y < height; y++) {
		size_t xp = 0;
		for(size_t x = 0; x < width; x++) {
			if(xp >= width)
				break;	//No more space in row.
			char* cellbase = tbuffer + y * (16 * stride) + xp * cellstride;
			const element& e = buffer[y * width + x];
			const bitmap_font::glyph& g = main_font.get_glyph(e.ch);
			char bgb = (e.bg >> 16);
			char bgg = (e.bg >> 8);
			char bgr = (e.bg >> 0);
			char fgb = (e.fg >> 16);
			char fgg = (e.fg >> 8);
			char fgr = (e.fg >> 0);
			const uint32_t* data = (g.data ? g.data : dummy);
			if(g.wide && xp < width - 1) {
				//Wide character, can draw full width.
				for(size_t y2 = 0; y2 < 16; y2++) {
					uint32_t d = data[y2 >> 1];
					d >>= 16 - ((y2 & 1) << 4);
					for(size_t j = 0; j < 16; j++) {
						uint32_t b = 15 - j;
						if(((d >> b) & 1) != 0) {
							cellbase[3 * j + 0] = fgr;
							cellbase[3 * j + 1] = fgg;
							cellbase[3 * j + 2] = fgb;
						} else {
							cellbase[3 * j + 0] = bgr;
							cellbase[3 * j + 1] = bgg;
							cellbase[3 * j + 2] = bgb;
						}
					}
					cellbase += stride;
				}
				xp += 2;
			} else if(g.wide) {
				//Wide character, can only draw half.
				for(size_t y2 = 0; y2 < 16; y2++) {
					uint32_t d = data[y2 >> 1];
					d >>= 16 - ((y2 & 1) << 4);
					for(size_t j = 0; j < 8; j++) {
						uint32_t b = 15 - j;
						if(((d >> b) & 1) != 0) {
							cellbase[3 * j + 0] = fgr;
							cellbase[3 * j + 1] = fgg;
							cellbase[3 * j + 2] = fgb;
						} else {
							cellbase[3 * j + 0] = bgr;
							cellbase[3 * j + 1] = bgg;
							cellbase[3 * j + 2] = bgb;
						}
					}
					cellbase += stride;
				}
				xp += 2;
			} else {
				//Narrow character.
				for(size_t y2 = 0; y2 < 16; y2++) {
					uint32_t d = data[y2 >> 2];
					d >>= 24 - ((y2 & 3) << 3);
					for(size_t j = 0; j < 8; j++) {
						uint32_t b = 7 - j;
						if(((d >> b) & 1) != 0) {
							cellbase[3 * j + 0] = fgr;
							cellbase[3 * j + 1] = fgg;
							cellbase[3 * j + 2] = fgb;
						} else {
							cellbase[3 * j + 0] = bgr;
							cellbase[3 * j + 1] = bgg;
							cellbase[3 * j + 2] = bgb;
						}
					}
					cellbase += stride;
				}
				xp += 1;
			}
		}
	}
}

size_t text_framebuffer::text_width(const std::string& text)
{
	auto x = main_font.get_metrics(text);
	return x.first / 8;
}

size_t text_framebuffer::write(const std::string& str, size_t w, size_t x, size_t y, uint32_t fg, uint32_t bg)
{
	return this->write(to_u32string(str), w, x, y, fg, bg);
}

size_t text_framebuffer::write(const std::u32string& str, size_t w, size_t x, size_t y, uint32_t fg, uint32_t bg)
{
	if(y >= height)
		return 0;
	size_t spos = 0;
	size_t slen = str.length();
	size_t pused = 0;
	while(spos < slen) {
		int32_t u = str[spos++];
		const bitmap_font::glyph& g = main_font.get_glyph(u);
		if(x < width) {
			element& e = buffer[y * width + x];
			e.ch = u;
			e.fg = fg;
			e.bg = bg;
		}
		x++;
		pused += (g.wide ? 2 : 1);
	}
	while(pused < w) {
		//Pad with spaces.
		if(x < width) {
			element& e = buffer[y * width + x];
			e.ch = 32;
			e.fg = fg;
			e.bg = bg;
		}
		pused++;
		x++;
	}
	return x;
}

text_framebuffer_panel::text_framebuffer_panel(wxWindow* parent, size_t w, size_t h, wxWindowID id,
	wxWindow* _redirect)
	: wxPanel(parent, id), text_framebuffer(w, h)
{
	redirect = _redirect;
	auto psize = get_pixels();
	SetMinSize(wxSize(psize.first, psize.second));
	this->Connect(wxEVT_PAINT, wxPaintEventHandler(text_framebuffer_panel::on_paint), NULL, this);
	this->Connect(wxEVT_ERASE_BACKGROUND, wxEraseEventHandler(text_framebuffer_panel::on_erase), NULL, this);
	this->Connect(wxEVT_SET_FOCUS, wxFocusEventHandler(text_framebuffer_panel::on_focus), NULL, this);
}

text_framebuffer_panel::~text_framebuffer_panel()
{
}

void text_framebuffer_panel::set_size(size_t _width, size_t _height)
{
	text_framebuffer::set_size(_width, _height);
	auto psize = get_pixels();
	buffer.resize(psize.first * psize.second * 3);
	if(!locked) {
		SetMinSize(wxSize(psize.first, psize.second));
	} else
		size_changed = true;
	request_paint();
}

void text_framebuffer_panel::request_paint()
{
	if(size_changed) {
		auto psize = get_pixels();
		SetMinSize(wxSize(psize.first, psize.second));
		size_changed = false;
		paint_requested = true;
		Refresh();
	} else if(!paint_requested) {
		paint_requested = true;
		Refresh();
	}
}

void text_framebuffer_panel::on_erase(wxEraseEvent& e)
{
}

void text_framebuffer_panel::on_paint(wxPaintEvent& e)
{
	locked = true;
	auto size = GetSize();
	text_framebuffer::set_size((size.x + 7) / 8, (size.y + 15) / 16);
	auto psize = get_pixels();
	buffer.resize(psize.first * psize.second * 3);
	prepare_paint();
	locked = false;
	psize = get_pixels();
	wxPaintDC dc(this);
	render(&buffer[0]);
	wxBitmap bmp1(wxImage(psize.first, psize.second, reinterpret_cast<unsigned char*>(&buffer[0]),
		true));
	dc.DrawBitmap(bmp1, 0, 0, false);
	paint_requested = false;
}

void text_framebuffer_panel::on_focus(wxFocusEvent& e)
{
	if(redirect)
		redirect->SetFocus();
}
