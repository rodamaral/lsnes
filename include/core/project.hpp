#ifndef _project__hpp__included__
#define _project__hpp__included__

#include <string>
#include <map>
#include <list>
#include <vector>

//Information about project.
struct project_info
{
	//Internal ID of the project.
	std::string id;
	//Name of the project.
	std::string name;
	//ROM used.
	std::string rom;
	//Last savestate.
	std::string last_save;
	//Directory for save/load dialogs.
	std::string directory;
	//Prefix for save/load dialogs.
	std::string prefix;
	//List of Lua scripts to run at startup.
	std::list<std::string> luascripts;
	//Memory watches.
	std::map<std::string, std::string> watches;
	//Stub movie data.
	std::string gametype;
	std::map<std::string, std::string> settings;
	std::string coreversion;
	std::string gamename;
	std::string projectid;
	std::string romimg_sha256[27];
	std::string romxml_sha256[27];
	std::vector<std::pair<std::string, std::string>> authors;
	std::map<std::string, std::vector<char>> movie_sram;
	std::vector<char> anchor_savestate;
	int64_t movie_rtc_second;
	int64_t movie_rtc_subsecond;
};

/**
 * Get currently active project.
 *
 * Returns: The currently active project or NULL if none.
 */
project_info* project_get();
/**
 * Change currently active project. This reloads Lua VM, ROM and savestate.
 *
 * Parameter p: The new currently active project, or NULL to switch out of any project.
 * Parameter current: If true, do not reload ROM, movie nor state.
 */
bool project_set(project_info* p, bool current = false);
/**
 * Enumerate all known projects.
 *
 * Returns: Map from IDs of projects to project names.
 */
std::map<std::string, std::string> project_enumerate();
/**
 * Load a given project.
 *
 * Parameter id: The ID of project to load.
 * Returns: The project information.
 */
project_info& project_load(const std::string& id);
/**
 * Flush any changes to project to disk.
 *
 * Parameter: The project to flush (NULL is no-op).
 */
void project_flush(project_info* p);
/**
 * Get project movie path.
 *
 * Returns: The movie path.
 */
std::string project_moviepath();
/**
 * Get project other path.
 *
 * Returns: The other path.
 */
std::string project_otherpath();
/**
 * Get project savestate extension.
 *
 * Returns: The savestate extension.
 */
std::string project_savestate_ext();
/**
 * Copy watches to project
 */
void project_copy_watches(project_info& p);

#endif
