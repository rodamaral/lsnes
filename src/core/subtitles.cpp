#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/framebuffer.hpp"
#include "core/moviedata.hpp"
#include "core/subtitles.hpp"
#include "core/window.hpp"
#include "library/string.hpp"
#include "fonts/wrapper.hpp"
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

	struct render_object_subtitle : public render_object
	{
		render_object_subtitle(int32_t _x, int32_t _y, const std::string& _text) throw()
			: x(_x), y(_y), text(_text), fg(0xFFFF80), bg(-1) {}
		~render_object_subtitle() throw() {}
		template<bool X> void op(struct framebuffer<X>& scr) throw()
		{
			fg.set_palette(scr);
			bg.set_palette(scr);
			main_font.render(scr, x, y, text, fg, bg, false, false);
		}
		void operator()(struct framebuffer<true>& scr) throw()  { op(scr); }
		void operator()(struct framebuffer<false>& scr) throw() { op(scr); }
		void clone(render_queue& q) const throw(std::bad_alloc) { q.clone_helper(this); }
	private:
		int32_t x;
		int32_t y;
		std::string text;
		premultiplied_color fg;
		premultiplied_color bg;
	};


	command::fnptr<const std::string&> edit_subtitle(lsnes_cmd, "edit-subtitle", "Edit a subtitle",
		"Syntax: edit-subtitle <first> <length> <text>\nAdd/Edit subtitle\n"
		"Syntax: edit-subtitle <first> <length>\nADelete subtitle\n",
		[](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
			auto r = regex("([0-9]+)[ \t]+([0-9]+)([ \t]+(.*))?", args, "Bad syntax");
			uint64_t frame = parse_value<uint64_t>(r[1]);
			uint64_t length = parse_value<uint64_t>(r[2]);
			std::string text = r[4];
			moviefile_subtiming key(frame, length);
			if(text == "")
				our_movie.subtitles.erase(key);
			else
				our_movie.subtitles[key] = s_unescape(text);
			notify_subtitle_change();
			redraw_framebuffer();
		});

	command::fnptr<> list_subtitle(lsnes_cmd, "list-subtitle", "List the subtitles",
		"Syntax: list-subtitle\nList the subtitles.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			for(auto i = our_movie.subtitles.rbegin(); i != our_movie.subtitles.rend(); i++) {
				messages << i->first.get_frame() << " " << i->first.get_length() << " "
					<< s_escape(i->second) << std::endl;
			}
		});

	command::fnptr<command::arg_filename> save_s(lsnes_cmd, "save-subtitle", "Save subtitles in .sub format",
		"Syntax: save-subtitle <file>\nSaves subtitles in .sub format to <file>\n",
		[](command::arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			if(our_movie.subtitles.empty())
				return;
			auto i = our_movie.subtitles.begin();
			uint64_t lastframe = i->first.get_frame() + i->first.get_length();
			std::ofstream y(std::string(args).c_str());
			if(!y)
				throw std::runtime_error("Can't open output file");
			std::string lasttxt = "";
			uint64_t since = 0;
			for(uint64_t i = 1; i < lastframe; i++) {
				moviefile_subtiming posmarker(i);
				auto j = our_movie.subtitles.upper_bound(posmarker);
				if(j == our_movie.subtitles.end())
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

std::string s_escape(std::string x)
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

std::string s_unescape(std::string x)
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

void render_subtitles(lua_render_context& ctx)
{
	if(our_movie.subtitles.empty())
		return;
	if(ctx.bottom_gap < 32)
		ctx.bottom_gap = 32;
	uint64_t curframe = movb.get_movie().get_current_frame() + 1;
	moviefile_subtiming posmarker(curframe);
	auto i = our_movie.subtitles.upper_bound(posmarker);
	if(i != our_movie.subtitles.end() && i->first.inrange(curframe)) {
		std::string subtxt = i->second;
		int32_t y = ctx.height;
		ctx.queue->create_add<render_object_subtitle>(0, y, subtxt);
	}
}

std::set<std::pair<uint64_t, uint64_t>> get_subtitles()
{
	std::set<std::pair<uint64_t, uint64_t>> r;
	for(auto i = our_movie.subtitles.rbegin(); i != our_movie.subtitles.rend(); i++)
		r.insert(std::make_pair(i->first.get_frame(), i->first.get_length()));
	return r;
}

std::string get_subtitle_for(uint64_t f, uint64_t l)
{
	moviefile_subtiming key(f, l);
	if(!our_movie.subtitles.count(key))
		return "";
	else
		return s_escape(our_movie.subtitles[key]);
}

void set_subtitle_for(uint64_t f, uint64_t l, const std::string& x)
{
	moviefile_subtiming key(f, l);
	if(x == "")
		our_movie.subtitles.erase(key);
	else
		our_movie.subtitles[key] = s_unescape(x);
	notify_subtitle_change();
	redraw_framebuffer();
}
