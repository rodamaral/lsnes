#ifndef _skycore__romimage__hpp__included__
#define _skycore__romimage__hpp__included__

#include <cstdint>
#include <iostream>
#include <cstring>
#include <stdexcept>
#include <vector>
#include "gauge.hpp"
#include "level.hpp"
#include "image.hpp"
#include "sound.hpp"
#include "demo.hpp"

namespace sky
{
	void load_rom(struct instance& inst, const std::string& filename);
	void combine_background(struct instance& inst, size_t back);
	demo lookup_demo(struct instance& inst, const uint8_t* levelhash);
}


#endif
