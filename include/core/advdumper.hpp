#ifndef _advdumper__hpp__included__
#define _advdumper__hpp__included__

#include <string>
#include <set>
#include <stdexcept>

class adv_dumper
{
public:
/**
 * Register a dumper.
 *
 * Parameter id: The ID of dumper.
 * Throws std::bad_alloc: Not enough memory.
 */
	adv_dumper(const std::string& id) throw(std::bad_alloc);
/**
 * Unregister a dumper.
 */
	~adv_dumper();
/**
 * Get ID of dumper.
 *
 * Returns: The id.
 */
	const std::string& id() throw();
/**
 * Get set of all dumpers.
 *
 * Returns: The set.
 * Throws std::bad_alloc: Not enough memory.
 */
	static std::set<adv_dumper*> get_dumper_set() throw(std::bad_alloc);
/**
 * List all valid submodes.
 *
 * Returns: List of all valid submodes. Empty list means this dumper has no submodes.
 * Throws std::bad_alloc: Not enough memory.
 */
	virtual std::set<std::string> list_submodes() throw(std::bad_alloc) = 0;
/**
 * Does this dumper want a prefix?
 *
 * parameter mode: The submode.
 */
	virtual bool wants_prefix(const std::string& mode) throw() = 0;
/**
 * Get human-readable name for this dumper.
 *
 * Returns: The name.
 * Throws std::bad_alloc: Not enough memory.
 */
	virtual std::string name() throw(std::bad_alloc) = 0;
/**
 * Get human-readable name for submode.
 *
 * Parameter mode: The submode.
 * Returns: The name.
 * Throws std::bad_alloc: Not enough memory.
 */
	virtual std::string modename(const std::string& mode) throw(std::bad_alloc) = 0;
/**
 * Is this dumper busy dumping?
 *
 * Return: True if busy, false if not.
 */
	virtual bool busy() = 0;
/**
 * Start dump.
 *
 * parameter mode: The mode to dump using.
 * parameter targetname: The target filename or prefix.
 * Throws std::bad_alloc: Not enough memory.
 * Throws std::runtime_error: Can't start dump.
 */
	virtual void start(const std::string& mode, const std::string& targetname) throw(std::bad_alloc,
		std::runtime_error) = 0;
/**
 * End current dump. 
 */
	virtual void end() throw() = 0;
private:
	std::string d_id;
};

#endif
