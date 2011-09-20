#include "win32-crap.hpp"

#if defined(_WIN32) || defined(_WIN64)
char* strdup(const char* orig)
{
	char* x = (char*)malloc(strlen(orig) + 1);
	if(x)
		strcpy(x, orig);
	return x;
}

#endif
