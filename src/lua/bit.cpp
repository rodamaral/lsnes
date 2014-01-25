#include "lua/internal.hpp"
#include "library/minmax.hpp"
#include "library/int24.hpp"
#include "library/serialization.hpp"

#define BITWISE_BITS 48
#define BITWISE_MASK ((1ULL << (BITWISE_BITS)) - 1)

namespace
{
	uint64_t combine_none(uint64_t chain, uint64_t arg)
	{
		return (chain & ~arg) & BITWISE_MASK;
	}

	uint64_t combine_any(uint64_t chain, uint64_t arg)
	{
		return (chain | arg) & BITWISE_MASK;
	}

	uint64_t combine_all(uint64_t chain, uint64_t arg)
	{
		return (chain & arg) & BITWISE_MASK;
	}

	uint64_t combine_parity(uint64_t chain, uint64_t arg)
	{
		return (chain ^ arg) & BITWISE_MASK;
	}

	uint64_t shift_lrotate(uint64_t base, uint64_t amount, uint64_t bits)
	{
		uint64_t mask = ((1ULL << bits) - 1);
		base &= mask;
		base = (base << amount) | (base >> (bits - amount));
		return base & mask & BITWISE_MASK;
	}

	uint64_t shift_rrotate(uint64_t base, uint64_t amount, uint64_t bits)
	{
		uint64_t mask = ((1ULL << bits) - 1);
		base &= mask;
		base = (base >> amount) | (base << (bits - amount));
		return base & mask & BITWISE_MASK;
	}

	uint64_t shift_lshift(uint64_t base, uint64_t amount, uint64_t bits)
	{
		uint64_t mask = ((1ULL << bits) - 1);
		base <<= amount;
		return base & mask & BITWISE_MASK;
	}

	uint64_t shift_lrshift(uint64_t base, uint64_t amount, uint64_t bits)
	{
		uint64_t mask = ((1ULL << bits) - 1);
		base &= mask;
		base >>= amount;
		return base & BITWISE_MASK;
	}

	uint64_t shift_arshift(uint64_t base, uint64_t amount, uint64_t bits)
	{
		uint64_t mask = ((1ULL << bits) - 1);
		base &= mask;
		bool negative = ((base >> (bits - 1)) != 0);
		base >>= amount;
		base |= ((negative ? BITWISE_MASK : 0) << (bits - amount));
		return base & mask & BITWISE_MASK;
	}

	template<uint64_t (*combine)(uint64_t chain, uint64_t arg), uint64_t init>
	int fold(lua::state& L, lua::parameters& P)
	{
		uint64_t ret = init;
		while(P.more())
			ret = combine(ret, P.arg<uint64_t>());
		L.pushnumber(ret);
		return 1;
	}

	template<uint64_t (*sh)(uint64_t base, uint64_t amount, uint64_t bits)>
	int shift(lua::state& L, lua::parameters& P)
	{
		auto base = P.arg<uint64_t>();
		auto amount = P.arg_opt<uint64_t>(1);
		auto bits = P.arg_opt<uint64_t>(BITWISE_BITS);
		L.pushnumber(sh(base, amount, bits));
		return 1;
	}

	template<typename T>
	int bswap(lua::state& L, lua::parameters& P)
	{
		T val = P.arg<T>();
		serialization::swap_endian(val);
		L.pushnumber(val);
		return 1;
	}

	lua::fnptr2 lua_bextract(lua_func_bit, "bit.extract", [](lua::state& L, lua::parameters& P)
		-> int {
		uint64_t num = P.arg<uint64_t>();
		uint64_t ret = 0;
		for(size_t i = 0;; i++) {
			if(P.is_boolean()) {
				if(P.arg<bool>())
					ret |= (1ULL << i);
			} else if(P.is_number()) {
				ret |= (((num >> P.arg<uint8_t>()) & 1) << i);
			} else
				break;
		}
		L.pushnumber(ret);
		return 1;
	});

	lua::fnptr2 lua_bvalue(lua_func_bit, "bit.value", [](lua::state& L, lua::parameters& P) -> int {
		uint64_t ret = 0;
		for(size_t i = 0;; i++) {
			if(P.is_number()) {
				ret |= (1ULL << P.arg<uint8_t>());
			} else if(P.is_nil()) {
			} else
				break;
		}
		L.pushnumber(ret);
		return 1;
	});

	lua::fnptr2 lua_testany(lua_func_bit, "bit.test_any", [](lua::state& L, lua::parameters& P) -> int {
		auto a = P.arg<uint64_t>();
		auto b = P.arg<uint64_t>();
		L.pushboolean((a & b) != 0);
		return 1;
	});

	lua::fnptr2 lua_testall(lua_func_bit, "bit.test_all", [](lua::state& L, lua::parameters& P) -> int {
		auto a = P.arg<uint64_t>();
		auto b = P.arg<uint64_t>();
		L.pushboolean((a & b) == b);
		return 1;
	});

	int poptable[] = {0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4};

	int popcount(uint64_t x)
	{
		int c = 0;
		for(unsigned i = 0; i < 16; i++) {
			c += poptable[x & 15];
			x >>= 4;
		}
		return c;
	}

	lua::fnptr2 lua_popcount(lua_func_bit, "bit.popcount", [](lua::state& L, lua::parameters& P) -> int {
		auto a = P.arg<uint64_t>();
		L.pushnumber(popcount(a));
		return 1;
	});

	lua::fnptr2 lua_clshift(lua_func_bit, "bit.clshift", [](lua::state& L, lua::parameters& P) -> int {
		auto a = P.arg<uint64_t>();
		auto b = P.arg<uint64_t>();
		auto amount = P.arg_opt<unsigned>(1);
		auto bits = P.arg_opt<unsigned>(BITWISE_BITS);
		uint64_t mask = ((1ULL << bits) - 1);
		a &= mask;
		b &= mask;
		a <<= amount;
		a &= mask;
		a |= (b >> (bits - amount));
		b <<= amount;
		b &= mask;
		L.pushnumber(a);
		L.pushnumber(b);
		return 2;
	});

	lua::fnptr2 lua_crshift(lua_func_bit, "bit.crshift", [](lua::state& L, lua::parameters& P) -> int {
		auto a = P.arg<uint64_t>();
		auto b = P.arg<uint64_t>();
		auto amount = P.arg_opt<unsigned>(1);
		auto bits = P.arg_opt<unsigned>(BITWISE_BITS);
		uint64_t mask = ((1ULL << bits) - 1);
		a &= mask;
		b &= mask;
		b >>= amount;
		b |= (a << (bits - amount));
		b &= mask;
		a >>= amount;
		L.pushnumber(a);
		L.pushnumber(b);
		return 2;
	});

	template<bool reverse>
	int flagdecode_core(lua::state& L, lua::parameters& P)
	{
		auto a = P.arg<uint64_t>();
		auto b = P.arg<uint64_t>();
		auto on = P.arg_opt<std::string>("");
		auto off = P.arg_opt<std::string>("");
		size_t onl = on.length();
		size_t offl = off.length();
		char onc = onl ? on[onl - 1] : '*';
		char offc = offl ? off[offl - 1] : '-';
		char buffer[65];
		unsigned i;
		size_t bias = min(b, (uint64_t)64) - 1;
		for(i = 0; i < 64 && i < b; i++) {
			char onc2 = (i < onl) ? on[i] : onc;
			char offc2 = (i < offl) ? off[i] : offc;
			buffer[reverse ? (bias - i) : i] = ((a >> i) & 1) ? onc2 : offc2;
		}
		buffer[i] = '\0';
		L.pushstring(buffer);
		return 1;
	}

	lua::fnptr2 lua_flagdecode(lua_func_bit, "bit.flagdecode", flagdecode_core<false>);
	lua::fnptr2 lua_rflagdecode(lua_func_bit, "bit.rflagdecode", flagdecode_core<true>);
	lua::fnptr2 bit_none(lua_func_bit, "bit.none", fold<combine_none, BITWISE_MASK>);
	lua::fnptr2 bit_any(lua_func_bit, "bit.any", fold<combine_any, 0>);
	lua::fnptr2 bit_all(lua_func_bit, "bit.all", fold<combine_all, BITWISE_MASK>);
	lua::fnptr2 bit_parity(lua_func_bit, "bit.parity", fold<combine_parity, 0>);
	lua::fnptr2 bit_lrotate(lua_func_bit, "bit.lrotate", shift<shift_lrotate>);
	lua::fnptr2 bit_rrotate(lua_func_bit, "bit.rrotate", shift<shift_rrotate>);
	lua::fnptr2 bit_lshift(lua_func_bit, "bit.lshift", shift<shift_lshift>);
	lua::fnptr2 bit_arshift(lua_func_bit, "bit.arshift", shift<shift_arshift>);
	lua::fnptr2 bit_lrshift(lua_func_bit, "bit.lrshift", shift<shift_lrshift>);
	lua::fnptr2 bit_swapword(lua_func_bit, "bit.swapword", bswap<uint16_t>);
	lua::fnptr2 bit_swaphword(lua_func_bit, "bit.swaphword", bswap<ss_uint24_t>);
	lua::fnptr2 bit_swapdword(lua_func_bit, "bit.swapdword", bswap<uint32_t>);
	lua::fnptr2 bit_swapqword(lua_func_bit, "bit.swapqword", bswap<uint64_t>);
	lua::fnptr2 bit_swapsword(lua_func_bit, "bit.swapsword", bswap<int16_t>);
	lua::fnptr2 bit_swapshword(lua_func_bit, "bit.swapshword", bswap<ss_int24_t>);
	lua::fnptr2 bit_swapsdword(lua_func_bit, "bit.swapsdword", bswap<int32_t>);
	lua::fnptr2 bit_swapsqword(lua_func_bit, "bit.swapsqword", bswap<int64_t>);
}
