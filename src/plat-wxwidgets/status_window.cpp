#include "core/window.hpp"

#include "plat-wxwidgets/emufn.hpp"
#include "plat-wxwidgets/status_window.hpp"


#define MAXSTATUS 30

class wx_status_panel : public wxPanel
{
public:
	wx_status_panel(unsigned lines);
	void on_paint(wxPaintEvent& e);
	bool dirty;
};

wx_status_window* wx_status_window::ptr;

wx_status_panel::wx_status_panel(unsigned lines)
	: wxPanel(wx_status_window::ptr)
{
	dirty = false;
	wxMemoryDC d;
	wxSize s = d.GetTextExtent(wxT("MMMMMM"));
	SetMinSize(wxSize(6 * s.x, lines * s.y));
	this->Connect(wxEVT_PAINT, wxPaintEventHandler(wx_status_panel::on_paint), NULL, this);
}


namespace {
	wx_status_panel* spanel;
}

wx_status_window::wx_status_window()
	: wxFrame(NULL, wxID_ANY, wxT("lsnes: Status"), wxDefaultPosition, wxSize(-1, -1), secondary_window_style)
{
	ptr = this;
	wxFlexGridSizer* top_s = new wxFlexGridSizer(1, 1, 0, 0);
	top_s->Add(spanel = new wx_status_panel(MAXSTATUS));
	top_s->SetSizeHints(this);
	SetSizer(top_s);
	Fit();
}

wx_status_window::~wx_status_window()
{
	ptr = NULL;
}

void wx_status_panel::on_paint(wxPaintEvent& e)
{
	wxPaintDC dc(this);
	dc.Clear();
	int y = 0;
	auto& status = window::get_emustatus();
	for(auto i : status) {
		std::string pstr = i.first + ": " + i.second;
		wxSize s = dc.GetTextExtent(towxstring(pstr));
		dc.DrawText(towxstring(pstr), 0, y);
		y += s.y;
	}
	dirty = false;
}

void wx_status_window::notify_status_change()
{
	if(!spanel || spanel->dirty)
		return;
	spanel->dirty = true;
	spanel->Refresh();
}

bool wx_status_window::ShouldPreventAppExit() const
{
	return false;
}
