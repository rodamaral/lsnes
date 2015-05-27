#ifndef _sky__random__hpp__included__
#define _sky__random__hpp__included__

#include <cstdint>

namespace sky
{
	struct random
	{
	public:
		random();
		void push(uint32_t x);
		uint32_t pull();
	private:
		void init();
		bool initialized;
		uint8_t state[32];
	};
}

#endif
