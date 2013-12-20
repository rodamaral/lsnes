#include "lua/internal.hpp"
#include "lua/unsaferewind.hpp"
#include "core/misc.hpp"
#include "library/string.hpp"

namespace
{
	uint64_t randnum()
	{
		std::string x = "0x" + get_random_hexstring(16);
		return parse_value<uint64_t>(x);
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

	lua::fnptr rboolean(lua_func_misc, "random.boolean", [](lua::state& L, const std::string& fname)
		-> int {
		L.pushboolean(randnum() % 2);
		return 1;
	});

	lua::fnptr rinteger(lua_func_misc, "random.integer", [](lua::state& L, const std::string& fname)
		-> int {
		int64_t low = 0;
		int64_t high = 0;
		if(L.type(2) == LUA_TNUMBER) {
			low = L.get_numeric_argument<int64_t>(1, fname.c_str());
			high = L.get_numeric_argument<int64_t>(2, fname.c_str());
			if(low > high)
				throw std::runtime_error("random.integer: high > low");
		} else {
			high = L.get_numeric_argument<int64_t>(1, fname.c_str()) - 1;
			if(high < 0)
				throw std::runtime_error("random.integer: high > low");
		}
		uint64_t rsize = high - low + 1;
		L.pushnumber(low + randnum_mod(rsize));
		return 1;
	});

	lua::fnptr rfloat(lua_func_misc, "random.float", [](lua::state& L, const std::string& fname)
		-> int {
		double _bits = 0;
		uint64_t* bits = (uint64_t*)&_bits;
		*bits = randnum() & 0xFFFFFFFFFFFFFULL;
		*bits |= 0x3FF0000000000000ULL;
		L.pushnumber(_bits - 1);
		return 1;
	});

	lua::fnptr ramong(lua_func_misc, "random.among", [](lua::state& L, const std::string& fname)
	{
		unsigned args = 1;
		while(L.type(args) != LUA_TNONE)
			args++;
		args--;
		if(!args) {
			L.pushnil();
			return 1;
		}
		uint64_t n = randnum_mod(args);
		L.pushvalue(n + 1);
		return 1;
	});

	lua::fnptr ramongt(lua_func_misc, "random.amongtable", [](lua::state& L, const std::string& fname)
	{
		if(L.type(1) != LUA_TTABLE)
			throw std::runtime_error("random.amongtable: First argument must be table");
		uint64_t size = 0;
		L.pushnil();
		while(L.next(1)) {
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
			if(!L.next(1))
				throw std::runtime_error("random.amongtable: Inconsistent table size");
			L.pop(1);
		}
		if(!L.next(1))
			throw std::runtime_error("random.amongtable: Inconsistent table size");
		return 1;
	});
}
