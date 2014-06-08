#include "lua/internal.hpp"
#include "interface/disassembler.hpp"
#include "interface/romtype.hpp"
#include "library/hex.hpp"
#include "library/memoryspace.hpp"
#include "core/instance.hpp"
#include "core/memorymanip.hpp"
#include "core/moviedata.hpp"
#include "core/rom.hpp"

namespace
{
	int disassemble(lua::state& L, lua::parameters& P)
	{
		std::string kind;
		uint64_t addr, count;

		P(kind, addr, P.optional(count, 1));

		disassembler* d;
		d = &disassembler::byname(kind);
		L.newtable();
		uint64_t laddr = addr;
		for(uint64_t i = 1; i <= count; i++) {
			uint64_t bytes = 0;
			L.pushnumber(i);
			L.newtable();
			L.pushstring("addr");
			L.pushnumber(laddr);
			L.settable(-3);

			L.pushstring("disasm");
			L.pushlstring(d->disassemble(laddr, [&bytes, laddr]() -> unsigned char {
				return CORE().memory->read<uint8_t>(laddr + bytes++);
			}));
			L.settable(-3);

			std::vector<unsigned char> tmp;
			tmp.resize(bytes);
			CORE().memory->read_range(laddr, &tmp[0], bytes);
			L.pushstring("bytes");
			L.pushlstring(hex::b_to(&tmp[0], bytes));
			L.settable(-3);
			L.settable(-3);
			laddr += bytes;
		}
		return 1;
	}

	int getregister(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		std::string r;

		P(r);

		const interface_device_reg* regs = core.rom->get_registers();
		if(!regs) {
			L.pushnil();
			return 1;
		}
		for(size_t i = 0; regs[i].name; i++) {
			if(r != regs[i].name)
				continue;
			if(regs[i].boolean)
				L.pushboolean(regs[i].read() != 0);
			else
				L.pushnumber(regs[i].read());
			return 1;
		}
		L.pushnil();
		return 1;
	}

	int getregisters(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		const interface_device_reg* regs = core.rom->get_registers();
		if(!regs) {
			L.pushnil();
			return 1;
		}
		L.newtable();
		for(size_t i = 0; regs[i].name; i++) {
			L.pushlstring(regs[i].name);
			if(regs[i].boolean)
				L.pushboolean(regs[i].read() != 0);
			else
				L.pushnumber(regs[i].read());
			L.settable(-3);
		}
		return 1;
	}

	int setregister(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		std::string r;

		P(r);

		const interface_device_reg* regs = core.rom->get_registers();
		if(!regs) {
			return 0;
		}
		for(size_t i = 0; regs[i].name; i++) {
			if(r != regs[i].name)
				continue;
			if(!regs[i].write)
				break;
			if(regs[i].boolean)
				regs[i].write(P.arg<bool>() ? 1 : 0);
			else
				regs[i].write(P.arg<uint64_t>());
			return 0;
		}
		return 0;
	}

	lua::functions LUA_disasm_fns(lua_func_misc, "memory", {
		{"disassemble", disassemble},
		{"getregister", getregister},
		{"getregisters", getregisters},
		{"setregister", setregister},
	});
}
