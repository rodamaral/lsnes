#ifndef _rom__hpp__included__
#define _rom__hpp__included__

#include <string>
#include <map>
#include <vector>
#include <stdexcept>
#include "core/misc.hpp"
#include "core/rom-small.hpp"
#include "interface/romtype.hpp"
#include "library/fileimage.hpp"

//ROM request.
struct rom_request
{
	//List of core types.
	std::vector<core_type*> cores;
	//Selected core (default core on call).
	bool core_guessed;
	size_t selected;
	//Filename selected (on entry, filename hint).
	bool has_slot[ROM_SLOT_COUNT];
	bool guessed[ROM_SLOT_COUNT];
	std::string filename[ROM_SLOT_COUNT];
	std::string hash[ROM_SLOT_COUNT];
	std::string hashxml[ROM_SLOT_COUNT];
	//Canceled flag.
	bool canceled;
};

/**
 * ROM loaded into memory.
 */
struct loaded_rom
{
/**
 * Create blank ROM
 */
	loaded_rom() throw();
/**
 * Take in ROM filename (or a bundle) and load it to memory.
 *
 * parameter file: The file to load
 * parameter tmpprefer: The core name to prefer.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Loading ROM file failed.
 */
	loaded_rom(const std::string& file, const std::string& tmpprefer = "") throw(std::bad_alloc,
		std::runtime_error);
/**
 * Take a ROM and load it.
 */
	loaded_rom(const std::string& file, const std::string& core, const std::string& type,
		const std::string& region);
/**
 * Load a multi-file ROM.
 */
	loaded_rom(const std::string file[ROM_SLOT_COUNT], const std::string& core, const std::string& type,
		const std::string& region);
/**
 * Take in ROM filename and load it to memory with specified type.
 *
 * parameter file: The file to load
 * parameter ctype: The core type to use.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Loading ROM file failed.
 */
	loaded_rom(const std::string& file, core_type& ctype) throw(std::bad_alloc, std::runtime_error);
/**
 * Switches the active cartridge to this cartridge. The compatiblity between selected region and original region
 * is checked. Region is updated after cartridge has been loaded.
 *
 * throws std::bad_alloc: Not enough memory
 * throws std::runtime_error: Switching cartridges failed.
 */
	void load(std::map<std::string, std::string>& settings, uint64_t rtc_sec, uint64_t rtc_subsec)
		throw(std::bad_alloc, std::runtime_error);
/**
 * Saves core state into buffer. WARNING: This takes emulated time.
 *
 * returns: The saved state.
 * throws std::bad_alloc: Not enough memory.
 */
	std::vector<char> save_core_state(bool nochecksum = false) throw(std::bad_alloc, std::runtime_error);

/**
 * Loads core state from buffer.
 *
 * parameter buf: The buffer containing the state.
 * throws std::runtime_error: Loading state failed.
 */
	void load_core_state(const std::vector<char>& buf, bool nochecksum = false) throw(std::runtime_error);

/**
 * Is file a gamepak?
 *
 * parameter filename: The file to probe.
 * retruns: True if gamepak, false if not.
 * throws std::runtime_error: No such file.
 */
	static bool is_gamepak(const std::string& filename) throw(std::bad_alloc, std::runtime_error);

/**
 * Get internal type representation.
 */
	core_type& get_internal_rom_type() { return *rtype; }
/**
 * Get internal region representation.
 */
	core_region& get_internal_region() { return *region; }
/**
 * Is same ROM type?
 */
	bool is_of_type(core_type& type) { return (rtype == &type); }
/**
 * Get gametype of this ROM.
 */
	core_sysregion& get_sysregion() { return rtype->combine_region(*region); }
/**
 * Set internal region representation.
 */
	void set_internal_region(core_region& reg) { region = &reg; }
/**
 * Access main ROM image.
 *
 * parameter index: The index of ROM slot to access.
 * returns: The ROM image (NULL image if index is out of range).
 */
	fileimage::image& get_rom(size_t index) { return get_image(index, false); }
/**
 * Access ROM markup image.
 *
 * parameter index: The index of ROM slot to access.
 * returns: The ROM markup image (NULL image if index is out of range).
 */
	fileimage::image& get_markup(size_t index) { return get_image(index, true); }
/**
 * Get filename of ROM pack, if any.
 */
	const std::string& get_pack_filename() { return load_filename; }
/**
 * Get MSU-1 base fileaname.
 */
	const std::string& get_msu1_base() { return msu1_base; }
	//ROM methods.
	std::string get_core_identifier() { return rtype->get_core_identifier(); }
	std::pair<uint32_t, uint32_t> get_scale_factors(uint32_t width, uint32_t height)
	{
		return rtype->get_scale_factors(width, height);
	}
	const std::string& get_hname() { return rtype->get_hname(); }
	core_sysregion& combine_region(core_region& reg) { return rtype->combine_region(reg); }
	bool isnull() { return !rtype || rtype->isnull(); }
	std::vector<std::string> get_trace_cpus() { return rtype->get_trace_cpus(); }
	controller_set controllerconfig(std::map<std::string, std::string>& settings)
	{
		return rtype->controllerconfig(settings);
	}
	core_setting_group& get_settings() { return rtype->get_settings(); }
	std::set<std::string> srams() { return rtype->srams(); }
	double get_PAR() { return rtype->get_PAR(); }
	std::string get_systemmenu_name() { return rtype->get_systemmenu_name(); }
	unsigned action_flags(unsigned id) { return rtype->action_flags(id); }
	std::set<const interface_action*> get_actions() { return rtype->get_actions(); }
	void execute_action(unsigned id, const std::vector<interface_action_paramval>& p)
	{
		return rtype->execute_action(id, p);
	}
	std::pair<unsigned, unsigned> lightgun_scale() { return rtype->lightgun_scale(); }
	const interface_device_reg* get_registers() { return rtype->get_registers(); }
	bool get_pflag() { return rtype->get_pflag(); }
	void set_pflag(bool pflag) { rtype->set_pflag(pflag); }
	std::pair<uint64_t, uint64_t> get_bus_map() { return rtype->get_bus_map(); }
	std::list<core_region*> get_regions() { return rtype->get_regions(); }
	const std::string& get_iname() { return rtype->get_iname(); }
	std::map<std::string, std::vector<char>> save_sram() throw(std::bad_alloc) { return rtype->save_sram(); }
	void load_sram(std::map<std::string, std::vector<char>>& sram) throw(std::bad_alloc)
	{
		rtype->load_sram(sram);
	}
	std::list<core_vma_info> vma_list() { return rtype->vma_list(); }
	framebuffer::raw& draw_cover() { return rtype->draw_cover(); }
	int reset_action(bool hard) { return rtype->reset_action(hard); }
	void pre_emulate_frame(portctrl::frame& cf) { return rtype->pre_emulate_frame(cf); }
	void emulate() { rtype->emulate(); }
	void runtosave() { rtype->runtosave(); }
	std::pair<uint32_t, uint32_t> get_audio_rate() { return rtype->get_audio_rate(); }
	void set_debug_flags(uint64_t addr, unsigned flags_set, unsigned flags_clear)
	{
		return rtype->set_debug_flags(addr, flags_set, flags_clear);
	}
	void set_cheat(uint64_t addr, uint64_t value, bool set)
	{
		return rtype->set_cheat(addr, value, set);
	}
	void debug_reset()
	{
		rtype->debug_reset();
	}
	//Region methods.
	const std::string& orig_region_get_iname() { return orig_region->get_iname(); }
	const std::string& orig_region_get_hname() { return orig_region->get_hname(); }
	const std::string& region_get_iname() { return region->get_iname(); }
	const std::string& region_get_hname() { return region->get_hname(); }
	double region_approx_framerate() { return region->approx_framerate(); }
	void region_fill_framerate_magic(uint64_t* magic) { region->fill_framerate_magic(magic); }
	bool region_compatible_with(core_region& run)
	{
		return orig_region && orig_region->compatible_with(run);
	}
private:
	//Static NULL image.
	static fileimage::image null_img;
	//Loaded ROM images.
	fileimage::image romimg[ROM_SLOT_COUNT];
	//Loaded ROM XML (markup) images.
	fileimage::image romxml[ROM_SLOT_COUNT];
	//MSU-1 base filename.
	std::string msu1_base;
	//Load filename.
	std::string load_filename;
	//ROM type.
	core_type* rtype;
	//ROM region.
	core_region* region;
	//Region ROM was loaded as.
	core_region* orig_region;
	//Get image.
	fileimage::image& get_image(size_t index, bool xml)
	{
		if(index < ROM_SLOT_COUNT) {
			if(xml)
				return romxml[index];
			else
				return romimg[index];
		} else
			return null_img;
	}
	//Handle bundle load case.
	void load_bundle(const std::string& file, std::istream& spec, const std::string& tmpprefer)
		throw(std::bad_alloc, std::runtime_error);
};

/**
 * Get major type and region of loaded ROM.
 *
 * returns: Tuple (ROM type, ROM region) of currently loaded ROM.
 */
std::pair<core_type*, core_region*> get_current_rom_info() throw();

/**
 * Read SRAMs from command-line and and load the files.
 *
 * parameter cmdline: Command line
 * returns: The loaded SRAM contents.
 * throws std::bad_alloc: Out of memory.
 * throws std::runtime_error: Failed to load.
 */
std::map<std::string, std::vector<char>> load_sram_commandline(const std::vector<std::string>& cmdline)
	throw(std::bad_alloc, std::runtime_error);

/**
 * Set the hasher callback.
 */
void set_hasher_callback(std::function<void(uint64_t, uint64_t)> cb);

struct romload_request
{
	//Pack file to load. Overrides everything else.
	std::string packfile;
	//Single file to load to default slot.
	std::string singlefile;
	//Core and system. May be blank.
	std::string core;
	std::string system;
	std::string region;
	//Files to load.
	std::string files[ROM_SLOT_COUNT];
};

bool load_null_rom();
bool _load_new_rom(const romload_request& req);
bool reload_active_rom();
regex_results get_argument(const std::vector<std::string>& cmdline, const std::string& regexp);
std::string get_requested_core(const std::vector<std::string>& cmdline);
loaded_rom construct_rom(const std::string& movie_filename, const std::vector<std::string>& cmdline);
void try_guess_roms(rom_request& req);
void record_filehash(const std::string& file, uint64_t prefix, const std::string& hash);
std::string try_to_guess_rom(const std::string& hint, const std::string& hash, const std::string& xhash,
	core_type& type, unsigned i);


//Map of preferred cores for each extension and type.
extern std::map<std::string, core_type*> preferred_core;
//Main hasher
extern fileimage::hash lsnes_image_hasher;


#endif
