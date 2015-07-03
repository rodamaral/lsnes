#include "lua/internal.hpp"
#include "library/string.hpp"
#include "library/minmax.hpp"
#include "core/dispatch.hpp"
#include "core/instance.hpp"
#include "core/moviedata.hpp"
#include "core/messages.hpp"
#include "core/window.hpp"
#include <fstream>


namespace
{
	class lua_inputmovie;

	class lua_inputframe
	{
		friend class lua_inputmovie;
	public:
		lua_inputframe(lua::state& L, portctrl::frame _f);
		static size_t overcommit(portctrl::frame _f) { return 0; }
		int get_button(lua::state& L, lua::parameters& P)
		{
			unsigned port, controller, button;

			P(P.skipped(), port, controller, button);

			short value = getbutton(port, controller, button);
			L.pushboolean(value ? 1 : 0);
			return 1;
		}
		int get_axis(lua::state& L, lua::parameters& P)
		{
			unsigned port, controller, button;

			P(P.skipped(), port, controller, button);

			short value = getbutton(port, controller, button);
			L.pushnumber(value);
			return 1;
		}
		int set_axis(lua::state& L, lua::parameters& P)
		{
			unsigned port, controller, button;
			short value;

			P(P.skipped(), port, controller, button);
			if(P.is_boolean()) value = P.arg<bool>() ? 1 : 0;
			else if(P.is_number()) value = P.arg<short>();
			else
				P.expected("number or boolean");

			setbutton(port, controller, button, value);
			return 0;
		}
		int serialize(lua::state& L, lua::parameters& P)
		{
			char buf[MAX_SERIALIZED_SIZE];
			f.serialize(buf);
			L.pushstring(buf);
			return 1;
		}
		int unserialize(lua::state& L, lua::parameters& P)
		{
			std::string buf;

			P(P.skipped(), buf);

			f.deserialize(buf.c_str());
			return 0;
		}
		int get_stride(lua::state& L, lua::parameters& P)
		{
			L.pushnumber(f.size());
			return 1;
		}
		portctrl::frame& get_frame()
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
			return f.axis3(port, controller, index);
		}
		void setbutton(unsigned port, unsigned controller, unsigned index, short value)
		{
			return f.axis3(port, controller, index, value);
		}
		portctrl::frame f;
	};

	int32_t get_pc_for(unsigned port, unsigned controller, unsigned button, bool extra0 = false);
	int32_t get_pc_for(unsigned port, unsigned controller, unsigned button, bool extra0)
	{
		movie& m = CORE().mlogic->get_movie();
		if(port == 0 && controller == 0 && button == 0)
			return m.get_pollcounters().max_polls() + (extra0 ? 1 : 0);
		if(port == 0 && controller == 0 && m.get_pollcounters().get_framepflag())
			return max(m.get_pollcounters().get_polls(port, controller, button), (uint32_t)1);
		return m.get_pollcounters().get_polls(port, controller, button);
	}

	void check_can_edit(unsigned port, unsigned controller, unsigned button, uint64_t frame,
		bool allow_past_end = false);

	void check_can_edit(unsigned port, unsigned controller, unsigned button, uint64_t frame,
		bool allow_past_end)
	{
		auto& core = CORE();
		movie& m = core.mlogic->get_movie();
		if(!m.readonly_mode())
			throw std::runtime_error("Not in playback mode");
		if(!allow_past_end && frame >= core.mlogic->get_mfile().input->size())
			throw std::runtime_error("Index out of movie");
		int32_t pc = get_pc_for(port, controller, button, true);
		if(pc < 0)
			throw std::runtime_error("Invalid control to edit");
		uint64_t firstframe = m.get_current_frame_first_subframe();
		uint64_t minframe = firstframe + pc;
		uint64_t msize = core.mlogic->get_mfile().input->size();
		if(minframe > msize || firstframe >= msize)
			throw std::runtime_error("Can not edit finished movie");
		if(frame < minframe)
			throw std::runtime_error("Can not edit past");
	}

	int current_first_subframe(lua::state& L, lua::parameters& P)
	{
		movie& m = CORE().mlogic->get_movie();
		L.pushnumber(m.get_current_frame_first_subframe());
		return 1;
	}

	int pollcounter(lua::state& L, lua::parameters& P)
	{
		unsigned port, controller, button;

		P(port, controller, button);

		uint32_t ret = 0;
		ret = get_pc_for(port, controller, button);
		L.pushnumber(ret);
		return 1;
	}

	portctrl::frame_vector& framevector(lua::state& L, lua::parameters& P);

	int _copy_movie(lua::state& L, lua::parameters& P)
	{
		portctrl::frame_vector& v = framevector(L, P);

		lua::_class<lua_inputmovie>::create(L, v);
		return 1;
	}

	int _get_frame(lua::state& L, lua::parameters& P)
	{
		uint64_t n;
		portctrl::frame_vector& v = framevector(L, P);

		P(n);

		if(n >= v.size())
			throw std::runtime_error("Requested frame outside movie");
		portctrl::frame _f = v[n];
		lua::_class<lua_inputframe>::create(L, _f);
		return 1;
	}

	int _set_frame(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		uint64_t n;
		lua_inputframe* f;
		portctrl::frame_vector& v = framevector(L, P);

		P(n, f);

		if(n >= v.size())
			throw std::runtime_error("Requested frame outside movie");
		//Checks if requested frame is from movie.
		if(&v == core.mlogic->get_mfile().input)
			check_can_edit(0, 0, 0, n);

		v[n] = f->get_frame();

		if(&v == core.mlogic->get_mfile().input) {
			//This can't add frames, so no need to adjust the movie.
			core.supdater->update();
			core.dispatch->status_update();
		}
		return 0;
	}

	int _get_size(lua::state& L, lua::parameters& P)
	{
		portctrl::frame_vector& v = framevector(L, P);

		L.pushnumber(v.size());
		return 1;
	}

	int _count_frames(lua::state& L, lua::parameters& P)
	{
		portctrl::frame_vector& v = framevector(L, P);

		L.pushnumber(v.count_frames());
		return 1;
	}

	int _find_frame(lua::state& L, lua::parameters& P)
	{
		uint64_t n;
		portctrl::frame_vector& v = framevector(L, P);

		P(n);

		L.pushnumber(v.find_frame(n));
		return 1;
	}

	int _subframe_to_frame(lua::state& L, lua::parameters& P)
	{
		uint64_t n;
		portctrl::frame_vector& v = framevector(L, P);

		P(n);

		L.pushnumber(v.subframe_to_frame(n));
		return 1;
	}

	int _blank_frame(lua::state& L, lua::parameters& P)
	{
		portctrl::frame_vector& v = framevector(L, P);

		portctrl::frame _f = v.blank_frame(true);
		lua::_class<lua_inputframe>::create(L, _f);
		return 1;
	}

	int _append_frames(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		uint64_t count;
		portctrl::frame_vector& v = framevector(L, P);

		P(count);

		{
			portctrl::frame_vector::notify_freeze freeze(v);
			for(uint64_t i = 0; i < count; i++)
				v.append(v.blank_frame(true));
		}
		if(&v == core.mlogic->get_mfile().input) {
			core.supdater->update();
			core.dispatch->status_update();
		}
		return 0;
	}

	int _append_frame(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		lua_inputframe* f;
		portctrl::frame_vector& v = framevector(L, P);

		P(f);

		v.append(v.blank_frame(true));
		if(&v == core.mlogic->get_mfile().input) {
			core.supdater->update();
			core.dispatch->status_update();
			check_can_edit(0, 0, 0, v.size() - 1);
		}
		v[v.size() - 1] = f->get_frame();
		if(&v == core.mlogic->get_mfile().input) {
			if(!v[v.size() - 1].sync()) {
				core.supdater->update();
			}
			core.dispatch->status_update();
		}
		return 0;
	}

	int _truncate(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		uint64_t n;
		portctrl::frame_vector& v = framevector(L, P);

		P(n);

		if(n > v.size())
			throw std::runtime_error("Requested truncate length longer than existing");
		if(&v == core.mlogic->get_mfile().input)
			check_can_edit(0, 0, 0, n);
		v.resize(n);
		if(&v == core.mlogic->get_mfile().input) {
			core.supdater->update();
			core.dispatch->status_update();
		}
		return 0;
	}

	int _edit(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		uint64_t frame;
		unsigned port, controller, button;
		short value;
		portctrl::frame_vector& v = framevector(L, P);

		P(frame, port, controller, button);
		if(P.is_boolean()) value = P.arg<bool>() ? 1 : 0;
		else if(P.is_number()) P(value);
		else
			P.expected("number or boolean");

		if(&v == core.mlogic->get_mfile().input)
			check_can_edit(port, controller, button, frame);
		v[frame].axis3(port, controller, button, value);

		if(&v == core.mlogic->get_mfile().input) {
			core.supdater->update();
			core.dispatch->status_update();
		}
		return 0;
	}

	template<bool same>
	int _copy_frames(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		uint64_t dst, src, count;
		bool backwards = same;

		portctrl::frame_vector& dstv = framevector(L, P);
		P(dst);
		portctrl::frame_vector& srcv = same ? dstv : framevector(L, P);
		P(src, count);
		if(same) P(backwards);

		if(src >= srcv.size() || src + count < src)
			throw std::runtime_error("Source index out of movie");
		if(dst > dstv.size() || dst + count < dst)
			throw std::runtime_error("Destination index out of movie");

		if(&dstv == core.mlogic->get_mfile().input)
			check_can_edit(0, 0, 0, dst, true);

		{
			portctrl::frame_vector::notify_freeze freeze(dstv);
			//Add enough blank frames to make the copy.
			while(dst + count > dstv.size())
				dstv.append(dstv.blank_frame(false));

			for(uint64_t i = backwards ? (count - 1) : 0; i < count; i = backwards ? (i - 1) : (i + 1))
				dstv[dst + i] = srcv[src + i];
		}
		if(&dstv == core.mlogic->get_mfile().input) {
			core.supdater->update();
			core.dispatch->status_update();
		}
		return 0;
	}

	int _serialize(lua::state& L, lua::parameters& P)
	{
		std::string filename;
		bool binary;

		portctrl::frame_vector& v = framevector(L, P);

		P(filename, binary);

		std::ofstream file(filename, binary ? std::ios_base::binary : std::ios_base::out);
		if(!file)
			throw std::runtime_error("Can't open file to write output to");
		if(binary) {
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
		lua_inputmovie(lua::state& L, const portctrl::frame_vector& _v);
		lua_inputmovie(lua::state& L, portctrl::frame& _f);
		static size_t overcommit(const portctrl::frame_vector& _v) { return 0; }
		static size_t overcommit(portctrl::frame& _f) { return 0; }
		int copy_movie(lua::state& L, lua::parameters& P)
		{
			return _copy_movie(L, P);
		}
		int get_frame(lua::state& L, lua::parameters& P)
		{
			return _get_frame(L, P);
		}
		int set_frame(lua::state& L, lua::parameters& P)
		{
			return _set_frame(L, P);
		}
		int get_size(lua::state& L, lua::parameters& P)
		{
			return _get_size(L, P);
		}
		int count_frames(lua::state& L, lua::parameters& P)
		{
			return _count_frames(L, P);
		}
		int find_frame(lua::state& L, lua::parameters& P)
		{
			return _find_frame(L, P);
		}
		int subframe_to_frame(lua::state& L, lua::parameters& P)
		{
			return _subframe_to_frame(L, P);
		}
		int blank_frame(lua::state& L, lua::parameters& P)
		{
			return _blank_frame(L, P);
		}
		int append_frames(lua::state& L, lua::parameters& P)
		{
			return _append_frames(L, P);
		}
		int append_frame(lua::state& L, lua::parameters& P)
		{
			return _append_frame(L, P);
		}
		int truncate(lua::state& L, lua::parameters& P)
		{
			return _truncate(L, P);
		}
		int edit(lua::state& L, lua::parameters& P)
		{
			return _edit(L, P);
		}
		int copy_frames(lua::state& L, lua::parameters& P)
		{
			return _copy_frames<true>(L, P);
		}
		int serialize(lua::state& L, lua::parameters& P)
		{
			return _serialize(L, P);
		}
		int debugdump(lua::state& L, lua::parameters& P)
		{
			char buf[MAX_SERIALIZED_SIZE];
			for(uint64_t i = 0; i < v.size(); i++) {
				v[i].serialize(buf);
				messages << buf << std::endl;
			}
			return 0;
		}
		portctrl::frame_vector* get_frame_vector()
		{
			return &v;
		}
		std::string print()
		{
			size_t s = v.size();
			return (stringfmt() << s << " " << ((s != 1) ? "frames" : "frame")).str();
		}
	private:
		void common_init(lua::state& L);
		portctrl::frame_vector v;
	};

	int copy_movie(lua::state& L, lua::parameters& P)
	{
		return _copy_movie(L, P);
	}

	int get_frame(lua::state& L, lua::parameters& P)
	{
		return _get_frame(L, P);
	}

	int set_frame(lua::state& L, lua::parameters& P)
	{
		return _set_frame(L, P);
	}

	int get_size(lua::state& L, lua::parameters& P)
	{
		return _get_size(L, P);
	}

	int count_frames(lua::state& L, lua::parameters& P)
	{
		return _count_frames(L, P);
	}

	int find_frame(lua::state& L, lua::parameters& P)
	{
		return _find_frame(L, P);
	}

	int subframe_to_frame(lua::state& L, lua::parameters& P)
	{
		return _subframe_to_frame(L, P);
	}

	int blank_frame(lua::state& L, lua::parameters& P)
	{
		return _blank_frame(L, P);
	}

	int append_frames(lua::state& L, lua::parameters& P)
	{
		return _append_frames(L, P);
	}

	int append_frame(lua::state& L, lua::parameters& P)
	{
		return _append_frame(L, P);
	}

	int truncate(lua::state& L, lua::parameters& P)
	{
		return _truncate(L, P);
	}

	int edit(lua::state& L, lua::parameters& P)
	{
		return _edit(L, P);
	}

	int copy_frames2(lua::state& L, lua::parameters& P)
	{
		return _copy_frames<false>(L, P);
	}

	int copy_frames(lua::state& L, lua::parameters& P)
	{
		return _copy_frames<true>(L, P);
	}

	int serialize(lua::state& L, lua::parameters& P)
	{
		return _serialize(L, P);
	}

	int unserialize(lua::state& L, lua::parameters& P)
	{
		lua_inputframe* f;
		std::string filename;
		bool binary;

		P(f, filename, binary);

		std::ifstream file(filename, binary ? std::ios_base::binary : std::ios_base::in);
		if(!file)
			throw std::runtime_error("Can't open file to read input from");
		lua_inputmovie* m = lua::_class<lua_inputmovie>::create(L, f->get_frame());
		portctrl::frame_vector& v = *m->get_frame_vector();
		portctrl::frame_vector::notify_freeze freeze(v);
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
			portctrl::frame tmpl = v.blank_frame(false);
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
	}

	int current_branch(lua::state& L, lua::parameters& P)
	{
		L.pushlstring(CORE().mlogic->get_mfile().current_branch());
		return 1;
	}

	int get_branches(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		for(auto& i : core.mlogic->get_mfile().branches)
			L.pushlstring(i.first);
		return core.mlogic->get_mfile().branches.size();
	}

	portctrl::frame_vector& framevector(lua::state& L, lua::parameters& P)
	{
		auto& core = CORE();
		if(P.is_nil()) {
			P.skip();
			return *core.mlogic->get_mfile().input;
		} else if(P.is_string()) {
			std::string x;
			P(x);
			if(!core.mlogic->get_mfile().branches.count(x))
				throw std::runtime_error("No such branch");
			return core.mlogic->get_mfile().branches[x];
		} else if(P.is<lua_inputmovie>())
			return *(P.arg<lua_inputmovie*>()->get_frame_vector());
		else
			return *core.mlogic->get_mfile().input;
	}

	lua::_class<lua_inputmovie> LUA_class_inputmovie(lua_class_movie, "INPUTMOVIE", {}, {
			{"copy_movie", &lua_inputmovie::copy_movie},
			{"get_frame", &lua_inputmovie::get_frame},
			{"set_frame", &lua_inputmovie::set_frame},
			{"get_size", &lua_inputmovie::get_size},
			{"count_frames", &lua_inputmovie::count_frames},
			{"find_frame", &lua_inputmovie::find_frame},
			{"subframe_to_frame", &lua_inputmovie::subframe_to_frame},
			{"blank_frame", &lua_inputmovie::blank_frame},
			{"append_frames", &lua_inputmovie::append_frames},
			{"append_frame", &lua_inputmovie::append_frame},
			{"truncate", &lua_inputmovie::truncate},
			{"edit", &lua_inputmovie::edit},
			{"debugdump", &lua_inputmovie::debugdump},
			{"copy_frames", &lua_inputmovie::copy_frames},
			{"serialize", &lua_inputmovie::serialize},
	}, &lua_inputmovie::print);

	lua::_class<lua_inputframe> LUA_class_inputframe(lua_class_movie, "INPUTFRAME", {}, {
			{"get_button", &lua_inputframe::get_button},
			{"get_axis", &lua_inputframe::get_axis},
			{"set_axis", &lua_inputframe::set_axis},
			{"set_button", &lua_inputframe::set_axis},
			{"serialize", &lua_inputframe::serialize},
			{"unserialize", &lua_inputframe::unserialize},
			{"get_stride", &lua_inputframe::get_stride},
	}, &lua_inputframe::print);

	lua::functions LUA_inputmovie_fns(lua_func_misc, "movie", {
		{"current_first_subframe", current_first_subframe},
		{"pollcounter", pollcounter},
		{"copy_movie", copy_movie},
		{"get_frame", get_frame},
		{"set_frame", set_frame},
		{"get_size", get_size},
		{"count_frames", count_frames},
		{"find_frame", find_frame},
		{"subframe_to_frame", subframe_to_frame},
		{"blank_frame", blank_frame},
		{"append_frames", append_frames},
		{"append_frame", append_frame},
		{"truncate", truncate},
		{"edit", edit},
		{"copy_frames2", copy_frames2},
		{"copy_frames", copy_frames},
		{"serialize", serialize},
		{"unserialize", unserialize},
		{"current_branch", current_branch},
		{"get_branches", get_branches},
	});

	lua_inputframe::lua_inputframe(lua::state& L, portctrl::frame _f)
	{
		f = _f;
	}

	void lua_inputmovie::common_init(lua::state& L)
	{
	}

	lua_inputmovie::lua_inputmovie(lua::state& L, const portctrl::frame_vector& _v)
	{
		v = _v;
		common_init(L);
	}

	lua_inputmovie::lua_inputmovie(lua::state& L, portctrl::frame& f)
	{
		v.clear(f.porttypes());
		common_init(L);
	}
}
