#include "lua-int.hpp"
#include "movie.hpp"
#include "mainloop.hpp"

namespace
{
	class lua_movie_currentframe : public lua_function
	{
	public:
		lua_movie_currentframe() : lua_function("movie.currentframe") {}
		int invoke(lua_State* LS, window* win)
		{
			auto& m = get_movie();
			lua_pushnumber(LS, m.get_current_frame());
			return 1;
		}
	} movie_currentframe;

	class lua_movie_framecount : public lua_function
	{
	public:
		lua_movie_framecount() : lua_function("movie.framecount") {}
		int invoke(lua_State* LS, window* win)
		{
			auto& m = get_movie();
			lua_pushnumber(LS, m.get_frame_count());
			return 1;
		}
	} movie_framecount;

	class lua_movie_readonly : public lua_function
	{
	public:
		lua_movie_readonly() : lua_function("movie.readonly") {}
		int invoke(lua_State* LS, window* win)
		{
			auto& m = get_movie();
			lua_pushboolean(LS, m.readonly_mode() ? 1 : 0);
			return 1;
		}
	} movie_readonly;

	class lua_movie_set_readwrite : public lua_function
	{
	public:
		lua_movie_set_readwrite() : lua_function("movie.set_readwrite") {}
		int invoke(lua_State* LS, window* win)
		{
			auto& m = get_movie();
			m.readonly_mode(false);
			return 0;
		}
	} movie_set_readwrite;

	class lua_movie_frame_subframes : public lua_function
	{
	public:
		lua_movie_frame_subframes() : lua_function("movie.frame_subframes") {}
		int invoke(lua_State* LS, window* win)
		{
			uint64_t frame = get_numeric_argument<uint64_t>(LS, 1, "movie.frame_subframes");
			auto& m = get_movie();
			lua_pushnumber(LS, m.frame_subframes(frame));
			return 1;
		}
	} movie_frame_subframes;

	class lua_movie_read_subframe : public lua_function
	{
	public:
		lua_movie_read_subframe() : lua_function("movie.read_subframe") {}
		int invoke(lua_State* LS, window* win)
		{
			uint64_t frame = get_numeric_argument<uint64_t>(LS, 1, "movie.frame_subframes");
			uint64_t subframe = get_numeric_argument<uint64_t>(LS, 2, "movie.frame_subframes");
			auto& m = get_movie();
			controls_t r = m.read_subframe(frame, subframe);
			lua_newtable(LS);
			for(size_t i = 0; i < TOTAL_CONTROLS; i++) {
				lua_pushnumber(LS, i);
				lua_pushnumber(LS, r(i));
				lua_settable(LS, -3);
			}
			return 1;
		}
	} movie_read_subframe;
}
