#ifndef _library__settings_cmd_bridge__hpp__included__
#define _library__settings_cmd_bridge__hpp__included__

#include "library/commands.hpp"
#include "library/settings.hpp"

/**
 * Bridge between command group and settings group.
 */
class settings_command_bridge
{
public:
/**
 * Create a bridge.
 */
	settings_command_bridge(setting_group& sgroup, command_group& cgroup, const std::string& set_cmd,
		const std::string& unset_cmd, const std::string& get_cmd, const std::string& show_cmd);
/**
 * Destroy a bridge.
 */
	~settings_command_bridge() throw();
/**
 * Set output to use.
 */
	void set_output(std::ostream& out);
private:
	command* c1;
	command* c2;
	command* c3;
	command* c4;
	std::ostream* output;
};

#endif
