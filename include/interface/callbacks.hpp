#ifndef _interface__callbacks__hpp__included__
#define _interface__callbacks__hpp__included__

#include <cstdint>
#include "library/framebuffer.hpp"

//Callbacks.
struct emucore_callbacks
{
public:
	virtual ~emucore_callbacks() throw();
	//Get the input for specified control.
	virtual int16_t get_input(unsigned port, unsigned index, unsigned control) = 0;
	//Set the input for specified control (used for system controls, only works in readwrite mode).
	//Returns the actual value of control (may differ if in readonly mode).
	virtual int16_t set_input(unsigned port, unsigned index, unsigned control, int16_t value) = 0;
	//Do timer tick.
	virtual void timer_tick(uint32_t increment, uint32_t per_second) = 0;
	//Get the firmware path.
	virtual std::string get_firmware_path() = 0;
	//Get the base path.
	virtual std::string get_base_path() = 0;
	//Get the current time.
	virtual time_t get_time() = 0;
	//Get random seed.
	virtual time_t get_randomseed() = 0;
	//Output frame.
	virtual void output_frame(framebuffer_raw& screen, uint32_t fps_n, uint32_t fps_d) = 0;
};

extern struct emucore_callbacks* ecore_callbacks;

#endif