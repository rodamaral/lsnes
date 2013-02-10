#include "video/avi/timer.hpp"
#include "library/minmax.hpp"

timer::timer(uint32_t rate_n, uint32_t rate_d)
{
	w = n = 0;
	set_step(rate_n, rate_d);
}

void timer::rate(uint32_t rate_n, uint32_t rate_d)
{
	double old_d = d;
	uint64_t old_d2 = d;
	set_step(rate_n, rate_d);
	//Adjust n.
	if(d == old_d2)
		return;
	n = n * (d / old_d) + 0.5;
	w += (n / d);
	n %= d;
}

// The highest value rate_n can safely have: 9,223,372,036,854,775,808
// The highest value rate_d can safely have: 18,446,744,073
void timer::set_step(uint32_t rate_n, uint32_t rate_d)
{
	uint64_t maxnscl = 9223372036854775808ULL / rate_n;
	uint64_t maxdscl = 18446744073ULL / rate_d;
	uint64_t maxscl = min(maxnscl, maxdscl);
	uint64_t _rate_n = maxscl * rate_n;
	uint64_t _rate_d = maxscl * rate_d;
	d = _rate_n;
	sw = 1000000000ULL * _rate_d / _rate_n;
	sn = 1000000000ULL * _rate_d % _rate_n;
}

uint64_t timer::read_next()
{
	uint64_t tmp_w = w + sw;
	uint64_t tmp_n = n + sn;
	tmp_w += (tmp_n / d);
	tmp_n %= d;
	return tmp_w;
}

void timer::reset()
{
	w = n = 0;
}
