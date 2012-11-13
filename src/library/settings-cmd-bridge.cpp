#include "settings-cmd-bridge.hpp"
#include "string.hpp"

namespace
{
	struct set_setting_cmd : public command
	{
		set_setting_cmd(setting_group& _sgroup, command_group& _cgroup, const std::string& _cmd,
			std::ostream*& out)
			: command(_cgroup, _cmd), sgroup(_sgroup), output(out), cmd(_cmd)
		{
		}
		~set_setting_cmd() throw() {}
		void invoke(const std::string& t) throw(std::bad_alloc, std::runtime_error)
		{
			auto r = regex("([^ \t]+)([ \t]+(|[^ \t].*))?", t, "Setting name required.");
			sgroup.set(r[1], r[3]);
			(*output) << "Setting '" << r[1] << "' set to '" << r[3] << "'" << std::endl;
		}
		std::string get_short_help() throw(std::bad_alloc)
		{
			return "Set a setting";
		}
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: " + cmd + " <setting> [<value>]\nSet setting to a new value. Omit <value> "
				"to set to ''\n";
		}
		setting_group& sgroup;
		std::ostream*& output;
		std::string cmd;
	};

	struct unset_setting_cmd : public command
	{
		unset_setting_cmd(setting_group& _sgroup, command_group& _cgroup, const std::string& _cmd,
			std::ostream*& out)
			: command(_cgroup, _cmd), sgroup(_sgroup), output(out), cmd(_cmd)
		{
		}
		~unset_setting_cmd() throw() {}
		void invoke(const std::string& t) throw(std::bad_alloc, std::runtime_error)
		{
			auto r = regex("([^ \t]+)[ \t]*", t, "Expected setting name and nothing else");
			sgroup.blank(r[1]);
			(*output) << "Setting '" << r[1] << "' unset" << std::endl;
		}
		std::string get_short_help() throw(std::bad_alloc)
		{
			return "Unset a setting";
		}
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: " + cmd + " <setting>\nTry to unset a setting. Note that not all settings "
				"can be unset\n";
		}
		setting_group& sgroup;
		std::ostream*& output;
		std::string cmd;
	};

	struct get_setting_cmd : public command
	{
		get_setting_cmd(setting_group& _sgroup, command_group& _cgroup, const std::string& _cmd,
			std::ostream*& out)
			: command(_cgroup, _cmd), sgroup(_sgroup), output(out), cmd(_cmd)
		{
		}
		~get_setting_cmd() throw() {}
		void invoke(const std::string& t) throw(std::bad_alloc, std::runtime_error)
		{
			auto r = regex("([^ \t]+)[ \t]*", t, "Expected setting name and nothing else");
			if(sgroup.is_set(r[1]))
				(*output) << "Setting '" << r[1] << "' has value '"
					<< sgroup.get(r[1]) << "'" << std::endl;
			else
				(*output) << "Setting '" << r[1] << "' is unset" << std::endl;
		}
		std::string get_short_help() throw(std::bad_alloc)
		{
			return "get value of a setting";
		}
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: " + cmd + " <setting>\nShow value of setting\n";
		}
		setting_group& sgroup;
		std::ostream*& output;
		std::string cmd;
	};

	struct show_setting_cmd : public command
	{
		show_setting_cmd(setting_group& _sgroup, command_group& _cgroup, const std::string& _cmd,
			std::ostream*& out)
			: command(_cgroup, _cmd), sgroup(_sgroup), output(out), cmd(_cmd)
		{
		}
		~show_setting_cmd() throw() {}
		void invoke(const std::string& t) throw(std::bad_alloc, std::runtime_error)
		{
			if(t != "")
				throw std::runtime_error("This command does not take arguments");
			for(auto i : sgroup.get_settings_set()) {
				if(!sgroup.is_set(i))
					(*output) << i << ": (unset)" << std::endl;
				else
					(*output) << i << ": " << sgroup.get(i) << std::endl;
			}
		}
		std::string get_short_help() throw(std::bad_alloc)
		{
			return "Sghow values of all settings";
		}
		std::string get_long_help() throw(std::bad_alloc)
		{
			return "Syntax: " + cmd + "\nShow value of all settings\n";
		}
		setting_group& sgroup;
		std::ostream*& output;
		std::string cmd;
	};
}

settings_command_bridge::settings_command_bridge(setting_group& sgroup, command_group& cgroup,
	const std::string& set_cmd, const std::string& unset_cmd, const std::string& get_cmd,
	const std::string& show_cmd)
{
	output = &std::cerr;
	c1 = new set_setting_cmd(sgroup, cgroup, set_cmd, output);
	c2 = new unset_setting_cmd(sgroup, cgroup, unset_cmd, output);
	c3 = new get_setting_cmd(sgroup, cgroup, get_cmd, output);
	c4 = new show_setting_cmd(sgroup, cgroup, show_cmd, output);
}

settings_command_bridge::~settings_command_bridge() throw()
{
	delete c1;
	delete c2;
	delete c3;
	delete c4;
}

void settings_command_bridge::set_output(std::ostream& out)
{
	output = &out;
}
