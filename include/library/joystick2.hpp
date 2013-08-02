#ifndef _library__joystick__hpp__included__
#define _library__joystick__hpp__included__

#include <cstdint>
#include <set>
#include "json.hpp"
#include "threadtypes.hpp"

class hw_gamepad
{
public:
	hw_gamepad(const JSON::node& state, unsigned jnum);
	hw_gamepad(const std::string& _name, unsigned jnum);
	~hw_gamepad();
	unsigned add_axis(uint64_t id, int64_t min, int64_t max, bool pressure, const std::string& name);
	unsigned add_button(uint64_t id, const std::string& name);
	unsigned add_hat(uint64_t id, const std::string& name);
	unsigned add_hat(uint64_t idx, uint64_t idy, int64_t mindev, const std::string& namex,
		const std::string& namey);
	unsigned axes() { return next_axis; }
	unsigned buttons() { return next_button; }
	unsigned hats() { return next_hat; }
	void calibrate_axis(unsigned num, int64_t minus, int64_t zero, int64_t plus, int64_t neutral,
		double threshold, bool pressure, bool disabled);
	void get_calibration(unsigned num, int64_t& minus, int64_t& zero, int64_t& plus, int64_t& neutral,
		double& threshold, bool& pressure, bool& disabled);
	double get_tolerance(unsigned num);
	int get_mode(unsigned num);
	std::set<unsigned> online_axes();
	std::set<unsigned> online_buttons();
	std::set<unsigned> online_hats();
	std::string axis_status(unsigned num);
	std::string button_status(unsigned num);
	std::string hat_status(unsigned num);
	void report_axis(uint64_t id, int64_t val);
	void report_button(uint64_t id, bool val);
	void report_hat(uint64_t id, int angle);
	void set_online(bool status);
	bool online() { return online_flag; }
	const std::string& name() { return _name; }
	void set_axis_cb(std::function<void(unsigned jnum, unsigned num, int16_t val)> fn)
	{
		axis_fn = fn;
	}
	void set_button_cb(std::function<void(unsigned jnum, unsigned num, bool val)> fn) { button_fn = fn; }
	void set_hat_cb(std::function<void(unsigned jnum, unsigned num, unsigned val)> fn) { hat_fn = fn; }
	void set_axismode_cb(std::function<void(unsigned jnum, unsigned num, int mode, double tolerance)> fn)
	{
		amode_fn = fn;
	}
	void set_newitem_cb(std::function<void(unsigned jnum, unsigned num, int type)> fn) { newitem_fn = fn; }
	void load(const JSON::node& state);
	JSON::node save();
	std::string get_summary();
private:
	struct axis_info
	{
		uint64_t id;
		unsigned num;
		int64_t minus;
		int64_t zero;
		int64_t plus;
		int64_t neutral;
		double threshold;
		bool pressure;
		bool disabled;
		bool online;
		std::string name;
		int16_t state;
		int64_t rstate;
	};
	struct button_info
	{
		uint64_t id;
		unsigned num;
		std::string name;
		bool online;
		bool state;
	};
	struct hat_info
	{
		bool axis;
		uint64_t id;
		uint64_t id2;
		unsigned num;
		int64_t mindev;
		std::string name;
		std::string name2;
		unsigned state;
		bool online;
	};
	hw_gamepad(const hw_gamepad&);
	hw_gamepad& operator=(const hw_gamepad&);
	std::function<void(unsigned jnum, unsigned num, int16_t val)> axis_fn;
	std::function<void(unsigned jnum, unsigned num, bool val)> button_fn;
	std::function<void(unsigned jnum, unsigned num, unsigned val)> hat_fn;
	std::function<void(unsigned jnum, unsigned num, int mode, double tolerance)> amode_fn;
	std::function<void(unsigned jnum, unsigned num, int type)> newitem_fn;
	bool online_flag;
	std::string _name;
	std::map<uint64_t, axis_info> _axes;
	std::map<uint64_t, hat_info*> _axes_hat;
	std::map<uint64_t, button_info> _buttons;
	std::map<uint64_t, hat_info> _hats;
	unsigned next_axis;
	unsigned next_button;
	unsigned next_hat;
	unsigned jid;
	mutex_class mutex;
};

class hw_gamepad_set
{
public:
	hw_gamepad_set();
	~hw_gamepad_set();
	void load(const JSON::node& state);
	JSON::node save();
	unsigned gamepads();
	hw_gamepad& operator[](unsigned gpnum);
	unsigned add(const std::string& name);
	void set_axis_cb(std::function<void(unsigned jnum, unsigned num, int16_t val)> fn);
	void set_button_cb(std::function<void(unsigned jnum, unsigned num, bool val)> fn);
	void set_hat_cb(std::function<void(unsigned jnum, unsigned num, unsigned val)> fn);
	void set_axismode_cb(std::function<void(unsigned jnum, unsigned num, int mode, double tolerance)> fn);
	void set_newitem_cb(std::function<void(unsigned jnum, unsigned num, int type)> fn);
	std::string get_summary();
private:
	hw_gamepad_set(const hw_gamepad_set&);
	hw_gamepad_set& operator=(const hw_gamepad_set&);
	std::function<void(unsigned jnum, unsigned num, int16_t val)> axis_fn;
	std::function<void(unsigned jnum, unsigned num, bool val)> button_fn;
	std::function<void(unsigned jnum, unsigned num, unsigned val)> hat_fn;
	std::function<void(unsigned jnum, unsigned num, int mode, double tolerance)> amode_fn;
	std::function<void(unsigned jnum, unsigned num, int type)> newitem_fn;
	std::vector<hw_gamepad*> _gamepads;
	mutex_class mutex;
};

#endif
