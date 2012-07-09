#ifndef _romtype__hpp__included__
#define _romtype__hpp__included__

#include <set>
#include <string>
#include <cstdint>
#include <vector>

struct core_region;
struct core_type;
struct core_sysregion;

struct core_region
{
public:
	core_region(const std::string& iname, const std::string& hname, unsigned priority, unsigned handle,
		bool multi, uint64_t* fmagic, int (*compatible)(unsigned rom, unsigned run));
	static core_region& lookup(const std::string& name);
	const std::string& get_iname();
	const std::string& get_hname();
	unsigned get_priority();
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
	int (*compatible)(unsigned rom, unsigned run);
};

struct core_romimage_info
{
	core_romimage_info(const std::string& iname, const std::string& hname, unsigned mandatory,
		unsigned (*headersize)(size_t imagesize));
	std::string iname;
	std::string hname;
	unsigned mandatory;
	unsigned (*headersize)(size_t imagesize);
};

struct core_romimage
{
	const char* markup;
	const unsigned char* data;
	size_t size;
};

struct core_type
{
public:
	core_type(const std::string& iname, const std::string& hname, int (*_load)(core_romimage* images));
	static std::set<core_type*> get_core_types();
	void add_region(core_region& reg);
	void add_romimage(core_romimage_info& info, unsigned index);
	core_region& get_preferred_region();
	std::set<core_region*> get_regions();
	core_sysregion& combine_region(core_region& reg);
	const std::string& get_iname();
	const std::string& get_hname();
	unsigned get_image_count();
	core_romimage_info get_image_info(unsigned index);
	bool load(core_romimage* images);
private:
	int (*loadimg)(core_romimage* images);
	core_type(const core_type&);
	core_type& operator=(const core_type&);
	std::string iname;
	std::string hname;
	std::set<core_region*> regions;
	std::vector<core_romimage_info*> imageinfo;
};

struct core_type_region_bind
{
public:
	core_type_region_bind(core_type& type, core_region& region);
private:
	core_type_region_bind(const core_type_region_bind&);
	core_type_region_bind& operator=(const core_type_region_bind&);
};

struct core_type_image_bind
{
public:
	core_type_image_bind(core_type& type, core_romimage_info& imageinfo, unsigned index);
private:
	core_type_image_bind(const core_type_image_bind&);
	core_type_image_bind& operator=(const core_type_image_bind&);
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
