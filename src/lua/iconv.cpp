#include <iconv.h>
#include "lua/internal.hpp"
#include "library/string.hpp"
#include "library/utf8.hpp"
#include <cstring>
#include <cerrno>

namespace
{
	struct buffer
	{
		buffer(bool _is_out, std::string& _str)
			: str(_str), is_out(_is_out)
		{
			buffer_size = 0;
			char_count = 0;
			if(!is_out) {
				while(buffer_size < sizeof(buf) && char_count < str.length())
					buf[buffer_size++] = str[char_count++];
			}
		}
		std::pair<char*,size_t> get()
		{
			return std::make_pair(buf, is_out ? sizeof(buf) : buffer_size);
		}
		void set(std::pair<char*,size_t> pos)
		{
			if(is_out) {
				size_t emitted = sizeof(buf) - pos.second;
				str.resize(str.length() + emitted);
				std::copy(pos.first - emitted, pos.first, str.begin() + char_count);
				char_count += emitted;
			} else {
				size_t eaten = buffer_size - pos.second;
				memmove(buf, buf + eaten, buffer_size - eaten);
				buffer_size -= eaten;
				while(buffer_size < sizeof(buf) && char_count < str.length())
					buf[buffer_size++] = str[char_count++];
			}
		}
		size_t left()
		{
			return buffer_size + str.length() - char_count;
		}
		size_t unprocessed()
		{
			return buffer_size;
		}
	private:
		char buf[1024];
		size_t buffer_size;
		size_t char_count;
		std::string& str;
		bool is_out;
	};

	struct lua_iconv
	{
	public:
		lua_iconv(lua::state& L, const char* from, const char* to);
		static size_t overcommit(const char* from, const char* to) { return 0; }
		~lua_iconv() throw();
		int call(lua::state& L, lua::parameters& P);
		static int create(lua::state& L, lua::parameters& P);
		std::string print()
		{
			return spec;
		}
	private:
		iconv_t ctx;
		buffer input();
		std::string spec;
	};

	lua::_class<lua_iconv> LUA_class_iconv(lua_class_pure, "ICONV", {
		{"new", lua_iconv::create},
	}, {
		{"__call", &lua_iconv::call},
	}, &lua_iconv::print);


	lua_iconv::lua_iconv(lua::state& L, const char* from, const char* to)
	{
		spec = std::string(from) + "->" + to;
		errno = 0;
		ctx = iconv_open(to, from);
		if(errno) {
			int err = errno;
			(stringfmt() << "Error creating character set converter: " << strerror(err)).throwex();
		}
	}

	lua_iconv::~lua_iconv() throw()
	{
		iconv_close(ctx);
	}

	int lua_iconv::call(lua::state& L, lua::parameters& P)
	{
		std::string src;

		P(P.skipped(), src);

		std::string dst;
		buffer input(false, src);
		buffer output(true, dst);
		std::string code = "";
		while(true) {
			auto _input = input.get();
			auto _output = output.get();
			int r = iconv(ctx, &_input.first, &_input.second, &_output.first, &_output.second);
			size_t unprocessed = _input.second;
			input.set(_input);
			output.set(_output);
			if(r < 0) {
				int err = errno;
				switch(err) {
				case E2BIG:
					continue; //Just retry with new output bufer.
				case EILSEQ:
					code = "INVALID";
					goto exit;
				case EINVAL:
					if(unprocessed != input.unprocessed())
						continue;  //Retry.
					code = "INCOMPLETE";
					goto exit;
				default:
					code = "INTERNALERR";
					goto exit;
				}
			} else if(!input.unprocessed())
				break;
		}
exit:
		L.pushboolean(!code.length());
		L.pushlstring(dst);
		if(code.length()) {
			L.pushnumber(input.left());
			L.pushlstring(code);
		}
		return code.length() ? 4 : 2;
	}

	int lua_iconv::create(lua::state& L, lua::parameters& P)
	{
		std::string from, to;

		P(from, to);

		lua::_class<lua_iconv>::create(L, from.c_str(), to.c_str());
		return 1;
	}

	int _lsnes_string_byteU(lua::state& L, lua::parameters& P)
	{
		std::string _str;
		size_t i, j;

		P(_str, P.optional(i, 1), P.optional2(j, i));

		std::u32string str = utf8::to32(_str);
		if(i == 0) i = 1;
		size_t p = 0;
		for(size_t k = i - 1; k < j && k < str.length(); k++) {
			L.pushnumber(str[k]);
			p++;
		}
		return p;
	}

	int _lsnes_string_charU(lua::state& L, lua::parameters& P)
	{
		std::u32string str;
		while(P.more()) {
			uint32_t cp;

			P(cp);

			//Surrogates are not valid unicode.
			if((cp & 0xD800) == 0xD800)
				throw std::runtime_error("Invalid character");
			//Explicit noncharacters.
			if(cp >= 0xFDD0 && cp < 0xFDF0)
				throw std::runtime_error("Invalid character");
			//The last two characters of each plane are noncharacters.
			if((cp & 0xFFFE) == 0xFFFE)
				throw std::runtime_error("Invalid character");
			//Last valid plane is plane 16.
			if((cp >> 16) > 16)
				throw std::runtime_error("Invalid character");
			//Ok.
			str += std::u32string(1, cp);
		}
		L.pushlstring(utf8::to8(str));
		return 1;
	}

	lua::functions LUA_iconv_fns(lua_func_bit, "", {
		{"_lsnes_string_byteU", _lsnes_string_byteU},
		{"_lsnes_string_charU", _lsnes_string_charU},
	});
}
