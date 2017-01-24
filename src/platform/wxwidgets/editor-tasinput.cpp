#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>
#include <wx/statline.h>
#include <wx/spinctrl.h>

#include "core/command.hpp"
#include "core/controller.hpp"
#include "core/framebuffer.hpp"
#include "core/instance.hpp"
#include "core/instance-map.hpp"
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
extern "C"
{
#ifndef UINT64_C
#define UINT64_C(val) val##ULL
#endif
#include <libswscale/swscale.h>
}

namespace
{
	const char* button_codes = "QWERTYUIOPASDFGHJKLZXCVBNM";

	std::string get_shorthand(int index) {
		if(index < 0 || index > (int)strlen(button_codes))
			return "";
		return std::string(" [") + std::string(1, button_codes[index]) + "]";
	}

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
		return 0; //NOTREACHED.
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
		return 0; //NOTREACHED.
	}
}

class wxeditor_tasinput : public wxDialog
{
public:
	wxeditor_tasinput(emulator_instance& _inst, wxWindow* parent);
	~wxeditor_tasinput() throw();
	bool ShouldPreventAppExit() const;
	void on_wclose(wxCloseEvent& e);
	void on_control(wxCommandEvent& e);
	void on_keyboard_up(wxKeyEvent& e);
	void on_keyboard_down(wxKeyEvent& e);
	void call_screen_update();
private:
	struct dispatch::target<> ahreconfigure;
	struct xypanel;
	struct control_triple
	{
		unsigned port;
		unsigned controller;
		unsigned xindex;
		unsigned yindex;	//Used for XY.
		enum portctrl::button::_type type;
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
		xypanel(wxWindow* win, emulator_instance& _inst, wxSizer* s, control_triple _t, wxEvtHandler* _obj,
			wxObjectEventFunction _fun, int _wxid);
		~xypanel();
		short get_x() { return x; }
		short get_y() { return y; }
		void on_click(wxMouseEvent& e);
		void on_numbers_change(wxSpinEvent& e);
		void on_paint(wxPaintEvent& e);
		void Destroy();
		void do_redraw();
	private:
		friend class wxeditor_tasinput;
		emulator_instance& inst;
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
		int xstep, ystep;
	};
	emulator_instance& inst;
	std::map<int, control_triple> inputs;
	std::vector<controller_double> panels;
	void update_controls();
	void connect_keyboard_recursive(wxWindow* win);
	control_triple* find_triple(unsigned controller, unsigned control);
	bool closing;
	unsigned current_controller;
	unsigned current_button;
	wxBoxSizer* hsizer;
};

namespace
{
	instance_map<wxeditor_tasinput> tasinputs;
}

wxeditor_tasinput::xypanel::xypanel(wxWindow* win, emulator_instance& _inst, wxSizer* s, control_triple _t,
	wxEvtHandler* _obj, wxObjectEventFunction _fun, int _wxid)
	: inst(_inst)
{
	CHECK_UI_THREAD;
	x = 0;
	y = 0;
	xnum = NULL;
	ynum = NULL;
	rctx = NULL;
	xstep = 1;
	ystep = 1;
	dirty = false;
	obj = _obj;
	fun = _fun;
	wxid = _wxid;
	t = _t;
	s->Add(new wxStaticText(win, wxID_ANY, towxstring(t.name), wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS));
	s->Add(graphics = new wxPanel(win, wxID_ANY));
	lightgun = false;
	if(t.type == portctrl::button::TYPE_LIGHTGUN && t.yindex != std::numeric_limits<unsigned>::max()) {
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
			wxSP_ARROW_KEYS | wxWANTS_CHARS, t.ymin, t.ymax, y));
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
		if(i.second.type == portctrl::button::TYPE_LIGHTGUN)
			i.second.panel->do_redraw();
}

void wxeditor_tasinput::xypanel::on_click(wxMouseEvent& e)
{
	CHECK_UI_THREAD;
	if(!e.Dragging() && !e.LeftDown())
		return;
	wxCommandEvent e2(0, wxid);
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
	CHECK_UI_THREAD;
	wxCommandEvent e2(0, wxid);
	if(xnum) x = xnum->GetValue();
	if(ynum) y = ynum->GetValue();
	do_redraw();
	(obj->*fun)(e2);
}

void wxeditor_tasinput::xypanel::do_redraw()
{
	CHECK_UI_THREAD;
	if(!dirty) {
		dirty = true;
		graphics->Refresh();
	}
}

void wxeditor_tasinput::xypanel::on_paint(wxPaintEvent& e)
{
	CHECK_UI_THREAD;
	wxPaintDC dc(graphics);
	if(lightgun) {
		//Draw the current screen.
		framebuffer::raw& _fb = inst.fbuf->render_get_latest_screen();
		framebuffer::fb<false> fb;
		auto osize = std::make_pair(_fb.get_width(), _fb.get_height());
		auto size = inst.rom->lightgun_scale();
		fb.reallocate(osize.first, osize.second, false);
		fb.copy_from(_fb, 1, 1);
		inst.fbuf->render_get_latest_screen_end();
		std::vector<uint8_t> buf;
		buf.resize(3 * (t.xmax - t.xmin + 1) * (t.ymax - t.ymin + 1));
		unsigned offX = -t.xmin;
		unsigned offY = -t.ymin;
		rctx = sws_getCachedContext(rctx, osize.first, osize.second, AV_PIX_FMT_RGBA,
			size.first, size.second, AV_PIX_FMT_BGR24, SWS_POINT, NULL, NULL, NULL);
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
	CHECK_UI_THREAD;
	graphics->Destroy();
	xnum->Destroy();
	if(ynum) ynum->Destroy();
}

wxeditor_tasinput::~wxeditor_tasinput() throw()
{
	tasinputs.remove(this->inst);
}

wxeditor_tasinput::wxeditor_tasinput(emulator_instance& _inst, wxWindow* parent)
	: wxDialog(parent, wxID_ANY, wxT("lsnes: TAS input plugin"), wxDefaultPosition, wxSize(-1, -1)),
	inst(_inst)
{
	CHECK_UI_THREAD;
	current_controller = 0;
	current_button = 0;
	closing = false;
	Centre();
	hsizer = new wxBoxSizer(wxHORIZONTAL);
	SetSizer(hsizer);
	Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(wxeditor_tasinput::on_wclose));
	update_controls();
	hsizer->SetSizeHints(this);
	Fit();

	ahreconfigure.set(inst.dispatch->autohold_reconfigure, [this]() {
		runuifun([this]() {
			try {
				this->update_controls();
			} catch(std::runtime_error& e) {
				//Close the window.
				bool wasc = closing;
				closing = true;
				inst.controls->tasinput_enable(false);
				if(!wasc)
					Destroy();
			}
		});
	});
}

void wxeditor_tasinput::connect_keyboard_recursive(wxWindow* win)
{
	CHECK_UI_THREAD;
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
	CHECK_UI_THREAD;
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
	inst.iqueue->run_async([t, xstate, ystate]() {
		CORE().controls->tasinput(t.port, t.controller, t.xindex, xstate);
		if(t.yindex != std::numeric_limits<unsigned>::max())
			CORE().controls->tasinput(t.port, t.controller, t.yindex, ystate);
	}, [](std::exception& e) {});
}

void wxeditor_tasinput::update_controls()
{
	CHECK_UI_THREAD;
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
	inst.iqueue->run([&_inputs, &_controller_labels](){
		std::map<std::string, unsigned> next_in_class;
		portctrl::frame model = CORE().controls->get_blank();
		const portctrl::type_set& pts = model.porttypes();
		unsigned cnum_g = 0;
		for(unsigned i = 0;; i++) {
			auto pcid = CORE().controls->lcid_to_pcid(i);
			if(pcid.first < 0)
				break;
			const portctrl::type& pt = pts.port_type(pcid.first);
			const portctrl::controller_set& pci = *(pt.controller_info);
			if((ssize_t)pci.controllers.size() <= pcid.second)
				continue;
			const portctrl::controller& pc = pci.controllers[pcid.second];
			//First check that this has non-hidden stuff.
			bool has_buttons = false;
			for(unsigned k = 0; k < pc.buttons.size(); k++) {
				const portctrl::button& pcb = pc.buttons[k];
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
				const portctrl::button& pcb = pc.buttons[k];
				if(pcb.type != portctrl::button::TYPE_BUTTON || pcb.shadow)
					continue;
				struct control_triple t;
				t.port = pcid.first;
				t.controller = pcid.second;
				t.xindex = k;
				t.yindex = std::numeric_limits<unsigned>::max();
				t.logical = cnum_g;
				t.type = portctrl::button::TYPE_BUTTON;
				t.name = pcb.name;
				_inputs.push_back(t);
			}
			for(unsigned k = 0; k < pc.buttons.size(); k++) {
				const portctrl::button& pcb = pc.buttons[k];
				if(pcb.type == portctrl::button::TYPE_BUTTON || pcb.shadow)
					continue;
				CORE().controls->tasinput(pcid.first, pcid.second, k, pcb.centers ? ((int)pcb.rmin +
					pcb.rmax) / 2 : pcb.rmin);
			}
			cnum_g++;
		}
	});
	int next_id = wxID_HIGHEST + 1;
	unsigned last_logical = 0xFFFFFFFFUL;
	wxSizer* current = NULL;
	wxPanel* current_p = NULL;
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
		if(i.type == portctrl::button::TYPE_BUTTON) {
			t.check = new wxCheckBox(current_p, next_id, towxstring(i.name + get_shorthand(i.xindex)),
				wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS);
			current->Add(t.check);
			t.check->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED,
				wxCommandEventHandler(wxeditor_tasinput::on_control), NULL, this);
		} else {
			t.panel = new xypanel(current_p, inst, current, i, this,
				wxCommandEventHandler(wxeditor_tasinput::on_control), next_id);
		}
		inputs[next_id++] = t;
	}
	if(_inputs.empty()) {
		throw std::runtime_error("No controlers");
	}
	auto tx = find_triple(current_controller = 0, current_button = 0);
	if(tx) {
		if(tx->check) tx->check->SetFocus();
		else tx->panel->xnum->SetFocus();
	}
	//Connect the keyboard.
	hsizer->Layout();
	connect_keyboard_recursive(this);
	Fit();
}

bool wxeditor_tasinput::ShouldPreventAppExit() const { return false; }

void wxeditor_tasinput::on_wclose(wxCloseEvent& e)
{
	CHECK_UI_THREAD;
	bool wasc = closing;
	closing = true;
	inst.controls->tasinput_enable(false);
	if(!wasc)
		Destroy();
}

void wxeditor_tasinput::on_keyboard_down(wxKeyEvent& e)
{
	CHECK_UI_THREAD;
	int key = e.GetKeyCode();
	if(key == WXK_LEFT || key == WXK_RIGHT || key == WXK_UP || key == WXK_DOWN) {
		//See if this is associated with a panel.
		int delta = 1;
		if(e.GetModifiers() & wxMOD_SHIFT) delta = 999999998;
		if(e.GetModifiers() & wxMOD_CONTROL) delta = 999999999;
		if(key == WXK_LEFT || key == WXK_UP) delta = -delta;
		bool vertical = (key == WXK_UP || key == WXK_DOWN);
		auto t = find_triple(current_controller, current_button);
		if(t->panel) {
			//Handle movement better if span is large.
			auto ctrl = vertical ? t->panel->ynum : t->panel->xnum;
			if(!ctrl)
				return;
			if(abs(delta) == 999999998) {
				int& wstep = vertical ? t->panel->ystep : t->panel->xstep;
				int range = ctrl->GetMax() - ctrl->GetMin();
				delta = (delta / abs(delta)) * wstep;
				wstep += sqrt(wstep);
				if(wstep > range / 20)
					wstep = range / 20;
			}
			if(abs(delta) == 999999999) {
				int range = ctrl->GetMax() - ctrl->GetMin();
				delta = (delta / abs(delta)) * range / 16;
			}
			ctrl->SetValue(min(max(ctrl->GetValue() + delta, ctrl->GetMin()), ctrl->GetMax()));
			wxSpinEvent e(wxEVT_COMMAND_SPINCTRL_UPDATED, ctrl->GetId());
			e.SetPosition(ctrl->GetValue());
			t->panel->on_numbers_change(e);
		}
		return;
	}
	if(key == WXK_F5) inst.iqueue->queue("+advance-frame");
}

void wxeditor_tasinput::on_keyboard_up(wxKeyEvent& e)
{
	CHECK_UI_THREAD;
	int key = e.GetKeyCode();
	if(key == WXK_LEFT || key == WXK_RIGHT) {
		auto t = find_triple(current_controller, current_button);
		if(t && t->panel) t->panel->xstep = 1;
	}
	if(key == WXK_UP || key == WXK_DOWN) {
		auto t = find_triple(current_controller, current_button);
		if(t && t->panel) t->panel->ystep = 1;
	}
	if(key == WXK_TAB) {
		//Reset speed.
		auto t = find_triple(current_controller, current_button);
		if(t && t->panel) t->panel->xstep = t->panel->ystep = 1;

		if(e.GetModifiers() & wxMOD_SHIFT) {
			if(current_controller)
				current_controller--;
			else
				current_controller = panels.size() - 1;
		} else {
			current_controller++;
			if(current_controller >= panels.size())
				current_controller = 0;
		}
		//Hilight zero control (but don't change). If it is a panel, it is X of it.
		current_button = 0;
		t = find_triple(current_controller, 0);
		if(t) {
			if(t->check)
				t->check->SetFocus();
			else
				t->panel->xnum->SetFocus();
		}
		return;
	}
	for(const char* b = button_codes; *b; b++) {
		if(key == *b) {
			//Reset speed.
			auto t = find_triple(current_controller, current_button);
			if(t && t->panel) t->panel->xstep = t->panel->ystep = 1;

			unsigned bn = b - button_codes;
			//Select (current_controller,bn).
			t = find_triple(current_controller, bn);
			if(!t) return;
			if(t->check) {
				//Focus and toggle the checkbox.
				t->check->SetFocus();
				t->check->SetValue(!t->check->GetValue());
				//Emit fake event.
				wxCommandEvent e(wxEVT_COMMAND_CHECKBOX_CLICKED, t->check->GetId());
				on_control(e);
				current_button = bn;
				return;
			} else if(bn == t->xindex) {
				//Focus the associated X box.
				t->panel->xnum->SetFocus();
				current_button = bn;
				return;
			} else if(bn == t->yindex) {
				//Focus the associated Y box.
				t->panel->ynum->SetFocus();
				current_button = bn;
				return;
			}
			return;
		}
	}
	if(key == '\b') {
		auto t = find_triple(current_controller, current_button);
		if(t && t->panel) {
			//Zero this.
			auto ctrl = t->panel->ynum;
			if(ctrl)
				ctrl->SetValue(min(max(0, ctrl->GetMin()), ctrl->GetMax()));
			ctrl = t->panel->xnum;
			ctrl->SetValue(min(max(0, ctrl->GetMin()), ctrl->GetMax()));
			wxSpinEvent e(wxEVT_COMMAND_SPINCTRL_UPDATED, ctrl->GetId());
			e.SetPosition(0);
			t->panel->on_numbers_change(e);
		}
	}
	if(key == ' ') {
		//Toggle button.
		auto t = find_triple(current_controller, current_button);
		if(t && t->check) {
			t->check->SetValue(!t->check->GetValue());
			wxCommandEvent e(wxEVT_COMMAND_CHECKBOX_CLICKED, t->check->GetId());
			on_control(e);
		}
		return;
	}
	if(key == WXK_RETURN) {
		try {
			auto t = find_triple(current_controller, current_button);
			if(!t || !t->panel) return;
			bool vertical = (current_button == t->yindex);
			auto ctrl = vertical ? t->panel->ynum : t->panel->xnum;
			std::string v = pick_text(this, "Enter coordinate", "Enter new coordinate value",
				(stringfmt() << ctrl->GetValue()).str(), false);
			int val = parse_value<int>(v);
			ctrl->SetValue(min(max(val, ctrl->GetMin()), ctrl->GetMax()));
			wxSpinEvent e(wxEVT_COMMAND_SPINCTRL_UPDATED, ctrl->GetId());
			e.SetPosition(ctrl->GetValue());
			t->panel->on_numbers_change(e);
		} catch(...) {
			return;
		}
	}
	if(key == WXK_F1) inst.iqueue->queue("cycle-jukebox-backward");
	if(key == WXK_F2) inst.iqueue->queue("cycle-jukebox-forward");
	if(key == WXK_F3) inst.iqueue->queue("save-jukebox");
	if(key == WXK_F4) inst.iqueue->queue("load-jukebox");
	if(key == WXK_F5) inst.iqueue->queue("-advance-frame");
}

wxeditor_tasinput::control_triple* wxeditor_tasinput::find_triple(unsigned controller, unsigned control)
{
	for(auto& i : inputs) {
		if(i.second.logical != controller)
			continue;
		if(i.second.xindex == control)
			return &i.second;
		if(i.second.yindex == control)
			return &i.second;
	}
	return NULL;
}

void wxeditor_tasinput_display(wxWindow* parent, emulator_instance& inst)
{
	CHECK_UI_THREAD;
	auto e = tasinputs.lookup(inst);
	if(e) {
		e->Raise();
		return;
	}
	wxeditor_tasinput* v;
	try {
		v = tasinputs.create(inst, parent);
	} catch(std::runtime_error& e) {
		wxMessageBox(_T("No controllers present"), _T("Error"), wxICON_EXCLAMATION | wxOK, parent);
		return;
	}
	v->Show();
	inst.controls->tasinput_enable(true);
}

void wxwindow_tasinput_update(emulator_instance& inst)
{
	auto e = tasinputs.lookup(inst);
	if(e) e->call_screen_update();
}
