#ifndef _avi__timer__hpp__included__
#define _avi__timer__hpp__included__

#include <cstdint>

class timer
{
public:
	timer(uint32_t rate_n, uint32_t rate_d = 1);
	void rate(uint32_t rate_n, uint32_t rate_d = 1);
	void increment()
	{
		w += sw;
		n += sn;
		w += (n / d);
		n %= d;
	}
	uint64_t read()
	{
		return w;
	}
	uint64_t read_next();
	void reset();
private:
	void set_step(uint32_t rate_n, uint32_t rate_d);
	uint64_t w;
	uint64_t n;
	uint64_t d;
	uint64_t sw;
	uint64_t sn;
};

#endif
