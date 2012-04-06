#ifndef _tcp__hpp__included__
#define _tcp__hpp__included__

#include <iostream>
#include <string>
#include <vector>

class socket_address
{
public:
	socket_address(const std::string& spec);
	socket_address next();
	std::ostream& connect();
	static bool supported();
private:
	socket_address(int f, int st, int p);
	int family;
	int socktype;
	int protocol;
	std::vector<char> memory;
};

#endif
