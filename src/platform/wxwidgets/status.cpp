#include "core/window.hpp"
#include "platform/wxwidgets/platform.hpp"
#include "platform/wxwidgets/window_status.hpp"
#include "library/string.hpp"
#include "library/minmax.hpp"
#include <iostream>

#define STATWIDTH 40
#define MAXSTATUS 15

namespace
{
	std::string string_pad(const std::string& x, size_t width)
	{
		if(x.length() >= width)
			return x;
		std::string y = x;
		y.append(width - y.length(), ' ');
		return y;
	}
}

wxwin_status::panel::panel(wxWindow* _parent, wxWindow* focuswin, unsigned lines)
	: wxPanel(_parent)
{
	tfocuswin = focuswin;
	parent = _parent;
	dirty = false;
	statusvars.set_size(STATWIDTH, lines ? lines : MAXSTATUS);
	auto s = statusvars.get_pixels();
	SetMinSize(wxSize(s.first, s.second));
	this->Connect(wxEVT_PAINT, wxPaintEventHandler(wxwin_status::panel::on_paint), NULL, this);
	this->Connect(wxEVT_SET_FOCUS, wxFocusEventHandler(wxwin_status::panel::on_focus), NULL, this);
	watch_flag = 0;
}

bool wxwin_status::panel::AcceptsFocus () const
{
	return false;
}

void wxwin_status::panel::on_focus(wxFocusEvent& e)
{
	if(tfocuswin)
		tfocuswin->SetFocus();
}

wxwin_status::wxwin_status(int flag, const std::string& title)
	: wxFrame(NULL, wxID_ANY, towxstring(title), wxDefaultPosition, wxSize(-1, -1),
		wxMINIMIZE_BOX | wxRESIZE_BORDER | wxSYSTEM_MENU | wxCAPTION | wxCLIP_CHILDREN)
{
	wxBoxSizer* top_s = new wxBoxSizer(wxVERTICAL);
	top_s->Add(spanel = new wxwin_status::panel(this, NULL, MAXSTATUS), 1, wxGROW);
	spanel->set_watch_flag(flag);
	top_s->SetSizeHints(this);
	SetSizer(top_s);
	Fit();
}

wxwin_status::~wxwin_status()
{
}

void wxwin_status::panel::on_paint(wxPaintEvent& e)
{
	//Quickly copy the status area.
	auto& s = platform::get_emustatus();
	std::map<std::string, std::string> newstatus;
	emulator_status::iterator i = s.first();
	while(s.next(i))
		newstatus[i.key] = i.value;

	memorywatches.clear();
	statusvars.clear();
	
	size_t mem_width = 0;
	size_t oth_width = 0;
	size_t mem_count = 0;
	size_t oth_count = 0;
	bool single = false;
	for(auto i : newstatus) {
		bool x = regex_match("M\\[.*\\]", i.first);
		if(x) {
			mem_width = max(mem_width, text_framebuffer::text_width(i.first) - 3);
			mem_count++;
		} else {
			oth_width = max(oth_width, text_framebuffer::text_width(i.first));
			oth_count++;
		}
	}
	if(watch_flag < 0)
		oth_count = 0;
	if(watch_flag > 0)
		mem_count = 0;
	if(!oth_count || !mem_count)
		single = true;

	regex_results r;
	if(mem_count) {
		size_t p = single ? 0 : 1;
		memorywatches.set_size(STATWIDTH, mem_count + p);
		if(!single)
			memorywatches.write("Memory watches:", 0, 0, 0, 0, 0xFFFFFF);
		for(auto i : newstatus) {
			if(r = regex("M\\[(.*)\\]", i.first)) {
				size_t n = memorywatches.write(r[1], mem_width + 1, 0, p, 0, 0xFFFFFF);
				memorywatches.write(i.second, 0, n, p, 0, 0xFFFFFF);
				p++;
			}
		}
	}

	if(oth_count) {
		size_t p = single ? 0 : 1;
		statusvars.set_size(STATWIDTH, oth_count + p);
		if(!single)
			statusvars.write("Status:", 0, 0, 0, 0, 0xFFFFFF);
		for(auto i : newstatus) {
			if(regex_match("M\\[.*\\]", i.first))
				continue;
			size_t n = statusvars.write(i.first, oth_width + 1, 0, p, 0, 0xFFFFFF);
			statusvars.write(i.second, 0, n, p, 0, 0xFFFFFF);
			p++;
		}
	}

	auto ssize = statusvars.get_pixels();
	auto msize = memorywatches.get_pixels();
	size_t y2 = !single ? ssize.second + 3 : 0;
	size_t yl = ssize.second + 1;
	std::vector<char> buffer1, buffer2;
	buffer1.resize(msize.first * msize.second * 3);
	buffer2.resize(ssize.first * ssize.second * 3);

	wxPaintDC dc(this);
	dc.Clear();
	if(oth_count) {
		statusvars.render(&buffer2[0]);
		wxBitmap bmp2(wxImage(ssize.first, ssize.second, reinterpret_cast<unsigned char*>(&buffer2[0]),
			true));
		dc.DrawBitmap(bmp2, 0, 0, false);
	}
	if(!single) {
		dc.SetPen(wxPen(wxColour(0, 0, 0)));
		dc.DrawLine(0, yl, msize.first, yl);
	}
	if(mem_count) {
		memorywatches.render(&buffer1[0]);
		wxBitmap bmp1(wxImage(msize.first, msize.second, reinterpret_cast<unsigned char*>(&buffer1[0]),
			true));
		dc.DrawBitmap(bmp1, 0, y2, false);
	}
	dirty = false;
}

void wxwin_status::notify_update() throw()
{
	if(spanel->dirty)
		return;
	spanel->dirty = true;
	spanel->Refresh();
}

bool wxwin_status::ShouldPreventAppExit() const
{
	return false;
}
