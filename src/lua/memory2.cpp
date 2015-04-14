#include "lua/internal.hpp"
#include "lua/debug.hpp"
#include "core/memorymanip.hpp"
#include "core/memorywatch.hpp"
#include "core/instance.hpp"
#include "core/moviedata.hpp"
#include "core/moviefile.hpp"
#include "core/rom.hpp"
#include "library/sha256.hpp"
#include "library/skein.hpp"
#include "library/string.hpp"
#include "library/serialization.hpp"
#include "library/memoryspace.hpp"
#include "library/minmax.hpp"
#include "library/int24.hpp"

namespace
{
	int handle_push_vma(lua::state& L, memory_space::region& r)
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
		lua_vma(lua::state& L, memory_space::region* r);
		static size_t overcommit(memory_space::region* r) { return 0; }
		int info(lua::state& L, lua::parameters& P);
		template<class T, bool _bswap> int rw(lua::state& L, lua::parameters& P);
		template<bool write, bool sign> int scattergather(lua::state& L, lua::parameters& P);
		template<class T> int hash(lua::state& L, lua::parameters& P);
		template<bool cmp> int storecmp(lua::state& L, lua::parameters& P);
		int readregion(lua::state& L, lua::parameters& P);
		int writeregion(lua::state& L, lua::parameters& P);
		int cheat(lua::state& L, lua::parameters& P);
		template<debug_context::etype type, bool reg> int registerX(lua::state& L, lua::parameters& P);
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
		static size_t overcommit() { return 0; }
		static int create(lua::state& L, lua::parameters& P);
		int index(lua::state& L, lua::parameters& P);
		int newindex(lua::state& L, lua::parameters& P);
		int call(lua::state& L, lua::parameters& P);
		std::string print()
		{
			return "";
		}
	};

	struct l_sha256_h
	{
		static sha256 create()
		{
			return sha256();
		}
		static void write(sha256& h, void* b, size_t s)
		{
			h.write(reinterpret_cast<uint8_t*>(b), s);
		}
		static std::string read(sha256& h)
		{
			return h.read();
		}
	};

	struct l_skein_h
	{
		static skein::hash create()
		{
			return skein::hash(skein::hash::PIPE_512, 256);
		}
		static void write(skein::hash& h, void* b, size_t s)
		{
			h.write(reinterpret_cast<uint8_t*>(b), s);
		}
		static std::string read(skein::hash& h)
		{
			uint8_t buf[32];
			h.read(buf);
			return hex::b_to(buf, 32);
		}
	};

	lua::_class<lua_vma> LUA_class_vma(lua_class_memory, "VMA", {}, {
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
			{"cheat", &lua_vma::cheat},
			{"sha256", &lua_vma::hash<l_sha256_h>},
			{"skein", &lua_vma::hash<l_skein_h>},
			{"store", &lua_vma::storecmp<false>},
			{"storecmp", &lua_vma::storecmp<true>},
			{"readregion", &lua_vma::readregion},
			{"writeregion", &lua_vma::writeregion},
			{"registerread", &lua_vma::registerX<debug_context::DEBUG_READ, true>},
			{"unregisterread", &lua_vma::registerX<debug_context::DEBUG_READ, false>},
			{"registerwrite", &lua_vma::registerX<debug_context::DEBUG_WRITE, true>},
			{"unregisterwrite", &lua_vma::registerX<debug_context::DEBUG_WRITE, false>},
			{"registerexec", &lua_vma::registerX<debug_context::DEBUG_EXEC, true>},
			{"unregisterexec", &lua_vma::registerX<debug_context::DEBUG_EXEC, false>},
	}, &lua_vma::print);

	lua::_class<lua_vma_list> LUA_class_vmalist(lua_class_memory, "VMALIST", {
		{"new", lua_vma_list::create},
	}, {
		{"__index", &lua_vma_list::index},
		{"__newindex", &lua_vma_list::newindex},
		{"__call", &lua_vma_list::call},
	});

	lua_vma::lua_vma(lua::state& L, memory_space::region* r)
	{
		vmabase = r->base;
		vmasize = r->size;
		vma = r->name;
		ro = r->readonly;
	}

	int lua_vma::info(lua::state& L, lua::parameters& P)
	{
		for(auto i : CORE().memory->get_regions())
			if(i->name == vma)
				return handle_push_vma(L, *i);
		(stringfmt() << P.get_fname() << ": Stale region").throwex();
		return 0; //NOTREACHED
	}

	template<class T, bool _bswap> int lua_vma::rw(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		uint64_t addr;
		T val;

		P(P.skipped(), addr);

		if(addr > vmasize || addr > vmasize - sizeof(T))
			throw std::runtime_error("VMA::rw<T>: Address outside VMA bounds");
		if(P.is_novalue()) {
			//Read.
			T val = core.memory->read<T>(addr + vmabase);
			if(_bswap) val = bswap(val);
			L.pushnumber(val);
			return 1;
		} else if(P.is_number()) {
			//Write.
			if(ro)
				(stringfmt() << P.get_fname() << ": VMA is read-only").throwex();
			P(val);
			if(_bswap) val = bswap(val);
			core.memory->write<T>(addr + vmabase, val);
			return 0;
		} else
			P.expected("number or nil");
		return 0; //NOTREACHED
	}

	template<bool write, bool sign> int lua_vma::scattergather(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
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
				core.memory->write<uint8_t>(addr + vmabase, val >> shift);
			else
				val = val + ((uint64_t)core.memory->read<uint8_t>(addr + vmabase) << shift);
			shift += 8;
		}
		if(!write) {
			int64_t sval = val;
			if(val >= (1ULL << (shift - 1))) sval -= (1ULL << shift);
			if(sign) L.pushnumber(sval); else L.pushnumber(val);
		}
		return write ? 0 : 1;
	}

	template<class T> int lua_vma::hash(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		uint64_t addr, size, rows, stride = 0;

		P(P.skipped(), addr, size, P.optional(rows, 1));
		if(rows > 1) P(stride);

		//First verify that all reads are to the region.
		if(!memoryspace_row_limited(addr, size, rows, stride, vmasize))
			throw std::runtime_error("Region out of range");

		auto hstate = T::create();
		//Try to map the VMA.
		char* vmabuf = core.memory->get_physical_mapping(vmabase, vmasize);
		if(vmabuf) {
			for(uint64_t i = 0; i < rows; i++) {
				T::write(hstate, vmabuf + addr, size);
				addr += stride;
			}
		} else {
			uint8_t buf[512];	//Must be power of 2.
			unsigned bf = 0;
			for(uint64_t i = 0; i < rows; i++) {
				for(uint64_t j = 0; j < size; j++) {
					buf[bf] = core.memory->read<uint8_t>(vmabase + addr + j);
					bf = (bf + 1) & (sizeof(buf) - 1);
					if(!bf)
						T::write(hstate, buf, sizeof(buf));
				}
				addr += stride;
			}
			if(bf)
				T::write(hstate, buf, bf);
		}
		L.pushlstring(T::read(hstate));
		return 1;
	}

	int lua_vma::readregion(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		uint64_t addr, size;

		P(P.skipped(), addr, size);

		if(addr >= vmasize || size > vmasize || addr + size > vmasize)
			throw std::runtime_error("Read out of range");

		L.newtable();
		char* vmabuf = core.memory->get_physical_mapping(vmabase, vmasize);
		if(vmabuf) {
			uint64_t ctr = 1;
			for(size_t i = 0; i < size; i++) {
				L.pushnumber(ctr++);
				L.pushnumber(static_cast<unsigned char>(vmabuf[addr + i]));
				L.settable(-3);
			}
		} else {
			uint64_t ctr = 1;
			for(size_t i = 0; i < size; i++) {
				L.pushnumber(ctr++);
				L.pushnumber(core.memory->read<uint8_t>(addr + i));
				L.settable(-3);
			}
		}
		return 1;
	}

	int lua_vma::writeregion(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		uint64_t addr;
		int ltbl;

		P(P.skipped(), addr, P.table(ltbl));

		auto g = core.memory->lookup(vmabase);
		if(!g.first || g.first->readonly)
			throw std::runtime_error("Memory address is read-only");
		if(addr >= vmasize)
			throw std::runtime_error("Write out of range");

		uint64_t ctr = 1;
		char* vmabuf = core.memory->get_physical_mapping(vmabase, vmasize);
		if(vmabuf) {
			for(size_t i = 0;; i++) {
				L.pushnumber(ctr++);
				L.gettable(ltbl);
				if(L.type(-1) == LUA_TNIL)
					break;
				if(addr + i >= vmasize)
					throw std::runtime_error("Write out of range");
				vmabuf[addr + i] = L.tointeger(-1);
				L.pop(1);
			}
		} else {
			for(size_t i = 0;; i++) {
				L.pushnumber(ctr++);
				L.gettable(ltbl);
				if(L.type(-1) == LUA_TNIL)
					break;
				if(addr + i >= vmasize)
					throw std::runtime_error("Write out of range");
				core.memory->write<uint8_t>(vmabase + addr + i, L.tointeger(-1));
				L.pop(1);
			}
		}
		return 0;
	}

	template<bool cmp> int lua_vma::storecmp(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		uint64_t addr, daddr, size, rows, stride = 0;
		bool equals = true;

		P(P.skipped(), addr, daddr, size, P.optional(rows, 1));
		if(rows > 1) P(stride);

		//First verify that all reads are to the region.
		if(!memoryspace_row_limited(addr, size, rows, stride, vmasize))
			throw std::runtime_error("Source out of range");

		//Calculate new size of target.
		auto& h = core.mlogic->get_mfile().dyn.host_memory;
		size_t rsize = size * rows;
		if(size && rsize / size != rows)
			throw std::runtime_error("Copy size out of range");
		if((size_t)daddr + rsize < rsize)
			throw std::runtime_error("Target out of range");
		if(daddr + rsize > h.size()) {
			h.resize(daddr + rsize);
			equals = false;
		}

		//Try to map the VMA.
		char* vmabuf = core.memory->get_physical_mapping(vmabase, vmasize);
		if(vmabuf) {
			for(uint64_t i = 0; i < rows; i++) {
				bool eq = (cmp && !memcmp(&h[daddr], vmabuf + addr, size));
				if(!eq)
					memcpy(&h[daddr], vmabuf + addr, size);
				equals &= eq;
				addr += stride;
				daddr += size;
			}
		} else {
			for(uint64_t i = 0; i < rows; i++) {
				for(uint64_t j = 0; j < size; j++) {
					uint8_t byte = core.memory->read<uint8_t>(vmabase + addr + j);
					bool eq = (cmp && ((uint8_t)h[daddr + j] == byte));
					h[daddr + j] = byte;
					equals &= eq;
				}
				addr += stride;
				daddr += size;
			}
		}
		if(cmp) L.pushboolean(equals);
		return cmp ? 1 : 0;
	}

	template<debug_context::etype type, bool reg> int lua_vma::registerX(lua::state& L, lua::parameters& P)
	{
		uint64_t addr;
		int lfn;
		P(P.skipped(), addr, P.function(lfn));

		if(reg) {
			handle_registerX<type>(L, vmabase + addr, lfn);
			L.pushvalue(lfn);
			return 1;
		} else {
			handle_unregisterX<type>(L, vmabase + addr, lfn);
			return 0;
		}
	}

	int lua_vma::cheat(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		uint64_t addr, value;

		P(P.skipped(), addr);
		if(addr >= vmasize)
			throw std::runtime_error("Address out of range");
		if(P.is_novalue()) {
			core.dbg->clear_cheat(vmabase + addr);
		} else {
			P(value);
			core.dbg->set_cheat(vmabase + addr, value);
		}
		return 0;
	}

	lua_vma_list::lua_vma_list(lua::state& L)
	{
	}

	int lua_vma_list::create(lua::state& L, lua::parameters& P)
	{
		lua::_class<lua_vma_list>::create(L);
		return 1;
	}

	int lua_vma_list::call(lua::state& L, lua::parameters& P)
	{
		L.newtable();
		size_t key = 1;
		for(auto i : CORE().memory->get_regions()) {
			L.pushnumber(key++);
			L.pushlstring(i->name);
			L.rawset(-3);
		}
		return 1;
	}

	int lua_vma_list::index(lua::state& L, lua::parameters& P)
	{
		std::string vma;

		P(P.skipped(), vma);

		auto l = CORE().memory->get_regions();
		size_t j;
		std::list<memory_space::region*>::iterator i;
		for(i = l.begin(), j = 0; i != l.end(); i++, j++)
			if((*i)->name == vma) {
				lua::_class<lua_vma>::create(L, *i);
				return 1;
			}
		(stringfmt() << P.get_fname() << ": No such VMA").throwex();
		return 0; //NOTREACHED
	}

	int lua_vma_list::newindex(lua::state& L, lua::parameters& P)
	{
		throw std::runtime_error("Writing is not allowed");
	}
}
