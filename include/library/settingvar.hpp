#ifndef _library__settingvar__hpp__included__
#define _library__settingvar__hpp__included__

#include <string>
#include <map>
#include <set>
#include "threads.hpp"
#include "string.hpp"
#include <string>

namespace settingvar
{
class base;
class group;
class description;

/**
 * A settings listener.
 */
struct listener
{
/**
 * Destructor.
 */
	virtual ~listener() throw();
/**
 * Listen for setting changing value.
 */
	virtual void on_setting_change(group& _group, const base& val) = 0;
};

/**
 * Group of setting variables.
 */
class group
{
public:
/**
 * Constructor.
 */
	group() throw(std::bad_alloc);
/**
 * Destructor.
 */
	~group() throw();
/**
 * Get all settings.
 */
	std::set<std::string> get_settings_set() throw(std::bad_alloc);
/**
 * Get setting.
 */
	base& operator[](const std::string& name);
/**
 * Add a listener.
 */
	void add_listener(struct listener& _listener) throw(std::bad_alloc);
/**
 * Remove a listener.
 */
	void remove_listener(struct listener& _listener) throw(std::bad_alloc);
/**
 * Register a setting.
 */
	void do_register(const std::string& name, base& _setting) throw(std::bad_alloc);
/**
 * Unregister a setting.
 */
	void do_unregister(const std::string& name, base* dummy) throw(std::bad_alloc);
/**
 * Fire listener.
 */
	void fire_listener(base& var) throw();
private:
	std::map<std::string, class base*> settings;
	std::set<struct listener*> listeners;
	threads::lock lock;
};

/**
 * Write-trough value cache.
 */
class cache
{
public:
/**
 * Constructor.
 */
	cache(group& grp);
/**
 * Enumerate contents.
 *
 * Note: This reads cached values in perference to actual values.
 */
	std::map<std::string, std::string> get_all();
/**
 * Enumerate valid keys.
 *
 * Returns: The set of actually valid keys.
 */
	std::set<std::string> get_keys();
/**
 * Set a value.
 *
 * Parameter name: Name of the setting.
 * Parameter value: New value for the setting.
 * Parameter allow_invalid: Cache value if invalid, instead of throwing.
 * Throws std::runtime_error: Failed to set the setting and invalid values not allowed.
 *
 * Note: If setting has cached value and setting it succeeds, the cached value is cleared.
 */
	void set(const std::string& name, const std::string& value, bool allow_invalid = false) throw(std::bad_alloc,
		std::runtime_error);
/**
 * Get a value.
 *
 * Parameter name: Name of the setting.
 * Return: Actual value of the setting.
 * Throws std::runtime_error: Setting doesn't exist.
 */
	std::string get(const std::string& name) throw(std::bad_alloc, std::runtime_error);
/**
 * Get descriptor for.
 */
	const description& get_description(const std::string& name) throw(std::bad_alloc,
		std::runtime_error);
/**
 * Get human-readable name.
 *
 * Parameter name: Name of the setting.
 * Return: Human-readable name of the setting.
 * Throws std::runtime_error: Setting doesn't exist.
 */
	std::string get_hname(const std::string& name) throw(std::bad_alloc, std::runtime_error);
private:
	group& grp;
	threads::lock lock;
	std::map<std::string, std::string> badcache;
};

/**
 * Enumeration.
 */
struct enumeration
{
	enumeration(std::initializer_list<const char*> v)
	{
		unsigned x = 0;
		for(auto i : v) {
			values[bound = x++] = i;
		}
	}
	std::string get(unsigned val) { return values.count(val) ? values[val] : ""; }
	unsigned max_val() { return bound; }
private:
	std::map<unsigned, std::string> values;
	unsigned bound;
};

/**
 * Description of setting.
 */
struct description
{
	enum _type
	{
		T_BOOLEAN,
		T_NUMERIC,
		T_STRING,
		T_PATH,
		T_ENUMERATION
	};
	_type type;
	int64_t min_val;
	int64_t max_val;
	enumeration* _enumeration;
};

/**
 * Get the description.
 */
template<class T> static class description& description_get(T dummy);

/**
 * Setting variable.
 */
class base
{
public:
/**
 * Constructor.
 */
	base(group& _group, const std::string& iname, const std::string& hname)
		throw(std::bad_alloc);
/**
 * Destructor.
 */
	virtual ~base() throw();
/**
 * Set setting.
 */
	virtual void str(const std::string& val) throw(std::runtime_error, std::bad_alloc) = 0;
/**
 * Get setting.
 */
	virtual std::string str() const throw(std::runtime_error, std::bad_alloc) = 0;
/**
 * Get setting name.
 */
	const std::string& get_iname() const throw() { return iname; }
	const std::string& get_hname() const throw() { return hname; }
/**
 * Get setting description.
 */
	virtual const description& get_description() const throw() = 0;
protected:
	base(const base&);
	base& operator=(const base&);
	group& sgroup;
	std::string iname;
	std::string hname;
	mutable threads::lock lock;
};

/**
 * Setting variable.
 */
template<class model> class variable : public base
{
	typedef typename model::valtype_t valtype_t;
	variable(const variable<model>&);
	variable<model>& operator=(const variable<model>&);
public:
/**
 * Constructor.
 */
	variable(group& sgroup, const std::string& iname, const std::string& hname,
		valtype_t defaultvalue)
		: base(sgroup, iname, hname)
	{
		value = defaultvalue;
	}
/**
 * Destructor.
 */
	virtual ~variable() throw()
	{
	}
/**
 * Set setting.
 */
	void str(const std::string& val) throw(std::runtime_error, std::bad_alloc)
	{
		{
			threads::alock h(lock);
			value = model::read(val);
		}
		sgroup.fire_listener(*this);
	}
/**
 * Get setting.
 */
	std::string str() const throw(std::runtime_error, std::bad_alloc)
	{
		threads::alock h(lock);
		return model::write(value);
	}
/**
 * Set setting.
 */
	void set(valtype_t _value) throw(std::runtime_error, std::bad_alloc)
	{
		{
			threads::alock h(lock);
			if(!model::valid(value))
				throw std::runtime_error("Invalid value");
			value = _value;
		}
		sgroup.fire_listener(*this);
	}
/**
 * Get setting.
 */
	valtype_t get() throw(std::bad_alloc)
	{
		threads::alock h(lock);
		return model::transform(value);
	}
/**
 * Get setting.
 */
	operator valtype_t()
	{
		return get();
	}
/**
 * Get setting description.
 */
	const description& get_description() const throw()
	{
		return description_get(dummy);
	}
private:
	valtype_t value;
	model dummy;
};

/**
 * Yes-no.
 */
struct yes_no
{
	static const char* enable;
	static const char* disable;
};

/**
 * Model: Boolean.
 */
template<typename values> struct model_bool
{
	typedef bool valtype_t;
	static bool valid(bool val) { return true; /* Any boolean is valid boolean. */ }
	static bool read(const std::string& val)
	{
		int x = string_to_bool(val);
		if(x < 0)
			throw std::runtime_error("Invalid boolean value");
		return (x != 0);
	}
	static std::string write(bool val)
	{
		return val ? values::enable : values::disable;
	}
	static bool transform(bool val) { return val; }
};

template<typename values> description& description_get(
	model_bool<values> X)
{
	static description x;
	static bool init = false;
	if(!init) {
		x.type = description::T_BOOLEAN;
		init = true;
	}
	return x;
}

/**
 * Model: Integer.
 */
template<int32_t minimum, int32_t maximum> struct model_int
{
	typedef int32_t valtype_t;
	static bool valid(int32_t val) { return (val >= minimum && val <= maximum); }
	static int32_t read(const std::string& val)
	{
		int x = parse_value<int32_t>(val);
		if(x < minimum || x > maximum)
			(stringfmt() << "Value out of range (" << minimum << " to " << maximum << ")").throwex();
		return x;
	}
	static std::string write(int32_t val)
	{
		return (stringfmt() << val).str();
	}
	static int transform(int val) { return val; }
};

template<int32_t m, int32_t M> description& description_get(model_int<m, M> X)
{
	static description x;
	static bool init = false;
	if(!init) {
		x.type = description::T_NUMERIC;
		x.min_val = m;
		x.max_val = M;
		init = true;
	}
	return x;
}

/**
 * Model: Path.
 */
struct model_path
{
	typedef std::string valtype_t;
	static bool valid(std::string val) { return true; /* Any boolean is valid boolean. */ }
	static std::string read(const std::string& val)
	{
		return val;
	}
	static std::string write(std::string val)
	{
		return val;
	}
	static std::string transform(std::string val)
	{
		return (val != "") ? val : ".";
	}
};

template<> description& description_get(model_path X)
{
	static description x;
	static bool init = false;
	if(!init) {
		x.type = description::T_PATH;
		init = true;
	}
	return x;
}

/**
 * Model: Enumerated.
 */
template<enumeration* e> struct model_enumerated
{
	typedef unsigned valtype_t;
	static bool valid(unsigned val) { return (val <= e->max_val()); }
	static unsigned read(const std::string& val)
	{
		for(unsigned i = 0; i <= e->max_val(); i++)
			if(val == e->get(i))
				return i;
		unsigned x = parse_value<unsigned>(val);
		if(x > e->max_val())
			(stringfmt() << "Value out of range (0  to " << e->max_val() << ")").throwex();
		return x;
	}
	static std::string write(unsigned val)
	{
		return e->get(val);
	}
	static int transform(int val) { return val; }
};

template<enumeration* e> description& description_get(model_enumerated<e> X)
{
	static description x;
	static bool init = false;
	if(!init) {
		x.type = description::T_ENUMERATION;
		x._enumeration = e;
		init = true;
	}
	return x;
}
}

#endif
