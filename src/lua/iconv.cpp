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
		~lua_iconv() throw();
		int call(lua::state& L, const std::string& fname);
		std::string print()
		{
			return spec;
		}
	private:
		iconv_t ctx;
		buffer input();
		std::string spec;
	};

	lua::_class<lua_iconv> class_iconv("ICONV");


	lua_iconv::lua_iconv(lua::state& L, const char* from, const char* to)
	{
		lua::objclass<lua_iconv>().bind_multi(L, {
			{"__call", &lua_iconv::call}
		});

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

	int lua_iconv::call(lua::state& L, const std::string& fname)
	{
		std::string src = L.get_string(2, fname.c_str());
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

	lua::fnptr iconv_new(lua_func_load, "iconv_new", [](lua::state& L, const std::string& fname) -> int {
		std::string from = L.get_string(1, fname.c_str());
		std::string to = L.get_string(2, fname.c_str());
		lua::_class<lua_iconv>::create(L, from.c_str(), to.c_str());
		return 1;
	});

	lua::fnptr iconv_byteU(lua_func_bit, "_lsnes_string_byteU", [](lua::state& L, const std::string& fname)
		-> int {
		std::string _str = L.get_string(1, fname.c_str());
		size_t i = 1;
		L.get_numeric_argument<size_t>(2, i, fname.c_str());
		size_t j = i;
		L.get_numeric_argument<size_t>(3, j, fname.c_str());
		std::u32string str = utf8::to32(_str);
		if(i == 0) i = 1;
		size_t p = 0;
		for(size_t k = i - 1; k < j && k < str.length(); k++) {
			L.pushnumber(str[k]);
			p++;
		}
		return p;
	});

	lua::fnptr iconv_charU(lua_func_bit, "_lsnes_string_charU", [](lua::state& L, const std::string& fname)
		-> int {
		std::u32string str;
		for(int i = 1; L.type(i) == LUA_TNUMBER; i++) {
			uint32_t cp = L.get_numeric_argument<uint32_t>(i, fname.c_str());
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
	});
}

