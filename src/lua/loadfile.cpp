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
			first_chunk = true;
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
		bool first_chunk;
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
				if(first_chunk) {
					static char binary_lua_id[] = {0x1B, 0x4C, 0x75, 0x61};
					if(upper_size >= 4 && !memcmp(upper_buf, binary_lua_id, 4))
						throw std::runtime_error("Binary Lua chunks are not allowed");
					first_chunk = false;
				}
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
		const std::string& get_err() { return err; }
	private:
		std::istream& s;
		replace rpl;
		std::string err;
	};

	class lua_file_reader
	{
	public:
		lua_file_reader(lua::state& L, std::istream* strm);
		~lua_file_reader()
		{
			delete &s;
		}
		int read(lua::state& L, const std::string& fname)
		{
			if(L.type(2) == LUA_TNUMBER) {
				//Read specified number of bytes.
				size_t sz = L.get_numeric_argument<size_t>(2, fname.c_str());
				std::vector<char> buf;
				buf.resize(sz);
				s.read(&buf[0], sz);
				if(!s && !s.gcount()) {
					L.pushnil();
					return 1;
				}
				L.pushlstring(&buf[0], s.gcount());
				return 1;
			} else if(L.type(2) == LUA_TNIL || L.type(2) == LUA_TNONE) {
				//Read next line.
				std::string tmp;
				std::getline(s, tmp);
				if(!s) {
					L.pushnil();
					return 1;
				}
				istrip_CR(tmp);
				L.pushlstring(tmp);
				return 1;
			} else
				(stringfmt() << "Expected number or nil as the 2nd argument of " << fname).throwex();
		}
		int lines(lua::state& L, const std::string& fname)
		{
			L.pushlightuserdata(this);
			L.pushcclosure(lua_file_reader::lines_helper2, 1);
			//Trick: The first parameter is the userdata for this object, so by making it state, we
			//can pin this object.
			L.pushvalue(1);
			L.pushboolean(true);
			return 3;
		}
		int lines_helper(lua_State* L)
		{
			std::string tmp;
			std::getline(s, tmp);
			if(!s) {
				lua_pushnil(L);
				return 1;
			}
			istrip_CR(tmp);
			lua_pushlstring(L, tmp.c_str(), tmp.length());
			return 1;
		}
		static int lines_helper2(lua_State* L)
		{
			reinterpret_cast<lua_file_reader*>(lua_touserdata(L, lua_upvalueindex(1)))->lines_helper(L);
		}
		std::string print()
		{
			return "";
		}
	private:
		std::istream& s;
	};

	const char* reader::rfn(lua_State* L, size_t* size)
	{
		try {
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
		} catch(std::exception& e) {
			err = e.what();
			*size = 0;
			return NULL;
		}
	}

	void load_chunk(lua::state& L, const std::string& fname)
	{
		std::string file2;
		std::string file1 = L.get_string(1, fname.c_str());
		if(L.type(2) != LUA_TNIL && L.type(2) != LUA_TNONE)
			file2 = L.get_string(2, fname.c_str());
		std::string absfilename = zip::resolverel(file1, file2);
		std::istream& file = zip::openrel(file1, file2);
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
		if(rc.get_err() != "")
			throw std::runtime_error(rc.get_err());
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

	lua::fnptr loadfile2(lua_func_load, "loadfile2", [](lua::state& L, const std::string& fname)
		-> int {
		load_chunk(L, fname);
		return 1;
	});

	lua::fnptr dofile2(lua_func_load, "dofile2", [](lua::state& L, const std::string& fname)
		-> int {
		load_chunk(L, fname);
		int old_sp = lua_gettop(L.handle());
		lua_call(L.handle(), 0, LUA_MULTRET);
		int new_sp = lua_gettop(L.handle());
		return new_sp - (old_sp - 1);
	});

	lua::fnptr resolvefile(lua_func_load, "resolve_filename", [](lua::state& L, const std::string& fname)
	{
		std::string file2;
		std::string file1 = L.get_string(1, fname.c_str());
		if(L.type(2) != LUA_TNIL && L.type(2) != LUA_TNONE)
			file2 = L.get_string(2, fname.c_str());
		std::string absfilename = zip::resolverel(file1, file2);
		L.pushlstring(absfilename);
		return 1;
	});

	lua::fnptr openfile(lua_func_load, "open_file", [](lua::state& L, const std::string& fname) -> int {
		std::string file2;
		std::string file1 = L.get_string(1, fname.c_str());
		if(L.type(2) != LUA_TNIL && L.type(2) != LUA_TNONE)
			file2 = L.get_string(2, fname.c_str());
		std::istream& s = zip::openrel(file1, file2);
		try {
			lua::_class<lua_file_reader>::create(L, &s);
			return 1;
		} catch(...) {
			delete &s;
			throw;
		}
	});

	lua::_class<lua_file_reader> class_filreader(lua_class_fileio, "FILEREADER", {}, {
		{"__call", &lua_file_reader::read},
		{"lines", &lua_file_reader::lines}
	});

	lua_file_reader::lua_file_reader(lua::state& L, std::istream* strm)
		: s(*strm)
	{
	}
}
