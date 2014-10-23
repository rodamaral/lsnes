#ifndef GAMBATTE_EXTRACALLBACKS_H
#define GAMBATTE_EXTRACALLBACKS_H

#include <cstdint>

namespace gambatte {
//Extra callbacks.
struct extra_callbacks
{
	//Context passed to callbacks.
	void* context;
	//Read P1 with both P14 and P15 at 1.
	uint8_t (*read_p1_high)(void* context);
	//Write P1.
	void (*write_p1)(void* context, uint8_t value);
	//LCD scanline complete.
	void (*lcd_scan)(void* context, uint8_t y, const uint32_t* data);
	//Read P1 (if true, return raw value)
	bool (*read_p1)(void* context, uint8_t& val);
	//Call PPU update at every timeslice?
	bool ppu_update_timeslice;
};

extern const extra_callbacks default_extra_callbacks;
}

#endif
