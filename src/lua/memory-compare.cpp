#include "lua/internal.hpp"
#include "core/instance.hpp"
#include "core/memorymanip.hpp"
#include "library/memoryspace.hpp"
#include "library/minmax.hpp"

namespace
{
	class compare_obj
	{
	public:
		compare_obj(lua::state& L, uint64_t addr, uint64_t size, uint64_t rows, uint64_t stride);
		static size_t overcommit(uint64_t addr, uint64_t size, uint64_t rows, uint64_t stride)
		{
			return lua::overcommit_std_align + (size_t)size * rows;
		}
		static int create(lua::state& L, lua::parameters& P);
		int call(lua::state& L, lua::parameters& P);
		std::string print()
		{
			std::ostringstream x;
			x << "addr=0x" << std::hex << addr << " rows=" << rows << " size=0x" << std::hex << size
				<< " stride=0x" << std::hex << stride;
			return x.str();
		}
	private:
		uint8_t* prev;
		bool try_map;
		uint64_t addr;
		uint64_t minaddr;
		uint64_t maxaddr;
		uint64_t rows;
		uint64_t size;
		uint64_t stride;
	};

	compare_obj::compare_obj(lua::state& L, uint64_t _addr, uint64_t _size, uint64_t _rows, uint64_t _stride)
	{
		if(!_size || !_rows) {
			//Empty.
			try_map = false;
			addr = 0;
			minaddr = 0;
			maxaddr = 0;
			rows = 0;
			size = 1;
			stride = 0;
			prev = NULL;
			return;
		} else {
			addr = _addr;
			size = _size;
			rows = _rows;
			stride = _stride;
			rpair(minaddr, maxaddr) = memoryspace_row_bounds(addr, size, rows, stride);
			try_map = (minaddr <= maxaddr && (maxaddr - minaddr + 1));
			if((((size_t)size * rows) + lua::overcommit_std_align) / rows < size)
				throw std::runtime_error("Size to monitor too large");
			prev = lua::align_overcommit<compare_obj, uint8_t>(this);
			memset(prev, 0, (size_t)size * rows);
		}
	}

	int compare_obj::create(lua::state& L, lua::parameters& P)
	{
		uint64_t addr, size;
		uint64_t stride = 0, rows = 1;

		addr = lua_get_read_address(P);
		P(size, P.optional(rows, 1));
		if(rows > 1)
			P(stride);

		compare_obj* o = lua::_class<compare_obj>::create(L, addr, size, rows, stride);
		o->call(L, P);
		L.pop(1);
		return 1;
	}

	int compare_obj::call(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		bool equals = true;
		char* pbuffer = try_map ? core.memory->get_physical_mapping(minaddr, maxaddr - minaddr + 1) :
			NULL;
		if(pbuffer) {
			//Mapable.
			uint64_t offset = addr - minaddr;
			for(uint64_t i = 0; i < rows; i++) {
				bool eq = !memcmp(&prev[i * size], pbuffer + offset, size);
				if(!eq)
					memcpy(&prev[i * size], pbuffer + offset, size);
				equals &= eq;
				offset += stride;
			}
		} else {
			//Not mapable.
			for(uint64_t i = 0; i < rows; i++) {
				uint64_t addr1 = addr + i * stride;
				uint64_t addr2 = i * size;
				for(uint64_t j = 0; j < size; j++) {
					uint8_t byte = core.memory->read<uint8_t>(addr1 + j);
					bool eq = prev[addr2 + j] == (char)byte;
					if(!eq)
						prev[addr2 + j] = byte;
					equals &= eq;
				}
			}
		}
		L.pushboolean(!equals);
		return 1;
	}

	lua::_class<compare_obj> LUA_class_vmalist(lua_class_memory, "COMPARE_OBJ", {
		{"new", compare_obj::create},
	}, {
		{"__call", &compare_obj::call},
	}, &compare_obj::print);
}
