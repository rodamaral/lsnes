#ifndef _library__lua_function__hpp__included__
#define _library__lua_function__hpp__included__

#include "lua-base.hpp"
#include "lua-pin.hpp"

namespace lua
{
class parameters;

/**
 * Group of functions.
 */
class function_group
{
public:
/**
 * Create a group.
 */
	function_group();
/**
 * Destroy a group.
 */
	~function_group();
/**
 * Add a function to group.
 */
	void do_register(const std::string& name, function& fun);
/**
 * Drop a function from group.
 */
	void do_unregister(const std::string& name, function* dummy);
/**
 * Request callbacks on all currently registered functions.
 */
	void request_callback(std::function<void(std::string, function*)> cb);
/**
 * Bind a callback.
 *
 * Callbacks for all registered functions are immediately called.
 */
	int add_callback(std::function<void(std::string, function*)> cb,
		std::function<void(function_group*)> dcb);
/**
 * Unbind a calback.
 */
	void drop_callback(int handle);
private:
	int next_handle;
	std::map<std::string, function*> functions;
	std::map<int, std::function<void(std::string, function*)>> callbacks;
	std::map<int, std::function<void(function_group*)>> dcallbacks;
};

/**
 * Function implemented in C++ exported to Lua.
 */
class function
{
public:
/**
 * Register function.
 */
	function(function_group& group, const std::string& name) throw(std::bad_alloc);
/**
 * Unregister function.
 */
	virtual ~function() throw();

/**
 * Invoke function.
 */
	virtual int invoke(state& L) = 0;
protected:
	std::string fname;
	function_group& group;
};

/**
 * Register multiple functions at once.
 */
class functions
{
public:
/**
 * Entry in list.
 */
	struct entry
	{
		const std::string& name;
		std::function<int(state& L, parameters& P)> func;
	};
/**
 * Create new functions.
 *
 * Parameter grp: The group to put the functions to.
 * Parameter basetable: The base table to interpret function names relative to.
 * Parameter fnlist: The list of functions to register.
 */
	functions(function_group& grp, const std::string& basetable, std::initializer_list<entry> fnlist);
/**
 * Dtor.
 */
	~functions();
private:
	class fn : public function
	{
	public:
		fn(function_group& grp, const std::string& name, std::function<int(state& L, parameters& P)> _func);
		~fn() throw();
		int invoke(state& L);
	private:
		std::function<int(state& L, parameters& P)> func;
	};
	functions(const functions&);
	functions& operator=(const functions&);
	std::set<fn*> funcs;
};

}

#endif
