#include "lua/internal.hpp"
#include "library/minmax.hpp"

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
	class lua_symmetric_bitwise : public lua_function
	{
	public:
		lua_symmetric_bitwise(const std::string& s) : lua_function(lua_func_bit, s) {};
		int invoke(lua_state& L)
		{
			int stacksize = 0;
			while(!L.isnone(stacksize + 1))
				stacksize++;
			uint64_t ret = init;
			for(int i = 0; i < stacksize; i++)
				ret = combine(ret, L.get_numeric_argument<uint64_t>(i + 1, fname.c_str()));
			L.pushnumber(ret);
			return 1;
		}
	};

	template<uint64_t (*shift)(uint64_t base, uint64_t amount, uint64_t bits)>
	class lua_shifter : public lua_function
	{
	public:
		lua_shifter(const std::string& s) : lua_function(lua_func_bit, s) {};
		int invoke(lua_state& L)
		{
			uint64_t base;
			uint64_t amount = 1;
			uint64_t bits = BITWISE_BITS;
			base = L.get_numeric_argument<uint64_t>(1, fname.c_str());
			L.get_numeric_argument(2, amount, fname.c_str());
			L.get_numeric_argument(3, bits, fname.c_str());
			L.pushnumber(shift(base, amount, bits));
			return 1;
		}
	};

	function_ptr_luafun lua_bextract(lua_func_bit, "bit.extract", [](lua_state& L, const std::string& fname)
		-> int {
		uint64_t num = L.get_numeric_argument<uint64_t>(1, fname.c_str());
		uint64_t ret = 0;
		for(size_t i = 0;; i++) {
			if(L.isboolean(i + 2)) {
				if(L.toboolean(i + 2))
					ret |= (1ULL << i);
			} else if(L.isnumber(i + 2)) {
				uint8_t bit = L.get_numeric_argument<uint8_t>(i + 2, fname.c_str());
				ret |= (((num >> bit) & 1) << i);
			} else
				break;
		}
		L.pushnumber(ret);
		return 1;
	});

	function_ptr_luafun lua_bvalue(lua_func_bit, "bit.value", [](lua_state& L, const std::string& fname) -> int {
		uint64_t ret = 0;
		for(size_t i = 0;; i++) {
			if(L.isnumber(i + 1)) {
				uint8_t bit = L.get_numeric_argument<uint8_t>(i + 1, fname.c_str());
				ret |= (1ULL << bit);
			} else if(L.isnil(i + 1)) {
			} else
				break;
		}
		L.pushnumber(ret);
		return 1;
	});

	function_ptr_luafun lua_testany(lua_func_bit, "bit.test_any", [](lua_state& L, const std::string& fname)
		-> int {
		uint64_t a = L.get_numeric_argument<uint64_t>(1, fname.c_str());
		uint64_t b = L.get_numeric_argument<uint64_t>(2, fname.c_str());
		L.pushboolean((a & b) != 0);
		return 1;
	});

	function_ptr_luafun lua_testall(lua_func_bit, "bit.test_all", [](lua_state& L, const std::string& fname)
		-> int {
		uint64_t a = L.get_numeric_argument<uint64_t>(1, fname.c_str());
		uint64_t b = L.get_numeric_argument<uint64_t>(2, fname.c_str());
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

	function_ptr_luafun lua_popcount(lua_func_bit, "bit.popcount", [](lua_state& L, const std::string& fname)
		-> int {
		uint64_t a = L.get_numeric_argument<uint64_t>(1, fname.c_str());
		L.pushnumber(popcount(a));
		return 1;
	});

	function_ptr_luafun lua_clshift(lua_func_bit, "bit.clshift", [](lua_state& L, const std::string& fname)
		-> int {
		unsigned amount = 1;
		unsigned bits = 48;
		uint64_t a = L.get_numeric_argument<uint64_t>(1, fname.c_str());
		uint64_t b = L.get_numeric_argument<uint64_t>(2, fname.c_str());
		L.get_numeric_argument(3, amount, fname.c_str());
		L.get_numeric_argument(4, bits, fname.c_str());
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

	function_ptr_luafun lua_crshift(lua_func_bit, "bit.crshift", [](lua_state& L, const std::string& fname)
		-> int {
		unsigned amount = 1;
		unsigned bits = 48;
		uint64_t a = L.get_numeric_argument<uint64_t>(1, fname.c_str());
		uint64_t b = L.get_numeric_argument<uint64_t>(2, fname.c_str());
		L.get_numeric_argument(3, amount, fname.c_str());
		L.get_numeric_argument(4, bits, fname.c_str());
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

	int flagdecode_core(lua_state& L, const std::string& fname, bool reverse)
	{
		uint64_t a = L.get_numeric_argument<uint64_t>(1, fname.c_str());
		uint64_t b = L.get_numeric_argument<uint64_t>(2, fname.c_str());
		std::string on, off;
		if(L.type(3) == LUA_TSTRING)
			on = L.get_string(3, fname.c_str());
		if(L.type(4) == LUA_TSTRING)
			off = L.get_string(4, fname.c_str());
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

	function_ptr_luafun lua_flagdecode(lua_func_bit, "bit.flagdecode", [](lua_state& L, const std::string& fname)
		-> int {
		return flagdecode_core(L, fname, false);
	});

	function_ptr_luafun lua_rflagdecode(lua_func_bit, "bit.rflagdecode", [](lua_state& L,
		const std::string& fname) -> int {
		return flagdecode_core(L, fname, true);
	});

	lua_symmetric_bitwise<combine_none, BITWISE_MASK> bit_none("bit.none");
	lua_symmetric_bitwise<combine_none, BITWISE_MASK> bit_bnot("bit.bnot");
	lua_symmetric_bitwise<combine_any, 0> bit_any("bit.any");
	lua_symmetric_bitwise<combine_any, 0> bit_bor("bit.bor");
	lua_symmetric_bitwise<combine_all, BITWISE_MASK> bit_all("bit.all");
	lua_symmetric_bitwise<combine_all, BITWISE_MASK> bit_band("bit.band");
	lua_symmetric_bitwise<combine_parity, 0> bit_parity("bit.parity");
	lua_symmetric_bitwise<combine_parity, 0> bit_bxor("bit.bxor");
	lua_shifter<shift_lrotate> bit_lrotate("bit.lrotate");
	lua_shifter<shift_rrotate> bit_rrotate("bit.rrotate");
	lua_shifter<shift_lshift> bit_lshift("bit.lshift");
	lua_shifter<shift_arshift> bit_arshift("bit.arshift");
	lua_shifter<shift_lrshift> bit_lrshift("bit.lrshift");
}
