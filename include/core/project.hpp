#ifndef _project__hpp__included__
#define _project__hpp__included__

#include <string>
#include <map>
#include <list>
#include <vector>
#include "core/rom-small.hpp"
#include "library/command.hpp"
#include "library/json.hpp"
#include "library/text.hpp"

class voice_commentary;
class memwatch_set;
class controller_state;
class button_mapping;
class emulator_dispatch;
class input_queue;
class status_updater;
namespace settingvar { class cache; }

//A branch.
struct project_branch_info
{
	//Parent branch ID.
	uint64_t pbid;
	//Name.
	text name;
};

//Information about project.
struct project_info
{
	//Internal ID of the project.
	text id;
	//Name of the project.
	text name;
	//Bundle ROM used.
	text rom;
	//ROMs used.
	text roms[ROM_SLOT_COUNT];
	//Last savestate.
	text last_save;
	//Directory for save/load dialogs.
	text directory;
	//Prefix for save/load dialogs.
	text prefix;
	//List of Lua scripts to run at startup.
	std::list<text> luascripts;
	//Memory watches.
	std::map<text, text> watches;
	//Macros.
	std::map<text, JSON::node> macros;
	//Branches.
	std::map<uint64_t, project_branch_info> branches;
	uint64_t active_branch;
	uint64_t next_branch;
	//Filename this was loaded from.
	text filename;
	//Stub movie data.
	text gametype;
	std::map<text, text> settings;
	text coreversion;
	text gamename;
	text projectid;
	text romimg_sha256[ROM_SLOT_COUNT];
	text romxml_sha256[ROM_SLOT_COUNT];
	text namehint[ROM_SLOT_COUNT];
	std::vector<std::pair<text, text>> authors;
	std::map<text, std::vector<char>> movie_sram;
	std::vector<char> anchor_savestate;
	int64_t movie_rtc_second;
	int64_t movie_rtc_subsecond;
/**
 * Make empty project info.
 */
	project_info(emulator_dispatch& _dispatch);
/**
 * Obtain parent of branch.
 *
 * Parameter bid: The branch ID.
 * Returns: The parent branch ID.
 * Throws std::runtime_error: Invalid branch ID.
 *
 * Note: bid 0 is root. Calling this on root always gives 0.
 */
	uint64_t get_parent_branch(uint64_t bid);
/**
 * Get current branch id.
 *
 * Returns: The branch id.
 */
	uint64_t get_current_branch() { return active_branch; }
/**
 * Set current branch.
 *
 * Parameter bid: The branch id.
 * Throws std::runtime_error: Invalid branch ID.
 */
	void set_current_branch(uint64_t bid);
/**
 * Get name of branch.
 *
 * Parameter bid: The branch id.
 * Throws std::runtime_error: Invalid branch ID.
 * Note: The name of ROOT branch is always empty string.
 */
	const text& get_branch_name(uint64_t bid);
/**
 * Set name of branch.
 *
 * Parameter bid: The branch id.
 * Parameter name: The new name
 * Throws std::runtime_error: Invalid branch ID.
 * Note: The name of ROOT branch can't be changed.
 */
	void set_branch_name(uint64_t bid, const text& name);
/**
 * Set parent branch of branch.
 *
 * Parameter bid: The branch id.
 * Parameter pbid: The new parent branch id.
 * Throws std::runtime_error: Invalid branch ID, or cyclic dependency.
 * Note: The parent of ROOT branch can't be set.
 */
	void set_parent_branch(uint64_t bid, uint64_t pbid);
/**
 * Enumerate child branches of specified branch.
 *
 * Parameter bid: The branch id.
 * Returns: The set of chilid branch IDs.
 * Throws std::runtime_error: Invalid branch ID.
 */
	std::set<uint64_t> branch_children(uint64_t bid);
/**
 * Create a new branch.
 *
 * Parameter pbid: Parent of the new branch.
 * Parameter name: Name of new branch.
 * Returns: Id of new branch.
 * Throws std::runtime_error: Invalid branch ID.
 */
	uint64_t create_branch(uint64_t pbid, const text& name);
/**
 * Delete a branch.
 *
 * Parameter bid: The branch id.
 * Throws std::runtime_error: Invalid branch ID or branch has children.
 */
	void delete_branch(uint64_t bid);
/**
 * Get name of current branch as string.
 */
	text get_branch_string();
/**
 * Flush the project to disk.
 */
	void flush();
private:
	void write(std::ostream& s);
	emulator_dispatch& edispatch;
};

class project_state
{
public:
	project_state(voice_commentary& _commentary, memwatch_set& _mwatch, command::group& _command,
		controller_state& _controls, settingvar::cache& _setcache, button_mapping& _buttons,
		emulator_dispatch& _edispatch, input_queue& _iqueue, loaded_rom& _rom, status_updater& _supdater);
	~project_state();
/**
 * Get currently active project.
 *
 * Returns: The currently active project or NULL if none.
 */
	project_info* get();
/**
 * Change currently active project. This reloads Lua VM, ROM and savestate.
 *
 * Parameter p: The new currently active project, or NULL to switch out of any project.
 * Parameter current: If true, do not reload ROM, movie nor state.
 */
	bool set(project_info* p, bool current = false);
/**
 * Enumerate all known projects.
 *
 * Returns: Map from IDs of projects to project names.
 */
	std::map<text, text> enumerate();
/**
 * Load a given project.
 *
 * Parameter id: The ID of project to load.
 * Returns: The project information.
 */
	project_info& load(const text& id);
/**
 * Get project movie path.
 *
 * Returns: The movie path.
 */
	text moviepath();
/**
 * Get project other path.
 *
 * Returns: The other path.
 */
	text otherpath();
/**
 * Get project savestate extension.
 *
 * Returns: The savestate extension.
 */
	text savestate_ext();
/**
 * Copy watches to project
 */
	void copy_watches(project_info& p);
/**
 * Copy macros to project.
 */
	void copy_macros(project_info& p, controller_state& s);
private:
	void recursive_list_branch(uint64_t bid, std::set<unsigned>& dset, unsigned depth, bool last_of);
	void do_branch_ls();
	void do_branch_mk(const text& a);
	void do_branch_rm(const text& a);
	void do_branch_set(const text& a);
	void do_branch_rp(const text& a);
	void do_branch_mv(const text& a);
	project_info* active_project;
	voice_commentary& commentary;
	memwatch_set& mwatch;
	command::group& command;
	controller_state& controls;
	settingvar::cache& setcache;
	button_mapping& buttons;
	emulator_dispatch& edispatch;
	input_queue& iqueue;
	loaded_rom& rom;
	status_updater& supdater;
	command::_fnptr<> branch_ls;
	command::_fnptr<const text&> branch_mk;
	command::_fnptr<const text&> branch_rm;
	command::_fnptr<const text&> branch_set;
	command::_fnptr<const text&> branch_rp;
	command::_fnptr<const text&> branch_mv;
};

#endif
