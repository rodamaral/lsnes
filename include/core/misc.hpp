#ifndef _misc__hpp__included__
#define _misc__hpp__included__

#include <string>
#include <vector>
#include <stdexcept>
#include <boost/lexical_cast.hpp>

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
std::ostream& _messages();
#define messages _messages()

/**
 * \brief Typeconvert string.
 */
template<typename T> inline T parse_value(const std::string& value) throw(std::bad_alloc, std::runtime_error)
{
	try {
		//Hack, since lexical_cast lets negative values slip through.
		if(!std::numeric_limits<T>::is_signed && value.length() && value[0] == '-') {
			throw std::runtime_error("Unsigned values can't be negative");
		}
		return boost::lexical_cast<T>(value);
	} catch(std::exception& e) {
		throw std::runtime_error("Can't parse value '" + value + "': " + e.what());
	}
}

template<> inline std::string parse_value(const std::string& value) throw(std::bad_alloc, std::runtime_error)
{
	return value;
}

uint32_t gcd(uint32_t a, uint32_t b) throw();

void create_lsnesrc();

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
