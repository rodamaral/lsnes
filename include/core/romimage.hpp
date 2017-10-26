#ifndef _romimage__hpp__included__
#define _romimage__hpp__included__

#include <functional>
#include "core/rom-small.hpp"
#include "interface/romtype.hpp"
#include "library/fileimage.hpp"
#include "library/threads.hpp"

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
 * A collection of files making up a ROM image.
 */
class rom_image
{
public:
/**
 * Create blank ROM
 */
	rom_image() throw();
/**
 * Take in ROM filename (or a bundle) and load it to memory.
 *
 * parameter file: The file to load
 * parameter tmpprefer: The core name to prefer.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Loading ROM file failed.
 */
	rom_image(const std::string& file, const std::string& tmpprefer = "") throw(std::bad_alloc,
		std::runtime_error);
/**
 * Take a ROM and load it.
 */
	rom_image(const std::string& file, const std::string& core, const std::string& type,
		const std::string& region);
/**
 * Load a multi-file ROM.
 */
	rom_image(const std::string file[ROM_SLOT_COUNT], const std::string& core, const std::string& type,
		const std::string& region);
/**
 * Take in ROM filename and load it to memory with specified type.
 *
 * parameter file: The file to load
 * parameter ctype: The core type to use.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Loading ROM file failed.
 */
	rom_image(const std::string& file, core_type& ctype) throw(std::bad_alloc, std::runtime_error);
/**
 * Destroy ROM image.
 */
	~rom_image();
/**
 * Get ROM type.
 */
	core_type& get_type() { return *rtype; }
/**
 * Get ROM region.
 */
	core_region& get_region() { return *orig_region; }
/**
 * Do region setup. Changes orig_region to specified if NULL.
 */
	void setup_region(core_region& reg);
/**
 * Get image.
 */
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
/**
 * Get filename of ROM pack, if any.
 */
	const std::string& get_pack_filename() { return load_filename; }
/**
 * Get MSU-1 base fileaname.
 */
	const std::string& get_msu1_base() { return msu1_base; }
/**
 * Is same ROM type?
 */
	bool is_of_type(core_type& type) { return (rtype == &type); }
/**
 * Is file a gamepak?
 *
 * parameter filename: The file to probe.
 * retruns: True if gamepak, false if not.
 * throws std::runtime_error: No such file.
 */
	static bool is_gamepak(const std::string& filename) throw(std::bad_alloc, std::runtime_error);
	//ROM functions.
	std::list<core_region*> get_regions() { return rtype->get_regions(); }
	const std::string& get_hname() { return rtype->get_hname(); }
private:
	rom_image(const rom_image&);
	rom_image& operator=(const rom_image&);
	//Account images.
	void account_images();
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
	//Reference count.
	threads::lock usage_lock;
	size_t usage_count;
	void get() { threads::alock l(usage_lock); usage_count++; }
	bool put() { threads::alock l(usage_lock); return !--usage_count; }
	friend class rom_image_handle;
	//Handle bundle load case.
	void load_bundle(const std::string& file, std::istream& spec, const std::string& tmpprefer)
		throw(std::bad_alloc, std::runtime_error);
	//Tracker.
	memtracker::autorelease tracker;
};

/**
 * A handle for ROM imageset.
 */
class rom_image_handle
{
public:
/**
 * Create a handle to NULL imageset.
 */
	rom_image_handle()
	{
		img = &null_img;
	}
/**
 * Create a new handle with refcount 1. The specified ROM imageset has to be allocated using new.
 */
	rom_image_handle(rom_image* _img)
	{
		img = _img;
		img->usage_count = 1;
	}
/**
 * Copy-construct a handle.
 */
	rom_image_handle(const rom_image_handle& h)
	{
		h.get();
		img = h.img;
	}
/**
 * Assign a handle.
 */
	rom_image_handle& operator=(const rom_image_handle& h)
	{
		if(img == h.img) return *this;
		h.get();
		put();
		img = h.img;
		return *this;
	}
/**
 * Destroy a handle.
 */
	~rom_image_handle()
	{
		put();
	}
/**
 * Access the handle.
 */
	rom_image& operator*()
	{
		return *img;
	}
/**
 * Access the handle.
 */
	rom_image* operator->()
	{
		return img;
	}
private:
	static rom_image null_img;
	mutable rom_image* img;
	void get() const { if(img != &null_img) img->get(); }
	void put() const { if(img != &null_img && img->put()) delete img; }
};


void record_filehash(const std::string& file, uint64_t prefix, const std::string& hash);
void set_hasher_callback(std::function<void(uint64_t, uint64_t)> cb);
rom_image_handle construct_rom(const std::string& movie_filename, const std::vector<std::string>& cmdline);

//Map of preferred cores for each extension and type.
extern std::map<std::string, core_type*> preferred_core;
//Main hasher
extern fileimage::hash lsnes_image_hasher;

#endif
