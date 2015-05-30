#ifndef _memorywatch__hpp__included__
#define _memorywatch__hpp__included__

#include <string>
#include <stdexcept>
#include <set>
#include "library/memorywatch.hpp"
#include "library/json.hpp"

class memory_space;
class project_state;
class loaded_rom;
class emu_framebuffer;
namespace framebuffer { class queue; }

/**
 * lsnes memory watch printer variables.
 */
struct memwatch_printer
{
/**
 * Ctor.
 */
	memwatch_printer();
/**
 * Serialize the printer to JSON value.
 */
	JSON::node serialize();
/**
 * Unserialize the printer from JSON value.
 */
	void unserialize(const JSON::node& node);
/**
 * Get a printer object corresponding to this object.
 */
	GC::pointer<memorywatch::item_printer> get_printer_obj(
		std::function<GC::pointer<mathexpr::mathexpr>(const text& n)> vars);
	//Fields.
	enum position_category {
		PC_DISABLED,
		PC_MEMORYWATCH,
		PC_ONSCREEN
	} position;
	bool cond_enable;			//Ignored for disabled.
	text enabled;			//Ignored for disabled.
	text onscreen_xpos;
	text onscreen_ypos;
	bool onscreen_alt_origin_x;
	bool onscreen_alt_origin_y;
	bool onscreen_cliprange_x;
	bool onscreen_cliprange_y;
	text onscreen_font;		//"" is system default.
	int64_t onscreen_fg_color;
	int64_t onscreen_bg_color;
	int64_t onscreen_halo_color;
};

/**
 * lsnes memory watch item.
 */
struct memwatch_item
{
/**
 * Ctor.
 */
	memwatch_item();
/**
 * Serialize the item to JSON value.
 */
	JSON::node serialize();
/**
 * Unserialize the item from JSON value.
 */
	void unserialize(const JSON::node& node);
/**
 * Get memory read operator.
 *
 * If bytes == 0, returns NULL.
 */
	mathexpr::operinfo* get_memread_oper(memory_space& memory, loaded_rom& rom);
/**
 * Translate compatiblity item.
 */
	void compatiblity_unserialize(memory_space& memory, const text& item);
	//Fields
	memwatch_printer printer;	//The printer.
	text expr;			//The main expression.
	text format;			//Format.
	unsigned bytes;				//Number of bytes to read (0 => Not memory read operator).
	bool signed_flag;			//Is signed?
	bool float_flag;			//Is float?
	int endianess;				//Endianess (-1 => little, 0 => host, 1 => Big).
	uint64_t scale_div;			//Scale divisor.
	uint64_t addr_base;			//Address base.
	uint64_t addr_size;			//Address size (0 => All).
};

struct memwatch_set
{
	memwatch_set(memory_space& _memory, project_state& _project, emu_framebuffer& _fbuf,
		loaded_rom& rom);
/**
 * Get the specified memory watch item.
 */
	memwatch_item& get(const text& name);
/**
 * Get the specified memory watch item as JSON serialization.
 *
 * Parameter name: The item name.
 * Parameter printer: JSON pretty-printer to use.
 */
	text get_string(const text& name, JSON::printer* printer = NULL);
/**
 * Set the specified memory watch item. Fills the runtime variables.
 *
 * Parameter name: The name of the new item.
 * Parameter item: The item to insert. Fields are shallow-copied.
 */
	void set(const text& name, memwatch_item& item);
/**
 * Set the specified memory watch item from JSON serialization. Fills the runtime variables.
 *
 * Parameter name: The name of the new item.
 * Parameter item: The serialization of item to insert.
 */
	void set(const text& name, const text& item);
/**
 * Set multiple items at once.
 *
 * Parameter list: The list of items.
 */
	void set_multi(std::list<std::pair<text, memwatch_item>>& list);
/**
 * Set multiple items at once from JSON descriptions.
 *
 * Parameter list: The list of items.
 */
	void set_multi(std::list<std::pair<text, text>>& list);
/**
 * Rename a memory watch item.
 *
 * Parameter oname: The old name.
 * Parameter newname: The new name.
 * Returns: True on success, false if failed (new item already exists).
 */
	bool rename(const text& oname, const text& newname);
/**
 * Delete an item.
 *
 * Parameter name: The name of the item to delete.
 */
	void clear(const text& name);
/**
 * Delete multiple items.
 *
 * Parameter names: The names of the items to delete.
 */
	void clear_multi(const std::set<text>& name);
/**
 * Enumerate item names.
 */
	std::set<text> enumerate();
/**
 * Get value of specified memory watch as a string.
 */
	text get_value(const text& name);
/**
 * Watch all the items.
 *
 * Parameter rq: The render queue to use.
 */
	void watch(struct framebuffer::queue& rq);
/**
 * Get memory watch vars that go to window.
 */
	const std::map<text, text>& get_window_vars() { return window_vars; }
private:
	void rebuild(std::map<text, memwatch_item>& nitems);
	std::map<text, memwatch_item> items;
	std::map<text, text> window_vars;
	std::map<text, bool> used_memorywatches;
	void erase_unused_watches();
	void watch_output(const text& name, const text& value);
	memorywatch::set watch_set;
	memory_space& memory;
	project_state& project;
	emu_framebuffer& fbuf;
	loaded_rom& rom;
};

#endif
