#include "lua/internal.hpp"
#include "lua/unsaferewind.hpp"
#include "core/random.hpp"
#include "library/string.hpp"
#include "library/hex.hpp"

namespace
{
	uint64_t randnum()
	{
		return hex::from<uint64_t>(get_random_hexstring(16));
	}

	uint64_t randnum_mod(uint64_t y)
	{
		uint64_t rmultA = std::numeric_limits<uint64_t>::max() % y;
		uint64_t rmultB = std::numeric_limits<uint64_t>::max() / y;
		if(rmultA == y - 1) {
			//Exact div.
			return randnum() % y;
		} else {
			//Not exact.
			uint64_t bound = y * rmultB;
			uint64_t x = bound;
			while(x >= bound)
				x = randnum();
			return x % y;
		}
	}

	int rand_boolean(lua::state& L, lua::parameters& P)
	{
		L.pushboolean(randnum() % 2);
		return 1;
	}

	int rand_integer(lua::state& L, lua::parameters& P)
	{
		uint64_t low = 0, high = 0;

		P(low);

		if(P.is_number()) {
			P(high);
			if(low > high)
				throw std::runtime_error("random.integer: high > low");
		} else {
			high = low - 1;
			low = 0;
			if(high < 0)
				throw std::runtime_error("random.integer: high > low");
		}
		uint64_t rsize = high - low + 1;
		L.pushnumber(low + randnum_mod(rsize));
		return 1;
	}

	int rand_float(lua::state& L, lua::parameters& P)
	{
		double _bits = 0;
		uint64_t* bits = (uint64_t*)&_bits;
		*bits = randnum() & 0xFFFFFFFFFFFFFULL;
		*bits |= 0x3FF0000000000000ULL;
		L.pushnumber(_bits - 1);
		return 1;
	}

	int rand_among(lua::state& L, lua::parameters& P)
	{
		unsigned args = 1;
		while(P.more()) {
			P.skip();
			args++;
		}
		args--;
		if(!args) {
			L.pushnil();
			return 1;
		}
		uint64_t n = randnum_mod(args);
		L.pushvalue(n + 1);
		return 1;
	}

	int rand_amongtable(lua::state& L, lua::parameters& P)
	{
		int ltbl;
		P(P.table(ltbl));

		uint64_t size = 0;
		L.pushnil();
		while(L.next(ltbl)) {
			size++;
			L.pop(1);
		}
		if(!size) {
			L.pushnil();
			return 1;
		}
		uint64_t n = randnum_mod(size);
		L.pushnil();
		for(uint64_t i = 0; i < n; i++) {
			if(!L.next(ltbl))
				throw std::runtime_error("random.amongtable: Inconsistent table size");
			L.pop(1);
		}
		if(!L.next(ltbl))
			throw std::runtime_error("random.amongtable: Inconsistent table size");
		return 1;
	}

	lua::functions LUA_random_fns(lua_func_bit, "random", {
		{"boolean", rand_boolean},
		{"integer", rand_integer},
		{"float", rand_float},
		{"among", rand_among},
		{"amongtable", rand_amongtable},
	});
}
