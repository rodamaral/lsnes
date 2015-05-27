#include "core/instance.hpp"
#include "core/messages.hpp"
#include "library/lua-base.hpp"
#include "library/lua-params.hpp"
#include "library/lua-class.hpp"
#include "library/memoryspace.hpp"
#include "library/serialization.hpp"
#include "library/string.hpp"
#include "lua/address.hpp"
#include "lua/internal.hpp"

namespace
{
	template<typename T> T bswap(T val)
	{
		T val2 = val;
		serialization::swap_endian(val2);
		return val2;
	}
}

uint64_t lua_get_vmabase(const std::string& vma)
{
	for(auto i : CORE().memory->get_regions())
		if(i->name == vma)
			return i->base;
	throw std::runtime_error("No such VMA");
}

uint64_t lua_get_read_address(lua::parameters& P)
{
	static std::map<std::string, char> deprecation_keys;
	char* deprecation = &deprecation_keys[P.get_fname()];
	uint64_t vmabase = 0;
	if(P.is<lua_address>()) {
		return P.arg<lua_address*>()->get();
	} else if(P.is_string())
		vmabase = lua_get_vmabase(P.arg<std::string>());
	else {
		//Deprecated.
		if(P.get_state().do_once(deprecation))
			messages << P.get_fname() << ": Global memory form is deprecated." << std::endl;
	}
	auto addr = P.arg<uint64_t>();
	return addr + vmabase;
}

lua_address::lua_address(lua::state& L)
{
}

int lua_address::create(lua::state& L, lua::parameters& P)
{
	lua_address* a = lua::_class<lua_address>::create(L);
	P(a->vma, a->addr);
	return 1;
}

std::string lua_address::print()
{
	return (stringfmt() << vma << "+0x" << std::hex << addr).str();
}

uint64_t lua_address::get()
{
	for(auto i : CORE().memory->get_regions())
		if(i->name == vma) {
			if(i->size > addr)
				return i->base + addr;
			(stringfmt() << "Address outside boound of memory area '" << vma << "' (" << std::hex
				<< addr << ">=" << std::hex << i->size << ")").throwex();
		}
	throw std::runtime_error("No such memory area '" + vma + "'");
}

std::string lua_address::get_vma()
{
	return vma;
}

uint64_t lua_address::get_offset()
{
	for(auto i : CORE().memory->get_regions())
		if(i->name == vma) {
			if(i->size > addr)
				return addr;
			(stringfmt() << "Address outside boound of memory area '" << vma << "' (" << std::hex
				<< addr << ">=" << std::hex << i->size << ")").throwex();
		}
	throw std::runtime_error("No such memory area '" + vma + "'");
}

int lua_address::l_get(lua::state& L, lua::parameters& P)
{
	L.pushnumber(get());
	return 1;
}

int lua_address::l_get_vma(lua::state& L, lua::parameters& P)
{
	L.pushlstring(get_vma());
	return 1;
}

int lua_address::l_get_offset(lua::state& L, lua::parameters& P)
{
	L.pushnumber(get_offset());
	return 1;
}

int lua_address::l_shift(lua::state& L, lua::parameters& P)
{
	int addrs = 1;
	int64_t a = 0, b = 0, c = 0;

	P(P.skipped(), a);
	if(P.more()) {
		addrs++;
		P(b);
	}
	if(P.more()) {
		addrs++;
		P(c);
	}

	lua_address* adr = lua::_class<lua_address>::create(L);
	adr->vma = vma;
	adr->addr = addr;
	switch(addrs) {
	case 1:
		adr->addr += a;
		break;
	case 2:
		adr->addr += a * b;
		break;
	case 3:
		adr->addr += a * b + c;
		break;
	}
	return 1;
}

int lua_address::l_replace(lua::state& L, lua::parameters& P)
{
	uint64_t newoffset;
	int bits;

	P(P.skipped(), newoffset, P.optional(bits, -1));

	lua_address* adr = lua::_class<lua_address>::create(L);
	adr->vma = vma;
	if(bits < 0)
		adr->addr = newoffset;
	else {
		uint64_t mask = ((1 << bits) - 1);
		adr->addr = (addr & ~mask) | (newoffset & mask);
	}
	return 1;
}

template<class T, bool _bswap> int lua_address::rw(lua::state& L, lua::parameters& P)
{
	auto& core = CORE();
	T val;

	P(P.skipped());

	if(P.is_novalue()) {
		//Read.
		T val = core.memory->read<T>(get());
		if(_bswap) val = bswap(val);
		L.pushnumber(val);
		return 1;
	} else if(P.is_number()) {
		//Write.
		P(val);
		if(_bswap) val = bswap(val);
		core.memory->write<T>(get(), val);
		return 0;
	} else
		P.expected("number or nil");
	return 0; //NOTREACHED
}

size_t lua_address::overcommit()
{
	return 0;
}

namespace
{
lua::_class<lua_address> LUA_class_address(lua_class_memory, "ADDRESS", {
	{"new", lua_address::create},
}, {
	{"addr", &lua_address::l_get},
	{"vma", &lua_address::l_get_vma},
	{"offset", &lua_address::l_get_offset},
	{"add", &lua_address::l_shift},
	{"replace", &lua_address::l_replace},
	{"sbyte", &lua_address::rw<int8_t, false>},
	{"byte", &lua_address::rw<uint8_t, false>},
	{"sword", &lua_address::rw<int16_t, false>},
	{"word", &lua_address::rw<uint16_t, false>},
	{"shword", &lua_address::rw<ss_int24_t, false>},
	{"hword", &lua_address::rw<ss_uint24_t, false>},
	{"sdword", &lua_address::rw<int32_t, false>},
	{"dword", &lua_address::rw<uint32_t, false>},
	{"sqword", &lua_address::rw<int64_t, false>},
	{"qword", &lua_address::rw<uint64_t, false>},
	{"float", &lua_address::rw<float, false>},
	{"double", &lua_address::rw<double, false>},
	{"isbyte", &lua_address::rw<int8_t, true>},
	{"ibyte", &lua_address::rw<uint8_t, true>},
	{"isword", &lua_address::rw<int16_t, true>},
	{"iword", &lua_address::rw<uint16_t, true>},
	{"ishword", &lua_address::rw<ss_int24_t, true>},
	{"ihword", &lua_address::rw<ss_uint24_t, true>},
	{"isdword", &lua_address::rw<int32_t, true>},
	{"idword", &lua_address::rw<uint32_t, true>},
	{"isqword", &lua_address::rw<int64_t, true>},
	{"iqword", &lua_address::rw<uint64_t, true>},
	{"ifloat", &lua_address::rw<float, true>},
	{"idouble", &lua_address::rw<double, true>},
}, &lua_address::print);
}
