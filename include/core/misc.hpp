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
