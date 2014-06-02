#include "core/instance.hpp"
#include "core/moviedata.hpp"
#include "library/keyboard.hpp"

#include "platform/wxwidgets/platform.hpp"

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

	std::map<emulator_instance*, mouse_keys*> mkeys;
}


void handle_wx_mouse(emulator_instance& inst, wxMouseEvent& e)
{
	if(!mkeys.count(&inst))
		return;
	auto s = mkeys[&inst];
	auto sfactors = calc_scale_factors(video_scale_factor, arcorrect_enabled,
		(our_rom.rtype) ? our_rom.rtype->get_PAR() : 1.0);
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
	if(mkeys.count(&inst))
		return;
	mkeys[&inst] = new mouse_keys(inst);
}

void deinitialize_wx_mouse(emulator_instance& inst)
{
	if(!mkeys.count(&inst))
		return;
	delete mkeys[&inst];
	mkeys.erase(&inst);
}
