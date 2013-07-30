#include "platform/wxwidgets/settings-common.hpp"

namespace 
{
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
		wxCheckBox* hflip;
		wxCheckBox* vflip;
		wxCheckBox* rotate;
		void on_close();
	private:
		void refresh();
		wxFlexGridSizer* top_s;
		wxStaticText* xscale;
		wxStaticText* yscale;
		wxStaticText* algo;
	};

	wxeditor_esettings_video::wxeditor_esettings_video(wxWindow* parent)
		: settings_tab(parent)
	{
		wxButton* tmp;
		top_s = new wxFlexGridSizer(8, 3, 0, 0);
		SetSizer(top_s);
		top_s->Add(new wxStaticText(this, -1, wxT("X scale factor: ")), 0, wxGROW);
		top_s->Add(xscale = new wxStaticText(this, -1, wxT("")), 1, wxGROW);
		top_s->Add(tmp = new wxButton(this, wxID_HIGHEST + 6, wxT("Change...")), 0, wxGROW);
		tmp->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxeditor_esettings_video::on_configure),
		NULL, this);
		top_s->Add(new wxStaticText(this, -1, wxT("Y scale factor: ")), 0, wxGROW);
		top_s->Add(yscale = new wxStaticText(this, -1, wxT("")), 1, wxGROW);
		top_s->Add(tmp = new wxButton(this, wxID_HIGHEST + 7, wxT("Change...")), 0, wxGROW);
		tmp->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxeditor_esettings_video::on_configure), NULL, this);
		top_s->Add(new wxStaticText(this, -1, wxT("Scaling type: ")), 0, wxGROW);
		top_s->Add(algo = new wxStaticText(this, -1, wxT("")), 1, wxGROW);
		top_s->Add(tmp = new wxButton(this, wxID_HIGHEST + 8, wxT("Change...")), 0, wxGROW);
		tmp->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
			wxCommandEventHandler(wxeditor_esettings_video::on_configure), NULL, this);
		top_s->Add(new wxStaticText(this, -1, wxT("Hflip: ")), 0, wxGROW);
		top_s->Add(hflip = new wxCheckBox(this, -1, wxT("")), 1, wxGROW);
		top_s->Add(new wxStaticText(this, -1, wxT("")), 0, wxGROW);
		top_s->Add(new wxStaticText(this, -1, wxT("Vflip: ")), 0, wxGROW);
		top_s->Add(vflip = new wxCheckBox(this, -1, wxT("")), 1, wxGROW);
		top_s->Add(new wxStaticText(this, -1, wxT("")), 0, wxGROW);
		top_s->Add(new wxStaticText(this, -1, wxT("Rotate: ")), 0, wxGROW);
		top_s->Add(rotate = new wxCheckBox(this, -1, wxT("")), 1, wxGROW);
		top_s->Add(new wxStaticText(this, -1, wxT("")), 0, wxGROW);

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
		std::string val;
		try {
			if(e.GetId() == wxID_HIGHEST + 6) {
				val = (stringfmt() << horizontal_scale_factor).str();
				val = pick_text(this, "Set X scaling factor", "Enter new horizontal scale factor "
				"(0.25-10):",
					val);
			} else if(e.GetId() == wxID_HIGHEST + 7) {
				val = (stringfmt() << horizontal_scale_factor).str();
				val = pick_text(this, "Set Y scaling factor", "Enter new vertical scale factor "
					"(0.25-10):", val);
			} else if(e.GetId() == wxID_HIGHEST + 8) {
				val = pick_among(this, "Select algorithm", "Select scaling algorithm", sa_choices);
			}
		} catch(...) {
			refresh();
			return;
		}
		std::string err;
		try {
			if(e.GetId() == wxID_HIGHEST + 6) {
				double x = parse_value<double>(val);
				if(x < 0.25 || x > 10)
					throw "Bad horizontal scaling factor (0.25-10)";
				horizontal_scale_factor = x;
			} else if(e.GetId() == wxID_HIGHEST + 7) {
				double x = parse_value<double>(val);
				if(x < 0.25 || x > 10)
					throw "Bad vertical scaling factor (0.25-10)";
				vertical_scale_factor = x;
			} else if(e.GetId() == wxID_HIGHEST + 8) {
				for(size_t i = 0; i < sizeof(scalealgo_choices) / sizeof(scalealgo_choices[0]); i++)
					if(val == scalealgo_choices[i])
						newflags = 1 << i;
				scaling_flags = newflags;
			}
		} catch(std::exception& e) {
			wxMessageBox(towxstring(std::string("Invalid value: ") + e.what()), wxT("Can't change value"),
				wxICON_EXCLAMATION | wxOK);
		}
		refresh();
	}

	void wxeditor_esettings_video::refresh()
	{
		xscale->SetLabel(towxstring((stringfmt() << horizontal_scale_factor).str()));
		yscale->SetLabel(towxstring((stringfmt() << vertical_scale_factor).str()));
		algo->SetLabel(towxstring(getalgo(scaling_flags)));
		hflip->SetValue(hflip_enabled);
		vflip->SetValue(vflip_enabled);
		rotate->SetValue(rotate_enabled);
		top_s->Layout();
		Fit();
	}

	void wxeditor_esettings_video::on_close()
	{
		hflip_enabled = hflip->GetValue();
		vflip_enabled = vflip->GetValue();
		rotate_enabled = rotate->GetValue();
	}

	settings_tab_factory _settings_tab("Video", [](wxWindow* parent) -> settings_tab* {
		return new wxeditor_esettings_video(parent);
	});
}
