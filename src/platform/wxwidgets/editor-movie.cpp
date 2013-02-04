#include "core/movie.hpp"
#include "core/moviedata.hpp"
#include "core/dispatch.hpp"
#include "core/window.hpp"

#include "interface/controller.hpp"
#include "platform/wxwidgets/platform.hpp"
#include "platform/wxwidgets/textrender.hpp"
#include "library/minmax.hpp"
#include "library/string.hpp"
#include "library/utf8.hpp"

#include <cstring>
#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>

void update_movie_state();

namespace
{
	const unsigned lines_to_display = 28;
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
	std::string title;
	unsigned port;
	unsigned controller;
	static control_info portinfo(unsigned& p, unsigned port, unsigned controller);
	static control_info fixedinfo(unsigned& p, const std::string& str);
	static control_info buttoninfo(unsigned& p, char character, unsigned idx);
	static control_info axisinfo(unsigned& p, const std::string& title, unsigned idx);
};

control_info control_info::portinfo(unsigned& p, unsigned port, unsigned controller)
{
	control_info i;
	i.position_left = p;
	i.reserved = utf8_strlen((stringfmt() << port << "-" << controller).str());
	p += i.reserved;
	i.index = 0;
	i.type = -2;
	i.ch = 0;
	i.title = "";
	i.port = port;
	i.controller = controller;
	return i;
}

control_info control_info::fixedinfo(unsigned& p, const std::string& str)
{
	control_info i;
	i.position_left = p;
	i.reserved = utf8_strlen(str);
	p += i.reserved;
	i.index = 0;
	i.type = -1;
	i.ch = 0;
	i.title = str;
	i.port = 0;
	i.controller = 0;
	return i;
}

control_info control_info::buttoninfo(unsigned& p, char character, unsigned idx)
{
	control_info i;
	i.position_left = p;
	i.reserved = 1;
	p += i.reserved;
	i.index = idx;
	i.type = 0;
	i.ch = character;
	i.title = "";
	i.port = 0;
	i.controller = 0;
	return i;
}

control_info control_info::axisinfo(unsigned& p, const std::string& title, unsigned idx)
{
	control_info i;
	i.position_left = p;
	i.reserved = utf8_strlen(title);
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

control_info axisinfo(unsigned& p, const std::string& title, unsigned idx);


class frame_controls
{
public:
	frame_controls();
	void set_types(controller_frame& f);
	short read_index(controller_frame& f, unsigned idx);
	void write_index(controller_frame& f, unsigned idx, short value);
	uint32_t read_pollcount(pollcounter_vector& v, unsigned idx);
	const std::list<control_info>& get_controlinfo() { return controlinfo; }
	std::string line1() { return _line1; }
	std::string line2() { return _line2; }
	size_t width() { return _width; }
private:
	size_t _width;
	std::string _line1;
	std::string _line2;
	void format_lines();
	void add_port(unsigned& c, unsigned pid, const port_type& p, const port_type_set& pts);
	std::string vector_to_string(const std::vector<uint32_t>& cp);
	std::vector<uint32_t> string_to_vector(const std::string& str);
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
	const port_type_set& pts = f.porttypes();
	unsigned pcnt = pts.ports();
	for(unsigned i = 0; i < pcnt; i++)
		add_port(nextp, i, pts.port_type(i), pts);
	format_lines();
}

void frame_controls::add_port(unsigned& c, unsigned pid, const port_type& p, const port_type_set& pts)
{
	unsigned i = 0;
	const port_controller_set& pci = *(p.controller_info);
	for(unsigned i = 0; i < pci.controller_count; i++) {
		if(!pci.controllers[i])
			continue;
		const port_controller& pc = *(pci.controllers[i]);
		if(pid || i)
			controlinfo.push_back(control_info::fixedinfo(c, "│"));
		unsigned nextp = c;
		controlinfo.push_back(control_info::portinfo(nextp, pid, i + 1));
		bool last_multibyte = false;
		for(unsigned j = 0; j < pc.button_count; j++) {
			if(!pc.buttons[j])
				continue;
			const port_controller_button& pcb = *(pc.buttons[j]);
			unsigned idx = pts.triple_to_index(pid, i, j);
			if(idx == 0xFFFFFFFFUL)
				continue;
			if(pcb.type == port_controller_button::TYPE_BUTTON) {
				if(last_multibyte)
					c++;
				controlinfo.push_back(control_info::buttoninfo(c, pcb.symbol, idx));
				last_multibyte = false;
			} else if(pcb.type == port_controller_button::TYPE_AXIS) {
				if(j)
					c++;
				controlinfo.push_back(control_info::axisinfo(c, pcb.name, idx));
				last_multibyte = true;
			}
		}
		if(nextp > c)
			c = nextp;
	}
}

short frame_controls::read_index(controller_frame& f, unsigned idx)
{
	if(idx == 0)
		return f.sync() ? 1 : 0;
	return f.axis2(idx);
}

void frame_controls::write_index(controller_frame& f, unsigned idx, short value)
{
	if(idx == 0)
		return f.sync(value);
	return f.axis2(idx, value);
}

uint32_t frame_controls::read_pollcount(pollcounter_vector& v, unsigned idx)
{
	if(idx == 0)
		return max(v.max_polls(), (uint32_t)1);
	return v.get_polls(idx);
}

std::string frame_controls::vector_to_string(const std::vector<uint32_t>& cp)
{
	std::ostringstream s;
	for(auto i : cp) {
		if(i < 0x80)
			s << (unsigned char)i;
		else if(i < 0x800)
			s << (unsigned char)(0xC0 + (i >> 6)) << (unsigned char)(0x80 + (i & 0x3F));
		else if(i < 0x10000)
			s << (unsigned char)(0xE0 + (i >> 12)) << (unsigned char)(0x80 + ((i >> 6) & 0x3F))
				 << (unsigned char)(0x80 + (i & 0x3F));
		else if(i < 0x10FFFF)
			s << (unsigned char)(0xF0 + (i >> 18)) << (unsigned char)(0x80 + ((i >> 12) & 0x3F))
				<< (unsigned char)(0x80 + ((i >> 6) & 0x3F))
				<< (unsigned char)(0x80 + (i & 0x3F));
	}
	return s.str();
}

std::vector<uint32_t> frame_controls::string_to_vector(const std::string& str)
{
	std::vector<uint32_t> cp;
	size_t spos = 0;
	size_t slen = str.length();
	uint16_t state = utf8_initial_state;
	while(true) {
		int ch = (spos < slen) ? (unsigned char)str[spos] : - 1;
		int32_t u = utf8_parse_byte(ch, state);
		if(u >= 0)
			cp.push_back(u);
		if(ch < 0)
			break;
		spos++;
	}
	return cp;
}

void frame_controls::format_lines()
{
	_width = 0;
	for(auto i : controlinfo) {
		if(i.position_left + i.reserved > _width)
			_width = i.position_left + i.reserved;
	}
	std::vector<uint32_t> cp1;
	std::vector<uint32_t> cp2;
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
			auto _title = string_to_vector(i.title);
			std::copy(_title.begin(), _title.end(), &cp1[i.position_left + off]);
		} else if(i.type == -2) {
			auto _title = string_to_vector((stringfmt() << i.port << "-" << i.controller).str());
			std::copy(_title.begin(), _title.end(), &cp1[i.position_left + off]);
		}
	}
	//Line2
	for(auto i : controlinfo) {
		auto _title = string_to_vector(i.title);
		if(i.type == -1 || i.type == 1)
			std::copy(_title.begin(), _title.end(), &cp2[i.position_left + off]);
		if(i.type == 0)
			cp2[i.position_left + off] = i.ch;
	}
	_line1 = vector_to_string(cp1);
	_line2 = vector_to_string(cp2);
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
		~_moviepanel();
		void signal_repaint();
		void on_scroll(wxScrollEvent& e);
		void on_paint(wxPaintEvent& e);
		void on_mouse(wxMouseEvent& e);
	private:
		int get_lines();
		void render(text_framebuffer& fb, unsigned long long pos);
		void on_mouse0(unsigned x, unsigned y, bool polarity);
		void on_mouse1(unsigned x, unsigned y, bool polarity);
		void on_mouse2(unsigned x, unsigned y, bool polarity);
		int width(controller_frame& f);
		std::string render_line1(controller_frame& f);
		std::string render_line2(controller_frame& f);
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
		int movielines;
		int moviepos;
		unsigned new_width;
		std::vector<uint8_t> pixels;
		int scroll_delta;
		unsigned press_x;
		uint64_t press_line;
		bool pressed;
		bool recursing;
	};
	_moviepanel* moviepanel;
	wxButton* closebutton;
	wxScrollBar* moviescroll;
	bool closing;
};

namespace
{
	wxeditor_movie* movieeditor_open;

	uint64_t first_editable(frame_controls& fc, unsigned idx)
	{
		uint64_t cffs = movb.get_movie().get_current_frame_first_subframe();
		pollcounter_vector& pv = movb.get_movie().get_pollcounters();
		return cffs + fc.read_pollcount(pv, idx);
	}
}

wxeditor_movie::_moviepanel::~_moviepanel() {}
wxeditor_movie::~wxeditor_movie() throw() {}

wxeditor_movie::_moviepanel::_moviepanel(wxeditor_movie* v)
	: wxPanel(v, wxID_ANY, wxDefaultPosition, wxSize(100, 100)),
	information_dispatch("movieeditor-listener")
{
	m = v;
	Connect(wxEVT_PAINT, wxPaintEventHandler(_moviepanel::on_paint), NULL, this);
	new_width = 0;
	moviepos = 0;
	scroll_delta = 0;
	spos = 0;
	prev_obj = NULL;
	prev_seqno = 0;
	max_subframe = 0;
	recursing = false;

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

std::string wxeditor_movie::_moviepanel::render_line1(controller_frame& f)
{
	update_cache();
	return fcontrols.line1();
}

std::string wxeditor_movie::_moviepanel::render_line2(controller_frame& f)
{
	update_cache();
	return fcontrols.line2();
}

void wxeditor_movie::_moviepanel::render_linen(text_framebuffer& fb, controller_frame& f, uint64_t sfn, int y)
{
	update_cache();
	size_t fbstride = fb.get_stride();
	auto fbsize = fb.get_characters();
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
	int past = -1;
	if(!movb.get_movie().readonly_mode())
		past = 1;
	else if(subframe_to_frame[sfn] < curframe)
		past = 1;
	else if(subframe_to_frame[sfn] > curframe)
		past = 0;
	bool now = (subframe_to_frame[sfn] == curframe);
	for(auto i : ctrlinfo) {
		int rpast = past;
		if(rpast == -1) {
			unsigned polls = fcontrols.read_pollcount(pv, i.index);
			rpast = ((cffs + polls) > sfn) ? 1 : 0;
		}
		if(i.type == -1) {
			//Separator.
			fb.write(i.title, 0, divcnt + 1 + i.position_left, y, 0x000000, now ? 0xFFC0C0 : 0xFFFFFF);
		} else if(i.type == 0) {
			//Button.
			char c[2];
			bool v = (fcontrols.read_index(f, i.index) != 0);
			c[0] = v ? i.ch : ' ';
			c[1] = 0;
			fb.write(c, 0, divcnt + 1 + i.position_left, y, rpast ? 0x808080 : 0x000000,
				 now ? 0xFFC0C0 : 0xFFFFFF);
		} else if(i.type == 1) {
			//Axis.
			char c[7];
			sprintf(c, "%6d", fcontrols.read_index(f, i.index));
			fb.write(c, 0, divcnt + 1 + i.position_left, y, rpast ? 0x808080 : 0x000000,
				 now ? 0xFFC0C0 : 0xFFFFFF);
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
	fb.write((stringfmt() << "Current frame: " << movb.get_movie().get_current_frame()).str(), _width, 0, 0,
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
		recursing = true;
		auto _press_line = press_line;
		runemufn([_press_line, line]() {
			if(!movb.get_movie().readonly_mode())
				return;
			controller_frame_vector& fv = movb.get_movie().get_frame_vector();
			auto a_press_line = _press_line;
			auto a_line = line;
			if(a_press_line > a_line)
				std::swap(a_press_line, a_line);
			for(uint64_t i = a_press_line; i <= a_line; i++)
				fv.append(fv.blank_frame(true));
			movb.get_movie().recount_frames();
			update_movie_state();
			graphics_driver_notify_status();
		});
		recursing = false;
	}
	for(auto i : fcontrols.get_controlinfo()) {
		unsigned off = divcnt + 1;
		unsigned idx = i.index;
		frame_controls* _fcontrols = &fcontrols;
		if(press_x >= i.position_left + off && press_x < i.position_left + i.reserved + off) {
			if(i.type == 0) {
				//Button.
				if(press_x == x) {
					//Drag action.
					auto _press_line = press_line;
					recursing = true;
					runemufn([idx, _press_line, line, _fcontrols]() {
						if(!movb.get_movie().readonly_mode())
							return;
						uint64_t fedit = first_editable(*_fcontrols, idx);
						controller_frame_vector& fv = movb.get_movie().get_frame_vector();
						auto a_press_line = _press_line;
						auto a_line = line;
						if(a_press_line > a_line)
							std::swap(a_press_line, a_line);
						for(uint64_t i = a_press_line; i <= a_line; i++) {
							if(i >= fv.size())
								continue;
							if(i < fedit)
								continue;
							controller_frame cf = fv[i];
							_fcontrols->write_index(cf, idx,
								!_fcontrols->read_index(cf, idx));
						}
						if(idx == 0) {
							movb.get_movie().recount_frames();
							update_movie_state();
							graphics_driver_notify_status();
						}
					});
					recursing = false;
					if(idx == 0) {
						max_subframe = 0;	//Reparse.
					}
				}
			} else if(i.type == 1) {
				if(press_x == x && press_line == line) {
					//Click change value.
					short value;
					bool valid = true;
					runemufn([idx, line, &value, _fcontrols, &valid]() {
						if(!movb.get_movie().readonly_mode()) {
							valid = false;
							return;
						}
						uint64_t fedit = first_editable(*_fcontrols, idx);
						controller_frame_vector& fv = movb.get_movie().get_frame_vector();
						if(line < fedit) {
							valid = false;
							return;
						}
						if(line >= fv.size())
							return;
						controller_frame cf = fv[line];
						value = _fcontrols->read_index(cf, idx);
					});
					if(!valid)
						continue;
					try {
						std::string text = pick_text(m, "Set value", "Enter new value:",
							(stringfmt() << value).str());
						value = parse_value<short>(text);
					} catch(canceled_exception& e) {
						return;
					} catch(std::exception& e) {
						wxMessageBox(wxT("Invalid value"), _T("Error"), wxICON_EXCLAMATION |
							wxOK, m);
						return;
					}
					runemufn([idx, line, value, _fcontrols]() {
						uint64_t fedit = first_editable(*_fcontrols, idx);
						controller_frame_vector& fv = movb.get_movie().get_frame_vector();
						if(line < fedit)
							return;
						if(line >= fv.size())
							return;
						controller_frame cf = fv[line];
						_fcontrols->write_index(cf, idx, value);
					});
				}
			}
		}
	}
}

void wxeditor_movie::_moviepanel::on_mouse1(unsigned x, unsigned y, bool polarity) {}
void wxeditor_movie::_moviepanel::on_mouse2(unsigned x, unsigned y, bool polarity) {}

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
	int lines, width;
	wxeditor_movie* m2 = m;
	runemufn([&lines, &width, m2, this]() {
		lines = this->get_lines();
		if(lines < lines_to_display)
			this->moviepos = 0;
		else if(this->moviepos > lines - lines_to_display)
			this->moviepos = lines - lines_to_display;
		this->render(fb, moviepos);
		auto x = fb.get_characters();
		width = x.first;
	});
	int prev_width = new_width;
	new_width = width;
	if(s)
		s->SetScrollbar(moviepos, lines_to_display, lines, lines_to_display - 1);
	auto size = fb.get_pixels();
	pixels.resize(size.first * size.second * 3);
	fb.render((char*)&pixels[0]);
	if(prev_width != new_width) {
		auto cell = fb.get_cell();
		SetMinSize(wxSize(new_width * cell.first, (lines_to_display + 3) * cell.second));
		if(new_width > 0 && s)
			m->Fit();
	}
	Refresh();
}

void wxeditor_movie::_moviepanel::on_mouse(wxMouseEvent& e)
{
	auto cell = fb.get_cell();
	if(e.LeftDown())
		on_mouse0(e.GetX() / cell.first, e.GetY() / cell.second, true);
	if(e.LeftUp())
		on_mouse0(e.GetX() / cell.first, e.GetY() / cell.second, false);
	if(e.MiddleDown())
		on_mouse1(e.GetX() / cell.first, e.GetY() / cell.second, true);
	if(e.MiddleUp())
		on_mouse1(e.GetX() / cell.first, e.GetY() / cell.second, false);
	if(e.RightDown())
		on_mouse2(e.GetX() / cell.first, e.GetY() / cell.second, true);
	if(e.RightUp())
		on_mouse2(e.GetX() / cell.first, e.GetY() / cell.second, false);
	int wrotate = e.GetWheelRotation();
	int threshold = e.GetWheelDelta();
	bool scrolled = false;
	auto s = m->get_scroll();
	scroll_delta += wrotate;
	while(wrotate && scroll_delta <= -threshold) {
		//Scroll down by line.
		moviepos++;
		if(movielines <= lines_to_display)
			moviepos = 0;
		else if(moviepos > movielines - lines_to_display + 1)
			moviepos = movielines - lines_to_display + 1;
		scrolled = true;
		scroll_delta += threshold;
	}
	while(wrotate && scroll_delta >= threshold) {
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
	dc.Clear();
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
