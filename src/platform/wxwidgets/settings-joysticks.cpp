#include "platform/wxwidgets/settings-common.hpp"
#include "platform/wxwidgets/textrender.hpp"
#include "core/keymapper.hpp"
#include <wx/choicebk.h>
#include "library/minmax.hpp"

namespace
{
	class edit_axis_properties : public wxDialog
	{
	public:
		edit_axis_properties(wxWindow* parent, unsigned _jid, unsigned _num)
			: wxDialog(parent, -1, towxstring(get_title(_jid, _num))), jid(_jid), num(_num)
		{
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
			t_s->Add(low = new wxTextCtrl(this, -1, wxT(""), wxDefaultPosition, wxSize(100, -1)), 1, wxGROW);
			t_s->Add(new wxStaticText(this, -1, wxT("Center:")));
			t_s->Add(mid = new wxTextCtrl(this, -1, wxT(""), wxDefaultPosition, wxSize(100, -1)), 1, wxGROW);
			t_s->Add(new wxStaticText(this, -1, wxT("Plus:")));
			t_s->Add(hi = new wxTextCtrl(this, -1, wxT(""), wxDefaultPosition, wxSize(100, -1)), 1, wxGROW);
			t_s->Add(new wxStaticText(this, -1, wxT("Neutral:")));
			t_s->Add(null = new wxTextCtrl(this, -1, wxT(""), wxDefaultPosition, wxSize(100, -1)), 1, wxGROW);
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
			int64_t minus, zero, plus, neutral;
			double threshold;
			bool pressure, disabled;
			const char* bad_what = NULL;

			try {
				bad_what = "Bad low calibration value";
				minus = boost::lexical_cast<int64_t>(tostdstring(low->GetValue()));
				bad_what = "Bad middle calibration value";
				zero = boost::lexical_cast<int64_t>(tostdstring(mid->GetValue()));
				bad_what = "Bad high calibration value";
				plus = boost::lexical_cast<int32_t>(tostdstring(hi->GetValue()));
				bad_what = "Bad neutral zone width";
				neutral = boost::lexical_cast<int64_t>(tostdstring(null->GetValue()));
				bad_what = "Bad threshold (range is 0 - 1)";
				threshold = boost::lexical_cast<double>(tostdstring(thresh->GetValue()));
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
		joystick_panel(wxWindow* parent, unsigned jid, hw_gamepad& gp)
			: text_framebuffer_panel(parent, 60, 32, -1, NULL), _gp(gp), _jid(jid)
		{
			unsigned rows = 2 + gp.axes() + gp.buttons() + gp.hats();
			std::string jname = (stringfmt() << "joystick" << _jid << " [" << gp.name() << "]").str();
			std::string status = std::string("Status: ") + (gp.online() ? "Online" : "Offline");
			base_width = max(jname.length(), status.length());
			unsigned y = 2;
			selected_row = rows;
			
			maxtitle = 0;
			if(gp.axes() > 0)
				maxtitle = max(maxtitle, 5 + numwidth(gp.axes() - 1));
			if(gp.buttons() > 0)
				maxtitle = max(maxtitle, 7 + numwidth(gp.buttons() - 1));
			if(gp.hats() > 0)
				maxtitle = max(maxtitle, 4 + numwidth(gp.hats() - 1));

			size_t widest = base_width;
			for(unsigned i = 0; i < gp.axes(); i++)
				widest = max(widest, axis_info(i).length());
			for(unsigned i = 0; i < gp.buttons(); i++)
				widest = max(widest, button_info(i).length());
			for(unsigned i = 0; i < gp.hats(); i++)
				widest = max(widest, hat_info(i).length());
			width_need = max(widest + 15, (size_t)80);
			height_need = 2 + _gp.axes() + _gp.buttons() + _gp.hats();
			set_size(width_need, height_need);

			write((stringfmt() << "joystick" << _jid << " [" << gp.name() << "]").str(), 256, 0, 0,
				0, 0xFFFFFF);
			write(std::string("Status: ") + (gp.online() ? "Online" : "Offline"), 256, 0, 1, 0,
				0xFFFFFF);
			
			for(unsigned i = 0; i < gp.axes(); i++) {
				axes_val.push_back(y);
				y++;
			}

			for(unsigned i = 0; i < gp.buttons(); i++) {
				buttons_val.push_back(y);
				y++;
			}

			for(unsigned i = 0; i < gp.hats(); i++) {
				hats_val.push_back(y);
				y++;
			}
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
			for(unsigned i = 0; i < axes_val.size(); i++) {
				size_t y = axes_val[i];
				write(axis_info(i), width_need, 0, y, 0x600000, (y == selected_row) ? 0xFFFFC0 :
					0xFFFFFF);
			}
			for(unsigned i = 0; i < buttons_val.size(); i++) {
				write(button_info(i), width_need, 0, buttons_val[i], 0x006000, 0xFFFFFF);
			}
			for(unsigned i = 0; i < hats_val.size(); i++) {
				write(hat_info(i), width_need, 0, hats_val[i], 0x000060, 0xFFFFFF);
			}
		}
		std::pair<unsigned, unsigned> size_needed() { return std::make_pair(width_need, height_need); }
		void on_mouse(wxMouseEvent& e)
		{
			auto cell = get_cell();
			size_t y = e.GetY() / cell.second;
			for(size_t i = 0; i < axes_val.size(); i++)
				if(e.LeftUp()) {
					if(axes_val[i] == y) {
						//Open dialog for axis i.
						wxDialog* d = new edit_axis_properties(this, _jid, i);
						d->ShowModal();
						d->Destroy();
					}
				} else
					selected_row = y;
		}
	private:
		std::string axis_info(unsigned i)
		{
			std::ostringstream x;
			x << "axis" << i;
			for(unsigned j = 4 + numwidth(i); j < maxtitle; j++)
				x << " ";
			std::string astatus = _gp.axis_status(i);
			x << astatus;
			for(unsigned j = astatus.length(); j <= 16; j++)
				x << " ";
			x << "[" << get_calibration(i) << "]";
			return x.str();
		}
		std::string button_info(unsigned i)
		{
			std::ostringstream x;
			x << "button" << i;
			for(unsigned j = 6 + numwidth(i); j < maxtitle; j++)
				x << " ";
			std::string astatus = _gp.button_status(i);
			x << astatus;
			return x.str();
		}
		std::string hat_info(unsigned i)
		{
			std::ostringstream x;
			x << "hat" << i;
			for(unsigned j = 3 + numwidth(i); j < maxtitle; j++)
				x << " ";
			std::string astatus = _gp.hat_status(i);
			x << astatus;
			return x.str();
		}
		std::string get_calibration(unsigned num)
		{
			int64_t minus, zero, plus, neutral;
			double threshold;
			bool pressure, disabled;
			_gp.get_calibration(num, minus, zero, plus, neutral, threshold, pressure, disabled);
			return (stringfmt()  << minus << "<-" << zero << "(" << neutral << ")->" << plus 
				<< " T:" << threshold << " " << (pressure ? "P" : "A")
				<< (disabled ? "D" : "E")).str();
		}
		unsigned _jid;
		hw_gamepad& _gp;
		size_t base_width;
		size_t width_need;
		size_t height_need;
		size_t maxtitle;
		size_t selected_row;
		std::vector<unsigned> axes_val;
		std::vector<unsigned> buttons_val;
		std::vector<unsigned> hats_val;
	};

	class joystick_config_window : public settings_tab
	{
	public:
		joystick_config_window(wxWindow* parent)
			: settings_tab(parent)
		{
			wxSizer* top1_s = new wxBoxSizer(wxVERTICAL);
			SetSizer(top1_s);
			std::map<std::string, unsigned> jsnum;
			top1_s->Add(jsp = new wxChoicebook(this, -1), 1, wxGROW);
			for(unsigned i = 0; i < lsnes_gamepads.gamepads(); i++) {
				hw_gamepad& gp = lsnes_gamepads[i];
				std::string name = gp.name();
				jsnum[name] = jsnum.count(name) ? (jsnum[name] + 1) : 1;
				std::string tname = (stringfmt() << "joystick" << i << ": " << name).str();
				joystick_panel* tmp;
				jsp->AddPage(tmp = new joystick_panel(jsp, i, gp), towxstring(tname));
				panels.insert(tmp);
			}
			top1_s->SetSizeHints(this);
			Fit();
			timer = new update_timer(this);
			timer->Start(100);
		}
		~joystick_config_window()
		{
			if(timer) {
				timer->Stop();
				delete timer;
			}
		}
		void update_all()
		{
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

	settings_tab_factory joysticks("Joysticks", [](wxWindow* parent) -> settings_tab* {
		return new joystick_config_window(parent);
	});
}
