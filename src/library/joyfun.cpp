#include "library/joyfun.hpp"


short calibration_correction(int64_t v, int64_t low, int64_t high)
{
	double _v = v;
	double _low = low;
	double _high = high;
	double _pos = 65535 * (_v - _low) / (_high - _low) - 32768;
	if(_pos < -32768)
		return -32768;
	else if(_pos > 32767)
		return 32767;
	else
		return static_cast<short>(_pos);
}

short angle_to_bitmask(int pov)
{
	short m = 0;
	if((pov >= 0 && pov <= 6000) || (pov >= 30000  && pov <= 36000))
		m |= 1;
	if(pov >= 3000 && pov <= 15000)
		m |= 2;
	if(pov >= 12000 && pov <= 24000)
		m |= 4;
	if(pov >= 21000 && pov <= 33000)
		m |= 8;
	return m;
}
