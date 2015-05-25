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
class set;
class superbase;

threads::rlock& get_setting_lock();

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
 * A set of setting variables.
 */
class set
{
public:
/**
 * Set add/drop listener.
 */
	class listener
	{
	public:
/**
 * Dtor.
 */
		virtual ~listener();
/**
 * New item in set.
 */
		virtual void create(set& s, const std::string& name, superbase& svar) = 0;
/**
 * Deleted item from set.
 */
		virtual void destroy(set& s, const std::string& name) = 0;
/**
 * Destroyed the entiere set.
 */
		virtual void kill(set& s) = 0;
	};
/**
 * Create a set.
 */
	set();
/**
 * Destructor.
 */
	~set();
/**
 * Register a supervariable.
 */
	void do_register(const std::string& name, superbase& info);
/**
 * Unregister a supervariable.
 */
	void do_unregister(const std::string& name, superbase& info);
/**
 * Add a callback on new supervariable.
 */
	void add_callback(listener& listener) throw(std::bad_alloc);
/**
 * Drop a callback on new supervariable.
 */
	void drop_callback(listener& listener);
private:
	char dummy;
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
	void do_unregister(const std::string& name, base& _setting) throw(std::bad_alloc);
/**
 * Fire listener.
 */
	void fire_listener(base& var) throw();
/**
 * Add a set of settings.
 */
	void add_set(set& s) throw(std::bad_alloc);
/**
 * Remove a set of settings.
 */
	void drop_set(set& s);
private:
/**
 * Set listener.
 */
	class xlistener : public set::listener
	{
	public:
		xlistener(group& _grp);
		~xlistener();
		void create(set& s, const std::string& name, superbase& sb);
		void destroy(set& s, const std::string& name);
		void kill(set& s);
	private:
		group& grp;
	} _listener;
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
 * Supervariable.
 */
class superbase
{
public:
/**
 * Constructor.
 */
	void _superbase(set& _s, const std::string& iname) throw(std::bad_alloc);
/**
 * Destructor.
 */
	virtual ~superbase() throw();
/**
 * Make a variable.
 */
	virtual base* make(group& grp) = 0;
/**
 * Notify set death.
 */
	void set_died();
private:
	set* s;
	std::string iname;
};

/**
 * Setting variable.
 */
class base
{
public:
/**
 * Constructor.
 */
	base(group& _group, const std::string& iname, const std::string& hname, bool dynamic) throw(std::bad_alloc);
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
/**
 * Notify group death.
 */
	void group_died();
protected:
	base(const base&);
	base& operator=(const base&);
	group* sgroup;
	std::string iname;
	std::string hname;
	bool is_dynamic;
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
		valtype_t defaultvalue, bool dynamic = false)
		: base(sgroup, iname, hname, dynamic)
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
			threads::arlock h(get_setting_lock());
			value = model::read(val);
		}
		sgroup->fire_listener(*this);
	}
/**
 * Get setting.
 */
	std::string str() const throw(std::runtime_error, std::bad_alloc)
	{
		threads::arlock h(get_setting_lock());
		return model::write(value);
	}
/**
 * Set setting.
 */
	void set(valtype_t _value) throw(std::runtime_error, std::bad_alloc)
	{
		{
			threads::arlock h(get_setting_lock());
			if(!model::valid(value))
				throw std::runtime_error("Invalid value");
			value = _value;
		}
		sgroup->fire_listener(*this);
	}
/**
 * Get setting.
 */
	valtype_t get() const throw(std::bad_alloc)
	{
		threads::arlock h(get_setting_lock());
		return model::transform(value);
	}
/**
 * Get setting.
 */
	operator valtype_t() const
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
 * Supervariable with model.
 */
template<class model> class supervariable : public superbase
{
	typedef typename model::valtype_t valtype_t;
	supervariable(const supervariable<model>&);
	supervariable<model>& operator=(const supervariable<model>&);
public:
/**
 * Constructor.
 */
	supervariable(set& _s, const std::string& _iname, const std::string& _hname, valtype_t _defaultvalue)
		throw(std::bad_alloc)
		: s(_s)
	{
		iname = _iname;
		hname = _hname;
		defaultvalue = _defaultvalue;
		_superbase(_s, iname);
	}
/**
 * Destructor.
 */
	~supervariable() throw()
	{
	}
/**
 * Make a variable.
 */
	base* make(group& grp)
	{
		return new variable<model>(grp, iname, hname, defaultvalue, true);
	}
/**
 * Read value in instance.
 */
	valtype_t operator()(group& grp)
	{
		base* b = &grp[iname];
		variable<model>* m = dynamic_cast<variable<model>*>(b);
		if(!m)
			throw std::runtime_error("No such setting in target group");
		return m->get();
	}
/**
 * Write value in instance.
 */
	void operator()(group& grp, valtype_t val)
	{
		base* b = &grp[iname];
		variable<model>* m = dynamic_cast<variable<model>*>(b);
		if(!m)
			throw std::runtime_error("No such setting in target group");
		m->set(val);
	}
private:
	set& s;
	std::string iname;
	std::string hname;
	valtype_t defaultvalue;
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
