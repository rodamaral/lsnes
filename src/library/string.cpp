#include "string.hpp"
#include "minmax.hpp"
#include "threadtypes.hpp"
#include "eatarg.hpp"
#include <cctype>
#include <boost/regex.hpp>
#include "map-pointer.hpp"

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

regex_results::regex_results(std::vector<std::string> res, std::vector<std::pair<size_t, size_t>> mch)
{
	matched = true;
	matches = mch;
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

std::pair<size_t, size_t> regex_results::match(size_t i) const
{
	return matches[i];
}

regex_results regex(const std::string& regexp, const std::string& str, const char* ex) throw(std::bad_alloc,
	std::runtime_error)
{
	static mutex_class m;
	umutex_class h(m);
	static std::map<std::string, map_pointer<boost::regex>> regexps;
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
		std::vector<std::pair<size_t, size_t>> mch;
		for(size_t i = 0; i < matches.size(); i++) {
			res.push_back(matches.str(i));
			mch.push_back(std::make_pair(matches[i].first - str.begin(),
				matches[i].second - matches[i].first));
		}
		return regex_results(res, mch);
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
		return utf8::to8(U"\u2023");
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
	std::string _cp = utf8::to8(std::u32string(1, cp));
	return _split_on_codepoint<char>(s, _cp);
}

string_list<char32_t> split_on_codepoint(const std::u32string& s, char32_t cp)
{
	std::u32string _cp(1, cp);
	return _split_on_codepoint<char32_t>(s, _cp);
}

template<typename T> void token_iterator<T>::ctor_eos()
{
	is_end_iterator = true;
}

template<typename T> void token_iterator<T>::ctor_itr(std::initializer_list<const T*> sep, bool whole_sequence)
	throw(std::bad_alloc)
{
	whole_seq = whole_sequence;
	is_end_iterator = false;
	bidx = 0;
	eidx = 0;
	for(auto i : sep)
		spliton.insert(i);
	load_helper();
}

template<typename T> bool token_iterator<T>::equals_op(const token_iterator<T>& itr) const throw()
{
	bool is_end_a = is_end_iterator || (bidx >= str.length());
	bool is_end_b = itr.is_end_iterator || (itr.bidx >= itr.str.length());
	if(is_end_a)
		if(is_end_b)
			return true;
		else
			return false;
	else
		if(is_end_b)
			return false;
		else
			return bidx == itr.bidx;
}

template<typename T> const std::basic_string<T>& token_iterator<T>::dereference() const throw()
{
	return tmp;
}

template<typename T> token_iterator<T> token_iterator<T>::postincrement() throw(std::bad_alloc)
{
	token_iterator<T> t = *this;
	++*this;
	return t;
}

template<typename T> token_iterator<T>& token_iterator<T>::preincrement() throw(std::bad_alloc)
{
	bidx = eidx + is_sep(eidx);
	load_helper();
	return *this;
}

template<typename T> void token_iterator<T>::load_helper()
{
	size_t t;
	if(whole_seq)
		while(bidx < str.length() && (t = is_sep(bidx)))
			bidx += t;
	eidx = bidx;
	while(eidx < str.length() && !is_sep(eidx))
		eidx++;
	tmp.resize(eidx - bidx);
	std::copy(str.begin() + bidx, str.begin() + eidx, tmp.begin());
}

template<typename T> size_t token_iterator<T>::is_sep(size_t pos)
{
	if(pos >= str.length())
		return 0;
	std::basic_string<T> h(1, str[pos++]);
	while(true) {
		if(spliton.count(h))
			return h.length();
		auto i = spliton.lower_bound(h);
		//If string at i is end-of-set or does not start with h, there can't be a match.
		if(i == spliton.end())
			return 0;
		std::basic_string<T> i2 = *i;
		if(i2.length() < h.length() || (i2.substr(0, h.length()) != h))
			return 0;
		h = h + std::basic_string<T>(1, str[pos++]);
	}
}

template<typename T> void token_iterator<T>::pull_fn()
{
	eat_argument(&token_iterator<T>::ctor_itr);
	eat_argument(&token_iterator<T>::ctor_eos);
	eat_argument(&token_iterator<T>::postincrement);
	eat_argument(&token_iterator<T>::preincrement);
	eat_argument(&token_iterator<T>::dereference);
	eat_argument(&token_iterator<T>::equals_op);
	eat_argument(&token_iterator<T>::is_sep);
	eat_argument(&token_iterator<T>::load_helper);
}

namespace
{
	template<typename T> void pull_token_itr()
	{
		token_iterator<T>::pull_fn();
	}

	void pull_token_itr2()
	{
		pull_token_itr<char>();
		pull_token_itr<char32_t>();
	}
}

void _dummy_63263896236732867328673826783276283673867()
{
	pull_token_itr2();
}