#include "lua/bitmap.hpp"
#include "lua/internal.hpp"
#include "library/serialization.hpp"
#include "library/memoryspace.hpp"
#include "core/instance.hpp"
#include "core/memorymanip.hpp"

namespace
{
	template<bool create, bool bpp2>
	int dump_sprite(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		lua_bitmap* b;
		uint64_t addr;
		uint32_t width, height;
		size_t stride1 = bpp2 ? 16 : 32;
		size_t stride2;

		if(!create) {
			P(b);
			width = b->width / 8;
			height = b->height / 8;
			if((width | height) & 8)
				throw std::runtime_error("The image size must be multiple of 8x8");
		}
		addr = lua_get_read_address(P);
		if(create)
			P(width, height);
		P(P.optional(stride2, bpp2 ? 256 : 512));

		if(create)
			b = lua::_class<lua_bitmap>::create(L, width * 8, height * 8);

		bool map = false;
		uint64_t rangebase;
		uint64_t rangesize;
		if(stride1 < (1 << 30) / (width ? width : 1) && stride2 < (1 << 30) / (height ? height : 1)) {
			map = true;
			rangebase = addr;
			rangesize = (width - 1) * stride1 + (height - 1) * stride2 + 32;
		}

		char* mem = map ? core.memory->get_physical_mapping(rangebase, rangesize) : NULL;
		if(mem) {
			for(unsigned j = 0; j < height; j++)
				for(unsigned i = 0; i < width; i++) {
					uint64_t sbase = addr + stride2 * j + stride1 * i - rangebase;
					for(unsigned k = 0; k < 8; k++) {
						uint8_t byte1 = mem[sbase + 2 * k];
						uint8_t byte2 = mem[sbase + 2 * k + 1];
						uint8_t byte3 = bpp2 ? 0 : mem[sbase + 2 * k + 16];
						uint8_t byte4 = bpp2 ? 0 : mem[sbase + 2 * k + 17];
						uint32_t soff = (j * 8 + k) * (8 * width) + i * 8;
						for(unsigned l = 0; l < 8; l++) {
							uint32_t v = 0;
							//No harm including the nonexistent planes (they are 0).
							if((byte1 >> (7 - l)) & 1) v |= 1;
							if((byte2 >> (7 - l)) & 1) v |= 2;
							if((byte3 >> (7 - l)) & 1) v |= 4;
							if((byte4 >> (7 - l)) & 1) v |= 8;
							b->pixels[soff + l] = v;
						}
					}
				}
		} else {
			for(unsigned j = 0; j < height; j++)
				for(unsigned i = 0; i < width; i++) {
					uint64_t sbase = addr + stride2 * j + stride1 * i;
					for(unsigned k = 0; k < 8; k++) {
						uint8_t byte1 = core.memory->read<uint8_t>(sbase + 2 * k);
						uint8_t byte2 = core.memory->read<uint8_t>(sbase + 2 * k + 1);
						uint8_t byte3 = bpp2 ? 0 : core.memory->read<uint8_t>(sbase + 2 * k +
							16);
						uint8_t byte4 = bpp2 ? 0 : core.memory->read<uint8_t>(sbase + 2 * k +
							17);
						uint32_t soff = (j * 8 + k) * (8 * width) + i * 8;
						for(unsigned l = 0; l < 8; l++) {
							//No harm including the nonexistent planes (they are 0).
							uint32_t v = 0;
							if((byte1 >> (7 - l)) & 1) v |= 1;
							if((byte2 >> (7 - l)) & 1) v |= 2;
							if((byte3 >> (7 - l)) & 1) v |= 4;
							if((byte4 >> (7 - l)) & 1) v |= 8;
							b->pixels[soff + l] = v;
						}
					}
				}
		}
		return create ? 1 : 0;
	}

	template<bool create>
	int dump_palette(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		uint64_t addr;
		bool full = false, ftrans;
		bool fourcc = false;
		lua_palette* p;

		if(!create) {
			P(p);
			size_t ccount = p->color_count;
			if(ccount != 4 && ccount != 16 && ccount != 256)
				throw std::runtime_error("Palette to read must be 4, 16 or 256 colors");
			full = (ccount == 256);
			fourcc = (ccount == 4);
		}
		addr = lua_get_read_address(P);
		if(create) {
			//Hacky way to do integers.
			if(P.is_number()) {
				uint64_t col;
				P(col);
				if(col == 4) fourcc = true;
				else if(col == 16);
				else if(col == 256) full = true;
				else
					throw std::runtime_error("Palette to read must be 4, 16 or 256 colors");
			} else {
				P(full);
			}
		}
		P(ftrans);

		size_t ps = full ? 256 : (fourcc ? 4 : 16);
		if(create) {
			p = lua::_class<lua_palette>::create(L);
			p->adjust_palette_size(ps);
		}
		uint8_t* mem = reinterpret_cast<uint8_t*>(core.memory->get_physical_mapping(addr, 2 * ps));
		if(mem) {
			for(unsigned j = 0; j < ps; j++) {
				if(j == 0 && ftrans)
					p->colors[j] = framebuffer::color(-1);
				else {
					uint64_t val = 0;
					uint16_t c = serialization::u16l(mem + 2 * j);
					uint64_t r = (c >> 0) & 0x1F;
					uint64_t g = (c >> 5) & 0x1F;
					uint64_t b = (c >> 10) & 0x1F;
					val = (r << 19) | ((r << 14) & 0xFF0000) | (g << 11) | ((g << 6) & 0xFF00) |
						(b << 3) | (b >> 2);
					p->colors[j] = framebuffer::color(val);
				}
			}
		} else {
			for(unsigned j = 0; j < ps; j++) {
				if(j == 0 && ftrans)
					p->colors[j] = framebuffer::color(-1);
				else {
					uint64_t val = 0;
					uint16_t c = core.memory->read<uint16_t>(addr + j * 2);
					uint64_t r = (c >> 0) & 0x1F;
					uint64_t g = (c >> 5) & 0x1F;
					uint64_t b = (c >> 10) & 0x1F;
					val = (r << 19) | ((r << 14) & 0xFF0000) | (g << 11) | ((g << 6) & 0xFF00) |
						(b << 3) | (b >> 2);
					p->colors[j] = framebuffer::color(val);
				}
			}
		}
		return create ? 1 : 0;
	}

	lua::functions bitmap_fns_snes(lua_func_misc, "bsnes", {
		{"dump_palette", dump_palette<true>},
		{"redump_palette", dump_palette<false>},
		{"dump_sprite", dump_sprite<true, false>},
		{"redump_sprite", dump_sprite<false, false>},
		{"dump_sprite2", dump_sprite<true, true>},
		{"redump_sprite2", dump_sprite<false, true>},
	});
}
