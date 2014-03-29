#include "level.hpp"
#include "util.hpp"
#include "lzs.hpp"
#include "physics.hpp"
#include "library/string.hpp"
#include "library/sha256.hpp"
#include "library/zip.hpp"

namespace sky
{
	struct tile_output_stream : public output_stream
	{
		tile_output_stream(std::vector<tile>& _out)
			: out(_out)
		{
			polarity = false;
		}
		void put(unsigned char byte)
		{
			polarity = !polarity;
			if(polarity)
				buffered = byte;
			else
				out.push_back(tile(combine(buffered, byte)));
		}
	private:
		std::vector<tile>& out;
		uint8_t buffered;
		bool polarity;
	};

	level::level()
	{
		memset(this, 0, sizeof(*this));
		length = 0;
		gravity = 8;
		o2_amount = 1;
		fuel_amount = 1;
	}

	level::level(rom_level& data)
	{
		length = data.get_length();
		gravity = data.get_gravity();
		o2_amount = data.get_o2_amount();
		fuel_amount = data.get_fuel_amount();
		for(unsigned i = 0; i < 72; i++)
			palette[i] = data.get_palette_color(i);
		for(unsigned i = 0; i < sizeof(tiles) / sizeof(tiles[0]); i++)
			tiles[i] = data.get_tile(i).rawtile();
	}

	rom_level::rom_level(const std::vector<char>& data, size_t offset, size_t length)
	{
		gravity = combine(access_array(data, offset + 0), access_array(data, offset + 1));
		fuel_amount = combine(access_array(data, offset + 2), access_array(data, offset + 3));
		o2_amount = combine(access_array(data, offset + 4), access_array(data, offset + 5));
		//Next 216 bytes are the palette table.
		for(unsigned i = 0; i < 72; i++) {
			uint8_t r = expand_color(access_array(data, offset + 3 * i + 6));
			uint8_t g = expand_color(access_array(data, offset + 3 * i + 7));
			uint8_t b = expand_color(access_array(data, offset + 3 * i + 8));
			palette[i] = ((uint32_t)r << 16) | ((uint32_t)g << 8) | ((uint32_t)b);
		}
		//After 222 bytes, there is compressed data.
		vector_input_stream in(data, offset + 222);
		tile_output_stream out(tiles);
		lzs_decompress(in, out, length >> 1 << 1);
	}

	tile level::at(uint32_t lpos, uint16_t hpos)
	{
		if(hpos < 12160 || hpos >= 53376)
			return tile();
		else {
			size_t ptr = 7 * static_cast<size_t>(lpos >> 16) + (hpos - 12160) / 5888;
			ptr = ptr * 2 + (reinterpret_cast<uint8_t*>(tiles) - reinterpret_cast<uint8_t*>(this));
			ptr &= 0xFFFF;
			return tile(*reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(this) + ptr));
		}
	}

	tile level::at_tile(int32_t l, int16_t h)
	{
		if(h < -3 || h > 3)
			return tile();
		else {
			size_t ptr = 7 * static_cast<size_t>(l) + h + 3;
			ptr = ptr * 2 + (reinterpret_cast<uint8_t*>(tiles) - reinterpret_cast<uint8_t*>(this));
			ptr &= 0xFFFF;
			return tile(*reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(this) + ptr));
		}
	}

	bool level::collides(uint32_t lpos, uint16_t hpos, int16_t vpos)
	{
		tile a = at(lpos, hpos - 1792);
		tile b = at(lpos, hpos + 1792);
		//Floor collision check.
		if(a.is_colliding_floor(0, vpos) || b.is_colliding_floor(0, vpos))
			return true;
		if(a.is_block() || b.is_block()) {
			tile c = at(lpos, hpos);
			int hchunk = 23 - (hpos / 128 - 49) % 46;
			int16_t offset = -5888;
			if(hchunk < 0) {
				hchunk = 1 - hchunk;
				offset = 5888;
			}
			if(c.is_colliding(hchunk, vpos))
				return true;
			tile d = at(lpos, hpos + offset);
			if(d.is_colliding(47 - hchunk, vpos))
				return true;
		}
		return false;
	}
	bool level::in_pipe(uint32_t lpos, uint16_t hpos, int16_t vpos)
	{
		int hchunk = 23 - (hpos / 128 - 49) % 46;
		if(hchunk <= 0)
			hchunk = 1 - hchunk;
		return at(lpos, hpos).in_pipe(hchunk, vpos);
	}

	inline void hash_uint16(sha256& h, uint16_t v)
	{
		uint8_t buf[2];
		buf[0] = v;
		buf[1] = v >> 8;
		h.write(buf, 2);
	}

	void rom_level::sha256_hash(uint8_t* buffer)
	{
		sha256 h;
		hash_uint16(h, gravity);
		hash_uint16(h, fuel_amount);
		hash_uint16(h, o2_amount);
		for(auto i : tiles)
			hash_uint16(h, i.rawtile());
		h.read(buffer);
	}

	roads_lzs::roads_lzs()
	{
		for(size_t i = 0; i < 31; i++)
			l[i] = NULL;
	}

	roads_lzs::roads_lzs(const std::vector<char>& file)
	{
		for(size_t i = 0; i < 31; i++)
			l[i] = NULL;
		for(size_t i = 0; i < 31; i++) {
			try {
				uint16_t off = combine(access_array(file, 4 * i + 0), access_array(file, 4 * i + 1));
				uint16_t len = combine(access_array(file, 4 * i + 2), access_array(file, 4 * i + 3));
				if(len)
					l[i] = new rom_level(file, off, len);
			} catch(...) {
				for(size_t i = 0; i < 31; i++)
					if(l[i] != NULL)
						delete l[i];
				throw;
			}
		}
	}

	roads_lzs::~roads_lzs()
	{
		for(size_t i = 0; i < 31; i++)
			if(l[i] != NULL)
				delete l[i];
	}

	roads_lzs::roads_lzs(const roads_lzs& x)
	{
		for(size_t i = 0; i < 31; i++)
			l[i] = NULL;
		try {
			for(size_t i = 0; i < 31; i++)
				l[i] = (x.l[i] != NULL) ? new rom_level(*x.l[i]) : NULL;
		} catch(...) {
			for(size_t i = 0; i < 31; i++)
				if(l[i] != NULL)
					delete l[i];
			throw;
		}
	}

	roads_lzs& roads_lzs::operator=(const roads_lzs& x)
	{
		if(this == &x)
			return *this;
		rom_level* tmp[31];
		for(size_t i = 0; i < 31; i++)
			tmp[i] = NULL;
		try {
			for(size_t i = 0; i < 31; i++)
				tmp[i] = (x.l[i] != NULL) ? new rom_level(*x.l[i]) : NULL;
		} catch(...) {
			for(size_t i = 0; i < 31; i++)
				if(tmp[i] != NULL)
					delete tmp[i];
			throw;
		}
		for(size_t i = 0; i < 31; i++) {
			delete l[i];
			l[i] = tmp[i];
		}
		return *this;
	}
}

#ifdef LEVEL_CHECKSUMMER
int main(int argc, char** argv)
{
	sky::roads_lzs levels(zip::readrel(argv[1], ""));
	uint32_t democount = 0;
	for(unsigned i = 0; i < 31; i++) {
		if(levels.present(i)) {
			uint8_t x[32];
			levels[i].sha256_hash(x);
			std::string demofile = (stringfmt() << argv[2] << i).str();
			try {
				std::vector<char> dem = zip::readrel(demofile, "");
				if(dem.size() == 0)
					continue;
				char d = 1;
				std::cout.write(&d, 1);
				std::cout.write((char*)x, 32);
				std::vector<char> comp;
				size_t dpos = 0;
				size_t run_start = 0;
				size_t run_byte = dem[0];
				while(dpos < dem.size()) {
					if(dpos - run_start > 256 || dem[dpos] != run_byte) {
						comp.push_back(dpos - run_start - 1);
						comp.push_back(run_byte);
						run_start = dpos;
						run_byte = dem[dpos];
					}
					dpos++;
				}
				comp.push_back(dpos - run_start - 1);
				comp.push_back(run_byte);
				char cmplen[3];
				cmplen[0] = comp.size() >> 16;
				cmplen[1] = comp.size() >> 8;
				cmplen[2] = comp.size();
				std::cout.write(cmplen, 3);
				std::cout.write(&comp[0], comp.size());
				democount++;
				std::cerr << "Demo entry for level " << i << ": " << dem.size() << " -> "
					<< comp.size() << std::endl;
			} catch(...) {
			}
		}
	}
	std::cerr << "Wrote " << democount << " demo entries." << std::endl;
}
#endif
