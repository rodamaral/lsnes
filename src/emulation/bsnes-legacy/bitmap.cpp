#include "lua/bitmap.hpp"
#include "lua/internal.hpp"
#include "core/memorymanip.hpp"

namespace
{
	uint64_t get_vmabase(lua::state& L, const std::string& vma)
	{
		for(auto i : lsnes_memory.get_regions())
			if(i->name == vma)
				return i->base;
		throw std::runtime_error("No such VMA");
	}

	lua::fnptr2 dump_memory_bitmap(lua_func_misc, "bsnes.dump_sprite", [](lua::state& L, lua::parameters& P)
		-> int {
		std::string vma;
		uint64_t addr;
		uint32_t width, height;
		uint64_t vmabase = 0;
		size_t stride1 = 32;
		size_t stride2;

		if(P.is_string()) {
			P(vma);
			vmabase = get_vmabase(L, vma);
		}
		P(addr, width, height, P.optional(stride2, 512));
		addr += vmabase;

		lua_bitmap* b = lua::_class<lua_bitmap>::create(L, width * 8, height * 8);
		for(unsigned j = 0; j < height; j++)
			for(unsigned i = 0; i < width; i++) {
				uint64_t sbase = addr + stride2 * j + stride1 * i;
				for(unsigned k = 0; k < 8; k++) {
					uint8_t byte1 = lsnes_memory.read<uint8_t>(sbase + 2 * k);
					uint8_t byte2 = lsnes_memory.read<uint8_t>(sbase + 2 * k + 1);
					uint8_t byte3 = lsnes_memory.read<uint8_t>(sbase + 2 * k + 16);
					uint8_t byte4 = lsnes_memory.read<uint8_t>(sbase + 2 * k + 17);
					uint32_t soff = (j * 8 + k) * (8 * width) + i * 8;
					for(unsigned l = 0; l < 8; l++) {
						uint32_t v = 0;
						if((byte1 >> (7 - l)) & 1) v |= 1;
						if((byte2 >> (7 - l)) & 1) v |= 2;
						if((byte3 >> (7 - l)) & 1) v |= 4;
						if((byte4 >> (7 - l)) & 1) v |= 8;
						b->pixels[soff + l] = v;
					}
				}
			}
		return 1;
	});

	lua::fnptr2 dump_memory_palette(lua_func_misc, "bsnes.dump_palette", [](lua::state& L, lua::parameters& P)
		-> int {
		std::string vma;
		uint64_t addr;
		bool full, ftrans;
		uint64_t vmabase = 0;

		if(P.is_string()) {
			P(vma);
			vmabase = get_vmabase(L, vma);
		}
		P(addr, full, ftrans);
		addr += vmabase;

		size_t ps = full ? 256 : 16;
		lua_palette* p = lua::_class<lua_palette>::create(L);
		for(unsigned j = 0; j < ps; j++) {
			if(j == 0 && ftrans)
				p->colors.push_back(framebuffer::color(-1));
			else {
				uint64_t val = 0;
				uint16_t c = lsnes_memory.read<uint16_t>(addr + j * 2);
				uint64_t r = (c >> 0) & 0x1F;
				uint64_t g = (c >> 5) & 0x1F;
				uint64_t b = (c >> 10) & 0x1F;
				val = (r << 19) | ((r << 14) & 0xFF0000) | (g << 11) | ((g << 6) & 0xFF00) |
					(b << 3) | (b >> 2);
				p->colors.push_back(framebuffer::color(val));
			}
		}
		return 1;
	});

}