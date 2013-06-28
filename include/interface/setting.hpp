#ifndef _interface__setting__hpp__included__
#define _interface__setting__hpp__included__

#include <string>
#include <stdexcept>
#include <vector>
#include <map>
#include <set>

struct core_setting;
struct core_setting_group;

/**
 * A value for setting (structure).
 */
struct core_setting_value_param
{
	const char* iname;
	const char* hname;
	signed index;
};

/**
 * A value for setting.
 */
struct core_setting_value
{
/**
 * Create a new setting value.
 *
 * Parameter _setting: The setting this value is for.
 * Parameter _iname: The internal value for setting.
 * Parameter _hname: The human-readable value for setting.
 */
	core_setting_value(struct core_setting& _setting, const std::string& _iname, const std::string& _hname,
		signed index)
		throw(std::bad_alloc);
/**
 * Destructor.
 */
	~core_setting_value();
/**
 * Setting this is for.
 */
	core_setting& setting;
/**
 * Internal value.
 */
	const std::string iname;
/**
 * Human-readable value.
 */
	const std::string hname;
/**
 * Index.
 */
	signed index;
private:
	core_setting_value(core_setting_value&);
	core_setting_value& operator=(core_setting_value&);
};

/**
 * A setting.
 */
struct core_setting
{
/**
 * Create a new setting.
 *
 * Parameter _group: The group setting is in.
 * Parameter _iname: The internal name of setting.
 * Parameter _hname: The human-readable name of setting.
 * Parameter _dflt: The default value.
 */
	core_setting(core_setting_group& _group, const std::string& _iname, const std::string& _hname,
		const std::string& _dflt) throw(std::bad_alloc);
/**
 * Create a new setting.
 *
 * Parameter _group: The group setting is in.
 * Parameter _iname: The internal name of setting.
 * Parameter _hname: The human-readable name of setting.
 * Parameter _dflt: The default value.
 * Parameter _values: Valid values.
 */
	core_setting(core_setting_group& _group, const std::string& _iname, const std::string& _hname,
		const std::string& _dflt, std::initializer_list<core_setting_value_param> _values)
		throw(std::bad_alloc);
/**
 * Create a new setting with regex.
 *
 * Parameter _group: The group setting is in.
 * Parameter _iname: The internal name of setting.
 * Parameter _hname: The human-readable name of setting.
 * Parameter _dflt: The default value.
 * Parameter _regex: The regular expression used for validation.
 */
	core_setting(core_setting_group& _group, const std::string& _iname, const std::string& _hname,
		const std::string& _dflt, const std::string& _regex) throw(std::bad_alloc);
/**
 * Destructor.
 */
	~core_setting();
/**
 * Internal name.
 */
	const std::string iname;
/**
 * Human-readable name.
 */
	const std::string hname;
/**
 * Regular expression for validation of fretext setting.
 */
	const std::string regex;
/**
 * The default value.
 */
	const std::string dflt;
/**
 * The values.
 */
	std::vector<core_setting_value*> values;
/**
 * Is this setting a boolean?
 */
	bool is_boolean() const throw();
/**
 * Is this setting a freetext setting?
 */
	bool is_freetext() const throw();
/**
 * Get set of human-readable strings.
 */
	std::vector<std::string> hvalues() const throw(std::runtime_error);
/**
 * Translate hvalue to ivalue.
 */
	std::string hvalue_to_ivalue(const std::string& hvalue) const throw(std::runtime_error);
/**
 * Translate ivalue to index.
 */
	signed ivalue_to_index(const std::string& ivalue) const throw(std::runtime_error);
/**
 * Validate a value.
 *
 * Parameter value: The value to validate.
 */
	bool validate(const std::string& value) const;
/**
 * Register a value.
 */
	void do_register(const std::string& name, core_setting_value& value);
/**
 * Unregister a value.
 */
	void do_unregister(const std::string& name);
/**
 * The group setting belongs to.
 */
	struct core_setting_group& group;
private:
	core_setting(core_setting&);
	core_setting& operator=(core_setting&);
};

/**
 * A group of settings.
 */
struct core_setting_group
{
/**
 * Create a new group of settings.
 */
	core_setting_group() throw();
/**
 * Destructor.
 */
	~core_setting_group() throw();
/**
 * Register a setting.
 */
	void do_register(const std::string& name, core_setting& setting);
/**
 * Unregister a setting.
 */
	void do_unregister(const std::string& name);
/**
 * The settings.
 */
	std::map<std::string, core_setting*> settings;
/**
 * Fill a map of settings with defaults.
 */
	void fill_defaults(std::map<std::string, std::string>& values) throw(std::bad_alloc);
/**
 * Get set of settings.
 */
	std::set<std::string> get_setting_set();
private:
	core_setting_group(core_setting_group&);
	core_setting_group& operator=(core_setting_group&);
};

#endif
