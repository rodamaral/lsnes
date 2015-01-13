#include <wx/wx.h>
#include <wx/event.h>
#include <wx/control.h>
#include <wx/combobox.h>

#include "platform/wxwidgets/platform.hpp"

#include "core/instance.hpp"
#include "core/instance-map.hpp"
#include "core/moviedata.hpp"
#include "core/rom.hpp"
#include "library/keyboard.hpp"


std::pair<double, double> calc_scale_factors(double factor, bool ar, double par);

namespace
{
	class mouse_keys
	{
	public:
		mouse_keys(emulator_instance& inst)
			: mouse_cal({0}),
			mouse_x(*inst.keyboard, "mouse_x", "mouse", mouse_cal),
			mouse_y(*inst.keyboard, "mouse_y", "mouse", mouse_cal),
			mouse_l(*inst.keyboard, "mouse_left", "mouse"),
			mouse_m(*inst.keyboard, "mouse_center", "mouse"),
			mouse_r(*inst.keyboard, "mouse_right", "mouse"),
			mouse_i(*inst.keyboard, "mouse_inwindow", "mouse")
		{
		}
		keyboard::mouse_calibration mouse_cal;
		keyboard::key_mouse mouse_x;
		keyboard::key_mouse mouse_y;
		keyboard::key_key mouse_l;
		keyboard::key_key mouse_m;
		keyboard::key_key mouse_r;
		keyboard::key_key mouse_i;
	};

	instance_map<mouse_keys> mkeys;
}


void handle_wx_mouse(emulator_instance& inst, wxMouseEvent& e)
{
	CHECK_UI_THREAD;
	auto s = mkeys.lookup(inst);
	if(!s) return;
	auto sfactors = calc_scale_factors(video_scale_factor, arcorrect_enabled, inst.rom->get_PAR());
	inst.iqueue->queue(keypress_info(keyboard::modifier_set(), s->mouse_x, e.GetX() /
		sfactors.first));
	inst.iqueue->queue(keypress_info(keyboard::modifier_set(), s->mouse_y, e.GetY() /
		sfactors.second));
	if(e.Entering())
		inst.iqueue->queue(keypress_info(keyboard::modifier_set(), s->mouse_i, 1));
	if(e.Leaving())
		inst.iqueue->queue(keypress_info(keyboard::modifier_set(), s->mouse_i, 0));
	if(e.LeftDown())
		inst.iqueue->queue(keypress_info(keyboard::modifier_set(), s->mouse_l, 1));
	if(e.LeftUp())
		inst.iqueue->queue(keypress_info(keyboard::modifier_set(), s->mouse_l, 0));
	if(e.MiddleDown())
		inst.iqueue->queue(keypress_info(keyboard::modifier_set(), s->mouse_m, 1));
	if(e.MiddleUp())
		inst.iqueue->queue(keypress_info(keyboard::modifier_set(), s->mouse_m, 0));
	if(e.RightDown())
		inst.iqueue->queue(keypress_info(keyboard::modifier_set(), s->mouse_r, 1));
	if(e.RightUp())
		inst.iqueue->queue(keypress_info(keyboard::modifier_set(), s->mouse_r, 0));
}

void initialize_wx_mouse(emulator_instance& inst)
{
	if(mkeys.exists(inst))
		return;
	mkeys.create(inst);
}

void deinitialize_wx_mouse(emulator_instance& inst)
{
	mkeys.destroy(inst);
}
