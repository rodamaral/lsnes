#include "lua/internal.hpp"
#include "interface/romtype.hpp"
#include "core/moviedata.hpp"

namespace
{
	function_ptr_luafun c_action(LS, "memory.action", [](lua_state& L, const std::string& fname) -> int {
		std::string name = L.get_string(1, fname.c_str());
		const interface_action* act = NULL;
		for(auto i : our_rom->rtype->get_actions())
			if(i->symbol == name) {
				act = i;
				break;
			}
		if(!act)
			throw std::runtime_error("No such action");
		if(!(our_rom->rtype->action_flags(act->id) & 1))
			throw std::runtime_error("Action not enabled.");
		std::vector<interface_action_paramval> params;
		unsigned idx = 2;
		for(auto i : act->params) {
			regex_results r;
			interface_action_paramval pv;
			if(r = regex("string(:(.*))?", i.model)) {
				pv.s = L.get_string(idx, fname.c_str());
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
				int64_t low, high, v;
				try {
					low = parse_value<int64_t>(r[1]);
					high = parse_value<int64_t>(r[2]);
				} catch(...) {
					messages << "Internal error: Unknown limits in '" << i.model << "'."
						<< std::endl;
					throw std::runtime_error("Internal error");
				}
				pv.i = L.get_numeric_argument<uint64_t>(idx, fname.c_str());
				if(pv.i < low || pv.i > high) {
					throw std::runtime_error("Parameter out of limits.");
				}
			} else if(r = regex("enum:(.*)", i.model)) {
				std::string p = L.get_string(idx, fname.c_str());
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
				pv.b = L.get_bool(idx, fname.c_str());
			} else if(regex_match("toggle", i.model)) {
				idx--;
			} else {
				messages << "Internal error: Unknown parameter model '" << i.model << "'."
					<< std::endl;
				throw std::runtime_error("Internal error");
			}
			params.push_back(pv);
			idx++;
		}
		if(L.type(idx) != LUA_TNONE) {
			throw std::runtime_error("Excess arguments for action");
		}
		our_rom->rtype->execute_action(act->id, params);
		return 0;
	});
}
