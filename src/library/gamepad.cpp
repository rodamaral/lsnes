#include <functional>
#include "gamepad.hpp"
#include "string.hpp"

namespace gamepad
{
namespace
{
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

	int16_t map_value(int64_t val, int64_t minus, int64_t center, int64_t plus, int64_t neutral, bool pressure)
	{
		double m = minus;
		double v = val;
		double c = center;
		double p = plus;
		double n = neutral;
		if(pressure) {
			if(m > p) {
				v = m - v + p;
				std::swap(m, p);
			}
			if(v <= m + n)
				return 0;
			else if(v < p)
				return 32767 * (v - m - n) / (p - m - n);
			else
				return 32767;
		} else {
			if(m > p) {
				v = m - v + p;
				c = m - c + p;
				std::swap(m, p);
			}
			if(v < m)
				return -32768;
			else if(v < c - n)
				return -32768 * (c - n - v) / (c - n - m);
			else if(v <= c + n)
				return 0;
			else if(v < p)
				return 32767 * (v - c - n) / (p - c - n);
			else
				return 32767;
		}
	}
}

pad::pad(const JSON::node& state, unsigned _jnum)
{
	axis_fn = [](uint64_t a, uint64_t b, int16_t c) {};
	button_fn = [](uint64_t a, uint64_t b, bool c) {};
	hat_fn = [](uint64_t a, uint64_t b, unsigned c) {};
	newitem_fn = [](uint64_t a, uint64_t b, int c) {};
	jid = _jnum;
	online_flag = false;
	load(state);
}

pad::pad(const std::string& _xname, unsigned _jnum)
{
	axis_fn = [](uint64_t a, uint64_t b, int16_t c) {};
	button_fn = [](uint64_t a, uint64_t b, bool c) {};
	hat_fn = [](uint64_t a, uint64_t b, unsigned c) {};
	newitem_fn = [](uint64_t a, uint64_t b, int c) {};
	_name = _xname;
	jid = _jnum;
	next_axis = 0;
	next_button = 0;
	next_hat = 0;
	online_flag = false;
}

pad::~pad()
{
	//Set _axes_hat entries that alias to NULL.
	for(auto i = _axes_hat.begin(); i != _axes_hat.end(); i++)
		for(auto j = i; j != _axes_hat.end(); j++)
			if(i != j && i->second == j->second)
				j->second = NULL;
	for(auto i : _axes_hat) delete i.second;
}

void pad::set_online(bool status)
{
	std::list<unsigned> axes_off;
	std::list<unsigned> buttons_off;
	std::list<unsigned> hats_off;
	{
		threads::alock H(mlock);
		if(status)
			online_flag = status;
		else {
			online_flag = status;
			//Offline everything.
			for(auto& i : _axes) {
				if(i.second.online) try { axes_off.push_back(i.first); } catch(...) {}
				i.second.online = false;
			}
			for(auto& i : _buttons) {
				if(i.second.online) try { buttons_off.push_back(i.first); } catch(...) {}
				i.second.online = false;
			}
			for(auto& i : _hats) {
				if(i.second.online) try { hats_off.push_back(i.first); } catch(...) {}
				i.second.online = false;
			}
			for(auto& i : _axes_hat) {
				if(i.second->online) try { hats_off.push_back(i.first); } catch(...) {}
				i.second->online = false;
			}
		}
	}
	for(auto i : axes_off)
		axis_fn(jid, i, 0);
	for(auto i : buttons_off)
		button_fn(jid, i, 0);
	for(auto i : hats_off)
		hat_fn(jid, i, 0);
}

unsigned pad::add_axis(uint64_t id, int64_t _min, int64_t _max, bool pressure, const std::string& xname)
{
	axis_info a;
	{
		threads::alock H(mlock);
		if(_axes.count(id)) {
			_axes[id].name = xname;
			_axes[id].online = true;
			return _axes[id].num;
		}
		a.id = id;
		a.num = next_axis++;
		a.online = true;
		a.state = 0;
		a.rstate = 0;
		a.minus = _min;
		a.zero = pressure ? _min : (_min + _max + 1) / 2;
		a.plus = _max;
		a.neutral = 0;
		a.threshold = 0.5;
		a.name = xname;
		a.pressure = pressure;
		a.disabled = false;
		_axes[id] = a;
	}
	newitem_fn(jid, a.num, 0);
	return a.num;
}

unsigned pad::add_button(uint64_t id, const std::string& xname)
{
	button_info b;
	{
		threads::alock H(mlock);
		if(_buttons.count(id)) {
			_buttons[id].name = xname;
			_buttons[id].online = true;
			return _buttons[id].num;
		}
		b.id = id;
		b.num = next_button++;
		b.name = xname;
		b.online = true;
		b.state = false;
		_buttons[id] = b;
	}
	newitem_fn(jid, b.num, 1);
	return b.num;
}

unsigned pad::add_hat(uint64_t id, const std::string& xname)
{
	hat_info h;
	{
		threads::alock H(mlock);
		if(_hats.count(id)) {
			_hats[id].name = xname;
			_hats[id].online = true;
			return _hats[id].num;
		}
		h.id = id;
		h.id2 = 0;
		h.num = next_hat++;
		h.mindev = 1;
		h.name = xname;
		h.online = true;
		h.state = 0;
		_hats[id] = h;
	}
	newitem_fn(jid, h.num, 2);
	return h.num;
}

unsigned pad::add_hat(uint64_t idx, uint64_t idy, int64_t mindev, const std::string& xnamex,
	const std::string& xnamey)
{
	hat_info h;
	{
		threads::alock H(mlock);
		if(_axes_hat.count(idx)) {
			_axes_hat[idx]->name = xnamex;
			_axes_hat[idy]->name2 = xnamey;
			_axes_hat[idx]->online = true;
			return _axes_hat[idx]->num;
		}
		h.id = idx;
		h.id2 = idy;
		h.num = next_hat++;
		h.mindev = mindev;
		h.name = xnamex;
		h.name2 = xnamey;
		h.online = true;
		h.state = 0;
		_axes_hat[idy] = _axes_hat[idx] = new hat_info(h);
	}
	newitem_fn(jid, h.num, 2);
	return h.num;
}

void pad::report_axis(uint64_t id, int64_t val)
{
	mlock.lock();
	if(_axes.count(id)) {
		axis_info& i = _axes[id];
		int16_t val2 = map_value(val, i.minus, i.zero, i.plus, i.neutral, i.pressure);
		int16_t ostate = i.state;
		i.state = i.disabled ? 0 : val2;
		i.rstate = val;
		int16_t nstate = i.state;
		unsigned inum = i.num;
		mlock.unlock();
		if(ostate != nstate)
			axis_fn(jid, inum, nstate);
	} else if(_axes_hat.count(id)) {
		hat_info& i = *_axes_hat[id];
		bool is_x = (id == i.id);
		bool is_y = (id == i.id2);
		signed ostate = i.state;
		if(is_x) { i.state = (val <= -i.mindev) ? (i.state | 0x8) : (i.state & 0x7); }
		if(is_x) { i.state = (val >= i.mindev) ? (i.state | 0x2) : (i.state & 0xD); }
		if(is_y) { i.state = (val <= -i.mindev) ? (i.state | 0x1) : (i.state & 0xE); }
		if(is_y) { i.state = (val >= i.mindev) ? (i.state | 0x4) : (i.state & 0xB); }
		int16_t nstate = i.state;
		unsigned inum = i.num;
		mlock.unlock();
		if(ostate != nstate)
			hat_fn(jid, inum, nstate);
	} else
		mlock.unlock();
}

void pad::report_button(uint64_t id, bool val)
{
	mlock.lock();
	if(!_buttons.count(id)) {
		mlock.unlock();
		return;
	}
	button_info& i = _buttons[id];
	bool ostate = i.state;
	i.state = val;
	int16_t nstate = i.state;
	unsigned inum = i.num;
	mlock.unlock();
	if(ostate != nstate)
		button_fn(jid, inum, nstate);
}

void pad::report_hat(uint64_t id, int angle)
{
	mlock.lock();
	unsigned h = angle_to_bitmask(angle);
	if(!_hats.count(id)) {
		mlock.unlock();
		return;
	}
	hat_info& i = _hats[id];
	signed ostate = i.state;
	i.state = h;
	int16_t nstate = i.state;
	unsigned inum = i.num;
	mlock.unlock();
	if(ostate != nstate)
		hat_fn(jid, inum, nstate);
}

std::set<unsigned> pad::online_axes()
{
	threads::alock H(mlock);
	std::set<unsigned> r;
	for(auto i : _axes)
		if(i.second.online) r.insert(i.second.num);
	return r;
}

std::set<unsigned> pad::online_buttons()
{
	threads::alock H(mlock);
	std::set<unsigned> r;
	for(auto i : _buttons)
		if(i.second.online) r.insert(i.second.num);
	return r;
}

std::set<unsigned> pad::online_hats()
{
	threads::alock H(mlock);
	std::set<unsigned> r;
	for(auto i : _hats)
		if(i.second.online) r.insert(i.second.num);
	for(auto i : _axes_hat)
		if(i.second->online) r.insert(i.second->num);
	return r;
}

void pad::load(const JSON::node& state)
{
	std::list<std::pair<unsigned, int>> notify_queue;
	{
		threads::alock H(mlock);
		_name = state["name"].as_string8();
		const JSON::node& hat_data = state["hats"];

		if(state.field_exists("buttons"))  {
			const JSON::node& button_data = state["buttons"];
			next_button = button_data.index_count();
			for(size_t i = 0; i < button_data.index_count(); i++) {
				const JSON::node& bn = button_data.index(i);
				button_info b;
				b.id = bn["id"].as_uint();
				b.name = bn.field_exists("name") ? bn["name"].as_string8() : "";
				b.num = i;
				b.online = false;
				b.state = false;
				_buttons[b.id] = b;
				notify_queue.push_back(std::make_pair(b.num, 1));
			}
		} else
			next_button = 0;

		if(state.field_exists("axes"))  {
			const JSON::node& axis_data = state["axes"];
			next_axis = axis_data.index_count();
			for(size_t i = 0; i < axis_data.index_count(); i++) {
				const JSON::node& bn = axis_data.index(i);
				axis_info a;
				a.id = bn["id"].as_uint();
				a.name = bn.field_exists("name") ? bn["name"].as_string8() : "";
				a.num = i;
				a.minus = bn["minus"].as_int();
				a.zero = bn["zero"].as_int();
				a.plus = bn["plus"].as_int();
				a.neutral = bn["neutral"].as_int();
				a.pressure = bn["pressure"].as_bool();
				a.threshold = bn["threshold"].as_double();
				a.disabled = bn.field_exists("disabled") ? bn["disabled"].as_bool() : false;
				a.online = false;
				a.state = 0;
				a.rstate = 0;
				_axes[a.id] = a;
				notify_queue.push_back(std::make_pair(a.num, 0));
			}
		} else
			next_axis = 0;

		if(state.field_exists("hats"))  {
			next_hat = hat_data.index_count();
			for(size_t i = 0; i < hat_data.index_count(); i++) {
				const JSON::node& bn = hat_data.index(i);
				hat_info h;
				h.id = bn["id"].as_uint();
				h.name = bn.field_exists("name") ? bn["name"].as_string8() : "";
				h.name2 = bn.field_exists("name2") ? bn["name2"].as_string8() : "";
				h.id2 = bn.field_exists("id2") ? bn["id2"].as_uint() : 0;
				h.num = i;
				h.mindev = bn.field_exists("mindev") ? bn["mindev"].as_int() : 1;
				h.online = false;
				h.state = 0;
				if(bn.field_exists("id2")) {
					_axes_hat[h.id2] = _axes_hat[h.id] = new hat_info(h);
				} else {
					_hats[h.id] = h;
				}
				notify_queue.push_back(std::make_pair(h.num, 2));
			}
		} else
			next_hat = 0;
	}
	for(auto i : notify_queue) {
		try {
			newitem_fn(jid, i.first, i.second);
		} catch(...) {
		}
	}
}

void pad::calibrate_axis(unsigned num, int64_t minus, int64_t zero, int64_t plus, int64_t neutral,
	double threshold, bool pressure, bool disabled)
{
	mlock.lock();
	for(auto& i : _axes) {
		if(i.second.num != num)
			continue;
		axis_info& a = i.second;

		double oldtolerance = a.threshold;
		int oldmode = a.disabled ? -1 : a.pressure ? 0 : 1;
		a.minus = minus;
		a.zero = zero;
		a.plus = plus;
		a.neutral = neutral;
		a.pressure = pressure;
		a.threshold = threshold;
		a.disabled = disabled;
		int newmode = a.disabled ? -1 : a.pressure ? 0 : 1;
		unsigned num = a.num;
		double newtreshold = a.threshold;
		if(oldmode >= 0 && newmode < 0) {
			if(a.state != 0)
				a.state = 0;
		}
		mlock.unlock();
		if(oldmode >= 0 && newmode < 0)
			try { axis_fn(jid, num, 0); } catch(...) {}
		if(oldmode != newmode || oldtolerance != newtreshold)
			try { amode_fn(jid, num, newmode, newtreshold); } catch(...) {}
		return;
	}
	mlock.unlock();
	return;
}

void pad::get_calibration(unsigned num, int64_t& minus, int64_t& zero, int64_t& plus, int64_t& neutral,
	double& threshold, bool& pressure, bool& disabled)
{
	threads::alock H(mlock);
	for(auto& i : _axes) {
		if(i.second.num != num)
			continue;
		axis_info& a = i.second;
		minus = a.minus;
		zero = a.zero;
		plus = a.plus;
		neutral = a.neutral;
		pressure = a.pressure;
		threshold = a.threshold;
		disabled = a.disabled;
	}
}

double pad::get_tolerance(unsigned num)
{
	threads::alock H(mlock);
	for(auto& i : _axes) {
		if(i.second.num != num)
			continue;
		return i.second.threshold;
	}
	return 0.5;
}

int pad::get_mode(unsigned num)
{
	threads::alock H(mlock);
	for(auto& i : _axes) {
		if(i.second.num != num)
			continue;
		return i.second.disabled ? -1 : i.second.pressure ? 0 : 1;
	}
	return -1;
}

void pad::axis_status(unsigned num, int64_t& raw, int16_t& pct)
{
	threads::alock H(mlock);
	for(auto& i : _axes) {
		if(i.second.num != num || !i.second.online)
			continue;
		raw = i.second.rstate;
		if(i.second.disabled)
			pct = 0;
		else
			pct = (int)(i.second.state / 327.67);
		return;
	}
	raw = 0;
	pct = 0;;
}

int pad::button_status(unsigned num)
{
	threads::alock H(mlock);
	for(auto& i : _buttons) {
		if(i.second.num != num || !i.second.online)
			continue;
		return i.second.state ? 1 : 0;
	}
	return -1;
}

int pad::hat_status(unsigned num)
{
	threads::alock H(mlock);
	for(auto& i : _hats) {
		if(i.second.num != num || !i.second.online)
			continue;
		return i.second.state;
	}
	for(auto& i : _axes_hat) {
		if(i.second->num != num || !i.second->online)
			continue;
		return i.second->state;
	}
	return -1;
}

JSON::node pad::save()
{
	threads::alock H(mlock);
	JSON::node r(JSON::object);
	r.insert("name", JSON::string(_name));

	JSON::node& buttons_json = r.insert("buttons", JSON::array());
	for(size_t i = 0; i < next_button; i++) {
		for(auto j : _buttons) {
			if(j.second.num != i)
				continue;
			JSON::node& btndata = buttons_json.append(JSON::object());
			btndata.insert("id", JSON::number(j.second.id));
			btndata.insert("name", JSON::string(j.second.name));
		}
	}

	JSON::node& axes_json = r.insert("axes", JSON::array());
	for(size_t i = 0; i < next_axis; i++) {
		for(auto j : _axes) {
			if(j.second.num != i)
				continue;
			JSON::node& axdata = axes_json.append(JSON::object());
			axdata.insert("id", JSON::number(j.second.id));
			axdata.insert("name", JSON::string(j.second.name));
			axdata.insert("minus", JSON::number(j.second.minus));
			axdata.insert("zero", JSON::number(j.second.zero));
			axdata.insert("plus", JSON::number(j.second.plus));
			axdata.insert("neutral", JSON::number(j.second.neutral));
			axdata.insert("pressure", JSON::boolean(j.second.pressure));
			axdata.insert("threshold", JSON::number(j.second.threshold));
			axdata.insert("disabled", JSON::boolean(j.second.disabled));
		}
	}

	JSON::node& hats_json = r.insert("hats", JSON::array());
	for(size_t i = 0; i < next_hat; i++) {
		for(auto j : _hats) {
			if(j.second.num != i)
				continue;
			JSON::node& axdata = hats_json.append(JSON::object());
			axdata.insert("id", JSON::number(j.second.id));
			axdata.insert("name", JSON::string(j.second.name));
		}
		for(auto j : _axes_hat) {
			if(j.second->num != i)
				continue;
			JSON::node& axdata = hats_json.append(JSON::object());
			axdata.insert("id", JSON::number(j.second->id));
			axdata.insert("id2", JSON::number(j.second->id2));
			axdata.insert("name", JSON::string(j.second->name));
			axdata.insert("name2", JSON::string(j.second->name2));
			axdata.insert("mindev", JSON::number(j.second->mindev));
			break;
		}
	}

	return r;
}

std::string pad::get_summary()
{
	threads::alock h(mlock);
	std::ostringstream x;
	x << "joystick" << jid << ": " << _name << " " << (online_flag ? "" : " [Offline]") << std::endl;
	for(auto i : _axes) {
		x << "joystick" << jid << "axis" << i.second.num << "[" << i.second.id << ":" << i.second.name
			<< "]: " << i.second.minus << " <- " << i.second.zero << "(" << i.second.neutral << ") -> "
			<< i.second.plus << " " << "Threshold: " << i.second.threshold;
		if(i.second.pressure)
			x << " [Pressure]";
		if(i.second.disabled)
			x << " [Disabled]";
		if(!i.second.online)
			x << " [Offline]";
		x << std::endl;
	}
	for(auto i : _buttons) {
		x << "joystick" << jid << "button" << i.second.num << "[" << i.second.id << ":" << i.second.name
			<< "]: ";
		if(!i.second.online)
			x << " [Offline]";
		x << std::endl;
	}
	for(auto i : _hats) {
		x << "joystick" << jid << "hat" << i.second.num << "[" << i.second.id << ":" << i.second.name
			<< "]: ";
		if(!i.second.online)
			x << " [Offline]";
		x << std::endl;
	}
	for(auto i : _axes_hat) {
		if(i.first == i.second->id)
			continue;
		x << "joystick" << jid << "hat" << i.second->num << "[" << i.second->id << ":" << i.second->name
			<< "/" << i.second->id2 << ":" << i.second->name2 << "]: ";
		if(!i.second->online)
			x << " [Offline]";
		x << std::endl;
	}
	return x.str();
}

set::set()
{
}

set::~set()
{
	for(auto i : _gamepads)
		delete i;
}

void set::load(const JSON::node& state)
{
	bool locked = true;
	mlock.lock();
	for(auto i : _gamepads)
		delete i;
	_gamepads.clear();

	for(size_t i = 0; i < state.index_count(); i++) {
		const JSON::node& gpn = state.index(i);
		pad* gp = NULL;
		try {
			gp = NULL;
			gp = new pad("", _gamepads.size());
			gp->set_axis_cb(axis_fn);
			gp->set_button_cb(button_fn);
			gp->set_hat_cb(hat_fn);
			gp->set_axismode_cb(amode_fn);
			gp->set_newitem_cb(newitem_fn);
			_gamepads.push_back(gp);
			locked = false;
			mlock.unlock();
			gp->load(gpn);
			mlock.lock();
			locked = true;
		} catch(std::runtime_error& e) {
			std::cerr << "Can't load gamepad #" << i << " configuration: " << e.what() << std::endl;
			if(!locked)
				mlock.lock();
			delete gp;
		}
	}
	mlock.unlock();
}

JSON::node set::save()
{
	threads::alock h(mlock);
	JSON::node n(JSON::array);
	for(auto i : _gamepads)
		n.append(i->save());
	return n;
}

unsigned set::gamepads()
{
	threads::alock h(mlock);
	return _gamepads.size();
}

pad& set::operator[](unsigned gpnum)
{
	threads::alock h(mlock);
	if(gpnum >= _gamepads.size())
		throw std::runtime_error("Invalid gamepad index");
	return *_gamepads[gpnum];
}

unsigned set::add(const std::string& name)
{
	threads::alock h(mlock);
	for(size_t i = 0; i < _gamepads.size(); i++) {
		if(!_gamepads[i]->online() && _gamepads[i]->name() == name) {
			auto& gp = _gamepads[i];
			gp->set_online(true);
			//Reset the functions.
			gp->set_axis_cb(axis_fn);
			gp->set_button_cb(button_fn);
			gp->set_hat_cb(hat_fn);
			gp->set_axismode_cb(amode_fn);
			gp->set_newitem_cb(newitem_fn);
			return i;
		}
	}
	//No suitable found, create one.
	pad* gp = NULL;
	try {
		gp = new pad(name, _gamepads.size());
		gp->set_online(true);
		gp->set_axis_cb(axis_fn);
		gp->set_button_cb(button_fn);
		gp->set_hat_cb(hat_fn);
		gp->set_axismode_cb(amode_fn);
		gp->set_newitem_cb(newitem_fn);
		_gamepads.push_back(gp);
		return _gamepads.size() - 1;
	} catch(...) {
		delete gp;
		throw;
	}
}

void set::offline_all()
{
	for(size_t i = 0; i < _gamepads.size(); i++) {
		if(_gamepads[i]->online()) {
			_gamepads[i]->set_online(false);
		}
	}
}

void set::set_axis_cb(std::function<void(unsigned jnum, unsigned num, int16_t val)> fn)
{
	threads::alock h(mlock);
	axis_fn = fn;
	for(auto i : _gamepads)
		i->set_axis_cb(axis_fn);
}

void set::set_button_cb(std::function<void(unsigned jnum, unsigned num, bool val)> fn)
{
	threads::alock h(mlock);
	button_fn = fn;
	for(auto i : _gamepads)
		i->set_button_cb(button_fn);
}

void set::set_hat_cb(std::function<void(unsigned jnum, unsigned num, unsigned val)> fn)
{
	threads::alock h(mlock);
	hat_fn = fn;
	for(auto i : _gamepads)
		i->set_hat_cb(hat_fn);
}

void set::set_axismode_cb(std::function<void(unsigned jnum, unsigned num, int mode, double tolerance)> fn)
{
	threads::alock h(mlock);
	amode_fn = fn;
	for(auto i : _gamepads)
		i->set_axismode_cb(amode_fn);
}

void set::set_newitem_cb(std::function<void(unsigned jnum, unsigned num, int type)> fn)
{
	threads::alock h(mlock);
	newitem_fn = fn;
	for(auto i : _gamepads)
		i->set_newitem_cb(newitem_fn);
}

std::string set::get_summary()
{
	threads::alock h(mlock);
	std::ostringstream x;
	for(auto i : _gamepads)
		x << i->get_summary();
	return x.str();
}
}
