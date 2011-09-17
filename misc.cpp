#include "lsnes.hpp"
#include "memorymanip.hpp"
#include "window.hpp"
#include "misc.hpp"
#include "rom.hpp"
#include <sstream>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <ctime>
#include <cstdlib>
#include <cstring>
#include <boost/filesystem.hpp>

namespace
{
	std::string rseed;
	uint64_t rcounter = 0;

	std::string get_random_hexstring_64(size_t index)
	{
		std::ostringstream str;
		str << rseed << " " << time(NULL) << " " << (rcounter++) << " " << index;
		std::string s = str.str();
		std::vector<char> x;
		x.resize(s.length());
		std::copy(s.begin(), s.end(), x.begin());
		return sha256::hash(reinterpret_cast<uint8_t*>(&x[0]), x.size());
	}

	std::string collect_identifying_information()
	{
		//TODO: Collect as much identifying information as possible.
		std::ostringstream str;
		time_t told = time(NULL);
		time_t tnew;
		uint64_t loops = 0;
		uint64_t base = 0;
		int cnt = 0;
		while(cnt < 3) {
			tnew = time(NULL);
			if(tnew > told) {
				told = tnew;
				cnt++;
				str << (loops - base) << " ";
				base = loops;
			}
			loops++;
		}
		return str.str();
	}
}

std::string get_random_hexstring(size_t length) throw(std::bad_alloc)
{
	std::string out;
	for(size_t i = 0; i < length; i += 64)
		out = out + get_random_hexstring_64(i);
	return out.substr(0, length);
}

void set_random_seed(const std::string& seed) throw(std::bad_alloc)
{
	std::ostringstream str;
	str << seed.length() << " " << seed;
	rseed = str.str();
}

void set_random_seed() throw(std::bad_alloc)
{
	//Try /dev/urandom first.
	{
		std::ifstream r("/dev/urandom", std::ios::binary);
		if(r.is_open()) {
			char buf[64];
			r.read(buf, 64);
			std::string s(buf, 64);
			set_random_seed(s);
			return;
		}
	}
	//Fall back to time.
	std::ostringstream str;
	str << collect_identifying_information() << " " << time(NULL);
	set_random_seed(str.str());
}

//VERY dirty hack.
namespace foobar
{
#include <nall/sha256.hpp>
}
using foobar::nall::sha256_ctx;
using foobar::nall::sha256_init;
using foobar::nall::sha256_final;
using foobar::nall::sha256_hash;
using foobar::nall::sha256_chunk;

/**
 * \brief Opaque internal state of SHA256
 */
struct sha256_opaque
{
/**
 * \brief Opaque internal state structure of SHA256
 */
	sha256_ctx shactx;
};

sha256::sha256() throw(std::bad_alloc)
{
	opaque = new sha256_opaque();
	finished = false;
	sha256_init(&opaque->shactx);
}

sha256::~sha256() throw()
{
	delete opaque;
}

void sha256::write(const uint8_t* data, size_t datalen) throw()
{
	sha256_chunk(&opaque->shactx, data, datalen);
}

void sha256::read(uint8_t* hashout) throw()
{
	if(!finished)
		sha256_final(&opaque->shactx);
	finished = true;
	sha256_hash(&opaque->shactx, hashout);
}

std::string sha256::read() throw(std::bad_alloc)
{
	uint8_t b[32];
	read(b);
	return sha256::tostring(b);
}

void sha256::hash(uint8_t* hashout, const uint8_t* data, size_t datalen) throw()
{
	sha256 s;
	s.write(data, datalen);
	s.read(hashout);
}

std::string sha256::tostring(const uint8_t* hashout) throw(std::bad_alloc)
{
	std::ostringstream str;
	for(unsigned i = 0; i < 32; i++)
		str << std::hex << std::setw(2) << std::setfill('0') << (unsigned)hashout[i];
	return str.str();
}

struct loaded_rom load_rom_from_commandline(std::vector<std::string> cmdline) throw(std::bad_alloc,
	std::runtime_error)
{
	struct rom_files f;
	try {
		f = rom_files(cmdline);
		f.resolve_relative();
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		throw std::runtime_error(std::string("Can't resolve ROM files: ") + e.what());
	}
	messages << "ROM type: " << gtype::tostring(f.rtype, f.region) << std::endl;
	if(f.rom != "")		messages << name_subrom(f.rtype, 0) << " file: '" << f.rom << "'" << std::endl;
	if(f.rom_xml != "")	messages << name_subrom(f.rtype, 1) << " file: '" << f.rom_xml << "'"
		<< std::endl;
	if(f.slota != "")	messages << name_subrom(f.rtype, 2) << " file: '" << f.slota << "'" << std::endl;
	if(f.slota_xml != "")	messages << name_subrom(f.rtype, 3) << " file: '" << f.slota_xml << "'"
		<< std::endl;
	if(f.slotb != "")	messages << name_subrom(f.rtype, 4) << " file: '" << f.slotb << "'" << std::endl;
	if(f.slotb_xml != "")	messages << name_subrom(f.rtype, 5) << " file: '" << f.slotb_xml << "'"
		<< std::endl;

	struct loaded_rom r;
	try {
		r = loaded_rom(f);
		r.do_patch(cmdline);
	} catch(std::bad_alloc& e) {
		OOM_panic();
	} catch(std::exception& e) {
		throw std::runtime_error(std::string("Can't load ROM: ") + e.what());
	}

	std::string not_present = "N/A";
	if(r.rom.valid)		messages << name_subrom(f.rtype, 0) << " hash: " << r.rom.sha256 << std::endl;
	if(r.rom_xml.valid)	messages << name_subrom(f.rtype, 1) << " hash: " << r.rom_xml.sha256 << std::endl;
	if(r.slota.valid)	messages << name_subrom(f.rtype, 2) << " hash: " << r.slota.sha256 << std::endl;
	if(r.slota_xml.valid)	messages << name_subrom(f.rtype, 3) << " hash: " << r.slota_xml.sha256
		<< std::endl;
	if(r.slotb.valid)	messages << name_subrom(f.rtype, 4) << " hash: " << r.slotb.sha256 << std::endl;
	if(r.slotb_xml.valid)	messages << name_subrom(f.rtype, 5) << " hash: " << r.slotb_xml.sha256
		<< std::endl;
	return r;
}

void dump_region_map() throw(std::bad_alloc)
{
	std::vector<struct memory_region> regions = get_regions();
	for(auto i = regions.begin(); i != regions.end(); ++i) {
		char buf[256];
		sprintf(buf, "Region: %08X-%08X %08X %s%c %s", i->baseaddr, i->lastaddr, i->size,
			i->readonly ? "R-" : "RW", i->native_endian ? 'N' : 'B', i->region_name.c_str());
		messages << buf << std::endl;
	}
}

void fatal_error() throw()
{
	window::fatal_error();
	std::cout << "PANIC: Fatal error, can't continue." << std::endl;
	exit(1);
}

std::string get_config_path() throw(std::bad_alloc)
{
	const char* tmp;
	std::string basedir;
	if((tmp = getenv("APPDATA"))) {
		//If $APPDATA exists, it is the base directory
		basedir = tmp;
	} else if((tmp = getenv("XDG_CONFIG_HOME"))) {
		//If $XDG_CONFIG_HOME exists, it is the base directory
		basedir = tmp;
	} else if((tmp = getenv("HOME"))) {
		//If $HOME exists, the base directory is '.config' there.
		basedir = std::string(tmp) + "/.config";
	} else {
		//Last chance: Return current directory.
		return ".";
	}
	//Try to create 'lsnes'. If it exists (or is created) and is directory, great. Otherwise error out.
	std::string lsnes_path = basedir + "/lsnes";
	boost::filesystem::path p(lsnes_path);
	if(!boost::filesystem::create_directories(p) && !boost::filesystem::is_directory(p)) {
		messages << "FATAL: Can't create configuration directory '" << lsnes_path << "'" << std::endl;
		fatal_error();
	}
	//Yes, this is racy, but portability is more important than being absolutely correct...
	std::string tfile = lsnes_path + "/test";
	remove(tfile.c_str());
	FILE* x;
	if(!(x = fopen(tfile.c_str(), "w+"))) {
		messages << "FATAL: Configuration directory '" << lsnes_path << "' is not writable" << std::endl;
		fatal_error();
	}
	fclose(x);
	remove(tfile.c_str());
	return lsnes_path;
}

extern const char* lsnesrc_file;

void create_lsnesrc()
{
	std::string rcfile = get_config_path() + "/lsnes.rc";
	std::ifstream x(rcfile.c_str());
	if(x) {
		x.close();
		return;
	}
	std::ofstream y(rcfile.c_str());
	if(!y) {
		messages << "FATAL: lsnes.rc (" << rcfile << ") doesn't exist nor it can be created" << std::endl;
		fatal_error();
	}
	y.write(lsnesrc_file, strlen(lsnesrc_file));
	y.close();
}


void OOM_panic()
{
	messages << "FATAL: Out of memory!" << std::endl;
	fatal_error();
}

std::ostream& _messages()
{
	return window::out();
}


std::string bsnes_core_version;
std::string lsnes_version = "0-β2";
