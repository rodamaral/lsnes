#include "core/window.hpp"
#include "plat-wxwidgets/platform.hpp"
#include "plat-wxwidgets/window_status.hpp"

#define MAXSTATUS 30

wxwin_status::panel::panel(wxwin_status* _parent, unsigned lines)
	: wxPanel(_parent)
{
	parent = _parent;
	dirty = false;
	wxMemoryDC d;
	wxSize s = d.GetTextExtent(wxT("MMMMMM"));
	SetMinSize(wxSize(6 * s.x, lines * s.y));
	this->Connect(wxEVT_PAINT, wxPaintEventHandler(wxwin_status::panel::on_paint), NULL, this);
}


wxwin_status::wxwin_status()
	: wxFrame(NULL, wxID_ANY, wxT("lsnes: Status"), wxDefaultPosition, wxSize(-1, -1),
		wxMINIMIZE_BOX | wxSYSTEM_MENU | wxCAPTION | wxCLIP_CHILDREN)
{
	wxFlexGridSizer* top_s = new wxFlexGridSizer(1, 1, 0, 0);
	top_s->Add(spanel = new wxwin_status::panel(this, MAXSTATUS));
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

	wxPaintDC dc(this);
	dc.Clear();
	int y = 0;
	for(auto i : newstatus) {
		std::string pstr = i.first + ": " + i.second;
		wxSize s = dc.GetTextExtent(towxstring(pstr));
		dc.DrawText(towxstring(pstr), 0, y);
		y += s.y;
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
