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
#include "text.hpp"


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
		instance(const text& id) throw();
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
 * State block for emergency save.
 */
	struct esave_state
	{
		esave_state()
		{
			initialized = false;
		}
		void init(const std::set<std::pair<instance, instance>>& obj)
		{
			if(initialized) return;
			initialized = true;
			itr = obj.begin();
			eitr = obj.end();
		}
		std::set<std::pair<instance, instance>>::const_iterator next()
		{
			if(itr == eitr) return itr;
			return itr++;
		}
		bool finished()
		{
			return (itr == eitr);
		}
		void reset()
		{
			initialized = false;
			segptr = segend = pred = instance();
		}
		instance segptr;
		instance segend;
		instance pred;
	private:
		bool initialized;
		std::set<std::pair<instance, instance>>::const_iterator itr;
		std::set<std::pair<instance, instance>>::const_iterator eitr;
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
	void read_base(const text& projectfile, bool lazy) throw(std::bad_alloc);
/**
 * Is lazy?
 */
	bool is_lazy() throw() { return lazy_mode; }
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
 * Write compressed representation of current load ID set to stream.
 *
 * parameter strm: The stream to write to.
 * returns: Rerecord count.
 * throws std::bad_alloc: Not enough memory.
 */
	uint64_t write(std::vector<char>& strm) throw(std::bad_alloc);
/**
 * Get size for compressed representation.
 *
 * Returns: The size of compressed representation.
 * Note: Uses no memory.
 */
	uint64_t size_emerg() const throw();
/**
 * Write part of compressed representation to buffer.
 *
 * Parameter state: The state variable (initially, pass esave_state()).
 * Parameter buf: The buffer to write to.
 * Parameter bufsize: The buffer size (needs to be at least 36).
 * Returns: The number of bytes written (0 if at the end).
 * Note: Uses no memory.
 */
	size_t write_emerg(struct esave_state& state, char* buf, size_t bufsize) const throw();
/**
 * Load compressed representation of load ID set from stream and union it with current set to form new current
 * set.
 *
 * parameter strm: The stream to read from.
 * parameter dummy: If true, don't actually do it, just simulate.
 * returns: Rerecord count.
 * throws std::bad_alloc: Not enough memory.
 */
	uint64_t read(std::vector<char>& strm) throw(std::bad_alloc);
/**
 * Load compressed representation of load ID set from stream, but don't do anything to it.
 *
 * parameter strm: The stream to read from.
 * returns: Rerecord count.
 * throws std::bad_alloc: Not enough memory.
 */
	static uint64_t count(std::vector<char>& strm) throw(std::bad_alloc);
/**
 * Count number of rerecords.
 *
 * returns: Rerecord count.
 */
	uint64_t count() throw();
/**
 * Debugging functions.
 */
	text debug_dump();
	bool debug_add(const instance& b) { return _add(b); }
	void debug_add(const instance& b, const instance& e) { return _add(b, e); }
	bool debug_in_set(const instance& b) { return _in_set(b); }
	bool debug_in_set(const instance& b, const instance& e) { return _in_set(b, e); }
	uint64_t debug_nodecount(std::set<std::pair<instance, instance>>& set);
private:
	bool _add(const instance& b);
	void _add(const instance& b, const instance& e);
	void _add(const instance& b, const instance& e, std::set<std::pair<instance, instance>>& set,
		uint64_t& cnt);
	bool _in_set(const instance& b) { return _in_set(b, b + 1); }
	bool _in_set(const instance& b, const instance& e);
	uint64_t emerg_action(struct esave_state& state, char* buf, size_t bufsize, uint64_t& scount) const;

	std::set<std::pair<instance, instance>> data;
	std::ofstream ohandle;
	bool handle_open;
	text current_projectfile;
	bool lazy_mode;
	uint64_t rcount;
};

std::ostream& operator<<(std::ostream& os, const struct rrdata_set::instance& j);

#endif
