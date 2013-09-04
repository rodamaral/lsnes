#include "demo.hpp"
#include <cstring>

namespace sky
{
	const uint16_t newdemo_lookup[] = {
		0x09, 0x08, 0x0A, 0x08, 0x01, 0x00, 0x02, 0x00, 0x05, 0x04, 0x06, 0x04, 0x01, 0x00, 0x02, 0x00,
		0x19, 0x18, 0x1A, 0x18, 0x11, 0x10, 0x12, 0x10, 0x15, 0x14, 0x16, 0x14, 0x11, 0x10, 0x12, 0x10
	};

	const uint16_t skyroads_lookup[] = {
		0x009, 0x001, 0x005, 0x201, 0x008, 0x000, 0x004, 0x200,
		0x00A, 0x002, 0x006, 0x202, 0x108, 0x100, 0x104, 0x300,
		0x019, 0x011, 0x015, 0x211, 0x018, 0x010, 0x014, 0x210,
		0x01A, 0x012, 0x016, 0x212, 0x118, 0x110, 0x114, 0x310
	};

	demo::demo()
	{
		buffer[0] = 0;
	}

	demo::demo(const std::vector<char>& demo, bool skyroads_fmt)
	{
		buffer[0] = skyroads_fmt ? 2 : 1;
		uint16_t dptr = 1;
		if(buffer[0] == 1) {
			//Uncompress RLE.
			size_t ptr = 0;
			while(ptr < demo.size()) {
				uint16_t count = (uint8_t)demo[ptr++];
				count++;
				uint8_t b = (ptr < demo.size()) ? demo[ptr++] : 5;
				for(unsigned i = 0; i < count; i++) {
					if(!dptr)
						break;
					buffer[dptr++] = b;
				}
			}
		}
		if(buffer[0] == 2) {
			for(unsigned i = 0; i < demo.size(); i++) {
				if(!dptr)
					break;
				buffer[dptr++] = demo[i];
			}
		}
		while(dptr)
			buffer[dptr++] = 5;	//Neutral input.
	}

	uint16_t demo::fetchkeys(uint16_t old, uint32_t lpos, uint32_t frame)
	{
		switch(buffer[0]) {
		case 1:
			old &= 96;
			if(frame < 65535)
				old |= newdemo_lookup[buffer[frame + 1] & 31];
			break;
		case 2:
			old &= 96;
			if(lpos / 1638  < 65535)
				old |= skyroads_lookup[buffer[lpos / 1638 + 1] & 31];
			break;
		default:
			old &= 127;
			break;
		}
		return old;
	}
}
