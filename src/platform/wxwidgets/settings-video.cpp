#include "platform/wxwidgets/settings-common.hpp"
#include "platform/wxwidgets/window_mainwindow.hpp"

namespace 
{
	enum
	{
		wxID_SCALE_FACTOR = wxID_HIGHEST + 1,
		wxID_SCALE_ALGO = wxID_HIGHEST + 2,
		wxID_AR_CORRECT = wxID_HIGHEST + 3,
		wxID_HFLIP = wxID_HIGHEST + 4,
		wxID_VFLIP = wxID_HIGHEST + 5,
		wxID_ROTATE = wxID_HIGHEST + 6,
	};
	
	const char* scalealgo_choices[] = {"Fast Bilinear", "Bilinear", "Bicubic", "Experimential", "Point", "Area",
		"Bicubic-Linear", "Gauss", "Sinc", "Lanczos", "Spline"};

	std::string getalgo(int flags)
	{
		for(size_t i = 0; i < sizeof(scalealgo_choices) / sizeof(scalealgo_choices[0]); i++)
			if(flags & (1 << i))
				return scalealgo_choices[i];
		return "unknown";
	}

	class wxeditor_esettings_video : public settings_tab
	{
	public:
		wxeditor_esettings_video(wxWindow* parent);
		~wxeditor_esettings_video();
		void on_configure(wxCommandEvent& e);
		wxCheckBox* arcorrect;
		wxCheckBox* hflip;
		wxCheckBox* vflip;
		wxCheckBox* rotate;
		wxComboBox* scalealgo;
		wxSpinCtrl* scalefact;
		void on_close();
	private:
		void refresh();
		wxFlexGridSizer* top_s;
	};

	wxeditor_esettings_video::wxeditor_esettings_video(wxWindow* parent)
		: settings_tab(parent)
	{
		std::vector<wxString> scales;
		for(size_t i = 0; i < sizeof(scalealgo_choices)/sizeof(scalealgo_choices[0]); i++)
			scales.push_back(towxstring(scalealgo_choices[i]));

		wxButton* tmp;
		top_s = new wxFlexGridSizer(4, 2, 0, 0);
		SetSizer(top_s);
		top_s->Add(new wxStaticText(this, -1, wxT("Scale %: ")), 0, wxGROW);
		top_s->Add(scalefact = new wxSpinCtrl(this, wxID_SCALE_FACTOR, wxT(""), wxDefaultPosition,
			wxDefaultSize, wxSP_ARROW_KEYS, 25, 1000, 100 * video_scale_factor), 1, wxGROW);
		top_s->Add(new wxStaticText(this, wxID_SCALE_ALGO, wxT("Scaling type: ")), 0, wxGROW);
		top_s->Add(scalealgo = new wxComboBox(this, -1, towxstring(getalgo(scaling_flags)), wxDefaultPosition,
			wxDefaultSize, scales.size(), &scales[0], wxCB_READONLY), 1, wxGROW);
		top_s->Add(arcorrect = new wxCheckBox(this, wxID_AR_CORRECT, wxT("AR correction")), 1, wxGROW);
		top_s->Add(rotate = new wxCheckBox(this, wxID_ROTATE, wxT("Rotate")), 1, wxGROW);
		top_s->Add(hflip = new wxCheckBox(this, wxID_HFLIP, wxT("X flip")), 1, wxGROW);
		top_s->Add(vflip = new wxCheckBox(this, wxID_VFLIP, wxT("Y flip")), 1, wxGROW);

		scalefact->Connect(wxEVT_COMMAND_SPINCTRL_UPDATED,
			wxCommandEventHandler(wxeditor_esettings_video::on_configure), NULL, this);
		scalealgo->Connect(wxEVT_COMMAND_COMBOBOX_SELECTED,
			wxCommandEventHandler(wxeditor_esettings_video::on_configure), NULL, this);
		arcorrect->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED,
			wxCommandEventHandler(wxeditor_esettings_video::on_configure), NULL, this);
		hflip->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED,
			wxCommandEventHandler(wxeditor_esettings_video::on_configure), NULL, this);
		vflip->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED,
			wxCommandEventHandler(wxeditor_esettings_video::on_configure), NULL, this);
		rotate->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED,
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
		std::vector<std::string> sa_choices;
		std::string v;
		int newflags = 1;
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
			else if(e.GetId() == wxID_HFLIP)
				hflip_enabled = hflip->GetValue();
			else if(e.GetId() == wxID_ROTATE)
				rotate_enabled = rotate->GetValue();
			else if(e.GetId() == wxID_VFLIP)
				vflip_enabled = vflip->GetValue();
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
		scalefact->SetValue(video_scale_factor * 100.0 + 0.5);
		arcorrect->SetValue(arcorrect_enabled);
		hflip->SetValue(hflip_enabled);
		vflip->SetValue(vflip_enabled);
		rotate->SetValue(rotate_enabled);
		scalealgo->SetValue(towxstring(getalgo(scaling_flags)));
		top_s->Layout();
		Fit();
	}

	void wxeditor_esettings_video::on_close()
	{
	}

	settings_tab_factory _settings_tab("Video", [](wxWindow* parent) -> settings_tab* {
		return new wxeditor_esettings_video(parent);
	});
}
