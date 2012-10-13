#ifndef _library__settings__hpp__included__
#define _library__settings__hpp__included__

#include <string>
#include <set>
#include <stdexcept>
#include <iostream>
#include <map>
#include <list>
#include "library/workthread.hpp"

class setting;
class setting_group;

/**
 * A settings listener.
 */
struct setting_listener
{
/**
 * Destructor.
 */
	virtual ~setting_listener() throw();
/**
 * Listen for setting being blanked.
 */
	virtual void blanked(setting_group& group, const std::string& sname) = 0;
/**
 * Listen for setting changing value.
 */
	virtual void changed(setting_group& group, const std::string& sname, const std::string& newval) = 0;
};

/**
 * A group of settings.
 */
class setting_group
{
public:
/**
 * Create a new group of settings.
 */
	setting_group() throw(std::bad_alloc);
/**
 * Destroy a group of settings.
 */
	~setting_group() throw();
/**
 * Can the setting be blanked?
 */
	bool blankable(const std::string& name) throw(std::bad_alloc, std::runtime_error);
/**
 * Look up setting and try to blank it.
 *
 * parameter name: Name of setting to blank.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Blanking this setting is not allowed (currently). Or setting does not exist.
 */
	void blank(const std::string& name) throw(std::bad_alloc, std::runtime_error);
/**
 * Look up a setting and see if it is set (not blanked)?
 *
 * parameter name: Name of setting to check.
 * returns: True if setting is not blanked, false if it is blanked.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Setting does not exist.
 */
	bool is_set(const std::string& name) throw(std::bad_alloc, std::runtime_error);
/**
 * Look up setting and set it.
 *
 * parameter name: Name of the setting.
 * parameter value: New value for setting.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Setting the setting to this value is not allowed (currently). Or setting does not exist.
 */
	void set(const std::string& name, const std::string& value) throw(std::bad_alloc, std::runtime_error);
/**
 * Look up setting an get value of it.
 *
 * returns: The setting value.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Setting does not exist.
 */
	std::string get(const std::string& name) throw(std::bad_alloc, std::runtime_error);
/**
 * Get set of all settings.
 */
	std::set<std::string> get_settings_set() throw(std::bad_alloc);
/**
 * Enable/Disable storage mode.
 *
 * In storage mode, invalid values are stored in addition to being rejected.
 */
	void set_storage_mode(bool enable) throw();
/**
 * Get invalid settings cache.
 */
	std::map<std::string, std::string> get_invalid_values() throw(std::bad_alloc);
/**
 * Add a listener.
 */
	void add_listener(struct setting_listener& listener) throw(std::bad_alloc);
/**
 * Remove a listener.
 */
	void remove_listener(struct setting_listener& listener) throw(std::bad_alloc);
/**
 * Register a setting.
 */
	void do_register(const std::string& name, setting& _setting) throw(std::bad_alloc);
/**
 * Unregister a setting.
 */
	void do_unregister(const std::string& name) throw(std::bad_alloc);
private:
	setting* get_by_name(const std::string& name);
	std::map<std::string, class setting*> settings;
	std::map<std::string, std::string> invalid_values;
	std::set<struct setting_listener*> listeners;
	mutex_class lock;
	bool storage_mode;
};

/**
 * A setting.
 */
class setting
{
public:
/**
 * Create new setting.
 *
 * parameter group: The group setting is in.
 * parameter name: Name of the setting.
 * throws std::bad_alloc: Not enough memory.
 */
	setting(setting_group& group, const std::string& name) throw(std::bad_alloc);

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
 * Is this setting set (not blanked)?
 *
 * returns: True if setting is not blanked, false if it is blanked.
 */
	virtual bool is_set() throw() = 0;

/**
 * Set value of setting.
 *
 * parameter value: New value for setting.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Setting the setting to this value is not allowed (currently).
 */
	virtual void set(const std::string& value) throw(std::bad_alloc, std::runtime_error) = 0;
/**
 * Get the value of setting.
 *
 * returns: The setting value.
 * throws std::bad_alloc: Not enough memory.
 */
	virtual std::string get() throw(std::bad_alloc) = 0;
/**
 * Lock holder
 */
	struct lock_holder
	{
		lock_holder(setting* t) { (targ = t)->mut.lock(); }
		~lock_holder() { targ->mut.unlock(); }
	private:
		setting* targ;
	};
	friend struct lock_holder;
protected:
	std::string settingname;
private:
	mutex_class mut;
	setting_group& in_group;
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
 * parameter group: Group setting is in.
 * parameter sname: Name of the setting.
 * parameter minv: Minimum value for the setting.
 * parameter maxv: Maximum value for the setting.
 * parameter dflt: Default (initial) value for the setting.
 * throws std::bad_alloc: Not enough memory.
 */
	numeric_setting(setting_group& group, const std::string& sname, int32_t minv, int32_t maxv, int32_t dflt)
		throw(std::bad_alloc);
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
 * parameter group: Group setting is in.
 * parameter sname: Name of the setting.
 * parameter dflt: Default (initial) value for the setting.
 * throws std::bad_alloc: Not enough memory.
 */
	boolean_setting(setting_group& group, const std::string& sname, bool dflt) throw(std::bad_alloc);
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
/**
 * Create a new path setting.
 *
 * Parameter group: The group setting is in.
 * Parameter sname: Name of the setting.
 */
	path_setting(setting_group& group, const std::string& sname) throw(std::bad_alloc);
	bool blank(bool really) throw(std::bad_alloc, std::runtime_error);
	bool is_set() throw();
	void set(const std::string& value) throw(std::bad_alloc, std::runtime_error);
	std::string get() throw(std::bad_alloc);
/**
 * Read the value of the setting.
 */
	operator std::string();
private:
	bool _default;
	std::string path;
};

#endif
