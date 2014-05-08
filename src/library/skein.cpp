#include <cstdint>
#include <cstring>
#include "skein.hpp"
#include <iostream>
#include <stdexcept>
#include <iomanip>
#include <algorithm>
#include "arch-detect.hpp"
#ifdef TEST_SKEIN_CODE
#include "hex.hpp"
#endif

namespace skein
{
//Jerry Solinas was not here.
#include "skein512c.inc"

static uint8_t bitmasks[] = {0, 128, 192, 224, 240, 248, 252, 254, 255};

#ifdef TEST_SKEIN_CODE
static void show_array(const char* prefix, const uint64_t* a, size_t e)
{
	std::cerr << prefix;
	for(size_t i = 0; i < e; i++) {
		std::cerr << hex::to(a[i]);
		if(i < e - 1)
			std::cerr << ", ";
	}
	std::cerr << std::endl;
}

static void show_array(const char* prefix, const uint8_t* a, size_t e)
{
	std::cerr << prefix;
	for(size_t i = 0; i < e; i++) {
		std::cerr << hex::to(a[i]);
	}
	std::cerr << std::endl;
}
#endif

inline static void to_words(uint64_t* out, const void* in, size_t words)
{
#ifdef ARCH_IS_I386
	memcpy(out, in, words<<3);
#else
	for(unsigned i = 0; i < words; i++)
		out[i]=0;
	for(unsigned i = 0; i < (words<<3); i++)
		out[i>>3]|=((uint64_t)reinterpret_cast<uint8_t*>(in)[i] << ((i&7)<<3));
#endif
}

inline static void to_bytes(void* out, const uint64_t* in, size_t bytes)
{
#ifdef ARCH_IS_I386
	memcpy(out, in, bytes);
#else
	for(size_t i = 0; i < bytes; i++)
		output[i] = (out[i>>3] >> ((i&7)<<3));
#endif
}

inline static void _skein256_compress(uint64_t* a, const uint64_t* b, const uint64_t* c, const uint64_t* d)
{
#ifdef TEST_SKEIN_CODE
	show_array("Key:   ", c, 4);
	show_array("Data:  ", b, 4);
	show_array("Tweak: ", d, 2);
#endif
	skein256_compress(a, b, c, d);
#ifdef TEST_SKEIN_CODE
	show_array("Out:   ", a, 4);
#endif
}

inline static void _skein512_compress(uint64_t* a, const uint64_t* b, const uint64_t* c, const uint64_t* d)
{
#ifdef TEST_SKEIN_CODE
	show_array("Key:   ", c, 8);
	show_array("Data:  ", b, 8);
	show_array("Tweak: ", d, 2);
#endif
	skein512_compress(a, b, c, d);
#ifdef TEST_SKEIN_CODE
	show_array("Out:   ", a, 8);
#endif
}

inline static void _skein1024_compress(uint64_t* a, const uint64_t* b, const uint64_t* c, const uint64_t* d)
{
#ifdef TEST_SKEIN_CODE
	show_array("Key:   ", c, 16);
	show_array("Data:  ", b, 16);
	show_array("Tweak: ", d, 2);
#endif
	skein1024_compress(a, b, c, d);
#ifdef TEST_SKEIN_CODE
	show_array("Out:   ", a, 16);
#endif
}

hash::hash(hash::variant v, uint64_t _outbits) throw(std::runtime_error)
{
	memset(chain, 0, sizeof(chain));
	memset(buffer, 0, sizeof(buffer));
	switch(v) {
	case PIPE_256: compress = _skein256_compress; fullbuffer = 32; break;
	case PIPE_512: compress = _skein512_compress; fullbuffer = 64; break;
	case PIPE_1024: compress = _skein1024_compress; fullbuffer = 128; break;
	default: throw std::runtime_error("Invalid Skein variant");
	}
	bufferfill = 0;
	data_low = 0;
	data_high = 0;
	outbits = _outbits;
	last_type = -1;
}

hash::~hash() throw()
{
	zeroize(chain, sizeof(chain));
	zeroize(buffer, sizeof(buffer));
	zeroize(&bufferfill, sizeof(bufferfill));
	zeroize(&data_low, sizeof(data_low));
	zeroize(&data_high, sizeof(data_high));
	zeroize(&last_type, sizeof(last_type));
}

void hash::configure()
{
	uint64_t config[16] = {0x133414853ULL,outbits};
	uint64_t tweak[2] = {32,0xC400000000000000ULL};
	uint64_t iv[16];
	compress(iv, config, chain, tweak);
	memcpy(chain, iv, fullbuffer);
	last_type = 4;
	zeroize(iv, sizeof(iv));
	zeroize(tweak, sizeof(tweak));
	zeroize(config, sizeof(config));
}

void hash::typechange(uint8_t newtype)
{
	if(last_type != newtype) {
		//Type change.
		//1) If changing from any other last type except NONE or CONFIG, flush.
		if(last_type >= 0 && last_type != 4)
			flush_buffer(last_type, true);
		//2) If changing over CONFIG, configure the hash.
		if(last_type < 4 && newtype > 4)
			configure();
		//3) If changing over MESSAGE, flush even if empty.
		if(last_type < 48 && newtype > 48) {
			last_type = newtype;
			data_low = 0;
			data_high = 0;
			flush_buffer(48, true);
		}
		last_type = newtype;
		data_low = 0;
		data_high = 0;
	}
}

void hash::write(const uint8_t* data, size_t datalen, hash::datatype type) throw(std::runtime_error)
{
	if(type < 0 || type == 4 || type > 62)
		throw std::runtime_error("Invalid data type to write");
	if(type < last_type)
		throw std::runtime_error("Data types in wrong order");
	while(datalen > 0) {
		typechange(type);
		if(bufferfill == fullbuffer)
			flush_buffer(type, false);
		if(datalen >= fullbuffer - bufferfill) {
			memcpy(buffer + bufferfill, data, fullbuffer - bufferfill);
			data += (fullbuffer - bufferfill);
			datalen -= (fullbuffer - bufferfill);
			bufferfill = fullbuffer;
		} else {
			memcpy(buffer + bufferfill, data, datalen);
			data += datalen;
			bufferfill += datalen;
			datalen = 0;
		}
	}
}

void hash::flush_buffer(uint8_t type, bool final)
{
	uint64_t _buffer[16];
	uint64_t _buffer2[16];
	uint64_t tweak[2];
	tweak[0] = data_low + bufferfill;
	tweak[1] = data_high;
	if(tweak[0] < data_low)
		tweak[1]++;
	tweak[1] += ((uint64_t)type << 56);
	if(!data_low && !data_high)
		tweak[1] += (1ULL << 62);
	if(final)
		tweak[1] += (1ULL << 63);
	to_words(_buffer, buffer, fullbuffer >> 3);
	compress(_buffer2, _buffer, chain, tweak);
	memcpy(chain, _buffer2, fullbuffer);
	data_low += bufferfill;
	if(data_low < bufferfill)
		data_high++;
	bufferfill = 0;
	memset(buffer, 0, fullbuffer);
	zeroize(_buffer, sizeof(_buffer));
	zeroize(_buffer2, sizeof(_buffer2));
	zeroize(tweak, sizeof(tweak));
}

void hash::read_partial(uint8_t* output, uint64_t startblock, uint64_t bits) throw()
{
	typechange(63);  //Switch to output.
	//The final one is special.
	uint64_t zeroes[16] = {0};
	uint64_t out[16];
	uint64_t tweak[2] = {8,0xFF00000000000000ULL};
	uint64_t offset = 0;
	zeroes[0] = startblock;
	for(uint64_t i = 0; i < bits; i += (fullbuffer<<3)) {
		compress(out, zeroes, chain, tweak);
		zeroes[0]++;
		uint64_t fullbytes = std::min((bits - i) >> 3, static_cast<uint64_t>(fullbuffer));
		to_bytes(output + offset, out, fullbytes);
		if(fullbytes < fullbuffer && i + 8 * fullbytes < bits) {
			output[offset + fullbytes] = (out[fullbytes>>3] >> ((fullbytes&7)<<3));
			output[offset + fullbytes] &= bitmasks[bits&7];
		}
		offset += fullbuffer;
	}
	zeroize(out, sizeof(out));
	zeroize(zeroes, sizeof(zeroes));
	zeroize(tweak, sizeof(tweak));
}

void hash::read(uint8_t* output) throw()
{
	read_partial(output, 0, outbits);
}

prng::prng() throw()
{
	_is_seeded = false;
	memset(state, 0, 128);
}

void prng::write(const void* buffer, size_t size) throw()
{
	hash h(hash::PIPE_1024, 1024);
	h.write(state, 128, hash::T_NONCE);
	h.write(reinterpret_cast<const uint8_t*>(buffer), size, hash::T_MESSAGE);
	h.read(state);
	if(size > 0)
		_is_seeded = true;
}

void prng::read(void* buffer, size_t size) throw(std::runtime_error)
{
	if(!_is_seeded)
		throw std::runtime_error("PRNG is not initialized");
	//We can't use skein itself here, but the underlying compression function.
	uint64_t chain[16] = {0};
	uint64_t zeroes[16] = {0};
	uint64_t out[16];
	uint64_t tweak[2] = {8,0xFF00000000000000ULL};
	to_words(chain, state, 16);
	zeroes[0] = 1;
	for(uint64_t i = 0; i < size; i += 128) {
		_skein1024_compress(out, zeroes, chain, tweak);
		zeroes[0]++;
		uint64_t fullbytes = std::min(size - i, static_cast<uint64_t>(128));
		to_bytes(reinterpret_cast<uint8_t*>(buffer) + i, out, fullbytes);
	}
	zeroes[0] = 0;
	_skein1024_compress(out, zeroes, chain, tweak);
	to_bytes(state, out, 128);
	zeroize(chain, sizeof(chain));
	zeroize(zeroes, sizeof(zeroes));
	zeroize(out, sizeof(out));
	zeroize(tweak, sizeof(tweak));
}

bool prng::is_seeded() const throw()
{
	return _is_seeded;
}

void zeroize(void* ptr, size_t size)
{
	if(!size) return;
	//Whee... Do it like OpenSSL/GnuTLS.
	volatile char* vptr = (volatile char*)ptr;
	volatile size_t vidx = 0;
	do { memset(ptr, 0, size); } while(vptr[vidx]);
}

}
#ifdef TEST_SKEIN_CODE
#define SKEIN_DEBUG
#include <skein.h>
#include <skein_debug.h>



int main(int argc, char** argv)
{
/*
	//skein_DebugFlag = SKEIN_DEBUG_STATE | SKEIN_DEBUG_TWEAK | SKEIN_DEBUG_INPUT_64;
	uint8_t out[128];
	skein::hash ctx(skein::hash::PIPE_512, 256);
	ctx.write((uint8_t*)argv[1], strlen(argv[1]), skein::hash::T_KEY);
	ctx.write((uint8_t*)argv[2], strlen(argv[2]), skein::hash::T_MESSAGE);
	ctx.read(out);
	show_array("New: ", out, 32);
	Skein_512_Ctxt_t ctx2;
	Skein_512_InitExt(&ctx2, 256, SKEIN_CFG_TREE_INFO_SEQUENTIAL, (uint8_t*)argv[1], strlen(argv[1]));
	Skein_512_Update(&ctx2, (uint8_t*)argv[2], strlen(argv[2]));
	Skein_512_Final(&ctx2, out);
	show_array("Ref: ", out, 32);
	return 0;

}
int main()
{
*/

	uint8_t buf[129] = {0xFF,0xFE,0xFD,0xFC,0xFB,0xFA,0xF9,0xF8,0xF7};
	uint8_t key[135] = {0x05,0x04,0x46,0x22,0x26,0x35,0x63,0x26,0xFF};
	uint8_t out[128];
	skein::hash ctx(skein::hash::PIPE_256, 256);
	ctx.write(key, sizeof(key), skein::hash::T_KEY);
	ctx.write(key, sizeof(key), skein::hash::T_NONCE);
	ctx.write(buf, 2);
	ctx.read(out);
	show_array("", out, 32);
}

#endif
