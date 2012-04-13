#ifndef _library__joyfun__hpp__included__
#define _library__joyfun__hpp__included__

#include <cstdint>
#include <string>
#include <vector>
#include <map>

/**
 * Perform axis calibration correction.
 *
 * Parameter v: The raw value read.
 * Parameter low: The low limit.
 * Parameter high: The high limit.
 * Returns: The calibrated read value.
 */
short calibration_correction(int64_t v, int64_t low, int64_t high);

/**
 * Translate hundredths of degree position into hat bitmask.
 *
 * 0 is assumed to be up, and values are assumed to be clockwise. Negative values are centered.
 *
 * Parameter angle: The angle.
 * Returns: The hat bitmask.
 */
short angle_to_bitmask(int angle);

/**
 * If a != b, a <- b and return true. Otherwise return false.
 *
 * Parameter a: The target.
 * Parameter b: The source.
 * Returns: a was not equal to b?
 */
template<typename T> bool make_equal(T& a, const T& b)
{
	bool r = (a != b);
	if(r)
		a = b;
	return r;
}

/**
 * Joystick.
 */
class joystick_model
{
public:
/**
 * Set name of joystick field.
 */
	void name(const std::string& newn);
/**
 * Get name of joystick field.
 */
	const std::string& name();
/**
 * Create a new axis.
 *
 * Parameter id: The id of the axis.
 * Parameter minv: The minimal calibration value.
 * Parameter maxv: The maximal calibration value.
 * Parameter xname: The name of axis.
 * Returns: The number of axis.
 */
	unsigned new_axis(uint64_t id, int64_t minv, int64_t maxv, const std::string& xname);
/**
 * Create a new button.
 *
 * Parameter id: The id of button.
 * Parameter xname: The name of button.
 * Returns: The number of button.
 */
	unsigned new_button(uint64_t id, const std::string& xname);
/**
 * Create a new hat from pair of axes.
 *
 * Parameter id_x: The id of x axis of the hat.
 * Parameter id_y: The id of y axis of the hat.
 * Parameter min_dev: The smallest deviation from zero to react to.
 * Parameter xname_x: The name of x axis.
 * Parameter xname_y: The name of y axis.
 * Returns: The number of hat.
 */
	unsigned new_hat(uint64_t id_x, uint64_t id_y, int64_t min_dev, const std::string& xname_x,
		const std::string& xname_y);
/**
 * Create a new hat from POV control.
 *
 * Parameter id: The id of POV control.
 * Parameter xname: The name of POV control.
 * Returns: The number of hat.
 */
	unsigned new_hat(uint64_t id, const std::string& xname);
/**
 * Get the number of axes.
 */
	unsigned axes() { return size_common(_axes); }
/**
 * Get the number of buttons.
 */
	unsigned buttons() { return size_common(_buttons); }
/**
 * Get the number of hats.
 */
	unsigned hats() { return size_common(_hats); }
/**
 * Report on specified axis.
 *
 * Parameter id: The number of axis to report.
 * Parameter res: Place to write the calibrated axis value to.
 * Returns: True if value has changed since last call, false otherwise.
 */
	bool axis(unsigned id, short& res) { return read_common(_axes, id, res); }
/**
 * Report on specified button.
 *
 * Parameter id: The number of button to report.
 * Parameter res: Place to write the value to.
 * Returns: True if value has changed since last call, false otherwise.
 */
	bool button(unsigned id, short& res) { return read_common(_buttons, id, res); }
/**
 * Report on specified hat.
 *
 * Parameter id: The number of hat to report.
 * Parameter res: Place to write the value to.
 * Returns: True if value has changed since last call, false otherwise.
 */
	bool hat(unsigned id, short& res) { return read_common(_hats, id, res); }
/**
 * Return name of axis.
 *
 * Parameter id: The axis number.
 * Returns: The name of the axis, or "" if not found.
 */
	std::string axisname(unsigned id);
/**
 * Return axis calibration data.
 *
 * Parameter id: The axis number.
 * Returns: The axis calibration (first minimum, then maximum).
 */
	std::pair<int64_t, int64_t> axiscalibration(unsigned id);
/**
 * Return name of button.
 *
 * Parameter id: The button number.
 * Returns: The name of the button, or "" if not found.
 */
	std::string buttonname(unsigned id);
/**
 * Return name of hat.
 *
 * Parameter id: The hat number.
 * Returns: The name of the hat, or "" if not found.
 */
	std::string hatname(unsigned id);
/**
 * Report possible change in axis value.
 *
 * Requests to update unknown axes are ignored.
 *
 * Parameter id: The ID of axis.
 * Parameter value: The value.
 */
	void report_axis(uint64_t id, int64_t value);
/**
 * Report possible change in button value.
 *
 * Requests to update unknown buttons are ignored.
 *
 * Parameter id: The ID of button.
 * Parameter value: The value.
 */
	void report_button(uint64_t id, bool value);
/**
 * Report possible change in POV value.
 *
 * Requests to update unknown POVs are ignored.
 *
 * Parameter id: The ID of POV.
 * Parameter value: The angle in hundredths of degree clockwise from up, or negative if centered.
 */
	void report_pov(uint64_t id, int angle);
/**
 * Report on joystick.
 */
	std::string compose_report(unsigned jnum);
private:
	struct change_info
	{
		change_info();
		void update(short newv);
		bool read(short& val);
	private:
		bool has_been_read;
		short last_read;
		short last_known;
	};
	struct hat_info
	{
		int xstate;
		int ystate;
	};
	struct hat_axis_info
	{
		bool yflag;		//Set on y axis, clear on x axis.
		int64_t mindev;
		unsigned hnum;
	};
	bool read_common(std::vector<change_info>& i, unsigned id, short& res);
	unsigned size_common(std::vector<change_info>& i);
	std::vector<change_info> _axes;
	std::vector<change_info> _buttons;
	std::vector<change_info> _hats;
	std::map<uint64_t, unsigned> button_map;
	std::map<uint64_t, unsigned> axis_map;	//No hats here!
	std::map<uint64_t, unsigned> pov_map;
	std::map<uint64_t, std::pair<int64_t, int64_t>> aranges;
	std::vector<hat_info> hatinfos;
	std::map<uint64_t, hat_axis_info> hataxis_map;
	std::map<unsigned, std::string> axisnames;
	std::map<unsigned, std::string> buttonnames;
	std::map<unsigned, std::string> hatnames;
	std::string joyname;
};

#endif
