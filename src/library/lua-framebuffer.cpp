#include "lua-base.hpp"
#include "framebuffer.hpp"
#include "lua-framebuffer.hpp"

framebuffer::color lua_get_fb_color(lua::state& L, int index, const std::string& fname) throw(std::bad_alloc,
	std::runtime_error)
{
	if(L.type(index) == LUA_TSTRING)
		return framebuffer::color(L.get_string(index, fname.c_str()));
	else if(L.type(index) == LUA_TNUMBER)
		return framebuffer::color(L.get_numeric_argument<int64_t>(index, fname.c_str()));
	else
		(stringfmt() << "Expected argument #" << index << " to " << fname
			<< " be string or number").throwex();
}

framebuffer::color lua_get_fb_color(lua::state& L, int index, const std::string& fname, int64_t dflt)
	throw(std::bad_alloc, std::runtime_error)
{
	if(L.type(index) == LUA_TSTRING)
		return framebuffer::color(L.get_string(index, fname.c_str()));
	else if(L.type(index) == LUA_TNUMBER)
		return framebuffer::color(L.get_numeric_argument<int64_t>(index, fname.c_str()));
	else if(L.type(index) == LUA_TNIL || L.type(index) == LUA_TNONE)
		return framebuffer::color(dflt);
	else
		(stringfmt() << "Expected argument #" << index << " to " << fname
			<< " be string, number or nil").throwex();
}
