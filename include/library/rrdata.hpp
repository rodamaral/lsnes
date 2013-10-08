#ifndef _library_rrdata__hpp__included__
#define _library_rrdata__hpp__included__

#define RRDATA_BYTES 32
#include <cstdint>
#include <stdexcept>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <set>


class rrdata_set
{
public:
	struct instance
	{
/**
 * Create new all zero load ID.
 */
		instance() throw();
/**
 * Create new load id from bytes.
 *
 * parameter b: 32 byte array containing the new ID.
 */
		instance(const unsigned char* b) throw();
/**
 * Create load id from string (mainly intended for debugging).
 */
		instance(const std::string& id) throw();
/**
 * The load ID.
 */
		unsigned char bytes[RRDATA_BYTES];
/**
 * Is this ID before another one?
 *
 * parameter i: Another ID.
 * returns: True if this ID is before another one, false otherwise.
 */
		bool operator<(const struct instance& i) const throw();
		bool operator<=(const struct instance& i) const throw() { return !(i < *this); }
		bool operator>=(const struct instance& i) const throw() { return !(*this < i); }
		bool operator>(const struct instance& i) const throw() { return (i < *this); }
/**
 * Is this ID equal to another one?
 *
 * parameter i: Another ID.
 * returns: True if this ID is equal to another one, false otherwise.
 */
		bool operator==(const struct instance& i) const throw();
		bool operator!=(const struct instance& i) const throw() { return !(*this == i); }
/**
 * Increment this ID.
 *
 * returns: Copy of the ID before the increment.
 */
		const struct instance operator++(int) throw();
/**
 * Increment this ID.
 *
 * returns: Reference to this.
 */
		struct instance& operator++() throw();
/**
 * Increment this ID by specified amount.
 *
 * returns: The incremented id.
 */
		struct instance operator+(unsigned inc) const throw();
/**
 * Difference.
 *
 * Returns: The difference, or UINT_MAX if too great.
 */
		unsigned operator-(const struct instance& m) const throw();
	};
/**
 * Ctor
 */
	rrdata_set() throw();
/**
 * Read the saved set of load IDs for specified project and switch to that project.
 *
 * parameter projectfile: The name of project backing file.
 * parameter lazy: If true, just switch to project, don't read the IDs.
 * throws std::bad_alloc: Not enough memory
 */
	void read_base(const std::string& projectfile, bool lazy) throw(std::bad_alloc);
/**
 * Switch to no project, closing the load IDs.
 */
	void close() throw();
/**
 * Add new specified instance to current project.
 *
 * Not allowed if there is no project open.
 *
 * parameter i: The load ID to add.
 */
	void add(const struct instance& i) throw(std::bad_alloc);
/**
 * Add internal instance, doing post-increment.
 */
	void add_internal() throw(std::bad_alloc);
/**
 * Set internal instance.
 *
 * Parameter b: The new instance.
 */
	void set_internal(const instance& b) throw();
/**
 * Write compressed representation of current load ID set to stream.
 *
 * parameter strm: The stream to write to.
 * returns: Rerecord count.
 * throws std::bad_alloc: Not enough memory.
 */
	uint64_t write(std::vector<char>& strm) throw(std::bad_alloc);
/**
 * Load compressed representation of load ID set from stream and union it with current set to form new current
 * set.
 *
 * parameter strm: The stream to read from.
 * parameter dummy: If true, don't actually do it, just simulate.
 * returns: Rerecord count.
 * throws std::bad_alloc: Not enough memory.
 */
	uint64_t read(std::vector<char>& strm, bool dummy = false) throw(std::bad_alloc);
/**
 * Load compressed representation of load ID set from stream, but don't do anything to it.
 *
 * parameter strm: The stream to read from.
 * returns: Rerecord count.
 * throws std::bad_alloc: Not enough memory.
 */
	uint64_t count(std::vector<char>& strm) throw(std::bad_alloc);
/**
 * Count number of rerecords.
 *
 * returns: Rerecord count.
 */
	uint64_t count() throw();
/**
 * Debugging functions.
 */
	std::string debug_dump();
	bool debug_add(const instance& b) { return _add(b); }
	void debug_add(const instance& b, const instance& e) { return _add(b, e); }
	bool debug_in_set(const instance& b) { return _in_set(b); }
	bool debug_in_set(const instance& b, const instance& e) { return _in_set(b, e); }
private:
	bool _add(const instance& b);
	void _add(const instance& b, const instance& e);
	bool _in_set(const instance& b) { return _in_set(b, b + 1); }
	bool _in_set(const instance& b, const instance& e);
	instance internal;
	std::set<std::pair<instance, instance>> data;
	std::ofstream ohandle;
	bool handle_open;
	std::string current_projectfile;
	bool lazy_mode;
	uint64_t rcount;
};

std::ostream& operator<<(std::ostream& os, const struct rrdata_set::instance& j);

#endif
