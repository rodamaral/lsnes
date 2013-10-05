#include "core/controller.hpp"
#include "core/framebuffer.hpp"
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

extern "C"
{
#ifndef UINT64_C
#define UINT64_C(val) val##ULL
#endif
#include <libswscale/swscale.h>
}

namespace
{
	const int padmajsize = 193;
	const int padminsize = 16;

	int32_t value_to_coordinate(int32_t rmin, int32_t rmax, int32_t val, int32_t dim)
	{
		if(dim == rmax - rmin + 1)
			return val - rmin;
		//Scale the values to be zero-based.
		val = min(max(val, rmin), rmax);
		rmax -= rmin;
		val -= rmin;
		int32_t center = rmax / 2;
		int32_t cc = (dim - 1) / 2;
		if(val == center)
			return cc;
		if(val < center) {
			//0 => 0, center => cc.
			return (val * (int64_t)cc + (center / 2)) / center;
		}
		if(val > center) {
			//center => cc, rmax => dim - 1.
			val -= center;
			rmax -= center;
			int32_t cc2 = (dim - 1 - cc);
			return (val * (int64_t)cc2 + (rmax / 2)) / rmax + cc;
		}
	}

	int32_t coordinate_to_value(int32_t rmin, int32_t rmax, int32_t val, int32_t dim)
	{
		if(dim == rmax - rmin + 1)
			return val + rmin;
		val = min(max(val, (int32_t)0), dim - 1);
		int32_t center = (rmax + rmin) / 2;
		int32_t cc = (dim - 1) / 2;
		if(val == cc)
			return center;
		if(val < cc) {
			//0 => rmin, cc => center.
			return ((center - rmin) * (int64_t)val + cc / 2) / cc + rmin;
		}
		if(val > cc) {
			//cc => center, dim - 1 => rmax.
			uint32_t cc2 = (dim - 1 - cc);
			return ((rmax - center) * (int64_t)(val - cc) + cc2 / 2) / cc2 + center;
		}
	}
}

class wxeditor_tasinput : public wxDialog
{
public:
	wxeditor_tasinput(wxWindow* parent);
	~wxeditor_tasinput() throw();
	bool ShouldPreventAppExit() const;
	void on_wclose(wxCloseEvent& e);
	void on_control(wxCommandEvent& e);
	void on_keyboard_up(wxKeyEvent& e);
	void on_keyboard_down(wxKeyEvent& e);
	void call_screen_update();
private:
	struct dispatch_target<> ahreconfigure;
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
		~xypanel();
		short get_x() { return x; }
		short get_y() { return y; }
		void on_click(wxMouseEvent& e);
		void on_numbers_change(wxSpinEvent& e);
		void on_paint(wxPaintEvent& e);
		void Destroy();
		void do_redraw();
	private:
		short x, y;
		wxEvtHandler* obj;
		wxObjectEventFunction fun;
		int wxid;
		control_triple t;
		wxPanel* graphics;
		wxSpinCtrl* xnum;
		wxSpinCtrl* ynum;
		bool lightgun;
		bool dirty;
		struct SwsContext* rctx;
	};
	std::map<int, control_triple> inputs;
	std::vector<controller_double> panels;
	void update_controls();
	void connect_keyboard_recursive(wxWindow* win);
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
	rctx = NULL;
	dirty = false;
	obj = _obj;
	fun = _fun;
	wxid = _wxid;
	t = _t;
	s->Add(new wxStaticText(win, wxID_ANY, towxstring(t.name)));
	s->Add(graphics = new wxPanel(win, wxID_ANY));
	lightgun = false;
	if(t.type == port_controller_button::TYPE_LIGHTGUN && t.yindex != std::numeric_limits<unsigned>::max()) {
		graphics->SetSize(t.xmax - t.xmin + 1, t.ymax - t.ymin + 1);
		lightgun = true;
	} else
		graphics->SetSize(padmajsize, (t.yindex != std::numeric_limits<unsigned>::max()) ? padmajsize :
			padminsize);
	graphics->SetMinSize(graphics->GetSize());
	graphics->SetMaxSize(graphics->GetSize());
	graphics->Connect(wxEVT_PAINT, wxPaintEventHandler(xypanel::on_paint), NULL, this);
	graphics->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(xypanel::on_click), NULL, this);
	graphics->Connect(wxEVT_MOTION, wxMouseEventHandler(xypanel::on_click), NULL, this);
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

wxeditor_tasinput::xypanel::~xypanel()
{
	sws_freeContext(rctx);
}

void wxeditor_tasinput::call_screen_update()
{
	for(auto i : inputs)
		if(i.second.type == port_controller_button::TYPE_LIGHTGUN)
			i.second.panel->do_redraw();
}

void wxeditor_tasinput::xypanel::on_click(wxMouseEvent& e)
{
	if(!e.Dragging() && !e.LeftDown())
		return;
	wxCommandEvent e2(0, wxid);
	unsigned xrange = t.xmax - t.xmin;
	unsigned yrange = t.ymax - t.ymin;
	wxSize ps = graphics->GetSize();
	x = coordinate_to_value(t.xmin, t.xmax, e.GetX(), ps.GetWidth());
	y = coordinate_to_value(t.ymin, t.ymax, e.GetY(), ps.GetHeight());
	if(xnum) xnum->SetValue(x);
	if(ynum) ynum->SetValue(y);
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
	if(!dirty) {
		dirty = true;
		graphics->Refresh();
	}
}

void wxeditor_tasinput::xypanel::on_paint(wxPaintEvent& e)
{
	wxPaintDC dc(graphics);
	if(lightgun) {
		//Draw the current screen.
		framebuffer_raw& _fb = render_get_latest_screen();
		framebuffer<false> fb;
		auto osize = std::make_pair(_fb.get_width(), _fb.get_height());
		auto size = our_rom.rtype->lightgun_scale();
		fb.reallocate(osize.first, osize.second, false);
		fb.copy_from(_fb, 1, 1);
		render_get_latest_screen_end();
		std::vector<uint8_t> buf;
		buf.resize(3 * (t.ymax - t.ymin + 1) * (t.ymax - t.ymin + 1));
		unsigned offX = -t.xmin;
		unsigned offY = -t.ymin;
		rctx = sws_getCachedContext(rctx, osize.first, osize.second, PIX_FMT_RGBA,
			size.first, size.second, PIX_FMT_BGR24, SWS_POINT, NULL, NULL, NULL);
		uint8_t* srcp[1];
		int srcs[1];
		uint8_t* dstp[1];
		int dsts[1];
		srcs[0] = 4 * (fb.rowptr(1) - fb.rowptr(0));
		dsts[0] = 3 * (t.xmax - t.xmin + 1);
		srcp[0] = reinterpret_cast<unsigned char*>(fb.rowptr(0));
		dstp[0] = &buf[3 * (offY * (t.xmax - t.xmin + 1) + offX)];
		memset(&buf[0], 0, buf.size());
		sws_scale(rctx, srcp, srcs, 0, size.second, dstp, dsts);
		wxBitmap bmp(wxImage(t.xmax - t.xmin + 1, t.ymax - t.ymin + 1, &buf[0], true));
		dc.DrawBitmap(bmp, 0, 0, false);
	} else {
		dc.SetBackground(*wxWHITE_BRUSH);
		dc.SetPen(*wxBLACK_PEN);
		dc.Clear();
	}
	wxSize ps = graphics->GetSize();
	dc.DrawLine(0, 0, ps.GetWidth(), 0);
	dc.DrawLine(0, 0, 0, ps.GetHeight());
	dc.DrawLine(0, ps.GetHeight() - 1, ps.GetWidth(), ps.GetHeight() - 1);
	dc.DrawLine(ps.GetWidth() - 1, 0, ps.GetWidth() - 1, ps.GetHeight());
	int xcenter = (ps.GetWidth() - 1) / 2;
	int ycenter = (ps.GetHeight() - 1) / 2;
	if(t.xcenter)
		dc.DrawLine(xcenter, 0, xcenter, ps.GetHeight());
	if((t.yindex != std::numeric_limits<unsigned>::max()) && t.ycenter)
		dc.DrawLine(0, ycenter, ps.GetWidth(), ycenter);
	dc.SetPen(*wxRED_PEN);
	int xdraw = value_to_coordinate(t.xmin, t.xmax, x, ps.GetWidth());
	int ydraw = value_to_coordinate(t.ymin, t.ymax, y, ps.GetHeight());
	dc.DrawLine(xdraw, 0, xdraw, ps.GetHeight());
	if((t.yindex != std::numeric_limits<unsigned>::max()) || t.ycenter)
		dc.DrawLine(0, ydraw, ps.GetWidth(), ydraw);
	dirty = false;
}

void wxeditor_tasinput::xypanel::Destroy()
{
	graphics->Destroy();
	xnum->Destroy();
	if(ynum) ynum->Destroy();
}

wxeditor_tasinput::~wxeditor_tasinput() throw() {}

wxeditor_tasinput::wxeditor_tasinput(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, wxT("lsnes: TAS input plugin"), wxDefaultPosition, wxSize(-1, -1))
{
	closing = false;
	Centre();
	hsizer = new wxBoxSizer(wxHORIZONTAL);
	SetSizer(hsizer);
	Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(wxeditor_tasinput::on_wclose));
	update_controls();
	hsizer->SetSizeHints(this);
	Fit();

	ahreconfigure.set(notify_autohold_reconfigure, [this]() {
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
	});
}

void wxeditor_tasinput::connect_keyboard_recursive(wxWindow* win)
{
	win->Connect(wxEVT_KEY_DOWN, wxKeyEventHandler(wxeditor_tasinput::on_keyboard_down), NULL, this);
	win->Connect(wxEVT_KEY_UP, wxKeyEventHandler(wxeditor_tasinput::on_keyboard_up), NULL, this);
	auto i = win->GetChildren().GetFirst();
	while(i) {
		connect_keyboard_recursive(i->GetData());
		i = i->GetNext();
	}
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
			if(pci.controllers.size() <= pcid.second)
				continue;
			const port_controller& pc = *(pci.controllers[pcid.second]);
			//First check that this has non-hidden stuff.
			bool has_buttons = false;
			for(unsigned k = 0; k < pc.buttons.size(); k++) {
				const port_controller_button& pcb = pc.buttons[k];
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
				if(pc.buttons[indices.first].shadow)
					continue;
				struct control_triple t;
				t.port = pcid.first;
				t.controller = pcid.second;
				t.xindex = indices.first;
				t.yindex = indices.second;
				t.xmin = pc.buttons[t.xindex].rmin;
				t.xmax = pc.buttons[t.xindex].rmax;
				t.xcenter = pc.buttons[t.xindex].centers;
				t.ymin = 0;
				t.ymax = 1;
				t.ycenter = false;
				t.type = pc.buttons[t.xindex].type;
				t.name = pc.buttons[t.xindex].name;
				t.logical = cnum_g;
				if(t.yindex != std::numeric_limits<unsigned>::max()) {
					t.ymin = pc.buttons[t.yindex].rmin;
					t.ymax = pc.buttons[t.yindex].rmax;
					t.ycenter = pc.buttons[t.yindex].centers;
					t.name = t.name + "/" + pc.buttons[t.yindex].name;
				}
				_inputs.push_back(t);
			}
			//Go through all buttons.
			for(unsigned k = 0; k < pc.buttons.size(); k++) {
				const port_controller_button& pcb = pc.buttons[k];
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
			for(unsigned k = 0; k < pc.buttons.size(); k++) {
				const port_controller_button& pcb = pc.buttons[k];
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
	//Connect the keyboard.
	connect_keyboard_recursive(this);
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

void wxeditor_tasinput::on_keyboard_down(wxKeyEvent& e)
{
	handle_wx_keyboard(e, true);
}

void wxeditor_tasinput::on_keyboard_up(wxKeyEvent& e)
{
	handle_wx_keyboard(e, false);
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

void wxwindow_tasinput_update()
{
	if(tasinput_open)
		tasinput_open->call_screen_update();
}