#ifndef _library__portfn__hpp__included__
#define _library__portfn__hpp__included__

#include <cstdlib>
#include <cstdint>
#include "controller-data.hpp"

/**
 * Generic port write function.
 */
template<unsigned controllers, unsigned analog_axis, unsigned buttons>
inline void generic_port_write(unsigned char* buffer, unsigned idx, unsigned ctrl, short x) throw()
{
	if(idx >= controllers)
		return;
	if(ctrl < analog_axis) {
		size_t offset = (controllers * buttons + 7) / 8;
		buffer[2 * idx * analog_axis + 2 * ctrl + offset] = (x >> 8);
		buffer[2 * idx * analog_axis + 2 * ctrl + offset + 1] = x;
	} else if(ctrl < analog_axis + buttons) {
		size_t bit = idx * buttons + ctrl - analog_axis;
		if(x)
			buffer[bit / 8] |= (1 << (bit % 8));
		else
			buffer[bit / 8] &= ~(1 << (bit % 8));
	}
}

/**
 * Generic port read function.
 */
template<unsigned controllers, unsigned analog_axis, unsigned buttons>
inline short generic_port_read(const unsigned char* buffer, unsigned idx, unsigned ctrl) throw()
{
	if(idx >= controllers)
		return 0;
	if(ctrl < analog_axis) {
		size_t offset = (controllers * buttons + 7) / 8;
		uint16_t a = buffer[2 * idx * analog_axis + 2 * ctrl + offset];
		uint16_t b = buffer[2 * idx * analog_axis + 2 * ctrl + offset + 1];
		return static_cast<short>(256 * a + b);
	} else if(ctrl < analog_axis + buttons) {
		size_t bit = idx * buttons + ctrl - analog_axis;
		return ((buffer[bit / 8] & (1 << (bit % 8))) != 0);
	} else
		return 0;
}

/**
 * Generic port size function.
 */
template<unsigned controllers, unsigned analog_axis, unsigned buttons>
inline size_t generic_port_size()
{
	return 2 * controllers * analog_axis + (controllers * buttons + 7) / 8;
}

/**
 * Generic port deserialization function.
 */
template<unsigned controllers, unsigned analog_axis, unsigned buttons>
inline size_t generic_port_deserialize(unsigned char* buffer, const char* textbuf) throw()
{
	if(!controllers)
		return DESERIALIZE_SPECIAL_BLANK;
	memset(buffer, 0, generic_port_size<controllers, analog_axis, buttons>());
	size_t ptr = 0;
	size_t offset = (controllers * buttons + 7) / 8;
	for(unsigned j = 0; j < controllers; j++) {
		for(unsigned i = 0; i < buttons; i++) {
			size_t bit = j * buttons + i;
			if(read_button_value(textbuf, ptr))
				buffer[bit / 8] |= (1 << (bit % 8));
		}
		for(unsigned i = 0; i < analog_axis; i++) {
			short v = read_axis_value(textbuf, ptr);
			buffer[2 * j * analog_axis + 2 * i + offset] = v >> 8;
			buffer[2 * j * analog_axis + 2 * i + offset + 1] = v;
		}
		skip_rest_of_field(textbuf, ptr, j + 1 < controllers);
	}
	return ptr;
}

/**
 * Generic port display function.
 */
template<unsigned controllers, unsigned analog_axis, unsigned buttons, const char** bbuffer>
inline void generic_port_display(const unsigned char* buffer, unsigned idx, char* buf) throw()
{
	if(idx > controllers) {
		buf[0] = '\0';
		return;
	}
	size_t ptr = 0;
	size_t offset = (controllers * buttons + 7) / 8;
	for(unsigned i = 0; i < analog_axis; i++) {
		uint16_t a = buffer[2 * idx * analog_axis + 2 * i + offset];
		uint16_t b = buffer[2 * idx * analog_axis + 2 * i + offset + 1];
		ptr += sprintf(buf + ptr, "%i ", static_cast<short>(256 * a + b));
	}
	for(unsigned i = 0; i < buttons; i++) {
		size_t bit = idx * buttons + i;
		buf[ptr++] = ((buffer[bit / 8] & (1 << (bit % 8))) != 0) ? (*bbuffer[i]) : '-';
	}
	buf[ptr] = '\0';
}

/**
 * Generic port serialization function.
 */
template<unsigned controllers, unsigned analog_axis, unsigned buttons, const char** bbuffer>
inline size_t generic_port_serialize(const unsigned char* buffer, char* textbuf) throw()
{
	size_t ptr = 0;
	size_t offset = (controllers * buttons + 7) / 8;
	for(unsigned j = 0; j < controllers; j++) {
		textbuf[ptr++] = '|';
		for(unsigned i = 0; i < buttons; i++) {
			size_t bit = j * buttons + i;
			textbuf[ptr++] = ((buffer[bit / 8] & (1 << (bit % 8))) != 0) ? (*bbuffer)[i] : '.';
		}
		for(unsigned i = 0; i < analog_axis; i++) {
			uint16_t a = buffer[2 * j * analog_axis + 2 * i + offset];
			uint16_t b = buffer[2 * j * analog_axis + 2 * i + offset + 1];
			ptr += sprintf(textbuf + ptr, " %i", static_cast<short>(256 * a + b));
		}
	}
	return ptr;
}

#endif
