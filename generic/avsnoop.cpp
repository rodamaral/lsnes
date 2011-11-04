#include "avsnoop.hpp"
#include "misc.hpp"
#include "globalwrap.hpp"
#include <sstream>
#include <iomanip>
#include <cmath>

gameinfo_struct::gameinfo_struct() throw(std::bad_alloc)
{
	length = 0;
	rerecords = "0";
}

std::string gameinfo_struct::get_readable_time(unsigned digits) const throw(std::bad_alloc)
{
	double bias = 0.5 * pow(10, -static_cast<int>(digits));
	double len = length + bias;
	std::ostringstream str;
	if(length >= 3600) {
		double hours = floor(len / 3600);
		str << hours << ":";
		len -= hours * 3600;
	}
	double minutes = floor(len / 60);
	len -= minutes * 60;
	double seconds = floor(len);
	len -= seconds;
	str << std::setw(2) << std::setfill('0') << minutes << ":" << seconds;
	if(digits > 0)
		str << ".";
	while(digits > 0) {
		len = 10 * len;
		str << '0' + static_cast<int>(len);
		len -= floor(len);
		digits--;
	}
}

size_t gameinfo_struct::get_author_count() const throw()
{
	return authors.size();
}

std::string gameinfo_struct::get_author_short(size_t idx) const throw(std::bad_alloc)
{
	if(idx >= authors.size())
		return "";
	const std::pair<std::string, std::string>& x = authors[idx];
	if(x.second != "")
		return x.second;
	else
		return x.first;
}

uint64_t gameinfo_struct::get_rerecords() const throw()
{
	uint64_t v = 0;
	uint64_t max = 0xFFFFFFFFFFFFFFFFULL;
	for(size_t i = 0; i < rerecords.length(); i++) {
		if(v < max / 10)
			//No risk of overflow.
			v = v * 10 + static_cast<unsigned>(rerecords[i] - '0');
		else if(v == max / 10) {
			//THis may overflow.
			v = v * 10;
			if(v + static_cast<unsigned>(rerecords[i] - '0') < v)
				return max;
			v = v + static_cast<unsigned>(rerecords[i] - '0');
		} else
			//Definite overflow.
			return max;
	}
	return v;
}


namespace
{
	globalwrap<std::list<av_snooper*>> snoopers;
	globalwrap<std::list<av_snooper::dump_notification*>> notifiers;
	uint32_t srate_n = 32000;
	uint32_t srate_d = 1;
	gameinfo_struct sgi;
	globalwrap<std::set<std::string>> sactive_dumpers;
}

av_snooper::av_snooper(const std::string& name) throw(std::bad_alloc)
{
	snoopers().push_back(this);
	for(auto i : notifiers())
		i->dump_starting(name);
	sactive_dumpers().insert(s_name = name);
}

av_snooper::~av_snooper() throw()
{
	sactive_dumpers().erase(s_name);
	for(auto i = snoopers().begin(); i != snoopers().end(); i++)
		if(*i == this) {
			snoopers().erase(i);
			break;
		}
	for(auto i : notifiers())
		i->dump_ending(s_name);
}

void av_snooper::frame(struct lcscreen& _frame, uint32_t fps_n, uint32_t fps_d, const uint32_t* raw, bool hires,
	bool interlaced, bool overscan, unsigned region) throw(std::bad_alloc, std::runtime_error)
{
	//Nothing.
}

void av_snooper::_frame(struct lcscreen& _frame, uint32_t fps_n, uint32_t fps_d, const uint32_t* raw, bool hires,
	bool interlaced, bool overscan, unsigned region) throw(std::bad_alloc)
{
	for(auto i : snoopers())
		try {
			i->frame(_frame, fps_n, fps_d, raw, hires, interlaced, overscan, region);
		} catch(std::bad_alloc& e) {
			throw;
		} catch(std::exception& e) {
			try {
				messages << "Error dumping frame: " << e.what() << std::endl;
			} catch(...) {
			}
		}
}

void av_snooper::sample(short l, short r) throw(std::bad_alloc, std::runtime_error)
{
	//Nothing.
}

void av_snooper::_sample(short l, short r) throw(std::bad_alloc)
{
	for(auto i : snoopers())
		try {
			i->sample(l, r);
		} catch(std::bad_alloc& e) {
			throw;
		} catch(std::exception& e) {
			try {
				messages << "Error dumping sample: " << e.what() << std::endl;
			} catch(...) {
			}
		}
}

void av_snooper::end() throw(std::bad_alloc, std::runtime_error)
{
	//Nothing.
}

void av_snooper::_end() throw(std::bad_alloc)
{
	for(auto i : snoopers())
		try {
			i->end();
		} catch(std::bad_alloc& e) {
			throw;
		} catch(std::exception& e) {
			try {
				messages << "Error ending dump: " << e.what() << std::endl;
			} catch(...) {
			}
		}
}

void av_snooper::sound_rate(uint32_t rate_n, uint32_t rate_d) throw(std::bad_alloc, std::runtime_error)
{
	std::ostringstream str;
	str << s_name << " dumper does not support variable samping rate.";
	throw std::runtime_error(str.str());
}

void av_snooper::_sound_rate(uint32_t rate_n, uint32_t rate_d)
{
	uint32_t g = gcd(rate_n, rate_d);
	srate_n = rate_n / g;
	srate_d = rate_d / g;
	for(auto i : snoopers())
		try {
			i->sound_rate(srate_n, srate_d);
		} catch(std::bad_alloc& e) {
			throw;
		} catch(std::exception& e) {
			try {
				messages << "Error setting sound frequency: " << e.what() << std::endl;
			} catch(...) {
			}
		}
}

std::pair<uint32_t, uint32_t> av_snooper::get_sound_rate() throw()
{
	return std::make_pair(srate_n, srate_d);
}

void av_snooper::gameinfo(const struct gameinfo_struct& gi) throw(std::bad_alloc, std::runtime_error)
{
	//Nothing.
}

void av_snooper::_gameinfo(const struct gameinfo_struct& gi) throw(std::bad_alloc)
{
	sgi = gi;
	for(auto i : snoopers())
		try {
			i->gameinfo(gi);
		} catch(std::bad_alloc& e) {
			throw;
		} catch(std::exception& e) {
			try {
				messages << "Error sending game info: " << e.what() << std::endl;
			} catch(...) {
			}
		}
}

const struct gameinfo_struct& av_snooper::get_gameinfo() throw(std::bad_alloc)
{
	return sgi;
}

bool av_snooper::dump_in_progress() throw()
{
	return !snoopers().empty();
}

av_snooper::dump_notification::dump_notification() throw(std::bad_alloc)
{
	notifiers().push_back(this);
}

av_snooper::dump_notification::~dump_notification() throw()
{
	for(auto i = notifiers().begin(); i != notifiers().end(); i++)
		if(*i == this) {
			notifiers().erase(i);
			return;
		}
}

void av_snooper::dump_notification::dump_starting(const std::string& type) throw()
{
}

void av_snooper::dump_notification::dump_ending(const std::string& type) throw()
{
}

const std::set<std::string>& av_snooper::active_dumpers()
{
	return sactive_dumpers();
}
