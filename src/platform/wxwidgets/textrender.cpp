#include "platform/wxwidgets/textrender.hpp"
#include "platform/wxwidgets/platform.hpp"
#include "fonts/wrapper.hpp"
#include "library/utf8.hpp"
#include "library/string.hpp"
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
	size_t stride = 24 * width;
	size_t cellstride = 24;
	for(size_t y = 0; y < height; y++) {
		size_t xp = 0;
		for(size_t x = 0; x < width; x++) {
			if(xp >= width)
				break;	//No more space in row.
			char* cellbase = tbuffer + y * (16 * stride) + xp * cellstride;
			const element& e = buffer[y * width + x];
			const framebuffer::font::glyph& g = main_font.get_glyph(e.ch);
			char bgb = (e.bg >> 16);
			char bgg = (e.bg >> 8);
			char bgr = (e.bg >> 0);
			char fgb = (e.fg >> 16);
			char fgg = (e.fg >> 8);
			char fgr = (e.fg >> 0);

			uint32_t cells = g.get_width() / 8;
			uint32_t drawc = 8 * std::min(cells, (uint32_t)((xp < width - 1) ? 2 : 1));
			for(size_t y2 = 0; y2 < g.get_height(); y2++) {
				for(size_t j = 0; j < drawc; j++) {
					if(g.read_pixel(j, y2)) {
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
			xp += cells;
		}
	}
}

size_t text_framebuffer::text_width(const std::string& text)
{
	auto x = main_font.get_metrics(text, 0, false, false);
	return x.first / 8;
}

size_t text_framebuffer::write(const std::string& str, size_t w, size_t x, size_t y, uint32_t fg, uint32_t bg)
{
	return write(utf8::to32(str), w, x, y, fg, bg);
}

size_t text_framebuffer::write(const std::u32string& str, size_t w, size_t x, size_t y, uint32_t fg, uint32_t bg)
{
	if(y >= height)
		return 0;
	size_t pused = 0;
	for(auto u : str) {
		if(u == 9) {
			//TAB.
			do {
				if(x + pused < width) {
					element& e = buffer[y * width + x + pused];
					e.ch = 32;	//Space.
					e.fg = fg;
					e.bg = bg;
				}
				pused++;
			} while(pused % 8);
			continue;
		}
		const framebuffer::font::glyph& g = main_font.get_glyph(u);
		if(x + pused < width) {
			element& e = buffer[y * width + x + pused];
			e.ch = u;
			e.fg = fg;
			e.bg = bg;
		}
		pused += g.get_width() / 8;
	}
	while(pused < w) {
		//Pad with spaces.
		if(x + pused < width) {
			element& e = buffer[y * width + x + pused];
			e.ch = 32;
			e.fg = fg;
			e.bg = bg;
		}
		pused++;
	}
	return x + pused;
}

text_framebuffer_panel::text_framebuffer_panel(wxWindow* parent, size_t w, size_t h, wxWindowID id,
	wxWindow* _redirect)
	: wxPanel(parent, id), text_framebuffer(w, h)
{
	CHECK_UI_THREAD;
	redirect = _redirect;
	auto psize = get_pixels();
	size_changed = false;
	locked = false;
	paint_requested = false;
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
	CHECK_UI_THREAD;
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
	CHECK_UI_THREAD;
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
	CHECK_UI_THREAD;
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
	CHECK_UI_THREAD;
	if(redirect)
		redirect->SetFocus();
}
