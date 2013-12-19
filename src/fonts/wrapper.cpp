#include "library/framebuffer.hpp"

#include <cstring>

extern const char* font_hex_data;
framebuffer::font main_font;

void do_init_font()
{
	static bool flag = false;
	if(flag)
		return;
	main_font.load_hex(font_hex_data, strlen(font_hex_data));
	flag = true;
}
