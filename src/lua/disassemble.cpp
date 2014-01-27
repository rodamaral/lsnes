#include "lua/internal.hpp"
#include "interface/disassembler.hpp"
#include "interface/romtype.hpp"
#include "library/hex.hpp"
#include "core/memorymanip.hpp"
#include "core/moviedata.hpp"

namespace
{
	lua::fnptr2 memdisass(lua_func_misc, "memory.disassemble", [](lua::state& L, lua::parameters& P) -> int {
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
				return lsnes_memory.read<uint8_t>(laddr + bytes++);
			}));
			L.settable(-3);

			std::vector<unsigned char> tmp;
			tmp.resize(bytes);
			lsnes_memory.read_range(laddr, &tmp[0], bytes);
			L.pushstring("bytes");
			L.pushlstring(hex::b_to(&tmp[0], bytes));
			L.settable(-3);
			L.settable(-3);
			laddr += bytes;
		}
		return 1;
	});

	lua::fnptr2 getreg(lua_func_misc, "memory.getregister", [](lua::state& L, lua::parameters& P) -> int {
		std::string r;

		P(r);

		const interface_device_reg* regs = our_rom.rtype->get_registers();
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
	});

	lua::fnptr2 setreg(lua_func_misc, "memory.setregister", [](lua::state& L, lua::parameters& P) -> int {
		std::string r;

		P(r);

		const interface_device_reg* regs = our_rom.rtype->get_registers();
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
	});
}
