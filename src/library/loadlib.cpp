#include "library/loadlib.hpp"
#include <sstream>

#if !defined(NO_DLFCN) && !defined(_WIN32) && !defined(_WIN64)
#include <dlfcn.h>
#include <unistd.h>
#endif

namespace
{
#if defined(_WIN32) || defined(_WIN64)
	std::string callsign = "dynamic link library";
#elif !defined(NO_DLFCN)
#if defined(__APPLE__)
	std::string callsign = "dynamic library";
#else
	std::string callsign = "shared object";
#endif
#else
	std::string callsign = "";
#endif
}

loaded_library::loaded_library(const std::string& filename) throw(std::bad_alloc, std::runtime_error)
{
#if !defined(NO_DLFCN) && !defined(_WIN32) && !defined(_WIN64)
	char buffer[16384];
	getcwd(buffer, 16383);
	std::string _filename = filename;
	if(filename.find_first_of("/") >= filename.length())
		_filename = buffer + std::string("/") + filename;
	handle = dlopen(_filename.c_str(), RTLD_LOCAL | RTLD_NOW);
	if(!handle)
		throw std::runtime_error(dlerror());
#elif defined(_WIN32) || defined(_WIN64)
	char buffer[16384];
	GetCurrentDirectory(16383, buffer);
	std::string _filename = filename;
	if(filename.find_first_of("/\\") >= filename.length())
		_filename = buffer + std::string("/") + filename;
	handle = LoadLibraryA(_filename.c_str());
	if(!handle) {
		int errcode = GetLastError();
		char errorbuffer[1024];
		if(FormatMessage(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM, NULL, errcode, 0,
			errorbuffer, sizeof(errorbuffer), NULL))
			throw std::runtime_error(errorbuffer);
		else {
			std::ostringstream str;
			str << "Unknown system error (code " << errcode << ")";
			throw std::runtime_error(str.str());
		}
	}
#else
	throw std::runtime_error("Loading libraries is not supported");
#endif
}

loaded_library::~loaded_library() throw()
{
#if !defined(NO_DLFCN) && !defined(_WIN32) && !defined(_WIN64)
	dlclose(handle);
#elif defined(_WIN32) || defined(_WIN64)
	FreeLibrary(handle);
#endif
}

void* loaded_library::operator[](const std::string& symbol) throw(std::bad_alloc, std::runtime_error)
{
#if !defined(NO_DLFCN) && !defined(_WIN32) && !defined(_WIN64)
	dlerror();
	void* s = dlsym(handle, symbol.c_str());
	if(s)
		return s;
	char* e = dlerror();
	if(e)
		throw std::runtime_error(e);
	return NULL;	//Yes, real NULL symbol.
#elif defined(_WIN32) || defined(_WIN64)
	void* s = (void*)GetProcAddress(handle, symbol.c_str());
	if(s)
		return s;
	int errcode = GetLastError();
	char errorbuffer[1024];
	if(FormatMessage(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM, NULL, errcode, 0,
		errorbuffer, sizeof(errorbuffer), NULL))
		throw std::runtime_error(errorbuffer);
	else {
		std::ostringstream str;
		str << "Unknown system error (code " << errcode << ")";
		throw std::runtime_error(str.str());
	}
#else
	throw std::runtime_error("Library loading not supported");
#endif
}

const std::string& loaded_library::call_library() throw()
{
	return callsign;
}
