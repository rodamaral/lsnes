#include "core/lua-int.hpp"

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
		lua_symmetric_bitwise(const std::string& s) : lua_function(s) {};
		int invoke(lua_State* L)
		{
			int stacksize = 0;
			while(!lua_isnone(L, stacksize + 1))
				stacksize++;
			uint64_t ret = init;
			for(int i = 0; i < stacksize; i++)
				ret = combine(ret, get_numeric_argument<uint64_t>(L, i + 1, fname.c_str()));
			lua_pushnumber(L, ret);
			return 1;
		}
	};

	template<uint64_t (*shift)(uint64_t base, uint64_t amount, uint64_t bits)>
	class lua_shifter : public lua_function
	{
	public:
		lua_shifter(const std::string& s) : lua_function(s) {};
		int invoke(lua_State* L)
		{
			uint64_t base;
			uint64_t amount = 1;
			uint64_t bits = BITWISE_BITS;
			base = get_numeric_argument<uint64_t>(L, 1, fname.c_str());
			get_numeric_argument(L, 2, amount, fname.c_str());
			get_numeric_argument(L, 3, bits, fname.c_str());
			lua_pushnumber(L, shift(base, amount, bits));
			return 1;
		}
	};

	function_ptr_luafun lua_bextract("bit.extract", [](lua_State* LS, const std::string& fname) -> int {
		uint64_t num = get_numeric_argument<uint64_t>(LS, 1, fname.c_str());
		uint64_t ret = 0;
		for(size_t i = 0;; i++) {
			if(lua_isnumber(LS, i + 2)) {
				uint8_t bit = get_numeric_argument<uint8_t>(LS, i + 2, fname.c_str());
				ret |= (((num >> bit) & 1) << i);
			} else if(lua_isboolean(LS, i + 2)) {
				if(lua_toboolean(LS, i + 2))
					ret |= (1ULL << i);
			} else
				break;
		}
		lua_pushnumber(LS, ret);
		return 1;
	});

	function_ptr_luafun lua_bvalue("bit.value", [](lua_State* LS, const std::string& fname) -> int {
		uint64_t ret = 0;
		for(size_t i = 0;; i++) {
			if(lua_isnumber(LS, i + 1)) {
				uint8_t bit = get_numeric_argument<uint8_t>(LS, i + 1, fname.c_str());
				ret |= (1ULL << bit);
			} else if(lua_isnil(LS, i + 1)) {
			} else
				break;
		}
		lua_pushnumber(LS, ret);
		return 1;
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