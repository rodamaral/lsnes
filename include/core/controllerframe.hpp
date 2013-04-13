#ifndef _controllerframe__hpp__included__
#define _controllerframe__hpp__included__

#include <cstring>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <map>
#include <list>
#include "library/controller-data.hpp"

/**
 * Controllers state.
 */
class controller_state
{
public:
/**
 * Constructor.
 */
	controller_state() throw();
/**
 * Convert lcid (Logical Controller ID) into pcid (Physical Controler ID).
 *
 * Parameter lcid: The logical controller ID.
 * Return: The physical controller ID, or <-1, -1> if no such controller exists.
 */
	std::pair<int, int> lcid_to_pcid(unsigned lcid) throw();
/**
 * Lookup (port,controller) pair corresponding to given legacy pcid.
 *
 * Parameter pcid: The legacy pcid.
 * Returns: The controller index, or <-1, -1> if no such thing exists.
 * Note: Even if this does return a valid index, it still may not exist.
 */
	std::pair<int, int> legacy_pcid_to_pair(unsigned pcid) throw();
/**
 * Is given pcid present?
 *
 * Parameter port: The port.
 * Parameter controller: The controller.
 * Returns: True if present, false if not.
 */
	bool pcid_present(unsigned port, unsigned controller) throw();
/**
 * Set types of ports.
 *
 * Parameter ptype: The new types for ports.
 * Throws std::runtime_error: Illegal port type.
 */
	void set_ports(const port_type_set& ptype) throw(std::runtime_error);
/**
 * Get status of current controls (with autohold/autofire factored in).
 *
 * Parameter framenum: Number of current frame (for evaluating autofire).
 * Returns: The current controls.
 */
	controller_frame get(uint64_t framenum) throw();
/**
 * Commit given controls (autohold/autofire is ignored).
 *
 * Parameter controls: The controls to commit
 */
	void commit(controller_frame controls) throw();
/**
 * Get status of committed controls.
 * Returns: The committed controls.
 */
	controller_frame get_committed() throw();
/**
 * Get blank frame.
 */
	controller_frame get_blank() throw();
/**
 * Send analog input to given controller.
 *
 * Parameter port: The port to send input to.
 * Parameter controller: The controller to send input to.
 * Parameter control: The control to send.
 * Parameter x: The coordinate to send.
 */
	void analog(unsigned port, unsigned controller, unsigned control, short x) throw();
/**
 * Manipulate autohold.
 *
 * Parameter port: The port.
 * Parameter controller: The controller.
 * Parameter pbid: The physical button ID to manipulate.
 * Parameter newstate: The new state for autohold.
 */
	void autohold2(unsigned port, unsigned controller, unsigned pbid, bool newstate) throw();
/**
 * Query autohold.
 *
 * Parameter port: The port.
 * Parameter controller: The controller.
 * Parameter pbid: The physical button ID to query.
 * Returns: The state of autohold.
 */
	bool autohold2(unsigned port, unsigned controller, unsigned pbid) throw();
/**
 * Reset all frame holds.
 */
	void reset_framehold() throw();
/**
 * Manipulate hold for frame.
 *
 * Parameter port: The port.
 * Parameter controller: The controller.
 * Parameter pbid: The physical button ID to manipulate.
 * Parameter newstate: The new state for framehold.
 */
	void framehold2(unsigned port, unsigned controller, unsigned pbid, bool newstate) throw();
/**
 * Query hold for frame.
 *
 * Parameter port: The port.
 * Parameter controller: The controller.
 * Parameter pbid: The physical button ID to query.
 * Returns: The state of framehold.
 */
	bool framehold2(unsigned port, unsigned controller, unsigned pbid) throw();
/**
 * Manipulate button.
 *
 * Parameter port: The port.
 * Parameter controller: The controller.
 * Parameter pbid: The physical button ID to manipulate.
 * Parameter newstate: The new state for button.
 */
	void button2(unsigned port, unsigned controller, unsigned pbid, bool newstate) throw();
/**
 * Query button.
 *
 * Parameter port: The port.
 * Parameter controller: The controller.
 * Parameter pbid: The physical button ID to query.
 * Returns: The state of button.
 */
	bool button2(unsigned port, unsigned controller, unsigned pbid) throw();
/**
 * Manipulate autofire.
 *
 * Parameter port: The port.
 * Parameter controller: The controller.
 * Parameter pbid: The physical button ID to manipulate.
 * Parameter duty: The new duty cycle for autofire.
 * Parameter cyclelen: The new cycle length.
 */
	void autofire2(unsigned port, unsigned controller, unsigned pbid, unsigned duty, unsigned cyclelen) throw();
/**
 * Query autofire.
 *
 * Parameter port: The port.
 * Parameter controller: The controller.
 * Parameter pbid: The physical button ID to query.
 * Returns: The state of autofire.
 */
	std::pair<unsigned, unsigned> autofire2(unsigned port, unsigned controller, unsigned pbid) throw();
/**
 * Manipulate TASinput.
 *
 * Parameter port: The port.
 * Parameter controller: The controller.
 * Parameter pbid: The physical button ID to manipulate.
 * Parameter state: The new state.
 */
	void tasinput(unsigned port, unsigned controller, unsigned pbid, int16_t state) throw();
/**
 * Query tasinput.
 *
 * Parameter port: The port.
 * Parameter controller: The controller.
 * Parameter pbid: The physical button ID to query.
 * Returns: The state of tasinput.
 */
	int16_t tasinput(unsigned port, unsigned controller, unsigned pbid) throw();
/**
 * Enage/Disenage tasinput.
 */
	void tasinput_enable(bool enabled);
/**
 * TODO: Document.
 */
	bool is_present(unsigned port, unsigned controller) throw();
private:
	struct autofire_info
	{
		uint64_t first_frame;
		unsigned duty;
		unsigned cyclelen;
		bool eval_at(uint64_t frame);
	};
	struct tasinput_info
	{
		int mode;
		int16_t state;
	};
	void reread_tasinput_mode(const port_type_set& ptype);
	const port_type_set* types;
	controller_frame _input;
	controller_frame _autohold;
	controller_frame _framehold;
	std::map<unsigned, autofire_info> _autofire;
	std::map<unsigned, tasinput_info> _tasinput;
	bool tasinput_enaged;
	controller_frame _committed;
};

/**
 * Generic port controller name function.
 */
template<unsigned controllers, const char** name>
inline const char* generic_controller_name(unsigned controller)
{
	if(controller >= controllers)
		return NULL;
	return *name;
}

#endif
