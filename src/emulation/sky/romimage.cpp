#include "romimage.hpp"
#include "framebuffer.hpp"
#include "tasdemos.hpp"
#include "physics.hpp"
#include "instance.hpp"
#include "library/hex.hpp"
#include "library/string.hpp"
#include "library/zip.hpp"
#include <sys/time.h>
#include <boost/iostreams/categories.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/stream_buffer.hpp>
#include <boost/iostreams/filter/symmetric.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/device/back_inserter.hpp>

namespace sky
{
uint64_t get_utime()
{
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
}


	void load_builtin_demos(std::vector<demoset_entry>& _demos)
	{
		const unsigned char* ptr = tasdemos_data;
		while(*ptr) {
			ptr++;
			demoset_entry e;
			memcpy(e.hash, ptr, 32);
			ptr += 32;
			uint32_t l = 0;
			l = (l << 8) | *(ptr++);
			l = (l << 8) | *(ptr++);
			l = (l << 8) | *(ptr++);
			e.demodata.resize(l);
			memcpy(&e.demodata[0], ptr, l);
			ptr += l;
			_demos.push_back(e);
		}
	}

	void load_demos(std::vector<demoset_entry>& _demos, const std::string& filename)
	{
		zip::reader r(filename);
		for(auto i : r) {
			regex_results rx;
			if(rx = regex("([0-9A-Fa-f]{64}).rec", i)) {
				demoset_entry e;
				memset(e.hash, 0, 32);
				for(unsigned j = 0; j < 64; j++) {
					unsigned x = i[j];
					x = (x & 0x1F) ^ 0x10;
					x = x - (x >> 4) * 7;
					e.hash[j / 2] = 16 * e.hash[j / 2] + x;
				}
				std::istream& z = r[i];
				boost::iostreams::back_insert_device<std::vector<char>> rd(e.demodata);
				boost::iostreams::copy(z, rd);
				delete &z;
				_demos.push_back(e);
			}
		}
	}

	void load_rom(struct instance& inst, const std::string& filename)
	{
		std::string errfile;
		try {
			errfile = "/SPEED.DAT";
			gauge _speed_dat(zip::readrel(filename + errfile, ""), 0x22);
			errfile = "/OXY_DISP.DAT";
			gauge _oxydisp_dat(zip::readrel(filename + errfile, ""), 0x0a);
			errfile = "/FUL_DISP.DAT";
			gauge _fueldisp_dat(zip::readrel(filename + errfile, ""), 0x0a);
			errfile = "/ROADS.LZS";
			roads_lzs _levels(zip::readrel(filename + errfile, ""));
			errfile = "/CARS.LZS";
			image _ship(zip::readrel(filename + errfile, ""));
			errfile = "/DASHBRD.LZS";
			image _dashboard(zip::readrel(filename + errfile, ""));
			if(_dashboard.width != 320 || _dashboard.height > 200) {
				std::cerr << _dashboard.width << "x" << _dashboard.height << std::endl;
				throw std::runtime_error("Must be 320 wide and at most 200 high");
			}
			errfile = "/GOMENU.LZS";
			image _levelselect(zip::readrel(filename + errfile, ""));
			if(_levelselect.width != 320 || _levelselect.height != 200)
				throw std::runtime_error("Must be 320x200");
			errfile = "/SFX.SND";
			sounds _soundfx(zip::readrel(filename + errfile, ""), 5);
			errfile = "/DEMO.REC";
			demo _builtin_demo(zip::readrel(filename + errfile, ""), true);
			image _backgrounds[10];
			for(unsigned i = 0; i < 10; i++) {
				std::string n = "/WORLDx.LZS";
				n[6] = '0' + i;
				errfile = n;
				//Skip nonexistent backgrounds.
				try {
					std::istream& x = zip::openrel(filename + errfile, "");
					delete &x;
				} catch(...) {
					continue;
				}
				_backgrounds[i] = image(zip::readrel(filename + errfile, ""));
				if(_backgrounds[i].width != 320 || _backgrounds[i].height > 200)
					throw std::runtime_error("Must be 320 wide and at most 200 high");
			}
			std::vector<demoset_entry> _demos;
			load_builtin_demos(_demos);
			errfile = "<demos>";
			load_demos(_demos, filename);

			inst.speed_dat = _speed_dat;
			inst.oxydisp_dat = _oxydisp_dat;
			inst.fueldisp_dat = _fueldisp_dat;
			inst.levels = _levels;
			inst.ship = _ship;
			inst.dashboard = _dashboard;
			inst.levelselect = _levelselect;
			inst.soundfx = _soundfx;
			inst.builtin_demo = _builtin_demo;
			inst.demos = _demos;
			memcpy(inst.dashpalette, inst.dashboard.palette, sizeof(inst.dashpalette));
			for(unsigned i = 0; i < 10; i++)
				inst.backgrounds[i] = _backgrounds[i];
			inst.rom_filename = filename;
		} catch(std::exception& e) { throw std::runtime_error(errfile + ": " + e.what()); }
	}

	//Combine background and dashboard into origbuffer and render.
	void combine_background(struct instance& inst, size_t back)
	{
		memset(inst.origbuffer, 0, sizeof(inst.origbuffer));
		image& bg = inst.backgrounds[back];
		if(bg.width && bg.height) {
			size_t pixels = 320 * bg.height;
			for(unsigned i = 0; i < pixels; i++)
				inst.origbuffer[i] = bg[i] & 0x00FFFFFFU;
		}
		{
			size_t pixels = 320 * inst.dashboard.height;
			size_t writestart = 64000 - 320 * inst.dashboard.height;
			for(unsigned i = 0; i < pixels; i++)
				if(inst.dashboard.decode[i])
					inst.origbuffer[i + writestart] = inst.dashboard[i] | 0xFF000000U;
		}
		render_backbuffer(inst);
	}

	demo lookup_demo(struct instance& inst, const uint8_t* levelhash)
	{
		for(auto i = inst.demos.rbegin(); i != inst.demos.rend(); i++) {
			if(!memcmp(i->hash, levelhash, 32))
				return demo(i->demodata, false);
		}
		throw std::runtime_error("No demo found for level");
	}
}

#ifdef DEMO_PLAYER
int main(int argc, char** argv)
{
	sky::load_rom(argv[1]);
	int lvl = atoi(argv[2]);
	if(!sky::levels.present(lvl)) {
		std::cerr << "Level " << lvl << " not found" << std::endl;
		return 1;
	}
	sky::level& l = sky::levels[lvl];
	uint8_t hash[32];
	l.sha256_hash(hash);
	std::cerr << "Level hash is " << hex::b_to(hash, 32) << std::endl;
	sky::demo d;
	try {
		if(lvl)
			d = sky::lookup_demo(hash);
		else
			d = sky::builtin_demo;
	} catch(std::exception& e) {
		std::cerr << "Demo lookup failed" << std::endl;
		return 1;
	}
	std::cerr << "Found a demo." << std::endl;
	sky::physics p;
	p.level_init(l);
	uint64_t t1 = sky::get_utime();
	while(!p.death) {
		uint16_t buttons = d.fetchkeys(0, p.lpos, p.framecounter);
		int lr = 0, ad = 0;
		bool jump = ((buttons & 16) != 0);
		if((buttons & 1) != 0) lr--;
		if((buttons & 2) != 0) lr++;
		if((buttons & 4) != 0) ad++;
		if((buttons & 8) != 0) ad--;
		if((buttons & 256) != 0) lr = 2;	//Cheat for demo.
		if((buttons & 512) != 0) ad = 2;	//Cheat for demo.
		p.simulate_frame(l, lr, ad, jump);
		//std::cerr << p.framecounter << " pos: l=" << p.lpos << " h=" << p.hpos
		//	<< " v=" << p.vpos << " death=" << (int)p.death << std::endl;
	}
	uint64_t t2 = sky::get_utime();
	std::cerr << "Simulated " << p.framecounter << " frames in " << (t2 - t1) << "usec ("
		<< 1000000 * p.framecounter / (t2 - t1) << " fps)" << std::endl;
	if(p.death == 255)
		std::cerr << "LEVEL COMPLETED!" << std::endl;
	return (p.death == 255) ? 0 : 2;
}
#endif
