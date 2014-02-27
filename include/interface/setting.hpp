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
 * A setting (structure)
 */
struct core_setting_param
{
	const char* iname;
	const char* hname;
	const char* dflt;
	std::vector<core_setting_value_param> values;
	const char* regex;
};

/**
 * A value for setting.
 */
struct core_setting_value
{
/**
 * Create a new setting value.
 */
	core_setting_value(const core_setting_value_param& p) throw(std::bad_alloc);
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
};

/**
 * A setting.
 */
struct core_setting
{
/**
 * Create a new setting.
 */
	core_setting(const core_setting_param& p);
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
	std::vector<core_setting_value> values;
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
};

/**
 * A group of settings.
 */
struct core_setting_group
{
	core_setting_group();
/**
 * Create a new group of settings.
 */
	core_setting_group(std::initializer_list<core_setting_param> settings);
/**
 * Create a new group of settings.
 */
	core_setting_group(std::vector<core_setting_param> settings);
/**
 * The settings.
 */
	std::map<std::string, core_setting> settings;
/**
 * Get specified setting.
 */
	core_setting& operator[](const std::string& name) { return settings.find(name)->second; }
/**
 * Translate ivalue to index.
 */
	signed ivalue_to_index(std::map<std::string, std::string>& values, const std::string& name) const
		throw(std::runtime_error)
	{
		return settings.find(name)->second.ivalue_to_index(values[name]);
	}
/**
 * Fill a map of settings with defaults.
 */
	void fill_defaults(std::map<std::string, std::string>& values) throw(std::bad_alloc);
/**
 * Get set of settings.
 */
	std::set<std::string> get_setting_set();
};

#endif
