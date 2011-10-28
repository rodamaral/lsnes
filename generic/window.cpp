#include "window.hpp"
#include <boost/iostreams/categories.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/stream_buffer.hpp>
#include <boost/iostreams/filter/symmetric.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/device/back_inserter.hpp>

namespace
{
	class window_output
	{
	public:
		typedef char char_type;
		typedef boost::iostreams::sink_tag category;
		window_output(window* win)
		{
		}

		void close()
		{
		}

		std::streamsize write(const char* s, std::streamsize n)
		{
			size_t oldsize = stream.size();
			stream.resize(oldsize + n);
			memcpy(&stream[oldsize], s, n);
			while(true) {
				size_t lf = stream.size();
				for(size_t i = 0; i < stream.size(); i++)
					if(stream[i] == '\n') {
						lf = i;
						break;
					}
				if(lf == stream.size())
					break;
				std::string foo(stream.begin(), stream.begin() + lf);
				window::message(foo);
				if(lf + 1 < stream.size())
					memmove(&stream[0], &stream[lf + 1], stream.size() - lf - 1);
				stream.resize(stream.size() - lf - 1);
			}
			return n;
		}
	protected:
		std::vector<char> stream;
	};

	window_callback* wcb = NULL;
}

void window::init()
{
	graphics_init();
	sound_init();
}

void window::quit()
{
	sound_quit();
	graphics_quit();
}

std::ostream& window::out() throw(std::bad_alloc)
{
	static std::ostream* cached = NULL;
	window* win = NULL;
	if(!cached)
		cached = new boost::iostreams::stream<window_output>(win);
	return *cached;
}

window_callback::~window_callback() throw()
{
}

void window_callback::on_close() throw()
{
}

void window_callback::on_click(int32_t x, int32_t y, uint32_t buttonmask) throw()
{
}

void window_callback::do_close() throw()
{
	if(wcb)
		wcb->on_close();
}

void window_callback::do_click(int32_t x, int32_t y, uint32_t buttonmask) throw()
{
	if(wcb)
		wcb->on_click(x, y, buttonmask);
}

void window_callback::set_callback_handler(window_callback& cb) throw()
{
	wcb = &cb;
}
