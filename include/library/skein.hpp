#ifndef _library__skein__hpp__included__
#define _library__skein__hpp__included__

#include <cstdint>
#include <cstdlib>

struct skein_hash
{
	enum variant { PIPE_256, PIPE_512, PIPE_1024 };
	enum datatype
	{
		T_KEY = 0,
		T_PERSONALIZATION = 8,
		T_PUBKEY = 12,
		T_KEYID = 16,
		T_NONCE = 20,
		T_MESSAGE = 48
	};
	skein_hash(variant v, uint64_t outbits);
	void write(const uint8_t* data, size_t datalen, datatype type = T_MESSAGE);
	void read(uint8_t* output);
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

#endif

