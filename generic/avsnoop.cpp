#include "avsnoop.hpp"
#include "misc.hpp"
#include "globalwrap.hpp"

namespace
{
	globalwrap<std::list<av_snooper*>> snoopers;
	std::list<av_snooper::dump_notification*> notifiers;
	std::string s_gamename;
	std::string s_rerecords;
	double s_gametime;
	std::list<std::pair<std::string, std::string>> s_authors;
	bool gameinfo_set;
}

av_snooper::av_snooper() throw(std::bad_alloc)
{
	snoopers().push_back(this);
	for(auto i = notifiers.begin(); i != notifiers.end(); i++)
		(*i)->dump_starting();
}

av_snooper::~av_snooper() throw()
{
	for(auto i = snoopers().begin(); i != snoopers().end(); i++)
		if(*i == this) {
			snoopers().erase(i);
			break;
		}
	for(auto i = notifiers.begin(); i != notifiers.end(); i++)
		(*i)->dump_ending();
}

void av_snooper::frame(struct lcscreen& _frame, uint32_t fps_n, uint32_t fps_d, bool dummy) throw(std::bad_alloc)
{
	for(auto i = snoopers().begin(); i != snoopers().end(); i++)
		try {
			(*i)->frame(_frame, fps_n, fps_d);
		} catch(std::bad_alloc& e) {
			throw;
		} catch(std::exception& e) {
			try {
				messages << "Error dumping frame: " << e.what() << std::endl;
			} catch(...) {
			}
		}
}

void av_snooper::sample(short l, short r, bool dummy) throw(std::bad_alloc)
{
	for(auto i = snoopers().begin(); i != snoopers().end(); i++)
		try {
			(*i)->sample(l, r);
		} catch(std::bad_alloc& e) {
			throw;
		} catch(std::exception& e) {
			try {
				messages << "Error dumping sample: " << e.what() << std::endl;
			} catch(...) {
			}
		}
}

void av_snooper::end(bool dummy) throw(std::bad_alloc)
{
	for(auto i = snoopers().begin(); i != snoopers().end(); i++)
		try {
			(*i)->end();
		} catch(std::bad_alloc& e) {
			throw;
		} catch(std::exception& e) {
			try {
				messages << "Error ending dump: " << e.what() << std::endl;
			} catch(...) {
			}
		}
}

void av_snooper::gameinfo(const std::string& gamename, const std::list<std::pair<std::string, std::string>>&
		authors, double gametime, const std::string& rerecords, bool dummy) throw(std::bad_alloc)
{
	s_gamename = gamename;
	s_authors = authors;
	s_gametime = gametime;
	s_rerecords = rerecords;
	gameinfo_set = true;
	for(auto i = snoopers().begin(); i != snoopers().end(); i++)
		(*i)->send_gameinfo();
}

void av_snooper::send_gameinfo() throw()
{
	if(gameinfo_set)
		try {
			gameinfo(s_gamename, s_authors, s_gametime, s_rerecords);
		} catch(...) {
		}
}

bool av_snooper::dump_in_progress() throw()
{
	return !snoopers().empty();
}

av_snooper::dump_notification::~dump_notification() throw()
{
}

void av_snooper::dump_notification::dump_starting() throw()
{
}

void av_snooper::dump_notification::dump_ending() throw()
{
}

void av_snooper::add_dump_notifier(av_snooper::dump_notification& notifier) throw(std::bad_alloc)
{
	notifiers.push_back(&notifier);
}

void av_snooper::remove_dump_notifier(av_snooper::dump_notification& notifier) throw()
{
	for(auto i = notifiers.begin(); i != notifiers.end(); i++)
		if(*i == &notifier) {
			notifiers.erase(i);
			return;
		}
}
