#ifndef _skycore__demo__hpp__included__
#define _skycore__demo__hpp__included__

#include <cstdint>
#include <cstdlib>
#include <vector>

namespace sky
{
	struct demo
	{
		demo();
		demo(const std::vector<char>& demo, bool skyroads_fmt);
		uint16_t fetchkeys(uint16_t old, uint32_t lpos, uint32_t frame);
	private:
		uint8_t buffer[65536];
	};
}
#endif
