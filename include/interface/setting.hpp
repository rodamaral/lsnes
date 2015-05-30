#ifndef _interface__setting__hpp__included__
#define _interface__setting__hpp__included__

#include <string>
#include <stdexcept>
#include <vector>
#include <map>
#include <set>
#include "library/text.hpp"

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
	const text iname;
/**
 * Human-readable value.
 */
	const text hname;
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
	const text iname;
/**
 * Human-readable name.
 */
	const text hname;
/**
 * Regular expression for validation of fretext setting.
 */
	const text regex;
/**
 * The default value.
 */
	const text dflt;
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
	std::vector<text> hvalues() const throw(std::runtime_error);
/**
 * Translate hvalue to ivalue.
 */
	text hvalue_to_ivalue(const text& hvalue) const throw(std::runtime_error);
/**
 * Translate ivalue to index.
 */
	signed ivalue_to_index(const text& ivalue) const throw(std::runtime_error);
/**
 * Validate a value.
 *
 * Parameter value: The value to validate.
 */
	bool validate(const text& value) const;
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
	std::map<text, core_setting> settings;
/**
 * Get specified setting.
 */
	core_setting& operator[](const text& name) { return settings.find(name)->second; }
/**
 * Translate ivalue to index.
 */
	signed ivalue_to_index(std::map<text, text>& values, const text& name) const
		throw(std::runtime_error)
	{
		return settings.find(name)->second.ivalue_to_index(values[name]);
	}
/**
 * Fill a map of settings with defaults.
 */
	void fill_defaults(std::map<text, text>& values) throw(std::bad_alloc);
/**
 * Get set of settings.
 */
	std::set<text> get_setting_set();
};

#endif
