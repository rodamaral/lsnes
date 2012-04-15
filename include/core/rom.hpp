#ifndef _rom__hpp__included__
#define _rom__hpp__included__

#include <string>
#include <map>
#include <vector>
#include <stdexcept>
#include "interface/core.hpp"
#include "misc.hpp"

/**
 * This structure gives all files associated with given ROM image.
 */
struct rom_files
{
/**
 * Construct defaults
 */
	rom_files() throw();

/**
 * Reads the filenames out of command line arguments given.
 *
 * parameter cmdline: The commmand line
 * throws std::bad_alloc: Not enough memory
 * throws std::runtime_error: Failed to load ROM filenames.
 */
	rom_files(const std::vector<std::string>& cmdline) throw(std::bad_alloc, std::runtime_error);

/**
 * Resolve relative references.
 *
 * throws std::bad_alloc: Not enough memory
 * throws std::runtime_error: Bad path.
 */
	void resolve_relative() throw(std::bad_alloc, std::runtime_error);

/**
 * The file to look other ROM files relative to. May be blank.
 */
	std::string base_file;
/**
 * Major ROM type.
 */
	struct systype_info_structure* rtype;
/**
 * Game region (the region ROM is to be loaded as)
 */
	struct region_info_structure* region;
/**
 * ROM slots.
 */
	std::vector<std::string> main_slots;
/**
 * ROM markup slots.
 */
	std::vector<std::string> markup_slots;
};

/**
 * Some loaded data or indication of no data.
 */
struct loaded_slot
{
/**
 * Construct empty slot.
 *
 * throws std::bad_alloc: Not enough memory.
 */
	loaded_slot() throw(std::bad_alloc);

/**
 * This constructor construct slot by reading data from file. If filename is "", constructs an empty slot.
 *
 * parameter filename: The filename to read. If "", empty slot is constructed.
 * parameter base: Base filename to interpret the filename against. If "", no base filename is used.
 * parameter slot: The rom slot this is for.
 * parameter xml_flag: If set, always keep trailing NUL.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Can't load the data.
 */
	loaded_slot(const std::string& filename, const std::string& base, struct rom_info_structure& slot,
		bool xml_flag = false) throw(std::bad_alloc, std::runtime_error);

/**
 * This method patches this slot using specified IPS patch.
 *
 * parameter patch: The patch to apply
 * parameter offset: The amount to add to the offsets in the IPS file. Parts with offsets below zero are not patched.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Bad IPS patch.
 */
	void patch(const std::vector<char>& patch, int32_t offset) throw(std::bad_alloc, std::runtime_error);

/**
 * Is this slot XML slot?
 */
	bool xml;
/**
 * If this slot is blank, this is set to false, data is undefined and sha256 is "". Otherwise this is set to true,
 * data to apporiate data, and sha256 to hash of data.
 */
	bool valid;
/**
 * The actual data for this slot.
 */
	std::vector<char> data;
/**
 * SHA-256 for the data in this slot if data is valid. If no valid data, this field is "".
 */
	std::string sha256;
/**
 * Get pointer to loaded data
 *
 * returns: Pointer to loaded data, or NULL if slot is blank.
 */
	operator const char*() const throw()
	{
		return valid ? reinterpret_cast<const char*>(&data[0]) : NULL;
	}
/**
 * Get pointer to loaded data
 *
 * returns: Pointer to loaded data, or NULL if slot is blank.
 */
	operator const uint8_t*() const throw()
	{
		return valid ? reinterpret_cast<const uint8_t*>(&data[0]) : NULL;
	}
/**
 * Get size of slot
 *
 * returns: The number of bytes in slot, or 0 if slot is blank.
 */
	operator unsigned() const throw()
	{
		return valid ? data.size() : 0;
	}
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
 * Takes in collection of ROM filenames and loads them into memory.
 *
 * parameter files: The files to load
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Loading ROM files failed.
 */
	loaded_rom(const rom_files& files) throw(std::bad_alloc, std::runtime_error);
/**
 * ROM type
 */
	struct systype_info_structure* rtype;
/**
 * ROM region (this is the currently active region).
 */
	struct region_info_structure* region;
/**
 * ROM original region (this is the region ROM is loaded as).
 */
	struct region_info_structure* orig_region;
/**
 * Loaded main ROMs
 */
	std::vector<loaded_slot> main_slots;
/**
 * Loaded ROM markups
 */
	std::vector<loaded_slot> markup_slots;
/**
 * Patch the ROMs.
 *
 * parameter cmdline: The command line.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Failed to patch the ROM.
 */
	void do_patch(const std::vector<std::string>& cmdline) throw(std::bad_alloc, std::runtime_error);

/**
 * Switches the active cartridge to this cartridge. The compatiblity between selected region and original region
 * is checked. Region is updated after cartridge has been loaded.
 *
 * throws std::bad_alloc: Not enough memory
 * throws std::runtime_error: Switching cartridges failed.
 */
	void load() throw(std::bad_alloc, std::runtime_error);
};

/**
 * Take current values of all SRAMs in current system and save their contents.
 *
 * returns: Saved SRAM contents.
 * throws std::bad_alloc: Out of memory.
 */
std::map<std::string, std::vector<char>> save_sram() throw(std::bad_alloc);

/**
 * Write contents of saved SRAMs into current system SRAMs.
 *
 * parameter sram: Saved SRAM contents.
 * throws std::bad_alloc: Out of memory.
 */
void load_sram(std::map<std::string, std::vector<char>>& sram) throw(std::bad_alloc);

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
 * Given commandline arguments, load a ROM.
 *
 * parameter cmdline: The command line.
 * returns: The loaded ROM set.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Can't load the ROMset.
 */
struct loaded_rom load_rom_from_commandline(std::vector<std::string> cmdline) throw(std::bad_alloc,
	std::runtime_error);

/**
 * Dump listing of regions to graphics system messages.
 *
 * throws std::bad_alloc: Not enough memory.
 */
void dump_region_map() throw(std::bad_alloc);

#endif
