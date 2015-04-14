#include "lsnes.hpp"

#include "core/advdumper.hpp"
#include "core/dispatch.hpp"
#include "core/framerate.hpp"
#include "core/instance.hpp"
#include "core/memorymanip.hpp"
#include "core/messages.hpp"
#include "core/nullcore.hpp"
#include "core/rom.hpp"
#include "core/settings.hpp"
#include "core/window.hpp"
#include "interface/callbacks.hpp"
#include "interface/cover.hpp"
#include "interface/romtype.hpp"
#include "library/portctrl-data.hpp"
#include "library/fileimage-patch.hpp"
#include "library/sha256.hpp"
#include "library/string.hpp"
#include "library/zip.hpp"

#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <set>
#include <boost/iostreams/categories.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/stream_buffer.hpp>
#include <boost/iostreams/filter/symmetric.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/device/back_inserter.hpp>

#ifdef USE_LIBGCRYPT_SHA256
#include <gcrypt.h>
#endif

namespace
{
	settingvar::supervariable<settingvar::model_bool<settingvar::yes_no>> savestate_no_check(lsnes_setgrp,
		"dont-check-savestate", "Movie‣Loading‣Don't check savestates", false);

	core_type* current_rom_type = &get_null_type();
	core_region* current_region = &get_null_region();
}

std::pair<core_type*, core_region*> get_current_rom_info() throw()
{
	return std::make_pair(current_rom_type, current_region);
}

loaded_rom::loaded_rom() throw()
{
	region = &image->get_region();
}

loaded_rom::loaded_rom(rom_image_handle _image) throw(std::bad_alloc, std::runtime_error)
{
	image = _image; 
	region = &image->get_region();
}

void loaded_rom::load(std::map<std::string, std::string>& settings, uint64_t rtc_sec, uint64_t rtc_subsec)
	throw(std::bad_alloc, std::runtime_error)
{
	auto& core = CORE();
	core_type* old_type = current_rom_type;
	core_core* old_core = current_rom_type->get_core();
	current_rom_type = &get_null_type();
	if(&rtype() != &get_null_type())
		image->setup_region(rtype().get_preferred_region());
	if(!region)
		region = &image->get_region();
	if(!image->get_region().compatible_with(*region))
		throw std::runtime_error("Trying to force incompatible region");
	if(!rtype().set_region(*region))
		throw std::runtime_error("Trying to force unknown region");

	core_romimage images[ROM_SLOT_COUNT];
	for(size_t i = 0; i < ROM_SLOT_COUNT; i++) {
		auto& img = image->get_image(i, false);
		auto& xml = image->get_image(i, true);
		images[i].markup = (const char*)xml;
		images[i].data = (const unsigned char*)img;
		images[i].size = (size_t)img;
	}
	if(!rtype().load(images, settings, rtc_sec, rtc_subsec))
		throw std::runtime_error("Can't load cartridge ROM");

	region = &rtype().get_region();
	rtype().power();
	auto nominal_fps = rtype().get_video_rate();
	auto nominal_hz = rtype().get_audio_rate();
	core.framerate->set_nominal_framerate(1.0 * nominal_fps.first / nominal_fps.second);
	core.mdumper->on_rate_change(nominal_hz.first, nominal_hz.second);

	current_rom_type = &rtype();
	current_region = region;
	//If core changes, unload the cartridge.
	if(old_core != current_rom_type->get_core())
		try {
			old_core->debug_reset();
			old_core->unload_cartridge();
		} catch(...) {}
	(*core.cmapper)();
	core.dispatch->core_changed(old_type != current_rom_type);
}

std::map<std::string, std::vector<char>> load_sram_commandline(const std::vector<std::string>& cmdline)
	throw(std::bad_alloc, std::runtime_error)
{
	std::map<std::string, std::vector<char>> ret;
	regex_results opt;
	for(auto i : cmdline) {
		if(opt = regex("--continue=(.+)", i)) {
			zip::reader r(opt[1]);
			for(auto j : r) {
				auto sramname = regex("sram\\.(.*)", j);
				if(!sramname)
					continue;
				std::istream& x = r[j];
				try {
					std::vector<char> out;
					boost::iostreams::back_insert_device<std::vector<char>> rd(out);
					boost::iostreams::copy(x, rd);
					delete &x;
					ret[sramname[1]] = out;
				} catch(...) {
					delete &x;
					throw;
				}
			}
			continue;
		} else if(opt = regex("--sram-([^=]+)=(.+)", i)) {
			try {
				ret[opt[1]] = zip::readrel(opt[2], "");
			} catch(std::bad_alloc& e) {
				throw;
			} catch(std::runtime_error& e) {
				throw std::runtime_error("Can't load SRAM '" + opt[1] + "': " + e.what());
			}
		}
	}
	return ret;
}

std::vector<char> loaded_rom::save_core_state(bool nochecksum) throw(std::bad_alloc, std::runtime_error)
{
	std::vector<char> ret;
	rtype().serialize(ret);
	if(nochecksum)
		return ret;
	size_t offset = ret.size();
	unsigned char tmp[32];
#ifdef USE_LIBGCRYPT_SHA256
	gcry_md_hash_buffer(GCRY_MD_SHA256, tmp, &ret[0], offset);
#else
	sha256::hash(tmp, ret);
#endif
	ret.resize(offset + 32);
	memcpy(&ret[offset], tmp, 32);
	return ret;
}

void loaded_rom::load_core_state(const std::vector<char>& buf, bool nochecksum) throw(std::runtime_error)
{
	if(nochecksum) {
		rtype().unserialize(&buf[0], buf.size());
		return;
	}

	if(buf.size() < 32)
		throw std::runtime_error("Savestate corrupt");
	if(!savestate_no_check(*CORE().settings)) {
		unsigned char tmp[32];
#ifdef USE_LIBGCRYPT_SHA256
		gcry_md_hash_buffer(GCRY_MD_SHA256, tmp, &buf[0], buf.size() - 32);
#else
		sha256::hash(tmp, reinterpret_cast<const uint8_t*>(&buf[0]), buf.size() - 32);
#endif
		if(memcmp(tmp, &buf[buf.size() - 32], 32))
			throw std::runtime_error("Savestate corrupt");
	}
	rtype().unserialize(&buf[0], buf.size() - 32);
}
