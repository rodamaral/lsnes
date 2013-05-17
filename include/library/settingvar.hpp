#ifndef _library__settingvar__hpp__included__
#define _library__settingvar__hpp__included__

#include <string>
#include <map>
#include <set>
#include "threadtypes.hpp"
#include "string.hpp"
#include <string>

class setting_var_base;
class setting_var_group;
class setting_var_description;

/**
 * A settings listener.
 */
struct setting_var_listener
{
/**
 * Destructor.
 */
	virtual ~setting_var_listener() throw();
/**
 * Listen for setting changing value.
 */
	virtual void on_setting_change(setting_var_group& group, const setting_var_base& val) = 0;
};

/**
 * Group of setting variables.
 */
class setting_var_group
{
public:
/**
 * Constructor.
 */
	setting_var_group() throw(std::bad_alloc);
/**
 * Destructor.
 */
	~setting_var_group() throw();
/**
 * Get all settings.
 */
	std::set<std::string> get_settings_set() throw(std::bad_alloc);
/**
 * Get setting.
 */
	setting_var_base& operator[](const std::string& name);
/**
 * Add a listener.
 */
	void add_listener(struct setting_var_listener& listener) throw(std::bad_alloc);
/**
 * Remove a listener.
 */
	void remove_listener(struct setting_var_listener& listener) throw(std::bad_alloc);
/**
 * Register a setting.
 */
	void do_register(const std::string& name, setting_var_base& _setting) throw(std::bad_alloc);
/**
 * Unregister a setting.
 */
	void do_unregister(const std::string& name) throw(std::bad_alloc);
/**
 * Fire listener.
 */
	void fire_listener(setting_var_base& var) throw();
private:
	std::map<std::string, class setting_var_base*> settings;
	std::set<struct setting_var_listener*> listeners;
	mutex_class lock;
};

/**
 * Write-trough value cache.
 */
class setting_var_cache
{
public:
/**
 * Constructor.
 */
	setting_var_cache(setting_var_group& grp);
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
	const setting_var_description& get_description(const std::string& name) throw(std::bad_alloc,
		std::runtime_error);
private:
	setting_var_group& grp;
	mutex_class lock;
	std::map<std::string, std::string> badcache;
};

/**
 * Description of setting.
 */
struct setting_var_description
{
	enum _type
	{
		T_BOOLEAN,
		T_NUMERIC,
		T_STRING,
		T_PATH
	};
	_type type;
	int64_t min_val;
	int64_t max_val;
};

/**
 * Get the description.
 */
template<class T> static class setting_var_description& setting_var_description_get(T dummy);

/**
 * Setting variable.
 */
class setting_var_base
{
public:
/**
 * Constructor.
 */
	setting_var_base(setting_var_group& group, const std::string& iname, const std::string& hname)
		throw(std::bad_alloc);
/**
 * Destructor.
 */
	virtual ~setting_var_base() throw();
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
	virtual const setting_var_description& get_description() const throw() = 0;
protected:
	setting_var_base(const setting_var_base&);
	setting_var_base& operator=(const setting_var_base&);
	setting_var_group& group;
	std::string iname;
	std::string hname;
	mutable mutex_class lock;
};

/**
 * Setting variable.
 */
template<class model> class setting_var : public setting_var_base
{
	typedef typename model::valtype_t valtype_t;
	setting_var(const setting_var<model>&);
	setting_var<model>& operator=(const setting_var<model>&);
public:
/**
 * Constructor.
 */
	setting_var(setting_var_group& group, const std::string& iname, const std::string& hname,
		valtype_t defaultvalue)
		: setting_var_base(group, iname, hname)
	{
		value = defaultvalue;
	}
/**
 * Destructor.
 */
	virtual ~setting_var() throw()
	{
	}
/**
 * Set setting.
 */
	void str(const std::string& val) throw(std::runtime_error, std::bad_alloc)
	{
		{
			umutex_class h(lock);
			value = model::read(val);
		}
		group.fire_listener(*this);
	}
/**
 * Get setting.
 */
	std::string str() const throw(std::runtime_error, std::bad_alloc)
	{
		umutex_class h(lock);
		return model::write(value);
	}
/**
 * Set setting.
 */
	void set(valtype_t _value) throw(std::runtime_error, std::bad_alloc)
	{
		{
			umutex_class h(lock);
			if(!model::valid(value))
				throw std::runtime_error("Invalid value");
			value = _value;
		}
		group.fire_listener(*this);
	}
/**
 * Get setting.
 */
	valtype_t get() throw(std::bad_alloc)
	{
		umutex_class h(lock);
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
	const setting_var_description& get_description() const throw()
	{
		return setting_var_description_get(dummy);
	}
private:
	valtype_t value;
	model dummy;
};

/**
 * Yes-no.
 */
struct setting_yes_no
{
	static const char* enable;
	static const char* disable;
};

/**
 * Model: Boolean.
 */
template<typename values> struct setting_var_model_bool
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

template<typename values> setting_var_description& setting_var_description_get(
	setting_var_model_bool<values> X)
{
	static setting_var_description x;
	static bool init = false;
	if(!init) {
		x.type = setting_var_description::T_BOOLEAN;
		init = true;
	}
	return x;
}

/**
 * Model: Integer.
 */
template<int32_t minimum, int32_t maximum> struct setting_var_model_int
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

template<int32_t m, int32_t M> setting_var_description& setting_var_description_get(setting_var_model_int<m, M> X)
{
	static setting_var_description x;
	static bool init = false;
	if(!init) {
		x.type = setting_var_description::T_NUMERIC;
		x.min_val = m;
		x.max_val = M;
		init = true;
	}
	return x;
}

/**
 * Model: Path.
 */
struct setting_var_model_path
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

template<> setting_var_description& setting_var_description_get(setting_var_model_path X)
{
	static setting_var_description x;
	static bool init = false;
	if(!init) {
		x.type = setting_var_description::T_PATH;
		init = true;
	}
	return x;
}

#endif
