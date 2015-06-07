#ifndef _library__loadlib__hpp__included__
#define _library__loadlib__hpp__included__

#include <string>
#include <stdexcept>
#include <map>
#include <set>
#include "threads.hpp"

namespace loadlib
{
threads::lock& global_mutex();

/**
 * A loaded library.
 */
class library
{
public:
/**
 * Construct a NULL library.
 */
	library()
	{
		lib = NULL;
	}
/**
 * Load a new library.
 *
 * Parameter filename: The name of file.
 * Throws std::bad_alloc: Not enough memory.
 * Throws std::runtime_error: Error loading shared library.
 */
	library(const std::string& filename) throw(std::bad_alloc, std::runtime_error)
	{
		try {
			set_loading(this);
			lib = new internal(filename);
			set_loading(NULL);
		} catch(...) {
			set_loading(NULL);
			throw;
		}
	}
/**
 * Unload a library.
 */
	~library() throw()
	{
		threads::alock h(global_mutex());
		if(lib && !--lib->refs)
			delete lib;
	}
/**
 * Look up a symbol.
 *
 * Parameter symbol: The symbol to look up.
 * Returns: The symbol value.
 * Throws std::bad_alloc: Not enough memory.
 * Throws std::runtime_error: Error looking up the symbol.
 */
	void* operator[](const std::string& symbol) const throw(std::bad_alloc, std::runtime_error)
	{
		threads::alock h(global_mutex());
		if(!lib) throw std::runtime_error("Symbol '" + symbol + "' not found");
		return (*lib)[symbol];
	}
/**
 * See what libraries are called on this platform.
 *
 * Returns: The name of library.
 */
	static const std::string& name() throw();
/**
 * See what standard library extension is on this platform.
 *
 * Returns: The extension of library.
 */
	static const std::string& extension() throw();
/**
 * Copy ctor.
 */
	library(const library& r)
	{
		threads::alock h(global_mutex());
		lib = r.lib;
		if(lib) ++lib->refs;
	}
	library& operator=(const library& r)
	{
		if(lib == r.lib)
			return *this;
		threads::alock h(global_mutex());
		if(lib && !--lib->refs)
			delete lib;
		lib = r.lib;
		if(lib) ++lib->refs;
		return *this;
	}
	std::string get_libname() const { return lib->libname; }
	void mark(const void* obj) const { if(lib) lib->mark(obj); }
	bool is_marked(const void* obj) const { return lib ? lib->is_marked(obj) : false; }
/**
 * Get currently loading library, or NULL if nothing is loading.
 */
	static library* loading() throw();
private:
	void set_loading(library* lib) throw(std::bad_alloc);
	struct internal
	{
		internal(const std::string& filename) throw(std::bad_alloc, std::runtime_error);
		~internal() throw();
		void* operator[](const std::string& symbol) const throw(std::bad_alloc, std::runtime_error);
		internal(const internal&);
		internal& operator=(const internal&);
		void* handle;
		size_t refs;
		std::string libname;
		void mark(const void* obj) { marked.insert(obj); }
		bool is_marked(const void* obj) { return marked.count(obj); }
		std::set<const void*> marked;
	};
	mutable internal* lib;
};

/**
 * A program module.
 */
class module
{
	template<typename T, typename... U> struct fntype { typedef T(*t)(U...); };
public:
/**
 * Symbol definition.
 */
	struct symbol
	{
		const char* name;
		void* address;
	};
/**
 * Construct a module from list of symbols.
 */
	module(std::initializer_list<symbol> symbols, std::function<void(const module&)> init_fn);
/**
 * Construct a module from library.
 */
	module(library lib);
/**
 * Copy ctor
 */
	module(const module& mod);
/**
 * Dtor.
 */
	~module();
/**
 * Symbol lookup.
 */
	void* operator[](const std::string& symbol) const throw(std::bad_alloc, std::runtime_error);
/**
 * Variable symbol lookup.
 */
	template<typename T> T* var(const std::string& symbol) const throw(std::bad_alloc, std::runtime_error)
	{
		return (T*)(*this)[symbol];
	}
/**
 * Function symbol lookup.
 */
	template<typename T, typename... U> typename fntype<T, U...>::t fn(const std::string& symbol) const
		throw(std::bad_alloc, std::runtime_error)
	{
		return (typename fntype<T, U...>::t)(*this)[symbol];
	}
/**
 * Get name.
 */
	std::string get_libname() const { return libname; }
/**
 * Run all not ran initialization functions.
 */
	static void run_initializers();
/**
 * Mark object (only works for libraries).
 */
	void mark(const void* obj) const { if(dynamic) lib.mark(obj); }
/**
 * Is object marked (only works for libraries)?
 */
	bool is_marked(const void* obj) const { return dynamic ? lib.is_marked(obj) : false; }
private:
	bool dynamic;
	library lib;
	std::map<std::string, void*> symbols;
	std::function<void(const module&)> init;
	std::string libname;
};

}
#endif
