#ifndef _library__memorywatch__hpp__included__
#define _library__memorywatch__hpp__included__

#include "mathexpr.hpp"
#include <functional>
#include <list>
#include <set>
#include <map>

class memory_space;

namespace memorywatch
{
/**
 * Read memory operator.
 */
struct memread_oper : public mathexpr::operinfo
{
/**
 * Ctor
 */
	memread_oper();
/**
 * Dtor
 */
	~memread_oper();
/**
 * Evaluate the operator.
 *
 * Note: The first promise is for the address.
 */
	void evaluate(mathexpr::value target, std::vector<std::function<mathexpr::value()>> promises);
	//Fields.
	unsigned bytes;		//Number of bytes to read.
	bool signed_flag;	//Is signed?
	bool float_flag;	//Is float?
	int endianess;		//Endianess (-1 => little, 0 => host, 1 => Big).
	uint64_t scale_div;	//Scale divisor.
	uint64_t addr_base;	//Address base.
	uint64_t addr_size;	//Address size (0 => All).
	memory_space* mspace;	//Memory space to read.
};

/**
 * Memory watch item printer.
 */
struct item_printer : public GC::item
{
/**
 * Dtor.
 */
	virtual ~item_printer();
/**
 * Show the watched value.
 */
	virtual void show(const std::string& iname, const std::string& val) = 0;
/**
 * Reset the printer.
 */
	virtual void reset() = 0;
protected:
	void trace();
};

/**
 * Memory watch item.
 */
struct item
{
/**
 * Ctor.
 *
 * Parameter t: The type of the result.
 */
	item(mathexpr::typeinfo& t)
		: expr(GC::obj_tag(), &t)
	{
	}
/**
 * Get the value as string.
 */
	std::string get_value();
/**
 * Print the value to specified printer.
 *
 * Parameter iname: The name of the watch.
 */
	void show(const std::string& iname);
	//Fields.
	GC::pointer<item_printer> printer;		//Printer to use.
	GC::pointer<mathexpr::mathexpr> expr;	//Expression to watch.
	std::string format;				//Formatting to use.
};

/**
 * A set of memory watches.
 */
struct set
{
/**
 * Dtor.
 */
	~set();
/**
 * Call reset on all items and their printers in the set.
 */
	void reset();
/**
 * Call reset and then show on all items in the set.
 */
	void refresh();
/**
 * Get the longest name (by UTF-8 length) in the set.
 *
 * Returns: The longest nontrivial name, or "" if none.
 */
	const std::string& get_longest_name()
	{
		return get_longest_name(set::utflength_rate);
	}
/**
 * Get the longest name (by arbitrary function) in the set.
 *
 * Parameter rate: Get length for name function.
 * Returns: The longest nontrivial name, or "" if none.
 */
	const std::string& get_longest_name(std::function<size_t(const std::string& n)> rate);
/**
 * Get the set of memory watch names.
 */
	std::set<std::string> names_set();
/**
 * Get specified memory watch item.
 *
 * Parameter name: The name of the item.
 * Returns: The item.
 * Throws std::runtime_error: No such item in set.
 */
	item& get(const std::string& name);
/**
 * Get specified memory watch item (without throwing).
 *
 * Parameter name: The name of the item.
 * Returns: The item, or NULL if no such item exists.
 */
	item* get_soft(const std::string& name);
/**
 * Create a new memory watch item.
 *
 * Parameter name: The name of the new item.
 * Parameter item: The new item. All fields are shallow-copied.
 */
	item* create(const std::string& name, item& item);
/**
 * Destroy a memory watch item.
 *
 * Parameter name: The name of the item to destroy.
 */
	void destroy(const std::string& name);
/**
 * Call routine for all roots.
 */
	void foreach(std::function<void(item& item)> cb);
/**
 * Swap set with another.
 */
	void swap(set& s) throw();
private:
	static size_t utflength_rate(const std::string& s);
	std::map<std::string, item> roots;
};
}

#endif
