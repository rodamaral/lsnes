#include "core/movie.hpp"
#include "core/moviedata.hpp"
#include "core/dispatch.hpp"
#include "core/emucore.hpp"
#include "core/window.hpp"

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

enum
{
	wxID_TOGGLE = wxID_HIGHEST + 1,
	wxID_CHANGE,
	wxID_SWEEP,
	wxID_APPEND_FRAME,
	wxID_CHANGE_LINECOUNT,
	wxID_INSERT_AFTER,
	wxID_DELETE_FRAME,
	wxID_DELETE_SUBFRAME,
	wxID_POSITION_LOCK,
	wxID_RUN_TO_FRAME,
	wxID_APPEND_FRAMES,
	wxID_TRUNCATE,
	wxID_SCROLL_FRAME,
	wxID_SCROLL_CURRENT_FRAME
};

void update_movie_state();

namespace
{
	unsigned lines_to_display = 28;
	uint64_t divs[] = {1000000, 100000, 10000, 1000, 100, 10, 1};
	uint64_t divsl[] = {1000000, 100000, 10000, 1000, 100, 10, 0};
	const unsigned divcnt = sizeof(divs)/sizeof(divs[0]);

	void connect_events(wxScrollBar* s, wxObjectEventFunction fun, wxEvtHandler* obj)
	{
		s->Connect(wxEVT_SCROLL_THUMBTRACK, fun, NULL, obj);
		s->Connect(wxEVT_SCROLL_PAGEDOWN, fun, NULL, obj);
		s->Connect(wxEVT_SCROLL_PAGEUP, fun, NULL, obj);
		s->Connect(wxEVT_SCROLL_LINEDOWN, fun, NULL, obj);
		s->Connect(wxEVT_SCROLL_LINEUP, fun, NULL, obj);
		s->Connect(wxEVT_SCROLL_TOP, fun, NULL, obj);
		s->Connect(wxEVT_SCROLL_BOTTOM, fun, NULL, obj);
	}
}

struct control_info
{
	unsigned position_left;
	unsigned reserved;	//Must be at least 6 for axes.
	unsigned index;		//Index in poll vector.
	int type;		//-2 => Port, -1 => Fixed, 0 => Button, 1 => axis.
	char ch;
	std::u32string title;
	unsigned port;
	unsigned controller;
	static control_info portinfo(unsigned& p, unsigned port, unsigned controller);
	static control_info fixedinfo(unsigned& p, const std::u32string& str);
	static control_info buttoninfo(unsigned& p, char character, const std::u32string& title, unsigned idx);
	static control_info axisinfo(unsigned& p, const std::u32string& title, unsigned idx);
};

control_info control_info::portinfo(unsigned& p, unsigned port, unsigned controller)
{
	control_info i;
	i.position_left = p;
	i.reserved = (stringfmt() << port << "-" << controller).str32().length();
	p += i.reserved;
	i.index = 0;
	i.type = -2;
	i.ch = 0;
	i.title = U"";
	i.port = port;
	i.controller = controller;
	return i;
}

control_info control_info::fixedinfo(unsigned& p, const std::u32string& str)
{
	control_info i;
	i.position_left = p;
	i.reserved = str.length();
	p += i.reserved;
	i.index = 0;
	i.type = -1;
	i.ch = 0;
	i.title = str;
	i.port = 0;
	i.controller = 0;
	return i;
}

control_info control_info::buttoninfo(unsigned& p, char character, const std::u32string& title, unsigned idx)
{
	control_info i;
	i.position_left = p;
	i.reserved = 1;
	p += i.reserved;
	i.index = idx;
	i.type = 0;
	i.ch = character;
	i.title = title;
	i.port = 0;
	i.controller = 0;
	return i;
}

control_info control_info::axisinfo(unsigned& p, const std::u32string& title, unsigned idx)
{
	control_info i;
	i.position_left = p;
	i.reserved = title.length();
	if(i.reserved < 6)
		i.reserved = 6;
	p += i.reserved;
	i.index = idx;
	i.type = 1;
	i.ch = 0;
	i.title = title;
	i.port = 0;
	i.controller = 0;
	return i;
}

class frame_controls
{
public:
	frame_controls();
	void set_types(controller_frame& f);
	short read_index(controller_frame& f, unsigned idx);
	void write_index(controller_frame& f, unsigned idx, short value);
	uint32_t read_pollcount(pollcounter_vector& v, unsigned idx);
	const std::list<control_info>& get_controlinfo() { return controlinfo; }
	std::u32string line1() { return _line1; }
	std::u32string line2() { return _line2; }
	size_t width() { return _width; }
private:
	size_t _width;
	std::u32string _line1;
	std::u32string _line2;
	void format_lines();
	void add_port(unsigned& c, unsigned pid, porttype_info& p);
	std::list<control_info> controlinfo;
};


frame_controls::frame_controls()
{
	_width = 0;
}

void frame_controls::set_types(controller_frame& f)
{
	unsigned nextp = 0;
	unsigned nextc = 0;
	controlinfo.clear();
	controlinfo.push_back(control_info::portinfo(nextp, 0, 0));
	controlinfo.push_back(control_info::buttoninfo(nextc, 'F', U"Framesync", 0));
	controlinfo.push_back(control_info::buttoninfo(nextc, 'R', U"Reset", 1));
	nextc++;
	controlinfo.push_back(control_info::axisinfo(nextc, U" rhigh", 2));
	nextc++;
	controlinfo.push_back(control_info::axisinfo(nextc, U"  rlow", 3));
	if(nextp > nextc)
		nextc = nextp;
	nextp = nextc;
	add_port(nextp, 1, f.get_port_type(0));
	add_port(nextp, 2, f.get_port_type(1));
	format_lines();
}

void frame_controls::add_port(unsigned& c, unsigned pid, porttype_info& p)
{
	unsigned i = 0;
	unsigned ccount = MAX_CONTROLLERS_PER_PORT * MAX_CONTROLS_PER_CONTROLLER;
	auto limits = get_core_logical_controller_limits();
	while(p.is_present(i)) {
		controlinfo.push_back(control_info::fixedinfo(c, U"\u2502"));
		unsigned nextp = c;
		controlinfo.push_back(control_info::portinfo(nextp, pid, i + 1));
		unsigned b = 0;
		if(p.is_analog(i)) {
			controlinfo.push_back(control_info::axisinfo(c, U" xaxis", 4 + ccount * pid + i *
				MAX_CONTROLS_PER_CONTROLLER - ccount));
			c++;
			controlinfo.push_back(control_info::axisinfo(c, U" yaxis", 5 + ccount * pid + i *
				MAX_CONTROLS_PER_CONTROLLER - ccount));
			if(p.button_symbols[0])
				c++;
			b = 2;
		}
		for(unsigned j = 0; p.button_symbols[j]; j++, b++) {
			unsigned lbid;
			for(lbid = 0; lbid < limits.second; lbid++)
				if(p.button_id(i, lbid) == b)
					break;
			if(lbid == limits.second)
				lbid = 0;
			std::u32string name = to_u32string(get_logical_button_name(lbid));
			controlinfo.push_back(control_info::buttoninfo(c, p.button_symbols[j], name, 4 + b + ccount *
				pid + i * MAX_CONTROLS_PER_CONTROLLER - ccount));
		}
		if(nextp > c)
			c = nextp;
		i++;
	}
}

short frame_controls::read_index(controller_frame& f, unsigned idx)
{
	if(idx == 0)
		return f.sync() ? 1 : 0;
	if(idx == 1)
		return f.reset() ? 1 : 0;
	if(idx == 2)
		return f.delay().first;
	if(idx == 3)
		return f.delay().second;
	return f.axis2(idx - 4);
}

void frame_controls::write_index(controller_frame& f, unsigned idx, short value)
{
	if(idx == 0)
		return f.sync(value);
	if(idx == 1)
		return f.reset(value);
	if(idx == 2)
		return f.delay(std::make_pair(value, f.delay().second));
	if(idx == 3)
		return f.delay(std::make_pair(f.delay().first, value));
	return f.axis2(idx - 4, value);
}

uint32_t frame_controls::read_pollcount(pollcounter_vector& v, unsigned idx)
{
	if(idx == 0)
		return max(v.max_polls(), (uint32_t)1);
	if(idx < 4)
		return v.get_system() ? 1 : 0;
	return v.get_polls(idx - 4);
}

void frame_controls::format_lines()
{
	_width = 0;
	for(auto i : controlinfo) {
		if(i.position_left + i.reserved > _width)
			_width = i.position_left + i.reserved;
	}
	std::u32string cp1;
	std::u32string cp2;
	uint32_t off = divcnt + 1;
	cp1.resize(_width + divcnt + 1);
	cp2.resize(_width + divcnt + 1);
	for(unsigned i = 0; i < cp1.size(); i++)
		cp1[i] = cp2[i] = 32;
	cp1[divcnt] = 0x2502;
	cp2[divcnt] = 0x2502;
	//Line1
	//For every port-controller, find the least coordinate.
	for(auto i : controlinfo) {
		if(i.type == -1) {
			auto _title = i.title;
			std::copy(_title.begin(), _title.end(), &cp1[i.position_left + off]);
		} else if(i.type == -2) {
			auto _title = (stringfmt() << i.port << "-" << i.controller).str32();
			std::copy(_title.begin(), _title.end(), &cp1[i.position_left + off]);
		}
	}
	//Line2
	for(auto i : controlinfo) {
		auto _title = i.title;
		if(i.type == -1 || i.type == 1)
			std::copy(_title.begin(), _title.end(), &cp2[i.position_left + off]);
		if(i.type == 0)
			cp2[i.position_left + off] = i.ch;
	}
	_line1 = cp1;
	_line2 = cp2;
}


class wxeditor_movie : public wxDialog
{
public:
	wxeditor_movie(wxWindow* parent);
	~wxeditor_movie() throw();
	bool ShouldPreventAppExit() const;
	void on_close(wxCommandEvent& e);
	void on_wclose(wxCloseEvent& e);
	void on_focus_wrong(wxFocusEvent& e);
	void on_keyboard_down(wxKeyEvent& e);
	void on_keyboard_up(wxKeyEvent& e);
	wxScrollBar* get_scroll();
	void update();
private:
	struct _moviepanel : public wxPanel, public information_dispatch
	{
		_moviepanel(wxeditor_movie* v);
		~_moviepanel() throw();
		void signal_repaint();
		void on_scroll(wxScrollEvent& e);
		void on_paint(wxPaintEvent& e);
		void on_erase(wxEraseEvent& e);
		void on_mouse(wxMouseEvent& e);
		void on_popup_menu(wxCommandEvent& e);
	private:
		int get_lines();
		void render(text_framebuffer& fb, unsigned long long pos);
		void on_mouse0(unsigned x, unsigned y, bool polarity);
		void on_mouse1(unsigned x, unsigned y, bool polarity);
		void on_mouse2(unsigned x, unsigned y, bool polarity);
		void do_toggle_buttons(unsigned idx, uint64_t row1, uint64_t row2);
		void do_alter_axis(unsigned idx, uint64_t row1, uint64_t row2);
		void do_sweep_axis(unsigned idx, uint64_t row1, uint64_t row2);
		void do_append_frames(uint64_t count);
		void do_append_frames();
		void do_insert_frame_after(uint64_t row);
		void do_delete_frame(uint64_t row, bool wholeframe);
		void do_truncate(uint64_t row);
		void do_set_stop_at_frame();
		void do_scroll_to_frame();
		void do_scroll_to_current_frame();
		uint64_t first_editable(unsigned index);
		uint64_t first_nextframe();
		int width(controller_frame& f);
		std::u32string render_line1(controller_frame& f);
		std::u32string render_line2(controller_frame& f);
		void render_linen(text_framebuffer& fb, controller_frame& f, uint64_t sfn, int y);
		unsigned long long spos;
		void* prev_obj;
		uint64_t prev_seqno;
		void update_cache();
		std::map<uint64_t, uint64_t> subframe_to_frame;
		uint64_t max_subframe;
		frame_controls fcontrols;
		wxeditor_movie* m;
		bool requested;
		text_framebuffer fb;
		uint64_t movielines;
		uint64_t moviepos;
		unsigned new_width;
		unsigned new_height;
		std::vector<uint8_t> pixels;
		int scroll_delta;
		unsigned press_x;
		uint64_t press_line;
		uint64_t rpress_line;
		unsigned press_index;
		bool pressed;
		bool recursing;
		uint64_t linecount;
		uint64_t cached_cffs;
		bool position_locked;
		wxMenu* current_popup;
	};
	_moviepanel* moviepanel;
	wxButton* closebutton;
	wxScrollBar* moviescroll;
	bool closing;
};

namespace
{
	wxeditor_movie* movieeditor_open;

	//Find the first real editable subframe.
	//Call only in emulator thread.
	uint64_t real_first_editable(frame_controls& fc, unsigned idx)
	{
		uint64_t cffs = movb.get_movie().get_current_frame_first_subframe();
		controller_frame_vector& fv = movb.get_movie().get_frame_vector();
		pollcounter_vector& pv = movb.get_movie().get_pollcounters();
		uint64_t vsize = fv.size();
		uint32_t pc = fc.read_pollcount(pv, idx);
		for(uint32_t i = 1; i < pc; i++)
			if(cffs + i >= vsize || fv[cffs + i].sync())
				return cffs + i;
		return cffs + pc;
	}

	//Find the first real editable whole frame.
	//Call only in emulator thread.
	uint64_t real_first_nextframe(frame_controls& fc)
	{
		uint64_t base = real_first_editable(fc, 0);
		controller_frame_vector& fv = movb.get_movie().get_frame_vector();
		uint64_t vsize = fv.size();
		for(uint32_t i = 0;; i++)
			if(base + i >= vsize || fv[base + i].sync())
				return base + i;
	}

	//Adjust movie length by specified number of frames.
	//Call only in emulator thread.
	void movie_framecount_change(int64_t adjust, bool known = true);
	void movie_framecount_change(int64_t adjust, bool known)
	{
		if(known)
			movb.get_movie().adjust_frame_count(adjust);
		else
			movb.get_movie().recount_frames();
		update_movie_state();
		graphics_plugin::notify_status();
	}
}

wxeditor_movie::_moviepanel::~_moviepanel() throw() {}
wxeditor_movie::~wxeditor_movie() throw() {}

wxeditor_movie::_moviepanel::_moviepanel(wxeditor_movie* v)
	: wxPanel(v, wxID_ANY, wxDefaultPosition, wxSize(100, 100), wxWANTS_CHARS),
	information_dispatch("movieeditor-listener")
{
	m = v;
	Connect(wxEVT_PAINT, wxPaintEventHandler(_moviepanel::on_paint), NULL, this);
	Connect(wxEVT_ERASE_BACKGROUND, wxEraseEventHandler(_moviepanel::on_erase), NULL, this);
	new_width = 0;
	new_height = 0;
	moviepos = 0;
	scroll_delta = 0;
	spos = 0;
	prev_obj = NULL;
	prev_seqno = 0;
	max_subframe = 0;
	recursing = false;
	position_locked = true;
	current_popup = NULL;

	Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(_moviepanel::on_mouse), NULL, this);
	Connect(wxEVT_LEFT_UP, wxMouseEventHandler(_moviepanel::on_mouse), NULL, this);
	Connect(wxEVT_MIDDLE_DOWN, wxMouseEventHandler(_moviepanel::on_mouse), NULL, this);
	Connect(wxEVT_MIDDLE_UP, wxMouseEventHandler(_moviepanel::on_mouse), NULL, this);
	Connect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(_moviepanel::on_mouse), NULL, this);
	Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(_moviepanel::on_mouse), NULL, this);
	Connect(wxEVT_MOUSEWHEEL, wxMouseEventHandler(_moviepanel::on_mouse), NULL, this);

	signal_repaint();
	requested = false;
}

void wxeditor_movie::_moviepanel::update_cache()
{
	movie& m = movb.get_movie();
	controller_frame_vector& fv = m.get_frame_vector();
	if(&m == prev_obj && prev_seqno == m.get_seqno()) {
		//Just process new subframes if any.
		for(uint64_t i = max_subframe; i < fv.size(); i++) {
			uint64_t prev = (i > 0) ? subframe_to_frame[i - 1] : 0;
			controller_frame f = fv[i];
			if(f.sync())
				subframe_to_frame[i] = prev + 1;
			else
				subframe_to_frame[i] = prev;
		}
		max_subframe = fv.size();
		return;
	}
	//Reprocess all subframes.
	for(uint64_t i = 0; i < fv.size(); i++) {
		uint64_t prev = (i > 0) ? subframe_to_frame[i - 1] : 0;
		controller_frame f = fv[i];
		if(f.sync())
			subframe_to_frame[i] = prev + 1;
		else
			subframe_to_frame[i] = prev;
	}
	max_subframe = fv.size();
	controller_frame model = fv.blank_frame(false);
	fcontrols.set_types(model);
	prev_obj = &m;
	prev_seqno = m.get_seqno();
}

int wxeditor_movie::_moviepanel::width(controller_frame& f)
{
	update_cache();
	return divcnt + 1 + fcontrols.width();
}

std::u32string wxeditor_movie::_moviepanel::render_line1(controller_frame& f)
{
	update_cache();
	return fcontrols.line1();
}

std::u32string wxeditor_movie::_moviepanel::render_line2(controller_frame& f)
{
	update_cache();
	return fcontrols.line2();
}

void wxeditor_movie::_moviepanel::render_linen(text_framebuffer& fb, controller_frame& f, uint64_t sfn, int y)
{
	update_cache();
	size_t fbstride = fb.get_stride();
	text_framebuffer::element* _fb = fb.get_buffer();
	text_framebuffer::element e;
	e.bg = 0xFFFFFF;
	e.fg = 0x000000;
	for(unsigned i = 0; i < divcnt; i++) {
		uint64_t fn = subframe_to_frame[sfn];
		e.ch = (fn >= divsl[i]) ? (((fn / divs[i]) % 10) + 48) : 32;
		_fb[y * fbstride + i] = e;
	}
	e.ch = 0x2502;
	_fb[y * fbstride + divcnt] = e;
	const std::list<control_info>& ctrlinfo = fcontrols.get_controlinfo();
	uint64_t curframe = movb.get_movie().get_current_frame();
	pollcounter_vector& pv = movb.get_movie().get_pollcounters();
	uint64_t cffs = movb.get_movie().get_current_frame_first_subframe();
	cached_cffs = cffs;
	int past = -1;
	if(!movb.get_movie().readonly_mode())
		past = 1;
	else if(subframe_to_frame[sfn] < curframe)
		past = 1;
	else if(subframe_to_frame[sfn] > curframe)
		past = 0;
	bool now = (subframe_to_frame[sfn] == curframe);
	unsigned xcord = 32768;
	if(pressed)
		xcord = press_x;

	for(auto i : ctrlinfo) {
		int rpast = past;
		unsigned off = divcnt + 1;
		bool cselected = (xcord >= i.position_left + off && xcord < i.position_left + i.reserved + off);
		if(rpast == -1) {
			unsigned polls = fcontrols.read_pollcount(pv, i.index);
			rpast = ((cffs + polls) > sfn) ? 1 : 0;
		}
		uint32_t bgc = 0xC0C0C0;
		if(rpast)
			bgc |= 0x0000FF;
		if(now)
			bgc |= 0xFF0000;
		if(cselected)
			bgc |= 0x00FF00;
		if(bgc == 0xC0C0C0)
			bgc = 0xFFFFFF;
		if(i.type == -1) {
			//Separator.
			fb.write(i.title, 0, divcnt + 1 + i.position_left, y, 0x000000, 0xFFFFFF);
		} else if(i.type == 0) {
			//Button.
			char32_t c[2];
			bool v = (fcontrols.read_index(f, i.index) != 0);
			c[0] = i.ch;
			c[1] = 0;
			fb.write(c, 0, divcnt + 1 + i.position_left, y, v ? 0x000000 : 0xC8C8C8, bgc);
		} else if(i.type == 1) {
			//Axis.
			char c[7];
			sprintf(c, "%6d", fcontrols.read_index(f, i.index));
			fb.write(c, 0, divcnt + 1 + i.position_left, y, 0x000000, bgc);
		}
	}
}

void wxeditor_movie::_moviepanel::render(text_framebuffer& fb, unsigned long long pos)
{
	spos = pos;
	controller_frame_vector& fv = movb.get_movie().get_frame_vector();
	controller_frame cf = fv.blank_frame(false);
	int _width = width(cf);
	fb.set_size(_width, lines_to_display + 3);
	size_t fbstride = fb.get_stride();
	auto fbsize = fb.get_characters();
	text_framebuffer::element* _fb = fb.get_buffer();
	fb.write((stringfmt() << "Current frame: " << movb.get_movie().get_current_frame() << " of "
		<< movb.get_movie().get_frame_count()).str(), _width, 0, 0,
		 0x000000, 0xFFFFFF);
	fb.write(render_line1(cf), _width, 0, 1, 0x000000, 0xFFFFFF);
	fb.write(render_line2(cf), _width, 0, 2, 0x000000, 0xFFFFFF);
	unsigned long long lines = fv.size();
	unsigned long long i;
	unsigned j;
	for(i = pos, j = 3; i < pos + lines_to_display; i++, j++) {
		text_framebuffer::element e;
		if(i >= lines) {
			//Out of range.
			e.bg = 0xFFFFFF;
			e.fg = 0x000000;
			e.ch = 32;
			for(unsigned k = 0; k < fbsize.first; k++)
				_fb[j * fbstride + k] = e;
		} else {
			controller_frame frame = fv[i];
			render_linen(fb, frame, i, j);
		}
	}
}

void wxeditor_movie::_moviepanel::do_toggle_buttons(unsigned idx, uint64_t row1, uint64_t row2)
{
	frame_controls* _fcontrols = &fcontrols;
	uint64_t _press_line = row1;
	uint64_t line = row2;
	if(_press_line > line)
		std::swap(_press_line, line);
	recursing = true;
	runemufn([idx, _press_line, line, _fcontrols]() {
		int64_t adjust = 0;
		if(!movb.get_movie().readonly_mode())
			return;
		uint64_t fedit = real_first_editable(*_fcontrols, idx);
		controller_frame_vector& fv = movb.get_movie().get_frame_vector();
		for(uint64_t i = _press_line; i <= line; i++) {
			if(i < fedit || i >= fv.size())
				continue;
			controller_frame cf = fv[i];
			bool v = _fcontrols->read_index(cf, idx);
			_fcontrols->write_index(cf, idx, !v);
			adjust += (v ? -1 : 1);
		}
		if(idx == 0)
			movie_framecount_change(adjust);
	});
	recursing = false;
	if(idx == 0)
		max_subframe = _press_line;	//Reparse.
}

void wxeditor_movie::_moviepanel::do_alter_axis(unsigned idx, uint64_t row1, uint64_t row2)
{
	frame_controls* _fcontrols = &fcontrols;
	uint64_t line = row1;
	uint64_t line2 = row2;
	short value;
	bool valid = true;
	runemufn([idx, line, &value, _fcontrols, &valid]() {
		if(!movb.get_movie().readonly_mode()) {
			valid = false;
			return;
		}
		uint64_t fedit = real_first_editable(*_fcontrols, idx);
		controller_frame_vector& fv = movb.get_movie().get_frame_vector();
		if(line < fedit || line >= fv.size()) {
			valid = false;
			return;
		}
		controller_frame cf = fv[line];
		value = _fcontrols->read_index(cf, idx);
	});
	if(!valid)
		return;
	try {
		std::string text = pick_text(m, "Set value", "Enter new value:", (stringfmt() << value).str());
		value = parse_value<short>(text);
	} catch(canceled_exception& e) {
		return;
	} catch(std::exception& e) {
		wxMessageBox(wxT("Invalid value"), _T("Error"), wxICON_EXCLAMATION | wxOK, m);
		return;
	}
	if(line > line2)
		std::swap(line, line2);
	runemufn([idx, line, line2, value, _fcontrols]() {
		uint64_t fedit = real_first_editable(*_fcontrols, idx);
		controller_frame_vector& fv = movb.get_movie().get_frame_vector();
		for(uint64_t i = line; i <= line2; i++) {
			if(i < fedit || i >= fv.size())
				continue;
			controller_frame cf = fv[i];
			_fcontrols->write_index(cf, idx, value);
		}
	});
}

void wxeditor_movie::_moviepanel::do_sweep_axis(unsigned idx, uint64_t row1, uint64_t row2)
{
	frame_controls* _fcontrols = &fcontrols;
	uint64_t line = row1;
	uint64_t line2 = row2;
	short value;
	short value2;
	bool valid = true;
	if(line > line2)
		std::swap(line, line2);
	runemufn([idx, line, line2, &value, &value2, _fcontrols, &valid]() {
		if(!movb.get_movie().readonly_mode()) {
			valid = false;
			return;
		}
		uint64_t fedit = real_first_editable(*_fcontrols, idx);
		controller_frame_vector& fv = movb.get_movie().get_frame_vector();
		if(line2 < fedit || line2 >= fv.size()) {
			valid = false;
			return;
		}
		controller_frame cf = fv[line];
		value = _fcontrols->read_index(cf, idx);
		controller_frame cf2 = fv[line2];
		value2 = _fcontrols->read_index(cf2, idx);
	});
	if(!valid)
		return;
	runemufn([idx, line, line2, value, value2, _fcontrols]() {
		uint64_t fedit = real_first_editable(*_fcontrols, idx);
		controller_frame_vector& fv = movb.get_movie().get_frame_vector();
		for(uint64_t i = line + 1; i <= line2 - 1; i++) {
			if(i < fedit || i >= fv.size())
				continue;
			controller_frame cf = fv[i];
			auto tmp2 = static_cast<int64_t>(i - line) * (value2 - value) /
				static_cast<int64_t>(line2 - line);
			short tmp = value + tmp2;
			_fcontrols->write_index(cf, idx, tmp);
		}
	});
}

void wxeditor_movie::_moviepanel::do_append_frames(uint64_t count)
{
	recursing = true;
	uint64_t _count = count;
	runemufn([_count]() {
		if(!movb.get_movie().readonly_mode())
			return;
		controller_frame_vector& fv = movb.get_movie().get_frame_vector();
		for(uint64_t i = 0; i < _count; i++)
			fv.append(fv.blank_frame(true));
		movie_framecount_change(_count);
	});
	recursing = false;
}

void wxeditor_movie::_moviepanel::do_append_frames()
{
	uint64_t value;
	try {
		std::string text = pick_text(m, "Append frames", "Enter number of frames to append:", "");
		value = parse_value<uint64_t>(text);
	} catch(canceled_exception& e) {
		return;
	} catch(std::exception& e) {
		wxMessageBox(wxT("Invalid value"), _T("Error"), wxICON_EXCLAMATION | wxOK, m);
		return;
	}
	do_append_frames(value);
}

void wxeditor_movie::_moviepanel::do_insert_frame_after(uint64_t row)
{
	recursing = true;
	frame_controls* _fcontrols = &fcontrols;
	uint64_t _row = row;
	runemufn([_row, _fcontrols]() {
		if(!movb.get_movie().readonly_mode())
			return;
		controller_frame_vector& fv = movb.get_movie().get_frame_vector();
		uint64_t fedit = real_first_editable(*_fcontrols, 0);
		//Find the start of the next frame.
		uint64_t nframe = _row + 1;
		uint64_t vsize = fv.size();
		while(nframe < vsize && !fv[nframe].sync())
			nframe++;
		if(nframe < fedit)
			return;
		fv.append(fv.blank_frame(true));
		if(nframe < vsize) {
			//Okay, gotta copy all data after this point. nframe has to be at least 1.
			for(uint64_t i = vsize - 1; i >= nframe; i--)
				fv[i + 1] = fv[i];
			fv[nframe] = fv.blank_frame(true);
		}
		movie_framecount_change(1);
	});
	max_subframe = row;
	recursing = false;
}

void wxeditor_movie::_moviepanel::do_delete_frame(uint64_t row, bool wholeframe)
{
	recursing = true;
	uint64_t _row = row;
	bool _wholeframe = wholeframe;
	frame_controls* _fcontrols = &fcontrols;
	runemufn([_row, _wholeframe, _fcontrols]() {
		controller_frame_vector& fv = movb.get_movie().get_frame_vector();
		uint64_t vsize = fv.size();
		if(_row >= vsize)
			return;
		if(_wholeframe) {
			if(_row < real_first_nextframe(*_fcontrols))
				return;
			//Scan backwards for the first subframe of this frame and forwards for the last.
			uint64_t fsf = _row;
			uint64_t lsf = _row;
			if(fv[_row].sync())
				lsf++;		//Bump by one so it finds the end.
			while(fsf < vsize && !fv[fsf].sync())
				fsf--;
			while(lsf < vsize && !fv[lsf].sync())
				lsf++;
			uint64_t tonuke = lsf - fsf;
			//Nuke from fsf to lsf.
			for(uint64_t i = fsf; i < vsize - tonuke; i++)
				fv[i] = fv[i + tonuke];
			fv.resize(vsize - tonuke);
			movie_framecount_change(-1);
		} else {
			if(_row < real_first_editable(*_fcontrols, 0))
				return;
			//Is the nuked frame a first subframe?
			bool is_first = fv[_row].sync();
			//Nuke the subframe.
			for(uint64_t i = _row; i < vsize - 1; i++)
				fv[i] = fv[i + 1];
			fv.resize(vsize - 1);
			//Next subframe inherits the sync flag.
			if(is_first) {
				if(_row < vsize - 1 && !fv[_row].sync())
					fv[_row].sync(true);
				else
					movie_framecount_change(-1);
			}
		}
		
	});
	max_subframe = row;
	recursing = false;
}

void wxeditor_movie::_moviepanel::do_truncate(uint64_t row)
{
	recursing = true;
	uint64_t _row = row;
	frame_controls* _fcontrols = &fcontrols;
	runemufn([_row, _fcontrols]() {
		controller_frame_vector& fv = movb.get_movie().get_frame_vector();
		uint64_t vsize = fv.size();
		if(_row >= vsize)
			return;
		if(_row < real_first_editable(*_fcontrols, 0))
			return;
		int64_t delete_count = 0;
		for(uint64_t i = _row; i < vsize; i++)
			if(fv[i].sync())
				delete_count--;
		fv.resize(_row);
		movie_framecount_change(delete_count);
	});
	max_subframe = row;
	recursing = false;
}

void wxeditor_movie::_moviepanel::do_set_stop_at_frame()
{
	uint64_t curframe;
	uint64_t frame;
	runemufn([&curframe]() {
		curframe = movb.get_movie().get_current_frame();
	});
	try {
		std::string text = pick_text(m, "Frame", (stringfmt() << "Enter frame to stop at (currently at "
			<< curframe << "):").str(), "");
		frame = parse_value<uint64_t>(text);
	} catch(canceled_exception& e) {
		return;
	} catch(std::exception& e) {
		wxMessageBox(wxT("Invalid value"), _T("Error"), wxICON_EXCLAMATION | wxOK, m);
		return;
	}
	if(frame < curframe) {
		wxMessageBox(wxT("The movie is already past that point"), _T("Error"), wxICON_EXCLAMATION | wxOK, m);
		return;
	}
	runemufn([frame]() {
		set_stop_at_frame(frame);
	});
}

void wxeditor_movie::_moviepanel::on_mouse0(unsigned x, unsigned y, bool polarity)
{
	if(y < 3)
		return;
	if(polarity) {
		press_x = x;
		press_line = spos + y - 3;
	}
	pressed = polarity;
	if(polarity)
		return;
	uint64_t line = spos + y - 3;
	if(press_x < divcnt && x < divcnt) {
		//Press on frame count.
		uint64_t row1 = press_line;
		uint64_t row2 = line;
		if(row1 > row2)
			std::swap(row1, row2);
		do_append_frames(row2 - row1 + 1);
	}
	for(auto i : fcontrols.get_controlinfo()) {
		unsigned off = divcnt + 1;
		unsigned idx = i.index;
		if((press_x >= i.position_left + off && press_x < i.position_left + i.reserved + off) &&
			(x >= i.position_left + off && x < i.position_left + i.reserved + off)) {
			if(i.type == 0)
				do_toggle_buttons(idx, press_line, line);
			else if(i.type == 1)
				do_alter_axis(idx, press_line, line);
		}
	}
}

void wxeditor_movie::_moviepanel::do_scroll_to_frame()
{
	uint64_t frame;
	try {
		std::string text = pick_text(m, "Frame", (stringfmt() << "Enter frame to scroll to:").str(), "");
		frame = parse_value<uint64_t>(text);
	} catch(canceled_exception& e) {
		return;
	} catch(std::exception& e) {
		wxMessageBox(wxT("Invalid value"), _T("Error"), wxICON_EXCLAMATION | wxOK, m);
		return;
	}
	uint64_t wouldbe;
	uint64_t low = 0;
	uint64_t high = max_subframe;
	while(low < high) {
		wouldbe = (low + high) / 2;
		if(subframe_to_frame[wouldbe] < frame)
			low = wouldbe;
		else if(subframe_to_frame[wouldbe] > frame)
			high = wouldbe;
		else
			break;
	}
	while(wouldbe > 1 && subframe_to_frame[wouldbe - 1] == frame)
		wouldbe--;
	moviepos = wouldbe;
	signal_repaint();
}

void wxeditor_movie::_moviepanel::do_scroll_to_current_frame()
{
	moviepos = cached_cffs;
	signal_repaint();
}

void wxeditor_movie::_moviepanel::on_popup_menu(wxCommandEvent& e)
{
	wxMenuItem* tmpitem;
	int id = e.GetId();
	switch(id) {
	case wxID_TOGGLE:
		do_toggle_buttons(press_index, press_line, press_line);
		return;
	case wxID_CHANGE:
		do_alter_axis(press_index, press_line, press_line);
		return;
	case wxID_SWEEP:
		do_sweep_axis(press_index, rpress_line, press_line);
		return;
	case wxID_APPEND_FRAME:
		do_append_frames(1);
		return;
	case wxID_APPEND_FRAMES:
		do_append_frames();
		return;
	case wxID_INSERT_AFTER:
		do_insert_frame_after(press_line);
		return;
	case wxID_DELETE_FRAME:
		do_delete_frame(press_line, true);
		return;
	case wxID_DELETE_SUBFRAME:
		do_delete_frame(press_line, false);
		return;
	case wxID_TRUNCATE:
		do_truncate(press_line);
		return;
	case wxID_RUN_TO_FRAME:
		do_set_stop_at_frame();
		return;
	case wxID_SCROLL_FRAME:
		do_scroll_to_frame();
		return;
	case wxID_SCROLL_CURRENT_FRAME:
		do_scroll_to_current_frame();
		return;
	case wxID_POSITION_LOCK:
		if(!current_popup)
			return;
		tmpitem = current_popup->FindItem(wxID_POSITION_LOCK);
		position_locked = tmpitem->IsChecked();
		return;
	case wxID_CHANGE_LINECOUNT:
		try {
			std::string text = pick_text(m, "Set number of lines", "Set number of lines visible:",
				(stringfmt() << lines_to_display).str());
			unsigned tmp = parse_value<unsigned>(text);
			if(tmp < 1 || tmp > 255)
				throw std::runtime_error("Value out of range");
			lines_to_display = tmp;
		} catch(canceled_exception& e) {
			return;
		} catch(std::exception& e) {
			wxMessageBox(wxT("Invalid value"), _T("Error"), wxICON_EXCLAMATION | wxOK, m);
			return;
		}
		signal_repaint();
		return;
	};
}

uint64_t wxeditor_movie::_moviepanel::first_editable(unsigned index)
{
	uint64_t cffs = cached_cffs;
	if(!subframe_to_frame.count(cffs))
		return cffs;
	uint64_t f = subframe_to_frame[cffs];
	pollcounter_vector& pv = movb.get_movie().get_pollcounters();
	uint32_t pc = fcontrols.read_pollcount(pv, index);
	for(uint32_t i = 1; i < pc; i++)
		if(!subframe_to_frame.count(cffs + i) || subframe_to_frame[cffs + i] > f)
				return cffs + i;
	return cffs + pc;
}

uint64_t wxeditor_movie::_moviepanel::first_nextframe()
{
	uint64_t base = first_editable(0);
	if(!subframe_to_frame.count(cached_cffs))
		return cached_cffs;
	uint64_t f = subframe_to_frame[cached_cffs];
	for(uint32_t i = 0;; i++)
		if(!subframe_to_frame.count(base + i) || subframe_to_frame[base + i] > f)
			return base + i;
}

void wxeditor_movie::_moviepanel::on_mouse1(unsigned x, unsigned y, bool polarity) {}
void wxeditor_movie::_moviepanel::on_mouse2(unsigned x, unsigned y, bool polarity)
{
	if(polarity) {
		rpress_line = spos + y - 3;
		return;
	}
	wxMenu menu;
	current_popup = &menu;
	bool enable_toggle_button = false;
	bool enable_change_axis = false;
	bool enable_insert_frame = false;
	bool enable_delete_frame = false;
	bool enable_delete_subframe = false;
	std::u32string title;
	if(y < 3)
		goto outrange;
	if(!movb.get_movie().readonly_mode())
		goto outrange;
	press_x = x;
	press_line = spos + y - 3;
	for(auto i : fcontrols.get_controlinfo()) {
		unsigned off = divcnt + 1;
		if(press_x >= i.position_left + off && press_x < i.position_left + i.reserved + off) {
			if(i.type == 0 && press_line >= first_editable(i.index) &&
				press_line < linecount) {
				enable_toggle_button = true;
				press_index = i.index;
				title = i.title;
			}
			if(i.type == 1 && press_line >= first_editable(i.index) &&
				press_line < linecount) {
				enable_change_axis = true;
				press_index = i.index;
				title = i.title;
			}
		}
	}
	if(press_line + 1 >= first_editable(0) && press_line < linecount)
		enable_insert_frame = true;
	if(press_line >= first_editable(0) && press_line < linecount)
		enable_delete_subframe = true;
	if(press_line >= first_nextframe() && press_line < linecount)
		enable_delete_frame = true;
	if(enable_toggle_button)
		menu.Append(wxID_TOGGLE, towxstring(U"Toggle " + title));
	if(enable_change_axis)
		menu.Append(wxID_CHANGE, towxstring(U"Change " + title));
	if(enable_change_axis && rpress_line != press_line)
		menu.Append(wxID_SWEEP, towxstring(U"Sweep " + title));
	if(enable_toggle_button || enable_change_axis)
		menu.AppendSeparator();
	menu.Append(wxID_INSERT_AFTER, wxT("Insert frame after"))->Enable(enable_insert_frame);
	menu.Append(wxID_APPEND_FRAME, wxT("Append frame"));
	menu.Append(wxID_APPEND_FRAMES, wxT("Append frames..."));
	menu.AppendSeparator();
	menu.Append(wxID_DELETE_FRAME, wxT("Delete frame"))->Enable(enable_delete_frame);
	menu.Append(wxID_DELETE_SUBFRAME, wxT("Delete subframe"))->Enable(enable_delete_subframe);
	menu.AppendSeparator();
	menu.Append(wxID_TRUNCATE, wxT("Truncate movie"))->Enable(enable_delete_subframe);
	menu.AppendSeparator();
outrange:
	menu.Append(wxID_SCROLL_FRAME, wxT("Scroll to frame..."));
	menu.Append(wxID_SCROLL_CURRENT_FRAME, wxT("Scroll to current frame"));
	menu.Append(wxID_RUN_TO_FRAME, wxT("Run to frame..."));
	menu.Append(wxID_CHANGE_LINECOUNT, wxT("Change number of lines visible"));
	menu.AppendCheckItem(wxID_POSITION_LOCK, wxT("Lock scroll to playback"))->Check(position_locked);
	menu.Connect(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(wxeditor_movie::_moviepanel::on_popup_menu),
		NULL, this);
	PopupMenu(&menu);
}

int wxeditor_movie::_moviepanel::get_lines()
{
	controller_frame_vector& fv = movb.get_movie().get_frame_vector();
	return fv.size();
}

void wxeditor_movie::_moviepanel::signal_repaint()
{
	if(requested || recursing)
		return;
	auto s = m->get_scroll();
	requested = true;
	uint32_t width, height;
	uint64_t lines;
	wxeditor_movie* m2 = m;
	uint64_t old_cached_cffs = cached_cffs;
	uint32_t prev_width, prev_height;
	bool done_again = false;
do_again:
	runemufn([&lines, &width, &height, m2, this]() {
		lines = this->get_lines();
		if(lines < lines_to_display)
			this->moviepos = 0;
		else if(this->moviepos > lines - lines_to_display)
			this->moviepos = lines - lines_to_display;
		this->render(fb, moviepos);
		auto x = fb.get_characters();
		width = x.first;
		height = x.second;
	});
	if(old_cached_cffs != cached_cffs && position_locked && !done_again) {
		moviepos = cached_cffs;
		done_again = true;
		goto do_again;
	}
	prev_width = new_width;
	prev_height = new_height;
	new_width = width;
	new_height = height;
	movielines = lines;
	if(s)
		s->SetScrollbar(moviepos, lines_to_display, lines, lines_to_display - 1);
	auto size = fb.get_pixels();
	pixels.resize(size.first * size.second * 3);
	fb.render((char*)&pixels[0]);
	if(prev_width != new_width || prev_height != new_height) {
		auto cell = fb.get_cell();
		SetMinSize(wxSize(new_width * cell.first, (lines_to_display + 3) * cell.second));
		if(new_width > 0 && s)
			m->Fit();
	}
	linecount = lines;
	Refresh();
}

void wxeditor_movie::_moviepanel::on_mouse(wxMouseEvent& e)
{
	auto cell = fb.get_cell();
	if(e.LeftDown() && !e.ControlDown())
		on_mouse0(e.GetX() / cell.first, e.GetY() / cell.second, true);
	if(e.LeftUp() && !e.ControlDown())
		on_mouse0(e.GetX() / cell.first, e.GetY() / cell.second, false);
	if(e.MiddleDown())
		on_mouse1(e.GetX() / cell.first, e.GetY() / cell.second, true);
	if(e.MiddleUp())
		on_mouse1(e.GetX() / cell.first, e.GetY() / cell.second, false);
	if(e.RightDown() || (e.LeftDown() && e.ControlDown()))
		on_mouse2(e.GetX() / cell.first, e.GetY() / cell.second, true);
	if(e.RightUp() || (e.LeftUp() && e.ControlDown()))
		on_mouse2(e.GetX() / cell.first, e.GetY() / cell.second, false);
	int wrotate = e.GetWheelRotation();
	int threshold = e.GetWheelDelta();
	bool scrolled = false;
	auto s = m->get_scroll();
	if(threshold)
		scroll_delta += wrotate;
	while(wrotate && threshold && scroll_delta <= -threshold) {
		//Scroll down by line.
		moviepos++;
		if(movielines <= lines_to_display)
			moviepos = 0;
		else if(moviepos > movielines - lines_to_display + 1)
			moviepos = movielines - lines_to_display + 1;
		scrolled = true;
		scroll_delta += threshold;
	}
	while(wrotate && threshold && scroll_delta >= threshold) {
		//Scroll up by line.
		if(moviepos > 0)
			moviepos--;
		scrolled = true;
		scroll_delta -= threshold;
	}
	if(scrolled)
		s->SetThumbPosition(moviepos);
	signal_repaint();
}

void wxeditor_movie::_moviepanel::on_scroll(wxScrollEvent& e)
{
	auto s = m->get_scroll();
	if(s)
		moviepos = s->GetThumbPosition();
	else
		moviepos = 0;
	signal_repaint();
}

void wxeditor_movie::_moviepanel::on_erase(wxEraseEvent& e)
{
	//Blank.
}

void wxeditor_movie::_moviepanel::on_paint(wxPaintEvent& e)
{
	auto size = fb.get_pixels();
	if(!size.first || !size.second) {
		wxPaintDC dc(this);
		dc.Clear();
		requested = false;
		return;
	}
	wxPaintDC dc(this);
	wxBitmap bmp(wxImage(size.first, size.second, &pixels[0], true));
	dc.DrawBitmap(bmp, 0, 0, false);
	requested = false;
}

wxeditor_movie::wxeditor_movie(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, wxT("lsnes: Edit movie"), wxDefaultPosition, wxSize(-1, -1))
{
	closing = false;
	Centre();
	wxFlexGridSizer* top_s = new wxFlexGridSizer(2, 1, 0, 0);
	SetSizer(top_s);

	wxBoxSizer* panel_s = new wxBoxSizer(wxHORIZONTAL);
	moviescroll = NULL;
	panel_s->Add(moviepanel = new _moviepanel(this), 1, wxGROW);
	panel_s->Add(moviescroll = new wxScrollBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
		wxSB_VERTICAL), 0, wxGROW);
	top_s->Add(panel_s, 1, wxGROW);
	connect_events(moviescroll, wxScrollEventHandler(wxeditor_movie::_moviepanel::on_scroll), moviepanel);
	moviepanel->Connect(wxEVT_KEY_DOWN, wxKeyEventHandler(wxeditor_movie::on_keyboard_down), NULL, this);
	moviepanel->Connect(wxEVT_KEY_UP, wxKeyEventHandler(wxeditor_movie::on_keyboard_up), NULL, this);

	wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->AddStretchSpacer();
	pbutton_s->Add(closebutton = new wxButton(this, wxID_OK, wxT("Close")), 0, wxGROW);
	closebutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxeditor_movie::on_close), NULL, this);
	top_s->Add(pbutton_s, 0, wxGROW);

	moviepanel->SetFocus();
	moviescroll->Connect(wxEVT_SET_FOCUS, wxFocusEventHandler(wxeditor_movie::on_focus_wrong), NULL, this);
	closebutton->Connect(wxEVT_SET_FOCUS, wxFocusEventHandler(wxeditor_movie::on_focus_wrong), NULL, this);
	Connect(wxEVT_SET_FOCUS, wxFocusEventHandler(wxeditor_movie::on_focus_wrong), NULL, this);

	panel_s->SetSizeHints(this);
	pbutton_s->SetSizeHints(this);
	top_s->SetSizeHints(this);
	Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(wxeditor_movie::on_wclose));
	Fit();

	moviepanel->signal_repaint();
}

bool wxeditor_movie::ShouldPreventAppExit() const { return false; }

void wxeditor_movie::on_close(wxCommandEvent& e)
{
	movieeditor_open = NULL;
	Destroy();
	closing = true;
}

void wxeditor_movie::on_wclose(wxCloseEvent& e)
{
	bool wasc = closing;
	closing = true;
	movieeditor_open = NULL;
	if(!wasc)
		Destroy();
}

void wxeditor_movie::update()
{
	moviepanel->signal_repaint();
}

wxScrollBar* wxeditor_movie::get_scroll()
{
	return moviescroll;
}

void wxeditor_movie::on_focus_wrong(wxFocusEvent& e)
{
	moviepanel->SetFocus();
}

void wxeditor_movie_display(wxWindow* parent)
{
	if(movieeditor_open)
		return;
	wxeditor_movie* v = new wxeditor_movie(parent);
	v->Show();
	movieeditor_open = v;
}

void wxeditor_movie::on_keyboard_down(wxKeyEvent& e)
{
	handle_wx_keyboard(e, true);
}

void wxeditor_movie::on_keyboard_up(wxKeyEvent& e)
{
	handle_wx_keyboard(e, false);
}

void wxeditor_movie_update()
{
	if(movieeditor_open)
		movieeditor_open->update();
}
