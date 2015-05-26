#ifndef _misc__hpp__included__
#define _misc__hpp__included__

#include <string>
#include <vector>
#include <stdexcept>
#include "library/string.hpp"
#include "library/memtracker.hpp"

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

/**
 * Clean up filename from dangerous chars
 */
std::string safe_filename(const std::string& str);

/**
 * Mangle some characters ()|/
 */
std::string mangle_name(const std::string& orig);

/**
 * Return a new temporary file. The file will be created.
 */
std::string get_temp_file();

memtracker& mem_tracker();

#endif
