#include "library/zip.hpp"
#include "library/minmax.hpp"
#include "lua/internal.hpp"

namespace
{
	class lua_zip_writer
	{
	public:
		lua_zip_writer(lua::state& L, const std::string& filename, unsigned compression);
		static size_t overcommit(const std::string& filename, unsigned compression) { return 0; }
		~lua_zip_writer()
		{
			if(w) delete w;
		}
		static int create(lua::state& L, lua::parameters& P)
		{
			std::string filename;
			unsigned compression;

			P(filename, P.optional(compression, 9));

			compression = clip(compression, 0U, 9U);

			lua::_class<lua_zip_writer>::create(L, filename, compression);
			return 1;
		}
		int commit(lua::state& L, lua::parameters& P)
		{
			if(!w) throw std::runtime_error("Zip writer already finished");

			if(file_open)
				w->close_file();
			file_open = NULL;
			w->commit();
			delete w;
			w = NULL;
			return 0;
		}
		int rollback(lua::state& L, lua::parameters& P)
		{
			if(!w) throw std::runtime_error("Zip writer already finished");

			delete w;
			w = NULL;
			return 0;
		}
		int close_file(lua::state& L, lua::parameters& P)
		{
			if(!w) throw std::runtime_error("Zip writer already finished");
			if(!file_open) throw std::runtime_error("Zip writer doesn't have file open");

			w->close_file();
			file_open = NULL;
			return 0;
		}
		int create_file(lua::state& L, lua::parameters& P)
		{
			std::string filename;

			if(!w) throw std::runtime_error("Zip writer already finished");

			P(P.skipped(), filename);

			if(file_open) {
				w->close_file();
				file_open = NULL;
			}
			file_open = &w->create_file(filename);
			return 0;
		}
		int write(lua::state& L, lua::parameters& P)
		{
			std::string _data;

			if(!w) throw std::runtime_error("Zip writer already finished");
			if(!file_open) throw std::runtime_error("Zip writer doesn't have file open");

			P(P.skipped(), _data);

			std::vector<char> data(_data.length());
			std::copy(_data.begin(), _data.end(), data.begin());
			file_open->write(&data[0], data.size());
			return 0;
		}
		std::string print()
		{
			return file;
		}
	private:
		zip::writer* w;
		std::ostream* file_open;
		std::string file;
	};

	lua::_class<lua_zip_writer> LUA_class_zipwriter(lua_class_fileio, "ZIPWRITER", {
		{"new", lua_zip_writer::create},
	}, {
		{"commit", &lua_zip_writer::commit},
		{"rollback", &lua_zip_writer::rollback},
		{"close_file", &lua_zip_writer::close_file},
		{"create_file", &lua_zip_writer::create_file},
		{"write", &lua_zip_writer::write}
	}, &lua_zip_writer::print);

	lua_zip_writer::lua_zip_writer(lua::state& L, const std::string& filename, unsigned compression)
	{
		file = filename;
		w = new zip::writer(filename, compression);
		file_open = NULL;
	}

	int zip_enumerate(lua::state& L, lua::parameters& P)
	{
		std::string filename;
		bool invert;

		P(filename, P.optional(invert, false));

		zip::reader r(filename);
		L.newtable();
		size_t idx = 1;
		for(auto i : r) {
			if(invert) {
				L.pushlstring(i);
				L.pushboolean(true);
			} else {
				L.pushnumber(idx++);
				L.pushlstring(i);
			}
			L.rawset(-3);
		}
		return 1;
	}

	lua::functions LUA_zip_fns(lua_func_zip, "zip", {
		{"enumerate", zip_enumerate},
	});
}
