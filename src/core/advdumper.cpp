#include "core/advdumper.hpp"
#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/globalwrap.hpp"
#include "core/lua.hpp"

#include <map>
#include <string>

namespace
{
	globalwrap<std::map<std::string, adv_dumper*>> dumpers;

	adv_dumper& find_by_name(const std::string& dname)
	{
		auto i = adv_dumper::get_dumper_set();
		for(auto j : i)
			if(j->id() == dname)
				return *j;
		throw std::runtime_error("Unknown dumper");
	}

	function_ptr_command<tokensplitter&> start_dump("start-dump", "Start dumping",
		"Syntax: start-dump <dumper> <prefix/filename>\nSyntax: start-dump <dumper> <mode> <prefix/filename>\n"
			"Start dumping using <dumper> in mode <mode> to <prefix/filename>\n",
		[](tokensplitter& t) throw(std::bad_alloc, std::runtime_error) {
			adv_dumper& d = find_by_name(t);
			auto modes = d.list_submodes();
			std::string mode;
			if(!modes.empty()) {
				mode = std::string(t);
				if(!modes.count(mode))
					throw std::runtime_error("Bad mode for dumper");
			}
			std::string target = t.tail();
			if(target == "")
				throw std::runtime_error("Command syntax error");
			d.start(mode, target);
		});

	function_ptr_command<tokensplitter&> end_dump("end-dump", "End dumping",
		"Syntax: end-dump <dumper>\nEnd dumping using dumper <dumper>\n",
		[](tokensplitter& t) throw(std::bad_alloc, std::runtime_error) {
			adv_dumper& d = find_by_name(t);
			if(t.tail() != "")
				throw std::runtime_error("Command syntax error");
			d.end();
		});
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

void render_video_hud(struct screen& target, struct lcscreen& source, uint32_t hscl, uint32_t vscl,
	uint32_t roffset, uint32_t goffset, uint32_t boffset, uint32_t lgap, uint32_t tgap, uint32_t rgap,
	uint32_t bgap, void(*fn)())
{
	struct lua_render_context lrc;
	render_queue rq;
	lrc.left_gap = lgap;
	lrc.right_gap = rgap;
	lrc.bottom_gap = bgap;
	lrc.top_gap = tgap;
	lrc.queue = &rq;
	lrc.width = source.width;
	lrc.height = source.height;
	lua_callback_do_video(&lrc);
	if(fn)
		fn();
	target.set_palette(roffset, goffset, boffset);
	target.reallocate(lrc.left_gap + source.width * hscl + lrc.right_gap, lrc.top_gap +
		source.height * vscl + lrc.bottom_gap, false);
	target.set_origin(lrc.left_gap, lrc.top_gap);
	target.copy_from(source, hscl, vscl);
	rq.run(target);
}
