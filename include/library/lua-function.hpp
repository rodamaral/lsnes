#ifndef _library__lua_function__hpp__included__
#define _library__lua_function__hpp__included__

#include "lua-base.hpp"
#include "lua-pin.hpp"

namespace lua
{
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
	void do_unregister(const std::string& name);
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
 * Register function pointer as lua function.
 */
class fnptr : public function
{
public:
/**
 * Register.
 */
	fnptr(function_group& group, const std::string& name,
		int (*_fn)(state& L, const std::string& fname))
		: function(group, name)
	{
		fn = _fn;
	}
/**
 * Invoke function.
 */
	int invoke(state& L)
	{
		return fn(L, fname);
	}
private:
	int (*fn)(state& L, const std::string& fname);
};

}

#endif
