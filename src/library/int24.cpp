#include "int24.hpp"
#include <cstring>

ss_uint24_t::ss_uint24_t() throw()
{
}

ss_uint24_t::ss_uint24_t(uint32_t val) throw()
{
	short magic = 256;
	memcpy(v, reinterpret_cast<char*>(&val) + *reinterpret_cast<char*>(&magic), 3);
}

ss_uint24_t::operator uint32_t() const throw()
{
	uint32_t val = 0;
	short magic = 256;
	memcpy(reinterpret_cast<char*>(&val) + *reinterpret_cast<char*>(&magic), v, 3);
	return val;
}

ss_int24_t::ss_int24_t() throw()
{
}

ss_int24_t::ss_int24_t(int32_t val) throw()
{
	short magic = 256;
	memcpy(v, reinterpret_cast<char*>(&val) + *reinterpret_cast<char*>(&magic), 3);
}

ss_int24_t::ss_int24_t(const ss_uint24_t& val) throw()
{
	memcpy(this, &val, 3);
}
ss_int24_t::operator int32_t() const throw()
{
	int32_t val = 0;
	short magic = 256;
	memcpy(reinterpret_cast<char*>(&val) + *reinterpret_cast<char*>(&magic), v, 3);
	if(val & 0x800000)
		val |= 0xFF000000U;
	return val;
}

namespace
{
	char assert1[(sizeof(ss_int24_t) == 3) ? 1 : -1];
	char assert2[(sizeof(ss_uint24_t) == 3) ? 1 : -1];
}

void dummy_3263623632786738267323()
{
	char* x = assert1;
	x[0] = 0;
	x = assert2;
	x[0] = 1;
}
