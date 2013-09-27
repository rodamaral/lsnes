#include "lua/internal.hpp"
#include "library/string.hpp"
#include "core/movie.hpp"
#include "core/moviedata.hpp"
#include "core/window.hpp"
#include <fstream>

void update_movie_state();

namespace
{
	class lua_inputmovie;

	class lua_inputframe
	{
		friend class lua_inputmovie;
	public:
		lua_inputframe(lua_State* L, controller_frame _f);
		int get_button(lua_State* L)
		{
			unsigned port = get_numeric_argument<unsigned>(L, 2, "INPUTFRAME::get_button");
			unsigned controller = get_numeric_argument<unsigned>(L, 3, "INPUTFRAME::get_button");
			unsigned button = get_numeric_argument<unsigned>(L, 4, "INPUTFRAME::get_button");
			short value = getbutton(port, controller, button);
			lua_pushboolean(L, value ? 1 : 0);
			return 1;
		}
		int get_axis(lua_State* L)
		{
			unsigned port = get_numeric_argument<unsigned>(L, 2, "INPUTFRAME::get_axis");
			unsigned controller = get_numeric_argument<unsigned>(L, 3, "INPUTFRAME::get_axis");
			unsigned button = get_numeric_argument<unsigned>(L, 4, "INPUTFRAME::get_axis");
			short value = getbutton(port, controller, button);
			lua_pushnumber(L, value);
			return 1;
		}
		int set_axis(lua_State* L)
		{
			unsigned port = get_numeric_argument<unsigned>(L, 2, "INPUTFRAME::set_axis");
			unsigned controller = get_numeric_argument<unsigned>(L, 3, "INPUTFRAME::set_axis");
			unsigned button = get_numeric_argument<unsigned>(L, 4, "INPUTFRAME::set_axis");
			short value;
			if(lua_type(L, 5) == LUA_TBOOLEAN)
				value = lua_toboolean(L, 5);
			else if(lua_type(L, 5) == LUA_TNUMBER)
				value = get_numeric_argument<short>(L, 5, "INPUTFRAME::set_axis");
			else
				(stringfmt() << "Expected argument 5 of INPUTFRAME::set_axis to be boolean or "
					<< "number").throwex();
			setbutton(port, controller, button, value);
			return 0;
		}
		int serialize(lua_State* L)
		{
			char buf[MAX_SERIALIZED_SIZE];
			f.serialize(buf);
			lua_pushstring(L, buf);
			return 1;
		}
		int unserialize(lua_State* L)
		{
			std::string buf = get_string_argument(L, 2, "INPUTFRAME::unserialize");
			f.deserialize(buf.c_str());
			return 0;
		}
		int get_stride(lua_State* L)
		{
			lua_pushnumber(L, f.size());
			return 1;
		}
		controller_frame& get_frame()
		{
			return f;
		}
		std::string print()
		{
			char buf[MAX_SERIALIZED_SIZE];
			f.serialize(buf);
			return buf;
		}
	private:
		short getbutton(unsigned port, unsigned controller, unsigned index)
		{
			if(port > 2 || controller > 3 || (port == 0 && controller > 0))
				return 0;
			else if(port > 0) {
				unsigned ctn = (port - 1) * MAX_CONTROLLERS_PER_PORT + controller;
				return f.axis(ctn, index);
			} else if(index == 0)
				return f.sync();
			else if(index == 1)
				return f.reset();
			else if(index == 3)
				return f.delay().first;
			else if(index == 2)
				return f.delay().second;
			return 0;
		}
		void setbutton(unsigned port, unsigned controller, unsigned index, short value)
		{
			if(port > 2 || controller > 3 || (port == 0 && controller > 0))
				return;
			if(port > 0) {
				unsigned ctn = (port - 1) * MAX_CONTROLLERS_PER_PORT + controller;
				f.axis(ctn, index, value);
			} else if(index == 0) {
				f.sync(value);
			} else if(index == 1) {
				f.reset(value);
			} else if(index == 3) {
				auto d = f.delay();
				d.first = value;
				f.delay(d);
			} else if(index == 2) {
				auto d = f.delay();
				d.second = value;
				f.delay(d);
			}
		}
		controller_frame f;
	};

	int32_t get_pc_for(unsigned port, unsigned controller, unsigned button, bool extra0 = false);
	int32_t get_pc_for(unsigned port, unsigned controller, unsigned button, bool extra0)
	{
		movie& m = movb.get_movie();
		if(port == 0) {
			//This is special.
			if(controller == 0 && button == 0)
				return m.get_pollcounters().max_polls() + (extra0 ? 1 : 0);
			else if(controller == 0 && button >= 1 && button <= 3)
				return m.get_pollcounters().get_system() ? 1 : 0;
		} else if(port <= 2) {
			if(controller < MAX_CONTROLLERS_PER_PORT && button < MAX_CONTROLS_PER_CONTROLLER)
				return m.get_pollcounters().get_polls((port - 1) * MAX_CONTROLLERS_PER_PORT +
					controller, button);
		}
		return -1;
	}

	void check_can_edit(unsigned port, unsigned controller, unsigned button, uint64_t frame,
		bool allow_past_end = false);

	void check_can_edit(unsigned port, unsigned controller, unsigned button, uint64_t frame,
		bool allow_past_end)
	{
		movie& m = movb.get_movie();
		if(!m.readonly_mode())
			throw std::runtime_error("Not in read-only mode");
		if(!allow_past_end && frame >= movb.get_movie().get_frame_vector().size())
			throw std::runtime_error("Index out of movie");
		int32_t pc = get_pc_for(port, controller, button, true);
		if(pc < 0)
			throw std::runtime_error("Invalid control to edit");
		uint64_t firstframe = m.get_current_frame_first_subframe();
		uint64_t minframe = firstframe + pc;
		uint64_t msize = movb.get_movie().get_frame_vector().size();
		if(minframe > msize || firstframe >= msize)
			throw std::runtime_error("Can not edit finished movie");
		if(frame < minframe)
			throw std::runtime_error("Can not edit past");
	}

	function_ptr_luafun movie_cfs("movie.current_first_subframe", [](lua_State* L, const std::string& fname)
		-> int {
		movie& m = movb.get_movie();
		lua_pushnumber(L, m.get_current_frame_first_subframe());
		return 1;
	});

	function_ptr_luafun movie_pc("movie.pollcounter", [](lua_State* L, const std::string& fname) -> int {
		unsigned port = get_numeric_argument<unsigned>(L, 1, fname.c_str());
		unsigned controller = get_numeric_argument<unsigned>(L, 2, fname.c_str());
		unsigned button = get_numeric_argument<unsigned>(L, 3, fname.c_str());
		uint32_t ret = 0;
		ret = get_pc_for(port, controller, button);
		lua_pushnumber(L, ret);
		return 1;
	});

	controller_frame_vector& framevector(lua_State* L, int& ptr, const std::string& fname);

	int _copy_movie(lua_State* L, const std::string& fname)
	{
		int ptr = 1;
		controller_frame_vector& v = framevector(L, ptr, fname);

		lua_inputmovie* m = lua_class<lua_inputmovie>::create(L, L, v);
		return 1;
	}

	int _get_frame(lua_State* L, const std::string& fname)
	{
		int ptr = 1;
		controller_frame_vector& v = framevector(L, ptr, fname);

		uint64_t n = get_numeric_argument<uint64_t>(L, ptr++, fname.c_str());
		if(n >= v.size())
			throw std::runtime_error("Requested frame outside movie");
		controller_frame _f = v[n];
		lua_inputframe* f = lua_class<lua_inputframe>::create(L, L, _f);
		return 1;
	}

	int _set_frame(lua_State* L, const std::string& fname)
	{
		int ptr = 1;
		controller_frame_vector& v = framevector(L, ptr, fname);

		uint64_t n = get_numeric_argument<uint64_t>(L, ptr++, fname.c_str());
		if(n >= v.size())
			throw std::runtime_error("Requested frame outside movie");
		//Checks if requested frame is from movie.
		if(&v == &movb.get_movie().get_frame_vector())
			check_can_edit(0, 0, 0, n);

		lua_inputframe* f = lua_class<lua_inputframe>::get(L, ptr++, fname.c_str());
		int64_t adjust = 0;
		if(v[n].sync()) adjust--;
		v[n] = f->get_frame();
		if(v[n].sync()) adjust++;

		if(&v == &movb.get_movie().get_frame_vector()) {
			movb.get_movie().adjust_frame_count(adjust);
			update_movie_state();
			platform::notify_status();
		}
		return 0;
	}

	int _get_size(lua_State* L, const std::string& fname)
	{
		int ptr = 1;
		controller_frame_vector& v = framevector(L, ptr, fname);

		lua_pushnumber(L, v.size());
		return 1;
	}

	int _count_frames(lua_State* L, const std::string& fname)
	{
		int ptr = 1;
		controller_frame_vector& v = framevector(L, ptr, fname);

		uint64_t c = 0;
		uint64_t s = v.size();
		for(uint64_t i = 0; i < s; i++)
			if(v[i].sync())
				c++;
		lua_pushnumber(L, c);
		return 1;
	}

	int _find_frame(lua_State* L, const std::string& fname)
	{
		int ptr = 1;
		controller_frame_vector& v = framevector(L, ptr, fname);

		uint64_t n = get_numeric_argument<uint64_t>(L, ptr++, fname.c_str());
		if(!n) {
			lua_pushnumber(L, -1);
			return 1;
		}
		uint64_t c = 0;
		uint64_t s = v.size();
		for(uint64_t i = 0; i < s; i++)
			if(v[i].sync() && ++c == n) {
				lua_pushnumber(L, i);
				return 1;
			}
		lua_pushnumber(L, -1);
		return 1;
	}

	int _blank_frame(lua_State* L, const std::string& fname)
	{
		int ptr = 1;
		controller_frame_vector& v = framevector(L, ptr, fname);

		controller_frame _f = v.blank_frame(true);
		lua_inputframe* f = lua_class<lua_inputframe>::create(L, L, _f);
		return 1;
	}

	int _append_frames(lua_State* L, const std::string& fname)
	{
		int ptr = 1;
		controller_frame_vector& v = framevector(L, ptr, fname);

		uint64_t count = get_numeric_argument<uint64_t>(L, ptr++, "lua_inputmovie::append_frames");
		for(uint64_t i = 0; i < count; i++)
			v.append(v.blank_frame(true));
		if(&v == &movb.get_movie().get_frame_vector()) {
			movb.get_movie().adjust_frame_count(count);
			update_movie_state();
			platform::notify_status();
		}
		return 0;
	}

	int _append_frame(lua_State* L, const std::string& fname)
	{
		int ptr = 1;
		controller_frame_vector& v = framevector(L, ptr, fname);

		lua_inputframe* f = lua_class<lua_inputframe>::get(L, ptr++, fname.c_str());

		v.append(v.blank_frame(true));
		if(&v == &movb.get_movie().get_frame_vector()) {
			movb.get_movie().adjust_frame_count(1);
			update_movie_state();
			platform::notify_status();
			check_can_edit(0, 0, 0, v.size() - 1);
		}
		v[v.size() - 1] = f->get_frame();
		if(&v == &movb.get_movie().get_frame_vector()) {
			if(!v[v.size() - 1].sync()) {
				movb.get_movie().adjust_frame_count(-1);
				update_movie_state();
			}
			platform::notify_status();
		}
		return 0;
	}

	int _truncate(lua_State* L, const std::string& fname)
	{
		int ptr = 1;
		controller_frame_vector& v = framevector(L, ptr, fname);

		uint64_t n = get_numeric_argument<uint64_t>(L, ptr++, fname.c_str());
		if(n > v.size())
			throw std::runtime_error("Requested truncate length longer than existing");
		if(&v == &movb.get_movie().get_frame_vector())
			check_can_edit(0, 0, 0, n);
		v.resize(n);
		if(&v == &movb.get_movie().get_frame_vector()) {
			movb.get_movie().recount_frames();
			update_movie_state();
			platform::notify_status();
		}
		return 0;
	}

	int _edit(lua_State* L, const std::string& fname)
	{
		int ptr = 1;
		controller_frame_vector& v = framevector(L, ptr, fname);

		uint64_t frame = get_numeric_argument<uint64_t>(L, ptr++, fname.c_str());
		unsigned port = get_numeric_argument<unsigned>(L, ptr++, fname.c_str());
		unsigned controller = get_numeric_argument<unsigned>(L, ptr++, fname.c_str());
		unsigned button = get_numeric_argument<unsigned>(L, ptr++, fname.c_str());
		short value;
		int64_t schange = 0;
		if(lua_type(L, ptr) == LUA_TBOOLEAN)
			value = lua_toboolean(L, ptr);
		else if(lua_type(L, ptr) == LUA_TNUMBER)
			value = get_numeric_argument<short>(L, ptr++, fname.c_str());
		else
			(stringfmt() << "Expected argument " << (ptr - 1) << " of " << fname << " to be boolean or "
				<< "number").throwex();
		movie& m = movb.get_movie();
		if(&v == &movb.get_movie().get_frame_vector())
			check_can_edit(port, controller, button, frame);
		if(port == 0) {
			if(controller == 0 && button == 0) {
				bool osync = v[frame].sync();
				v[frame].sync(value);
				if(osync && !value)
					schange = -1;
				if(!osync && value)
					schange = 1;
			} else if(controller == 0 && button == 1) {
				v[frame].reset(value);
			} else if(controller == 0 && button == 2) {
				auto d = v[frame].delay();
				d.second = value;
				v[frame].delay(d);
			} else if(controller == 0 && button == 3) {
				auto d = v[frame].delay();
				d.first = value;
				v[frame].delay(d);
			}
		} else
			v[frame].axis((port - 1) * MAX_CONTROLLERS_PER_PORT + controller, button,
				value);
		if(&v == &movb.get_movie().get_frame_vector()) {
			if(schange) {
				movb.get_movie().adjust_frame_count(schange);
				update_movie_state();
			}
			platform::notify_status();
		}
		return 0;
	}

	template<bool same>
	int _copy_frames(lua_State* L, const std::string& fname)
	{
		int ptr = 1;
		controller_frame_vector& dstv = framevector(L, ptr, fname);
		uint64_t dst = get_numeric_argument<uint64_t>(L, ptr++, fname.c_str());
		controller_frame_vector& srcv = same ? dstv : framevector(L, ptr, fname);
		uint64_t src = get_numeric_argument<uint64_t>(L, ptr++, fname.c_str());
		uint64_t count = get_numeric_argument<uint64_t>(L, ptr++, fname.c_str());
		int64_t schange = 0;
		bool backwards = same ? get_boolean_argument(L, ptr++, fname.c_str()) : same;

		if(src >= srcv.size() || src + count < src)
			throw std::runtime_error("Source index out of movie");
		if(dst > dstv.size() || dst + count < dst)
			throw std::runtime_error("Destination index out of movie");

		movie& m = movb.get_movie();
		if(&dstv == &movb.get_movie().get_frame_vector())
			check_can_edit(0, 0, 0, dst, true);

		//Add enough blank frames to make the copy.
		while(dst + count > dstv.size())
			dstv.append(dstv.blank_frame(false));

		for(uint64_t i = backwards ? (count - 1) : 0; i < count; i = backwards ? (i - 1) : (i + 1)) {
			if(dstv[dst + i].sync()) schange--;
			dstv[dst + i] = srcv[src + i];
			if(dstv[dst + i].sync()) schange++;
		}
		if(&dstv == &movb.get_movie().get_frame_vector()) {
			if(schange) {
				movb.get_movie().adjust_frame_count(schange);
				update_movie_state();
			}
			platform::notify_status();
		}
		return 0;
	}

	int _serialize(lua_State* L, const std::string& fname)
	{
		int ptr = 1;
		controller_frame_vector& v = framevector(L, ptr, fname);
		std::string filename = get_string_argument(L, ptr++, fname.c_str());
		bool binary = get_boolean_argument(L, ptr++, fname.c_str());
		std::ofstream file(filename, binary ? std::ios_base::binary : std::ios_base::out);
		if(!file)
			throw std::runtime_error("Can't open file to write output to");
		if(binary) {
			uint64_t pages = v.get_page_count();
			uint64_t stride = v.get_stride();
			uint64_t pageframes = v.get_frames_per_page();
			uint64_t vsize = v.size();
			size_t pagenum = 0;
			while(vsize > 0) {
				uint64_t count = (vsize > pageframes) ? pageframes : vsize;
				size_t bytes = count * stride;
				unsigned char* content = v.get_page_buffer(pagenum++);
				file.write(reinterpret_cast<char*>(content), bytes);
				vsize -= count;
			}
		} else {
			char buf[MAX_SERIALIZED_SIZE];
			for(uint64_t i = 0; i < v.size(); i++) {
				v[i].serialize(buf);
				file << buf << std::endl;
			}
		}
		return 0;
	}

	class lua_inputmovie
	{
	public:
		lua_inputmovie(lua_State* L, const controller_frame_vector& _v);
		lua_inputmovie(lua_State* L, controller_frame& _f);
		int copy_movie(lua_State* L)
		{
			return _copy_movie(L, "INPUTMOVIE::copy_movie");
		}
		int get_frame(lua_State* L)
		{
			return _get_frame(L, "INPUTMOVIE::get_frame");
		}
		int set_frame(lua_State* L)
		{
			return _set_frame(L, "INPUTMOVIE::set_frame");
		}
		int get_size(lua_State* L)
		{
			return _get_size(L, "INPUTMOVIE::get_size");
		}
		int count_frames(lua_State* L)
		{
			return _count_frames(L, "INPUTMOVIE::count_frames");
		}
		int find_frame(lua_State* L)
		{
			return _find_frame(L, "INPUTMOVIE::find_frame");
		}
		int blank_frame(lua_State* L)
		{
			return _blank_frame(L, "INPUTMOVIE::blank_frame");
		}
		int append_frames(lua_State* L)
		{
			return _append_frames(L, "INPUTMOVIE::append_frames");
		}
		int append_frame(lua_State* L)
		{
			return _append_frame(L, "INPUTMOVIE::append_frame");
		}
		int truncate(lua_State* L)
		{
			return _truncate(L, "INPUTMOVIE::truncate");
		}
		int edit(lua_State* L)
		{
			return _edit(L, "INPUTMOVIE::edit");
		}
		int copy_frames(lua_State* L)
		{
			return _copy_frames<true>(L, "INPUTMOVIE::copy_frames");
		}
		int serialize(lua_State* L)
		{
			return _serialize(L, "INPUTMOVIE::serialize");
		}
		int debugdump(lua_State* L)
		{
			char buf[MAX_SERIALIZED_SIZE];
			for(uint64_t i = 0; i < v.size(); i++) {
				v[i].serialize(buf);
				messages << buf << std::endl;
			}
			return 0;
		}
		controller_frame_vector* get_frame_vector()
		{
			return &v;
		}
		std::string print()
		{
			size_t s = v.size();
			return (stringfmt() << s << " " << ((s != 1) ? "frames" : "frame")).str();
		}
	private:
		void common_init(lua_State* L);
		controller_frame_vector v;
	};

	function_ptr_luafun movie_getdata("movie.copy_movie", [](lua_State* L, const std::string& fname) -> int {
		return _copy_movie(L, fname);
	});

	function_ptr_luafun movie_getframe("movie.get_frame", [](lua_State* L, const std::string& fname) -> int {
		return _get_frame(L, fname);
	});

	function_ptr_luafun movie_setframe("movie.set_frame", [](lua_State* L, const std::string& fname) -> int {
		return _set_frame(L, fname);
	});

	function_ptr_luafun movie_get_size("movie.get_size", [](lua_State* L, const std::string& fname) -> int {
		return _get_size(L, fname);
	});

	function_ptr_luafun movie_count_frames("movie.count_frames", [](lua_State* L, const std::string& fname)
		-> int {
		return _count_frames(L, fname);
	});

	function_ptr_luafun movie_find_frame("movie.find_frame", [](lua_State* L, const std::string& fname)
		-> int {
		return _find_frame(L, fname);
	});

	function_ptr_luafun movie_blank_frame("movie.blank_frame", [](lua_State* L, const std::string& fname) -> int {
		return _blank_frame(L, fname);
	});

	function_ptr_luafun movie_append_frames("movie.append_frames", [](lua_State* L, const std::string& fname)
		-> int {
		return _append_frames(L, fname);
	});

	function_ptr_luafun movie_append_frame("movie.append_frame", [](lua_State* L, const std::string& fname)
		-> int {
		return _append_frame(L, fname);
	});

	function_ptr_luafun movie_truncate("movie.truncate", [](lua_State* L, const std::string& fname)
		-> int {
		return _truncate(L, fname);
	});

	function_ptr_luafun movie_edit("movie.edit", [](lua_State* L, const std::string& fname) -> int {
		return _edit(L, fname);
	});

	function_ptr_luafun movie_copyframe2("movie.copy_frames2", [](lua_State* L, const std::string& fname) -> int {
		return _copy_frames<false>(L, fname);
	});

	function_ptr_luafun movie_copyframe("movie.copy_frames", [](lua_State* L, const std::string& fname) -> int {
		return _copy_frames<true>(L, fname);
	});

	function_ptr_luafun movie_serialize("movie.serialize", [](lua_State* L, const std::string& fname) -> int {
		return _serialize(L, fname);
	});

	function_ptr_luafun movie_unserialize("movie.unserialize", [](lua_State* L, const std::string& fname) -> int {
		lua_inputframe* f = lua_class<lua_inputframe>::get(L, 1, fname.c_str());
		std::string filename = get_string_argument(L, 2, fname.c_str());
		bool binary = get_boolean_argument(L, 3, fname.c_str());
		std::ifstream file(filename, binary ? std::ios_base::binary : std::ios_base::in);
		if(!file)
			throw std::runtime_error("Can't open file to read input from");
		lua_inputmovie* m = lua_class<lua_inputmovie>::create(L, L, f->get_frame());
		controller_frame_vector& v = *m->get_frame_vector();
		if(binary) {
			uint64_t stride = v.get_stride();
			uint64_t pageframes = v.get_frames_per_page();
			uint64_t vsize = 0;
			size_t pagenum = 0;
			size_t pagesize = stride * pageframes;
			while(file) {
				v.resize(vsize + pageframes);
				unsigned char* contents = v.get_page_buffer(pagenum++);
				file.read(reinterpret_cast<char*>(contents), pagesize);
				vsize += (file.gcount() / stride);
			}
			v.resize(vsize);
		} else {
			std::string line;
			controller_frame tmpl = v.blank_frame(false);
			while(file) {
				std::getline(file, line);
				istrip_CR(line);
				if(line.length() == 0)
					continue;
				tmpl.deserialize(line.c_str());
				v.append(tmpl);
			}
		}
		return 1;
	});


	controller_frame_vector& framevector(lua_State* L, int& ptr, const std::string& fname)
	{
		if(lua_type(L, ptr) == LUA_TNIL) { //NONE can be handled as else case.
			ptr++;
			return movb.get_movie().get_frame_vector();
		} else if(lua_class<lua_inputmovie>::is(L, ptr))
			return *lua_class<lua_inputmovie>::get(L, ptr++, fname.c_str())->get_frame_vector();
		else
			return movb.get_movie().get_frame_vector();
	}
}

DECLARE_LUACLASS(lua_inputmovie, "INPUTMOVIE");
DECLARE_LUACLASS(lua_inputframe, "INPUTFRAME");

namespace
{
	lua_inputframe::lua_inputframe(lua_State* L, controller_frame _f)
	{
		f = _f;
		static char done_key;
		if(lua_do_once(L, &done_key)) {
			objclass<lua_inputframe>().bind(L, "get_button", &lua_inputframe::get_button);
			objclass<lua_inputframe>().bind(L, "get_axis", &lua_inputframe::get_axis);
			objclass<lua_inputframe>().bind(L, "set_axis", &lua_inputframe::set_axis);
			objclass<lua_inputframe>().bind(L, "set_button", &lua_inputframe::set_axis);
			objclass<lua_inputframe>().bind(L, "serialize", &lua_inputframe::serialize);
			objclass<lua_inputframe>().bind(L, "unserialize", &lua_inputframe::unserialize);
			objclass<lua_inputframe>().bind(L, "get_stride", &lua_inputframe::get_stride);
		}
	}

	void lua_inputmovie::common_init(lua_State* L)
	{
		static char done_key;
		if(lua_do_once(L, &done_key)) {
			objclass<lua_inputmovie>().bind(L, "copy_movie", &lua_inputmovie::copy_movie);
			objclass<lua_inputmovie>().bind(L, "get_frame", &lua_inputmovie::get_frame);
			objclass<lua_inputmovie>().bind(L, "set_frame", &lua_inputmovie::set_frame);
			objclass<lua_inputmovie>().bind(L, "get_size", &lua_inputmovie::get_size);
			objclass<lua_inputmovie>().bind(L, "count_frames", &lua_inputmovie::count_frames);
			objclass<lua_inputmovie>().bind(L, "find_frame", &lua_inputmovie::find_frame);
			objclass<lua_inputmovie>().bind(L, "blank_frame", &lua_inputmovie::blank_frame);
			objclass<lua_inputmovie>().bind(L, "append_frames", &lua_inputmovie::append_frames);
			objclass<lua_inputmovie>().bind(L, "append_frame", &lua_inputmovie::append_frame);
			objclass<lua_inputmovie>().bind(L, "truncate", &lua_inputmovie::truncate);
			objclass<lua_inputmovie>().bind(L, "edit", &lua_inputmovie::edit);
			objclass<lua_inputmovie>().bind(L, "debugdump", &lua_inputmovie::debugdump);
			objclass<lua_inputmovie>().bind(L, "copy_frames", &lua_inputmovie::copy_frames);
			objclass<lua_inputmovie>().bind(L, "serialize", &lua_inputmovie::serialize);
		}
	}
	
	lua_inputmovie::lua_inputmovie(lua_State* L, const controller_frame_vector& _v)
	{
		v = _v;
		common_init(L);
	}

	lua_inputmovie::lua_inputmovie(lua_State* L, controller_frame& f)
	{
		v.clear(f.get_port_type(0), f.get_port_type(1));
		common_init(L);
	}
}
