#include "lsnes.hpp"

#include "core/random.hpp"
#include "library/crandom.hpp"
#include "library/hex.hpp"
#include "library/serialization.hpp"
#include "library/skein.hpp"
#include "library/string.hpp"
#include "library/threads.hpp"

#include <cstdlib>
#include <csignal>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <ctime>
#include <cstdlib>
#include <cstring>
#include <sys/time.h>
#include <unistd.h>
#include <boost/filesystem.hpp>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

#ifdef USE_LIBGCRYPT_SHA256
#include <gcrypt.h>
#endif

namespace
{
	skein::prng prng;
	threads::lock seed_mutex;

	void do_mix_tsc()
	{
		const unsigned slots = 32;
		static unsigned count = 0;
		static uint64_t last_reseed = 0;
		static uint64_t buf[slots + 1];
		threads::alock h(seed_mutex);
		buf[count++] = crandom::arch_get_tsc();
		if(count == 0) count = 1;  //Shouldn't happen.
		if(count == slots || buf[count - 1] - last_reseed > 300000000) {
			last_reseed = buf[count - 1];
			buf[slots] = crandom::arch_get_random();
			prng.write(buf, sizeof(buf));
			count = 0;
		}
	}

	std::string get_random_hexstring_64(size_t index)
	{
		uint64_t buf[7];
		uint8_t out[32];
		timeval tv;
		buf[3] = crandom::arch_get_random();
		buf[4] = crandom::arch_get_random();
		buf[5] = crandom::arch_get_random();
		buf[6] = crandom::arch_get_random();
		gettimeofday(&tv, NULL);
		buf[0] = tv.tv_sec;
		buf[1] = tv.tv_usec;
		buf[2] = crandom::arch_get_tsc();
		threads::alock h(seed_mutex);
		prng.write(buf, sizeof(buf));
		prng.read(out, sizeof(out));
		return hex::b_to(out, sizeof(out));
	}
}

void contribute_random_entropy(void* buf, size_t bytes)
{
	do_mix_tsc();
	threads::alock h(seed_mutex);
	prng.write(buf, sizeof(buf));
}

std::string get_random_hexstring(size_t length) throw(std::bad_alloc)
{
	std::string out;
	for(size_t i = 0; i < length; i += 64)
		out = out + get_random_hexstring_64(i);
	return out.substr(0, length);
}

void set_random_seed(const std::string& seed) throw(std::bad_alloc)
{
	std::vector<char> x(seed.begin(), seed.end());
	{
		threads::alock h(seed_mutex);
		prng.write(&x[0], x.size());
	}
}

void set_random_seed() throw(std::bad_alloc)
{
	char buf[128];
	crandom::generate(buf, 128);
	std::string s(buf, sizeof(buf));
	set_random_seed(s);
}

void random_mix_timing_entropy()
{
	do_mix_tsc();
}

//Generate highly random stuff.
void highrandom_256(uint8_t* buf)
{
	uint8_t tmp[104];
	std::string s = get_random_hexstring(64);
	std::copy(s.begin(), s.end(), reinterpret_cast<char*>(tmp));
	crandom::generate(tmp + 64, 32);
	serialization::u64b(tmp + 96, crandom::arch_get_tsc());
	skein::hash hsh(skein::hash::PIPE_1024, 256);
	hsh.write(tmp, 104);
	hsh.read(buf);
}
