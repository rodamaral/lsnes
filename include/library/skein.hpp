#ifndef _library__skein__hpp__included__
#define _library__skein__hpp__included__

#include <cstdint>
#include <cstdlib>
#include <stdexcept>

namespace skein
{
/**
 * Skein hash function (v1.3).
 */
struct hash
{
/**
 * Variant to use (256-bit, 512-bit, 1024-bit)
 */
	enum variant { PIPE_256, PIPE_512, PIPE_1024 };
/**
 * Data type for piece of data.
 */
	enum datatype
	{
		T_KEY = 0,
		T_PERSONALIZATION = 8,
		T_PUBKEY = 12,
		T_KEYID = 16,
		T_NONCE = 20,
		T_MESSAGE = 48
	};
/**
 * Create a new hash state.
 *
 * Parameter v: The variant to use.
 * Parameter outbits: Number of output bits.
 * Throws std::runtime_error: Variant is invalid.
 */
	hash(variant v, uint64_t outbits) throw(std::runtime_error);
/**
 * Write data to be hashed.
 *
 * Parameter data: The data to append.
 * Parameter datalen: Number of bytes in data.
 * Parameter type: The data type. Must be monotonically increasing.
 * Throws std::runtime_error: Types not monotonic, or invalid type.
 *
 * Note: Data types 4 (CONFIG) and 63 (OUTPUT) are not allowed.
 */
	void write(const uint8_t* data, size_t datalen, datatype type = T_MESSAGE) throw(std::runtime_error);
/**
 * Read the output hash.
 *
 * Parameter output: Buffer to store the output to.
 */
	void read(uint8_t* output) throw();
/**
 * Read partial output hash.
 *
 * Parameter output: Buffer to store the output to.
 * Parameter startblock: The block number (each block is 256/512/1024 bits depending on variant) to start from.
 * Parameter bits: Number of bits to output.
 */
	void read_partial(uint8_t* output, uint64_t startblock, uint64_t bits) throw();
private:
	void typechange(uint8_t newtype);
	void configure();
	void flush_buffer(uint8_t type, bool final);
	uint64_t chain[16];
	uint8_t buffer[128];
	void (*compress)(uint64_t* out, const uint64_t* data, const uint64_t* key, const uint64_t* tweak);
	unsigned bufferfill;
	unsigned fullbuffer;
	uint64_t data_low;
	uint64_t data_high;
	uint64_t outbits;
	int8_t last_type;
};

/**
 * Skein PRNG.
 */
struct prng
{
public:
/**
 * Construct a PRNG.
 *
 * Note: To seed the PRNG, write the initial seed there.
 */
	prng() throw();
/**
 * (Re)seed the PRNG and mark it seeded.
 *
 * Parameter buffer: Buffer to read the seed from.
 * Parameter size: Number of bytes in seed.
 */
	void write(const void* buffer, size_t size) throw();
/**
 * Read data from PRNG.
 *
 * Parameter buffer: Buffer to write the data to.
 * Parameter size: Number of random bytes to write.
 * Throws std::runtime_error: Generator is not seeded.
 */
	void read(void* buffer, size_t size) throw(std::runtime_error);
/**
 * Is seeded?
 */
	bool is_seeded() const throw();
private:
	uint8_t state[128];
	bool _is_seeded;
};
}

#endif

