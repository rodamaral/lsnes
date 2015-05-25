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

	std::string CONST_pattern = "@@LUA_SCRIPT_FILENAME@@";

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
					const static char binary_lua_id[] = {0x1B, 0x4C, 0x75, 0x61};
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
				if(upper_buf[upper_ptr] == CONST_pattern[matched]) {
					matched++;
					upper_ptr++;
					if(matched == CONST_pattern.length()) {
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
					buffer[emitted++] = CONST_pattern[copied++];
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
		const char* rfn(size_t* size);
		static const char* rfn(lua_State* unused, void* data, size_t* size)
		{
			return reinterpret_cast<reader*>(data)->rfn(size);
		}
		const std::string& get_err() { return err; }
	private:
		std::istream& s;
		replace rpl;
		std::string err;
		char buffer[4096];
	};

	class lua_file_reader
	{
	public:
		lua_file_reader(lua::state& L, std::istream* strm);
		static size_t overcommit(std::istream* strm) { return 0; }
		~lua_file_reader()
		{
			delete &s;
		}
		static int create(lua::state& L, lua::parameters& P)
		{
			auto file1 = P.arg<std::string>();
			auto file2 = P.arg_opt<std::string>("");
			std::istream& s = zip::openrel(file1, file2);
			try {
				lua::_class<lua_file_reader>::create(L, &s);
				return 1;
			} catch(...) {
				delete &s;
				throw;
			}
		}
		int read(lua::state& L, lua::parameters& P)
		{
			P(P.skipped());

			if(P.is_number()) {
				//Read specified number of bytes.
				size_t sz;
				P(sz);
				std::vector<char> buf;
				buf.resize(sz);
				s.read(&buf[0], sz);
				if(!s && !s.gcount()) {
					L.pushnil();
					return 1;
				}
				L.pushlstring(&buf[0], s.gcount());
				return 1;
			} else if(P.is_novalue()) {
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
				P.expected("number or nil");
			return 0; //NOTREACHED
		}
		int lines(lua::state& L, lua::parameters& P)
		{
			L.pushlightuserdata(this);
			L.push_trampoline(lua_file_reader::lines_helper2, 1);
			//Trick: The first parameter is the userdata for this object, so by making it state, we
			//can pin this object.
			L.pushvalue(1);
			L.pushboolean(true);
			return 3;
		}
		int lines_helper(lua::state& L)
		{
			std::string tmp;
			std::getline(s, tmp);
			if(!s) {
				L.pushnil();
				return 1;
			}
			istrip_CR(tmp);
			L.pushlstring(tmp.c_str(), tmp.length());
			return 1;
		}
		static int lines_helper2(lua::state& L)
		{
			return reinterpret_cast<lua_file_reader*>(L.touserdata(L.trampoline_upval(1)))->
				lines_helper(L);
		}
	private:
		std::istream& s;
	};

	const char* reader::rfn(size_t* size)
	{
		try {
			auto g = rpl.run([this]() -> std::pair<const char*, size_t> {
				size_t size;
				if(!this->s)
					return std::make_pair(reinterpret_cast<const char*>(NULL), 0);
				this->s.read(this->buffer, sizeof(this->buffer));
				size = this->s.gcount();
				if(!size) {
					return std::make_pair(reinterpret_cast<const char*>(NULL), 0);
				}
				return std::make_pair(this->buffer, size);
			});
			*size = g.second;
			return g.first;
		} catch(std::exception& e) {
			err = e.what();
			*size = 0;
			return NULL;
		}
	}

	void load_chunk(lua::state& L, lua::parameters& P)
	{
		auto file1 = P.arg<std::string>();
		auto file2 = P.arg_opt<std::string>("");
		std::string absfilename = zip::resolverel(file1, file2);
		std::istream& file = zip::openrel(file1, file2);
		std::string chunkname;
		if(file2 != "")
			chunkname = file2 + "[" + file1 + "]";
		else
			chunkname = file1;
		reader rc(file, absfilename);
		int r = lua_load(L.handle(), reader::rfn, &rc, chunkname.c_str() LUA_LOADMODE_ARG("t") );
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

	lua_file_reader::lua_file_reader(lua::state& L, std::istream* strm)
		: s(*strm)
	{
	}

	int loadfile2(lua::state& L, lua::parameters& P)
	{
		load_chunk(L, P);
		return 1;
	}

	int dofile2(lua::state& L, lua::parameters& P)
	{
		load_chunk(L, P);
		int old_sp = lua_gettop(L.handle());
		lua_call(L.handle(), 0, LUA_MULTRET);
		int new_sp = lua_gettop(L.handle());
		return new_sp - (old_sp - 1);
	}

	int resolve_filename(lua::state& L, lua::parameters& P)
	{
		auto file1 = P.arg<std::string>();
		auto file2 = P.arg_opt<std::string>("");
		std::string absfilename = zip::resolverel(file1, file2);
		L.pushlstring(absfilename);
		return 1;
	}

	lua::functions LUA_load_fns(lua_func_load, "", {
		{"loadfile2", loadfile2},
		{"dofile2", dofile2},
		{"resolve_filename", resolve_filename},
	});

	lua::_class<lua_file_reader> LUA_class_filreader(lua_class_fileio, "FILEREADER", {
		{"open", lua_file_reader::create},
	}, {
		{"__call", &lua_file_reader::read},
		{"lines", &lua_file_reader::lines}
	});
}
