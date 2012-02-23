#include "library/string.hpp"

std::string strip_CR(const std::string& str)
{
	std::string x = str;
	istrip_CR(x);
	return x;
}

void istrip_CR(std::string& str)
{
	size_t crc = 0;
	size_t xl = str.length();
	while(crc < xl) {
		char y = str[xl - crc - 1];
		if(y != '\r' && y != '\n')
			break;
		crc++;
	}
	str = str.substr(0, xl - crc);
}

int firstchar(const std::string& str)
{
	if(str.length())
		return static_cast<unsigned char>(str[0]);
	else
		return -1;
}
