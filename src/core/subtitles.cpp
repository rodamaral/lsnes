#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/framebuffer.hpp"
#include "core/instance.hpp"
#include "core/messages.hpp"
#include "core/movie.hpp"
#include "core/moviefile.hpp"
#include "core/subtitles.hpp"
#include "fonts/wrapper.hpp"
#include "library/string.hpp"
#include "lua/lua.hpp"

#include <fstream>

moviefile_subtiming::moviefile_subtiming(uint64_t _frame)
{
	position_only = true;
	frame = _frame;
	length = 0;
}

moviefile_subtiming::moviefile_subtiming(uint64_t first, uint64_t _length)
{
	position_only = false;
	frame = first;
	length = _length;
}

bool moviefile_subtiming::operator<(const moviefile_subtiming& a) const
{
	//This goes in inverse order due to behaviour of lower_bound/upper_bound.
	if(frame > a.frame)
		return true;
	if(frame < a.frame)
		return false;
	if(position_only && a.position_only)
		return false;
	//Position only compares greater than any of same frame.
	if(position_only != a.position_only)
		return position_only;
	//Break ties on length.
	return (length > a.length);
}

bool moviefile_subtiming::operator==(const moviefile_subtiming& a) const
{
	if(frame != a.frame)
		return false;
	if(position_only && a.position_only)
		return true;
	if(position_only != a.position_only)
		return false;
	return (length != a.length);
}

bool moviefile_subtiming::inrange(uint64_t x) const
{
	if(position_only)
		return false;
	return (x >= frame && x < frame + length);
}

uint64_t moviefile_subtiming::get_frame() const { return frame; }
uint64_t moviefile_subtiming::get_length() const { return length; }

namespace
{
	std::string s_subescape(std::string x)
	{
		std::string y;
		for(size_t i = 0; i < x.length(); i++) {
			char ch = x[i];
			if(ch == '\n')
				y += "|";
			else if(ch == '|')
				y += "⎢";
			else
				y.append(1, ch);
		}
		return y;
	}

	struct render_object_subtitle : public framebuffer::object
	{
		render_object_subtitle(int32_t _x, int32_t _y, const std::string& _text) throw()
			: x(_x), y(_y), text(_text), fg(0xFFFF80), bg(-1) {}
		~render_object_subtitle() throw() {}
		template<bool X> void op(struct framebuffer::fb<X>& scr) throw()
		{
			main_font.render(scr, x, y, text, fg, bg, false, false);
		}
		void operator()(struct framebuffer::fb<true>& scr) throw()  { op(scr); }
		void operator()(struct framebuffer::fb<false>& scr) throw() { op(scr); }
		void clone(framebuffer::queue& q) const throw(std::bad_alloc) { q.clone_helper(this); }
	private:
		int32_t x;
		int32_t y;
		std::string text;
		framebuffer::color fg;
		framebuffer::color bg;
	};


	command::fnptr<const std::string&> CMD_edit_subtitle(lsnes_cmds, "edit-subtitle", "Edit a subtitle",
		"Syntax: edit-subtitle <first> <length> <text>\nAdd/Edit subtitle\n"
		"Syntax: edit-subtitle <first> <length>\nADelete subtitle\n",
		[](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
			auto& core = CORE();
			auto r = regex("([0-9]+)[ \t]+([0-9]+)([ \t]+(.*))?", args, "Bad syntax");
			uint64_t frame = parse_value<uint64_t>(r[1]);
			uint64_t length = parse_value<uint64_t>(r[2]);
			std::string text = r[4];
			moviefile_subtiming key(frame, length);
			if(text == "")
				core.mlogic->get_mfile().subtitles.erase(key);
			else
				core.mlogic->get_mfile().subtitles[key] =
					subtitle_commentary::s_unescape(text);
			core.dispatch->subtitle_change();
			core.fbuf->redraw_framebuffer();
		});

	command::fnptr<> CMD_list_subtitle(lsnes_cmds, "list-subtitle", "List the subtitles",
		"Syntax: list-subtitle\nList the subtitles.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			auto& core = CORE();
			for(auto i = core.mlogic->get_mfile().subtitles.rbegin(); i !=
				core.mlogic->get_mfile().subtitles.rend();
				i++) {
				messages << i->first.get_frame() << " " << i->first.get_length() << " "
					<< subtitle_commentary::s_escape(i->second) << std::endl;
			}
		});

	command::fnptr<command::arg_filename> CMD_save_s(lsnes_cmds, "save-subtitle", "Save subtitles in .sub format",
		"Syntax: save-subtitle <file>\nSaves subtitles in .sub format to <file>\n",
		[](command::arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			auto& core = CORE();
			if(core.mlogic->get_mfile().subtitles.empty())
				return;
			auto i = core.mlogic->get_mfile().subtitles.begin();
			uint64_t lastframe = i->first.get_frame() + i->first.get_length();
			std::ofstream y(std::string(args).c_str());
			if(!y)
				throw std::runtime_error("Can't open output file");
			std::string lasttxt = "";
			uint64_t since = 0;
			for(uint64_t i = 1; i < lastframe; i++) {
				moviefile_subtiming posmarker(i);
				auto j = core.mlogic->get_mfile().subtitles.upper_bound(posmarker);
				if(j == core.mlogic->get_mfile().subtitles.end())
					continue;
				if(lasttxt != j->second || !j->first.inrange(i)) {
					if(lasttxt != "")
						y << "{" << since << "}{" << i - 1 << "}" << s_subescape(lasttxt)
							<< std::endl;
					since = i;
					lasttxt = j->first.inrange(i) ? j->second : "";
				}
			}
			if(lasttxt != "")
				y << "{" << since << "}{" << lastframe - 1 << "}" << s_subescape(lasttxt)
					<< std::endl;
			messages << "Saved subtitles to " << std::string(args) << std::endl;
		});
}

subtitle_commentary::subtitle_commentary(movie_logic& _mlogic, emu_framebuffer& _fbuf, emulator_dispatch& _dispatch)
	: mlogic(_mlogic), fbuf(_fbuf), edispatch(_dispatch)
{
}

std::string subtitle_commentary::s_escape(std::string x)
{
	std::string y;
	for(size_t i = 0; i < x.length(); i++) {
		char ch = x[i];
		if(ch == '\n')
			y += "\\n";
		else if(ch == '\\')
			y += "\\";
		else
			y.append(1, ch);
	}
	return y;
}

std::string subtitle_commentary::s_unescape(std::string x)
{
	bool escape = false;
	std::string y;
	for(size_t i = 0; i < x.length(); i++) {
		char ch = x[i];
		if(escape) {
			if(ch == 'n')
				y.append(1, '\n');
			if(ch == '\\')
				y.append(1, '\\');
			escape = false;
		} else {
			if(ch == '\\')
				escape = true;
			else
				y.append(1, ch);
		}
	}
	return y;
}

void subtitle_commentary::render(lua::render_context& ctx)
{
	if(!mlogic || mlogic.get_mfile().subtitles.empty())
		return;
	if(ctx.bottom_gap < 32)
		ctx.bottom_gap = 32;
	uint64_t curframe = mlogic.get_movie().get_current_frame() + 1;
	moviefile_subtiming posmarker(curframe);
	auto i = mlogic.get_mfile().subtitles.upper_bound(posmarker);
	if(i != mlogic.get_mfile().subtitles.end() && i->first.inrange(curframe)) {
		std::string subtxt = i->second;
		int32_t y = ctx.height;
		ctx.queue->create_add<render_object_subtitle>(0, y, subtxt);
	}
}

std::set<std::pair<uint64_t, uint64_t>> subtitle_commentary::get_all()
{
	std::set<std::pair<uint64_t, uint64_t>> r;
	if(!mlogic)
		return r;
	for(auto i = mlogic.get_mfile().subtitles.rbegin(); i !=
		mlogic.get_mfile().subtitles.rend(); i++)
		r.insert(std::make_pair(i->first.get_frame(), i->first.get_length()));
	return r;
}

std::string subtitle_commentary::get(uint64_t f, uint64_t l)
{
	if(!mlogic)
		return "";
	moviefile_subtiming key(f, l);
	if(!mlogic.get_mfile().subtitles.count(key))
		return "";
	else
		return s_escape(mlogic.get_mfile().subtitles[key]);
}

void subtitle_commentary::set(uint64_t f, uint64_t l, const std::string& x)
{
	if(!mlogic)
		return;
	moviefile_subtiming key(f, l);
	if(x == "")
		mlogic.get_mfile().subtitles.erase(key);
	else
		mlogic.get_mfile().subtitles[key] = s_unescape(x);
	edispatch.subtitle_change();
	fbuf.redraw_framebuffer();
}
