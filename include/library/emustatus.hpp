#ifndef _library__emustatus__hpp__included__
#define _library__emustatus__hpp__included__

#include "threadtypes.hpp"

#include <map>
#include <string>

class emulator_status
{
public:
/**
 * Constructor.
 *
 * Throws std::bad_alloc: Not enough memory.
 */
	emulator_status() throw(std::bad_alloc);
/**
 * Destructor
 */
	~emulator_status() throw();
/**
 * Insert/Replace key.
 *
 * Parameter key: Key to insert/replace.
 * Parameter value: The value to assign.
 * Throws std::bad_alloc: Not enough memory.
 */
	void set(const std::string& key, const std::string& value) throw(std::bad_alloc);
/**
 * Has key?
 *
 * Parameter key: Key to check.
 * Returns: True if key exists, false if not.
 */
	bool haskey(const std::string& key) throw();
/**
 * Erase key.
 *
 * Parameter key: Key to erase.
 */
	void erase(const std::string& key) throw();
/**
 * Read key.
 *
 * Parameter key: The key to read.
 * Returns: The value of key ("" if not found).
 */
	std::string get(const std::string& key) throw(std::bad_alloc);
/**
 * Iterator.
 */
	struct iterator
	{
/**
 * Not valid flag.
 */
		bool not_valid;
/**
 * Key.
 */
		std::string key;
/**
 * Value.
 */
		std::string value;
	};
/**
 * Get first iterator
 *
 * Returns: Before-the-start iterator.
 * Throws std::bad_alloc: Not enough memory.
 */
	iterator first() throw(std::bad_alloc);
/**
 * Get next value.
 *
 * Parameter itr: Iterator to advance.
 * Returns: True if next value was found, false if not.
 * Throws std::bad_alloc: Not enough memory.
 */
	bool next(iterator& itr) throw(std::bad_alloc);
private:
	emulator_status(const emulator_status&);
	emulator_status& operator=(const emulator_status&);
	mutex_class lock;
	std::map<std::string, std::string> content;
};

#endif
