#ifndef _interface__romtype__hpp__included__
#define _interface__romtype__hpp__included__

#include <list>
#include <string>
#include <cstdint>
#include <vector>
#include "interface/controller.hpp"
#include "interface/setting.hpp"

struct core_region;
struct core_type;
struct core_sysregion;
struct core_romimage;
struct core_romimage_info;
struct core_core;

struct core_region_params
{
	const char* iname;
	const char* hname;
	unsigned priority;
	unsigned handle;
	bool multi;
	uint64_t framemagic[4];
	//Ended by UINT_MAX.
	unsigned* compatible_runs;
};

struct core_romimage_info_params
{
	const char* iname;
	const char* hname;
	unsigned mandatory;
	int pass_mode;		//0 => Content, 1 => File, 2 => Directory.
	unsigned headersize;	//Header size to remove (0 if there is never header to remove).
};

struct core_type_params
{
	const char* iname;
	const char* hname;
	unsigned id;
	unsigned reset_support;
	int (*load_rom)(core_romimage* images, std::map<std::string, std::string>& settings, uint64_t rtc_sec,
		uint64_t rtc_subsec);
	controller_set (*controllerconfig)(std::map<std::string, std::string>& settings);
	const char* extensions;		//Separate by ;
	const char* bios;		//Name of BIOS. NULL if none.
	core_region** regions;		//Terminate with NULL.
	core_romimage_info** images;	//Terminate with NULL.
	core_setting_group* settings;
	core_core* core;
};

struct core_core_params
{
	std::string (*core_identifier)();
	bool (*set_region)(core_region& region);
	std::pair<uint32_t, uint32_t> (*video_rate)();
	std::pair<uint32_t, uint32_t> (*audio_rate)();
	std::pair<uint32_t, uint32_t> (*snes_rate)();
	std::map<std::string, std::vector<char>> (*save_sram)() throw(std::bad_alloc);
	void (*load_sram)(std::map<std::string, std::vector<char>>& sram) throw(std::bad_alloc);
	void (*serialize)(std::vector<char>& out);
	void (*unserialize)(const char* in, size_t insize);
};

struct core_region
{
public:
	core_region(const core_region_params& params);
	const std::string& get_iname();
	const std::string& get_hname();
	unsigned get_priority();
	unsigned get_handle();
	bool is_multi();
	void fill_framerate_magic(uint64_t* magic);	//4 elements filled.
	double approx_framerate();
	bool compatible_with(core_region& run);
private:
	core_region(const core_region&);
	core_region& operator=(const core_region&);
	std::string iname;
	std::string hname;
	bool multi;
	unsigned handle;
	unsigned priority;
	uint64_t magic[4];
	std::vector<unsigned> compatible;
};

struct core_romimage_info
{
	core_romimage_info(const core_romimage_info_params& params);
	std::string iname;
	std::string hname;
	unsigned mandatory;
	int pass_mode;
	unsigned headersize;
	size_t get_headnersize(size_t imagesize);
};

struct core_romimage
{
	const char* markup;
	const unsigned char* data;
	size_t size;
};

struct core_core
{
	core_core(core_core_params& params);
	bool set_region(core_region& region);
	std::pair<uint32_t, uint32_t> get_video_rate();
	std::pair<uint32_t, uint32_t> get_audio_rate();
	std::pair<uint32_t, uint32_t> get_snes_rate();	//(0,0) for non-SNES.
	std::string get_core_identifier();
	std::map<std::string, std::vector<char>> save_sram() throw(std::bad_alloc);
	void load_sram(std::map<std::string, std::vector<char>>& sram) throw(std::bad_alloc);
	void serialize(std::vector<char>& out);
	void unserialize(const char* in, size_t insize);
private:
	std::string (*_core_identifier)();
	bool (*_set_region)(core_region& region);
	std::pair<uint32_t, uint32_t> (*_video_rate)();
	std::pair<uint32_t, uint32_t> (*_audio_rate)();
	std::pair<uint32_t, uint32_t> (*_snes_rate)();
	std::map<std::string, std::vector<char>> (*_save_sram)() throw(std::bad_alloc);
	void (*_load_sram)(std::map<std::string, std::vector<char>>& sram) throw(std::bad_alloc);
	void (*_serialize)(std::vector<char>& out);
	void (*_unserialize)(const char* in, size_t insize);
};

struct core_type
{
public:
	core_type(core_type_params& params);
	static std::list<core_type*> get_core_types();
	core_region& get_preferred_region();
	std::list<core_region*> get_regions();
	core_sysregion& combine_region(core_region& reg);
	const std::string& get_iname();
	const std::string& get_hname();
	const std::list<std::string>& get_extensions();
	bool is_known_extension(const std::string& ext);
	std::string get_biosname();
	unsigned get_id();
	unsigned get_image_count();
	core_romimage_info get_image_info(unsigned index);
	bool load(core_romimage* images, std::map<std::string, std::string>& settings, uint64_t rtc_sec,
		uint64_t rtc_subsec);
	controller_set controllerconfig(std::map<std::string, std::string>& settings);
	unsigned get_reset_support();
	core_setting_group& get_settings();
	bool set_region(core_region& region) { return core->set_region(region); }
	std::pair<uint32_t, uint32_t> get_video_rate() { return core->get_video_rate(); }
	std::pair<uint32_t, uint32_t> get_audio_rate() { return core->get_audio_rate(); }
	std::pair<uint32_t, uint32_t> get_snes_rate() { return core->get_snes_rate(); }
	std::string get_core_identifier() { return core->get_core_identifier(); }
	std::map<std::string, std::vector<char>> save_sram() throw(std::bad_alloc) { return core->save_sram(); }
	void load_sram(std::map<std::string, std::vector<char>>& sram) throw(std::bad_alloc)
	{
		core->load_sram(sram);
	}
	void serialize(std::vector<char>& out) { core->serialize(out); }
	void unserialize(const char* in, size_t insize) { core->unserialize(in, insize); }
private:
	core_type(const core_type&);
	core_type& operator=(const core_type&);
	int (*loadimg)(core_romimage* images, std::map<std::string, std::string>& settings, uint64_t rtc_sec,
		uint64_t rtc_subsec);
	controller_set (*_controllerconfig)(std::map<std::string, std::string>& settings);
	unsigned id;
	unsigned reset_support;
	std::string iname;
	std::string hname;
	std::string biosname;
	std::list<std::string> extensions;
	std::list<core_region*> regions;
	std::vector<core_romimage_info*> imageinfo;
	core_setting_group* settings;
	core_core* core;
};

struct core_sysregion
{
public:
	core_sysregion(const std::string& name, core_type& type, core_region& region);
	static core_sysregion& lookup(const std::string& name);
	const std::string& get_name();
	core_region& get_region();
	core_type& get_type();
	void fill_framerate_magic(uint64_t* magic);	//4 elements filled.
private:
	core_sysregion(const core_sysregion&);
	core_sysregion& operator=(const core_sysregion&);
	std::string name;
	core_type& type;
	core_region& region;
};

#endif
