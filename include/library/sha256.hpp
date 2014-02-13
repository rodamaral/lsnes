#ifndef _library__sha256__hpp__included__
#define _library__sha256__hpp__included__

#include "hex.hpp"
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>

/**
 * This class implements interface to SHA-256.
 */
class sha256
{
public:
/**
 * Creates new SHA-256 context, initially containing empty data.
 */
	sha256() throw(std::bad_alloc)
	{
		real_init();
		finished = false;
	}

/**
 * Destructor
 */
	~sha256() throw()
	{
		real_destroy();
	}

/**
 * This function appends specified data to be hashed. Don't call after calling read().
 *
 * Parameter data: The data to write.
 * Parameter datalen: The length of data written.
 */
	void write(const uint8_t* data, size_t datalen) throw()
	{
		if(finished)
			return;
		real_write(data, datalen);
	}

/**
 * Reads the hash of data written. Can be called multiple times, but after the first call, data can't be appended
 * anymore.
 *
 * Parameter hashout: 32-byte buffer to store the hash to.
 */
	void read(uint8_t* hashout) throw()
	{
		if(!finished) {
			real_finish(finalhash);
			finished = true;
		}
		memcpy(hashout, finalhash, 32);
	}

/**
 * Reads 32-byte binary hash from hashout and returns 64-hex hexadecimal hash.
 *
 * Parameter hashout: The binary hash
 * Returns: Hexadecimal hash
 * Throws std::bad_alloc: Not enough memory.
 */
	static std::string tostring(const uint8_t* hashout) throw(std::bad_alloc)
	{
		return hex::b_to(hashout, 32);
	}

/**
 * This function appends specified data to be hashed. Don't call after calling read().
 *
 * Parameter data: The data to write.
 * Parameter datalen: The length of data written.
 */
	void write(const char* data, size_t datalen) throw()
	{
		write(reinterpret_cast<const uint8_t*>(data), datalen);
	}

/**
 * Similar to read(uint8_t*) but instead returns the hash as hexadecimal string.
 *
 * Returns: The hash in hex form.
 * Throws std::bad_alloc: Not enough memory.
 */
	std::string read() throw(std::bad_alloc)
	{
		uint8_t x[32];
		read(x);
		return tostring(x);
	}

/**
 * Hashes block of data.
 *
 * Parameter hashout: 32-byte buffer to write the hash to.
 * Parameter data: The data to hash.
 * Parameter datalen: The length of data hashed.
 */
	static void hash(uint8_t* hashout, const uint8_t* data, size_t datalen) throw()
	{
		sha256 i;
		i.write(data, datalen);
		i.read(hashout);
	}

/**
 * Hashes block of data.
 *
 * Parameter hashout: 32-byte buffer to write the hash to.
 * Parameter data: The data to hash.
 */
	static void hash(uint8_t* hashout, const std::vector<uint8_t>& data) throw()
	{
		hash(hashout, &data[0], data.size());
	}

/**
 * Hashes block of data.
 *
 * Parameter hashout: 32-byte buffer to write the hash to.
 * Parameter data: The data to hash.
 */
	static void hash(uint8_t* hashout, const std::vector<char>& data) throw()
	{
		hash(hashout, reinterpret_cast<const uint8_t*>(&data[0]), data.size());
	}

/**
 * Hashes block of data.
 *
 * Parameter data: The data to hash.
 * Parameter datalen: The length of data hashed.
 * Returns: Hexadecimal hash of the data.
 * Throws std::bad_alloc: Not enough memory.
 */
	static std::string hash(const uint8_t* data, size_t datalen) throw(std::bad_alloc)
	{
		uint8_t hashout[32];
		hash(hashout, data, datalen);
		return tostring(hashout);
	}

/**
 * Hashes block of data.
 *
 * Parameter data: The data to hash.
 * Returns: Hexadecimal hash of the data.
 * Throws std::bad_alloc: Not enough memory.
 */
	static std::string hash(const std::vector<uint8_t>& data) throw(std::bad_alloc)
	{
		uint8_t hashout[32];
		hash(hashout, &data[0], data.size());
		return tostring(hashout);
	}

/**
 * Hashes block of data.
 *
 * Parameter data: The data to hash.
 * Returns: Hexadecimal hash of the data.
 * Throws std::bad_alloc: Not enough memory.
 */
	static std::string hash(const std::vector<char>& data) throw(std::bad_alloc)
	{
		uint8_t hashout[32];
		hash(hashout, reinterpret_cast<const uint8_t*>(&data[0]), data.size());
		return tostring(hashout);
	}
private:
	uint32_t state[8];
	uint32_t datablock[16];
	unsigned blockbytes;
	uint64_t totalbytes;
	bool finished;
	uint8_t finalhash[32];
	void real_init();
	void real_destroy();
	void real_finish(uint8_t* hash);
	void real_write(const uint8_t* data, size_t datalen);
};

#endif
