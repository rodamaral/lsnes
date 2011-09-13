#ifndef _keymapper__hpp__included__
#define _keymapper__hpp__included__

#include <string>
#include <sstream>
#include <stdexcept>
#include <list>
#include <set>
#include <iostream>
#include "window.hpp"
#include "misc.hpp"

/**
 * \brief Fixup command according to key polarity.
 * 
 * Takes in a raw command and returns the command that should be actually executed given the key polarity.
 * 
 * \param cmd Raw command.
 * \param polarity Polarity (True => Being pressed, False => Being released).
 * \return The fixed command, "" if no command should be executed.
 * \throws std::bad_alloc Not enough memory.
 */
std::string fixup_command_polarity(std::string cmd, bool polarity) throw(std::bad_alloc);

/**
 * \brief Alias-expanding command handler.
 */
struct aliasexpand_commandhandler : public commandhandler
{
public:
/**
 * \brief Constructor.
 */
	aliasexpand_commandhandler() throw();

/**
 * \brief Destructor.
 */
	~aliasexpand_commandhandler() throw();

/**
 * \brief Run specified command, expanding aliases.
 * 
 * Runs the specified command, expanding any possible aliases, including running scripts and internally recognizing
 * many special command classes (bind commands, alias commands, dumper commands, settings and graphics platform
 * commands).
 * 
 * This won't recursively expand aliases nor scripts.
 * 
 * \param cmd The command
 * \param win The context to graphics platform.
 * \throws std::bad_alloc Not enough memory.
 */
	void docommand(std::string& cmd, window* win) throw(std::bad_alloc);

/**
 * \brief Run specified command, expanding aliases.
 * 
 * Runs the specified command, expanding any possible aliases, including running scripts and internally recognizing
 * many special command classes (bind commands, alias commands, dumper commands, settings and graphics platform
 * commands).
 * 
 * This won't recursively expand aliases nor scripts. The diffrence from the other version is that explicit list of
 * commands that have been recursed through is given.
 * 
 * \param cmd The command
 * \param win The context to graphics platform.
 * \param recursive_commands The set of commands already recursively executed.
 * \throws std::bad_alloc Not enough memory.
 */
	void docommand(std::string& cmd, window* win, std::set<std::string>& recursive_commands) throw(std::bad_alloc);

/**
 * \brief Run specified command, no alias expansion.
 * 
 * Runs the specified command (is not of those internally handled classes) without alias / script expansion.
 * 
 * \param cmd The command
 * \param win The context to graphics platform.
 * \throws std::bad_alloc Not enough memory.
 * \throws std::runtime_error Error running command.
 */
	virtual void docommand2(std::string& cmd, window* win) throw(std::bad_alloc, std::runtime_error) = 0;
private:
	aliasexpand_commandhandler(const aliasexpand_commandhandler&);
	aliasexpand_commandhandler& operator=(const aliasexpand_commandhandler&);
	
};

/**
 * \brief Keyboard mapper.
 * 
 * This class handles internals of mapping events from keyboard buttons and pseudo-buttons. The helper class T has
 * to have the following:
 * 
 * unsigned T::mod_str(const std::string& mod): Translate modifiers set mod into modifier mask.
 * typedef T::internal_keysymbol: Key symbol to match against. Needs to have == operator available.
 * T::internal_keysymbol key_str(const std::string& keyname): Translate key name to key to match against.
 * typedef T::keysymbol: Key symbol from keyboard (or pseudo-button). Carries modifiers too.
 * unsigned mod_key(T::keysymbol key): Get modifier mask for keyboard key.
 * T::internal_keysymbol key_key(T::keysymbol key): Get key symbol to match against for given keyboard key.
 * std::string T::name_key(unsigned mod, unsigned modmask, T::internal_keysymbol key): Print name of key with mods.
 */
template<class T>
class keymapper
{
public:
/**
 * \brief Bind a key.
 * 
 * Binds a key, erroring out if binding would conflict with existing one.
 * 
 * \param mod Modifier set to require to be pressed.
 * \param modmask Modifier set to take into account.
 * \param keyname Key to bind the action to.
 * \param command The command to bind.
 * \throws std::bad_alloc Not enough memory.
 * \throws std::runtime_error The binding would conflict with existing one.
 */
	void bind(std::string mod, std::string modmask, std::string keyname, std::string command) throw(std::bad_alloc,
		std::runtime_error)
	{
		unsigned _mod = T::mod_str(mod);
		unsigned _modmask = T::mod_str(modmask);
		if(_mod & ~_modmask)
			throw std::runtime_error("Mod must be subset of modmask");
		typename T::internal_keysymbol _keyname = T::key_str(keyname);
		/* Check for collisions. */
		for(auto i = bindings.begin(); i != bindings.end(); i++) {
			if(!(_keyname == i->symbol))
				continue;
			if((_mod & _modmask & i->modmask) != (i->mod & _modmask & i->modmask))
				continue;
			throw std::runtime_error("Would conflict with " + T::name_key(i->mod, i->modmask, i->symbol));
		}
		struct kdata k;
		k.mod = _mod;
		k.modmask = _modmask;
		k.symbol = _keyname;
		k.command = command;
		bindings.push_back(k);
	}

/**
 * \brief Unbind a key.
 * 
 * Unbinds a key, erroring out if binding does not exist..
 * 
 * \param mod Modifier set to require to be pressed.
 * \param modmask Modifier set to take into account.
 * \param keyname Key to bind the action to.
 * \throws std::bad_alloc Not enough memory.
 * \throws std::runtime_error The binding does not exist.
 */
	void unbind(std::string mod, std::string modmask, std::string keyname) throw(std::bad_alloc,
		std::runtime_error)
	{
		unsigned _mod = T::mod_str(mod);
		unsigned _modmask = T::mod_str(modmask);
		typename T::internal_keysymbol _keyname = T::key_str(keyname);
		for(auto i = bindings.begin(); i != bindings.end(); i++) {
			if(!(_keyname == i->symbol) || _mod != i->mod || _modmask != i->modmask)
				continue;
			bindings.erase(i);
			return;
		}
		throw std::runtime_error("No such binding");
	}

/**
 * \brief Map key symbol from keyboard + polarity into a command.
 * 
 * Takes in symbol from keyboard and polarity. Outputs command to run.
 * 
 * \param sym Symbol from keyboard (with its mods).
 * \param polarity True if key is being pressed, false if being released.
 * \return The command to run. "" if none.
 * \throws std::bad_alloc Not enough memory.
 */
	std::string map(typename T::keysymbol sym, bool polarity) throw(std::bad_alloc)
	{
		unsigned _mod = T::mod_key(sym);
		typename T::internal_keysymbol _keyname = T::key_key(sym);
		for(auto i = bindings.begin(); i != bindings.end(); i++) {
			if((!(_keyname == i->symbol)) || ((_mod & i->modmask) != (i->mod & i->modmask)))
				continue;
			std::string x = fixup_command_polarity(i->command, polarity);
			if(x == "")
				continue;
			return x;
		}
		return "";
	}

/**
 * \brief Dump list of bindigns as messages to specified graphics handle.
 * 
 * \param win The graphics system handle.
 * \throws std::bad_alloc Not enough memory.
 */
	void dumpbindings(window* win) throw(std::bad_alloc)
	{
		for(auto i = bindings.begin(); i != bindings.end(); i++)
			out(win) << "bind " << T::name_key(i->mod, i->modmask, i->symbol) << " " << i->command
				<< std::endl;
	}
private:
	struct kdata
	{
		unsigned mod;
		unsigned modmask;
		typename T::internal_keysymbol symbol;
		std::string command;
	};
	std::list<kdata> bindings;
};

#endif
