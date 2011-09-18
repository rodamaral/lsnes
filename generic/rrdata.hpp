#ifndef _rrdata__hpp__included__
#define _rrdata__hpp__included__

#define RRDATA_BYTES 32
#include <cstdint>
#include <stdexcept>

/**
 * Set of load IDs
 */
class rrdata
{
public:
/**
 * One load ID.
 */
	struct instance
	{
/**
 * Create new random load ID.
 *
 * throws std::bad_alloc: Not enough memory
 */
		instance() throw(std::bad_alloc);
/**
 * Create new load id from bytes.
 *
 * parameter b: 32 byte array containing the new ID.
 */
		instance(unsigned char* b) throw();
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
/**
 * Is this ID equal to another one?
 *
 * parameter i: Another ID.
 * returns: True if this ID is equal to another one, false otherwise.
 */
		bool operator==(const struct instance& i) const throw();
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
	};
/**
 * Read the saved set of load IDs for specified project and switch to that project.
 *
 * parameter project: The name of project.
 * throws std::bad_alloc: Not enough memory
 */
	static void read_base(const std::string& project) throw(std::bad_alloc);
/**
 * Switch to no project, closing the load IDs.
 */
	static void close() throw();
/**
 * Add new specified instance to current project.
 *
 * Not allowed if there is no project open.
 *
 * parameter i: The load ID to add.
 */
	static void add(const struct instance& i) throw(std::bad_alloc);
/**
 * Generate new load ID and add it to the current proejct.
 *
 * Not allowed if there is no project open.
 *
 * throws std::bad_alloc: Not enough memory.
 */
	static void add_internal() throw(std::bad_alloc);
/**
 * Write compressed representation of current load ID set to stream.
 *
 * parameter strm: The stream to write to.
 * throws std::bad_alloc: Not enough memory.
 */
	static uint64_t write(std::ostream& strm) throw(std::bad_alloc);
/**
 * Load compressed representation of load ID set from stream and union it with current set to form new current
 * set.
 *
 * parameter strm: The stream to read from.
 * throws std::bad_alloc: Not enough memory.
 */
	static uint64_t read(std::istream& strm) throw(std::bad_alloc);
/**
 * Internal pointer used by add_internal.
 */
	static struct instance* internal;
};

/**
 * Print load ID. Mainly useful for deubugging.
 *
 * parameter os: Stream to print to.
 * parameter i: load ID to print.
 */
std::ostream& operator<<(std::ostream& os, const struct rrdata::instance& i);

#endif