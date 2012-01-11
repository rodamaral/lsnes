#include "lsnes.hpp"
#include <snes/snes.hpp>
#include <ui-libsnes/libsnes.hpp>

#include "core/controllerframe.hpp"

namespace
{
	struct porttype_none : public porttype_info
	{
		porttype_none() : porttype_info(PT_NONE, "none", 0) {}
		void write(unsigned char* buffer, unsigned idx, unsigned ctrl, short x) const throw()
		{
		}

		short read(const unsigned char* buffer, unsigned idx, unsigned ctrl) const  throw()
		{
			return 0;
		}

		void display(const unsigned char* buffer, unsigned idx, char* buf) const  throw()
		{
			buf[0] = '\0';
		}

		size_t serialize(const unsigned char* buffer, char* textbuf) const  throw()
		{
			return 0;
		}

		size_t deserialize(unsigned char* buffer, const char* textbuf) const  throw()
		{
			return DESERIALIZE_SPECIAL_BLANK;
		}

		devicetype_t devicetype(unsigned idx) const  throw()
		{
			return DT_NONE;
		}

		unsigned controllers() const  throw()
		{
			return 0;
		}

		unsigned internal_type() const  throw()
		{
			return SNES_DEVICE_NONE;
		}

		bool legal(unsigned port) const  throw()
		{
			return true;
		}

		int button_id(unsigned controller, unsigned lbid) const throw()
		{
			return -1;
		}

		void set_core_controller(unsigned port) const throw()
		{
			if(port > 1)
				return;
			snes_set_controller_port_device(port != 0, SNES_DEVICE_NONE);
		}
	} none;
}
