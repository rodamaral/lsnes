#include "library/zip.hpp"
#include "lua/internal.hpp"

namespace
{
	class lua_zip_writer
	{
	public:
		lua_zip_writer(lua::state& L, const std::string& filename, unsigned compression);
		~lua_zip_writer()
		{
			if(w) delete w;
		}
		static int create(lua::state& L, lua::parameters& P)
		{
			auto filename = P.arg<std::string>();
			auto compression = P.arg_opt<unsigned>(9);
			if(compression < 0)
				compression = 0;
			if(compression > 9)
				compression = 9;
			lua::_class<lua_zip_writer>::create(L, filename, compression);
			return 1;
		}
		int commit(lua::state& L, const std::string& fname)
		{
			if(!w)
				throw std::runtime_error("Zip writer already finished");
			if(file_open)
				w->close_file();
			file_open = NULL;
			w->commit();
			delete w;
			w = NULL;
		}
		int rollback(lua::state& L, const std::string& fname)
		{
			if(!w)
				throw std::runtime_error("Zip writer already finished");
			delete w;
			w = NULL;
		}
		int close_file(lua::state& L, const std::string& fname)
		{
			if(!w)
				throw std::runtime_error("Zip writer already finished");
			if(!file_open)
				throw std::runtime_error("Zip writer doesn't have file open");
			w->close_file();
			file_open = NULL;
		}
		int create_file(lua::state& L, const std::string& fname)
		{
			if(!w)
				throw std::runtime_error("Zip writer already finished");
			std::string filename = L.get_string(2, "ZIPWRITER::create_file");
			if(file_open) {
				w->close_file();
				file_open = NULL;
			}
			file_open = &w->create_file(filename);
		}
		int write(lua::state& L, const std::string& fname)
		{
			if(!w)
				throw std::runtime_error("Zip writer already finished");
			if(!file_open)
				throw std::runtime_error("Zip writer doesn't have file open");
			std::string _data = L.get_string(2, "ZIPWRITER::write");
			std::vector<char> data(_data.length());
			std::copy(_data.begin(), _data.end(), data.begin());
			file_open->write(&data[0], data.size());
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

	lua::_class<lua_zip_writer> class_zipwriter(lua_class_fileio, "ZIPWRITER", {
		{"new", lua_zip_writer::create},
	}, {
		{"commit", &lua_zip_writer::commit},
		{"rollback", &lua_zip_writer::rollback},
		{"close_file", &lua_zip_writer::close_file},
		{"create_file", &lua_zip_writer::create_file},
		{"write", &lua_zip_writer::write}
	});

	lua_zip_writer::lua_zip_writer(lua::state& L, const std::string& filename, unsigned compression)
	{
		file = filename;
		w = new zip::writer(filename, compression);
		file_open = NULL;
	}

	lua::fnptr2 lua_enumerate_zip(lua_func_zip, "zip.enumerate", [](lua::state& L, lua::parameters& P) -> int {
		auto filename = P.arg<std::string>();
		auto invert = P.arg_opt<bool>(false);

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
	});
}
