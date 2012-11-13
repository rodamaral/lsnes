#include "messagebuffer.hpp"

messagebuffer::update_handler::~update_handler() throw()
{
}

messagebuffer::messagebuffer(size_t maxmessages, size_t windowsize) throw(std::bad_alloc, std::logic_error)
{
	if(windowsize > maxmessages)
		throw std::logic_error("Invalid window size");
	if(maxmessages == 0)
		throw std::logic_error("Invalid max message count");
	first_present_message = 0;
	next_message_number = 0;
	window_start = 0;
	max_messages = maxmessages;
	window_size = windowsize;
	scroll_frozen = false;
	updates_frozen = false;
	window_start_at_freeze = 0;
	window_size_at_freeze = 0;
	next_message_number_at_freeze = 0;
}

void messagebuffer::add_message(const std::string& msg) throw(std::bad_alloc, std::runtime_error)
{
	messages_buf[next_message_number++] = msg;
	//If too many messages, erase one.
	if(messages_buf.size() > max_messages)
		messages_buf.erase(first_present_message++);
	//Force scrolling if message in window got erased.
	if(window_start < first_present_message) {
		window_start = first_present_message;
		send_notifications();
	}
	//Autoscrolling at the end.
	if(!scroll_frozen && window_start + window_size + 1 == next_message_number) {
		window_start++;
		send_notifications();
	}
}

const std::string& messagebuffer::get_message(size_t msgnum) throw(std::bad_alloc, std::logic_error)
{
	if(!messages_buf.count(msgnum))
		throw std::logic_error("Invalid message number");
	return messages_buf[msgnum];
}

size_t messagebuffer::get_msg_first() throw()
{
	return first_present_message;
}

size_t messagebuffer::get_msg_count() throw()
{
	return next_message_number - first_present_message;
}

size_t messagebuffer::get_visible_first() throw()
{
	return window_start;
}

size_t messagebuffer::get_visible_count() throw()
{
	if(window_start + window_size > next_message_number)
		return next_message_number - window_start;
	else
		return window_size;
}

bool messagebuffer::is_more_messages() throw()
{
	return (window_start + window_size < next_message_number);
}

void messagebuffer::freeze_scrolling() throw()
{
	scroll_frozen = true;
}

void messagebuffer::unfreeze_scrolling() throw()
{
	scroll_frozen = false;
}

void messagebuffer::freeze_updates() throw()
{
	updates_frozen = true;
	window_size_at_freeze = window_size;
	window_start_at_freeze = window_start;
	next_message_number_at_freeze = next_message_number;
}

bool messagebuffer::unfreeze_updates() throw()
{
	updates_frozen = false;
	if(window_start_at_freeze < first_present_message)
		return true;
	uint64_t messages_visible_at_freeze;
	uint64_t messages_visible_now;
	if(window_start_at_freeze + window_size_at_freeze >= next_message_number_at_freeze)
		messages_visible_at_freeze = next_message_number_at_freeze - window_start_at_freeze;
	else
		messages_visible_at_freeze = window_size_at_freeze;
	messages_visible_now = get_visible_count();
	if(messages_visible_now != messages_visible_at_freeze)
		return true;
	if(window_start_at_freeze != window_start)
		return true;
	return false;
}

void messagebuffer::scroll_beginning() throw(std::bad_alloc, std::runtime_error)
{
	if(window_start == first_present_message)
		return;
	window_start = first_present_message;
	send_notifications();
}

void messagebuffer::scroll_up_page() throw(std::bad_alloc, std::runtime_error)
{
	if(window_start == first_present_message)
		return;
	if(window_start < first_present_message + window_size)
		window_start = first_present_message;
	else
		window_start -= window_size;
	send_notifications();
}

void messagebuffer::scroll_up_line() throw(std::bad_alloc, std::runtime_error)
{
	if(window_start == first_present_message)
		return;
	window_start--;
	send_notifications();
}

void messagebuffer::scroll_down_line() throw(std::bad_alloc, std::runtime_error)
{
	if(window_start + window_size >= next_message_number)
		return;
	window_start++;
	send_notifications();
}

void messagebuffer::scroll_down_page() throw(std::bad_alloc, std::runtime_error)
{
	if(window_start + window_size >= next_message_number)
		return;
	window_start += window_size;
	if(window_start + window_size >= next_message_number)
		window_start = next_message_number - window_size;
	send_notifications();
}

void messagebuffer::scroll_end() throw(std::bad_alloc, std::runtime_error)
{
	if(first_present_message + window_size > next_message_number)
		return;
	window_start = next_message_number - window_size;
	send_notifications();
}

void messagebuffer::register_handler(messagebuffer::update_handler& handler) throw(std::bad_alloc)
{
	handlers.insert(&handler);
}

void messagebuffer::unregister_handler(messagebuffer::update_handler& handler) throw()
{
	handlers.erase(&handler);
}

void messagebuffer::set_max_window_size(size_t windowsize) throw(std::bad_alloc, std::logic_error)
{
	if(windowsize > max_messages)
		throw std::logic_error("Invalid window size");
	if(window_size > windowsize) {
		//Shrink window.
		size_t shrinkage = window_size - windowsize;
		bool autoscrolling = !scroll_frozen && (window_start + window_size >= next_message_number);
		if(autoscrolling && window_start + windowsize < next_message_number)
			window_start = next_message_number - windowsize;
		window_size = windowsize;
		send_notifications();
	} else if(window_size < windowsize) {
		//Enlarge window.
		size_t enlargement = windowsize - window_size;
		if(first_present_message + windowsize >= next_message_number)
			window_start = first_present_message;
		else if(window_start + windowsize >= next_message_number)
			window_start = next_message_number - windowsize;
		window_size = windowsize;
		send_notifications();
	}
}

size_t messagebuffer::get_max_window_size() throw()
{
	return window_size;
}

void messagebuffer::send_notifications()
{
	if(updates_frozen)
		return;
	for(auto i : handlers)
		i->messagebuffer_update();
}
