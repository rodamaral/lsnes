#include "string.hpp"
#include "minmax.hpp"
#include "threadtypes.hpp"
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
	static mutex_class m;
	umutex_class h(m);
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

namespace
{
	template<typename ch>
	std::list<std::basic_string<ch>> _split_on_codepoint(const std::basic_string<ch>& s,
		const std::basic_string<ch>& cp)
	{
		std::list<std::basic_string<ch>> ret;
		size_t start = 0;
		size_t end = 0;
		size_t len = s.length();
		while(end < len) {
			end = s.find(cp, start);
			std::basic_string<ch> x;
			if(end < len) {
				x.resize(end - start);
				std::copy(s.begin() + start, s.begin() + end, x.begin());
				start = end + cp.length();
			} else {
				x.resize(len - start);
				std::copy(s.begin() + start, s.end(), x.begin());
			}
			ret.push_back(x);
		}
		return ret;
	}
}

template<typename T>
string_list<T>::string_list()
{
}

template<typename T>
string_list<T>::string_list(const std::list<std::basic_string<T>>& list)
{
	v.resize(list.size());
	std::copy(list.begin(), list.end(), v.begin());
}

template<typename T>
bool string_list<T>::empty()
{
	return (v.size() == 0);
}

template<typename T>
string_list<T> string_list<T>::strip_one() const
{
	return string_list<T>(&v[0], (v.size() > 0) ? (v.size() - 1) : 0);
}

template<typename T>
size_t string_list<T>::size() const
{
	return v.size();
}

template<typename T>
const std::basic_string<T>& string_list<T>::operator[](size_t idx) const
{
	if(idx >= v.size())
		throw std::runtime_error("Index out of range");
	return v[idx];
}

template<typename T>
string_list<T>::string_list(const std::basic_string<T>* array, size_t arrsize)
{
	v.resize(arrsize);
	std::copy(array, array + arrsize, v.begin());
}

template<typename T>
bool string_list<T>::operator<(const string_list<T>& x) const
{
	for(size_t i = 0; i < v.size() && i < x.v.size(); i++)
		if(v[i] < x.v[i])
			return true;
		else if(v[i] > x.v[i])
			return false;
	return (v.size() < x.v.size());
}

template<typename T>
bool string_list<T>::operator==(const string_list<T>& x) const
{
	if(v.size() != x.v.size())
		return false;
	for(size_t i = 0; i < v.size(); i++)
		if(v[i] != x.v[i])
			return false;
	return true;
}

template<typename T>
bool string_list<T>::prefix_of(const string_list<T>& x) const
{
	if(v.size() > x.v.size())
		return false;
	for(size_t i = 0; i < v.size(); i++)
		if(v[i] != x.v[i])
			return false;
	return true;
}

namespace
{
	template<typename T> std::basic_string<T> separator();
	template<> std::basic_string<char> separator()
	{
		return to_u8string(U"\u2023");
	}

	template<> std::basic_string<char16_t> separator()
	{
		return u"\u2023";
	}

	template<> std::basic_string<char32_t> separator()
	{
		return U"\u2023";
	}

	template<> std::basic_string<wchar_t> separator()
	{
		return L"->";
	}
}

template<typename T>
std::basic_string<T> string_list<T>::debug_name() const
{
	std::basic_stringstream<T> x;
	for(size_t i = 0; i < v.size(); i++)
		if(i != 0)
			x << separator<T>() << v[i];
		else
			x << v[i];
	return x.str();
}

template class string_list<char>;
template class string_list<wchar_t>;
template class string_list<char16_t>;
template class string_list<char32_t>;


string_list<char> split_on_codepoint(const std::string& s, char32_t cp)
{
	std::string _cp = to_u8string(std::u32string(1, cp));
	return _split_on_codepoint<char>(s, _cp);
}

string_list<char32_t> split_on_codepoint(const std::u32string& s, char32_t cp)
{
	std::u32string _cp(1, cp);
	return _split_on_codepoint<char32_t>(s, _cp);
}
