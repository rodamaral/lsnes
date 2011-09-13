#ifndef _misc__hpp__included__
#define _misc__hpp__included__

#include <string>
#include <vector>
#include "window.hpp"
#include <boost/lexical_cast.hpp>

/**
 * \brief Recognize "foo", "foo (anything)" and "foo\t(anything)".
 * 
 * \param haystack The string to search.
 * \param needle The string to find.
 * \return True if found, false if not.
 */
bool is_cmd_prefix(const std::string& haystack, const std::string& needle) throw();

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
 * \param win Handle to send the messages to.
 * \return The loaded ROM set.
 * \throws std::bad_alloc Not enough memory.
 * \throws std::runtime_error Can't load the ROMset.
 */
struct loaded_rom load_rom_from_commandline(std::vector<std::string> cmdline, window* win) throw(std::bad_alloc,
	std::runtime_error);

/**
 * \brief Dump listing of regions to graphics system messages.
 * 
 * \param win Handle to send the messages to.
 * \throws std::bad_alloc Not enough memory.
 */
void dump_region_map(window* win) throw(std::bad_alloc);

/**
 * \brief Return printing stream.
 * 
 * \param win Handle to graphics system.
 * \return Stream. If win is NULL, this is std::cout. Otherwise it is win->out().
 * \throws std::bad_alloc Not enough memory.
 */
std::ostream& out(window* win) throw(std::bad_alloc);

/**
 * \brief Fatal error.
 * 
 * Fatal error. If win is non-NULL, it calls win->fatal_error(). Otherwise just immediately quits with error.
 */
void fatal_error(window* win) throw();

/**
 * \brief Get path to config directory.
 * 
 * \param win Graphics system handle.
 * \return The config directory path.
 * \throw std::bad_alloc Not enough memory.
 */
std::string get_config_path(window* win) throw(std::bad_alloc);

/**
 * \brief Panic on OOM.
 */
void OOM_panic(window* win);

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

void create_lsnesrc(window* win);

/**
 * \brief Opaque internal state of SHA256
 */
struct sha256_opaque;

/**
 * \brief SHA-256 function
 *
 * This class implements interface to SHA-256.
 */
class sha256
{
public:
/**
 * \brief Create new SHA-256 context
 *
 * Creates new SHA-256 context, initially containing empty data.
 */
	sha256() throw(std::bad_alloc);

/**
 * \brief Destructor
 */
	~sha256() throw();

/**
 * \brief Append data to be hashed
 *
 * This function appends specified data to be hashed. Don't call after calling read().
 *
 * \param data The data to write.
 * \param datalen The length of data written.
 */
	void write(const uint8_t* data, size_t datalen) throw();

/**
 * \brief Read the hash value
 *
 * Reads the hash of data written. Can be called multiple times, but after the first call, data can't be appended
 * anymore.
 *
 * \param hashout 32-byte buffer to store the hash to.
 */
	void read(uint8_t* hashout) throw();

/**
 * \brief Read the hash value
 *
 * Similar to read(uint8_t*) but instead returns the hash as hexadecimal string.
 *
 * \return The hash in hex form.
 * \throw std::bad_alloc Not enough memory.
 */
	std::string read() throw(std::bad_alloc);

/**
 * \brief Hash block of data.
 *
 * Hashes block of data.
 *
 * \param hashout 32-byte buffer to write the hash to.
 * \param data The data to hash.
 * \param datalen The length of data hashed.
 */
	static void hash(uint8_t* hashout, const uint8_t* data, size_t datalen) throw();

/**
 * \brief Hash block of data.
 *
 * Hashes block of data.
 *
 * \param hashout 32-byte buffer to write the hash to.
 * \param data The data to hash.
 */
	static void hash(uint8_t* hashout, const std::vector<uint8_t>& data) throw()
	{
		hash(hashout, &data[0], data.size());
	}

/**
 * \brief Hash block of data.
 *
 * Hashes block of data.
 *
 * \param hashout 32-byte buffer to write the hash to.
 * \param data The data to hash.
 */
	static void hash(uint8_t* hashout, const std::vector<char>& data) throw()
	{
		hash(hashout, reinterpret_cast<const uint8_t*>(&data[0]), data.size());
	}

/**
 * \brief Hash block of data.
 *
 * Hashes block of data.
 *
 * \param data The data to hash.
 * \param datalen The length of data hashed.
 * \return Hexadecimal hash of the data.
 */
	static std::string hash(const uint8_t* data, size_t datalen) throw(std::bad_alloc)
	{
		uint8_t hashout[32];
		hash(hashout, data, datalen);
		return tostring(hashout);
	}

/**
 * \brief Hash block of data.
 *
 * Hashes block of data.
 *
 * \param data The data to hash.
 * \return Hexadecimal hash of the data.
 */
	static std::string hash(const std::vector<uint8_t>& data) throw(std::bad_alloc)
	{
		uint8_t hashout[32];
		hash(hashout, &data[0], data.size());
		return tostring(hashout);
	}

/**
 * \brief Hash block of data.
 *
 * Hashes block of data.
 *
 * \param data The data to hash.
 * \return Hexadecimal hash of the data.
 */
	static std::string hash(const std::vector<char>& data) throw(std::bad_alloc)
	{
		uint8_t hashout[32];
		hash(hashout, reinterpret_cast<const uint8_t*>(&data[0]), data.size());
		return tostring(hashout);
	}

/**
 * \brief Translate binary hash to hexadecimal hash
 *
 * Reads 32-byte binary hash from hashout and returns 64-hex hexadecimal hash.
 *
 * \param hashout The binary hash
 * \return Hexadecimal hash
 * \throws std::bad_alloc Not enough memory.
 */
	static std::string tostring(const uint8_t* hashout) throw(std::bad_alloc);
private:
	sha256(const sha256& x) throw();
	sha256& operator=(const sha256& x) throw();
	sha256_opaque* opaque;
	bool finished;
};

#endif
