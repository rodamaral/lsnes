#ifndef _settings__hpp__included__
#define _settings__hpp__included__

#include <string>
#include <stdexcept>
#include "window.hpp"

/**
 * \brief A setting.
 */
class setting
{
public:
/**
 * \brief Create new setting.
 * 
 * \param name Name of the setting.
 * \throws std::bad_alloc Not enough memory.
 */
	setting(const std::string& name) throw(std::bad_alloc);

/**
 * \brief Remove the setting.
 */
	~setting() throw();

/**
 * \brief Blank a setting.
 * 
 * Set the setting to special blank state.
 * 
 * \throws std::bad_alloc Not enough memory.
 * \throws std::runtime_error Blanking this setting is not allowed (currently).
 */
	virtual void blank() throw(std::bad_alloc, std::runtime_error) = 0;

/**
 * \brief Is this setting set (not blanked)?
 * 
 * \return True if setting is not blanked, false if it is blanked.
 */
	virtual bool is_set() throw() = 0;

/**
 * \brief Set value of setting.
 * 
 * \param value New value for setting.
 * \throws std::bad_alloc Not enough memory.
 * \throws std::runtime_error Setting the setting to this value is not allowed (currently).
 */
	virtual void set(const std::string& value) throw(std::bad_alloc, std::runtime_error) = 0;

/**
 * \brief Get the value of setting.
 * 
 * \return The setting value.
 * \throws std::bad_alloc Not enough memory.
 */
	virtual std::string get() throw(std::bad_alloc) = 0;
protected:
	std::string settingname;
};

/**
 * \brief Look up setting and call set() on it.
 * 
 * \param _setting The setting to set.
 * \param value The value to set it into.
 * \throws std::bad_alloc Not enough memory.
 * \throws std::runtime_error Setting the setting to this value is not allowed (currently), or no such setting.
 */
void setting_set(const std::string& _setting, const std::string& value) throw(std::bad_alloc, std::runtime_error);

/**
 * \brief Look up setting and call blank() on it.
 * 
 * \param _setting The setting to blank.
 * \throws std::bad_alloc Not enough memory.
 * \throws std::runtime_error Blanking this setting is not allowed (currently), or no such setting.
 */
void setting_blank(const std::string& _setting) throw(std::bad_alloc, std::runtime_error);

/**
 * \brief Look up setting and call get() on it.
 * 
 * \param _setting The setting to get.
 * \return The setting value.
 * \throws std::bad_alloc Not enough memory.
 * \throws std::runtime_error No such setting.
 */
std::string setting_get(const std::string& _setting) throw(std::bad_alloc, std::runtime_error);

/**
 * \brief Look up setting and call is_set() on it.
 * 
 * \param _setting The setting to get.
 * \return Flase if setting is not blanked, true if it is blanked (note: this is reverse of is_set().
 * \throws std::bad_alloc Not enough memory.
 * \throws std::runtime_error No such setting.
 */
bool setting_isblank(const std::string& _setting) throw(std::bad_alloc, std::runtime_error);

/**
 * \brief Print all settings and values.
 */
void setting_print_all(window* win) throw(std::bad_alloc);

class numeric_setting : public setting
{
public:
	numeric_setting(const std::string& sname, int32_t minv, int32_t maxv, int32_t dflt) throw(std::bad_alloc);
	void blank() throw(std::bad_alloc, std::runtime_error);
	bool is_set() throw();
	void set(const std::string& value) throw(std::bad_alloc, std::runtime_error);
	std::string get() throw(std::bad_alloc);
	operator int32_t() throw();
private:
	int32_t value;
	int32_t minimum;
	int32_t maximum;
};

class boolean_setting : public setting
{
public:
	boolean_setting(const std::string& sname, bool dflt) throw(std::bad_alloc);
	void blank() throw(std::bad_alloc, std::runtime_error);
	bool is_set() throw();
	void set(const std::string& value) throw(std::bad_alloc, std::runtime_error);
	std::string get() throw(std::bad_alloc);
	operator bool() throw();
private:
	bool value;
};

#endif
