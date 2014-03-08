#include "library/json.hpp"
#include <iostream>
#include <fstream>
#include <list>
#include <string>

int main(int argc, char** argv)
{
	int ret = 0;
	bool print_parsed = false;
	bool end_opt = false;
	std::list<std::string> files;
	for(int i = 1; i < argc; i++) {
		std::string opt = argv[i];
		if(end_opt)
			files.push_back(opt);
		else if(opt == "--")
			end_opt = true;
		else if(opt == "--print")
			print_parsed = true;
		else if(opt.substr(0, 2) == "--") {
			std::cerr << "Unknown option '" << opt << "'" << std::endl;
			return 2;
		} else
			files.push_back(opt);
	}
	for(auto i : files) {
		std::string doc;
		try {
			{
				std::ifstream strm(i, std::ios::binary);
				if(!strm)
					throw std::runtime_error("Can't open");
				while(strm) {
					std::string line;
					std::getline(strm, line);
					doc = doc + line + "\n";
				}
			}
			JSON::node n(doc);
			if(print_parsed) {
				JSON::printer_indenting ip;
				std::cout << n.serialize(&ip) << std::endl;
			}
		} catch(JSON::error& e) {
			std::cerr << i << ": " << e.extended_error(doc) << std::endl;
			ret = 1;
		} catch(std::exception& e) {
			std::cerr << i << ": " << e.what() << std::endl;
			ret = 1;
		}
	}
	return ret;
}
