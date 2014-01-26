#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>

const char* hexes = "0123456789ABCDEF";

struct encoder
{
	encoder(std::ostream& _output) : output(_output)
	{
		have_quote = false;
	}
	size_t operator()(unsigned char* buf, size_t bufuse, bool eof)
	{
		if(!bufuse) return 0;
		std::ostringstream out;
		size_t i = 0;
		while(i < bufuse) {
			if(!have_quote) {
				out << "\"";
				have_quote = true;
			}
			unsigned char ch = buf[i];
			if(ch == 9) {
				out << "\\t";
			} else if(ch == 10) {
				out << "\\n\"" << std::endl;
				have_quote = false;
			} else if(ch == 13) {
				out << "\\r";
			} else if(ch < 32) {
				out << "\\x" << hexes[(ch >> 4)] << hexes[ch & 15];
			} else if(ch == '\"') {
				out << "\\\"";
			} else if(ch == '\\') {
				out << "\\\\";
			} else if(ch < 127) {
				out << ch;
			} else {
				out << "\\x" << hexes[(ch >> 4)] << hexes[ch & 15];
			}
			i++;
		}
		output << out.str();
		return i;
	}
	size_t operator()()
	{
		if(have_quote) {
			output << "\"";
			have_quote = false;
		}
	}
private:
	std::ostream& output;
	bool have_quote;
};

void do_encode(std::istream& input, std::ostream& output)
{
	char buf[4096];
	size_t bufuse = 0;
	bool eof = false;
	encoder e(output);
	while(true) {
		if(!eof) {
			input.read(buf + bufuse, 4096 - bufuse);
			bufuse += input.gcount();
		}
		if(!input)
			eof = true;
		size_t bytes = e(reinterpret_cast<unsigned char*>(buf), bufuse, eof);
		memmove(buf, buf + bytes, bufuse - bytes);
		bufuse -= bytes;
		if(eof && !bufuse) break;
	}
	e();
}

int main(int argc, char** argv)
{
	if(argc != 3) {
		std::cerr << "Usage: txt2cstr <symbol> <file>" << std::endl;
		return 1;
	}
	std::ifstream in(argv[2], std::ios::binary);
	std::cout << "const char* " << argv[1] << " =" << std::endl;
	do_encode(in, std::cout);
	std::cout << ";" << std::endl;
}
