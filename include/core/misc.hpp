#ifndef _misc__hpp__included__
#define _misc__hpp__included__

#include <string>
#include <vector>
#include <stdexcept>
#include "library/string.hpp"

/**
 * \brief Get random hexes
 *
 * Get string of random hex characters of specified length.
 *
 * \param length The number of hex characters to return.
 * \return The random hexadecimal string.
 * \throws std::bad_alloc Not enough memory.
 */
std::string get_random_hexstring(size_t length) throw(std::bad_alloc);

/**
 * \brief Set random seed
 *
 * This function sets the random seed to use.
 *
 * \param seed The value to use as seed.
 * \throw std::bad_alloc Not enough memory.
 */
void set_random_seed(const std::string& seed) throw(std::bad_alloc);

/**
 * \brief Set random seed to (hopefully) unique value
 *
 * This function sets the random seed to value that should only be used once. Note, the value is not necressarily
 * crypto-secure, even if it is unique.
 *
 * \throw std::bad_alloc Not enough memory.
 */
void set_random_seed() throw(std::bad_alloc);

/**
 * \brief Load a ROM.
 *
 * Given commandline arguments, load a ROM.
 *
 * \param cmdline The command line.
 * \return The loaded ROM set.
 * \throws std::bad_alloc Not enough memory.
 * \throws std::runtime_error Can't load the ROMset.
 */
struct loaded_rom load_rom_from_commandline(std::vector<std::string> cmdline) throw(std::bad_alloc,
	std::runtime_error);

/**
 * \brief Dump listing of regions to graphics system messages.
 *
 * \throws std::bad_alloc Not enough memory.
 */
void dump_region_map() throw(std::bad_alloc);

/**
 * \brief Fatal error.
 *
 * Fatal error.
 */
void fatal_error() throw();

/**
 * \brief Get path to config directory.
 *
 * \return The config directory path.
 * \throw std::bad_alloc Not enough memory.
 */
std::string get_config_path() throw(std::bad_alloc);

/**
 * \brief Panic on OOM.
 */
void OOM_panic();

/**
 * messages -> window::out().
 */
class messages_relay_class
{
public:
	operator std::ostream&() { return getstream(); }
	static std::ostream& getstream();
};
template<typename T> inline std::ostream& operator<<(messages_relay_class& x, T value)
{
	return messages_relay_class::getstream() << value;
};
inline std::ostream& operator<<(messages_relay_class& x, std::ostream& (*fn)(std::ostream& o))
{
	return fn(messages_relay_class::getstream());
};
extern messages_relay_class messages;

uint32_t gcd(uint32_t a, uint32_t b) throw();

/**
 * Return hexadecimal representation of address
 */
std::string format_address(void* addr);

/**
 * Get state of running global ctors flag.
 */
bool in_global_ctors();
/**
 * Clear the global ctors flag.
 */
void reached_main();

#endif
