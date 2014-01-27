#include "lua/internal.hpp"
#include "core/memorymanip.hpp"
#include "core/memorywatch.hpp"
#include "core/moviedata.hpp"
#include "core/moviefile.hpp"
#include "core/rom.hpp"
#include "library/sha256.hpp"
#include "library/string.hpp"
#include "library/serialization.hpp"
#include "library/minmax.hpp"
#include "library/int24.hpp"

namespace
{
	int handle_push_vma(lua::state& L, memory_region& r)
	{
		L.newtable();
		L.pushstring("name");
		L.pushlstring(r.name.c_str(), r.name.size());
		L.settable(-3);
		L.pushstring("address");
		L.pushnumber(r.base);
		L.settable(-3);
		L.pushstring("size");
		L.pushnumber(r.size);
		L.settable(-3);
		L.pushstring("last");
		L.pushnumber(r.last_address());
		L.settable(-3);
		L.pushstring("readonly");
		L.pushboolean(r.readonly);
		L.settable(-3);
		L.pushstring("special");
		L.pushboolean(r.special);
		L.settable(-3);
		L.pushstring("endian");
		L.pushnumber(r.endian);
		L.settable(-3);
		return 1;
	}

	template<typename T> T bswap(T val)
	{
		T val2 = val;
		serialization::swap_endian(val2);
		return val2;
	}

	class lua_vma
	{
	public:
		lua_vma(lua::state& L, memory_region* r);
		int info(lua::state& L, const std::string& fname);
		template<class T, bool _bswap> int rw(lua::state& L, const std::string& fname);
		template<bool write, bool sign> int scattergather(lua::state& L, const std::string& fname);
		std::string print()
		{
			return vma;
		}
	private:
		std::string vma;
		uint64_t vmabase;
		uint64_t vmasize;
		bool ro;
	};

	class lua_vma_list
	{
	public:
		lua_vma_list(lua::state& L);
		static int create(lua::state& L, lua::parameters& P);
		int index(lua::state& L, const std::string& fname);
		int newindex(lua::state& L, const std::string& fname);
		int call(lua::state& L, const std::string& fname);
		std::string print()
		{
			return "";
		}
	};

	lua::_class<lua_vma> class_vma(lua_class_memory, "VMA", {}, {
			{"info", &lua_vma::info},
			{"read", &lua_vma::scattergather<false, false>},
			{"sread", &lua_vma::scattergather<false, true>},
			{"write", &lua_vma::scattergather<true, false>},
			{"sbyte", &lua_vma::rw<int8_t, false>},
			{"byte", &lua_vma::rw<uint8_t, false>},
			{"sword", &lua_vma::rw<int16_t, false>},
			{"word", &lua_vma::rw<uint16_t, false>},
			{"shword", &lua_vma::rw<ss_int24_t, false>},
			{"hword", &lua_vma::rw<ss_uint24_t, false>},
			{"sdword", &lua_vma::rw<int32_t, false>},
			{"dword", &lua_vma::rw<uint32_t, false>},
			{"sqword", &lua_vma::rw<int64_t, false>},
			{"qword", &lua_vma::rw<uint64_t, false>},
			{"float", &lua_vma::rw<float, false>},
			{"double", &lua_vma::rw<double, false>},
			{"isbyte", &lua_vma::rw<int8_t, true>},
			{"ibyte", &lua_vma::rw<uint8_t, true>},
			{"isword", &lua_vma::rw<int16_t, true>},
			{"iword", &lua_vma::rw<uint16_t, true>},
			{"ishword", &lua_vma::rw<ss_int24_t, true>},
			{"ihword", &lua_vma::rw<ss_uint24_t, true>},
			{"isdword", &lua_vma::rw<int32_t, true>},
			{"idword", &lua_vma::rw<uint32_t, true>},
			{"isqword", &lua_vma::rw<int64_t, true>},
			{"iqword", &lua_vma::rw<uint64_t, true>},
			{"ifloat", &lua_vma::rw<float, true>},
			{"idouble", &lua_vma::rw<double, true>},
	});
	lua::_class<lua_vma_list> class_vmalist(lua_class_memory, "VMALIST", {
		{"new", lua_vma_list::create},
	}, {
		{"__index", &lua_vma_list::index},
		{"__newindex", &lua_vma_list::newindex},
		{"__call", &lua_vma_list::call},
	});

	lua_vma::lua_vma(lua::state& L, memory_region* r)
	{
		vmabase = r->base;
		vmasize = r->size;
		vma = r->name;
		ro = r->readonly;
	}

	int lua_vma::info(lua::state& L, const std::string& fname)
	{
		lua::parameters P(L, fname);

		for(auto i : lsnes_memory.get_regions())
			if(i->name == vma)
				return handle_push_vma(L, *i);
		(stringfmt() << P.get_fname() << ": Stale region").throwex();
	}

	template<class T, bool _bswap> int lua_vma::rw(lua::state& L, const std::string& fname)
	{
		lua::parameters P(L, fname);
		uint64_t addr;
		T val;

		P(P.skipped(), addr);

		if(addr > vmasize || addr > vmasize - sizeof(T))
			throw std::runtime_error("VMA::rw<T>: Address outside VMA bounds");
		if(P.is_novalue()) {
			//Read.
			T val = lsnes_memory.read<T>(addr + vmabase);
			if(_bswap) val = bswap(val);
			L.pushnumber(val);
			return 1;
		} else if(P.is_number()) {
			//Write.
			if(ro)
				(stringfmt() << P.get_fname() << ": VMA is read-only").throwex();
			P(val);
			if(_bswap) val = bswap(val);
			lsnes_memory.write<T>(addr + vmabase, val);
			return 0;
		} else
			P.expected("number or nil");
	}

	template<bool write, bool sign> int lua_vma::scattergather(lua::state& L, const std::string& fname)
	{
		lua::parameters P(L, fname);
		uint64_t val = 0;
		unsigned shift = 0;
		uint64_t addr = 0;

		P(P.skipped());
		if(write)
			P(val);

		while(!P.is_novalue()) {
			if(P.is_boolean()) {
				if(P.arg<bool>())
					addr++;
				else
					addr--;
			} else
				addr = P.arg<uint64_t>();
			if(write)
				lsnes_memory.write<uint8_t>(addr + vmabase, val >> shift);
			else
				val = val + ((uint64_t)lsnes_memory.read<uint8_t>(addr + vmabase) << shift);
			shift += 8;
		}
		if(!write) {
			int64_t sval = val;
			if(val >= (1ULL << (shift - 1))) sval -= (1ULL << shift);
			if(sign) L.pushnumber(sval); else L.pushnumber(val);
		}
		return write ? 0 : 1;
	}

	lua_vma_list::lua_vma_list(lua::state& L)
	{
	}

	int lua_vma_list::create(lua::state& L, lua::parameters& P)
	{
		lua::_class<lua_vma_list>::create(L);
		return 1;
	}

	int lua_vma_list::call(lua::state& L, const std::string& fname)
	{
		L.newtable();
		size_t key = 1;
		for(auto i : lsnes_memory.get_regions()) {
			L.pushnumber(key++);
			L.pushlstring(i->name);
			L.rawset(-3);
		}
		return 1;
	}

	int lua_vma_list::index(lua::state& L, const std::string& fname)
	{
		lua::parameters P(L, fname);
		std::string vma;

		P(P.skipped(), vma);

		auto l = lsnes_memory.get_regions();
		size_t j;
		std::list<memory_region*>::iterator i;
		for(i = l.begin(), j = 0; i != l.end(); i++, j++)
			if((*i)->name == vma) {
				lua::_class<lua_vma>::create(L, *i);
				return 1;
			}
		(stringfmt() << P.get_fname() << ": No such VMA").throwex();
	}

	int lua_vma_list::newindex(lua::state& L, const std::string& fname)
	{
		throw std::runtime_error("Writing is not allowed");
	}
}
