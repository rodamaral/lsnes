#include "loadlib.hpp"
#include <sstream>
#include <list>

#if !defined(NO_DLFCN) && !defined(_WIN32) && !defined(_WIN64)
#include <dlfcn.h>
#include <unistd.h>
#endif

namespace loadlib
{
threads::lock& global_mutex()
{
	static threads::lock m;
	return m;
}

namespace
{
#if defined(_WIN32) || defined(_WIN64)
	std::string callsign = "dynamic link library";
	std::string callsign_ext = "dll";
#elif !defined(NO_DLFCN)
#if defined(__APPLE__)
	std::string callsign = "dynamic library";
	std::string callsign_ext = "bundle";
#else
	std::string callsign = "shared object";
	std::string callsign_ext = "so";
#endif
#else
	std::string callsign = "";
	std::string callsign_ext = "";
#endif
}

library::internal::internal(const std::string& filename) throw(std::bad_alloc, std::runtime_error)
{
	libname = filename;
	refs = 1;
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

library::internal::~internal() throw()
{
#if !defined(NO_DLFCN) && !defined(_WIN32) && !defined(_WIN64)
	dlclose(handle);
#elif defined(_WIN32) || defined(_WIN64)
	FreeLibrary(handle);
#endif
}

void* library::internal::operator[](const std::string& symbol) const throw(std::bad_alloc, std::runtime_error)
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

const std::string& library::name() throw()
{
	return callsign;
}

const std::string& library::extension() throw()
{
	return callsign_ext;
}

namespace
{
	std::list<loadlib::module*>& module_queue()
	{
		static std::list<module*> x;
		return x;
	}
}

module::module(std::initializer_list<symbol> _symbols, std::function<void(const module&)> init_fn)
{
	dynamic = false;
	for(auto i : _symbols)
		symbols[i.name] = i.address;
	init = init_fn;
	if(init) {
		threads::alock h(global_mutex());
		module_queue().push_back(this);
	}
	libname = "<anonymous module inside executable>";
}

module::module(library _lib)
{
	dynamic = true;
	lib = _lib;
	libname = _lib.get_libname();
}

module::~module()
{
	threads::alock h(global_mutex());
	for(auto i = module_queue().begin(); i != module_queue().end(); i++) {
		if(*i == this) {
			module_queue().erase(i);
			break;
		}
	}
}

module::module(const module& mod)
{
	dynamic = mod.dynamic;
	lib = mod.lib;
	symbols = mod.symbols;
	init = mod.init;
	libname = mod.libname;
	if(init) {
		threads::alock h(global_mutex());
		module_queue().push_back(this);
	}
}

void* module::operator[](const std::string& symbol) const throw(std::bad_alloc, std::runtime_error)
{
	if(dynamic)
		return lib[symbol];
	else if(symbols.count(symbol))
		return symbols.find(symbol)->second;
	else
		throw std::runtime_error("Symbol '" + symbol + "' not found");
}

void module::run_initializers()
{
	for(auto i : module_queue())
		if(i->init) {
			i->init(*i);
			i->init = std::function<void(const module&)>();
		}
	module_queue().clear();
}
}
