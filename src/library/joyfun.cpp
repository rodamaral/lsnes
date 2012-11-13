#include "joyfun.hpp"
#include "string.hpp"
#include <sstream>
#include <iostream>

short calibration_correction(int64_t v, int64_t low, int64_t high)
{
	double _v = v;
	double _low = low;
	double _high = high;
	double _pos = 65535 * (_v - _low) / (_high - _low) - 32768;
	if(_pos < -32768)
		return -32768;
	else if(_pos > 32767)
		return 32767;
	else
		return static_cast<short>(_pos);
}

short angle_to_bitmask(int pov)
{
	short m = 0;
	if((pov >= 0 && pov <= 6000) || (pov >= 30000  && pov <= 36000))
		m |= 1;
	if(pov >= 3000 && pov <= 15000)
		m |= 2;
	if(pov >= 12000 && pov <= 24000)
		m |= 4;
	if(pov >= 21000 && pov <= 33000)
		m |= 8;
	return m;
}

joystick_model::change_info::change_info()
{
	has_been_read = false;
	last_read = 0;
	last_known = 0;
}

void joystick_model::change_info::update(short newv)
{
	last_known = newv;
}

bool joystick_model::change_info::read(short& val)
{
	bool r = (last_known != last_read || !has_been_read);
	has_been_read = true;
	val = last_read = last_known;
	return r;
}

bool joystick_model::read_common(std::vector<change_info>& i, unsigned id, short& res)
{
	if(id >= i.size())
		return false;
	return i[id].read(res);
}

unsigned joystick_model::size_common(std::vector<change_info>& i)
{
	return i.size();
}

void joystick_model::report_button(uint64_t id, bool value)
{
	if(button_map.count(id))
		_buttons[button_map[id]].update(value ? 1 : 0);
}

void joystick_model::report_pov(uint64_t id, int angle)
{
	if(pov_map.count(id))
		_hats[pov_map[id]].update(angle_to_bitmask(angle));
}

void joystick_model::report_axis(uint64_t id, int64_t value)
{
	//The axis case.
	if(axis_map.count(id))
		_axes[axis_map[id]].update(calibration_correction(value, aranges[id].first, aranges[id].second));
	//It isn't known axis. See if it is connected with axis pair for hat.
	else if(hataxis_map.count(id)) {
		struct hat_axis_info& ai = hataxis_map[id];
		struct hat_info& hi = hatinfos[ai.hnum];
		short v = 0;
		if(value <= -ai.mindev)		v = -1;
		if(value >= ai.mindev)		v = 1;
		if(ai.yflag)			hi.ystate = v;
		else				hi.xstate = v;
		v = 0;
		if(hi.ystate < 0)		v |= 1;
		if(hi.xstate > 0)		v |= 2;
		if(hi.ystate > 0)		v |= 4;
		if(hi.xstate < 0)		v |= 8;
		_hats[ai.hnum].update(v);
	}
}

unsigned joystick_model::new_axis(uint64_t id, int64_t minv, int64_t maxv, const std::string& xname)
{
	unsigned n = axes();
	aranges[id] = std::make_pair(minv, maxv);
	_axes.resize(n + 1);
	axis_map[id] = n;
	axisnames[n] = xname;
	return n;
}

unsigned joystick_model::new_button(uint64_t id, const std::string& xname)
{
	unsigned n = buttons();
	_buttons.resize(n + 1);
	button_map[id] = n;
	buttonnames[n] = xname;
	return n;
}

unsigned joystick_model::new_hat(uint64_t id_x, uint64_t id_y, int64_t min_dev, const std::string& xname_x,
	const std::string& xname_y)
{
	unsigned n = hats();
	hatinfos.resize(n + 1);
	hatinfos[n].xstate = 0;
	hatinfos[n].ystate = 0;
	_hats.resize(n + 1);
	hataxis_map[id_x].yflag = false;
	hataxis_map[id_x].hnum = n;
	hataxis_map[id_x].mindev = min_dev;
	hataxis_map[id_y].yflag = true;
	hataxis_map[id_y].hnum = n;
	hataxis_map[id_y].mindev = min_dev;
	hatnames[n] = "<X: " + xname_x + " Y: " + xname_y + ">";
	return n;
}

unsigned joystick_model::new_hat(uint64_t id, const std::string& xname)
{
	unsigned n = hats();
	_hats.resize(n + 1);
	pov_map[id] = n;
	hatnames[n] = xname;
	return n;
}

std::pair<int64_t, int64_t> joystick_model::axiscalibration(unsigned id)
{
	for(auto i : axis_map)
		if(i.second == id)
			return aranges[i.first];
	return std::make_pair(0, 0);
}

std::string joystick_model::axisname(unsigned id)
{
	if(axisnames.count(id))		return axisnames[id];
	else				return "";
}

std::string joystick_model::buttonname(unsigned id)
{
	if(buttonnames.count(id))	return buttonnames[id];
	else				return "";
}

std::string joystick_model::hatname(unsigned id)
{
	if(hatnames.count(id))		return hatnames[id];
	else				return "";
}

void joystick_model::name(const std::string& newn)
{
	joyname = newn;
}

const std::string& joystick_model::name()
{
	return joyname;
}

std::string joystick_model::compose_report(unsigned jnum)
{
	std::ostringstream out;
	out << "Joystick #" << jnum << ": " << joyname << std::endl;
	for(size_t i = 0; i < _axes.size(); i++) {
		auto c = axiscalibration(i);
		out << "  Axis #" << i << ": " << axisnames[i] << "(" << c.first << " - " << c.second << ")"
			<< std::endl;
	}
	for(size_t i = 0; i < _buttons.size(); i++)
		out << "  Button #" << i << ": " << buttonnames[i] << std::endl;
	for(size_t i = 0; i < _hats.size(); i++)
		out << "  Hat #" << i << ": " << hatnames[i] << std::endl;
	return out.str();
}
