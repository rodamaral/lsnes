#include "core/controller.hpp"
#include "core/movie.hpp"
#include "core/moviedata.hpp"
#include "core/dispatch.hpp"
#include "core/window.hpp"

#include "interface/controller.hpp"
#include "core/mainloop.hpp"
#include "platform/wxwidgets/platform.hpp"
#include "platform/wxwidgets/textrender.hpp"
#include "library/minmax.hpp"
#include "library/string.hpp"
#include "library/utf8.hpp"

#include <algorithm>
#include <cstring>
#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>
#include <wx/statline.h>
#include <wx/spinctrl.h>

class wxeditor_tasinput : public wxDialog, public information_dispatch
{
public:
	wxeditor_tasinput(wxWindow* parent);
	~wxeditor_tasinput() throw();
	bool ShouldPreventAppExit() const;
	void on_wclose(wxCloseEvent& e);
	void on_control(wxCommandEvent& e);
	void on_autohold_reconfigure();
private:
	struct xypanel;
	struct control_triple
	{
		unsigned port;
		unsigned controller;
		unsigned xindex;
		unsigned yindex;	//Used for XY.
		enum port_controller_button::_type type;
		short xmin;
		short ymin;
		short xmax;
		short ymax;
		bool xcenter;
		bool ycenter;
		xypanel* panel;
		wxCheckBox* check;
		unsigned logical;	//Logical controller. Internal only.
		std::string name;	//Name. Internal only.
	};
	struct controller_double
	{
		wxPanel* panel;
		wxStaticBox* box;
		wxStaticText* label;
		wxSizer* rtop;
		wxSizer* top;
	};
	struct xypanel : public wxEvtHandler
	{
		xypanel(wxWindow* win, wxSizer* s, control_triple _t, wxEvtHandler* _obj, wxObjectEventFunction _fun,
			int _wxid);
		short get_x() { return x; }
		short get_y() { return y; }
		void on_click(wxMouseEvent& e);
		void on_numbers_change(wxSpinEvent& e);
		void on_paint(wxPaintEvent& e);
		void Destroy();
	private:
		short x, y;
		wxEvtHandler* obj;
		wxObjectEventFunction fun;
		int wxid;
		control_triple t;
		unsigned awidth;
		unsigned aheight;
		wxPanel* graphics;
		wxSpinCtrl* xnum;
		wxSpinCtrl* ynum;
		void do_redraw();
	};
	std::map<int, control_triple> inputs;
	std::vector<controller_double> panels;
	void update_controls();
	bool closing;
	wxBoxSizer* hsizer;
};

namespace
{
	wxeditor_tasinput* tasinput_open;
}

wxeditor_tasinput::xypanel::xypanel(wxWindow* win, wxSizer* s, control_triple _t, wxEvtHandler* _obj,
	wxObjectEventFunction _fun, int _wxid)
{
	x = 0;
	y = 0;
	xnum = NULL;
	ynum = NULL;
	obj = _obj;
	fun = _fun;
	wxid = _wxid;
	t = _t;
	s->Add(new wxStaticText(win, wxID_ANY, towxstring(t.name)));
	s->Add(graphics = new wxPanel(win, wxID_ANY));
	graphics->SetSize(128, (t.yindex != std::numeric_limits<unsigned>::max()) ? 128 : 16);
	graphics->SetMinSize(graphics->GetSize());
	graphics->SetMaxSize(graphics->GetSize());
	graphics->Connect(wxEVT_PAINT, wxPaintEventHandler(xypanel::on_paint), NULL, this);
	graphics->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(xypanel::on_click), NULL, this);
	x = t.xcenter ? ((int)t.xmin + t.xmax) / 2 : t.xmin;
	y = t.ycenter ? ((int)t.ymin + t.ymax) / 2 : t.ymin;
	s->Add(xnum = new wxSpinCtrl(win, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS,
		t.xmin, t.xmax, x));
	if(t.yindex != std::numeric_limits<unsigned>::max())
		s->Add(ynum = new wxSpinCtrl(win, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize,
			wxSP_ARROW_KEYS, t.ymin, t.ymax, y));
	xnum->Connect(wxEVT_COMMAND_SPINCTRL_UPDATED, wxSpinEventHandler(xypanel::on_numbers_change), NULL, this);
	if(ynum) ynum->Connect(wxEVT_COMMAND_SPINCTRL_UPDATED, wxSpinEventHandler(xypanel::on_numbers_change), NULL,
		this);
}

void wxeditor_tasinput::xypanel::on_click(wxMouseEvent& e)
{
	wxCommandEvent e2(0, wxid);
	unsigned xrange = t.xmax - t.xmin;
	unsigned yrange = t.ymax - t.ymin;
	wxSize ps = graphics->GetSize();
	x = t.xmin + (int32_t)(t.xmax - t.xmin) * e.GetX() / ps.GetWidth();
	y = t.ymin + (int32_t)(t.ymax - t.ymin) * e.GetY() / ps.GetHeight();
	if(xnum) xnum->SetValue(x);
	if(xnum) ynum->SetValue(y);
	do_redraw();
	(obj->*fun)(e2);
}

void wxeditor_tasinput::xypanel::on_numbers_change(wxSpinEvent& e)
{
	wxCommandEvent e2(0, wxid);
	if(xnum) x = xnum->GetValue();
	if(ynum) y = ynum->GetValue();
	do_redraw();
	(obj->*fun)(e2);
}

void wxeditor_tasinput::xypanel::do_redraw()
{
	graphics->Refresh();
}

void wxeditor_tasinput::xypanel::on_paint(wxPaintEvent& e)
{
	wxPaintDC dc(graphics);
	dc.SetBackground(*wxWHITE_BRUSH);
	dc.SetPen(*wxBLACK_PEN);
	dc.Clear();
	wxSize ps = graphics->GetSize();
	dc.DrawLine(0, 0, ps.GetWidth(), 0);
	dc.DrawLine(0, 0, 0, ps.GetHeight());
	dc.DrawLine(0, ps.GetHeight() - 1, ps.GetWidth(), ps.GetHeight() - 1);
	dc.DrawLine(ps.GetWidth() - 1, 0, ps.GetWidth() - 1, ps.GetHeight());
	if(t.xcenter)
		dc.DrawLine(ps.GetWidth() / 2, 0, ps.GetWidth() / 2, ps.GetHeight());
	if((t.yindex != std::numeric_limits<unsigned>::max()) && t.ycenter)
		dc.DrawLine(0, ps.GetHeight() / 2, ps.GetWidth(), ps.GetHeight() / 2);
	dc.SetPen(*wxRED_PEN);
	int xdraw = (x - t.xmin) * ps.GetWidth() / (t.xmax - t.xmin);
	int ydraw = (y - t.ymin) * ps.GetHeight() / (t.ymax - t.ymin);
	dc.DrawLine(xdraw, 0, xdraw, ps.GetHeight());
	if((t.yindex != std::numeric_limits<unsigned>::max()) || t.ycenter)
		dc.DrawLine(0, ydraw, ps.GetWidth(), ydraw);
}

void wxeditor_tasinput::xypanel::Destroy()
{
	graphics->Destroy();
	xnum->Destroy();
	if(ynum) ynum->Destroy();
}

wxeditor_tasinput::~wxeditor_tasinput() throw() {}

wxeditor_tasinput::wxeditor_tasinput(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, wxT("lsnes: TAS input plugin"), wxDefaultPosition, wxSize(-1, -1)),
	information_dispatch("tasinput-listener")
{
	closing = false;
	Centre();
	hsizer = new wxBoxSizer(wxHORIZONTAL);
	SetSizer(hsizer);
	Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(wxeditor_tasinput::on_wclose));
	update_controls();
	hsizer->SetSizeHints(this);
	Fit();
}

void wxeditor_tasinput::on_autohold_reconfigure()
{
	runuifun([this]() {
		try {
			this->update_controls();
		} catch(std::runtime_error& e) {
			//Close the window.
			bool wasc = closing;
			closing = true;
			tasinput_open = NULL;
			controls.tasinput_enable(false);
			if(!wasc)
				Destroy();
		}
	});
}

void wxeditor_tasinput::on_control(wxCommandEvent& e)
{
	int id = e.GetId();
	if(!inputs.count(id))
		return;
	auto t = inputs[id];
	int16_t xstate = 0;
	int16_t ystate = 0;
	if(t.check)
		xstate = t.check->GetValue() ? 1 : 0;
	else if(t.panel) {
		xstate = t.panel->get_x();
		ystate = t.panel->get_y();
	}
	runemufn([t, xstate, ystate]() {
		controls.tasinput(t.port, t.controller, t.xindex, xstate);
		if(t.yindex != std::numeric_limits<unsigned>::max())
			controls.tasinput(t.port, t.controller, t.yindex, ystate);
	});
}

void wxeditor_tasinput::update_controls()
{
	for(auto i : inputs) {
		if(i.second.check)
			i.second.check->Destroy();
		if(i.second.panel)
			i.second.panel->Destroy();
	}
	for(auto i : panels) {
		hsizer->Detach(i.panel);
		i.panel->Destroy();
	}
	inputs.clear();
	panels.clear();

	std::vector<control_triple> _inputs;
	std::vector<std::string> _controller_labels;
	runemufn([&_inputs, &_controller_labels](){
		std::map<std::string, unsigned> next_in_class;
		controller_frame model = controls.get_blank();
		const port_type_set& pts = model.porttypes();
		unsigned pcnt = pts.ports();
		unsigned cnum_g = 0;
		for(unsigned i = 0;; i++) {
			auto pcid = controls.lcid_to_pcid(i);
			if(pcid.first < 0)
				break;
			const port_type& pt = pts.port_type(pcid.first);
			const port_controller_set& pci = *(pt.controller_info);
			if(pci.controller_count <= pcid.second || !pci.controllers[pcid.second])
				continue;
			const port_controller& pc = *(pci.controllers[pcid.second]);
			//First check that this has non-hidden stuff.
			bool has_buttons = false;
			for(unsigned k = 0; k < pc.button_count; k++) {
				if(!pc.buttons[k])
					continue;
				const port_controller_button& pcb = *(pc.buttons[k]);
				if(!pcb.shadow)
					has_buttons = true;
			}
			if(!has_buttons)
				continue;
			//Okay, a valid controller.
			if(!next_in_class.count(pc.cclass))
				next_in_class[pc.cclass] = 1;
			uint32_t cnum = next_in_class[pc.cclass]++;
			_controller_labels.push_back((stringfmt() << pc.cclass << "-" << cnum).str());
			//Go through all controller pairs.
			for(unsigned k = 0; k < pc.analog_actions(); k++) {
				std::pair<unsigned, unsigned> indices = pc.analog_action(k);
				if(pc.buttons[indices.first]->shadow)
					continue;
				struct control_triple t;
				t.port = pcid.first;
				t.controller = pcid.second;
				t.xindex = indices.first;
				t.yindex = indices.second;
				t.xmin = pc.buttons[t.xindex]->rmin;
				t.xmax = pc.buttons[t.xindex]->rmax;
				t.xcenter = pc.buttons[t.xindex]->centers;
				t.ymin = 0;
				t.ymax = 1;
				t.ycenter = false;
				t.type = pc.buttons[t.xindex]->type;
				t.name = pc.buttons[t.xindex]->name;
				t.logical = cnum_g;
				if(t.yindex != std::numeric_limits<unsigned>::max()) {
					t.ymin = pc.buttons[t.yindex]->rmin;
					t.ymax = pc.buttons[t.yindex]->rmax;
					t.ycenter = pc.buttons[t.yindex]->centers;
					t.name = t.name + "/" + pc.buttons[t.yindex]->name;
				}
				_inputs.push_back(t);
			}
			//Go through all buttons.
			for(unsigned k = 0; k < pc.button_count; k++) {
				if(!pc.buttons[k])
					continue;
				const port_controller_button& pcb = *(pc.buttons[k]);
				if(pcb.type != port_controller_button::TYPE_BUTTON || pcb.shadow)
					continue;
				struct control_triple t;
				t.port = pcid.first;
				t.controller = pcid.second;
				t.xindex = k;
				t.yindex = std::numeric_limits<unsigned>::max();
				t.logical = cnum_g;
				t.type = port_controller_button::TYPE_BUTTON;
				t.name = pcb.name;
				_inputs.push_back(t);
			}
			for(unsigned k = 0; k < pc.button_count; k++) {
				if(!pc.buttons[k])
					continue;
				const port_controller_button& pcb = *(pc.buttons[k]);
				if(pcb.type == port_controller_button::TYPE_BUTTON || pcb.shadow)
					continue;
				controls.tasinput(pcid.first, pcid.second, k, pcb.centers ? ((int)pcb.rmin +
					pcb.rmax) / 2 : pcb.rmin);
			}
			cnum_g++;
		}
	});
	int next_id = wxID_HIGHEST + 1;
	unsigned last_logical = 0xFFFFFFFFUL;
	wxSizer* current;
	wxPanel* current_p;
	for(auto i : _inputs) {
		if(i.logical != last_logical) {
			//New controller starts.
			std::cerr << "Controller: " << _controller_labels[i.logical] << std::endl;
			controller_double d;
			current_p = d.panel = new wxPanel(this, wxID_ANY);
			d.rtop = new wxBoxSizer(wxVERTICAL);
			d.panel->SetSizer(d.rtop);
			d.box = new wxStaticBox(d.panel, wxID_ANY, towxstring(_controller_labels[i.logical]));
			current = d.top = new wxStaticBoxSizer(d.box, wxVERTICAL);
#ifdef __WXMAC__
			d.label = new wxStaticText(d.panel, wxID_ANY, towxstring(_controller_labels[i.logical]));
			d.top->Add(d.label);
#endif
			d.rtop->Add(d.top);
			hsizer->Add(d.panel);
			panels.push_back(d);
			last_logical = i.logical;
		}
		struct control_triple t = i;
		t.check = NULL;
		t.panel = NULL;
		std::cerr << "Control: " << i.name << std::endl;
		if(i.type == port_controller_button::TYPE_BUTTON) {
			t.check = new wxCheckBox(current_p, next_id, towxstring(i.name));
			current->Add(t.check);
			t.check->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED,  
				wxCommandEventHandler(wxeditor_tasinput::on_control), NULL, this);
		} else {
			t.panel = new xypanel(current_p, current, i, this,
				wxCommandEventHandler(wxeditor_tasinput::on_control), next_id);
		}
		inputs[next_id++] = t;
	}
	if(_inputs.empty()) {
		throw std::runtime_error("No controlers");
	}
	Fit();
}

bool wxeditor_tasinput::ShouldPreventAppExit() const { return false; }

void wxeditor_tasinput::on_wclose(wxCloseEvent& e)
{
	bool wasc = closing;
	closing = true;
	tasinput_open = NULL;
	controls.tasinput_enable(false);
	if(!wasc)
		Destroy();
}

void wxeditor_tasinput_display(wxWindow* parent)
{
	if(tasinput_open)
		return;
	wxeditor_tasinput* v;
	try {
		v = new wxeditor_tasinput(parent);
	} catch(std::runtime_error& e) {
		wxMessageBox(_T("No controllers present"), _T("Error"), wxICON_EXCLAMATION | wxOK, parent);
		return;
	}
	v->Show();
	tasinput_open = v;
	controls.tasinput_enable(true);
}
