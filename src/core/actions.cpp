#include "core/command.hpp"
#include "core/rom.hpp"
#include "core/moviedata.hpp"
#include "interface/romtype.hpp"

namespace
{
	function_ptr_command<const std::string&> action(lsnes_cmd, "action", "Execute core action",
		"Syntax: action <name> [<params>...]\nExecutes core action.\n",
		[](const std::string& _args) throw(std::bad_alloc, std::runtime_error) {
			if(_args == "") {
				messages << "Action name required." << std::endl;
				return;
			}
			std::string args = _args;
			std::string sym;
			extract_token(args, sym, " \t");
			const interface_action* act = NULL;
			for(auto i : our_rom->rtype->get_actions())
				if(i->symbol == sym) {
					act = i;
					break;
				}
			std::vector<interface_action_paramval> params;
			for(auto i : act->params) {
				if(args == "") {
					messages << "Action needs more parameters." << std::endl;
					return;
				}
				std::string p;
				extract_token(args, p, " \t");
				regex_results r;
				if(r = regex("string(:(.*))?", i.model)) {
					try {
						if(r[2] != "" && !regex_match(r[2], p)) {
							messages << "String does not satisfy constraints."
								<< std::endl;
							return;
						}
					} catch(...) {
						messages << "Internal error: Bad constraint in '" << i.model << "'."
							<< std::endl;
						return;
					}
					interface_action_paramval pv;
					pv.s = p;
					params.push_back(pv);
				} else if(r = regex("int:([0-9]+),([0-9]+)", i.model)) {
					int64_t low, high, v;
					try {
						low = parse_value<int64_t>(r[1]);
						high = parse_value<int64_t>(r[2]);
					} catch(...) {
						messages << "Internal error: Unknown limits in '" << i.model << "'."
							<< std::endl;
						return;
					}
					try {
						v = parse_value<int64_t>(p);
					} catch(...) {
						messages << "Can't parse parameter as integer." << std::endl;
						return;
					}
					if(v < low || v > high) {
						messages << "Parameter out of limits." << std::endl;
						return;
					}
					interface_action_paramval pv;
					pv.i = v;
					params.push_back(pv);
				} else if(i.model == "bool") {
					int r = string_to_bool(p);
					if(r < 0) {
						messages << "Bad value for boolean parameter." << std::endl;
						return;
					}
					interface_action_paramval pv;
					pv.b = (r > 0);
					params.push_back(pv);
				} else {
					messages << "Internal error: Unknown parameter model '" << i.model << "'."
						<< std::endl;
					return;
				}
			}
			if(args != "") {
				messages << "Excess parameters for action." << std::endl;
				return;
			}
			our_rom->rtype->execute_action(act->id, params);
		});
}
