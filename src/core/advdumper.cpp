#include "core/advdumper.hpp"
#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "library/globalwrap.hpp"
#include "lua/lua.hpp"
#include "library/string.hpp"

#include <map>
#include <string>

namespace
{
	globalwrap<std::map<std::string, adv_dumper*>> dumpers;
}

const std::string& adv_dumper::id() throw()
{
	return d_id;
}

adv_dumper::~adv_dumper()
{
	dumpers().erase(d_id);
	information_dispatch::do_dumper_update();
}

std::set<adv_dumper*> adv_dumper::get_dumper_set() throw(std::bad_alloc)
{
	std::set<adv_dumper*> d;
	for(auto i : dumpers())
		d.insert(i.second);
	return d;
}

adv_dumper::adv_dumper(const std::string& id) throw(std::bad_alloc)
{
	d_id = id;
	dumpers()[d_id] = this;
}

unsigned adv_dumper::target_type_mask = 3;
unsigned adv_dumper::target_type_file = 0;
unsigned adv_dumper::target_type_prefix = 1;
unsigned adv_dumper::target_type_special = 2;

template<bool X> bool render_video_hud(struct framebuffer::fb<X>& target, struct framebuffer::raw& source, uint32_t hscl,
	uint32_t vscl, uint32_t lgap, uint32_t tgap, uint32_t rgap, uint32_t bgap, void(*fn)())
{
	bool lua_kill_video = false;
	struct lua_render_context lrc;
	framebuffer::queue rq;
	lrc.left_gap = lgap;
	lrc.right_gap = rgap;
	lrc.bottom_gap = bgap;
	lrc.top_gap = tgap;
	lrc.queue = &rq;
	lrc.width = source.get_width();
	lrc.height = source.get_height();
	lua_callback_do_video(&lrc, lua_kill_video, hscl, vscl);
	if(fn)
		fn();
	target.reallocate(lrc.left_gap + source.get_width() * hscl + lrc.right_gap, lrc.top_gap +
		source.get_height() * vscl + lrc.bottom_gap, false);
	target.set_origin(lrc.left_gap, lrc.top_gap);
	target.copy_from(source, hscl, vscl);
	rq.run(target);
	return !lua_kill_video;
}

uint64_t killed_audio_length(uint32_t fps_n, uint32_t fps_d, double& fraction)
{
	auto g = information_dispatch::get_sound_rate();
	double x = 1.0 * fps_d * g.first / (fps_n * g.second) + fraction;
	uint64_t y = x;
	fraction = x - y;
	return y;
}

template bool render_video_hud(struct framebuffer::fb<false>& target, struct framebuffer::raw& source, uint32_t hscl,
	uint32_t vscl, uint32_t lgap, uint32_t tgap, uint32_t rgap, uint32_t bgap, void(*fn)());
template bool render_video_hud(struct framebuffer::fb<true>& target, struct framebuffer::raw& source, uint32_t hscl,
	uint32_t vscl, uint32_t lgap, uint32_t tgap, uint32_t rgap, uint32_t bgap, void(*fn)());
