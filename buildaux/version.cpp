#include <cstdio>
#include <cstdlib>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>

std::string X = "$Format:%h by %cn on %ci$";

std::string derive_format(std::string kwformat)
{
	if(kwformat[0] != '$' || kwformat[1] != 'F' || kwformat[kwformat.length() - 1] != '$') {
		std::cerr << "Bad keyword format '" << kwformat << "'" << std::endl;
		exit(1);
	}
	return "--pretty=f" + kwformat.substr(2, kwformat.length() - 3);
}

std::string shellquote(std::string arg)
{
	std::ostringstream x;
	x << "'";
	for(size_t i = 0; i < arg.length(); i++) {
		if(arg[i] == '\'')
			x << "\\'";
		else
			x << arg[i];
	}
	x << "'";
	return x.str();
}

std::string runlog(std::string logformat)
{
	std::string command = "git log " + shellquote(logformat) + " -1";
	std::string retval;
	int r;
	char buf[4096] = {0};
	FILE* out = popen(command.c_str(), "r");
	if(!out) {
		std::cerr << "Can't invoke git to get the version" << std::endl;
		exit(1);
	}
	while((r = fread(buf, 1, 4095, out)) > 0) {
		buf[r] = 0;
		retval = retval + buf;
	}
	if(ferror(out)) {
		std::cerr << "Error reading git version output" << std::endl;
		exit(1);
	}
	pclose(out);
	return retval;
}

std::string get_main_version()
{
	std::ifstream x("VERSION");
	if(!x) {
		std::cerr << "Error reading main version" << std::endl;
		exit(1);
	}
	std::string out;
	std::getline(x, out);
	if(out == "") {
		std::cerr << "Error reading main version" << std::endl;
		exit(1);
	}
	return out;
}

int main()
{
	std::string gitversion;
	std::string mainversion = get_main_version();
	if(X[0] == '$') {
		std::string logformat = derive_format(X);
		gitversion = runlog(logformat);
	} else
		gitversion = X;
	std::cout << "#include <string>" << std::endl;
	std::cout << "std::string lsnes_git_revision = \"" << gitversion << "\";" << std::endl;
	std::cout << "std::string lsnes_version = \"" << mainversion << "\";" << std::endl;
	return 0;
}
