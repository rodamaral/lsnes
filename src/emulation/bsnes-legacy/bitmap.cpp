#include "lua/bitmap.hpp"
#include "lua/internal.hpp"
#include "library/serialization.hpp"
#include "library/memoryspace.hpp"
#include "core/instance.hpp"
#include "core/memorymanip.hpp"

namespace
{
	template<typename T> struct unmapped_ptr
	{
		unmapped_ptr(memory_space* _mspace, uint64_t _base) { mspace = _mspace; base = _base; }
		unmapped_ptr operator+(uint64_t offset) { return unmapped_ptr(mspace, base + offset); }
		T operator*() { return mspace->read<T>(base); }
		T operator[](uint64_t offset) { return mspace->read<T>(base + offset); }
	private:
		memory_space* mspace;
		uint64_t base;
	};

	template<typename T, T (*decode)(const void*)> struct deserialized_ptr
	{
		deserialized_ptr(char* _ptr) { ptr = _ptr; }
		deserialized_ptr<T, decode> operator+(uint64_t offset) { return deserialized_ptr(ptr + offset); }
		T operator*() { return decode(ptr); }
		T operator[](uint64_t offset) { return decode(ptr + offset); }
	private:
		char* ptr;
	};

	template<unsigned bits, typename T> void dumpsprite_loop(T addr, uint32_t width, uint32_t height,
		size_t stride1, size_t stride2, lua_bitmap& b)
	{
		for(unsigned j = 0; j < height; j++)
			for(unsigned i = 0; i < width; i++) {
				T sbase = addr + stride2 * j + stride1 * i;
				for(unsigned k = 0; k < 8; k++) {
					uint8_t arr[bits];
					for(unsigned l = 0; l < bits; l++)
						arr[l] = sbase[2 * k + 16 * (l >> 1) + (l & 1)];
					uint32_t soff = (j * 8 + k) * (8 * width) + i * 8;
					for(unsigned l = 0; l < 8; l++) {
						uint32_t v = 0;
						for(unsigned m = 0; m < bits; m++)
							if((arr[m] >> (7 - l)) & 1) v |= (1 << m);
						b.pixels[soff + l] = v;
					}
				}
			}
	}

	template<bool create, unsigned bits>
	int dump_sprite(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		lua_bitmap* b;
		uint64_t addr;
		uint32_t width, height;
		size_t stride1 = 8 * bits;
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
		P(P.optional(stride2, 128 * bits));

		if(create)
			b = lua::_class<lua_bitmap>::create(L, width * 8, height * 8);

		bool map = false;
		uint64_t rangebase = 0;
		uint64_t rangesize = 0;
		if(stride1 < (1 << 30) / (width ? width : 1) && stride2 < (1 << 30) / (height ? height : 1)) {
			map = true;
			rangebase = addr;
			rangesize = (width - 1) * stride1 + (height - 1) * stride2 + 32;
		}

		char* mem = map ? core.memory->get_physical_mapping(rangebase, rangesize) : NULL;
		if(mem)
			dumpsprite_loop<bits>(mem, width, height, stride1, stride2, *b);
		else
			dumpsprite_loop<bits>(unmapped_ptr<uint8_t>(core.memory, addr), width, height, stride1,
				stride2, *b);
		return create ? 1 : 0;
	}

	template<typename T> void dump_palette_loop(T addr, size_t ps, bool ftrans, lua_palette& p)
	{
		for(unsigned j = 0; j < ps; j++) {
			if(j == 0 && ftrans)
				p.colors[j] = framebuffer::color(-1);
			else {
				uint64_t val = 0;
				uint16_t c = addr[j * 2];
				uint64_t r = (c >> 0) & 0x1F;
				uint64_t g = (c >> 5) & 0x1F;
				uint64_t b = (c >> 10) & 0x1F;
				val = (r << 19) | ((r << 14) & 0xFF0000) | (g << 11) | ((g << 6) & 0xFF00) |
					(b << 3) | (b >> 2);
				p.colors[j] = framebuffer::color(val);
			}
		}
	}

	template<bool create>
	int dump_palette(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		uint64_t addr;
		bool full, ftrans;
		lua_palette* p;

		if(!create) {
			P(p);
			size_t ccount = p->color_count;
			if(ccount != 16 && ccount != 256)
				throw std::runtime_error("Palette to read must be 16 or 256 colors");
			full = (ccount == 256);
		}
		addr = lua_get_read_address(P);
		if(create)
			P(full);
		P(ftrans);

		size_t ps = full ? 256 : 16;
		if(create) {
			p = lua::_class<lua_palette>::create(L);
			p->adjust_palette_size(ps);
		}
		char* mem = core.memory->get_physical_mapping(addr, 2 * ps);
		if(mem)
			dump_palette_loop(deserialized_ptr<uint16_t, serialization::u16l>(mem), ps, ftrans, *p);
		else
			dump_palette_loop(unmapped_ptr<uint16_t>(core.memory, addr), ps, ftrans, *p);
		return create ? 1 : 0;
	}

	lua::functions bitmap_fns_snes(lua_func_misc, "bsnes", {
		{"dump_palette", dump_palette<true>},
		{"redump_palette", dump_palette<false>},
		{"dump_sprite", dump_sprite<true, 4>},
		{"redump_sprite", dump_sprite<false, 4>},
		{"dump_sprite2", dump_sprite<true, 2>},
		{"redump_sprite2", dump_sprite<false, 2>},
		{"dump_sprite8", dump_sprite<true, 8>},
		{"redump_sprite8", dump_sprite<false, 8>},
	});
}
