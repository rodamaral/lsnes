#include "json.hpp"
#include <string>
#include <cstring>
#include <fstream>
#include <sstream>
#include <set>

const char* hexes = "0123456789abcdef";

text quote_c_string(const text& raw)
{
	std::ostringstream out;
	out << "\"";
	for(size_t i = 0; i < raw.length(); i++) {
		char32_t ch = raw[i];
		if(ch == 10)		//Linefeed.
			out << "\\n";
		else if(ch == 34)	//Quote
			out << "\\\"";
		else if(ch == 92)	//Backslash
			out << "\\\\";
		else if(ch < 32 || ch == 127)	//Controls or DEL.
			out << "\\x" << hexes[ch / 16] << hexes[ch % 16];
		else if(ch >= 128 && ch < 160) {
			//The contination range is 80-BF, so this is right.
			out << "\\xC2\\x" << hexes[ch / 16] << hexes[ch % 16];
		} else {
			char buf[4] = {0};
			raw.output_utf8_fragment(i, buf, 4);
			out << ch;
		}
	}
	out << "\"";
	return out.str();
}

void process_command_inverse(JSON::node& n, text name, std::ostream& imp)
{
	if(name == "__mod" || n.index_count() < 4)
		return;
	auto& inv = n.index(3);
	for(auto i = inv.begin(); i != inv.end(); i++) {
		text args = i.key();
		text desc = i->as_string();
		if(args != "")
			imp << "\t" << quote_c_string(name + " " + args) << ", " << quote_c_string(desc) << ","
				<< std::endl;
		else
			imp << "\t" << quote_c_string(name) << ", " << quote_c_string(desc) << "," << std::endl;
	}
}

bool is_crap_filename(const text& arg)
{
	if(arg.length() >= 4 && arg.substr(arg.length() - 4) == ".exe") return true;
	if(arg.length() >= 9 && arg.substr(arg.length() - 9) == "/mkstubsi") return true;
	if(arg == "mkstubsi") return true;
	return false;
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
		if(argv[i][0] == 0 || is_crap_filename(argv[i]))
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
			process_command_inverse(*j, j.key(), impl);
	}

	impl << "\t0\n};" << std::endl;
	impl << "}" << std::endl;
	return 0;
}
