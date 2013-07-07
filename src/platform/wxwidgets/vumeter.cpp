#include "core/audioapi.hpp"
#include "core/dispatch.hpp"

#include "library/string.hpp"
#include "platform/wxwidgets/platform.hpp"

#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>

namespace
{
	bool vumeter_open = false;

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
	wxwin_vumeter(wxWindow* parent);
	~wxwin_vumeter() throw();
	bool ShouldPreventAppExit() const;
	void on_close(wxCommandEvent& e);
	void on_wclose(wxCloseEvent& e);
	void refresh();
	void on_game_change(wxScrollEvent& e);
	void on_vout_change(wxScrollEvent& e);
	void on_vin_change(wxScrollEvent& e);
private:
	struct _vupanel : public wxPanel
	{
		_vupanel(wxwin_vumeter* v)
			: wxPanel(v, wxID_ANY, wxDefaultPosition, wxSize(320, 64))
		{
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
			mleft = vu_to_pixels(audioapi_vu_mleft);
			mright = vu_to_pixels(audioapi_vu_mright);
			vout = vu_to_pixels(audioapi_vu_vout);
			vin = vu_to_pixels(audioapi_vu_vin);
			Refresh();
			obj->update_sent = false;
		}

		void on_paint(wxPaintEvent& e)
		{
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
	volatile bool update_sent;
	bool closing;
	wxButton* closebutton;
	struct dispatch_target<> vulistener;
	_vupanel* vupanel;
	wxStaticText* rate;
	wxSlider* gamevol;
	wxSlider* voutvol;
	wxSlider* vinvol;
};

wxwin_vumeter::wxwin_vumeter(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, wxT("lsnes: VU meter"), wxDefaultPosition, wxSize(-1, -1))
{
	update_sent = false;
	closing = false;
	Centre();
	wxFlexGridSizer* top_s = new wxFlexGridSizer(4, 1, 0, 0);
	SetSizer(top_s);

	top_s->Add(vupanel = new _vupanel(this));
	top_s->Add(rate = new wxStaticText(this, wxID_ANY, wxT("")), 0, wxGROW);

	wxFlexGridSizer* slier_s = new wxFlexGridSizer(3, 2, 0, 0);
	slier_s->Add(new wxStaticText(this, wxID_ANY, wxT("Game:")), 0, wxGROW);
	slier_s->Add(gamevol = new wxSlider(this, wxID_ANY, to_db(audioapi_music_volume()), -100, 50,
		wxDefaultPosition, wxSize(320, -1)), 1, wxGROW);
	slier_s->Add(new wxStaticText(this, wxID_ANY, wxT("Voice out:")), 1, wxGROW);
	slier_s->Add(voutvol = new wxSlider(this, wxID_ANY, to_db(audioapi_voicep_volume()), -100, 50,
		wxDefaultPosition, wxSize(320, -1)), 1, wxGROW);
	slier_s->Add(new wxStaticText(this, wxID_ANY, wxT("Voice in:")), 1, wxGROW);
	slier_s->Add(vinvol = new wxSlider(this, wxID_ANY, to_db(audioapi_voicer_volume()), -100, 50,
		wxDefaultPosition, wxSize(320, -1)), 1, wxGROW);
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

	wxSizer* pbutton_s = new wxBoxSizer(wxHORIZONTAL);
	pbutton_s->AddStretchSpacer();
	pbutton_s->Add(closebutton = new wxButton(this, wxID_OK, wxT("Close")), 0, wxGROW);
	top_s->Add(pbutton_s, 1, wxGROW);
	pbutton_s->SetSizeHints(this);

	closebutton->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
		wxCommandEventHandler(wxwin_vumeter::on_close), NULL, this);
	Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(wxwin_vumeter::on_wclose));

	top_s->SetSizeHints(this);
	Fit();
	vulistener.set(notify_vu_change, [this]() {
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

void wxwin_vumeter::on_game_change(wxScrollEvent& e)
{
	audioapi_music_volume(pow(10, gamevol->GetValue() / 20.0));
}

void wxwin_vumeter::on_vout_change(wxScrollEvent& e)
{
	audioapi_voicep_volume(pow(10, voutvol->GetValue() / 20.0));
}

void wxwin_vumeter::on_vin_change(wxScrollEvent& e)
{
	audioapi_voicer_volume(pow(10, vinvol->GetValue() / 20.0));
}

void wxwin_vumeter::refresh()
{
	auto rate_cur = audioapi_voice_rate();
	unsigned rate_nom = audioapi_orig_voice_rate();
	rate->SetLabel(towxstring((stringfmt() << "Current: " << rate_cur.second << "Hz (nominal " << rate_nom
		<< "Hz), record: " << rate_cur.first << "Hz").str()));
	vupanel->signal_repaint();
}

void wxwin_vumeter::on_close(wxCommandEvent& e)
{
	closing = true;
	Destroy();
	vumeter_open = false;
}

void wxwin_vumeter::on_wclose(wxCloseEvent& e)
{
	bool wasc = closing;
	closing = true;
	if(!wasc)
		Destroy();
	vumeter_open = false;
}

bool wxwin_vumeter::ShouldPreventAppExit() const { return false; }

void open_vumeter_window(wxWindow* parent)
{
	if(vumeter_open)
		return;
	wxwin_vumeter* v = new wxwin_vumeter(parent);
	v->Show();
	vumeter_open = true;
}
