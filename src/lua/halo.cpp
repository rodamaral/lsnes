#include "lua/halo.hpp"
#include "lua/bitmap.hpp"
#include "library/range.hpp"
#include "library/eatarg.hpp"
#include <cstdint>
#include <iostream>

namespace
{
	void render_halo1(unsigned char* prow, size_t width)
	{
		uint32_t x = prow[0];
		for(unsigned i = 0; i < width - 1; i++) {
			x = (x << 10) | prow[i + 1];
			prow[i] = (x >> 19) | (x >> 10) | (x << 1);
		}
		prow[width - 1] = (x >> 10) | (x << 1);
	}
	void render_halo2(unsigned char* prow, const unsigned char* srow, size_t width)
	{
		uint32_t x = prow[0];
		uint32_t y = srow[0];
		uint32_t z = 0;
		for(unsigned i = 0; i < width - 1; i++) {
			x = (x << 10) | prow[i + 1];
			y = (y << 10) | srow[i + 1];
			z = x | y;
			prow[i] = (z >> 19) | (x >> 10) | (z << 1) | (y >> 9);
		}
		prow[width - 1] = (x >> 10) | ((x | y) << 1) | (y >> 9);
	}
	void render_halo3(unsigned char* prow, const unsigned char* arow, const unsigned char* brow, size_t width)
	{
		uint32_t x = prow[0];
		uint32_t y = arow[0] | brow[0];
		uint32_t z = 0;
		for(unsigned i = 0; i < width - 1; i++) {
			x = (x << 10) | prow[i + 1];
			y = (y << 10) | arow[i + 1] | brow[i + 1];
			z = x | y;
			prow[i] = (z >> 19) | (x >> 10) | (z << 1) | (y >> 9);
		}
		prow[width - 1] = (x >> 10) | (z << 1) | (y >> 9);
	}
	void mask_halo(unsigned char* pixmap, size_t pixels)
	{
		for(size_t i = 0; i < pixels; i++)
			pixmap[i] &= 0x03;
	}

	template<bool T> class text_bitmap_holder
	{
	public:
		text_bitmap_holder(const framebuffer::color& bg, const framebuffer::color& fg,
			const framebuffer::color& hl, const unsigned char* _pixmap, size_t _width)
		{
			cmap[0] = bg;
			cmap[1] = fg;
			cmap[2] = hl;
			cmap[3] = fg;
			pixmap = _pixmap;
			width = _width;
		}
		size_t stride() { return width; }
		void lock() {}
		void unlock() {}
		void draw(size_t bmpidx, typename framebuffer::fb<T>::element_t& target)
		{
			cmap[pixmap[bmpidx]].apply(target);
		}
	private:
		framebuffer::color cmap[4];
		const unsigned char* pixmap;
		size_t width;
	};
}

void render_halo(unsigned char* pixmap, size_t width, size_t height)
{
	if(!height || !width) return;
	if(height == 1) {
		render_halo1(pixmap, width);
	} else {
		render_halo2(pixmap, pixmap + width, width);
		size_t off = width;
		for(size_t i = 1; i < height - 1; i++, off += width)
			render_halo3(pixmap + off, pixmap + off - width, pixmap + off + width, width);
		render_halo2(pixmap + off, pixmap + off - width, width);
	}
	mask_halo(pixmap, width * height);
}

template<bool X> void halo_blit(struct framebuffer::fb<X>& scr, unsigned char* pixmap, size_t width,
	size_t height, size_t owidth, size_t oheight, uint32_t x, uint32_t y, framebuffer::color& bg,
	framebuffer::color& fg, framebuffer::color& hl) throw()
{
	uint32_t low = 1;
	uint32_t border = 0;
	if(hl) {
		//Make space for halo.
		low = 0;
		border = 2;
		render_halo(pixmap, width, height);
	}
	range bX = (range::make_w(scr.get_width()) - x) & range::make_s(low, owidth + border);
	range bY = (range::make_w(scr.get_height()) - y) & range::make_s(low, oheight + border);
	//Because outside is always false, screen can be passed as 0x0.
	lua_bitmap_composite(scr, x, y, bX, bY, range::make_w(0), range::make_w(0), false,
		text_bitmap_holder<X>(bg, fg, hl, pixmap, width));
}

void _pull_fn_68269328963289632986296386936()
{
	eat_argument(halo_blit<false>);
	eat_argument(halo_blit<true>);
}
