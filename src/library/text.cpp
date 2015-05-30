#include "text.hpp"
#include "utf8.hpp"
#include <cstring>

namespace
{
	size_t ustrlen(const char32_t* str)
	{
		size_t ans = 0;
		while(str[ans]) ans++;
		return ans;
	}

	template<typename T>
	struct iterator_pair
	{
		T begin;
		T end;
		size_t size;
		iterator_pair(T _begin, T _end, size_t _size)
			: begin(_begin), end(_end), size(_size)
		{
		}
	};

	template<typename T> struct is_wide {};
	template<> struct is_wide<text> { static const bool flag = true; };
	template<> struct is_wide<std::string> { static const bool flag = false; };
	template<> struct is_wide<std::u32string> { static const bool flag = true; };
	template<> struct is_wide<const char*> { static const bool flag = false; };
	template<> struct is_wide<const char32_t*> { static const bool flag = true; };

	iterator_pair<const char32_t*> toiterator(const text& str)
	{
		const char32_t* ptr = str._internal_ptr();
		size_t len = str.length();
		return iterator_pair<const char32_t*>(ptr, ptr + len, len);
	}

	iterator_pair<std::string::const_iterator> toiterator(const std::string& str)
	{
		return iterator_pair<std::string::const_iterator>(str.begin(), str.end(), utf8::strlen(str));
	}

	iterator_pair<std::u32string::const_iterator> toiterator(const std::u32string& str)
	{
		return iterator_pair<std::u32string::const_iterator>(str.begin(), str.end(), str.length());
	}

	iterator_pair<const char*> toiterator(const char* str)
	{
		auto len = utf8::strlen_c(str);
		return iterator_pair<const char*>(str, str + len.first, len.second);
	}

	iterator_pair<const char32_t*> toiterator(const char32_t* str)
	{
		size_t len = ustrlen(str);
		return iterator_pair<const char32_t*>(str, str + len, len);
	}

	template<typename A, typename B>
	text _concatenate(const A& a, const B& b)
	{
		text ret;
		auto aitr = toiterator(a);
		auto bitr = toiterator(b);
		ret._internal_reallocate(aitr.size + bitr.size);
		if(is_wide<A>::flag)
			std::copy(aitr.begin, aitr.end, ret._internal_ptr_mut());
		else
			utf8::to32i(aitr.begin, aitr.end, ret._internal_ptr_mut());
		if(is_wide<B>::flag)
			std::copy(bitr.begin, bitr.end, ret._internal_ptr_mut() + aitr.size);
		else
			utf8::to32i(bitr.begin, bitr.end, ret._internal_ptr_mut() + aitr.size);
		ret._internal_set_len(aitr.size + bitr.size);
		return ret;
	}

	class compare_iterator
	{
	public:
		typedef std::output_iterator_tag iterator_category;
		typedef char32_t value_type;
		typedef int difference_type;
		typedef char32_t* pointer;
		typedef char32_t& reference;
		compare_iterator(const char32_t* _left, size_t _leftlen)
			: left(_left), leftlen(_leftlen), _pos(0), _decision(0), pos(&_pos), decision(&_decision)
		{
		}
		compare_iterator& operator++() { return *this; }
		compare_iterator operator++(int);
		compare_iterator& operator*() { return *this; }
		compare_iterator& operator=(char32_t rhs)
		{
			if(*decision) return *this;
			if(*pos == leftlen) {
				//We have come here, all being equal so far and rhs has more characters.
				//so the rhs is greater.
				*decision = -1;
				return *this;
			}
			char32_t lhs = left[*pos];
			if(lhs < rhs) {
				//lhs is smaller.
				*decision = -1;
				return *this;
			} else if(lhs > rhs) {
				//rhs is greater.
				*decision = 1;
				return *this;
			}
			++*pos;
			return *this;
		}
		int get_decision()
		{
			if(*decision) return *decision;
			//If lhs didn't get scanned completely, it is greater.
			return (*pos < leftlen) ? 1 : 0;
		}
	private:
		const char32_t* left;
		size_t leftlen;
		size_t _pos;
		int _decision;
		size_t* pos;
		int* decision;
	};

	template<typename B>
	int _compare(const text& a, const B& b) throw()
	{
		auto bitr = toiterator(b);
		compare_iterator itr(a._internal_ptr(), a.length());
		if(is_wide<B>::flag)
			std::copy(bitr.begin, bitr.end, itr);
		else
			utf8::to32i(bitr.begin, bitr.end, itr);
		return itr.get_decision();
	}

	template<typename B>
	void _make(text& init, const B& b) throw()
	{
		auto bitr = toiterator(b);
		init._internal_init();
		init._internal_reallocate(bitr.size);
		if(is_wide<B>::flag)
			std::copy(bitr.begin, bitr.end, init._internal_ptr_mut());
		else
			utf8::to32i(bitr.begin, bitr.end, init._internal_ptr_mut());
		init._internal_set_len(bitr.size);
	}

}

text::text() throw()
	: data(NULL), len(0), allocated(0), tracker(memtracker::singleton(), "Strings", sizeof(*this))
{
}

text::text(const std::string& str) throw(std::bad_alloc)
	: tracker(memtracker::singleton(), "Strings", sizeof(*this))
{
	_make(*this, str);
}

text::text(const std::u32string& str) throw(std::bad_alloc)
	: tracker(memtracker::singleton(), "Strings", sizeof(*this))
{
	_make(*this, str);
}

text::text(const char* str) throw(std::bad_alloc)
	: tracker(memtracker::singleton(), "Strings", sizeof(*this))
{
	_make(*this, str);
}

text::text(const char32_t* str) throw(std::bad_alloc)
	: tracker(memtracker::singleton(), "Strings", sizeof(*this))
{
	_make(*this, str);
}

text::text(const text& t) throw(std::bad_alloc)
	: tracker(t.tracker.get_tracker(), t.tracker.get_category(), sizeof(*this))
{
	_make(*this, t);
}

text& text::operator=(const text& t) throw(std::bad_alloc)
{
	if(this == &t) return *this;
	if(allocated < t.len)
		reallocate(t.len);
	std::copy(t.data, t.data + t.len, data);
	len = t.len;
	return *this;
}

text::~text() throw() { delete[] data; }

void text::set_tracking_category(const char* category)
{
	tracker.track(memtracker::singleton(), category);
}

text text::concatenate(const text& a, const text& b) throw(std::bad_alloc)
{
	return _concatenate(a, b);
}

text text::concatenate(const std::string& a, const text& b) throw(std::bad_alloc)
{
	return _concatenate(a, b);
}

text text::concatenate(const std::u32string& a, const text& b) throw(std::bad_alloc)
{
	return _concatenate(a, b);
}

text text::concatenate(const char* a, const text& b) throw(std::bad_alloc)
{
	return _concatenate(a, b);
}

text text::concatenate(const char32_t* a, const text& b) throw(std::bad_alloc)
{
	return _concatenate(a, b);
}

text text::concatenate(const text& a, const std::string& b) throw(std::bad_alloc)
{
	return _concatenate(a, b);
}

text text::concatenate(const text& a, const std::u32string& b) throw(std::bad_alloc)
{
	return _concatenate(a, b);
}

text text::concatenate(const text& a, const char* b) throw(std::bad_alloc)
{
	return _concatenate(a, b);
}

text text::concatenate(const text& a, const char32_t* b) throw(std::bad_alloc)
{
	return _concatenate(a, b);
}

int text::compare(const text& a, const text& b) throw()
{
	return _compare(a, b);
}

int text::compare(const text& a, const std::string& b) throw()
{
	return _compare(a, b);
}

int text::compare(const text& a, const std::u32string& b) throw()
{
	return _compare(a, b);
}

int text::compare(const text& a, const char* b) throw()
{
	return _compare(a, b);
}

int text::compare(const text& a, const char32_t* b) throw()
{
	return _compare(a, b);
}

text text::getline(std::istream& is)
{
	std::string line;
	std::getline(is, line);
	return text(line);
}

text text::substr(size_t start, size_t _len) const throw(std::bad_alloc, std::out_of_range)
{
	if(start > len || _len > len || start + _len > len)
		throw std::out_of_range("Text subslice out of range");
	text t;
	t.reallocate(_len);
	std::copy(data + start, data + start + _len, t.data);
	t.len = _len;
	return t;
}

text text::substr(size_t start) const throw(std::bad_alloc, std::out_of_range)
{
	return substr(start, len - start);
}

size_t text::find_first_of(const char32_t* chlist, size_t ptr) const throw()
{
	for(size_t i = ptr; i < len; i++) {
		for(size_t j = 0; chlist[j]; j++) {
			if(data[i] == chlist[j])
				return i;
		}
	}
	return (size_t)(ssize_t)-1;
}

size_t text::find_first_not_of(const char32_t* chlist, size_t ptr) const throw()
{
	for(size_t i = ptr; i < len; i++) {
		for(size_t j = 0; chlist[j]; j++) {
			if(data[i] == chlist[j])
				goto ok;
		}
		return i;
ok:
		;
	}
	return (size_t)(ssize_t)-1;
}

size_t text::find_last_of(const char32_t* chlist, size_t ptr) const throw()
{
	for(size_t i = len - 1; i < len && i >= ptr; i--) {
		for(size_t j = 0; chlist[j]; j++) {
			if(data[i] == chlist[j])
				return i;
		}
	}
	return (size_t)(ssize_t)-1;
}

size_t text::find_last_not_of(const char32_t* chlist, size_t ptr) const throw()
{
	for(size_t i = len - 1; i < len && i >= ptr; i--) {
		for(size_t j = 0; chlist[j]; j++) {
			if(data[i] == chlist[j])
				goto ok;
		}
		return i;
ok:
		;
	}
	return (size_t)(ssize_t)-1;
}

void text::reallocate(size_t newsize)
{
	auto oldalloc = allocated;
	char32_t* ndata = new char32_t[newsize];
	tracker(newsize);
	allocated = newsize;
	if(len) std::copy(data, data + len, ndata);
	if(data) delete[] data;
	tracker(-(ssize_t)oldalloc);
	data = ndata;
}

void text::ostream_helper(std::ostream& os) const
{
	char buffer[4096];
	size_t pos = 0;
	size_t len = length();
	while(pos < len) {
		std::pair<size_t, size_t> frag = output_utf8_fragment(pos, buffer, sizeof(buffer));
		pos += frag.first;
		os.write(buffer, frag.second);
	}
}

void text::resize(size_t sz)
{
	if(sz > allocated)
		reallocate(sz);
	for(size_t i = len; i < sz; i++)
		data[i] = 0;
	len = sz;
}

std::pair<size_t, size_t> text::output_utf8_fragment(size_t startidx, char* out, size_t outsize) const throw()
{
	size_t ptr = 0;
	size_t chs = 0;
	for(size_t i = startidx; i < len; i++) {
		uint32_t ch = data[i];
		size_t chlen;
		if(ch < 0x80) chlen = 1;
		else if(ch < 0x800) chlen = 2;
		else if(ch < 0x10000) chlen = 3;
		else chlen = 4;
		if(ptr + chlen > outsize)
			break;
		switch(chlen) {
		case 1:
			out[ptr++] = ch;
			break;
		case 2:
			out[ptr++] = 192 + (ch >> 6);
			out[ptr++] = 128 + ((ch) & 0x3F);
			break;
		case 3:
			out[ptr++] = 224 + (ch >> 12);
			out[ptr++] = 128 + ((ch >> 6) & 0x3F);
			out[ptr++] = 128 + ((ch) & 0x3F);
			break;
		case 4:
			out[ptr++] = 240 + ((ch >> 18) & 7);
			out[ptr++] = 128 + ((ch >> 12) & 0x3F);
			out[ptr++] = 128 + ((ch >> 6) & 0x3F);
			out[ptr++] = 128 + ((ch) & 0x3F);
			break;
		}
		chs++;
	}
	return std::make_pair(chs, ptr);
}

size_t text::length_utf8() const throw()
{
	size_t ans = 0;
	for(size_t i = 0; i < len; i++) {
		if(data[i] < 0x80) ans += 1;
		else if(data[i] < 0x800) ans += 2;
		else if(data[i] < 0x10000) ans += 3;
		else ans += 4;
	}
	return ans;
}
