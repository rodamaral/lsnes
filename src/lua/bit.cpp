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
		uint64_t base, amount, bits;

		P(base, P.optional(amount, 1), P.optional(bits, BITWISE_BITS));

		L.pushnumber(sh(base, amount, bits));
		return 1;
	}

	template<typename T>
	int bswap(lua::state& L, lua::parameters& P)
	{
		T val;

		P(val);

		serialization::swap_endian(val);
		L.pushnumber(val);
		return 1;
	}

	int bit_extract(lua::state& L, lua::parameters& P)
	{
		uint64_t ret = 0;
		uint64_t num;

		P(num);

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
	}

	int bit_value(lua::state& L, lua::parameters& P)
	{
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
	}

	template<bool all>
	int bit_test(lua::state& L, lua::parameters& P)
	{
		uint64_t a, b;

		P(a, b);

		uint64_t t = a & b;
		bool c = all ? (t == b) : (t != 0);
		L.pushboolean(c);
		return 1;
	}

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

	int bit_popcount(lua::state& L, lua::parameters& P)
	{
		uint64_t a;

		P(a);

		L.pushnumber(popcount(a));
		return 1;
	}

	template<bool right>
	int bit_cshift(lua::state& L, lua::parameters& P)
	{
		uint64_t a, b;
		unsigned amount, bits;

		P(a, b, P.optional(amount, 1), P.optional(bits, BITWISE_BITS));

		uint64_t mask = ((1ULL << bits) - 1);
		a &= mask;
		b &= mask;
		if(right) {
			b >>= amount;
			b |= (a << (bits - amount));
			b &= mask;
			a >>= amount;
		} else {
			a <<= amount;
			a &= mask;
			a |= (b >> (bits - amount));
			b <<= amount;
			b &= mask;
		}
		L.pushnumber(a);
		L.pushnumber(b);
		return 2;
	}

	template<bool reverse>
	int flagdecode_core(lua::state& L, lua::parameters& P)
	{
		uint64_t a, b;
		std::string on, off;

		P(a, b, P.optional(on, ""), P.optional(off, ""));

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

	class lua_bit_dummy {};
	lua::_class<lua_bit_dummy> bitops(lua_class_pure, "*bit", {
		{"flagdecode", flagdecode_core<false>},
		{"rflagdecode", flagdecode_core<true>},
		{"none", fold<combine_none, BITWISE_MASK>},
		{"any", fold<combine_any, 0>},
		{"all", fold<combine_all, BITWISE_MASK>},
		{"parity", fold<combine_parity, 0>},
		{"bnot", fold<combine_none, BITWISE_MASK>},
		{"bor", fold<combine_any, 0>},
		{"band", fold<combine_all, BITWISE_MASK>},
		{"bxor", fold<combine_parity, 0>},
		{"lrotate", shift<shift_lrotate>},
		{"rrotate", shift<shift_rrotate>},
		{"lshift", shift<shift_lshift>},
		{"arshift", shift<shift_arshift>},
		{"lrshift", shift<shift_lrshift>},
		{"swapword", bswap<uint16_t>},
		{"swaphword", bswap<ss_uint24_t>},
		{"swapdword", bswap<uint32_t>},
		{"swapqword", bswap<uint64_t>},
		{"swapsword", bswap<int16_t>},
		{"swapshword", bswap<ss_int24_t>},
		{"swapsdword", bswap<int32_t>},
		{"swapsqword", bswap<int64_t>},
		{"extract", bit_extract},
		{"value", bit_value},
		{"test_any", bit_test<false>},
		{"test_all", bit_test<true>},
		{"popcount", bit_popcount},
		{"clshift", bit_cshift<false>},
		{"crshift", bit_cshift<true>},
	});
}
