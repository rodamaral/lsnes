#include "lsnes.hpp"
#include "lua/bitmap.hpp"
#include "lua/internal.hpp"
#include "library/serialization.hpp"
#include "library/memoryspace.hpp"
#include "core/instance.hpp"
#include "core/memorymanip.hpp"
#ifdef BSNES_HAS_DEBUGGER
#define DEBUGGER
#endif
#include <snes/snes.hpp>
#include <gameboy/gameboy.hpp>
#include LIBSNES_INCLUDE_FILE


namespace
{
	int change_cpu_frequency(lua::state& L, lua::parameters& P)
	{
		uint64_t freq;
		P(freq);
		SNES::cpu.frequency = freq;
		return 0;
	}

	int change_smp_frequency(lua::state& L, lua::parameters& P)
	{
		uint64_t freq;
		P(freq);
		SNES::smp.frequency = freq;
		return 0;
	}

	int get_cpu_frequency(lua::state& L, lua::parameters& P)
	{
		L.pushnumber(SNES::cpu.frequency);
		return 1;
	}

	int get_smp_frequency(lua::state& L, lua::parameters& P)
	{
		L.pushnumber(SNES::smp.frequency);
		return 1;
	}

	lua::functions bitmap_fns_snes(lua_func_misc, "bsnes", {
		{"set_cpu_frequency", change_cpu_frequency},
		{"set_smp_frequency", change_smp_frequency},
		{"get_cpu_frequency", get_cpu_frequency},
		{"get_smp_frequency", get_smp_frequency},
	});
}
