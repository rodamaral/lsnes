#ifndef _settings__hpp__included__
#define _settings__hpp__included__

#include "core/window.hpp"

#include <string>
#include <set>
#include <stdexcept>
#include <iostream>

/**
 * A setting.
 */
class setting
{
public:
/**
 * Create new setting.
 *
 * parameter name: Name of the setting.
 * throws std::bad_alloc: Not enough memory.
 */
	setting(const std::string& name) throw(std::bad_alloc);

/**
 * Remove the setting.
 */
	~setting() throw();

/**
 * Set the setting to special blank state. Not all settings can be blanked.
 *
 * Parameter really: Do dummy clear if false, otherwise try it for real.
 * Returns: True on success, false on failure.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Blanking this setting is not allowed (currently).
 */
	virtual bool blank(bool really) throw(std::bad_alloc, std::runtime_error);

/**
 * Can the setting be blanked?
 */
	static bool blankable(const std::string& name) throw(std::bad_alloc, std::runtime_error);

/**
 * Look up setting and try to blank it.
 *
 * parameter name: Name of setting to blank.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Blanking this setting is not allowed (currently). Or setting does not exist.
 */
	static void blank(const std::string& name) throw(std::bad_alloc, std::runtime_error);

/**
 * Is this setting set (not blanked)?
 *
 * returns: True if setting is not blanked, false if it is blanked.
 */
	virtual bool is_set() throw() = 0;

/**
 * Look up a setting and see if it is set (not blanked)?
 *
 * parameter name: Name of setting to check.
 * returns: True if setting is not blanked, false if it is blanked.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Setting does not exist.
 */
	static bool is_set(const std::string& name) throw(std::bad_alloc, std::runtime_error);

/**
 * Set value of setting.
 *
 * parameter value: New value for setting.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Setting the setting to this value is not allowed (currently).
 */
	virtual void set(const std::string& value) throw(std::bad_alloc, std::runtime_error) = 0;

/**
 * Look up setting and set it.
 *
 * parameter name: Name of the setting.
 * parameter value: New value for setting.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Setting the setting to this value is not allowed (currently). Or setting does not exist.
 */
	static void set(const std::string& name, const std::string& value) throw(std::bad_alloc, std::runtime_error);

/**
 * Get the value of setting.
 *
 * returns: The setting value.
 * throws std::bad_alloc: Not enough memory.
 */
	virtual std::string get() throw(std::bad_alloc) = 0;

/**
 * Look up setting an get value of it.
 *
 * returns: The setting value.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Setting does not exist.
 */
	static std::string get(const std::string& name) throw(std::bad_alloc, std::runtime_error);

/**
 * Get set of all settings.
 */
	static std::set<std::string> get_settings_set() throw(std::bad_alloc);
/**
 * Lock holder
 */
	struct lock_holder
	{
		lock_holder(setting* t) { (targ = t)->mut->lock(); }
		~lock_holder() { targ->mut->unlock(); }
	private:
		setting* targ;
	};
	friend struct lock_holder;
protected:
	std::string settingname;
private:
	static setting* get_by_name(const std::string& name);
	mutex* mut;
};

/**
 * Setting having numeric value.
 */
class numeric_setting : public setting
{
public:
/**
 * Create a new numeric setting.
 *
 * parameter sname: Name of the setting.
 * parameter minv: Minimum value for the setting.
 * parameter maxv: Maximum value for the setting.
 * parameter dflt: Default (initial) value for the setting.
 * throws std::bad_alloc: Not enough memory.
 */
	numeric_setting(const std::string& sname, int32_t minv, int32_t maxv, int32_t dflt) throw(std::bad_alloc);
/**
 * Returns true (these settings are always set).
 */
	bool is_set() throw();
/**
 * Set the value of setting. Accepts only numeric values.
 *
 * parameter value: New value.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Invalid value.
 */
	void set(const std::string& value) throw(std::bad_alloc, std::runtime_error);
/**
 * Gets the value of the setting.
 *
 * returns: Value of setting as string.
 * throws std::bad_alloc: Not enough memory.
 */
	std::string get() throw(std::bad_alloc);
/**
 * Get the value of setting as numeric.
 *
 * returns: Value of the setting as numeric.
 */
	operator int32_t() throw();
private:
	int32_t value;
	int32_t minimum;
	int32_t maximum;
};

/**
 * Setting having boolean value.
 */
class boolean_setting : public setting
{
public:
/**
 * Create a new boolean setting.
 *
 * parameter sname: Name of the setting.
 * parameter dflt: Default (initial) value for the setting.
 * throws std::bad_alloc: Not enough memory.
 */
	boolean_setting(const std::string& sname, bool dflt) throw(std::bad_alloc);
/**
 * Returns true (these settings are always set).
 */
	bool is_set() throw();
/**
 * Set the value of setting.
 *
 * The following values are accepted as true: true, yes, on, 1, enable and enabled.
 * The following values are accepted as false: false, no, off, 0, disable and disabled.
 *
 * parameter value: New value.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Invalid value.
 */
	void set(const std::string& value) throw(std::bad_alloc, std::runtime_error);
/**
 * Gets the value of the setting.
 *
 * returns: Value of setting as string.
 * throws std::bad_alloc: Not enough memory.
 */
	std::string get() throw(std::bad_alloc);
/**
 * Get the value of setting as boolean.
 *
 * returns: Value of the setting as boolean.
 */
	operator bool() throw();
private:
	bool value;
};

/**
 * Setting having path value.
 */
class path_setting : public setting
{
public:
	path_setting(const std::string& sname) throw(std::bad_alloc);
	bool blank(bool really) throw(std::bad_alloc, std::runtime_error);
	bool is_set() throw();
	void set(const std::string& value) throw(std::bad_alloc, std::runtime_error);
	std::string get() throw(std::bad_alloc);
	operator std::string();
private:
	bool _default;
	std::string path;
};


#endif
