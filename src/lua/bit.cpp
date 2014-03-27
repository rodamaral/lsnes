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

	template<bool complement>
	int bit_test2(lua::state& L, lua::parameters& P)
	{
		uint64_t a, b;

		P(a, b);

		uint64_t t = a & (1ULL << b);
		L.pushboolean((t != 0) != complement);
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

	int bit_compose(lua::state& L, lua::parameters& P)
	{
		uint64_t res = 0;
		uint8_t shift = 0;
		while(P.more()) {
			uint64_t t;
			P(t);
			res |= (t << shift);
			shift += 8;
		}
		L.pushnumber(res);
		return 1;
	}

	template<typename T, bool littleendian>
	int bit_ldbinarynumber(lua::state& L, lua::parameters& P)
	{
		T val;
		char buffer[sizeof(T)];
		size_t pos;
		const char* str;
		size_t len;

		str = L.tolstring(P.skip(), len);
		P(pos);

		if(pos + sizeof(T) > len)
			throw std::runtime_error(P.get_fname() + ": Offset out of range");

		L.pushnumber(serialization::read_endian<T>(str + pos, littleendian ? -1 : 1));
		return 1;
	}

	template<typename T, bool littleendian>
	int bit_stbinarynumber(lua::state& L, lua::parameters& P)
	{
		T val;
		char buffer[sizeof(T)];

		P(val);

		serialization::write_endian<T>(buffer, val, littleendian ? -1 : 1);
		L.pushlstring(buffer, sizeof(T));
		return 1;
	}

	int bit_quotent(lua::state& L, lua::parameters& P)
	{
		int64_t a, b;

		P(a, b);

		L.pushnumber(a / b);
		return 1;
	}

	int bit_multidiv(lua::state& L, lua::parameters& P)
	{
		double v;
		unsigned values = L.gettop();

		P(v);

		for(unsigned i = 1; i < values; i++) {
			int64_t q;
			P(q);
			int64_t qx = v / q;
			L.pushnumber(qx);
			v -= qx * q;
		}
		L.pushnumber(v);
		return values;
	};

	lua::functions bitops(lua_func_bit, "bit", {
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
		{"test", bit_test2<false>},
		{"testn", bit_test2<true>},
		{"popcount", bit_popcount},
		{"clshift", bit_cshift<false>},
		{"crshift", bit_cshift<true>},
		{"compose", bit_compose},
		{"quotent", bit_quotent},
		{"multidiv", bit_multidiv},
		{"binary_ld_u8be", bit_ldbinarynumber<uint8_t, false>},
		{"binary_ld_s8be", bit_ldbinarynumber<int8_t, false>},
		{"binary_ld_u16be", bit_ldbinarynumber<uint16_t, false>},
		{"binary_ld_s16be", bit_ldbinarynumber<int16_t, false>},
		{"binary_ld_u24be", bit_ldbinarynumber<ss_uint24_t, false>},
		{"binary_ld_s24be", bit_ldbinarynumber<ss_int24_t, false>},
		{"binary_ld_u32be", bit_ldbinarynumber<uint32_t, false>},
		{"binary_ld_s32be", bit_ldbinarynumber<int32_t, false>},
		{"binary_ld_u64be", bit_ldbinarynumber<uint64_t, false>},
		{"binary_ld_s64be", bit_ldbinarynumber<int64_t, false>},
		{"binary_ld_floatbe", bit_ldbinarynumber<float, false>},
		{"binary_ld_doublebe", bit_ldbinarynumber<double, false>},
		{"binary_ld_u8le", bit_ldbinarynumber<uint8_t, true>},
		{"binary_ld_s8le", bit_ldbinarynumber<int8_t, true>},
		{"binary_ld_u16le", bit_ldbinarynumber<uint16_t, true>},
		{"binary_ld_s16le", bit_ldbinarynumber<int16_t, true>},
		{"binary_ld_u24le", bit_ldbinarynumber<ss_uint24_t, true>},
		{"binary_ld_s24le", bit_ldbinarynumber<ss_int24_t, true>},
		{"binary_ld_u32le", bit_ldbinarynumber<uint32_t, true>},
		{"binary_ld_s32le", bit_ldbinarynumber<int32_t, true>},
		{"binary_ld_u64le", bit_ldbinarynumber<uint64_t, true>},
		{"binary_ld_s64le", bit_ldbinarynumber<int64_t, true>},
		{"binary_ld_floatle", bit_ldbinarynumber<float, true>},
		{"binary_ld_doublele", bit_ldbinarynumber<double, true>},
		{"binary_st_u8be", bit_stbinarynumber<uint8_t, false>},
		{"binary_st_s8be", bit_stbinarynumber<int8_t, false>},
		{"binary_st_u16be", bit_stbinarynumber<uint16_t, false>},
		{"binary_st_s16be", bit_stbinarynumber<int16_t, false>},
		{"binary_st_u24be", bit_stbinarynumber<ss_uint24_t, false>},
		{"binary_st_s24be", bit_stbinarynumber<ss_int24_t, false>},
		{"binary_st_u32be", bit_stbinarynumber<uint32_t, false>},
		{"binary_st_s32be", bit_stbinarynumber<int32_t, false>},
		{"binary_st_u64be", bit_stbinarynumber<uint64_t, false>},
		{"binary_st_s64be", bit_stbinarynumber<int64_t, false>},
		{"binary_st_floatbe", bit_stbinarynumber<float, false>},
		{"binary_st_doublebe", bit_stbinarynumber<double, false>},
		{"binary_st_u8le", bit_stbinarynumber<uint8_t, true>},
		{"binary_st_s8le", bit_stbinarynumber<int8_t, true>},
		{"binary_st_u16le", bit_stbinarynumber<uint16_t, true>},
		{"binary_st_s16le", bit_stbinarynumber<int16_t, true>},
		{"binary_st_u24le", bit_stbinarynumber<ss_uint24_t, true>},
		{"binary_st_s24le", bit_stbinarynumber<ss_int24_t, true>},
		{"binary_st_u32le", bit_stbinarynumber<uint32_t, true>},
		{"binary_st_s32le", bit_stbinarynumber<int32_t, true>},
		{"binary_st_u64le", bit_stbinarynumber<uint64_t, true>},
		{"binary_st_s64le", bit_stbinarynumber<int64_t, true>},
		{"binary_st_floatle", bit_stbinarynumber<float, true>},
		{"binary_st_doublele", bit_stbinarynumber<double, true>},
	});
}
