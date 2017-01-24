#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>
#include <wx/clipbrd.h>

#include "core/framebuffer.hpp"
#include "core/instance.hpp"
#include "core/instance-map.hpp"
#include "core/moviedata.hpp"
#include "core/dispatch.hpp"
#include "core/window.hpp"
#include "core/ui-services.hpp"

#include "interface/controller.hpp"
#include "core/mainloop.hpp"
#include "core/mbranch.hpp"
#include "core/project.hpp"
#include "platform/wxwidgets/loadsave.hpp"
#include "platform/wxwidgets/platform.hpp"
#include "platform/wxwidgets/scrollbar.hpp"
#include "platform/wxwidgets/textrender.hpp"
#include "library/minmax.hpp"
#include "library/string.hpp"
#include "library/utf8.hpp"

#include <algorithm>
#include <cstring>
#include <limits>

extern "C"
{
#ifndef UINT64_C
#define UINT64_C(val) val##ULL
#endif
#include <libswscale/swscale.h>
}

enum
{
	wxID_TOGGLE = wxID_HIGHEST + 1,
	wxID_CHANGE,
	wxID_SWEEP,
	wxID_APPEND_FRAME,
	wxID_CHANGE_LINECOUNT,
	wxID_INSERT_AFTER,
	wxID_INSERT_AFTER_MULTIPLE,
	wxID_DELETE_FRAME,
	wxID_DELETE_SUBFRAME,
	wxID_POSITION_LOCK,
	wxID_RUN_TO_FRAME,
	wxID_APPEND_FRAMES,
	wxID_TRUNCATE,
	wxID_SCROLL_FRAME,
	wxID_SCROLL_CURRENT_FRAME,
	wxID_COPY_FRAMES,
	wxID_CUT_FRAMES,
	wxID_PASTE_FRAMES,
	wxID_PASTE_APPEND,
	wxID_INSERT_CONTROLLER_AFTER,
	wxID_DELETE_CONTROLLER_SUBFRAMES,
	wxID_MBRANCH_NEW,
	wxID_MBRANCH_IMPORT,
	wxID_MBRANCH_EXPORT,
	wxID_MBRANCH_RENAME,
	wxID_MBRANCH_DELETE,
	wxID_MBRANCH_FIRST,
	wxID_MBRANCH_LAST = wxID_MBRANCH_FIRST + 1024
};

namespace
{
	unsigned lines_to_display = 28;
	uint64_t divs[] = {1000000, 100000, 10000, 1000, 100, 10, 1};
	uint64_t divsl[] = {1000000, 100000, 10000, 1000, 100, 10, 0};
	const unsigned divcnt = sizeof(divs)/sizeof(divs[0]);

	class exp_imp_type
	{
	public:
		typedef std::pair<std::string, int> returntype;
		exp_imp_type()
		{
		}
		filedialog_input_params input(bool save) const
		{
			filedialog_input_params ip;
			ip.types.push_back(filedialog_type_entry("Input tracks (text)", "*.lstt", "lstt"));
			ip.types.push_back(filedialog_type_entry("Input tracks (binary)", "*.lstb", "lstb"));
			if(!save)
				ip.types.push_back(filedialog_type_entry("Movie files", "*.lsmv", "lsmv"));
			ip.default_type = 1;
			return ip;
		}
		std::pair<std::string, int> output(const filedialog_output_params& p, bool save) const
		{
			int m;
			switch(p.typechoice) {
			case 0: 	m = MBRANCH_IMPORT_TEXT; break;
			case 1: 	m = MBRANCH_IMPORT_BINARY; break;
			case 2: 	m = MBRANCH_IMPORT_MOVIE; break;
			};
			return std::make_pair(p.path, m);
		}
	private:
	};
}

struct control_info
{
	unsigned position_left;
	unsigned reserved;	//Must be at least 6 for axes.
	unsigned index;		//Index in poll vector.
	int type;		//-2 => Port, -1 => Fixed, 0 => Button, 1 => axis.
	char32_t ch;
	std::u32string title;
	unsigned port;
	unsigned controller;
	portctrl::button::_type axistype;
	int rmin;
	int rmax;
	static control_info portinfo(unsigned& p, unsigned port, unsigned controller);
	static control_info fixedinfo(unsigned& p, const std::u32string& str);
	static control_info buttoninfo(unsigned& p, char32_t character, const std::u32string& title, unsigned idx,
		unsigned port, unsigned controller);
	static control_info axisinfo(unsigned& p, const std::u32string& title, unsigned idx,
		unsigned port, unsigned controller, portctrl::button::_type _axistype, int _rmin,
		int _rmax);
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

control_info control_info::buttoninfo(unsigned& p, char32_t character, const std::u32string& title, unsigned idx,
	unsigned port, unsigned controller)
{
	control_info i;
	i.position_left = p;
	i.reserved = 1;
	p += i.reserved;
	i.index = idx;
	i.type = 0;
	i.ch = character;
	i.title = title;
	i.port = port;
	i.controller = controller;
	return i;
}

control_info control_info::axisinfo(unsigned& p, const std::u32string& title, unsigned idx,
	unsigned port, unsigned controller, portctrl::button::_type _axistype, int _rmin, int _rmax)
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
	i.port = port;
	i.controller = controller;
	i.axistype = _axistype;
	i.rmin = _rmin;
	i.rmax = _rmax;
	return i;
}

class frame_controls
{
public:
	frame_controls();
	void set_types(portctrl::frame& f);
	short read_index(portctrl::frame& f, unsigned idx);
	void write_index(portctrl::frame& f, unsigned idx, short value);
	uint32_t read_pollcount(portctrl::counters& v, unsigned idx);
	const std::list<control_info>& get_controlinfo() { return controlinfo; }
	std::u32string line1() { return _line1; }
	std::u32string line2() { return _line2; }
	size_t width() { return _width; }
private:
	size_t _width;
	std::u32string _line1;
	std::u32string _line2;
	void format_lines();
	void add_port(unsigned& c, unsigned pid, const portctrl::type& p, const portctrl::type_set& pts);
	std::list<control_info> controlinfo;
};


frame_controls::frame_controls()
{
	_width = 0;
}

void frame_controls::set_types(portctrl::frame& f)
{
	unsigned nextp = 0;
	controlinfo.clear();
	const portctrl::type_set& pts = f.porttypes();
	unsigned pcnt = pts.ports();
	for(unsigned i = 0; i < pcnt; i++)
		add_port(nextp, i, pts.port_type(i), pts);
	format_lines();
}

void frame_controls::add_port(unsigned& c, unsigned pid, const portctrl::type& p, const portctrl::type_set& pts)
{
	const portctrl::controller_set& pci = *(p.controller_info);
	for(unsigned i = 0; i < pci.controllers.size(); i++) {
		const portctrl::controller& pc = pci.controllers[i];
		if(pid || i)
			controlinfo.push_back(control_info::fixedinfo(c, U"\u2502"));
		unsigned nextp = c;
		controlinfo.push_back(control_info::portinfo(nextp, pid, i + 1));
		bool last_multibyte = false;
		for(unsigned j = 0; j < pc.buttons.size(); j++) {
			const portctrl::button& pcb = pc.buttons[j];
			unsigned idx = pts.triple_to_index(pid, i, j);
			if(idx == 0xFFFFFFFFUL)
				continue;
			if(pcb.type == portctrl::button::TYPE_BUTTON) {
				if(last_multibyte)
					c++;
				controlinfo.push_back(control_info::buttoninfo(c, pcb.symbol, utf8::to32(pcb.name),
					idx, pid, i));
				last_multibyte = false;
			} else if(pcb.type == portctrl::button::TYPE_AXIS ||
				pcb.type == portctrl::button::TYPE_RAXIS ||
				pcb.type == portctrl::button::TYPE_TAXIS ||
				pcb.type == portctrl::button::TYPE_LIGHTGUN) {
				if(j)
					c++;
				controlinfo.push_back(control_info::axisinfo(c, utf8::to32(pcb.name), idx, pid, i,
					pcb.type, pcb.rmin, pcb.rmax));
				last_multibyte = true;
			}
		}
		if(nextp > c)
			c = nextp;
	}
}

short frame_controls::read_index(portctrl::frame& f, unsigned idx)
{
	if(idx == 0)
		return f.sync() ? 1 : 0;
	return f.axis2(idx);
}

void frame_controls::write_index(portctrl::frame& f, unsigned idx, short value)
{
	if(idx == 0)
		return f.sync(value);
	return f.axis2(idx, value);
}

uint32_t frame_controls::read_pollcount(portctrl::counters& v, unsigned idx)
{
	if(idx == 0)
		return max(v.max_polls(), (uint32_t)1);
	for(auto i : controlinfo)
		if(idx == i.index && i.port == 0 && i.controller == 0)
			return max(v.get_polls(idx), (uint32_t)(v.get_framepflag() ? 1 : 0));
	return v.get_polls(idx);
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

namespace
{
	//TODO: Use real clipboard.
	std::string clipboard;

	void copy_to_clipboard(const std::string& text)
	{
		clipboard = text;
	}

	bool clipboard_has_text()
	{
		return (clipboard.length() > 0);
	}

	std::string copy_from_clipboard()
	{
		return clipboard;
	}

	std::string encode_line(portctrl::frame& f)
	{
		char buffer[512];
		f.serialize(buffer);
		return buffer;
	}

	std::string encode_line(frame_controls& info, portctrl::frame& f, unsigned port,
		unsigned controller)
	{
		std::ostringstream x;
		bool last_axis = false;
		bool first = true;
		for(auto i : info.get_controlinfo()) {
			if(i.port != port)
				continue;
			if(i.controller != controller)
				continue;
			switch(i.type) {
			case 0:		//Button.
				if(last_axis)
					x << " ";
				if(info.read_index(f, i.index)) {
					char32_t tmp1[2];
					tmp1[0] = i.ch;
					tmp1[1] = 0;
					x << utf8::to8(std::u32string(tmp1));
				} else
					x << "-";
				last_axis = false;
				first = false;
				break;
			case 1:		//Axis.
				if(!first)
					x << " ";
				x << info.read_index(f, i.index);
				first = false;
				last_axis = true;
				break;
			}
		}
		return x.str();
	}

	short read_short(const std::u32string& s, size_t& r)
	{
		unsigned short _res = 0;
		bool negative = false;
		if(r < s.length() && s[r] == '-') {
			negative = true;
			r++;
		}
		while(r < s.length() && s[r] >= 48 && s[r] <= 57) {
			_res = _res * 10 + (s[r] - 48);
			r++;
		}
		return negative ? -_res : _res;
	}

	void decode_line(frame_controls& info, portctrl::frame& f, std::string line, unsigned port,
		unsigned controller)
	{
		std::u32string _line = utf8::to32(line);
		bool last_axis = false;
		bool first = true;
		short y;
		char32_t y2;
		size_t ridx = 0;
		for(auto i : info.get_controlinfo()) {
			if(i.port != port)
				continue;
			if(i.controller != controller)
				continue;
			switch(i.type) {
			case 0:		//Button.
				if(last_axis) {
					ridx++;
					while(ridx < _line.length() && (_line[ridx] == 9 || _line[ridx] == 10 ||
						_line[ridx] == 13 || _line[ridx] == 32))
						ridx++;
				}
				y2 = (ridx < _line.length()) ? _line[ridx++] : 0;
				if(y2 == U'-' || y2 == 0)
					info.write_index(f, i.index, 0);
				else
					info.write_index(f, i.index, 1);
				last_axis = false;
				first = false;
				break;
			case 1:		//Axis.
				if(!first)
					ridx++;
				while(ridx < _line.length() && (_line[ridx] == 9 || _line[ridx] == 10 ||
					_line[ridx] == 13 || _line[ridx] == 32))
					ridx++;
				y = read_short(_line, ridx);
				info.write_index(f, i.index, y);
				first = false;
				last_axis = true;
				break;
			}
		}
	}

	std::string encode_lines(portctrl::frame_vector& fv, uint64_t start, uint64_t end)
	{
		std::ostringstream x;
		x << "lsnes-moviedata-whole" << std::endl;
		for(uint64_t i = start; i < end; i++) {
			portctrl::frame tmp = fv[i];
			x << encode_line(tmp) << std::endl;
		}
		return x.str();
	}

	std::string encode_lines(frame_controls& info, portctrl::frame_vector& fv, uint64_t start,
		uint64_t end, unsigned port, unsigned controller)
	{
		std::ostringstream x;
		x << "lsnes-moviedata-controller" << std::endl;
		for(uint64_t i = start; i < end; i++) {
			portctrl::frame tmp = fv[i];
			x << encode_line(info, tmp, port, controller) << std::endl;
		}
		return x.str();
	}

	int clipboard_get_data_type()
	{
		if(!clipboard_has_text())
			return -1;
		std::string y = copy_from_clipboard();
		std::istringstream x(y);
		std::string hdr;
		std::getline(x, hdr);
		if(hdr == "lsnes-moviedata-whole")
			return 1;
		if(hdr == "lsnes-moviedata-controller")
			return 0;
		return -1;
	}

	std::set<unsigned> controller_index_set(frame_controls& info, unsigned port, unsigned controller)
	{
		std::set<unsigned> r;
		for(auto i : info.get_controlinfo()) {
			if(i.port == port && i.controller == controller && (i.type == 0 || i.type == 1))
				r.insert(i.index);
		}
		return r;
	}

	void move_index_set(frame_controls& info, portctrl::frame_vector& fv, uint64_t src, uint64_t dst,
		uint64_t len, const std::set<unsigned>& indices)
	{
		if(src == dst)
			return;
		portctrl::frame_vector::notify_freeze freeze(fv);
		if(src > dst) {
			//Copy forwards.
			uint64_t shift = src - dst;
			for(uint64_t i = dst; i < dst + len; i++) {
				portctrl::frame _src = fv[i + shift];
				portctrl::frame _dst = fv[i];
				for(auto j : indices)
					info.write_index(_dst, j, info.read_index(_src, j));
			}
		} else {
			//Copy backwards.
			uint64_t shift = dst - src;
			for(uint64_t i = src + len - 1; i >= src && i < src + len; i--) {
				portctrl::frame _src = fv[i];
				portctrl::frame _dst = fv[i + shift];
				for(auto j : indices)
					info.write_index(_dst, j, info.read_index(_src, j));
			}
		}
	}

	void zero_index_set(frame_controls& info, portctrl::frame_vector& fv, uint64_t dst, uint64_t len,
		const std::set<unsigned>& indices)
	{
		portctrl::frame_vector::notify_freeze freeze(fv);
		for(uint64_t i = dst; i < dst + len; i++) {
			portctrl::frame _dst = fv[i];
			for(auto j : indices)
				info.write_index(_dst, j, 0);
		}
	}

	control_info find_paired(control_info ci, const std::list<control_info>& info)
	{
		if(ci.axistype == portctrl::button::TYPE_TAXIS)
			return ci;
		bool even = true;
		bool next_flag = false;
		control_info previous;
		for(auto i : info) {
			if(i.port != ci.port || i.controller != ci.controller)
				continue;
			if(i.axistype != portctrl::button::TYPE_AXIS &&
				i.axistype != portctrl::button::TYPE_RAXIS &&
				i.axistype != portctrl::button::TYPE_LIGHTGUN)
				continue;
			if(next_flag)
				return i;
			if(i.index == ci.index) {
				//This and...
				if(even)
					next_flag = true; //Next.
				else
					return previous; //Pevious.
			}
			previous = i;
			even = !even;
		}
		//Huh, no pair.
		return ci;
	}

	int32_t value_to_coordinate(int32_t rmin, int32_t rmax, int32_t val, int32_t dim)
	{
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
		if(dim == rmin - rmax + 1) {
			return val + rmin;
		}
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

	std::string windowname(control_info X, control_info Y)
	{
		if(X.index == Y.index)
			return (stringfmt() << utf8::to8(X.title)).str();
		else
			return (stringfmt() << utf8::to8(X.title) << "/" <<  utf8::to8(Y.title)).str();
	}

	class window_prompt : public wxDialog
	{
	public:
		window_prompt(wxWindow* parent, uint8_t* _bitmap, unsigned _width,
			unsigned _height, control_info X, control_info Y, unsigned posX, unsigned posY)
			: wxDialog(parent, wxID_ANY, towxstring(windowname(X, Y)), wxPoint(posX, posY))
		{
			CHECK_UI_THREAD;
			dirty = false;
			bitmap = _bitmap;
			width = _width;
			height = _height;
			cX = X;
			cY = Y;
			oneaxis = false;
			if(X.index == Y.index) {
				//One-axis never has a bitmap.
				bitmap = NULL;
				height = 32;
				oneaxis = true;
			}
			wxSizer* s = new wxBoxSizer(wxVERTICAL);
			SetSizer(s);
			s->Add(panel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(width, height)), 0,
				wxGROW);
			panel->Connect(wxEVT_PAINT, wxPaintEventHandler(window_prompt::on_paint), NULL, this);
			panel->Connect(wxEVT_ERASE_BACKGROUND, wxEraseEventHandler(window_prompt::on_erase), NULL,
				this);
			Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(window_prompt::on_wclose));
			panel->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(window_prompt::on_mouse), NULL, this);
			panel->Connect(wxEVT_MOTION, wxMouseEventHandler(window_prompt::on_mouse), NULL, this);
			Fit();
		}
		void on_wclose(wxCloseEvent& e)
		{
			CHECK_UI_THREAD;
			EndModal(wxID_CANCEL);
		}
		void on_erase(wxEraseEvent& e)
		{
			//Blank.
		}
		void on_paint(wxPaintEvent& e)
		{
			CHECK_UI_THREAD;
			wxPaintDC dc(panel);
			if(bitmap) {
				wxBitmap bmp(wxImage(width, height, bitmap, true));
				dc.DrawBitmap(bmp, 0, 0, false);
			} else {
				dc.SetBackground(*wxWHITE_BRUSH);
				dc.Clear();
				auto xval = value_to_coordinate(cX.rmin, cX.rmax, 0, width);
				auto yval = value_to_coordinate(cY.rmin, cY.rmax, 0, height);
				dc.SetPen(*wxBLACK_PEN);
				if(cX.rmin < 0 && cX.rmax > 0)
					dc.DrawLine(xval, 0, xval, height);
				if(!oneaxis && cY.rmin < 0 && cY.rmax > 0)
					dc.DrawLine(0, yval, width, yval);
			}
			dc.SetPen(*wxRED_PEN);
			dc.DrawLine(mouseX, 0, mouseX, height);
			if(!oneaxis)
				dc.DrawLine(0, mouseY, width, mouseY);
			dirty = false;
		}
		void on_mouse(wxMouseEvent& e)
		{
			CHECK_UI_THREAD;
			if(e.LeftDown()) {
				result.first = coordinate_to_value(cX.rmin, cX.rmax, e.GetX(), width);
				if(!oneaxis)
					result.second = coordinate_to_value(cY.rmin, cY.rmax, e.GetY(), height);
				else
					result.second = 0;
				EndModal(wxID_OK);
			}
			mouseX = e.GetX();
			mouseY = e.GetY();
			if(!dirty) {
				dirty = true;
				panel->Refresh();
			}
		}
		std::pair<int, int> get_results()
		{
			return result;
		}
	private:
		std::pair<int, int> result;
		wxPanel* panel;
		bool oneaxis;
		bool dirty;
		int mouseX;
		int mouseY;
		int height;
		int width;
		control_info cX;
		control_info cY;
		uint8_t* bitmap;
	};

	std::pair<int, int> prompt_coodinates_window(wxWindow* parent, uint8_t* bitmap, unsigned width,
		unsigned height, control_info X, control_info Y, unsigned posX, unsigned posY)
	{
		CHECK_UI_THREAD;
		window_prompt* p = new window_prompt(parent, bitmap, width, height, X, Y, posX, posY);
		if(p->ShowModal() == wxID_CANCEL) {
			delete p;
			throw canceled_exception();
		}
		auto r = p->get_results();
		delete p;
		return r;
	}

	//Has the special behaviour of showing empty name specially.
	std::string pick_among_branches(wxWindow* parent, const std::string& title, const std::string& prompt,
		const std::vector<std::string>& choices)
	{
		std::vector<std::string> choices2 = choices;
		for(size_t i = 0; i < choices2.size(); i++)
			if(choices2[i] == "") choices2[i] = "(Default branch)";
		auto idx = pick_among_index(parent, title, prompt, choices2);
		if(idx < choices.size())
			return choices[idx];
		throw canceled_exception();
	}
}

class wxeditor_movie : public wxDialog
{
public:
	wxeditor_movie(emulator_instance& _inst, wxWindow* parent);
	~wxeditor_movie() throw();
	bool ShouldPreventAppExit() const;
	void on_close(wxCommandEvent& e);
	void on_wclose(wxCloseEvent& e);
	void on_focus_wrong(wxFocusEvent& e);
	void on_keyboard_down(wxKeyEvent& e);
	void on_keyboard_up(wxKeyEvent& e);
	scroll_bar* get_scroll();
	void update();
private:
	struct _moviepanel : public wxPanel
	{
		_moviepanel(wxeditor_movie* v, emulator_instance& _inst);
		~_moviepanel() throw();
		void signal_repaint();
		void on_paint(wxPaintEvent& e);
		void on_erase(wxEraseEvent& e);
		void on_mouse(wxMouseEvent& e);
		void on_popup_menu(wxCommandEvent& e);
		uint64_t moviepos;
	private:
		int get_lines();
		void render(text_framebuffer& fb, unsigned long long pos);
		void on_mouse0(unsigned x, unsigned y, bool polarity, bool shift, unsigned X, unsigned Y);
		void on_mouse1(unsigned x, unsigned y, bool polarity);
		void on_mouse2(unsigned x, unsigned y, bool polarity);
		void popup_axis_panel(uint64_t row, control_info ci, unsigned screenX, unsigned screenY);
		void do_toggle_buttons(unsigned idx, uint64_t row1, uint64_t row2, bool force_false);
		void do_alter_axis(unsigned idx, uint64_t row1, uint64_t row2);
		void do_sweep_axis(unsigned idx, uint64_t row1, uint64_t row2);
		void do_append_frames(uint64_t count);
		void do_append_frames();
		void do_insert_frame_after(uint64_t row, bool multi);
		void do_delete_frame(uint64_t row1, uint64_t row2, bool wholeframe);
		void do_truncate(uint64_t row);
		void do_set_stop_at_frame();
		void do_scroll_to_frame();
		void do_scroll_to_current_frame();
		void do_copy(uint64_t row1, uint64_t row2, unsigned port, unsigned controller);
		void do_copy(uint64_t row1, uint64_t row2);
		void do_cut(uint64_t row1, uint64_t row2, unsigned port, unsigned controller);
		void do_cut(uint64_t row1, uint64_t row2);
		void do_paste(uint64_t row, unsigned port, unsigned controller, bool append);
		void do_paste(uint64_t row, bool append);
		void do_insert_controller(uint64_t row, unsigned port, unsigned controller);
		void do_delete_controller(uint64_t row1, uint64_t row2, unsigned port, unsigned controller);
		uint64_t first_editable(unsigned index);
		uint64_t first_nextframe();
		int width(portctrl::frame& f);
		std::u32string render_line1(portctrl::frame& f);
		std::u32string render_line2(portctrl::frame& f);
		void render_linen(text_framebuffer& fb, portctrl::frame& f, uint64_t sfn, int y);
		emulator_instance& inst;
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
		unsigned new_width;
		unsigned new_height;
		std::vector<uint8_t> pixels;
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
		std::map<int, std::string> branch_names;
	};
	emulator_instance& inst;
	_moviepanel* moviepanel;
	wxButton* closebutton;
	scroll_bar* moviescroll;
	bool closing;
};

namespace
{
	instance_map<wxeditor_movie> movieeditors;

	//Find the first real editable subframe.
	//Call only in emulator thread.
	uint64_t real_first_editable(frame_controls& fc, unsigned idx)
	{
		uint64_t cffs = CORE().mlogic->get_movie().get_current_frame_first_subframe();
		portctrl::frame_vector& fv = *CORE().mlogic->get_mfile().input;
		portctrl::counters& pv = CORE().mlogic->get_movie().get_pollcounters();
		uint64_t vsize = fv.size();
		uint32_t pc = fc.read_pollcount(pv, idx);
		for(uint32_t i = 1; i < pc; i++)
			if(cffs + i >= vsize || fv[cffs + i].sync())
				return cffs + i;
		return cffs + pc;
	}

	uint64_t real_first_editable(frame_controls& fc, std::set<unsigned> idx)
	{
		uint64_t m = 0;
		for(auto i : idx)
			m = max(m, real_first_editable(fc, i));
		return m;
	}

	//Find the first real editable whole frame.
	//Call only in emulator thread.
	uint64_t real_first_nextframe(frame_controls& fc)
	{
		uint64_t base = real_first_editable(fc, 0);
		portctrl::frame_vector& fv = *CORE().mlogic->get_mfile().input;
		uint64_t vsize = fv.size();
		for(uint32_t i = 0;; i++)
			if(base + i >= vsize || fv[base + i].sync())
				return base + i;
	}
}

wxeditor_movie::_moviepanel::~_moviepanel() throw() {}
wxeditor_movie::~wxeditor_movie() throw()
{
	movieeditors.remove(inst);
}

wxeditor_movie::_moviepanel::_moviepanel(wxeditor_movie* v, emulator_instance& _inst)
	: wxPanel(v, wxID_ANY, wxDefaultPosition, wxSize(100, 100), wxWANTS_CHARS), inst(_inst)
{
	CHECK_UI_THREAD;
	m = v;
	Connect(wxEVT_PAINT, wxPaintEventHandler(_moviepanel::on_paint), NULL, this);
	Connect(wxEVT_ERASE_BACKGROUND, wxEraseEventHandler(_moviepanel::on_erase), NULL, this);
	new_width = 0;
	new_height = 0;
	moviepos = 0;
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
	movie& m = inst.mlogic->get_movie();
	portctrl::frame_vector& fv = *inst.mlogic->get_mfile().input;
	if(&m == prev_obj && prev_seqno == m.get_seqno()) {
		//Just process new subframes if any.
		for(uint64_t i = max_subframe; i < fv.size(); i++) {
			uint64_t prev = (i > 0) ? subframe_to_frame[i - 1] : 0;
			portctrl::frame f = fv[i];
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
		portctrl::frame f = fv[i];
		if(f.sync())
			subframe_to_frame[i] = prev + 1;
		else
			subframe_to_frame[i] = prev;
	}
	max_subframe = fv.size();
	portctrl::frame model = fv.blank_frame(false);
	fcontrols.set_types(model);
	prev_obj = &m;
	prev_seqno = m.get_seqno();
}

int wxeditor_movie::_moviepanel::width(portctrl::frame& f)
{
	update_cache();
	return divcnt + 1 + fcontrols.width();
}

std::u32string wxeditor_movie::_moviepanel::render_line1(portctrl::frame& f)
{
	update_cache();
	return fcontrols.line1();
}

std::u32string wxeditor_movie::_moviepanel::render_line2(portctrl::frame& f)
{
	update_cache();
	return fcontrols.line2();
}

void wxeditor_movie::_moviepanel::render_linen(text_framebuffer& fb, portctrl::frame& f, uint64_t sfn, int y)
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
	uint64_t curframe = inst.mlogic->get_movie().get_current_frame();
	portctrl::counters& pv = inst.mlogic->get_movie().get_pollcounters();
	uint64_t cffs = inst.mlogic->get_movie().get_current_frame_first_subframe();
	cached_cffs = cffs;
	int past = -1;
	if(!inst.mlogic->get_movie().readonly_mode())
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
	portctrl::frame_vector& fv = *inst.mlogic->get_mfile().input;
	portctrl::frame cf = fv.blank_frame(false);
	int _width = width(cf);
	fb.set_size(_width, lines_to_display + 3);
	size_t fbstride = fb.get_stride();
	auto fbsize = fb.get_characters();
	text_framebuffer::element* _fb = fb.get_buffer();
	fb.write((stringfmt() << "Current frame: " << inst.mlogic->get_movie().get_current_frame() << " of "
		<< inst.mlogic->get_movie().get_frame_count()).str(), _width, 0, 0,
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
			portctrl::frame frame = fv[i];
			render_linen(fb, frame, i, j);
		}
	}
}

void wxeditor_movie::_moviepanel::do_toggle_buttons(unsigned idx, uint64_t row1, uint64_t row2, bool force_false)
{
	frame_controls* _fcontrols = &fcontrols;
	uint64_t _press_line = row1;
	uint64_t line = row2;
	bool _force_false = force_false;
	if(_press_line > line)
		std::swap(_press_line, line);
	recursing = true;
	inst.iqueue->run([idx, _press_line, line, _fcontrols, _force_false]() {
		if(!CORE().mlogic->get_movie().readonly_mode())
			return;
		uint64_t fedit = real_first_editable(*_fcontrols, idx);
		portctrl::frame_vector& fv = *CORE().mlogic->get_mfile().input;
		portctrl::frame_vector::notify_freeze freeze(fv);
		for(uint64_t i = _press_line; i <= line; i++) {
			if(i < fedit || i >= fv.size())
				continue;
			portctrl::frame cf = fv[i];
			if(!_force_false)
				_fcontrols->write_index(cf, idx, !_fcontrols->read_index(cf, idx));
			else
				_fcontrols->write_index(cf, idx, 0);
		}
	});
	recursing = false;
	if(idx == 0)
		max_subframe = _press_line;	//Reparse.
	signal_repaint();
}

void wxeditor_movie::_moviepanel::do_alter_axis(unsigned idx, uint64_t row1, uint64_t row2)
{
	CHECK_UI_THREAD;
	frame_controls* _fcontrols = &fcontrols;
	uint64_t line = row1;
	uint64_t line2 = row2;
	short value;
	bool valid = true;
	inst.iqueue->run([idx, line, &value, _fcontrols, &valid]() {
		if(!CORE().mlogic->get_movie().readonly_mode()) {
			valid = false;
			return;
		}
		uint64_t fedit = real_first_editable(*_fcontrols, idx);
		portctrl::frame_vector& fv = *CORE().mlogic->get_mfile().input;
		if(line < fedit || line >= fv.size()) {
			valid = false;
			return;
		}
		portctrl::frame_vector::notify_freeze freeze(fv);
		portctrl::frame cf = fv[line];
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
	inst.iqueue->run([idx, line, line2, value, _fcontrols]() {
		uint64_t fedit = real_first_editable(*_fcontrols, idx);
		portctrl::frame_vector& fv = *CORE().mlogic->get_mfile().input;
		portctrl::frame_vector::notify_freeze freeze(fv);
		for(uint64_t i = line; i <= line2; i++) {
			if(i < fedit || i >= fv.size())
				continue;
			portctrl::frame cf = fv[i];
			_fcontrols->write_index(cf, idx, value);
		}
	});
	signal_repaint();
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
	inst.iqueue->run([idx, line, line2, &value, &value2, _fcontrols, &valid]() {
		if(!CORE().mlogic->get_movie().readonly_mode()) {
			valid = false;
			return;
		}
		uint64_t fedit = real_first_editable(*_fcontrols, idx);
		portctrl::frame_vector& fv = *CORE().mlogic->get_mfile().input;
		if(line2 < fedit || line2 >= fv.size()) {
			valid = false;
			return;
		}
		portctrl::frame cf = fv[line];
		value = _fcontrols->read_index(cf, idx);
		portctrl::frame cf2 = fv[line2];
		value2 = _fcontrols->read_index(cf2, idx);
	});
	if(!valid)
		return;
	inst.iqueue->run([idx, line, line2, value, value2, _fcontrols]() {
		uint64_t fedit = real_first_editable(*_fcontrols, idx);
		portctrl::frame_vector& fv = *CORE().mlogic->get_mfile().input;
		portctrl::frame_vector::notify_freeze freeze(fv);
		for(uint64_t i = line + 1; i <= line2 - 1; i++) {
			if(i < fedit || i >= fv.size())
				continue;
			portctrl::frame cf = fv[i];
			auto tmp2 = static_cast<int64_t>(i - line) * (value2 - value) /
				static_cast<int64_t>(line2 - line);
			short tmp = value + tmp2;
			_fcontrols->write_index(cf, idx, tmp);
		}
	});
	signal_repaint();
}

void wxeditor_movie::_moviepanel::do_append_frames(uint64_t count)
{
	recursing = true;
	uint64_t _count = count;
	inst.iqueue->run([_count]() {
		if(!CORE().mlogic->get_movie().readonly_mode())
			return;
		portctrl::frame_vector& fv = *CORE().mlogic->get_mfile().input;
		portctrl::frame_vector::notify_freeze freeze(fv);
		for(uint64_t i = 0; i < _count; i++)
			fv.append(fv.blank_frame(true));
	});
	recursing = false;
	signal_repaint();
}

void wxeditor_movie::_moviepanel::do_append_frames()
{
	CHECK_UI_THREAD;
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
	signal_repaint();
}

void wxeditor_movie::_moviepanel::do_insert_frame_after(uint64_t row, bool multi)
{
	CHECK_UI_THREAD;
	uint64_t multicount = 1;
	if(multi) {
		try {
			std::string text = pick_text(m, "Append frames", "Enter number of frames to insert:", "");
			multicount = parse_value<uint64_t>(text);
		} catch(canceled_exception& e) {
			return;
		} catch(std::exception& e) {
			wxMessageBox(wxT("Invalid value"), _T("Error"), wxICON_EXCLAMATION | wxOK, m);
			return;
		}
	}
	recursing = true;
	frame_controls* _fcontrols = &fcontrols;
	uint64_t _row = row;
	inst.iqueue->run([_row, _fcontrols, multicount]() {
		if(!CORE().mlogic->get_movie().readonly_mode())
			return;
		portctrl::frame_vector& fv = *CORE().mlogic->get_mfile().input;
		uint64_t fedit = real_first_editable(*_fcontrols, 0);
		//Find the start of the next frame.
		uint64_t nframe = _row + 1;
		uint64_t vsize = fv.size();
		while(nframe < vsize && !fv[nframe].sync())
			nframe++;
		if(nframe < fedit)
			return;
		portctrl::frame_vector::notify_freeze freeze(fv);
		for(uint64_t k = 0; k < multicount; k++)
			fv.append(fv.blank_frame(true));
		if(nframe < vsize) {
			//Okay, gotta copy all data after this point. nframe has to be at least 1.
			for(uint64_t i = vsize - 1; i >= nframe; i--)
				fv[i + multicount] = fv[i];
			for(uint64_t k = 0; k < multicount; k++)
				fv[nframe + k] = fv.blank_frame(true);
		}
	});
	max_subframe = row;
	recursing = false;
	signal_repaint();
}

void wxeditor_movie::_moviepanel::do_delete_frame(uint64_t row1, uint64_t row2, bool wholeframe)
{
	recursing = true;
	uint64_t _row1 = row1;
	uint64_t _row2 = row2;
	bool _wholeframe = wholeframe;
	frame_controls* _fcontrols = &fcontrols;
	if(_row1 > _row2) std::swap(_row1, _row2);
	inst.iqueue->run([_row1, _row2, _wholeframe, _fcontrols]() {
		portctrl::frame_vector& fv = *CORE().mlogic->get_mfile().input;
		uint64_t vsize = fv.size();
		if(_row1 >= vsize)
			return;		//Nothing to do.
		portctrl::frame_vector::notify_freeze freeze(fv);
		uint64_t row2 = min(_row2, vsize - 1);
		uint64_t row1 = min(_row1, vsize - 1);
		row1 = max(row1, real_first_editable(*_fcontrols, 0));
		if(_wholeframe) {
			if(_row2 < real_first_nextframe(*_fcontrols))
				return;		//Nothing to do.
			//Scan backwards for the first subframe of this frame and forwards for the last.
			uint64_t fsf = row1;
			uint64_t lsf = row2;
			if(fv[_row2].sync())
				lsf++;		//Bump by one so it finds the end.
			while(fsf < vsize && !fv[fsf].sync())
				fsf--;
			while(lsf < vsize && !fv[lsf].sync())
				lsf++;
			fsf = max(fsf, real_first_editable(*_fcontrols, 0));
			uint64_t tonuke = lsf - fsf;
			int64_t frames_tonuke = 0;
			//Count frames nuked.
			for(uint64_t i = fsf; i < lsf; i++)
				if(fv[i].sync())
					frames_tonuke++;
			//Nuke from fsf to lsf.
			for(uint64_t i = fsf; i < vsize - tonuke; i++)
				fv[i] = fv[i + tonuke];
			fv.resize(vsize - tonuke);
		} else {
			if(row2 < real_first_editable(*_fcontrols, 0))
				return;		//Nothing to do.
			//The sync flag needs to be inherited if:
			//1) Some deleted subframe has sync flag AND
			//2) The subframe immediately after deleted region doesn't.
			bool inherit_sync = false;
			for(uint64_t i = row1; i <= row2; i++)
				inherit_sync = inherit_sync || fv[i].sync();
			inherit_sync = inherit_sync && (row2 + 1 < vsize && !fv[_row2 + 1].sync());
			int64_t frames_tonuke = 0;
			//Count frames nuked.
			for(uint64_t i = row1; i <= row2; i++)
				if(fv[i].sync())
					frames_tonuke++;
			//If sync is inherited, one less frame is nuked.
			if(inherit_sync) frames_tonuke--;
			//Nuke the subframes.
			uint64_t tonuke = row2 - row1 + 1;
			for(uint64_t i = row1; i < vsize - tonuke; i++)
				fv[i] = fv[i + tonuke];
			fv.resize(vsize - tonuke);
			//Next subframe inherits the sync flag.
			if(inherit_sync)
				fv[row1].sync(true);
		}
	});
	max_subframe = _row1;
	recursing = false;
	signal_repaint();
}

void wxeditor_movie::_moviepanel::do_truncate(uint64_t row)
{
	recursing = true;
	uint64_t _row = row;
	frame_controls* _fcontrols = &fcontrols;
	inst.iqueue->run([_row, _fcontrols]() {
		portctrl::frame_vector& fv = *CORE().mlogic->get_mfile().input;
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
	});
	max_subframe = row;
	recursing = false;
	signal_repaint();
}

void wxeditor_movie::_moviepanel::do_set_stop_at_frame()
{
	CHECK_UI_THREAD;
	uint64_t curframe;
	uint64_t frame;
	inst.iqueue->run([&curframe]() {
		curframe = CORE().mlogic->get_movie().get_current_frame();
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
	inst.iqueue->run([frame]() {
		set_stop_at_frame(frame);
	});
}

void wxeditor_movie::_moviepanel::on_mouse0(unsigned x, unsigned y, bool polarity, bool shift, unsigned X, unsigned Y)
{
	CHECK_UI_THREAD;
	if(y < 3)
		return;
	if(polarity) {
		press_x = x;
		press_line = spos + y - 3;
	}
	pressed = polarity;
	if(polarity) {
		signal_repaint();
		return;
	}
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
				do_toggle_buttons(idx, press_line, line, false);
			else if(i.type == 1) {
				if(shift) {
					if(press_line == line && (i.port || i.controller))
						try {
							wxPoint spos = GetScreenPosition();
							popup_axis_panel(line, i, spos.x + X, spos.y + Y);
						} catch(canceled_exception& e) {
						}
				} else
					do_alter_axis(idx, press_line, line);
			}
		}
	}
}

void wxeditor_movie::_moviepanel::popup_axis_panel(uint64_t row, control_info ci, unsigned screenX, unsigned screenY)
{
	CHECK_UI_THREAD;
	control_info ciX;
	control_info ciY;
	control_info ci2 = find_paired(ci, fcontrols.get_controlinfo());
	if(ci.index == ci2.index) {
		ciX = ciY = ci;
	} else if(ci2.index < ci.index) {
		ciX = ci2;
		ciY = ci;
	} else {
		ciX = ci;
		ciY = ci2;
	}
	frame_controls* _fcontrols = &fcontrols;
	if(ciX.index == ciY.index) {
		auto c = prompt_coodinates_window(m, NULL, 256, 0, ciX, ciX, screenX, screenY);
		inst.iqueue->run([ciX, row, c, _fcontrols]() {
			uint64_t fedit = real_first_editable(*_fcontrols, ciX.index);
			if(row < fedit) return;
			portctrl::frame_vector& fv = *CORE().mlogic->get_mfile().input;
			portctrl::frame cf = fv[row];
			_fcontrols->write_index(cf, ciX.index, c.first);
		});
		signal_repaint();
	} else if(ci.axistype == portctrl::button::TYPE_LIGHTGUN) {
		framebuffer::raw& _fb = inst.fbuf->render_get_latest_screen();
		framebuffer::fb<false> fb;
		auto osize = std::make_pair(_fb.get_width(), _fb.get_height());
		auto size = inst.rom->lightgun_scale();
		fb.reallocate(osize.first, osize.second, false);
		fb.copy_from(_fb, 1, 1);
		inst.fbuf->render_get_latest_screen_end();
		std::vector<uint8_t> buf;
		buf.resize(3 * (ciX.rmax - ciX.rmin + 1) * (ciY.rmax - ciY.rmin + 1));
		unsigned offX = -ciX.rmin;
		unsigned offY = -ciY.rmin;
		struct SwsContext* ctx = sws_getContext(osize.first, osize.second, AV_PIX_FMT_RGBA,
			size.first, size.second, AV_PIX_FMT_BGR24, SWS_POINT, NULL, NULL, NULL);
		uint8_t* srcp[1];
		int srcs[1];
		uint8_t* dstp[1];
		int dsts[1];
		srcs[0] = 4 * (fb.rowptr(1) - fb.rowptr(0));
		dsts[0] = 3 * (ciX.rmax - ciX.rmin + 1);
		srcp[0] = reinterpret_cast<unsigned char*>(fb.rowptr(0));
		dstp[0] = &buf[3 * (offY * (ciX.rmax - ciX.rmin + 1) + offX)];
		memset(&buf[0], 0, buf.size());
		sws_scale(ctx, srcp, srcs, 0, size.second, dstp, dsts);
		sws_freeContext(ctx);
		auto c = prompt_coodinates_window(m, &buf[0], (ciX.rmax - ciX.rmin + 1), (ciY.rmax - ciY.rmin + 1),
			ciX, ciY, screenX, screenY);
		inst.iqueue->run([ciX, ciY, row, c, _fcontrols]() {
			uint64_t fedit = real_first_editable(*_fcontrols, ciX.index);
			fedit = max(fedit, real_first_editable(*_fcontrols, ciY.index));
			if(row < fedit) return;
			portctrl::frame_vector& fv = *CORE().mlogic->get_mfile().input;
			portctrl::frame cf = fv[row];
			_fcontrols->write_index(cf, ciX.index, c.first);
			_fcontrols->write_index(cf, ciY.index, c.second);
		});
		signal_repaint();
	} else {
		auto c = prompt_coodinates_window(m, NULL, 256, 256, ciX, ciY, screenX, screenY);
		inst.iqueue->run([ciX, ciY, row, c, _fcontrols]() {
			uint64_t fedit = real_first_editable(*_fcontrols, ciX.index);
			fedit = max(fedit, real_first_editable(*_fcontrols, ciY.index));
			if(row < fedit) return;
			portctrl::frame_vector& fv = *CORE().mlogic->get_mfile().input;
			portctrl::frame cf = fv[row];
			_fcontrols->write_index(cf, ciX.index, c.first);
			_fcontrols->write_index(cf, ciY.index, c.second);
		});
		signal_repaint();
	}
}

void wxeditor_movie::_moviepanel::do_scroll_to_frame()
{
	CHECK_UI_THREAD;
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
	uint64_t wouldbe = 0;
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
	CHECK_UI_THREAD;
	wxMenuItem* tmpitem;
	int id = e.GetId();

	unsigned port = 0;
	unsigned controller = 0;
	for(auto i : fcontrols.get_controlinfo())
		if(i.index == press_index) {
		port = i.port;
		controller = i.controller;
	}

	switch(id) {
	case wxID_TOGGLE:
		do_toggle_buttons(press_index, rpress_line, press_line, false);
		return;
	case wxID_CHANGE:
		do_alter_axis(press_index, rpress_line, press_line);
		return;
	case wxID_CLEAR:
		do_toggle_buttons(press_index, rpress_line, press_line, true);
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
		do_insert_frame_after(press_line, false);
		return;
	case wxID_INSERT_AFTER_MULTIPLE:
		do_insert_frame_after(press_line, true);
		return;
	case wxID_DELETE_FRAME:
		do_delete_frame(press_line, rpress_line, true);
		return;
	case wxID_DELETE_SUBFRAME:
		do_delete_frame(press_line, rpress_line, false);
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
			m->get_scroll()->set_page_size(lines_to_display);
		} catch(canceled_exception& e) {
			return;
		} catch(std::exception& e) {
			wxMessageBox(wxT("Invalid value"), _T("Error"), wxICON_EXCLAMATION | wxOK, m);
			return;
		}
		signal_repaint();
		return;
	case wxID_COPY_FRAMES:
		if(press_index == std::numeric_limits<unsigned>::max())
			do_copy(rpress_line, press_line);
		else
			do_copy(rpress_line, press_line, port, controller);
		return;
	case wxID_CUT_FRAMES:
		if(press_index == std::numeric_limits<unsigned>::max())
			do_cut(rpress_line, press_line);
		else
			do_cut(rpress_line, press_line, port, controller);
		return;
	case wxID_PASTE_FRAMES:
		if(press_index == std::numeric_limits<unsigned>::max() || clipboard_get_data_type() == 1)
			do_paste(press_line, false);
		else
			do_paste(press_line, port, controller, false);
		return;
	case wxID_PASTE_APPEND:
		if(press_index == std::numeric_limits<unsigned>::max() || clipboard_get_data_type() == 1)
			do_paste(press_line, true);
		else
			do_paste(press_line, port, controller, true);
		return;
	case wxID_INSERT_CONTROLLER_AFTER:
		if(press_index == std::numeric_limits<unsigned>::max())
			;
		else
			do_insert_controller(press_line, port, controller);
		return;
	case wxID_DELETE_CONTROLLER_SUBFRAMES:
		if(press_index == std::numeric_limits<unsigned>::max())
			;
		else
			do_delete_controller(press_line, rpress_line, port, controller);
		return;
	case wxID_MBRANCH_NEW:
		try {
			std::string newname;
			std::string oldname;
			inst.iqueue->run([&oldname]() { oldname = CORE().mbranch->get(); });
			newname = pick_text(this, "Enter new branch name", "Enter name for a new branch (to fork "
				"from " + inst.mbranch->name(oldname) + "):", "", false);
			inst.iqueue->run_async([this, oldname, newname] {
				CORE().mbranch->_new(newname, oldname);
			}, [this](std::exception& e) {
				show_exception_any(this, "Error creating branch", "Can't create branch", e);
			});
		} catch(canceled_exception& e) {
		}
		return;
	case wxID_MBRANCH_IMPORT:
		try {
			int mode;
			std::string filename;
			std::string branch;
			std::string dbranch;
			auto g = choose_file_load(this, "Choose file to import", UI_get_project_moviepath(inst),
				exp_imp_type());
			filename = g.first;
			mode = g.second;
			if(mode == MBRANCH_IMPORT_MOVIE) {
				std::set<std::string> brlist;
				try {
					inst.iqueue->run([this, filename, &brlist]() {
						brlist = CORE().mbranch->_movie_branches(filename);
					});
				} catch(std::exception& e) {
					show_exception(this, "Can't get branches in movie", "", e);
					return;
				}
				if(brlist.size() == 0) {
					show_message_ok(this, "No branches in movie file",
						"Can't import movie file as it has no branches", wxICON_EXCLAMATION);
					return;
				} else if(brlist.size() == 1) {
					branch = *brlist.begin();
				} else {
					std::vector<std::string> choices(brlist.begin(), brlist.end());
					branch = pick_among_branches(this, "Select branch to import",
						"Select branch to import", choices);
				}
				//Import from movie.
			}
			dbranch = pick_text(this, "Enter new branch name", "Enter name for an imported branch:",
				branch, false);
			inst.iqueue->run_async([this, filename, branch, dbranch, mode]() {
				CORE().mbranch->import_branch(filename, branch, dbranch, mode);
			}, [this](std::exception& e) {
				show_exception_any(this, "Can't import branch", "", e);
			});
		} catch(canceled_exception& e) {
		}
		return;
	case wxID_MBRANCH_EXPORT:
		try {
			int mode;
			std::string file;
			auto g = choose_file_save(this, "Choose file to export", UI_get_project_moviepath(inst),
				exp_imp_type());
			file = g.first;
			mode = g.second;
			inst.iqueue->run_async([this, file, mode]() {
				std::string bname = CORE().mbranch->get();
				CORE().mbranch->export_branch(file, bname, mode == MBRANCH_IMPORT_BINARY);
			}, [this](std::exception& e) {
				show_exception_any(this, "Can't export branch", "", e);
			});
		} catch(canceled_exception& e) {
		}
		return;
	case wxID_MBRANCH_RENAME:
		try {
			std::string newname;
			std::string oldname;
			std::set<std::string> list;
			inst.iqueue->run([&list]() { list = CORE().mbranch->enumerate(); });
			std::vector<std::string> choices(list.begin(), list.end());
			oldname = pick_among_branches(this, "Select branch to rename", "Select branch to rename",
				choices);
			newname = pick_text(this, "Enter new branch name", "Enter name for a new branch (to rename "
				"'" + inst.mbranch->name(oldname) + "'):", oldname, false);
			inst.iqueue->run_async([this, oldname, newname] {
				CORE().mbranch->rename(oldname, newname);
			}, [this](std::exception& e) {
				show_exception_any(this, "Error renaming branch", "Can't rename branch", e);
			});
		} catch(canceled_exception& e) {
		}
		return;
	case wxID_MBRANCH_DELETE:
		try {
			std::string oldname;
			std::set<std::string> list;
			inst.iqueue->run([&list]() { list = CORE().mbranch->enumerate(); });
			std::vector<std::string> choices(list.begin(), list.end());
			oldname = pick_among_branches(this, "Select branch to delete", "Select branch to delete",
				choices);
			inst.iqueue->run_async([this, oldname] {
				CORE().mbranch->_delete(oldname);
			}, [this](std::exception& e) {
				show_exception_any(this, "Error deleting branch", "Can't delete branch", e);
			});
		} catch(canceled_exception& e) {
		}
		return;
	};
	if(id >= wxID_MBRANCH_FIRST && id <= wxID_MBRANCH_LAST) {
		if(!branch_names.count(id)) return;
		std::string name = branch_names[id];
		inst.iqueue->run_async([this, name]() {
			CORE().mbranch->set(name);
		}, [this](std::exception& e) {
			show_exception_any(this, "Error changing branch", "Can't change branch", e);
		});
	}
}

uint64_t wxeditor_movie::_moviepanel::first_editable(unsigned index)
{
	uint64_t cffs = cached_cffs;
	if(!subframe_to_frame.count(cffs))
		return cffs;
	uint64_t f = subframe_to_frame[cffs];
	portctrl::counters& pv = inst.mlogic->get_movie().get_pollcounters();
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
	CHECK_UI_THREAD;
	if(polarity) {
		//Pressing mouse, just record line it was pressed on.
		rpress_line = spos + y - 3;
		press_x = x;
		pressed = true;
		signal_repaint();
		return;
	}
	//Releasing mouse, open popup menu.
	pressed = false;
	unsigned off = divcnt + 1;
	press_x = x;
	if(y < 3) {
		signal_repaint();
		return;
	}
	press_line = spos + y - 3;
	wxMenu menu;
	current_popup = &menu;
	menu.Connect(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(wxeditor_movie::_moviepanel::on_popup_menu),
		NULL, this);

	//Find what controller is the click on.
	bool clicked_button = false;
	control_info clicked;
	std::string controller_name;
	if(press_x < off) {
		clicked_button = false;
		press_index = std::numeric_limits<unsigned>::max();
	} else {
		for(auto i : fcontrols.get_controlinfo())
			if(press_x >= i.position_left + off && press_x < i.position_left + i.reserved + off) {
				if(i.type == 0 || i.type == 1) {
					clicked_button = true;
					clicked = i;
					controller_name = (stringfmt() << "controller " << i.port << "-"
						<< (i.controller + 1)).str();
					press_index = i.index;
				}
			}
	}

	//Find first editable frame, controllerframe and buttonframe.
	bool not_editable = !inst.mlogic->get_movie().readonly_mode();
	uint64_t eframe_low = first_editable(0);
	uint64_t ebutton_low = clicked_button ? first_editable(clicked.index) : std::numeric_limits<uint64_t>::max();
	uint64_t econtroller_low = ebutton_low;
	for(auto i : fcontrols.get_controlinfo())
		if(i.port == clicked.port && i.controller == clicked.controller && (i.type == 0 || i.type == 1))
			econtroller_low = max(econtroller_low, first_editable(i.index));

	bool click_zero = (clicked_button && !clicked.port && !clicked.controller);
	bool enable_append_frame = !not_editable;
	bool enable_toggle_button = false;
	bool enable_change_axis = false;
	bool enable_sweep_axis = false;
	bool enable_insert_frame = false;
	bool enable_insert_controller = false;
	bool enable_delete_frame = false;
	bool enable_delete_subframe = false;
	bool enable_delete_controller_subframe = false;
	bool enable_truncate_movie = false;
	bool enable_cut_frame = false;
	bool enable_copy_frame = false;
	bool enable_paste_frame = false;
	bool enable_paste_append = false;
	std::string copy_title;
	std::string paste_title;

	//Toggle button is enabled if clicked on button and either end is in valid range.
	enable_toggle_button = (!not_editable && clicked_button && clicked.type == 0 && ((press_line >= ebutton_low &&
		press_line < linecount) || (rpress_line >= ebutton_low && rpress_line < linecount)));
	//Change axis is enabled in similar conditions, except if type is axis.
	enable_change_axis = (!not_editable && clicked_button && clicked.type == 1 && ((press_line >= ebutton_low &&
		press_line < linecount) || (rpress_line >= ebutton_low && rpress_line < linecount)));
	//Sweep axis is enabled if change axis is enabled and lines don't match.
	enable_sweep_axis = (enable_change_axis && press_line != rpress_line);
	//Insert frame is enabled if this frame is completely editable and press and release lines match.
	enable_insert_frame = (!not_editable && press_line + 1 >= eframe_low && press_line < linecount &&
		press_line == rpress_line);
	//Insert controller frame is enabled if controller is completely editable and lines match.
	enable_insert_controller = (!not_editable && clicked_button && press_line >= econtroller_low &&
		press_line < linecount && press_line == rpress_line);
	enable_insert_controller = enable_insert_controller && (clicked.port || clicked.controller);
	//Delete frame is enabled if range is completely editable (relative to next-frame).
	enable_delete_frame = (!not_editable && press_line >= first_nextframe() && press_line < linecount &&
		rpress_line >= first_nextframe() && rpress_line < linecount);
	//Delete subframe is enabled if range is completely editable.
	enable_delete_subframe = (!not_editable && press_line >= eframe_low && press_line < linecount &&
		rpress_line >= eframe_low && rpress_line < linecount);
	//Delete controller subframe is enabled if range is completely controller-editable.
	enable_delete_controller_subframe = (!not_editable && clicked_button && press_line >= econtroller_low &&
		press_line < linecount && rpress_line >= econtroller_low && rpress_line < linecount);
	enable_delete_controller_subframe = enable_delete_controller_subframe && (clicked.port || clicked.controller);
	//Truncate movie is enabled if lines match and is completely editable.
	enable_truncate_movie = (!not_editable && press_line == rpress_line && press_line >= eframe_low &&
		press_line < linecount);
	//Cut frames is enabled if range is editable (possibly controller-editable).
	if(clicked_button)
		enable_cut_frame = (!not_editable && press_line >= econtroller_low && press_line < linecount
			&& rpress_line >= econtroller_low && rpress_line < linecount && !click_zero);
	else
		enable_cut_frame = (!not_editable && press_line >= eframe_low && press_line < linecount
			&& rpress_line >= eframe_low && rpress_line < linecount);
	if(clicked_button && clipboard_get_data_type() == 0) {
		enable_paste_append = (!not_editable && linecount >= eframe_low);
		enable_paste_frame = (!not_editable && press_line >= econtroller_low && press_line < linecount
			&& rpress_line >= econtroller_low && rpress_line < linecount && !click_zero);
	} else if(clipboard_get_data_type() == 1) {
		enable_paste_append = (!not_editable && linecount >= econtroller_low);
		enable_paste_frame = (!not_editable && press_line >= eframe_low && press_line < linecount
			&& rpress_line >= eframe_low && rpress_line < linecount);
	}
	//Copy frames is enabled if range exists.
	enable_copy_frame = (press_line < linecount && rpress_line < linecount);
	copy_title = (clicked_button ? controller_name : "frames");
	paste_title = ((clipboard_get_data_type() == 0) ? copy_title : "frames");

	if(clipboard_get_data_type() == 0 && click_zero) enable_paste_append = enable_paste_frame = false;

	if(enable_toggle_button)
		menu.Append(wxID_TOGGLE, towxstring(U"Toggle " + clicked.title));
	if(enable_change_axis)
		menu.Append(wxID_CHANGE, towxstring(U"Change " + clicked.title));
	if(enable_sweep_axis)
		menu.Append(wxID_SWEEP, towxstring(U"Sweep " + clicked.title));
	if(enable_toggle_button || enable_change_axis)
		menu.Append(wxID_CLEAR, towxstring(U"Clear " + clicked.title));
	if(enable_toggle_button || enable_change_axis || enable_sweep_axis)
		menu.AppendSeparator();
	menu.Append(wxID_INSERT_AFTER, wxT("Insert frame after"))->Enable(enable_insert_frame);
	menu.Append(wxID_INSERT_AFTER_MULTIPLE, wxT("Insert frames after"))->Enable(enable_insert_frame);
	menu.Append(wxID_INSERT_CONTROLLER_AFTER, wxT("Insert controller frame"))
		->Enable(enable_insert_controller);
	menu.Append(wxID_APPEND_FRAME, wxT("Append frame"))->Enable(enable_append_frame);
	menu.Append(wxID_APPEND_FRAMES, wxT("Append frames..."))->Enable(enable_append_frame);
	menu.AppendSeparator();
	menu.Append(wxID_DELETE_FRAME, wxT("Delete frame(s)"))->Enable(enable_delete_frame);
	menu.Append(wxID_DELETE_SUBFRAME, wxT("Delete subframe(s)"))->Enable(enable_delete_subframe);
	menu.Append(wxID_DELETE_CONTROLLER_SUBFRAMES, wxT("Delete controller subframes(s)"))
		->Enable(enable_delete_controller_subframe);
	menu.AppendSeparator();
	menu.Append(wxID_TRUNCATE, wxT("Truncate movie"))->Enable(enable_truncate_movie);
	menu.AppendSeparator();
	menu.Append(wxID_CUT_FRAMES, towxstring("Cut " + copy_title))->Enable(enable_cut_frame);
	menu.Append(wxID_COPY_FRAMES, towxstring("Copy " + copy_title))->Enable(enable_copy_frame);
	menu.Append(wxID_PASTE_FRAMES, towxstring("Paste " + paste_title))->Enable(enable_paste_frame);
	menu.Append(wxID_PASTE_APPEND, towxstring("Paste append " + paste_title))->Enable(enable_paste_append);
	menu.AppendSeparator();
	menu.Append(wxID_SCROLL_FRAME, wxT("Scroll to frame..."));
	menu.Append(wxID_SCROLL_CURRENT_FRAME, wxT("Scroll to current frame"));
	menu.Append(wxID_RUN_TO_FRAME, wxT("Run to frame..."));
	menu.Append(wxID_CHANGE_LINECOUNT, wxT("Change number of lines visible"));
	menu.AppendCheckItem(wxID_POSITION_LOCK, wxT("Lock scroll to playback"))->Check(position_locked);
	menu.AppendSeparator();

	wxMenu* branches_submenu = new wxMenu();
	branches_submenu->Append(wxID_MBRANCH_NEW, wxT("New branch..."));
	branches_submenu->Append(wxID_MBRANCH_IMPORT, wxT("Import branch..."));
	branches_submenu->Append(wxID_MBRANCH_EXPORT, wxT("Export branch..."));
	branches_submenu->Append(wxID_MBRANCH_RENAME, wxT("Rename branch..."));
	branches_submenu->Append(wxID_MBRANCH_DELETE, wxT("Delete branch..."));
	branches_submenu->AppendSeparator();
	std::set<std::string> list;
	std::string current;
	bool ro;
	inst.iqueue->run([&list, &current, &ro]() {
		list = CORE().mbranch->enumerate();
		current = CORE().mbranch->get();
		ro = CORE().mlogic->get_movie().readonly_mode();
	});
	int ass_id = wxID_MBRANCH_FIRST;
	for(auto i : list) {
		bool selected = (i == current);
		wxMenuItem* it;
		it = branches_submenu->AppendCheckItem(ass_id, towxstring(inst.mbranch->name(i)));
		branch_names[ass_id++] = i;
		if(selected) it->Check(selected);
		it->Enable(ro);
	}
	menu.AppendSubMenu(branches_submenu, wxT("Branches"));
	menu.Connect(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(wxeditor_movie::_moviepanel::on_popup_menu),
		NULL, this);
	branches_submenu->Connect(wxEVT_COMMAND_MENU_SELECTED,
		wxCommandEventHandler(wxeditor_movie::_moviepanel::on_popup_menu), NULL, this);
	PopupMenu(&menu);
	//delete branches_submenu;
	signal_repaint();
}

int wxeditor_movie::_moviepanel::get_lines()
{
	portctrl::frame_vector& fv = *inst.mlogic->get_mfile().input;
	return fv.size();
}

void wxeditor_movie::_moviepanel::signal_repaint()
{
	CHECK_UI_THREAD;
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
	inst.iqueue->run([&lines, &width, &height, m2, this]() {
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
	if(s) {
		s->set_range(lines);
		s->set_position(moviepos);
	}
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
	CHECK_UI_THREAD;
	auto cell = fb.get_cell();
	if(e.LeftDown() && !e.ControlDown())
		on_mouse0(e.GetX() / cell.first, e.GetY() / cell.second, true, e.ShiftDown(), e.GetX(), e.GetY());
	if(e.LeftUp() && !e.ControlDown())
		on_mouse0(e.GetX() / cell.first, e.GetY() / cell.second, false, e.ShiftDown(), e.GetX(), e.GetY());
	if(e.MiddleDown())
		on_mouse1(e.GetX() / cell.first, e.GetY() / cell.second, true);
	if(e.MiddleUp())
		on_mouse1(e.GetX() / cell.first, e.GetY() / cell.second, false);
	if(e.RightDown() || (e.LeftDown() && e.ControlDown()))
		on_mouse2(e.GetX() / cell.first, e.GetY() / cell.second, true);
	if(e.RightUp() || (e.LeftUp() && e.ControlDown()))
		on_mouse2(e.GetX() / cell.first, e.GetY() / cell.second, false);
	auto s = m->get_scroll();
	unsigned speed = 1;
	if(e.ShiftDown())
		speed = 10;
	if(e.ShiftDown() && e.ControlDown())
		speed = 50;
	s->apply_wheel(e.GetWheelRotation(), e.GetWheelDelta(), speed);
}

void wxeditor_movie::_moviepanel::on_erase(wxEraseEvent& e)
{
	//Blank.
}

void wxeditor_movie::_moviepanel::on_paint(wxPaintEvent& e)
{
	CHECK_UI_THREAD;
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

void wxeditor_movie::_moviepanel::do_copy(uint64_t row1, uint64_t row2, unsigned port, unsigned controller)
{
	frame_controls* _fcontrols = &fcontrols;
	uint64_t line = row1;
	uint64_t line2 = row2;
	if(line2 < line)
		std::swap(line, line2);
	std::string copied;
	inst.iqueue->run([port, controller, line, line2, _fcontrols, &copied]() {
		portctrl::frame_vector& fv = *CORE().mlogic->get_mfile().input;
		uint64_t vsize = fv.size();
		if(!vsize)
			return;
		uint64_t _line = min(line, vsize - 1);
		uint64_t _line2 = min(line2, vsize - 1);
		copied = encode_lines(*_fcontrols, fv, _line, _line2 + 1, port, controller);
	});
	copy_to_clipboard(copied);
}

void wxeditor_movie::_moviepanel::do_copy(uint64_t row1, uint64_t row2)
{
	uint64_t line = row1;
	uint64_t line2 = row2;
	if(line2 < line)
		std::swap(line, line2);
	std::string copied;
	inst.iqueue->run([line, line2, &copied]() {
		portctrl::frame_vector& fv = *CORE().mlogic->get_mfile().input;
		uint64_t vsize = fv.size();
		if(!vsize)
			return;
		uint64_t _line = min(line, vsize - 1);
		uint64_t _line2 = min(line2, vsize - 1);
		copied = encode_lines(fv, _line, _line2 + 1);
	});
	copy_to_clipboard(copied);
}

void wxeditor_movie::_moviepanel::do_cut(uint64_t row1, uint64_t row2, unsigned port, unsigned controller)
{
	do_copy(row1, row2, port, controller);
	do_delete_controller(row1, row2, port, controller);
}

void wxeditor_movie::_moviepanel::do_cut(uint64_t row1, uint64_t row2)
{
	do_copy(row1, row2);
	do_delete_frame(row1, row2, false);
}

void wxeditor_movie::_moviepanel::do_paste(uint64_t row, bool append)
{
	frame_controls* _fcontrols = &fcontrols;
	recursing = true;
	uint64_t _gapstart = row;
	std::string cliptext = copy_from_clipboard();
	inst.iqueue->run([_fcontrols, &cliptext, _gapstart, append]() {
		//Insert enough lines for the pasted content.
		uint64_t gapstart = _gapstart;
		if(!CORE().mlogic->get_movie().readonly_mode())
			return;
		uint64_t gaplen = 0;
		int64_t newframes = 0;
		{
			std::istringstream y(cliptext);
			std::string z;
			if(!std::getline(y, z))
				return;
			istrip_CR(z);
			if(z != "lsnes-moviedata-whole")
				return;
			while(std::getline(y, z))
				gaplen++;
		}
		portctrl::frame_vector& fv = *CORE().mlogic->get_mfile().input;
		uint64_t vsize = fv.size();
		if(gapstart < real_first_editable(*_fcontrols, 0))
			return;
		if(gapstart > vsize)
			return;
		portctrl::frame_vector::notify_freeze freeze(fv);
		if(append) gapstart = vsize;
		for(uint64_t i = 0; i < gaplen; i++)
			fv.append(fv.blank_frame(false));
		for(uint64_t i = vsize - 1; i >= gapstart && i <= vsize; i--)
			fv[i + gaplen] = fv[i];
		//Write the pasted frames.
		{
			std::istringstream y(cliptext);
			std::string z;
			std::getline(y, z);
			uint64_t idx = gapstart;
			while(std::getline(y, z)) {
				fv[idx++].deserialize(z.c_str());
				if(fv[idx - 1].sync())
					newframes++;
			}
		}
	});
	recursing = false;
	signal_repaint();
}

void wxeditor_movie::_moviepanel::do_paste(uint64_t row, unsigned port, unsigned controller, bool append)
{
	if(!port && !controller)
		return;
	frame_controls* _fcontrols = &fcontrols;
	auto iset = controller_index_set(fcontrols, port, controller);
	recursing = true;
	uint64_t _gapstart = row;
	std::string cliptext = copy_from_clipboard();
	inst.iqueue->run([_fcontrols, iset, &cliptext, _gapstart, port, controller, append]() {
		//Insert enough lines for the pasted content.
		uint64_t gapstart = _gapstart;
		if(!CORE().mlogic->get_movie().readonly_mode())
			return;
		uint64_t gaplen = 0;
		int64_t newframes = 0;
		{
			std::istringstream y(cliptext);
			std::string z;
			if(!std::getline(y, z))
				return;
			istrip_CR(z);
			if(z != "lsnes-moviedata-controller")
				return;
			while(std::getline(y, z)) {
				gaplen++;
				newframes++;
			}
		}
		portctrl::frame_vector& fv = *CORE().mlogic->get_mfile().input;
		uint64_t vsize = fv.size();
		if(gapstart < real_first_editable(*_fcontrols, iset))
			return;
		if(gapstart > vsize)
			return;
		portctrl::frame_vector::notify_freeze freeze(fv);
		if(append) gapstart = vsize;
		for(uint64_t i = 0; i < gaplen; i++)
			fv.append(fv.blank_frame(true));
		move_index_set(*_fcontrols, fv, gapstart, gapstart + gaplen, vsize - gapstart, iset);
		//Write the pasted frames.
		{
			std::istringstream y(cliptext);
			std::string z;
			std::getline(y, z);
			uint64_t idx = gapstart;
			while(std::getline(y, z)) {
				portctrl::frame f = fv[idx++];
				decode_line(*_fcontrols, f, z, port, controller);
			}
		}
	});
	recursing = false;
	signal_repaint();
}

void wxeditor_movie::_moviepanel::do_insert_controller(uint64_t row, unsigned port, unsigned controller)
{
	if(!port && !controller)
		return;
	frame_controls* _fcontrols = &fcontrols;
	auto iset = controller_index_set(fcontrols, port, controller);
	recursing = true;
	uint64_t gapstart = row;
	inst.iqueue->run([_fcontrols, iset, gapstart, port, controller]() {
		//Insert enough lines for the pasted content.
		if(!CORE().mlogic->get_movie().readonly_mode())
			return;
		portctrl::frame_vector& fv = *CORE().mlogic->get_mfile().input;
		uint64_t vsize = fv.size();
		if(gapstart < real_first_editable(*_fcontrols, iset))
			return;
		if(gapstart > vsize)
			return;
		fv.append(fv.blank_frame(true));
		move_index_set(*_fcontrols, fv, gapstart, gapstart + 1, vsize - gapstart, iset);
		zero_index_set(*_fcontrols, fv, gapstart, 1, iset);
	});
	recursing = false;
	signal_repaint();
}

void wxeditor_movie::_moviepanel::do_delete_controller(uint64_t row1, uint64_t row2, unsigned port,
	unsigned controller)
{
	if(!port && !controller)
		return;
	frame_controls* _fcontrols = &fcontrols;
	auto iset = controller_index_set(fcontrols, port, controller);
	recursing = true;
	if(row1 > row2) std::swap(row1, row2);
	uint64_t gapstart = row1;
	uint64_t gaplen = row2 - row1 + 1;
	inst.iqueue->run([_fcontrols, iset, gapstart, gaplen, port, controller]() {
		//Insert enough lines for the pasted content.
		if(!CORE().mlogic->get_movie().readonly_mode())
			return;
		portctrl::frame_vector& fv = *CORE().mlogic->get_mfile().input;
		uint64_t vsize = fv.size();
		if(gapstart < real_first_editable(*_fcontrols, iset))
			return;
		if(gapstart > vsize)
			return;
		move_index_set(*_fcontrols, fv, gapstart + gaplen, gapstart, vsize - gapstart - gaplen, iset);
		zero_index_set(*_fcontrols, fv, vsize - gaplen, gaplen, iset);
	});
	recursing = false;
	signal_repaint();
}


wxeditor_movie::wxeditor_movie(emulator_instance& _inst, wxWindow* parent)
	: wxDialog(parent, wxID_ANY, wxT("lsnes: Edit movie"), wxDefaultPosition, wxSize(-1, -1)), inst(_inst)
{
	CHECK_UI_THREAD;
	closing = false;
	Centre();
	wxFlexGridSizer* top_s = new wxFlexGridSizer(2, 1, 0, 0);
	SetSizer(top_s);

	wxBoxSizer* panel_s = new wxBoxSizer(wxHORIZONTAL);
	moviescroll = NULL;
	panel_s->Add(moviepanel = new _moviepanel(this, inst), 1, wxGROW);
	panel_s->Add(moviescroll = new scroll_bar(this, wxID_ANY, true), 0, wxGROW);
	top_s->Add(panel_s, 1, wxGROW);

	moviescroll->set_page_size(lines_to_display);
	moviescroll->set_handler([this](scroll_bar& s) {
		this->moviepanel->moviepos = s.get_position();
		this->moviepanel->signal_repaint();
	});
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
	CHECK_UI_THREAD;
	Destroy();
	closing = true;
}

void wxeditor_movie::on_wclose(wxCloseEvent& e)
{
	CHECK_UI_THREAD;
	bool wasc = closing;
	closing = true;
	if(!wasc)
		Destroy();
}

void wxeditor_movie::update()
{
	moviepanel->signal_repaint();
}

scroll_bar* wxeditor_movie::get_scroll()
{
	return moviescroll;
}

void wxeditor_movie::on_focus_wrong(wxFocusEvent& e)
{
	CHECK_UI_THREAD;
	moviepanel->SetFocus();
}

void wxeditor_movie_display(wxWindow* parent, emulator_instance& inst)
{
	CHECK_UI_THREAD;
	auto e = movieeditors.lookup(inst);
	if(e) {
		e->Raise();
		return;
	}
	movieeditors.create(inst, parent)->Show();
}

void wxeditor_movie::on_keyboard_down(wxKeyEvent& e)
{
	CHECK_UI_THREAD;
	handle_wx_keyboard(inst, e, true);
}

void wxeditor_movie::on_keyboard_up(wxKeyEvent& e)
{
	CHECK_UI_THREAD;
	handle_wx_keyboard(inst, e, false);
}

void wxeditor_movie_update(emulator_instance& inst)
{
	auto e = movieeditors.lookup(inst);
	if(e) e->update();
}
