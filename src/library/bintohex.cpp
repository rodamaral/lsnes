#include "library/bintohex.hpp"
#include <sstream>
#include <iomanip>

std::string binary_to_hex(const uint8_t* data, size_t datalen) throw(std::bad_alloc)
{
	std::ostringstream y;
	for(size_t i = 0; i < datalen; i++)
		y << std::setw(2) << std::setfill('0') << std::hex << static_cast<unsigned>(data[i]);
	return y.str();
}
