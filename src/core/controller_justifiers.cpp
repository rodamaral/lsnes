#include "lsnes.hpp"
#include <snes/snes.hpp>
#include <ui-libsnes/libsnes.hpp>

#include "core/controllerframe.hpp"

namespace
{
	struct porttype_justifiers : public porttype_info
	{
		porttype_justifiers() : porttype_info(PT_JUSTIFIERS, "justifiers", 9) {}
		void write(unsigned char* buffer, unsigned idx, unsigned ctrl, short x) const throw()
		{
			if(ctrl >= 4 || idx > 1)
				return;
			switch(ctrl) {
			case 0:
			case 1:
				serialize_short(buffer + 4 * idx + 2 * ctrl + 1, x);
				break;
			case 2:
			case 3:
				if(x)
					buffer[0] |= (1 << (2 * idx + ctrl - 2));
				else
					buffer[0] &= ~(1 << (2 * idx + ctrl - 2));
				break;
			}
		}

		short read(const unsigned char* buffer, unsigned idx, unsigned ctrl) const  throw()
		{
			if(ctrl >= 4 || idx > 1)
				return 0;
			switch(ctrl) {
			case 0:
			case 1:
				return unserialize_short(buffer + 4 * idx + 2 * ctrl + 1);
			case 2:
			case 3:
				return ((buffer[0] & (1 << (2 * idx + ctrl - 2))) != 0);
			}
		}

		void display(const unsigned char* buffer, unsigned idx, char* buf) const  throw()
		{
			if(idx == 0)
				sprintf(buf, "%i %i %c%c", unserialize_short(buffer + 1),
					unserialize_short(buffer + 3), ((buffer[0] & 1) ? 'T' : '-'),
					((buffer[0] & 2) ? 'S' : '-'));
			else if(idx == 1)
				sprintf(buf, "%i %i %c%c", unserialize_short(buffer + 5),
					unserialize_short(buffer + 7), ((buffer[0] & 4) ? 'T' : '-'),
					((buffer[0] & 8) ? 'S' : '-'));
			else
				buf[0] = '\0';
		}

		size_t serialize(const unsigned char* buffer, char* textbuf) const  throw()
		{
			char tmp[128];
			sprintf(tmp, "|%c%c %i %i|%c%c %i %i", ((buffer[0] & 1) ? 'T' : '.'),
				((buffer[0] & 2) ? 'S' : '.'), unserialize_short(buffer + 1),
				unserialize_short(buffer + 3), ((buffer[0] & 4) ? 'T' : '.'),
				((buffer[0] & 8) ? 'S' : '.'), unserialize_short(buffer + 5),
				unserialize_short(buffer + 7));
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
			serialize_short(buffer + 1, read_axis_value(textbuf, idx));
			serialize_short(buffer + 3, read_axis_value(textbuf, idx));
			skip_rest_of_field(textbuf, idx, true);
			if(read_button_value(textbuf, idx))
				buffer[0] |= 4;
			if(read_button_value(textbuf, idx))
				buffer[0] |= 8;
			serialize_short(buffer + 5, read_axis_value(textbuf, idx));
			serialize_short(buffer + 7, read_axis_value(textbuf, idx));
			skip_rest_of_field(textbuf, idx, false);
			return idx;
		}

		devicetype_t devicetype(unsigned idx) const  throw()
		{
			return (idx < 2) ? DT_JUSTIFIER : DT_NONE;
		}

		unsigned controllers() const  throw()
		{
			return 2;
		}

		unsigned internal_type() const  throw()
		{
			return SNES_DEVICE_JUSTIFIERS;
		}

		bool legal(unsigned port) const  throw()
		{
			return (port > 0);
		}
	} justifiers;
}
