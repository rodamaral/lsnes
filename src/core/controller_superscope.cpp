#include "lsnes.hpp"
#include <snes/snes.hpp>
#include <ui-libsnes/libsnes.hpp>

#include "core/controllerframe.hpp"

namespace
{
	struct porttype_superscope : public porttype_info
	{
		porttype_superscope() : porttype_info(PT_SUPERSCOPE, "superscope", 5) {}
		void write(unsigned char* buffer, unsigned idx, unsigned ctrl, short x) const throw()
		{
			if(ctrl >= 6 || idx > 0)
				return;
			switch(ctrl) {
			case 0:
			case 1:
				serialize_short(buffer + 2 * ctrl + 1, x);
				break;
			case 2:
			case 3:
			case 4:
			case 5:
				if(x)
					buffer[0] |= (1 << (ctrl - 2));
				else
					buffer[0] &= ~(1 << (ctrl - 2));
				break;
			}
		}

		short read(const unsigned char* buffer, unsigned idx, unsigned ctrl) const  throw()
		{
			if(ctrl >= 6 || idx > 0)
				return 0;
			switch(ctrl) {
			case 0:
			case 1:
				return unserialize_short(buffer + 2 * ctrl + 1);
			case 2:
			case 3:
			case 4:
			case 5:
				return ((buffer[0] & (1 << (ctrl - 2))) != 0);
			}
		}

		void display(const unsigned char* buffer, unsigned idx, char* buf) const  throw()
		{
			if(idx > 0) {
				buf[0] = '\0';
				return;
			}
			sprintf(buf, "%i %i %c%c%c%c", unserialize_short(buffer + 1), unserialize_short(buffer + 3),
				((buffer[0] & 1) ? 'T' : '-'), ((buffer[0] & 2) ? 'C' : '-'),
				((buffer[0] & 4) ? 'U' : '-'), ((buffer[0] & 8) ? 'P' : '-'));
		}

		size_t serialize(const unsigned char* buffer, char* textbuf) const  throw()
		{
			char tmp[128];
			sprintf(tmp, "|%c%c%c%c %i %i", ((buffer[0] & 1) ? 'T' : '.'), ((buffer[0] & 2) ? 'C' : '.'),
				((buffer[0] & 4) ? 'U' : '.'), ((buffer[0] & 8) ? 'P' : '.'),
				unserialize_short(buffer + 1), unserialize_short(buffer + 3));
			size_t len = strlen(tmp);
			memcpy(textbuf, tmp, len);
			return len;
		}

		size_t deserialize(unsigned char* buffer, const char* textbuf) const  throw()
		{
			buffer[0] = 0;
			size_t idx = 0;
			if(read_button_value(textbuf, idx))
				buffer[0] |= 1;
			if(read_button_value(textbuf, idx))
				buffer[0] |= 2;
			if(read_button_value(textbuf, idx))
				buffer[0] |= 4;
			if(read_button_value(textbuf, idx))
				buffer[0] |= 8;
			serialize_short(buffer + 1, read_axis_value(textbuf, idx));
			serialize_short(buffer + 3, read_axis_value(textbuf, idx));
			skip_rest_of_field(textbuf, idx, false);
			return idx;
		}

		devicetype_t devicetype(unsigned idx) const  throw()
		{
			return (idx == 0) ? DT_SUPERSCOPE : DT_NONE;
		}

		unsigned controllers() const  throw()
		{
			return 1;
		}

		unsigned internal_type() const  throw()
		{
			return SNES_DEVICE_SUPER_SCOPE;
		}

		bool legal(unsigned port) const  throw()
		{
			return (port > 0);
		}

		int button_id(unsigned controller, unsigned lbid) const throw()
		{
			if(controller > 0)
				return -1;
			switch(lbid) {
			case LOGICAL_BUTTON_TRIGGER:	return SNES_DEVICE_ID_SUPER_SCOPE_TRIGGER;
			case LOGICAL_BUTTON_CURSOR:	return SNES_DEVICE_ID_SUPER_SCOPE_CURSOR;
			case LOGICAL_BUTTON_TURBO:	return SNES_DEVICE_ID_SUPER_SCOPE_TURBO;
			case LOGICAL_BUTTON_PAUSE:	return SNES_DEVICE_ID_SUPER_SCOPE_PAUSE;
			default:			return -1;
			}
		}

		void set_core_controller(unsigned port) const throw()
		{
			if(port > 1)
				return;
			snes_set_controller_port_device(port != 0, SNES_DEVICE_SUPER_SCOPE);
		}

		bool is_analog(unsigned controller) const throw()
		{
			return (controller == 0);
		}

		bool is_mouse(unsigned controller) const throw()
		{
			return false;
		}
	} superscope;
}
