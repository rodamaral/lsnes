#include "json.hpp"
#include <string>
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

std::string quote_help(const std::u32string& raw)
{
	const size_t initial_width = 8;
	const size_t break_width = 80;
	std::ostringstream out;
	//Compute split positions.
	std::set<size_t> splits;
	size_t last_word = 0;
	size_t last_word_width = initial_width;
	size_t width = initial_width;
	for(size_t i = 0; i < raw.length(); i++) {
		if(raw[i] == U'\n') {
			splits.insert(i);
			width = initial_width;
			last_word = i;
			last_word_width = initial_width;
		}
		if(raw[i] == U' ') {
			width++;
			if(width > break_width && last_word_width > initial_width) {
				//Wordwrap at last word.
				splits.insert(last_word);
				width = width - last_word_width + initial_width;
			}
			last_word = i;
			last_word_width = width;
		} else {
			//FIXME: Handle double-width characters.
			width++;
		}
	}
	out << "\t\"\\t";
	for(size_t i = 0; i < raw.length(); i++) {
		if(splits.count(i)) {
			out << "\\n\"\n\t\"";
		} else {
			char32_t z[2] = {raw[i]};
			if(z[0] == U'\"')
				out << "\\\"";
			else if(z[0] == U'\n')
				out << "\\n";
			else if(z[0] < 32 || z[0] == 127)
				out << "\\x" << hexes[z[0] / 16] << hexes[z[0] % 16];
			else
				out << utf8::to8(z);
		}
	}
	out << "\\n\"\n";
	return out.str();
}

void process_command(const std::string& name, JSON::node& n, std::ostream& hdr, std::ostream& imp)
{
	hdr << "extern command::stub " << name << ";" << std::endl;
	imp << "command::stub " << name << " = {" << std::endl;
	std::string cmdname = n["__name"].as_string8();
	std::string desc = n.field_exists("__description") ? n["__description"].as_string8() :
		"No description available";
	imp << "\t" << quote_c_string(cmdname) << ", " << quote_c_string(desc) << "," << std::endl;
	bool first = true;
	for(auto i = n.begin(); i != n.end(); i++) {
		std::string hleafname = i.key8();
		if(hleafname == "__name" || hleafname == "__description" || hleafname == "__inverse" ||
			hleafname == "__class")
			continue;
		std::u32string hleafc = i->as_string();
		if(!first)
			imp << "\t\"\\n\"" << std::endl;
		first = false;
		imp << "\t" << quote_c_string(std::string("Syntax: ") + cmdname + " " + hleafname + "\n")
			<< std::endl;
		imp << quote_help(hleafc);
	}
	if(first) imp << "\t\"No help available for '" << cmdname << "'\"" << std::endl;
	imp << "};" << std::endl;
	imp << std::endl;
}

int main(int argc, char** argv)
{
	if(argc != 2) {
		std::cerr << "Need one filename base" << std::endl;
		return 1;
	}
	std::string fname = argv[1];

	if(fname.length() > 5 && fname.substr(fname.length() - 5) == ".json")
		fname = fname.substr(0, fname.length() - 5);

	std::ifstream infile(fname + std::string(".json"));
	std::string in_json;
	std::string tmp;
	while(infile) {
		std::getline(infile, tmp);
		in_json = in_json + tmp + "\n";
	}
	JSON::node n(in_json);

	std::ofstream hdr(fname + std::string(".hpp"));
	std::ofstream impl(fname + std::string(".cpp"));
	impl << "#include \"cmdhelp/" << fname << ".hpp\"" << std::endl;
	impl << "namespace STUBS" << std::endl;
	impl << "{" << std::endl;
	hdr << "#pragma once" << std::endl;
	hdr << "#include \"library/command.hpp\"" << std::endl;
	hdr << "namespace STUBS" << std::endl;
	hdr << "{" << std::endl;
	for(auto i = n.begin(); i != n.end(); i++)
		process_command(i.key8(), *i, hdr, impl);
	impl << "}" << std::endl;
	hdr << "}" << std::endl;
	return 0;
}
