#include "lsnes.hpp"
#include <snes/snes.hpp>
#include <ui-libsnes/libsnes.hpp>

#include "core/controllerframe.hpp"

namespace
{
	const char* buttons = "BYsSudlrAXLR";

	struct porttype_gamepad : public porttype_info
	{
		porttype_gamepad() : porttype_info(PT_GAMEPAD, "gamepad", 2) {}
		void write(unsigned char* buffer, unsigned idx, unsigned ctrl, short x) const throw()
		{
			if(ctrl >= 12 || idx > 0)
				return;
			if(x)
				buffer[ctrl / 8] |= (1 << (ctrl % 8));
			else
				buffer[ctrl / 8] &= ~(1 << (ctrl % 8));
		}

		short read(const unsigned char* buffer, unsigned idx, unsigned ctrl) const  throw()
		{
			if(ctrl >= 12 || idx > 0)
				return 0;
			return ((buffer[ctrl / 8] & (1 << (ctrl % 8))) != 0);
		}

		void display(const unsigned char* buffer, unsigned idx, char* buf) const  throw()
		{
			if(idx > 0) {
				buf[0] = '\0';
				return;
			}
			for(unsigned i = 0; i < 12; i++)
				buf[i] = (buffer[i / 8] & (1 << (i % 8))) ? buttons[i] : ' ';
			buf[12] = '\0';
		}

		size_t serialize(const unsigned char* buffer, char* textbuf) const  throw()
		{
			textbuf[0] = '|';
			for(unsigned i = 0; i < 12; i++)
				textbuf[i + 1] = (buffer[i / 8] & (1 << (i % 8))) ? buttons[i] : '.';
			return 13;
		}

		size_t deserialize(unsigned char* buffer, const char* textbuf) const  throw()
		{
			buffer[0] = 0;
			buffer[1] = 0;
			size_t idx = 0;
			for(unsigned i = 0; i < 12; i++)
				if(read_button_value(textbuf, idx))
					buffer[i / 8] |= (1 << (i % 8));
			skip_rest_of_field(textbuf, idx, false);
			return idx;
		}

		devicetype_t devicetype(unsigned idx) const  throw()
		{
			return (idx == 0) ? DT_GAMEPAD : DT_NONE;
		}

		unsigned controllers() const  throw()
		{
			return 1;
		}

		unsigned internal_type() const  throw()
		{
			return SNES_DEVICE_JOYPAD;
		}

		bool legal(unsigned port) const  throw()
		{
			return true;
		}

		int button_id(unsigned controller, unsigned lbid) const throw()
		{
			if(controller > 0)
				return -1;
			switch(lbid) {
			case LOGICAL_BUTTON_LEFT:	return SNES_DEVICE_ID_JOYPAD_LEFT;
			case LOGICAL_BUTTON_RIGHT:	return SNES_DEVICE_ID_JOYPAD_RIGHT;
			case LOGICAL_BUTTON_UP:		return SNES_DEVICE_ID_JOYPAD_UP;
			case LOGICAL_BUTTON_DOWN:	return SNES_DEVICE_ID_JOYPAD_DOWN;
			case LOGICAL_BUTTON_A:		return SNES_DEVICE_ID_JOYPAD_A;
			case LOGICAL_BUTTON_B:		return SNES_DEVICE_ID_JOYPAD_B;
			case LOGICAL_BUTTON_X:		return SNES_DEVICE_ID_JOYPAD_X;
			case LOGICAL_BUTTON_Y:		return SNES_DEVICE_ID_JOYPAD_Y;
			case LOGICAL_BUTTON_L:		return SNES_DEVICE_ID_JOYPAD_L;
			case LOGICAL_BUTTON_R:		return SNES_DEVICE_ID_JOYPAD_R;
			case LOGICAL_BUTTON_SELECT:	return SNES_DEVICE_ID_JOYPAD_SELECT;
			case LOGICAL_BUTTON_START:	return SNES_DEVICE_ID_JOYPAD_START;
			default:			return -1;
			}
		}

		void set_core_controller(unsigned port) const throw()
		{
			if(port > 1)
				return;
			snes_set_controller_port_device(port != 0, SNES_DEVICE_JOYPAD);
		}
	} gamepad;
}
