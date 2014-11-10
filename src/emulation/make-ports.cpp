#include "portctrl-data.hpp"
#include "portctrl-parse.hpp"
#include "json.hpp"
#include <string>

int main()
{
	std::string in_json;
	std::string tmp;
	while(std::cin) {
		std::getline(std::cin, tmp);
		in_json = in_json + tmp + "\n";
	}
	JSON::node n(in_json);
	auto x = portctrl::pcs_from_json_array(n, "ports");
	unsigned t = 0;
	std::string out = portctrl::pcs_write_classes(x, t);
	std::cout << out;
	return 0;
}
