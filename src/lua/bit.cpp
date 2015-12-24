#include "lua/internal.hpp"
#include "library/minmax.hpp"
#include "library/int24.hpp"
#include "library/serialization.hpp"

namespace
{
	template<unsigned nbits>
	uint64_t maskn()
	{
		if(nbits > 63)
			return 0xFFFFFFFFFFFFFFFFULL;
		else
			return (1ULL << (nbits&63)) - 1;
	}

	uint64_t maskx(unsigned nbits)
	{
		if(nbits > 63)
			return 0xFFFFFFFFFFFFFFFFULL;
		else
			return (1ULL << (nbits&63)) - 1;
	}

	template<unsigned nbits>
	uint64_t combine_none(uint64_t chain, uint64_t arg)
	{
		return (chain & ~arg) & maskn<nbits>();
	}

	template<unsigned nbits>
	uint64_t combine_any(uint64_t chain, uint64_t arg)
	{
		return (chain | arg) & maskn<nbits>();
	}

	template<unsigned nbits>
	uint64_t combine_all(uint64_t chain, uint64_t arg)
	{
		return (chain & arg) & maskn<nbits>();
	}

	template<unsigned nbits>
	uint64_t combine_parity(uint64_t chain, uint64_t arg)
	{
		return (chain ^ arg) & maskn<nbits>();
	}

	template<unsigned nbits>
	uint64_t shift_lrotate(uint64_t base, uint64_t amount, uint64_t bits)
	{
		uint64_t mask = maskx(bits);
		base &= mask;
		base = (base << amount) | (base >> (bits - amount));
		return base & mask & maskn<nbits>();
	}

	template<unsigned nbits>
	uint64_t shift_rrotate(uint64_t base, uint64_t amount, uint64_t bits)
	{
		uint64_t mask = maskx(bits);
		base &= mask;
		base = (base >> amount) | (base << (bits - amount));
		return base & mask & maskn<nbits>();
	}

	template<unsigned nbits>
	uint64_t shift_lshift(uint64_t base, uint64_t amount, uint64_t bits)
	{
		uint64_t mask = maskx(bits);
		base <<= amount;
		return base & mask & maskn<nbits>();
	}

	template<unsigned nbits>
	uint64_t shift_lrshift(uint64_t base, uint64_t amount, uint64_t bits)
	{
		uint64_t mask = maskx(bits);
		base &= mask;
		base >>= amount;
		return base & maskn<nbits>();
	}

	template<unsigned nbits>
	uint64_t shift_arshift(uint64_t base, uint64_t amount, uint64_t bits)
	{
		uint64_t mask = maskx(bits);
		base &= mask;
		bool negative = ((base >> (bits - 1)) != 0);
		base >>= amount;
		base |= ((negative ? maskn<nbits>() : 0) << (bits - amount));
		return base & mask & maskn<nbits>();
	}

	uint64_t zero()
	{
		return 0;
	}

	template<unsigned nbits>
	uint64_t ones()
	{
		return maskn<nbits>();
	}

	template<uint64_t (*combine)(uint64_t chain, uint64_t arg), uint64_t (*init)()>
	int fold(lua::state& L, lua::parameters& P)
	{
		uint64_t ret = init();
		while(P.more())
			ret = combine(ret, P.arg<uint64_t>());
		L.pushnumber(ret);
		return 1;
	}

	template<uint64_t (*sh)(uint64_t base, uint64_t amount, uint64_t bits), unsigned nbits>
	int shift(lua::state& L, lua::parameters& P)
	{
		uint64_t base, amount, bits;

		P(base, P.optional(amount, 1), P.optional(bits, nbits));

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

	const int CONST_poptable[] = {0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4};

	int popcount(uint64_t x)
	{
		int c = 0;
		for(unsigned i = 0; i < 16; i++) {
			c += CONST_poptable[x & 15];
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

	template<bool right, unsigned nbits>
	int bit_cshift(lua::state& L, lua::parameters& P)
	{
		uint64_t a, b;
		unsigned amount, bits;

		P(a, b, P.optional(amount, 1), P.optional(bits, nbits));

		uint64_t mask = maskx(bits);
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

		auto on32 = utf8::to32(on);
		auto off32 = utf8::to32(off);

		size_t onl = on32.length();
		size_t offl = off32.length();
		auto onc = onl ? on32[onl - 1] : '*';
		auto offc = offl ? off32[offl - 1] : '-';
		char32_t buffer[65];
		unsigned i;
		size_t bias = min(b, (uint64_t)64) - 1;
		for(i = 0; i < 64 && i < b; i++) {
			auto onc2 = (i < onl) ? on32[i] : onc;
			auto offc2 = (i < offl) ? off32[i] : offc;
			buffer[reverse ? (bias - i) : i] = ((a >> i) & 1) ? onc2 : offc2;
		}
		buffer[i] = '\0';
		L.pushlstring(utf8::to8(buffer));
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

	int bit_mul32(lua::state& L, lua::parameters& P)
	{
		uint32_t a, b;
		uint64_t c;

		P(a, b);

		c = (uint64_t)a * b;
		L.pushnumber(c & 0xFFFFFFFFU);
		L.pushnumber(c >> 32);
		return 2;
	}

	int bit_fextract(lua::state& L, lua::parameters& P)
	{
		uint64_t a, b, c;

		P(a, b, c);
		if(b > 63)
			L.pushnumber(0);
		else if(c > 63)
			L.pushnumber(a >> b);
		else
			L.pushnumber((a >> b) & ((1 << c) - 1));
		return 1;
	}

	int bit_bfields(lua::state& L, lua::parameters& P)
	{
		uint64_t v;
		unsigned values = L.gettop();

		P(v);
		for(unsigned i = 1; i < values; i++) {
			uint64_t q;
			P(q);
			if(q > 63) {
				L.pushnumber(v);
				v = 0;
			} else {
				L.pushnumber(v & ((1 << q) - 1));
				v >>= q;
			}
		}
		return values - 1;
	}

	lua::functions LUA_bitops_fn(lua_func_bit, "bit", {
		{"flagdecode", flagdecode_core<false>},
		{"rflagdecode", flagdecode_core<true>},
		{"none", fold<combine_none<48>, ones<48>>},
		{"any", fold<combine_any<48>, zero>},
		{"all", fold<combine_all<48>, ones<48>>},
		{"parity", fold<combine_parity<48>, zero>},
		{"bnot", fold<combine_none<48>, ones<48>>},
		{"bor", fold<combine_any<48>, zero>},
		{"band", fold<combine_all<48>, ones<48>>},
		{"bxor", fold<combine_parity<48>, zero>},
		{"lrotate", shift<shift_lrotate<48>, 48>},
		{"rrotate", shift<shift_rrotate<48>, 48>},
		{"lshift", shift<shift_lshift<48>, 48>},
		{"arshift", shift<shift_arshift<48>, 48>},
		{"lrshift", shift<shift_lrshift<48>, 48>},
#ifdef LUA_SUPPORTS_INTEGERS
		{"wnone", fold<combine_none<64>, ones<64>>},
		{"wany", fold<combine_any<64>, zero>},
		{"wall", fold<combine_all<64>, ones<64>>},
		{"wparity", fold<combine_parity<64>, zero>},
		{"wbnot", fold<combine_none<64>, ones<64>>},
		{"wbor", fold<combine_any<64>, zero>},
		{"wband", fold<combine_all<64>, ones<64>>},
		{"wbxor", fold<combine_parity<64>, zero>},
		{"wlrotate", shift<shift_lrotate<64>, 64>},
		{"wrrotate", shift<shift_rrotate<64>, 64>},
		{"wlshift", shift<shift_lshift<64>, 64>},
		{"warshift", shift<shift_arshift<64>, 64>},
		{"wlrshift", shift<shift_lrshift<64>, 64>},
		{"wclshift", bit_cshift<false, 64>},
		{"wcrshift", bit_cshift<true, 64>},
#endif
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
		{"clshift", bit_cshift<false, 48>},
		{"crshift", bit_cshift<true, 48>},
		{"compose", bit_compose},
		{"quotent", bit_quotent},
		{"multidiv", bit_multidiv},
		{"mul32", bit_mul32},
		{"fextract", bit_fextract},
		{"bfields", bit_bfields},
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
