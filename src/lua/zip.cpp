#include "library/zip.hpp"
#include "lua/internal.hpp"

namespace
{
	class lua_zip_writer
	{
	public:
		lua_zip_writer(lua_state& L, const std::string& filename, unsigned compression);
		~lua_zip_writer()
		{
			if(w) delete w;
		}
		int commit(lua_state& L, const std::string& fname)
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
		int rollback(lua_state& L, const std::string& fname)
		{
			if(!w)
				throw std::runtime_error("Zip writer already finished");
			delete w;
			w = NULL;
		}
		int close_file(lua_state& L, const std::string& fname)
		{
			if(!w)
				throw std::runtime_error("Zip writer already finished");
			if(!file_open)
				throw std::runtime_error("Zip writer doesn't have file open");
			w->close_file();
			file_open = NULL;
		}
		int create_file(lua_state& L, const std::string& fname)
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
		int write(lua_state& L, const std::string& fname)
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

	lua_class<lua_zip_writer> class_zipwriter("ZIPWRITER");

	lua_zip_writer::lua_zip_writer(lua_state& L, const std::string& filename, unsigned compression)
	{
		file = filename;
		w = new zip::writer(filename, compression);
		file_open = NULL;
		objclass<lua_zip_writer>().bind_multi(L, {
			{"commit", &lua_zip_writer::commit},
			{"rollback", &lua_zip_writer::rollback},
			{"close_file", &lua_zip_writer::close_file},
			{"create_file", &lua_zip_writer::create_file},
			{"write", &lua_zip_writer::write}
		});
	}

	function_ptr_luafun lua_zip(lua_func_zip, "zip.create", [](lua_state& L,
		const std::string& fname) -> int {
		unsigned compression = 9;
		std::string filename = L.get_string(1, fname.c_str());
		L.get_numeric_argument<unsigned>(2, compression, fname.c_str());
		if(compression < 0)
			compression = 0;
		if(compression > 9)
			compression = 9;
		lua_class<lua_zip_writer>::create(L, filename, compression);
		return 1;
	});

	function_ptr_luafun lua_enumerate_zip(lua_func_zip, "zip.enumerate", [](lua_state& L,
		const std::string& fname) -> int {
		std::string filename = L.get_string(1, fname.c_str());
		bool invert = false;
		if(L.type(2) != LUA_TNONE && L.type(2) != LUA_TNIL)
			invert = L.toboolean(2);
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
