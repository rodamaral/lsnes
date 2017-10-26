#include "cmdhelp/jukebox.hpp"
#include "core/jukebox.hpp"
#include "core/settings.hpp"
#include "library/settingvar.hpp"

#include <functional>
#include <stdexcept>

namespace
{
	settingvar::supervariable<settingvar::model_bool<settingvar::yes_no>> SET_jukebox_dflt_binary(lsnes_setgrp,
		"jukebox-default-binary", "Movie‣Saving‣Saveslots binary", true);
	settingvar::supervariable<settingvar::model_int<0,999999999>> SET_jukebox_size(lsnes_setgrp, "jukebox-size",
		"Movie‣Number of save slots", 12);
}

struct save_jukebox_listener : public settingvar::listener
{
	save_jukebox_listener(settingvar::group& _grp, save_jukebox& _jukebox)
		: grp(_grp), jukebox(_jukebox)
	{
		grp.add_listener(*this);
	}
	~save_jukebox_listener() throw() { grp.remove_listener(*this); };
	void on_setting_change(settingvar::group& _grp, const settingvar::base& val)
	{
		if(val.get_iname() == "jukebox-size")
			jukebox.set_size((size_t)SET_jukebox_size(_grp));
	}
private:
	settingvar::group& grp;
	save_jukebox& jukebox;
};

save_jukebox::save_jukebox(settingvar::group& _settings, command::group& _cmd)
	: settings(_settings), cmd(_cmd),
	slotsel(cmd, CJUKEBOX::sel, [this](const std::string& a) { this->do_slotsel(a); }),
	cycleprev(cmd, CJUKEBOX::prev, [this]() { this->cycle_prev(); }),
	cyclenext(cmd, CJUKEBOX::next, [this]() { this->cycle_next(); })
{
	listener = new save_jukebox_listener(_settings, *this);
	current_slot = 0;
}

save_jukebox::~save_jukebox()
{
	delete listener;
}

size_t save_jukebox::get_slot()
{
	if(!current_size)
		throw std::runtime_error("No save slots available");
	return current_slot;
}

void save_jukebox::set_slot(size_t slot)
{
	if(slot >= current_size)
		throw std::runtime_error("Selected slot out of range");
	current_slot = slot;
	if(update) update();
}

void save_jukebox::cycle_next()
{
	if(!current_size)
		throw std::runtime_error("No save slots available");
	current_slot++;
	if(current_slot >= current_size)
		current_slot = 0;
	if(update) update();
}

void save_jukebox::cycle_prev()
{
	if(!current_size)
		throw std::runtime_error("No save slots available");
	if(current_slot)
		current_slot--;
	else
		current_slot = current_size - 1;
	if(update) update();
}

bool save_jukebox::save_binary()
{
	return SET_jukebox_dflt_binary(settings);
}

std::string save_jukebox::get_slot_name()
{
	return (stringfmt() << "$SLOT:" << (get_slot() + 1)).str();
}

void save_jukebox::set_size(size_t size)
{
	current_size = size;
	if(current_slot >= current_size)
		current_slot = 0;
	if(update) update();
}

void save_jukebox::set_update(std::function<void()> _update)
{
	update = _update;
}

void save_jukebox::unset_update()
{
	update = std::function<void()>();
}

void save_jukebox::do_slotsel(const std::string& args)
{
	if(!regex_match("[1-9][0-9]{0,8}", args))
		throw std::runtime_error("Bad slot number");
	uint32_t slot = parse_value<uint32_t>(args);
	set_slot(slot - 1);
}
