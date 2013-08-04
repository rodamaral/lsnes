#include "lua/internal.hpp"
#include "library/minmax.hpp"
#include "library/zip.hpp"
#include "core/memorymanip.hpp"
#include <functional>

namespace
{
	std::string dashstring(char ch, int dashes)
	{
		if(dashes)
			return std::string(1, ch) + std::string(dashes, '=') + std::string(1, ch);
		else
			return std::string(1, ch) + std::string(1, ch);
	}

	struct replace
	{
		replace()
		{
			upper_buf = NULL;
			upper_ptr = 0;
			upper_size = 0;
			upper_eof = false;
			matched = 0;
			copied = 0;
			source = 0;
			target = "[[]]";
		}
		replace(const std::string& _target)
			: replace()
		{
			target = _target;
		}
		std::pair<const char*, size_t> run(std::function<std::pair<const char*, size_t>()> fn);
	private:
		char buffer[4096];
		std::string target;
		size_t matched;
		size_t copied;
		int source;
		const char* upper_buf;
		size_t upper_ptr;
		size_t upper_size;
		bool upper_eof;
	};

	std::string pattern = "@@LUA_SCRIPT_FILENAME@@";

	std::pair<const char*, size_t> replace::run(std::function<std::pair<const char*, size_t>()> fn)
	{
		size_t emitted = 0;
		while(emitted < sizeof(buffer)) {
			while(upper_ptr == upper_size && !upper_eof) {
				auto g = fn();
				upper_buf = g.first;
				upper_size = g.second;
				upper_ptr = 0;
				if(!upper_buf && !upper_size)
					upper_eof = true;
			}
			if(upper_ptr == upper_size && source == 0) {
				if(!matched)
					break;
				copied = 0;
				source = 1;
			}
			switch(source) {
			case 0:		//Upper_buf.
				if(upper_buf[upper_ptr] == pattern[matched]) {
					matched++;
					upper_ptr++;
					if(matched == pattern.length()) {
						source = 2;
						copied = 0;
					}
				} else if(matched) {
					//Flush the rest.
					source = 1;
					copied = 0;
				} else {
					buffer[emitted++] = upper_buf[upper_ptr++];
				}
				break;
			case 1:		//Source.
				if(matched == 2 && upper_ptr < upper_size && upper_buf[upper_ptr] == '@') {
					//This is exceptional, just flush the first '@'.
					buffer[emitted++] = '@';
					upper_ptr++;
					matched = 2;
					source = 0;
				} else if(copied == matched) {
					//End.
					matched = 0;
					source = 0;
				} else {
					buffer[emitted++] = pattern[copied++];
				}
				break;
			case 2:		//Target.
				if(copied == target.size()) {
					//End
					matched = 0;
					source = 0;
				} else {
					buffer[emitted++] = target[copied++];
				}
				break;
			}
		}
		if(!emitted)
			return std::make_pair(reinterpret_cast<const char*>(NULL), 0);
		return std::make_pair(buffer, emitted);
	}

	struct reader
	{
		reader(std::istream& _s, const std::string& fn)
			: s(_s)
		{
			int dashes = 0;
			while(true) {
				std::string tmpl = dashstring(']', dashes);
				if(fn.find(tmpl) == std::string::npos)
					break;
			}
			rpl = replace(dashstring('[', dashes) + fn + dashstring(']', dashes));
		}
		const char* rfn(lua_State* L, size_t* size);
		static const char* rfn(lua_State* L, void* data, size_t* size)
		{
			return reinterpret_cast<reader*>(data)->rfn(L, size);
		}
	private:
		std::istream& s;
		replace rpl;
	};
	
	const char* reader::rfn(lua_State* L, size_t* size)
	{
		auto g = rpl.run([this]() -> std::pair<const char*, size_t> {
			size_t size;
			static char buffer[4096];
			if(!this->s)
				return std::make_pair(reinterpret_cast<const char*>(NULL), 0);
			this->s.read(buffer, sizeof(buffer));
			size = this->s.gcount();
			if(!size) {
				return std::make_pair(reinterpret_cast<const char*>(NULL), 0);
			}
			return std::make_pair(buffer, size);
		});
		*size = g.second;
		return g.first;
	}

	void load_chunk(lua_state& L, const std::string& fname)
	{
		std::string file2;
		std::string file1 = L.get_string(1, fname.c_str());
		if(L.type(2) != LUA_TNIL && L.type(2) != LUA_TNONE)
			file2 = L.get_string(2, fname.c_str());
		std::string absfilename = resolve_file_relative(file1, file2);
		std::istream& file = open_file_relative(file1, file2);
		std::string chunkname;
		if(file2 != "")
			chunkname = file2 + "[" + file1 + "]";
		else
			chunkname = file1;
		reader rc(file, absfilename);
		int r = lua_load(L.handle(), reader::rfn, &rc, chunkname.c_str()
#if LUA_VERSION_NUM == 502
			, "t"
#endif			
		);
		delete &file;
		if(r == 0) {
			return;
		} else if(r == LUA_ERRSYNTAX) {
			(stringfmt() << "Syntax error: " << L.tostring(-1)).throwex();
		} else if(r == LUA_ERRMEM) {
			(stringfmt() << "Out of memory: " << L.tostring(-1)).throwex();
		} else {
			(stringfmt() << "Unknown error: " << L.tostring(-1)).throwex();
		}
	}

	function_ptr_luafun loadfile2(LS, "loadfile2", [](lua_state& L, const std::string& fname)
		-> int {
		load_chunk(L, fname);
		return 1;
	});

	function_ptr_luafun dofile2(LS, "dofile2", [](lua_state& L, const std::string& fname)
		-> int {
		load_chunk(L, fname);
		int old_sp = lua_gettop(L.handle());
		lua_call(L.handle(), 0, LUA_MULTRET);
		int new_sp = lua_gettop(L.handle());
		return new_sp - (old_sp - 1);
	});
}
