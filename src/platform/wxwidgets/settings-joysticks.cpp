#include "platform/wxwidgets/settings-common.hpp"
#include "platform/wxwidgets/textrender.hpp"
#include "core/keymapper.hpp"
#include <wx/choicebk.h>
#include "library/minmax.hpp"

namespace
{
	const char32_t* box_symbols = 	U"\u250c\u2500\u2500\u2500\u2510"
					U"\u2502\u0020\u0020\u0020\u2502"
					U"\u2502\u0020\u0020\u0020\u2502"
					U"\u2502\u0020\u0020\u0020\u2502"
					U"\u2514\u2500\u2500\u2500\u2518";
	const char32_t* bbox_symbols = 	U"\u250c\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2510"
					U"\u2502\u0020\u0020\u0020\u0020\u0020\u0020\u0020\u0020\u2502"
					U"\u2502\u0020\u0020\u0020\u0020\u0020\u0020\u0020\u0020\u2502"
					U"\u2502\u0020\u0020\u0020\u0020\u0020\u0020\u0020\u0020\u2502"
					U"\u2514\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2518";
	unsigned sbits_lookup[] = {0020, 0002, 0040, 0004, 0200, 0202, 0400, 0404, 0010, 0001, 0050, 0005,
		0100, 0101, 0500, 0252};

	class edit_axis_properties : public wxDialog
	{
	public:
		edit_axis_properties(wxWindow* parent, unsigned _jid, unsigned _num)
			: wxDialog(parent, -1, towxstring(get_title(_jid, _num))), jid(_jid), num(_num)
		{
			CHECK_UI_THREAD;
			int64_t minus, zero, plus, neutral;
			double threshold;
			bool pressure, disabled;
			lsnes_gamepads[jid].get_calibration(num, minus, zero, plus, neutral, threshold, pressure,
				disabled);

			Centre();
			wxSizer* top_s = new wxBoxSizer(wxVERTICAL);
			SetSizer(top_s);
			wxSizer* t_s = new wxFlexGridSizer(2);
			t_s->Add(new wxStaticText(this, -1, wxT("Minus:")));
			t_s->Add(low = new wxTextCtrl(this, -1, wxT(""), wxDefaultPosition, wxSize(100, -1)), 1,
				wxGROW);
			t_s->Add(new wxStaticText(this, -1, wxT("Center:")));
			t_s->Add(mid = new wxTextCtrl(this, -1, wxT(""), wxDefaultPosition, wxSize(100, -1)), 1,
				wxGROW);
			t_s->Add(new wxStaticText(this, -1, wxT("Plus:")));
			t_s->Add(hi = new wxTextCtrl(this, -1, wxT(""), wxDefaultPosition, wxSize(100, -1)), 1,
				wxGROW);
			t_s->Add(new wxStaticText(this, -1, wxT("Neutral:")));
			t_s->Add(null = new wxTextCtrl(this, -1, wxT(""), wxDefaultPosition, wxSize(100, -1)), 1,
				wxGROW);
			t_s->Add(new wxStaticText(this, -1, wxT("Threshold:")));
			t_s->Add(thresh = new wxTextCtrl(this, -1, wxT(""), wxDefaultPosition, wxSize(100, -1)), 1,
				 wxGROW);
			t_s->Add(_disabled = new wxCheckBox(this, -1, wxT("Disabled")));
			t_s->Add(_pressure = new wxCheckBox(this, -1, wxT("Pressure")));
			top_s->Add(t_s, 1, wxGROW);

			low->SetValue(towxstring((stringfmt() << minus).str()));
			mid->SetValue(towxstring((stringfmt() << zero).str()));
			hi->SetValue(towxstring((stringfmt() << plus).str()));
			null->SetValue(towxstring((stringfmt() << neutral).str()));
			thresh->SetValue(towxstring((stringfmt() << threshold).str()));
			_pressure->SetValue(pressure);
			_disabled->SetValue(disabled);

			wxBoxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
			pbutton_s->AddStretchSpacer();
			pbutton_s->Add(okbutton = new wxButton(this, wxID_OK, wxT("OK")), 0, wxGROW);
			pbutton_s->Add(cancel = new wxButton(this, wxID_CANCEL, wxT("Cancel")), 0, wxGROW);
			okbutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
				wxCommandEventHandler(edit_axis_properties::on_ok), NULL, this);
			cancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
				wxCommandEventHandler(edit_axis_properties::on_cancel), NULL, this);
			top_s->Add(pbutton_s, 0, wxGROW);
			top_s->SetSizeHints(this);
			Fit();
		}
		~edit_axis_properties()
		{
		}
		void on_ok(wxCommandEvent& e)
		{
			CHECK_UI_THREAD;
			int64_t minus, zero, plus, neutral;
			double threshold;
			bool pressure, disabled;
			const char* bad_what = NULL;

			try {
				bad_what = "Bad low calibration value";
				minus = raw_lexical_cast<int64_t>(tostdstring(low->GetValue()));
				bad_what = "Bad middle calibration value";
				zero = raw_lexical_cast<int64_t>(tostdstring(mid->GetValue()));
				bad_what = "Bad high calibration value";
				plus = raw_lexical_cast<int32_t>(tostdstring(hi->GetValue()));
				bad_what = "Bad neutral zone width";
				neutral = raw_lexical_cast<int64_t>(tostdstring(null->GetValue()));
				bad_what = "Bad threshold (range is 0 - 1)";
				threshold = raw_lexical_cast<double>(tostdstring(thresh->GetValue()));
				if(threshold <= 0 || threshold >= 1)
					throw 42;
				pressure = _pressure->GetValue();
				disabled = _disabled->GetValue();
			} catch(...) {
				wxMessageBox(towxstring(bad_what), _T("Error"), wxICON_EXCLAMATION | wxOK);
				return;
			}
			lsnes_gamepads[jid].calibrate_axis(num, minus, zero, plus, neutral, threshold, pressure,
				disabled);
			EndModal(wxID_OK);
		}
		void on_cancel(wxCommandEvent& e)
		{
			EndModal(wxID_CANCEL);
		}
	private:
		unsigned jid;
		unsigned num;
		std::string get_title(unsigned id, unsigned num)
		{
			return (stringfmt() << "Configure axis " << num << " of joystick " << id).str();
		}
		wxTextCtrl* low;
		wxTextCtrl* mid;
		wxTextCtrl* hi;
		wxTextCtrl* null;
		wxTextCtrl* thresh;
		wxCheckBox* _disabled;
		wxCheckBox* _pressure;
		wxButton* okbutton;
		wxButton* cancel;
	};

	size_t numwidth(unsigned num)
	{
		if(num < 10)
			return 1;
		else
			return 1 + numwidth(num / 10);
	}

	class joystick_panel : public text_framebuffer_panel
	{
	public:
		joystick_panel(wxWindow* parent, emulator_instance& _inst, unsigned jid, gamepad::pad& gp)
			: text_framebuffer_panel(parent, 60, 32, -1, NULL), inst(_inst), _jid(jid), _gp(gp)
		{
			CHECK_UI_THREAD;
			const unsigned pwidth = 80;
			const unsigned b_perrow = 8;
			const unsigned s_perrow = 16;
			unsigned rows = 9 + (gp.axes() + b_perrow - 1) / b_perrow * 5 + (gp.buttons() +
				s_perrow - 1) / s_perrow * 5 + (gp.hats() + s_perrow - 1) / s_perrow * 5;

			std::string jname = (stringfmt() << "joystick" << _jid << " [" << gp.name() << "]").str();
			std::string status = std::string("Status: ") + (gp.online() ? "Online" : "Offline");
			unsigned y = 2;
			selected_row = gp.axes();


			set_size(pwidth, rows);

			write((stringfmt() << "joystick" << _jid << " [" << gp.name() << "]").str(), 256, 0, 0,
				0, 0xFFFFFF);
			write(std::string("Status: ") + (gp.online() ? "Online" : "Offline"), 256, 0, 1, 0,
				0xFFFFFF);

			if(gp.axes())
				write("Axes:", 256, 0, y++, 0, 0xFFFFFF);
			unsigned xp = 0;
			for(unsigned i = 0; i < gp.axes(); i++) {
				axes_val.push_back(std::make_pair(xp, y));
				xp += 10;
				if(xp >= pwidth) {
					xp = 0;
					y += 5;
				}
			}
			if(xp)
				y += 5;
			xp = 0;
			if(gp.buttons())
				write("Buttons:", 256, 0, y++, 0, 0xFFFFFF);
			for(unsigned i = 0; i < gp.buttons(); i++) {
				buttons_val.push_back(std::make_pair(xp, y));
				xp += 5;
				if(xp >= pwidth) {
					xp = 0;
					y += 5;
				}
			}
			if(xp)
				y += 5;
			xp = 0;
			if(gp.hats())
				write("Hats:", 256, 0, y++, 0, 0xFFFFFF);
			for(unsigned i = 0; i < gp.hats(); i++) {
				hats_val.push_back(std::make_pair(xp, y));
				xp += 5;
				if(xp >= pwidth) {
					xp = 0;
					y += 5;
				}
			}
			if(xp)
				y += 5;
			cal_row = y;
			Connect(wxEVT_LEFT_UP, wxMouseEventHandler(joystick_panel::on_mouse), NULL, this);
			Connect(wxEVT_MOTION, wxMouseEventHandler(joystick_panel::on_mouse), NULL, this);
			Fit();
		}
		~joystick_panel()
		{
		}
		void on_change(wxCommandEvent& e)
		{
		}
		void prepare_paint()
		{
			CHECK_UI_THREAD;
			for(unsigned i = 0; i < 4; i++)
				write("", 256, 0, cal_row + i, 0, 0xFFFFFF);
			for(unsigned i = 0; i < axes_val.size(); i++)
				draw_axis(axes_val[i].first, axes_val[i].second, i, axis_info(i));
			for(unsigned i = 0; i < buttons_val.size(); i++)
				draw_button(buttons_val[i].first, buttons_val[i].second, i, button_info(i));
			for(unsigned i = 0; i < hats_val.size(); i++)
				draw_hat(hats_val[i].first, hats_val[i].second, i, hat_info(i));
		}
		std::pair<unsigned, unsigned> size_needed() { return std::make_pair(width_need, height_need); }
		void on_mouse(wxMouseEvent& e)
		{
			CHECK_UI_THREAD;
			auto cell = get_cell();
			size_t x = e.GetX() / cell.first;
			size_t y = e.GetY() / cell.second;
			selected_row = axes_val.size();
			for(size_t i = 0; i < axes_val.size(); i++) {
				if(x < axes_val[i].first || x >= axes_val[i].first + 10)
					continue;
				if(y < axes_val[i].second || y >= axes_val[i].second + 5)
					continue;
				if(e.LeftUp()) {
					//Open dialog for axis i.
					wxDialog* d = new edit_axis_properties(this, _jid, i);
					d->ShowModal();
					d->Destroy();
				} else
					selected_row = i;
			}
		}
	private:
		struct axis_state_info
		{
			bool offline;
			int64_t rawvalue;
			int16_t value;
			int64_t minus;
			int64_t zero;
			int64_t plus;
			int64_t neutral;
			double threshold;
			bool pressure;
			bool disabled;
		};
		struct axis_state_info axis_info(unsigned i)
		{
			struct axis_state_info ax;
			ax.offline = (_gp.online_axes().count(i) == 0);
			_gp.axis_status(i, ax.rawvalue, ax.value);
			_gp.get_calibration(i, ax.minus, ax.zero, ax.plus, ax.neutral, ax.threshold, ax.pressure,
				ax.disabled);
			return ax;
		}
		int button_info(unsigned i)
		{
			return _gp.button_status(i);
		}
		int hat_info(unsigned i)
		{
			return _gp.hat_status(i);
		}
		emulator_instance& inst;
		unsigned _jid;
		gamepad::pad& _gp;
		size_t base_width;
		size_t width_need;
		size_t height_need;
		size_t maxtitle;
		size_t cal_row;
		size_t selected_row;
		std::vector<std::pair<unsigned, unsigned>> axes_val;
		std::vector<std::pair<unsigned, unsigned>> buttons_val;
		std::vector<std::pair<unsigned, unsigned>> hats_val;
		void draw_axis(unsigned x, unsigned y, unsigned num, axis_state_info state)
		{
			CHECK_UI_THREAD;
			unsigned stride = get_characters().first;
			text_framebuffer::element* fb = get_buffer() + (y * stride + x);
			uint32_t fg, bg;
			fg = state.offline ? 0x808080 : 0x000000;
			bg = (num != selected_row) ? 0xFFFFFF : 0xFFFFC0;
			draw_bbox(fb, stride, fg, bg);
			fb[1 * stride + 6] = E(D(num, 100), fg, bg);
			fb[1 * stride + 7] = E(D(num, 10), fg, bg);
			fb[1 * stride + 8] = E(D(num, 1), fg, bg);
			if(state.offline) {
				write("Offline", 8, x + 1, y + 2, fg, bg);
				if(state.disabled)
					write("Disabled", 8, x + 1, y + 3, fg, bg);
			} else {
				std::string r1 = (stringfmt() << state.rawvalue).str();
				std::string r2 = (stringfmt() << state.value << "%").str();
				if(state.disabled)
					r2 = "Disabled";
				size_t r1l = (r1.length() < 8) ? r1.length() : 8;
				size_t r2l = (r2.length() < 8) ? r2.length() : 8;
				write(r1, r1l, x + 9 - r1l, y + 2, fg, bg);
				write(r2, r2l, x + 9 - r2l, y + 3, fg, bg);
			}
			if(num == selected_row) {
				uint32_t fg2 = 0;
				uint32_t bg2 = 0xFFFFFF;
				//This is selected, Also draw the calibration parameters.
				write((stringfmt() << "Calibration for axis" << num << ":").str(), 256, 0, cal_row,
					fg2, bg2);
				if(state.pressure) {
					write((stringfmt() << "Released: " << state.zero).str(), 40, 0,
						cal_row + 1, fg2, bg2);
					write((stringfmt() << "Pressed: " << state.plus).str(), 40, 40,
						cal_row + 1, fg2, bg2);
					write((stringfmt() << "Analog threshold: " << state.neutral).str(), 40, 0,
						cal_row + 2, fg2, bg2);
					write((stringfmt() << "Digital threshold: " << state.threshold).str(), 40, 40,
						cal_row + 2, fg2, bg2);
					write("Pressure sensitive button", 40, 0, cal_row + 3, fg2, bg2);
				} else {
					write((stringfmt() << "Left/Up: " << state.minus).str(), 40, 0,
						cal_row + 1, fg2, bg2);
					write((stringfmt() << "Right/Down: " << state.plus).str(), 40, 40,
						cal_row + 1, fg2, bg2);
					write((stringfmt() << "Center: " << state.zero).str(), 40, 0,
						cal_row + 2, fg2, bg2);
					write((stringfmt() << "Analog threshold: " << state.neutral).str(), 40, 40,
						cal_row + 2, fg2, bg2);
					write((stringfmt() << "Digital threshold: " << state.threshold).str(), 40, 0,
						cal_row + 3, fg2, bg2);
					write("Axis", 40, 40, cal_row + 3, fg2, bg2);
				}

			}
		}
		void draw_button(unsigned x, unsigned y, unsigned num, int state)
		{
			unsigned stride = get_characters().first;
			text_framebuffer::element* fb = get_buffer() + (y * stride + x);
			uint32_t fg, bg;
			if(state < 0) {
				fg = 0x808080;
				bg = 0xFFFFFF;
			} else if(state > 0) {
				fg = 0xFFFFFF;
				bg = 0x000000;
			} else {
				fg = 0x000000;
				bg = 0xFFFFFF;
			}
			draw_box(fb, stride, fg, bg);
			if(num > 9) {
				fb[2 * stride + 1] = E(D(num, 100), fg, bg);
				fb[2 * stride + 2] = E(D(num, 10), fg, bg);
				fb[2 * stride + 3] = E(D(num, 1), fg, bg);
			} else
				fb[2 * stride + 2] = E(D(num, 1), fg, bg);
		}
		void draw_hat(unsigned x, unsigned y, unsigned num, int state)
		{
			unsigned stride = get_characters().first;
			text_framebuffer::element* fb = get_buffer() + (y * stride + x);
			uint32_t fg, bg;
			if(state < 0) {
				fg = 0x808080;
				bg = 0xFFFFFF;
			} else {
				fg = 0x000000;
				bg = 0xFFFFFF;
			}
			draw_box(fb, stride, fg, bg);
			unsigned sbits = 0020;
			if(state > 0)
				sbits = sbits_lookup[state & 15];
			if(num > 9) {
				fb[2 * stride + 1] = E(D(num, 100), fg, bg);
				fb[2 * stride + 2] = E(D(num, 10), fg, bg);
				fb[2 * stride + 3] = E(D(num, 1), fg, bg);
			} else
				fb[2 * stride + 2] = E(D(num, 1), fg, bg);
			for(unsigned y = 1; y < 4; y++)
				for(unsigned x = 1; x < 4; x++)
					if((sbits >> (y * 3 + x - 4)) & 1)
						std::swap(fb[y * stride + x].fg, fb[y * stride + x].bg);
		}
		void draw_box(text_framebuffer::element* fb, unsigned stride, uint32_t fg, uint32_t bg)
		{
			for(unsigned y = 0; y < 5; y++)
				for(unsigned x = 0; x < 5; x++)
					fb[y * stride + x] = E(box_symbols[5 * y + x], fg, bg);
		}
		void draw_bbox(text_framebuffer::element* fb, unsigned stride, uint32_t fg, uint32_t bg)
		{
			for(unsigned y = 0; y < 5; y++)
				for(unsigned x = 0; x < 10; x++)
					fb[y * stride + x] = E(bbox_symbols[10 * y + x], fg, bg);
		}
		char32_t D(unsigned val, unsigned div)
		{
			if(val < div && div != 1)
				return U' ';
			return U'0' + ((val / div) % 10);
		}
	};

	class joystick_config_window : public settings_tab
	{
	public:
		joystick_config_window(wxWindow* parent, emulator_instance& _inst)
			: settings_tab(parent, _inst)
		{
			CHECK_UI_THREAD;
			if(!lsnes_gamepads.gamepads())
				throw std::runtime_error("No joysticks available");
			wxSizer* top1_s = new wxBoxSizer(wxVERTICAL);
			SetSizer(top1_s);
			std::map<std::string, unsigned> jsnum;
			top1_s->Add(jsp = new wxChoicebook(this, -1), 1, wxGROW);
			for(unsigned i = 0; i < lsnes_gamepads.gamepads(); i++) {
				gamepad::pad& gp = lsnes_gamepads[i];
				std::string name = gp.name();
				jsnum[name] = jsnum.count(name) ? (jsnum[name] + 1) : 1;
				std::string tname = (stringfmt() << "joystick" << i << ": " << name).str();
				joystick_panel* tmp;
				jsp->AddPage(tmp = new joystick_panel(jsp, inst, i, gp), towxstring(tname));
				panels.insert(tmp);
			}
			top1_s->SetSizeHints(this);
			Fit();
			timer = new update_timer(this);
			timer->Start(100);
		}
		~joystick_config_window()
		{
			CHECK_UI_THREAD;
			if(timer) {
				timer->Stop();
				delete timer;
			}
		}
		void update_all()
		{
			CHECK_UI_THREAD;
			if(closing()) {
				timer->Stop();
				delete timer;
				timer = NULL;
				return;
			}
			for(auto i : panels) {
				i->request_paint();
			}
		}
	private:
		class update_timer : public wxTimer
		{
		public:
			update_timer(joystick_config_window* p)
			{
				w = p;
			}
			void Notify()
			{
				w->update_all();
			}
		private:
			joystick_config_window* w;
		};
		update_timer* timer;
		wxChoicebook* jsp;
		std::set<joystick_panel*> panels;
	};

	settings_tab_factory joysticks("Joysticks", [](wxWindow* parent, emulator_instance& _inst) -> settings_tab* {
		return new joystick_config_window(parent, _inst);
	});
}
