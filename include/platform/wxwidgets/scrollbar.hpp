#ifndef _scrollbar__hpp__defined__
#define _scrollbar__hpp__defined__

#include <wx/scrolbar.h>
#include <functional>

class scroll_bar : public wxScrollBar
{
public:
	scroll_bar(wxWindow* parent, wxWindowID id, bool vertical);
	~scroll_bar();
	void set_page_size(unsigned pagesize);
	void set_range(unsigned range);
	void set_position(unsigned position);
	void apply_delta(int delta);
	void apply_wheel(int wheel, int wheelunit, unsigned speed);
	unsigned get_position();
	void set_handler(std::function<void(scroll_bar&)> cb);
private:
	void on_scroll(wxScrollEvent& e);
	std::function<void(scroll_bar&)> callback;
	unsigned pagesize;
	unsigned range;
	unsigned position;
	int scroll_acc;
};

#endif
