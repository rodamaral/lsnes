#include "core/loadlib.hpp"
#include "core/command.hpp"
#include <stdexcept>
#include <sstream>

namespace {
	function_ptr_command<arg_filename> load_lib("load-library", "Load a library",
		"Syntax: load-library <file>\nLoad library <file>\n",
		[](arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			try {
				load_library(args);
			} catch(std::exception& e) {
				std::ostringstream x;
				x << "Can't load '" << std::string(args) << "': " << e.what();
				throw std::runtime_error(x.str());
			}
		});
}

#if !defined(NO_DLFCN) && !defined(_WIN32) && !defined(_WIN64)
#include <dlfcn.h>
#include <unistd.h>
void load_library(const std::string& filename)
{
	char buffer[16384];
	getcwd(buffer, 16383);
	std::string _filename = filename;
	if(filename.find_first_of("/") >= filename.length())
		_filename = buffer + std::string("/") + filename;
	void* h = dlopen(_filename.c_str(), RTLD_LOCAL | RTLD_NOW);
	if(!h)
		throw std::runtime_error(dlerror());
}
extern const bool load_library_supported = true;
#ifdef __APPLE__
const char* library_is_called = "Dynamic Library";
#else
const char* library_is_called = "Shared Object";
#endif
#elif defined(_WIN32) || defined(_WIN64)
#include <windows.h>
void load_library(const std::string& filename)
{
	char buffer[16384];
	GetCurrentDirectory(16383, buffer);
	std::string _filename = filename;
	if(filename.find_first_of("/\\") >= filename.length())
		_filename = buffer + std::string("/") + filename;
	HMODULE h = LoadLibraryA(_filename.c_str());
	if(!h) {
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
}
extern const bool load_library_supported = true;
const char* library_is_called = "Dynamic Link Library";
#else
void load_library(const std::string& filename)
{
	throw std::runtime_error("Library loader not supported on this platform");
}
extern const bool load_library_supported = false;
const char* library_is_called = NULL;
#endif



