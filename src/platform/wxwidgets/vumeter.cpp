#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>

#include "core/audioapi.hpp"
#include "core/audioapi-driver.hpp"
#include "core/dispatch.hpp"
#include "core/instance.hpp"
#include "core/window.hpp"

#include "library/string.hpp"
#include "platform/wxwidgets/platform.hpp"

namespace
{
	std::set<emulator_instance*> vumeter_open;

	unsigned vu_to_pixels(float vu)
	{
		if(vu < -100)
			vu = -100;
		if(vu > 20)
			vu = 20;
		unsigned _vu = 2 * (vu + 100);
		if(_vu > 2000)
			_vu = 0;	//Overflow.
		return _vu;
	}

	int to_db(float value)
	{
		if(value < 1e-10)
			return -100;
		int v = 20 / log(10) * log(value);
		if(v < -100)
			v = 100;
		if(v > 50)
			v = 50;
		return v;
	}

	void connect_events(wxSlider* s, wxObjectEventFunction fun, wxEvtHandler* obj)
	{
		CHECK_UI_THREAD;
		s->Connect(wxEVT_SCROLL_THUMBTRACK, fun, NULL, obj);
		s->Connect(wxEVT_SCROLL_PAGEDOWN, fun, NULL, obj);
		s->Connect(wxEVT_SCROLL_PAGEUP, fun, NULL, obj);
		s->Connect(wxEVT_SCROLL_LINEDOWN, fun, NULL, obj);
		s->Connect(wxEVT_SCROLL_LINEUP, fun, NULL, obj);
		s->Connect(wxEVT_SCROLL_TOP, fun, NULL, obj);
		s->Connect(wxEVT_SCROLL_BOTTOM, fun, NULL, obj);
	}

	uint32_t game_text_buf[16] = {
		0x00000000, 0x00000000, 0x3c000000, 0x66000000,
		0xc2000000, 0xc078ec7c, 0xc00cfec6, 0xde7cd6fe,
		0xc6ccd6c0, 0xc6ccd6c0, 0x66ccd6c6, 0x3a76c67c,
		0x00000000, 0x00000000, 0x00000000, 0x00000000
	};

	uint32_t vout_text_buf[16] = {
		0x00000000, 0x00000000, 0xc6000010, 0xc6000030,
		0xc6000030, 0xc67cccfc, 0xc6c6cc30, 0xc6c6cc30,
		0xc6c6cc30, 0x6cc6cc30, 0x38c6cc36, 0x107c761c,
		0x00000000, 0x00000000, 0x00000000, 0x00000000
	};

	uint32_t vin_text_buf[16] = {
		0x00000000, 0x00000000, 0xc6180000, 0xc6180000,
		0xc6000000, 0xc638dc00, 0xc6186600, 0xc6186600,
		0xc6186600, 0xc6186600, 0x38186600, 0x103c6600,
		0x00000000, 0x00000000, 0x00000000, 0x00000000
	};

	unsigned get_strip_color(unsigned i)
	{
		if(i < 80)
			return 0;
		if(i < 120)
			return 51 * (i - 80) / 4;
		if(i < 160)
			return 510;
		if(i < 200)
			return 510 - 51 * (i - 160) / 4;
		return 0;
	}
}

class wxwin_vumeter : public wxDialog
{
public:
	wxwin_vumeter(wxWindow* parent, emulator_instance& _inst);
	~wxwin_vumeter() throw();
	bool ShouldPreventAppExit() const;
	void on_close(wxCommandEvent& e);
	void on_wclose(wxCloseEvent& e);
	void refresh();
	void on_game_change(wxScrollEvent& e);
	void on_vout_change(wxScrollEvent& e);
	void on_vin_change(wxScrollEvent& e);
	void on_game_reset(wxCommandEvent& e);
	void on_vout_reset(wxCommandEvent& e);
	void on_vin_reset(wxCommandEvent& e);
	void on_devsel(wxCommandEvent& e);
	void on_mute(wxCommandEvent& e);
private:
	struct _vupanel : public wxPanel
	{
		_vupanel(wxwin_vumeter* v, audioapi_instance& _audio)
			: wxPanel(v, wxID_ANY, wxDefaultPosition, wxSize(320, 64)), audio(_audio)
		{
			CHECK_UI_THREAD;
			obj = v;
			buffer.resize(61440);
			bufferstride = 960;
			for(unsigned i = 0; i < 320; i++) {
				unsigned color = get_strip_color(i);
				if(color < 256) {
					colorstrip[3 * i + 0] = 255;
					colorstrip[3 * i + 1] = color;
				} else {
					colorstrip[3 * i + 0] = 255 - (color - 255);
					colorstrip[3 * i + 1] = 255;
				}
				colorstrip[3 * i + 2] = 0;
			}
			this->Connect(wxEVT_PAINT, wxPaintEventHandler(_vupanel::on_paint), NULL, this);
		}

		void signal_repaint()
		{
			CHECK_UI_THREAD;
			mleft = vu_to_pixels(audio.vu_mleft);
			mright = vu_to_pixels(audio.vu_mright);
			vout = vu_to_pixels(audio.vu_vout);
			vin = vu_to_pixels(audio.vu_vin);
			Refresh();
			obj->update_sent = false;
		}

		void on_paint(wxPaintEvent& e)
		{
			CHECK_UI_THREAD;
			wxPaintDC dc(this);
			dc.Clear();
			memset(&buffer[0], 255, buffer.size());
			draw_text(0, 8, 32, 16, game_text_buf);
			draw_text(0, 24, 32, 16, vout_text_buf);
			draw_text(0, 40, 32, 16, vin_text_buf);
			for(unsigned i = 32; i <= 272; i += 40)
				draw_vline(i, 0, 63);
			draw_vline(231, 0, 63);		//0dB is thicker.
			draw_meter(32, 8, 8, mleft);
			draw_meter(32, 16, 8, mright);
			draw_meter(32, 24, 16, vout);
			draw_meter(32, 40, 16, vin);
			wxBitmap bmp2(wxImage(320, 64, &buffer[0], true));
			dc.DrawBitmap(bmp2, 0, 0, false);
		}

		wxwin_vumeter* obj;
		volatile unsigned mleft;
		volatile unsigned mright;
		volatile unsigned vout;
		volatile unsigned vin;
		std::vector<unsigned char> buffer;
		unsigned char colorstrip[960];
		size_t bufferstride;
		audioapi_instance& audio;
		void draw_text(unsigned x, unsigned y, unsigned w, unsigned h, const uint32_t* buf)
		{
			unsigned spos = 0;
			unsigned pos = y * bufferstride + 3 * x;
			for(unsigned j = 0; j < h; j++) {
				for(unsigned i = 0; i < w; i++) {
					unsigned _spos = spos + i;
					unsigned char val = ((buf[_spos >> 5] >> (31 - (_spos & 31))) & 1) ? 0 : 255;
					buffer[pos + 3 * i + 0] = val;
					buffer[pos + 3 * i + 1] = val;
					buffer[pos + 3 * i + 2] = val;
				}
				pos += bufferstride;
				spos += 32 * ((w + 31) / 32);
			}
		}
		void draw_vline(unsigned x, unsigned y1, unsigned y2)
		{
			unsigned pos = y1 * bufferstride + 3 * x;
			for(unsigned j = y1; j < y2; j++) {
				buffer[pos + 0] = 0;
				buffer[pos + 1] = 0;
				buffer[pos + 2] = 0;
				pos += bufferstride;
			}
		}
		void draw_meter(unsigned x, unsigned y, unsigned h, unsigned val)
		{
			if(val > 320 - x)
				val = 320 - x;
			unsigned pos = y * bufferstride + 3 * x;
			for(unsigned j = 0; j < h; j++) {
				if(val)
					memcpy(&buffer[pos], colorstrip, 3 * val);
				pos += bufferstride;
			}
		}
	};
	emulator_instance& inst;
	volatile bool update_sent;
	bool closing;
	wxButton* closebutton;
	struct dispatch::target<> vulistener;
	_vupanel* vupanel;
	wxStaticText* rate;
	wxSlider* gamevol;
	wxSlider* voutvol;
	wxSlider* vinvol;
	wxButton* dgamevol;
	wxButton* dvoutvol;
	wxButton* dvinvol;
	wxComboBox* pdev;
	wxComboBox* rdev;
	wxCheckBox* mute;
	struct dispatch::target<bool> unmuted;
	struct dispatch::target<std::pair<std::string, std::string>> devchange;
};

wxwin_vumeter::wxwin_vumeter(wxWindow* parent, emulator_instance& _inst)
	: wxDialog(parent, wxID_ANY, wxT("lsnes: VU meter"), wxDefaultPosition, wxSize(-1, -1)), inst(_inst)
{
	CHECK_UI_THREAD;
	update_sent = false;
	closing = false;
	Centre();
	wxFlexGridSizer* top_s = new wxFlexGridSizer(5, 1, 0, 0);
	SetSizer(top_s);

	top_s->Add(vupanel = new _vupanel(this, *inst.audio));
	top_s->Add(rate = new wxStaticText(this, wxID_ANY, wxT("")), 0, wxGROW);

	wxFlexGridSizer* slier_s = new wxFlexGridSizer(3, 3, 0, 0);
	slier_s->Add(new wxStaticText(this, wxID_ANY, wxT("Game:")), 0, wxGROW);
	slier_s->Add(gamevol = new wxSlider(this, wxID_ANY, to_db(inst.audio->music_volume()), -100, 50,
		wxDefaultPosition, wxSize(320, -1)), 1, wxGROW);
	slier_s->Add(dgamevol = new wxButton(this, wxID_ANY, wxT("Reset")));
	slier_s->Add(new wxStaticText(this, wxID_ANY, wxT("Voice out:")), 1, wxGROW);
	slier_s->Add(voutvol = new wxSlider(this, wxID_ANY, to_db(inst.audio->voicep_volume()), -100, 50,
		wxDefaultPosition, wxSize(320, -1)), 1, wxGROW);
	slier_s->Add(dvoutvol = new wxButton(this, wxID_ANY, wxT("Reset")));
	slier_s->Add(new wxStaticText(this, wxID_ANY, wxT("Voice in:")), 1, wxGROW);
	slier_s->Add(vinvol = new wxSlider(this, wxID_ANY, to_db(inst.audio->voicer_volume()), -100, 50,
		wxDefaultPosition, wxSize(320, -1)), 1, wxGROW);
	slier_s->Add(dvinvol = new wxButton(this, wxID_ANY, wxT("Reset")));
	top_s->Add(slier_s, 1, wxGROW);

	gamevol->SetLineSize(1);
	vinvol->SetLineSize(1);
	voutvol->SetLineSize(1);
	gamevol->SetPageSize(10);
	vinvol->SetPageSize(10);
	voutvol->SetPageSize(10);
	connect_events(gamevol, wxScrollEventHandler(wxwin_vumeter::on_game_change), this);
	connect_events(voutvol, wxScrollEventHandler(wxwin_vumeter::on_vout_change), this);
	connect_events(vinvol, wxScrollEventHandler(wxwin_vumeter::on_vin_change), this);
	dgamevol->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxwin_vumeter::on_game_reset), NULL,
		this);
	dvoutvol->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxwin_vumeter::on_vout_reset), NULL,
		this);
	dvinvol->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(wxwin_vumeter::on_vin_reset), NULL,
		this);

	auto pdev_map = audioapi_driver_get_devices(false);
	auto rdev_map = audioapi_driver_get_devices(true);
	std::string current_pdev = pdev_map[audioapi_driver_get_device(false)];
	std::string current_rdev = rdev_map[audioapi_driver_get_device(true)];
	std::vector<wxString> available_pdevs;
	std::vector<wxString> available_rdevs;
	for(auto i : pdev_map)
		available_pdevs.push_back(towxstring(i.second));
	for(auto i : rdev_map)
		available_rdevs.push_back(towxstring(i.second));

	wxSizer* hw_s = new wxFlexGridSizer(3, 2, 0, 0);
	hw_s->Add(new wxStaticText(this, wxID_ANY, wxT("Input device:")), 0, wxGROW);
	hw_s->Add(rdev = new wxComboBox(this, wxID_ANY, towxstring(current_rdev), wxDefaultPosition,
		wxSize(-1, -1), available_rdevs.size(), &available_rdevs[0], wxCB_READONLY), 1, wxGROW);
	hw_s->Add(new wxStaticText(this, wxID_ANY, wxT("Output device:")), 0, wxGROW);
	hw_s->Add(pdev = new wxComboBox(this, wxID_ANY, towxstring(current_pdev), wxDefaultPosition,
		wxSize(-1, -1), available_pdevs.size(), &available_pdevs[0], wxCB_READONLY), 1, wxGROW);
	hw_s->Add(new wxStaticText(this, wxID_ANY, wxT("")), 0, wxGROW);
	hw_s->Add(mute = new wxCheckBox(this, wxID_ANY, wxT("Mute sounds")), 1, wxGROW);
	mute->SetValue(!platform::is_sound_enabled());
	top_s->Add(hw_s);

	wxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->AddStretchSpacer();
	pbutton_s->Add(closebutton = new wxButton(this, wxID_OK, wxT("Close")), 0, wxGROW);
	top_s->Add(pbutton_s, 1, wxGROW);
	pbutton_s->SetSizeHints(this);

	closebutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxwin_vumeter::on_close), NULL, this);
	rdev->Connect(wxEVT_COMMAND_COMBOBOX_SELECTED,
		wxCommandEventHandler(wxwin_vumeter::on_devsel), NULL, this);
	pdev->Connect(wxEVT_COMMAND_COMBOBOX_SELECTED,
		wxCommandEventHandler(wxwin_vumeter::on_devsel), NULL, this);
	mute->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED,
		wxCommandEventHandler(wxwin_vumeter::on_mute), NULL, this);
	Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(wxwin_vumeter::on_wclose));

	unmuted.set(inst.dispatch->sound_unmute, [this](bool unmute) {
		runuifun([this, unmute]() { if(!this->closing) this->mute->SetValue(!unmute); });
	});
	devchange.set(inst.dispatch->sound_change, [this](std::pair<std::string, std::string> d) {
		runuifun([this, d]() {
			if(this->closing) return;
			auto pdevs = audioapi_driver_get_devices(false);
			if(pdevs.count(d.second)) this->pdev->SetValue(towxstring(pdevs[d.second]));
			auto rdevs = audioapi_driver_get_devices(true);
			if(rdevs.count(d.first)) this->rdev->SetValue(towxstring(rdevs[d.first]));
		});
	});

	top_s->SetSizeHints(this);
	Fit();
	vulistener.set(inst.dispatch->vu_change, [this]() {
		if(!this->update_sent) {
			this->update_sent = true;
			runuifun([this]() -> void { this->refresh(); });
		}
	});
	refresh();
}

wxwin_vumeter::~wxwin_vumeter() throw()
{
}

void wxwin_vumeter::on_devsel(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	std::string newpdev = tostdstring(pdev->GetValue());
	std::string newrdev = tostdstring(rdev->GetValue());
	std::string _newpdev = "null";
	std::string _newrdev = "null";
	for(auto i : audioapi_driver_get_devices(false))
		if(i.second == newpdev)
			_newpdev = i.first;
	for(auto i : audioapi_driver_get_devices(true))
		if(i.second == newrdev)
			_newrdev = i.first;
	platform::set_sound_device(_newpdev, _newrdev);
}

void wxwin_vumeter::on_game_change(wxScrollEvent& e)
{
	CHECK_UI_THREAD;
	inst.audio->music_volume(pow(10, gamevol->GetValue() / 20.0));
}

void wxwin_vumeter::on_vout_change(wxScrollEvent& e)
{
	CHECK_UI_THREAD;
	inst.audio->voicep_volume(pow(10, voutvol->GetValue() / 20.0));
}

void wxwin_vumeter::on_vin_change(wxScrollEvent& e)
{
	CHECK_UI_THREAD;
	inst.audio->voicer_volume(pow(10, vinvol->GetValue() / 20.0));
}

void wxwin_vumeter::on_game_reset(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	inst.audio->music_volume(1);
	gamevol->SetValue(0);
}

void wxwin_vumeter::on_vout_reset(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	inst.audio->voicep_volume(1);
	voutvol->SetValue(0);
}

void wxwin_vumeter::on_vin_reset(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	inst.audio->voicer_volume(1);
	vinvol->SetValue(0);
}

void wxwin_vumeter::on_mute(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	platform::sound_enable(!mute->GetValue());
}

void wxwin_vumeter::refresh()
{
	CHECK_UI_THREAD;
	auto rate_cur = inst.audio->voice_rate();
	unsigned rate_nom = inst.audio->orig_voice_rate();
	rate->SetLabel(towxstring((stringfmt() << "Current: " << rate_cur.second << "Hz (nominal " << rate_nom
		<< "Hz), record: " << rate_cur.first << "Hz").str()));
	vupanel->signal_repaint();
}

void wxwin_vumeter::on_close(wxCommandEvent& e)
{
	CHECK_UI_THREAD;
	closing = true;
	Destroy();
	vumeter_open.erase(&inst);
}

void wxwin_vumeter::on_wclose(wxCloseEvent& e)
{
	CHECK_UI_THREAD;
	bool wasc = closing;
	closing = true;
	if(!wasc)
		Destroy();
	vumeter_open.erase(&inst);
}

bool wxwin_vumeter::ShouldPreventAppExit() const { return false; }

void open_vumeter_window(wxWindow* parent, emulator_instance& inst)
{
	CHECK_UI_THREAD;
	if(vumeter_open.count(&inst))
		return;
	wxwin_vumeter* v = new wxwin_vumeter(parent, inst);
	v->Show();
	vumeter_open.insert(&inst);
}
