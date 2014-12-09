#include "platform/wxwidgets/settings-common.hpp"
#include "platform/wxwidgets/window_mainwindow.hpp"

namespace
{
	enum
	{
		wxID_SCALE_FACTOR = wxID_HIGHEST + 1,
		wxID_SCALE_ALGO = wxID_HIGHEST + 2,
		wxID_AR_CORRECT = wxID_HIGHEST + 3,
		wxID_ORIENT = wxID_HIGHEST + 4,
	};

	const char* scalealgo_choices[] = {"Fast Bilinear", "Bilinear", "Bicubic", "Experimential", "Point", "Area",
		"Bicubic-Linear", "Gauss", "Sinc", "Lanczos", "Spline"};
	const char* orientations[] = {"Normal", "Rotate 90° left", "Rotate 90° right", "Rotate 180°",
		"Flip horizontal", "Flip vertical", "Transpose", "Transpose other"};
	unsigned orientation_flags[] = {0, 7, 1, 6, 2, 4, 5, 3};
	unsigned inv_orientation_flags[] = {0, 2, 4, 7, 5, 6, 3, 1};

	std::string getalgo(int flags)
	{
		for(size_t i = 0; i < sizeof(scalealgo_choices) / sizeof(scalealgo_choices[0]); i++)
			if(flags & (1 << i))
				return scalealgo_choices[i];
		return "unknown";
	}

	unsigned get_orientation()
	{
		unsigned x = 0;
		x |= (rotate_enabled ? 1 : 0);
		x |= (hflip_enabled ? 2 : 0);
		x |= (vflip_enabled ? 4 : 0);
		return inv_orientation_flags[x];
	}

	class wxeditor_esettings_video : public settings_tab
	{
	public:
		wxeditor_esettings_video(wxWindow* parent, emulator_instance& _inst);
		~wxeditor_esettings_video();
		void on_configure(wxCommandEvent& e);
		wxCheckBox* arcorrect;
		wxComboBox* scalealgo;
		wxComboBox* orient;
		wxSpinCtrl* scalefact;
		void on_close();
	private:
		void refresh();
		wxFlexGridSizer* top_s;
	};

	wxeditor_esettings_video::wxeditor_esettings_video(wxWindow* parent, emulator_instance& _inst)
		: settings_tab(parent, _inst)
	{
		CHECK_UI_THREAD;
		std::vector<wxString> scales;
		std::vector<wxString> orients;
		for(size_t i = 0; i < sizeof(scalealgo_choices)/sizeof(scalealgo_choices[0]); i++)
			scales.push_back(towxstring(scalealgo_choices[i]));
		for(size_t i = 0; i < sizeof(orientations)/sizeof(orientations[0]); i++)
			orients.push_back(towxstring(orientations[i]));

		top_s = new wxFlexGridSizer(4, 2, 0, 0);
		SetSizer(top_s);
		top_s->Add(new wxStaticText(this, -1, wxT("Scale %: ")), 0, wxGROW);
		top_s->Add(scalefact = new wxSpinCtrl(this, wxID_SCALE_FACTOR, wxT(""), wxDefaultPosition,
			wxDefaultSize, wxSP_ARROW_KEYS, 25, 1000, 100 * video_scale_factor), 1, wxGROW);
		top_s->Add(new wxStaticText(this, -1, wxT("Scaling type: ")), 0, wxGROW);
		top_s->Add(scalealgo = new wxComboBox(this, wxID_SCALE_ALGO, towxstring(getalgo(scaling_flags)),
			wxDefaultPosition, wxDefaultSize, scales.size(), &scales[0], wxCB_READONLY), 1, wxGROW);
		top_s->Add(new wxStaticText(this, -1, wxT("Orientation: ")), 0, wxGROW);
		top_s->Add(orient = new wxComboBox(this, wxID_ORIENT, towxstring(orientations[get_orientation()]),
			wxDefaultPosition, wxDefaultSize, orients.size(), &orients[0], wxCB_READONLY), 1, wxGROW);

		top_s->Add(arcorrect = new wxCheckBox(this, wxID_AR_CORRECT, wxT("AR correction")), 1, wxGROW);

		scalefact->Connect(wxEVT_COMMAND_SPINCTRL_UPDATED,
			wxCommandEventHandler(wxeditor_esettings_video::on_configure), NULL, this);
		scalealgo->Connect(wxEVT_COMMAND_COMBOBOX_SELECTED,
			wxCommandEventHandler(wxeditor_esettings_video::on_configure), NULL, this);
		arcorrect->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED,
			wxCommandEventHandler(wxeditor_esettings_video::on_configure), NULL, this);
		orient->Connect(wxEVT_COMMAND_COMBOBOX_SELECTED,
			wxCommandEventHandler(wxeditor_esettings_video::on_configure), NULL, this);

		refresh();
		top_s->SetSizeHints(this);
		Fit();
	}

	wxeditor_esettings_video::~wxeditor_esettings_video()
	{
	}

	void wxeditor_esettings_video::on_configure(wxCommandEvent& e)
	{
		CHECK_UI_THREAD;
		std::vector<std::string> sa_choices;
		std::string v;
		for(size_t i = 0; i < sizeof(scalealgo_choices) / sizeof(scalealgo_choices[0]); i++)
			sa_choices.push_back(scalealgo_choices[i]);
		std::string name;
		if(e.GetId() <= wxID_HIGHEST || e.GetId() > wxID_HIGHEST + 10)
			return;
		try {
			if(e.GetId() == wxID_SCALE_FACTOR) {
				video_scale_factor = scalefact->GetValue() / 100.0;
			} else if(e.GetId() == wxID_SCALE_ALGO) {
				if(scalealgo->GetSelection() != wxNOT_FOUND)
					scaling_flags = 1 << scalealgo->GetSelection();
			} else if(e.GetId() == wxID_AR_CORRECT)
				arcorrect_enabled = arcorrect->GetValue();
			else if(e.GetId() == wxID_ORIENT) {
				unsigned f = orientation_flags[orient->GetSelection()];
				rotate_enabled = f & 1;
				hflip_enabled = f & 2;
				vflip_enabled = f & 4;
			}
			if(main_window)
				main_window->notify_update();
		} catch(std::exception& e) {
			wxMessageBox(towxstring(std::string("Invalid value: ") + e.what()), wxT("Can't change value"),
				wxICON_EXCLAMATION | wxOK);
		}
		refresh();
	}

	void wxeditor_esettings_video::refresh()
	{
		CHECK_UI_THREAD;
		scalefact->SetValue(video_scale_factor * 100.0 + 0.5);
		arcorrect->SetValue(arcorrect_enabled);
		orient->SetSelection(get_orientation());
		scalealgo->SetValue(towxstring(getalgo(scaling_flags)));
		top_s->Layout();
		Fit();
	}

	void wxeditor_esettings_video::on_close()
	{
	}

	settings_tab_factory _settings_tab("Video", [](wxWindow* parent, emulator_instance& _inst) -> settings_tab* {
		return new wxeditor_esettings_video(parent, _inst);
	});
}
