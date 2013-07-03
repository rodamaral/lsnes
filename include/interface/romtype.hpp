#ifndef _interface__romtype__hpp__included__
#define _interface__romtype__hpp__included__

#include <list>
#include <string>
#include <cstdint>
#include <vector>
#include "interface/controller.hpp"
#include "interface/setting.hpp"
#include "library/framebuffer.hpp"
#include "library/threadtypes.hpp"

struct core_region;
struct core_type;
struct core_sysregion;
struct core_romimage;
struct core_romimage_info;
struct core_core;

/**
 * Interface device register.
 */
struct interface_device_reg
{
	const char* name;
	uint64_t (*read)();
	void (*write)(uint64_t v);
	bool boolean;
};

/**
 * An parameter for action in interface.
 */
struct interface_action_param
{
	//Name of the action parameter.
	const char* name;
	//Model of the action parameter.
	//bool: Boolean.
	//int:<val>,<val>: Integer in specified range.
	//string[:<regex>]: String with regex.
	const char* model;
};

/**
 * Value for action in interface.
 */
struct interface_action_paramval
{
	std::string s;
	int64_t i;
	bool b;
};

/**
 * An action for interface.
 */
struct interface_action
{
	interface_action(struct core_core& _core, unsigned id, const std::string& title, const std::string& symbol,
		std::initializer_list<interface_action_param> p);
	~interface_action();
	unsigned id;
	std::string title;
	std::string symbol;
	std::list<interface_action_param> params;
	struct core_core& core;
};


/**
 * Parameters about a region (e.g. NTSC, PAL, World).
 *
 * Both movies ("run region") and ROMs ("cart region") have regions. The set of regions allowed for latter is superset
 * of former, since autodetection regions are allowed for carts but not runs.
 */
struct core_region_params
{
/**
 * Internal name of the region. Should be all-lowercase.
 */
	const char* iname;
/**
 * Human-readable name of the region.
 */
	const char* hname;
/**
 * Priority of region. Higher numbers are more preferred when tiebreaking.
 */
	unsigned priority;
/**
 * ID number of region. Unique among regions allowed for given core type.
 */
	unsigned handle;
/**
 * Multi-region flag. If set, this region gets further resolved by autodetection.
 */
	bool multi;
/**
 * Nominal framerate. The first number is numerator, the second is denominator.
 */
	uint64_t framemagic[2];
/**
 * ID numbers of regions of runs compatible with this cartridge region.
 */
	std::vector<unsigned> compatible_runs;
};

/**
 * Parameters about individual ROM image.
 */
struct core_romimage_info_params
{
/**
 * Internal name of ROM image.
 */
	const char* iname;
/**
 * Human-readable name of ROM image.
 */
	const char* hname;
/**
 * Upon loading a set of ROM images, the OR of mandatory fields of all present ROM images must equal OR of mandatory
 * fields of all possible ROM images.
 */
	unsigned mandatory;
/**
 * The way image is passed:
 * 0 => Pass by content.
 * 1 => Pass by filename.
 * 2 => Pass by directory.
 */
	int pass_mode;
/**
 * Size of optional copier header to remove. 0 means there never is copier header.
 */
	unsigned headersize;
};

/**
 * A Virtual Memory Area (VMA), which is a chunk of lsnes memory space.
 *
 * Mapping stuff above 4PB should be avoided.
 */
struct core_vma_info
{
/**
 * Name of the VMA.
 */
	std::string name;
/**
 * Base address of the VMA.
 */
	uint64_t base;
/**
 * Size of the VMA.
 */
	uint64_t size;
/**
 * Direct backing RAM for the VMA. May be NULL.
 */
	void* backing_ram;
/**
 * If true, the VMA can't be written to.
 */
	bool readonly;
/**
 * Default endianess. -1 => Little endian, 0 => The same as host system, 1 => Big endian.
 */
	int endian;
/**
 * If backing_ram is NULL, this routine is used to access the memory one byte at a time.
 *
 * Parameter offset: The offset into VMA to access.
 * Parameter data: Byte to write. Ignored if write = false.
 * Parameter write: If true, do write, otherwise do read.
 * Returns: The read value. Only valid if write = false.
 */
	uint8_t (*iospace_rw)(uint64_t offset, uint8_t data, bool write);
};

/**
 * Parameters about system type.
 *
 * Each system type may have its own regions, image slots and controller configuration.
 */
struct core_type_params
{
/**
 * Internal name of the system type.
 */
	const char* iname;
/**
 * Human-readable name of the system type.
 */
	const char* hname;
/**
 * ID of system type. Must be unique among core system is valid for.
 */
	unsigned id;
/**
 * System menu name.
 */
	const char* sysname;
/**
 * Load a ROM slot set. Changes the ROM currently loaded for core.
 *
 * Parameter images: The set of images to load.
 * Parameter settings: The settings to use.
 * Parameter rtc_sec: The initial RTC seconds value.
 * Parameter rtc_subsec: The initial RTC subseconds value.
 * Returns: -1 on failure, 0 on success.
 */
	int (*load_rom)(core_romimage* images, std::map<std::string, std::string>& settings, uint64_t rtc_sec,
		uint64_t rtc_subsec);
/**
 * Obtain controller config for given settings.
 *
 * Parameter settings: The settings to use.
 * Returns: The controller configuration.
 */
	controller_set (*controllerconfig)(std::map<std::string, std::string>& settings);
/**
 * Semicolon-separated list of extensions this system type uses.
 */
	const char* extensions;
/**
 * The name of BIOS for this system. NULL if there is no bios.
 */
	const char* bios;
/**
 * List of regions valid for this system type.
 */
	std::vector<core_region*> regions;
/**
 * List of image slots for this system type.
 */
	std::vector<core_romimage_info*> images;
/**
 * Description of settings for this system type.
 */
	core_setting_group* settings;
/**
 * Core this system is emulated by.
 */
	core_core* core;
/**
 * Get bus mapping.
 *
 * Returns: The bus mapping (base,size), or (0,0) if this system does not have bus mapping.
 */
	std::pair<uint64_t, uint64_t> (*get_bus_map)();
/**
 * Get list of valid VMAs. ROM must be loaded.
 *
 * Returns: The list of VMAs.
 */
	std::list<core_vma_info> (*vma_list)();
/**
 * Get list of valid SRAM names. ROM must be loaded.
 *
 * Returns: The list of SRAMs.
 */
	std::set<std::string> (*srams)();
};

/**
 * Core type parameters.
 *
 * Each core type has its own loaded ROM.
 */
struct core_core_params
{
/**
 * Get the name of the core.
 */
	std::string (*core_identifier)();
/**
 * Set the current region.
 *
 * Parameter region: The new region.
 * Returns: True on success, false on failure (bad region).
 */
	bool (*set_region)(core_region& region);
/**
 * Get current video frame rate as (numerator, denominator).
 */
	std::pair<uint32_t, uint32_t> (*video_rate)();
/**
 * Get audio sampling rate as (numerator, denominator).
 *
 * Note: This value should not be changed while ROM is running, since video dumper may malfunction.
 */
	std::pair<uint32_t, uint32_t> (*audio_rate)();
/**
 * Get SNES CPU and SMP rates. If not SNES, this should be NULL.
 */
	std::pair<uint32_t, uint32_t> (*snes_rate)();
/**
 * Save all SRAMs.
 */
	std::map<std::string, std::vector<char>> (*save_sram)() throw(std::bad_alloc);
/**
 * Load all SRAMs.
 *
 * Note: Must handle SRAM being missing or shorter or longer than expected.
 */
	void (*load_sram)(std::map<std::string, std::vector<char>>& sram) throw(std::bad_alloc);
/**
 * Serialize the system state.
 */
	void (*serialize)(std::vector<char>& out);
/**
 * Unserialize the system state.
 */
	void (*unserialize)(const char* in, size_t insize);
/**
 * Get current region.
 */
	core_region& (*get_region)();
/**
 * Poweron the console.
 */
	void (*power)();
/**
 * Unload the cartridge from the console.
 */
	void (*unload_cartridge)();
/**
 * Get the current scale factors for screen as (xscale, yscale).
 */
	std::pair<uint32_t, uint32_t> (*get_scale_factors)(uint32_t width, uint32_t height);
/**
 * Do basic core initialization. Called on lsnes startup.
 */
	void (*install_handler)();
/**
 * Do basic core uninitialization. Called on lsnes shutdown.
 */
	void (*uninstall_handler)();
/**
 * Emulate one frame.
 */
	void (*emulate)();
/**
 * Get core into state where saving is possible. Must run less than one frame.
 */
	void (*runtosave)();
/**
 * Get the polled flag.
 *
 * The emulator core sets polled flag when the game asks for input.
 *
 * If polled flag is clear when frame ends, the frame is marked as lag.
 */
	bool (*get_pflag)();
/**
 * Set the polled flag.
 */
	void (*set_pflag)(bool pflag);
/**
 * Set of valid port types for the core.
 */
	std::vector<port_type*> port_types;
/**
 * Draw run cover screen.
 *
 * Should display information about the ROM loaded.
 */
	framebuffer_raw& (*draw_cover)();
/**
 * Get shortened name of the core.
 */
	std::string (*get_core_shortname)();
/**
 * Set the system controls to appropriate values for next frame.
 *
 * E.g. if core supports resetting, set the reset button in the frame to pressed if reset is wanted.
 */
	void (*pre_emulate_frame)(controller_frame& cf);
/**
 * Execute action.
 */
	void (*execute_action)(unsigned id, const std::vector<interface_action_paramval>& p);
/**
 * Get set of interface device registers.
 */
	const struct interface_device_reg* (*get_registers)();
};

struct core_region
{
public:
	core_region(const core_region_params& params);
	core_region(std::initializer_list<core_region_params> p) : core_region(*p.begin()) {}
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
	core_romimage_info(std::initializer_list<core_romimage_info_params> p) : core_romimage_info(*p.begin()) {};
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
	core_core(const core_core_params& params);
	core_core(std::initializer_list<core_core_params> p) : core_core(*p.begin()) {}
	~core_core() throw();
	bool set_region(core_region& region);
	std::pair<uint32_t, uint32_t> get_video_rate();
	std::pair<uint32_t, uint32_t> get_audio_rate();
	std::pair<uint32_t, uint32_t> get_snes_rate();	//(0,0) for non-SNES.
	std::string get_core_identifier();
	std::map<std::string, std::vector<char>> save_sram() throw(std::bad_alloc);
	void load_sram(std::map<std::string, std::vector<char>>& sram) throw(std::bad_alloc);
	void serialize(std::vector<char>& out);
	void unserialize(const char* in, size_t insize);
	core_region& get_region();
	void power();
	void unload_cartridge();
	std::pair<uint32_t, uint32_t> get_scale_factors(uint32_t width, uint32_t height);
	void install_handler();
	void uninstall_handler();
	void emulate();
	void runtosave();
	bool get_pflag();
	void set_pflag(bool pflag);
	framebuffer_raw& draw_cover();
	std::vector<port_type*> get_port_types() { return port_types; }
	std::string get_core_shortname();
	void pre_emulate_frame(controller_frame& cf);
	void execute_action(unsigned id, const std::vector<interface_action_paramval>& p);
	static std::set<core_core*> all_cores();
	static void install_all_handlers();
	static void uninstall_all_handlers();
	void hide() { hidden = true; }
	bool is_hidden() { return hidden; }
	struct _param_register_proxy
	{
		_param_register_proxy(core_core& _c) : cre(_c) {}
		void do_register(const std::string& key, interface_action& act)
		{
			cre.do_register_action(key, act);
		}
		void do_unregister(const std::string& key)
		{
			cre.do_unregister_action(key);
		}
	private:
		core_core& cre;
	};
	void do_register_action(const std::string& key, interface_action& act);
	void do_unregister_action(const std::string& key);
	std::set<const interface_action*> get_actions();
	_param_register_proxy param_register_proxy;
	const interface_device_reg* get_registers();
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
	core_region& (*_get_region)();
	void (*_power)();
	void (*_unload_cartridge)();
	std::pair<uint32_t, uint32_t> (*_get_scale_factors)(uint32_t width, uint32_t height);
	void (*_install_handler)();
	void (*_uninstall_handler)();
	void (*_emulate)();
	void (*_runtosave)();
	bool (*_get_pflag)();
	void (*_set_pflag)(bool pflag);
	std::vector<port_type*> port_types;
	framebuffer_raw& (*_draw_cover)();
	std::string (*_get_core_shortname)();
	void (*_pre_emulate_frame)(controller_frame& cf);
	void (*_execute_action)(unsigned id, const std::vector<interface_action_paramval>& p);
	const interface_device_reg* (*_get_registers)();
	bool hidden;
	std::map<std::string, interface_action*> actions;
	mutex_class actions_lock;
};

struct core_type
{
public:
	core_type(const core_type_params& params);
	core_type(std::initializer_list<core_type_params> p) : core_type(*p.begin()) {}
	~core_type() throw();
	static std::list<core_type*> get_core_types();
	core_region& get_preferred_region();
	std::list<core_region*> get_regions();
	core_sysregion& combine_region(core_region& reg);
	const std::string& get_iname();
	const std::string& get_hname();
	const std::list<std::string>& get_extensions();
	bool is_known_extension(const std::string& ext);
	core_sysregion& lookup_sysregion(const std::string& sysreg);
	std::string get_biosname();
	unsigned get_id();
	unsigned get_image_count();
	core_romimage_info get_image_info(unsigned index);
	bool load(core_romimage* images, std::map<std::string, std::string>& settings, uint64_t rtc_sec,
		uint64_t rtc_subsec);
	controller_set controllerconfig(std::map<std::string, std::string>& settings);
	core_setting_group& get_settings();
	std::pair<uint64_t, uint64_t> get_bus_map();
	std::list<core_vma_info> vma_list();
	std::set<std::string> srams();
	core_core* get_core() { return core; }
	bool set_region(core_region& region) { return core->set_region(region); }
	std::pair<uint32_t, uint32_t> get_video_rate() { return core->get_video_rate(); }
	std::pair<uint32_t, uint32_t> get_audio_rate() { return core->get_audio_rate(); }
	std::pair<uint32_t, uint32_t> get_snes_rate() { return core->get_snes_rate(); }
	std::string get_core_identifier() { return core->get_core_identifier(); }
	std::string get_core_shortname() { return core->get_core_shortname(); }
	std::map<std::string, std::vector<char>> save_sram() throw(std::bad_alloc) { return core->save_sram(); }
	void load_sram(std::map<std::string, std::vector<char>>& sram) throw(std::bad_alloc)
	{
		core->load_sram(sram);
	}
	void serialize(std::vector<char>& out) { core->serialize(out); }
	void unserialize(const char* in, size_t insize) { core->unserialize(in, insize); }
	core_region& get_region() { return core->get_region(); }
	void power() { core->power(); }
	void unload_cartridge() { core->unload_cartridge(); }
	std::pair<uint32_t, uint32_t> get_scale_factors(uint32_t width, uint32_t height)
	{
		return core->get_scale_factors(width, height);
	}
	void install_handler() { core->install_handler(); }
	void uninstall_handler() { core->uninstall_handler(); }
	void emulate() { core->emulate(); }
	void runtosave() { core->runtosave(); }
	bool get_pflag() { return core->get_pflag(); }
	void set_pflag(bool pflag) { core->set_pflag(pflag); }
	framebuffer_raw& draw_cover() { return core->draw_cover(); }
	std::string get_systemmenu_name() { return sysname; }
	void execute_action(unsigned id, const std::vector<interface_action_paramval>& p)
	{
		return core->execute_action(id, p);
	}
	bool is_hidden() { return core->is_hidden(); }
	void pre_emulate_frame(controller_frame& cf) { return core->pre_emulate_frame(cf); }
	std::set<const interface_action*> get_actions() { return core->get_actions(); }
	const interface_device_reg* get_registers() { return core->get_registers(); }
private:
	core_type(const core_type&);
	core_type& operator=(const core_type&);
	int (*loadimg)(core_romimage* images, std::map<std::string, std::string>& settings, uint64_t rtc_sec,
		uint64_t rtc_subsec);
	controller_set (*_controllerconfig)(std::map<std::string, std::string>& settings);
	std::pair<uint64_t, uint64_t> (*_get_bus_map)();
	std::list<core_vma_info> (*_vma_list)();
	std::set<std::string> (*_srams)();
	unsigned id;
	std::string iname;
	std::string hname;
	std::string biosname;
	std::string sysname;
	std::list<std::string> extensions;
	std::list<core_region*> regions;
	std::vector<core_romimage_info*> imageinfo;
	core_setting_group* settings;
	core_core* core;
};

/**
 * System type / region pair.
 *
 * All run regions for given system must have valid pairs.
 */
struct core_sysregion
{
public:
/**
 * Create a new system type / region pair.
 *
 * Parameter name: The internal name of the pair.
 * Parameter type: The system.
 * Parameter region: The region.
 */
	core_sysregion(const std::string& name, core_type& type, core_region& region);
	~core_sysregion() throw();
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

//Set to true if new core is detected.
extern bool new_core_flag;

#endif
