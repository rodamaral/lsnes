#ifndef _rom__hpp__included__
#define _rom__hpp__included__

#include <string>
#include <map>
#include <vector>
#include <stdexcept>
#include "misc.hpp"

/**
 * Region of ROM.
 */
enum rom_region
{
/**
 * Autodetect region
 */
	REGION_AUTO = 0,
/**
 * (force) PAL region
 */
	REGION_PAL,
/**
 * (force) NTSC region
 */
	REGION_NTSC
};

/**
 * Major type of ROM
 */
enum rom_type
{
/**
 * Ordinary SNES ROM
 */
	ROMTYPE_SNES,
/**
 * BS-X Slotted ROM.
 */
	ROMTYPE_BSXSLOTTED,
/**
 * BS-X (non-slotted) ROM.
 */
	ROMTYPE_BSX,
/**
 * Sufami Turbo ROM.
 */
	ROMTYPE_SUFAMITURBO,
/**
 * Super Game Boy ROM.
 */
	ROMTYPE_SGB,
/**
 * No ROM.
 */
	ROMTYPE_NONE
};

/**
 * This enumeration enumerates possible ROM types and regions for those.
 */
enum gametype_t
{
/**
 * NTSC-region SNES game
 */
	GT_SNES_NTSC = 0,
/**
 * PAL-region SNES game
 */
	GT_SNES_PAL = 1,
/**
 * NTSC-region BSX slotted game
 */
	GT_BSX_SLOTTED = 2,
/**
 * NTSC-region BSX (non-slotted) game
 */
	GT_BSX = 3,
/**
 * NTSC-region sufami turbo game
 */
	GT_SUFAMITURBO = 4,
/**
 * NTSC-region Super Game Boy game
 */
	GT_SGB_NTSC = 5,
/**
 * PAL-region Super Game Boy game
 */
	GT_SGB_PAL = 6,
/**
 * Invalid game type
 */
	GT_INVALID = 7
};

/**
 * Translations between diffrent representations of type.
 */
class gtype
{
public:
/**
 * Translate from major ROM type and region to string representation of the type.
 *
 * parameter rtype: The major ROM type.
 * parameter region: Region.
 * returns: String representation of combined type/region.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Invalid type.
 */
	static std::string tostring(rom_type rtype, rom_region region) throw(std::bad_alloc, std::runtime_error);
/**
 * Translate major/region combination to string representation.
 *
 * This function produces the same IDs as the other tostring(), except that it can't produce arbitrary-region ones.
 *
 * parameter gametype: Type of the game.
 * returns: String representation of the type.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Invalid type.
 */
	static std::string tostring(gametype_t gametype) throw(std::bad_alloc, std::runtime_error);
/**
 * Combine major/region into game type.
 *
 * For arbitrary-region types, this gives NTSC types.
 *
 * parameter rtype: Major type.
 * parameter region: The region.
 * returns: The combined game type.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Invalid type.
 */
	static gametype_t togametype(rom_type rtype, rom_region region) throw(std::bad_alloc, std::runtime_error);
/**
 * Parse string representation to game type.
 *
 * parameter gametype: The game type to parse.
 * returns: The parsed game type.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Invalid type.
 */
	static gametype_t togametype(const std::string& gametype) throw(std::bad_alloc, std::runtime_error);
/**
 * Parse string representation into major type.
 *
 * parameter gametype: The game type to parse.
 * returns: The major type.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Invalid type.
 */
	static rom_type toromtype(const std::string& gametype) throw(std::bad_alloc, std::runtime_error);
/**
 * Extract major type out of game type.
 *
 * parameter gametype: the game type to parse.
 * returns: The major type.
 */
	static rom_type toromtype(gametype_t gametype) throw();
/**
 * Extract region out of game type.
 *
 * parameter gametype: the game type to parse.
 * returns: The region.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Invalid type.
 */
	static rom_region toromregion(const std::string& gametype) throw(std::bad_alloc, std::runtime_error);
/**
 * Extract region out of game type.
 *
 * parameter gametype: the game type to parse.
 * returns: The region.
 */
	static rom_region toromregion(gametype_t gametype) throw();
};

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
	enum rom_type rtype;
/**
 * Game region (the region ROM is to be loaded as)
 */
	enum rom_region region;
/**
 * Relative filename of main ROM file.
 */
	std::string rom;
/**
 * Relative filename of main ROM XML file.
 */
	std::string rom_xml;
/**
 * Relative filename of slot A ROM file (non-SNES only).
 */
	std::string slota;
/**
 * Relative filename of slot A XML file (non-SNES only).
 */
	std::string slota_xml;
/**
 * Relative filename of slot B ROM file (Sufami Turbo only).
 */
	std::string slotb;
/**
 * Relative filename of slot B XML file (Sufami Turbo only).
 */
	std::string slotb_xml;
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
 * parameter xml_flag: If set, always keep trailing NUL.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Can't load the data.
 */
	loaded_slot(const std::string& filename, const std::string& base, bool xml_flag = false) throw(std::bad_alloc,
		std::runtime_error);

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
	enum rom_type rtype;
/**
 * ROM region (this is the currently active region).
 */
	enum rom_region region;
/**
 * ROM original region (this is the region ROM is loaded as).
 */
	enum rom_region orig_region;
/**
 * Loaded main ROM
 */
	loaded_slot rom;
/**
 * Loaded main ROM XML
 */
	loaded_slot rom_xml;
/**
 * Loaded slot A ROM (.bs, .st or .dmg)
 */
	loaded_slot slota;
/**
 * Loaded slot A XML (.bs, .st or .dmg)
 */
	loaded_slot slota_xml;
/**
 * Loaded slot B ROM (.st)
 */
	loaded_slot slotb;
/**
 * Loaded slot B XML (.st).
 */
	loaded_slot slotb_xml;

/**
 * Patch the ROM.
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
 * Recognize the slot this ROM goes to.
 *
 * parameter major: The major type.
 * parameter romname: Name of the ROM type.
 * returns: Even if this is main rom, odd if XML. 0/1 for main slot, 2/3 for slot A, 4/5 for slot B. -1 if not valid
 *	rom type.
 * throws std::bad_alloc: Not enough memory
 */
int recognize_commandline_rom(enum rom_type major, const std::string& romname) throw(std::bad_alloc);

/**
 * Recognize major type from flags.
 *
 * parameter flags: Flags telling what ROM parameters are present.
 * returns: The recognzed major type.
 * throws std::bad_alloc: Not enough memory
 * throws std::runtime_error: Illegal flags.
 */
rom_type recognize_platform(unsigned long flags) throw(std::bad_alloc, std::runtime_error);

/**
 * Name a sub-ROM.
 *
 * parameter major: The major type.
 * parameter romnumber: ROM number to name (as returned by recognize_commandline_rom).
 * throws std::bad_alloc: Not enough memory
 */
std::string name_subrom(enum rom_type major, unsigned romnumber) throw(std::bad_alloc);

/**
 * Get major type and region of loaded ROM.
 *
 * returns: Tuple (ROM type, ROM region) of currently loaded ROM.
 */
std::pair<enum rom_type, enum rom_region> get_current_rom_info() throw();

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
 * Saves core state into buffer. WARNING: This takes emulated time.
 *
 * returns: The saved state.
 * throws std::bad_alloc: Not enough memory.
 */
std::vector<char> save_core_state() throw(std::bad_alloc);

/**
 * Loads core state from buffer.
 *
 * parameter buf: The buffer containing the state.
 * throws std::runtime_error: Loading state failed.
 */
void load_core_state(const std::vector<char>& buf) throw(std::runtime_error);

/**
 * Read index of ROMs and add ROMs found to content-searchable storage.
 *
 * parameter filename: The filename of index file.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Loading index failed.
 */
void load_index_file(const std::string& filename) throw(std::bad_alloc, std::runtime_error);

/**
 * Search all indices, looking for file with specified SHA-256 (specifying hash of "" results "").
 *
 * parameter hash: The hash of file.
 * returns: Absolute filename.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Not found.
 */
std::string lookup_file_by_sha256(const std::string& hash) throw(std::bad_alloc, std::runtime_error);

#endif
