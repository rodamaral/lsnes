#include "json.hpp"
#include <string>
#include <cstring>
#include <fstream>
#include <sstream>
#include <set>

const char* hexes = "0123456789abcdef";

std::string quote_c_string(const std::string& raw, bool hiraw = false);

std::string quote_c_string(const std::string& raw, bool hiraw)
{
	std::ostringstream out;
	out << "\"";
	for(auto i : raw) {
		unsigned char ch = i;
		if(ch == '\n')
			out << "\\n";
		else if(ch == '\"')
			out << "\\\"";
		else if(ch == '\\')
			out << "\\\\";
		else if(ch < 32 || ch == 127 || (ch > 127 && !hiraw))
			out << "\\x" << hexes[ch / 16] << hexes[ch % 16];
		else
			out << ch;
	}
	out << "\"";
	return out.str();
}

void process_command_inverse(JSON::node& n, std::ostream& imp)
{
	if(!n.field_exists("__inverse"))
		return;
	auto& inv = n["__inverse"];
	auto name = n["__name"].as_string8();
	for(auto i = inv.begin(); i != inv.end(); i++) {
		std::string args = i.key8();
		std::string desc = i->as_string8();
		if(args != "")
			imp << "\t" << quote_c_string(name + " " + args) << ", " << quote_c_string(desc, true) << ","
				<< std::endl;
		else
			imp << "\t" << quote_c_string(name) << ", " << quote_c_string(desc, true) << "," << std::endl;
	}
}

int main(int argc, char** argv)
{
	std::ofstream impl("inverselist.cpp");
	impl << "#include \"cmdhelp/inverselist.hpp\"" << std::endl;
	impl << "namespace STUBS" << std::endl;
	impl << "{" << std::endl;
	impl << "const char* inverse_cmd_list[] = {" << std::endl;

	for(int i = 1; i < argc; i++) {
		//Hack, skip crap.
		if(argv[i][0] == 0 || argv[i][strlen(argv[i])-1] == 'e')
			continue;
		std::ifstream infile(argv[i]);
		std::string in_json;
		std::string tmp;
		while(infile) {
			std::getline(infile, tmp);
			in_json = in_json + tmp + "\n";
		}
		JSON::node n(in_json);
		for(auto j = n.begin(); j != n.end(); j++)
			process_command_inverse(*j, impl);
	}

	impl << "\t0\n};" << std::endl;
	impl << "}" << std::endl;
	return 0;
}
