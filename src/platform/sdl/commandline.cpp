#include "platform/sdl/platform.hpp"

#include <sstream>
#define MAXHISTORY 1000

namespace
{
	volatile uint32_t autorepeat_first = 10;
	volatile uint32_t autorepeat_subsequent = 4;
}

commandline_model::commandline_model() throw()
{
	enabled_flag = false;
	autorepeating_key = SPECIAL_NOOP;
	autorepeat_phase = 0;
	cursor_pos = 0;
	overwrite_mode = false;
}

std::string commandline_model::key(uint32_t k) throw(std::bad_alloc)
{
	std::string ret;
	switch(k) {
	case SPECIAL_NOOP:
		return "";
	case SPECIAL_ACK:
		history_itr = history.end();
		history.push_back(codepoints);
		if(history.size() > MAXHISTORY)
			history.pop_front();
		ret = read_command();
		enabled_flag = false;
		return ret;
	case SPECIAL_NAK:
		enabled_flag = false;
		return "";
	default:
		//Fall through.
		;
	};

	//Set up autorepeat if needed.
	if(k & PRESSED_MASK) {
		autorepeating_key = k;
		autorepeat_counter = 0;
		autorepeat_phase = 1;
	} else {
		//These all are active on positive edge.
		autorepeating_key = SPECIAL_NOOP;
		autorepeat_counter = 0;
		autorepeat_phase = 0;
		return "";
	}

	switch(k & ~PRESSED_MASK) {
	case SPECIAL_INSERT:
		overwrite_mode = !overwrite_mode;
	case SPECIAL_HOME:
		cursor_pos = 0;
		return "";
	case SPECIAL_END:
		cursor_pos = codepoints.size();
		return "";
	case SPECIAL_PGUP:
	case SPECIAL_UP:
		scroll_history_up();
		return "";
	case SPECIAL_PGDN:
	case SPECIAL_DOWN:
		scroll_history_down();
		return "";
	case SPECIAL_LEFT:
		if(cursor_pos > 0)
			cursor_pos--;
		return "";
	case SPECIAL_RIGHT:
		if(cursor_pos < codepoints.size())
			cursor_pos++;
		return "";
	case SPECIAL_BACKSPACE:
		if(cursor_pos == 0)
			//Nothing to delete.
			return "";
		delete_codepoint(cursor_pos - 1);
		cursor_pos--;
		return "";
	case SPECIAL_DELETE:
		if(cursor_pos == codepoints.size())
			//Nothing to delete.
			return "";
		delete_codepoint(cursor_pos);
		return "";
	case 0:
	case PRESSED_MASK:
		//Eh?
		return "";
	default:
		//This can't be NOOP, ACK nor NAK because those were checked above, nor is it special. Therefore
		//it is a character.
		handle_cow();
		if(!overwrite_mode || cursor_pos == codepoints.size()) {
			codepoints.resize(codepoints.size() + 1);
			for(size_t i = codepoints.size() - 1; i > cursor_pos; i--)
				codepoints[i] = codepoints[i - 1];
		}
		codepoints[cursor_pos++] = k & ~PRESSED_MASK;
		return "";
	};
}

void commandline_model::tick() throw(std::bad_alloc)
{
	if(autorepeat_phase == 0 || !enabled_flag)
		return;
	if(autorepeat_phase == 1) {
		autorepeat_counter++;
		if(autorepeat_counter >= autorepeat_first) {
			key(autorepeating_key);
			autorepeat_phase = 2;
			autorepeat_counter = 0;
		}
	}
	if(autorepeat_phase == 2) {
		autorepeat_counter++;
		if(autorepeat_counter >= autorepeat_subsequent) {
			key(autorepeating_key);
			autorepeat_counter = 0;
		}
	}
}

size_t commandline_model::cursor() throw()
{
	return enabled_flag ? cursor_pos : 0;
}

bool commandline_model::enabled() throw()
{
	return enabled_flag;
}

bool commandline_model::overwriting() throw()
{
	return overwrite_mode;
}

void commandline_model::enable(const std::string& cmd) throw()
{
	enable();
	unsigned left = 0;
	unsigned tmp = 0;
	for(size_t itr = 0; itr < cmd.length(); itr++) {
		unsigned char ch = cmd[itr];
		if(ch < 128)
			codepoints.push_back(ch);
		else if(left) {
			left--;
			tmp = tmp * 64 + (ch & 0x3F);
			if(!left)
				codepoints.push_back(tmp);
		} else if(ch < 192) {
		} else if(ch < 224) {
			left = 1;
			tmp = ch & 0x1F;
		} else if(ch < 240) {
			left = 2;
			tmp = ch & 0x0F;
		} else if(ch < 248) {
			left = 3;
			tmp = ch & 0x07;
		}
	}
	cursor_pos = codepoints.size();
}

void commandline_model::enable() throw()
{
	if(enabled_flag)
		return;
	enabled_flag = true;
	autorepeat_phase = 0;
	autorepeat_counter = 0;
	codepoints.clear();
	cursor_pos = 0;
	overwrite_mode = false;
	history_itr = history.end();
}

std::string commandline_model::read_command() throw(std::bad_alloc)
{
	std::ostringstream o;
	for(auto i : codepoints) {
		char buf[5] = {0, 0, 0, 0, 0};
		if(i < 0x80)
			buf[0] = i;
		else if(i < 0x800) {
			buf[0] = 0xC0 | (i >> 6);
			buf[1] = 0x80 | (i & 63);
		} else if(i < 0x10000) {
			buf[0] = 0xE0 | (i >> 12);
			buf[1] = 0x80 | ((i >> 6) & 63);
			buf[2] = 0x80 | (i & 63);
		} else if(i < 0x10FFFF) {
			buf[0] = 0xF0 | (i >> 18);
			buf[1] = 0x80 | ((i >> 12) & 63);
			buf[2] = 0x80 | ((i >> 6) & 63);
			buf[3] = 0x80 | (i & 63);
		}
		o << buf;
	}
	return o.str();
}

void commandline_model::handle_cow()
{
	if(history_itr == history.end())
		//Latched to end of history.
		return;
	codepoints = *history_itr;
	history_itr = history.end();
}

void commandline_model::scroll_history_up()
{
	if(history_itr == history.end())
		//Save the current commandline.
		saved_codepoints = codepoints;
	if(history_itr == history.begin())
		//At the begnning.
		return;
	history_itr--;
	codepoints = *history_itr;
	if(cursor_pos > codepoints.size())
		cursor_pos = codepoints.size();
}

void commandline_model::scroll_history_down()
{
	if(history_itr == history.end())
		//At the end.
		return;
	history_itr++;
	if(history_itr == history.end())
		codepoints = saved_codepoints;
	else
		codepoints = *history_itr;
	if(cursor_pos > codepoints.size())
		cursor_pos = codepoints.size();
}

void commandline_model::delete_codepoint(size_t idx)
{
	handle_cow();
	for(size_t i = idx; i < codepoints.size() - 1; i++)
		codepoints[i] = codepoints[i + 1];
	codepoints.resize(codepoints.size() - 1);
}

void commandline_model::paint(SDL_Surface* surf, uint32_t x, uint32_t y, uint32_t maxwidth, uint32_t color,
	bool box, uint32_t boxcolor) throw()
{
	if(!maxwidth)
		return;
	try {
		if(box)
			draw_box(surf, x, y, maxwidth, 16, boxcolor);
		if(enabled_flag)
			draw_string(surf, read_command(), x, y, maxwidth, color, overwrite_mode ? 2 : 1, cursor_pos);
		else
			draw_string(surf, "", x, y, maxwidth, color, 0, 0);
	} catch(...) {
		OOM_panic();
	}
}
