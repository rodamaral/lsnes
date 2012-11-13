#include "string.hpp"
#include "minmax.hpp"
#include <cctype>
#include <boost/regex.hpp>


std::string strip_CR(const std::string& str)
{
	std::string x = str;
	istrip_CR(x);
	return x;
}

void istrip_CR(std::string& str)
{
	size_t crc = 0;
	size_t xl = str.length();
	while(crc < xl) {
		char y = str[xl - crc - 1];
		if(y != '\r' && y != '\n')
			break;
		crc++;
	}
	str = str.substr(0, xl - crc);
}

int firstchar(const std::string& str)
{
	if(str.length())
		return static_cast<unsigned char>(str[0]);
	else
		return -1;
}

int extract_token(std::string& str, std::string& tok, const char* sep, bool sequence) throw(std::bad_alloc)
{
	if(!*sep) {
		tok = str;
		str = "";
		return (tok == "") ? -2 : -1;
	}
	size_t s = str.find_first_of(sep);
	if(s < str.length()) {
		int ech = static_cast<unsigned char>(str[s]);
		size_t t = sequence ? min(str.find_first_not_of(sep, s), str.length()) : (s + 1);
		tok = str.substr(0, s);
		str = str.substr(t);
		return ech;
	} else {
		tok = str;
		str = "";
		return -1;
	}

	int ech = (s < str.length()) ? static_cast<unsigned char>(str[s]) : -1;
	tok = str.substr(0, s);
	str = str.substr(s + 1);
	return ech;
}

int string_to_bool(const std::string& x)
{
	std::string y = x;
	for(size_t i = 0; i < y.length(); i++)
		y[i] = tolower(y[i]);
	if(y == "on" || y == "true" || y == "yes" || y == "1" || y == "enable" || y == "enabled")
		return 1;
	if(y == "off" || y == "false" || y == "no" || y == "0" || y == "disable" || y == "disabled")
		return 0;
	return -1;
}

regex_results::regex_results()
{
	matched = false;
}

regex_results::regex_results(std::vector<std::string> res)
{
	matched = true;
	results = res;
}

regex_results::operator bool() const
{
	return matched;
}

bool regex_results::operator!() const
{
	return !matched;
}

size_t regex_results::size() const
{
	return results.size();
}
const std::string& regex_results::operator[](size_t i) const
{
	return results[i];
}

regex_results regex(const std::string& regexp, const std::string& str, const char* ex) throw(std::bad_alloc,
	std::runtime_error)
{
	static std::map<std::string, boost::regex*> regexps;
	if(!regexps.count(regexp)) {
		boost::regex* y = NULL;
		try {
			y = new boost::regex(regexp, boost::regex::extended & ~boost::regex::collate);
			regexps[regexp] = y;
		} catch(std::bad_alloc& e) {
			delete y;
			throw;
		} catch(std::exception& e) {
			throw std::runtime_error(e.what());
		}
	}

	boost::smatch matches;
	bool x = boost::regex_match(str.begin(), str.end(), matches, *(regexps[regexp]));
	if(x) {
		std::vector<std::string> res;
		for(size_t i = 0; i < matches.size(); i++)
			res.push_back(matches.str(i));
		return regex_results(res);
	} else if(ex)
		throw std::runtime_error(ex);
	else
		return regex_results();
}

bool regex_match(const std::string& regexp, const std::string& str) throw(std::bad_alloc, std::runtime_error)
{
	return regex(regexp, str);
}
