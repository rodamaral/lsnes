#include "platform/wxwidgets/scrollbar.hpp"
#include "platform/wxwidgets/platform.hpp"
#include "library/minmax.hpp"
#include <functional>
#include <iostream>

scroll_bar::scroll_bar(wxWindow* parent, wxWindowID id, bool vertical)
	: wxScrollBar(parent, id, wxDefaultPosition, wxDefaultSize, vertical ? wxSB_VERTICAL : wxSB_HORIZONTAL)
{
	CHECK_UI_THREAD;
	Connect(wxEVT_SCROLL_THUMBTRACK, wxScrollEventHandler(scroll_bar::on_scroll), NULL, this);
	Connect(wxEVT_SCROLL_PAGEDOWN, wxScrollEventHandler(scroll_bar::on_scroll), NULL, this);
	Connect(wxEVT_SCROLL_PAGEUP, wxScrollEventHandler(scroll_bar::on_scroll), NULL, this);
	Connect(wxEVT_SCROLL_LINEDOWN, wxScrollEventHandler(scroll_bar::on_scroll), NULL, this);
	Connect(wxEVT_SCROLL_LINEUP, wxScrollEventHandler(scroll_bar::on_scroll), NULL, this);
	Connect(wxEVT_SCROLL_TOP, wxScrollEventHandler(scroll_bar::on_scroll), NULL, this);
	Connect(wxEVT_SCROLL_BOTTOM, wxScrollEventHandler(scroll_bar::on_scroll), NULL, this);
	position = 0;
	range = 0;
	pagesize = 1;
	scroll_acc = 0;
}

scroll_bar::~scroll_bar()
{
}

void scroll_bar::set_page_size(unsigned _pagesize)
{
	CHECK_UI_THREAD;
	if(!_pagesize) _pagesize = 1;
	pagesize = _pagesize;
	if(range > pagesize)
		SetScrollbar(position, pagesize, range, max(pagesize - 1, 1U));
	else
		SetScrollbar(0, 0, 0, 0);
}

void scroll_bar::set_range(unsigned _range)
{
	CHECK_UI_THREAD;
	if(pagesize >= _range)
		position = 0;
	else if(position + pagesize > _range)
		position = _range - pagesize;
	range = _range;
	if(range > pagesize)
		SetScrollbar(position, pagesize, range, max(pagesize - 1, 1U));
	else
		SetScrollbar(0, 0, 0, 0);
}

void scroll_bar::set_position(unsigned _position)
{
	CHECK_UI_THREAD;
	if(pagesize >= range)
		_position = 0;
	else if(_position + pagesize > range)
		_position = range - pagesize;
	position = _position;
	if(range > pagesize)
		SetScrollbar(position, pagesize, range, max(pagesize - 1, 1U));
	else
		SetScrollbar(0, 0, 0, 0);
}

void scroll_bar::apply_delta(int delta)
{
	CHECK_UI_THREAD;
	unsigned maxscroll = range - pagesize;
	if(maxscroll > range)
		maxscroll = 0;
	unsigned newscroll = position + delta;
	if(newscroll > range)
		newscroll = (delta < 0) ? 0 : maxscroll;
	position = newscroll;
	if(position > maxscroll)
		position = maxscroll;
	if(range > pagesize)
		SetScrollbar(position, pagesize, range, max(pagesize - 1, 1U));
	else
		SetScrollbar(0, 0, 0, 0);
	callback(*this);
}

void scroll_bar::apply_wheel(int wheel, int wheelunit, unsigned speed)
{
	if(!wheel || !wheelunit)
		return;
	scroll_acc += wheel;
	while(wheelunit && scroll_acc <= -wheelunit) {
		apply_delta(static_cast<int>(speed));
		scroll_acc += wheelunit;
	}
	while(wheelunit && scroll_acc >= wheelunit) {
		apply_delta(-static_cast<int>(speed));
		scroll_acc -= wheelunit;
	}
}

unsigned scroll_bar::get_position()
{
	return position;
}

void scroll_bar::set_handler(std::function<void(scroll_bar&)> cb)
{
	callback = cb;
}

void scroll_bar::on_scroll(wxScrollEvent& e)
{
	CHECK_UI_THREAD;
	if(range)
		position = GetThumbPosition();
	if(pagesize >= range)
		position = 0;
	else if(position + pagesize > range)
		position = range - pagesize;
	callback(*this);
}
