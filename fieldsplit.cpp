#include "lsnes.hpp"
#include "fieldsplit.hpp"
#include <iostream>

fieldsplitter::fieldsplitter(const std::string& _line) throw(std::bad_alloc)
{
	line = _line;
	position = 0;
}

fieldsplitter::operator bool() throw()
{
	return (position < line.length());
}

fieldsplitter::operator std::string() throw(std::bad_alloc)
{
	size_t nextp, oldp = position;
	nextp = line.find_first_of("|", position);
	if(nextp > line.length()) {
		position = line.length();
		return line.substr(oldp);
	} else {
		position = nextp + 1;
		return line.substr(oldp, nextp - oldp);
	}
}
