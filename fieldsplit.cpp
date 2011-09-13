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

tokensplitter::tokensplitter(const std::string& _line) throw(std::bad_alloc)
{
	line = _line;
	position = 0;
}

tokensplitter::operator bool() throw()
{
	return (position < line.length());
}

tokensplitter::operator std::string() throw(std::bad_alloc)
{
	size_t nextp, oldp = position;
	nextp = line.find_first_of(" \t", position);
	if(nextp > line.length()) {
		position = line.length();
		return line.substr(oldp);
	} else {
		position = nextp;
		while(position < line.length() && (line[position] == ' ' || line[position] == '\t'))
			position++;
		return line.substr(oldp, nextp - oldp);
	}
}

std::string tokensplitter::tail() throw(std::bad_alloc)
{
	return line.substr(position);
}
