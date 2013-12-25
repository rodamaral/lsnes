#include "json.hpp"

int main()
{
	std::string doc;
	std::string line;
	while(std::getline(std::cin, line))
		doc = doc + line + "\n";
	JSON::node n(doc);
	JSON::printer_indenting printer;
	std::cout << n.serialize(&printer);
	return 0;
}
