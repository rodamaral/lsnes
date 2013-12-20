#include "sha256.hpp"
#include "hex.hpp"
#include <cstdint>
#include <sstream>
#include <iostream>
#include <iomanip>
#include "arch-detect.hpp"

//Since this isn't used for anything too performance-sensitive, just write a implementation, no need to specially
//optimize.

namespace
{
	//Initial state of SHA256.
	const uint32_t sha256_initial_state[] = {
		0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
	};

	//The round constants.
	const uint32_t k[] = {
		0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
		0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
		0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
		0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
		0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
		0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
		0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
		0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
	};

	template<unsigned p>
	inline uint32_t rotate_r(uint32_t num)
	{
		return (num >> p) | (num << (32 - p));
	}

	inline uint32_t sigma0(uint32_t num)
	{
		return rotate_r<2>(num) ^ rotate_r<13>(num) ^ rotate_r<22>(num);
	}

	inline uint32_t sigma1(uint32_t num)
	{
		return rotate_r<6>(num) ^ rotate_r<11>(num) ^ rotate_r<25>(num);
	}

	inline uint32_t esigma0(uint32_t num)
	{
		return rotate_r<7>(num) ^ rotate_r<18>(num) ^ (num >> 3);
	}

	inline uint32_t esigma1(uint32_t num)
	{
		return rotate_r<17>(num) ^ rotate_r<19>(num) ^ (num >> 10);
	}

	inline uint32_t majority(uint32_t a, uint32_t b, uint32_t c)
	{
		return ((a & b) ^ (a & c) ^ (b & c));
	}

	inline uint32_t choose(uint32_t k, uint32_t a, uint32_t b)
	{
		return (k & a) | ((~k) & b);
	}

#define SHOW(a,b,c,d,e,f,g,h) "\t" << hex::to32(a) << "\t" << hex::to32(b) << "\t" << hex::to32(c) << "\t" \
	<< hex::to32(d) << "\t" << hex::to32(e) << "\t" << hex::to32(f) << "\t" << hex::to32(g) << "\t" \
	<< hex::to32(h)

#define WROUND(i, shift) \
	Xsigma0 = esigma0(datablock[(i + shift + 1) & 15]); \
	Xsigma1 = esigma1(datablock[(i + shift + 14) & 15]); \
	datablock[(i + shift) & 15] += Xsigma0 + Xsigma1 + datablock[(i + shift + 9) & 15];

#define ROUND(a,b,c,d,e,f,g,h, i, l) \
	X = h + k[i | l] + datablock[(i & 8) | l] + sigma1(e) + choose(e, f, g); \
	h = X + sigma0(a) + majority(a, b, c); \
	d += X; \

#define ROUND8A(a, b, c, d, e, f, g, h, i) \
	ROUND(a, b, c, d, e, f, g, h, i, 0); \
	ROUND(h, a, b, c, d, e, f, g, i, 1); \
	ROUND(g, h, a, b, c, d, e, f, i, 2); \
	ROUND(f, g, h, a, b, c, d, e, i, 3); \
	ROUND(e, f, g, h, a, b, c, d, i, 4); \
	ROUND(d, e, f, g, h, a, b, c, i, 5); \
	ROUND(c, d, e, f, g, h, a, b, i, 6); \
	ROUND(b, c, d, e, f, g, h, a, i, 7)

#define ROUND8B(a, b, c, d, e, f, g, h, i) \
	WROUND(i, 0); \
	ROUND(a, b, c, d, e, f, g, h, i, 0); \
	WROUND(i, 1); \
	ROUND(h, a, b, c, d, e, f, g, i, 1); \
	WROUND(i, 2); \
	ROUND(g, h, a, b, c, d, e, f, i, 2); \
	WROUND(i, 3); \
	ROUND(f, g, h, a, b, c, d, e, i, 3); \
	WROUND(i, 4); \
	ROUND(e, f, g, h, a, b, c, d, i, 4); \
	WROUND(i, 5); \
	ROUND(d, e, f, g, h, a, b, c, i, 5); \
	WROUND(i, 6); \
	ROUND(c, d, e, f, g, h, a, b, i, 6); \
	WROUND(i, 7); \
	ROUND(b, c, d, e, f, g, h, a, i, 7)


	void compress_sha256(uint32_t* state, uint32_t* datablock, unsigned& blockbytes)
	{
		uint32_t a = state[0];
		uint32_t b = state[1];
		uint32_t c = state[2];
		uint32_t d = state[3];
		uint32_t e = state[4];
		uint32_t f = state[5];
		uint32_t g = state[6];
		uint32_t h = state[7];
		uint32_t X, Xsigma0, Xsigma1;
		ROUND8A(a, b, c, d, e, f, g, h, 0);
		ROUND8A(a, b, c, d, e, f, g, h, 8);
		ROUND8B(a, b, c, d, e, f, g, h, 16);
		ROUND8B(a, b, c, d, e, f, g, h, 24);
		ROUND8B(a, b, c, d, e, f, g, h, 32);
		ROUND8B(a, b, c, d, e, f, g, h, 40);
		ROUND8B(a, b, c, d, e, f, g, h, 48);
		ROUND8B(a, b, c, d, e, f, g, h, 56);
		state[0] += a;
		state[1] += b;
		state[2] += c;
		state[3] += d;
		state[4] += e;
		state[5] += f;
		state[6] += g;
		state[7] += h;
		memset(datablock, 0, 64);
		blockbytes = 0;
	}
}

void sha256::real_init()
{
	for(unsigned i = 0; i < 8; i++)
		state[i] = sha256_initial_state[i];
	memset(datablock, 0, sizeof(datablock));
	blockbytes = 0;
	totalbytes = 0;
}

void sha256::real_destroy()
{
}

void sha256::real_finish(uint8_t* hash)
{
	datablock[blockbytes / 4] |= (static_cast<uint32_t>(0x80) << (24 - blockbytes % 4 * 8));
	if(blockbytes > 55)
		//We can't fit the length into this block.
		compress_sha256(state, datablock, blockbytes);
	//Write the length.
	datablock[14] = totalbytes >> 29;
	datablock[15] = totalbytes << 3;
	compress_sha256(state, datablock, blockbytes);
	for(unsigned i = 0; i < 32; i++)
		hash[i] = state[i / 4] >> (24 - i % 4 * 8);
}

void sha256::real_write(const uint8_t* data, size_t datalen)
{
#ifdef ARCH_IS_I386
	//First pad blockbytes to multiple of four.
	size_t i = 0;
	while(blockbytes & 3 && i < datalen) {
		datablock[blockbytes / 4] |= (static_cast<uint32_t>(data[i]) << (24 - blockbytes % 4 * 8));
		blockbytes++;
		if(blockbytes == 64)
			compress_sha256(state, datablock, blockbytes);
		i++;
	}
	size_t blocks = (datalen - i) / 4;
	unsigned ptr = blockbytes / 4;
	//Then process four bytes ata time.
	for(size_t j = 0; j < blocks; j++) {
		uint32_t x = *reinterpret_cast<const uint32_t*>(data + i);
		asm("bswap %0" : "+r"(x));
		datablock[ptr] = x;
		ptr = (ptr + 1) & 15;
		i += 4;
		blockbytes += 4;
		if(blockbytes == 64)
			compress_sha256(state, datablock, blockbytes);
	}
	//And finally process tail.
	while(i < datalen) {
		datablock[blockbytes / 4] |= (static_cast<uint32_t>(data[i]) << (24 - blockbytes % 4 * 8));
		blockbytes++;
		if(blockbytes == 64)
			compress_sha256(state, datablock, blockbytes);
		i++;
	}
#else
	for(size_t i = 0; i < datalen; i++) {
		datablock[blockbytes / 4] |= (static_cast<uint32_t>(data[i]) << (24 - blockbytes % 4 * 8));
		blockbytes++;
		if(blockbytes == 64)
			compress_sha256(state, datablock, blockbytes);
	}
#endif
	totalbytes += datalen;
}

#ifdef SHA256_SELFTEST

#define TEST_LOOPS 100000
#define TEST_DATASET 4096
#include <sys/time.h>

int main(int argc, char** argv)
{
	sha256 i;
	i.write(argv[1], strlen(argv[1]));
	i.write(argv[2], strlen(argv[2]));
	std::cerr << i.read() << std::endl;
	struct timeval t1;
	struct timeval t2;
	char buffer[TEST_DATASET] = {0};
	gettimeofday(&t1, NULL);
	sha256 i2;
	for(unsigned j = 0; j < TEST_LOOPS; j++)
		i2.write(buffer, TEST_DATASET);
	gettimeofday(&t2, NULL);
	uint64_t _t1 = (uint64_t)t1.tv_sec * 1000000 + t1.tv_usec;
	uint64_t _t2 = (uint64_t)t2.tv_sec * 1000000 + t2.tv_usec;
	std::cerr << "Hashing performance: " << static_cast<double>(TEST_LOOPS * TEST_DATASET) / (_t2 - _t1)
		<< "MB/s." << std::endl;
}

#endif
