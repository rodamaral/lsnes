#include "lua/internal.hpp"
#include "interface/romtype.hpp"
#include "core/instance.hpp"
#include "core/moviedata.hpp"
#include "core/messages.hpp"
#include "core/rom.hpp"

namespace
{
	int action(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		std::string name;

		P(name);

		const interface_action* act = NULL;
		for(auto i : core.rom->get_actions())
			if(i->get_symbol() == name) {
				act = i;
				break;
			}
		if(!act)
			throw std::runtime_error("No such action");
		if(!(core.rom->action_flags(act->id) & 1))
			throw std::runtime_error("Action not enabled.");
		std::vector<interface_action_paramval> params;
		for(auto i : act->params) {
			regex_results r;
			interface_action_paramval pv;
			if(r = regex("string(:(.*))?", i.model)) {
				P(pv.s);
				bool bad = false;;
				try {
					if(r[2] != "" && !regex_match(r[2], pv.s))
						bad = true;
				} catch(...) {
					messages << "Internal error: Bad constraint in '" << i.model << "'."
						<< std::endl;
					throw std::runtime_error("Internal error");
				}
				if(bad)
					throw std::runtime_error("String does not satisfy constraints.");
			} else if(r = regex("int:([0-9]+),([0-9]+)", i.model)) {
				int64_t low, high;
				try {
					low = parse_value<int64_t>(r[1]);
					high = parse_value<int64_t>(r[2]);
				} catch(...) {
					messages << "Internal error: Unknown limits in '" << i.model << "'."
						<< std::endl;
					throw std::runtime_error("Internal error");
				}
				P(pv.i);
				if(pv.i < low || pv.i > high) {
					throw std::runtime_error("Parameter out of limits.");
				}
			} else if(r = regex("enum:(.*)", i.model)) {
				std::string p;
				P(p);
				unsigned num = 0;
				try {
					JSON::node e(r[1]);
					for(auto i2 : e) {
						std::string n;
						if(i2.type() == JSON::string)
							n = i2.as_string8();
						else if(i2.type() == JSON::array)
							n = i2.index(0).as_string8();
						else
							throw std::runtime_error("Choice not array nor "
								"string");
						if(n == p)
							goto out;
						num++;
					}
				} catch(std::exception& e) {
					messages << "JSON parse error parsing " << "model: "
						<< e.what() << std::endl;
					throw std::runtime_error("Internal error");
				}
				throw std::runtime_error("Invalid choice for enumeration.");
out:
				pv.i = num;
			} else if(regex_match("bool", i.model)) {
				P(pv.b);
			} else if(regex_match("toggle", i.model)) {
			} else {
				messages << "Internal error: Unknown parameter model '" << i.model << "'."
					<< std::endl;
				throw std::runtime_error("Internal error");
			}
			params.push_back(pv);
		}
		if(P.more())
			throw std::runtime_error("Excess arguments for action");
		core.rom->execute_action(act->id, params);
		return 0;
	}

	int action_flags(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		std::string name;

		P(name);

		const interface_action* act = NULL;
		for(auto i : core.rom->get_actions())
			if(i->get_symbol() == name) {
				act = i;
				break;
			}
		if(!act)
			throw std::runtime_error("No such action");
		L.pushnumber(core.rom->action_flags(act->id));
		return 1;
	}

	lua::functions LUA_actions_fns(lua_func_misc, "memory", {
		{"action", action},
		{"action_flags", action_flags},
	});
}
