#ifndef _ui_services__hpp__included__
#define _ui_services__hpp__included__

#include <map>
#include <set>
#include <functional>
#include <list>
#include <string>
#include <sstream>

/*********************************************************************************************************************
UI services.

- All functions here are safe to call from another thread.
- If function takes onerror parameter, that function is asynchronous.
- The onerror handler is called from arbitrary thread.
*********************************************************************************************************************/

class emulator_instance;
class dumper_factory_base;
namespace keyboard { class modifier_set; }
namespace keyboard { class key_key; }

struct project_author_info
{
	//True if this is a project, false if not. Ignored when saving.
	bool is_project;
	//Lua scripts. Ignored when saving if not in project context.
	std::list<std::string> luascripts;
	//Autorun new lua scripts. Ignored when saving if not in project context, always loaded as false.
	bool autorunlua;
	//Authors. First of each pair is full name, second is nickname.
	std::list<std::pair<std::string, std::string>> authors;
	//Name of the game.
	std::string gamename;
	//Project Directory. Ignored if not in project context.
	std::string directory;
	//Project name. Ignored if not in project context.
	std::string projectname;
	//Save prefix.
	std::string prefix;
};

struct dumper_information_1
{
	//The factory for this dumper.
	dumper_factory_base* factory;
	//Name of this dumper.
	std::string name;
	//Is this dumper active?
	bool active;
	//Hidden?
	bool hidden;
	//Modes available (first is internal name, second is human-readable one).
	std::map<std::string, std::string> modes;
};

struct dumper_information
{
	std::map<std::string, dumper_information_1> dumpers;
};

/**
 * Fill branch name map.
 */
void UI_get_branch_map(emulator_instance& instance, uint64_t& cur, std::map<uint64_t, std::string>& namemap,
	std::map<uint64_t, std::set<uint64_t>>& childmap);
/**
 * Arrange current project to be flushed.
 */
void UI_call_flush(emulator_instance& instance, std::function<void(std::exception&)> onerror);
/**
 * Arrage branch to be created.
 */
void UI_create_branch(emulator_instance& instance, uint64_t id, const std::string& name,
	std::function<void(std::exception&)> onerror);
/**
 * Arrage branch to be renamed.
 */
void UI_rename_branch(emulator_instance& instance, uint64_t id, const std::string& name,
	std::function<void(std::exception&)> onerror);
/**
 * Arrage branch to be reparented.
 */
void UI_reparent_branch(emulator_instance& instance, uint64_t id, uint64_t pid,
	std::function<void(std::exception&)> onerror);
/**
 * Arrage branch to be deleted.
 */
void UI_delete_branch(emulator_instance& instance, uint64_t id, std::function<void(std::exception&)> onerror);
/**
 * Arrage branch to be switched.
 */
void UI_switch_branch(emulator_instance& instance, uint64_t id, std::function<void(std::exception&)> onerror);
/**
 * Load project author info.
 */
project_author_info UI_load_author_info(emulator_instance& instance);
/**
 * Save project author info.
 */
void UI_save_author_info(emulator_instance& instance, project_author_info& info);
/**
 * Get available dumpers.
 */
dumper_information UI_get_dumpers(emulator_instance& instance);
/**
 * Start dumping.
 */
void UI_start_dump(emulator_instance& inst, dumper_factory_base& factory, const std::string& mode,
	const std::string& prefix);
/**
 * End dumping.
 */
void UI_end_dump(emulator_instance& inst, dumper_factory_base& factory);
/**
 * Send a keypress event.
 */
void UI_do_keypress(emulator_instance& inst, const keyboard::modifier_set& mods, keyboard::key_key& key,
	bool polarity);
/**
 * Is there a valid movie?
 */
bool UI_has_movie(emulator_instance& inst);
/**
 * Save a movie into buffer.
 */
void UI_save_movie(emulator_instance& inst, std::ostringstream& buffer);
/**
 * Look up (platform,game) pair.
 */
std::pair<std::string, std::string> UI_lookup_platform_and_game(emulator_instance& inst);
/**
 * Get otherpath of current project.
 */
std::string UI_get_project_otherpath(emulator_instance& inst);
/**
 * Get moviepath of current project.
 */
std::string UI_get_project_moviepath(emulator_instance& inst);
/**
 * Is in project context?
 */
bool UI_in_project_context(emulator_instance& inst);

#endif
