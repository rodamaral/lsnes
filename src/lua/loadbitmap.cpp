#include "lua/bitmap.hpp"
#include "library/zip.hpp"
#include <limits>

namespace
{
	inline uint64_t tocolor(unsigned r, unsigned g, unsigned b, unsigned a)
	{
		if(!a)
			return -1;
		else
			return (static_cast<int64_t>(256 - a) << 24) | (static_cast<int64_t>(r) << 16) |
				(static_cast<int64_t>(g) << 8) | static_cast<int64_t>(b);
	}
}

struct lua_loaded_bitmap lua_loaded_bitmap::load(std::istream& file)
{
	struct lua_loaded_bitmap b;
	std::string magic;
	unsigned pcolors;
	unsigned R;
	unsigned G;
	unsigned B;
	unsigned A;
	file >> magic;
	if(magic != "LSNES-BITMAP")
		throw std::runtime_error("Bitmap load: Wrong magic");
	file >> b.w;
	file >> b.h;
	if(b.h >= std::numeric_limits<size_t>::max() / b.w)
		throw std::runtime_error("Bitmap load: Bitmap too large");
	b.bitmap.resize(b.w * b.h);
	file >> pcolors;
	if(pcolors > 65536)
		throw std::runtime_error("Bitmap load: Palette too big");
	if(pcolors > 0) {
		//Paletted.
		b.d = false;
		b.palette.resize(pcolors);
		for(size_t i = 0; i < pcolors; i++) {
			file >> R;
			file >> G;
			file >> B;
			file >> A;
			if(R > 255 || G > 255 || B > 255 || A > 256)	//Yes, a can be 256.
				throw std::runtime_error("Bitmap load: Palette entry out of range");
			b.palette[i] = tocolor(R, G, B, A);
		}
		for(size_t i = 0; i < b.w * b.h; i++) {
			file >> R;
			if(R >= pcolors)
				throw std::runtime_error("Bitmap load: color index out of range");
			b.bitmap[i] = R;
		}
	} else {
		//Direct.
		b.d = true;
		for(size_t i = 0; i < b.w * b.h; i++) {
			file >> R;
			file >> G;
			file >> B;
			file >> A;
			if(R > 255 || G > 255 || B > 255 || A > 256)	//Yes, a can be 256.
				throw std::runtime_error("Bitmap load: Color out of range");
			b.bitmap[i] = tocolor(R, G, B, A);
		}
	}
	if(!file)
		throw std::runtime_error("Bitmap load: Error reading bitmap");
	return b;
}

struct lua_loaded_bitmap lua_loaded_bitmap::load(const std::string& name)
{
	struct lua_loaded_bitmap b;
	std::istream& file = zip::openrel(name, "");
	try {
		b = lua_loaded_bitmap::load(file);
		delete &file;
		return b;
	} catch(...) {
		delete &file;
		throw;
	}
}
