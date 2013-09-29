#include "core/dispatch.hpp"
#include "library/globalwrap.hpp"
#include "core/misc.hpp"

#include <sstream>
#include <iomanip>
#include <cmath>

#define START_EH_BLOCK try {
#define END_EH_BLOCK(obj, call) } catch(std::bad_alloc& e) { \
	OOM_panic(); \
	} catch(std::exception& e) { \
		messages << "[dumper " << obj->get_name() << "] Warning: " call ": " << e.what() \
			<< std::endl; \
	} catch(int code) { \
		messages << "[dumper " << obj->get_name() << "] Warning: " call ": Error code #" << code \
			<< std::endl; \
	}

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
	return str.str();
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
	globalwrap<std::list<information_dispatch*>> dispatch;
	globalwrap<std::list<information_dispatch*>> dispatch_audio;
	uint32_t srate_n = 32000;
	uint32_t srate_d = 1;
	struct gameinfo_struct sgi;
	bool recursive = false;
}

information_dispatch::information_dispatch(const std::string& name) throw(std::bad_alloc)
{
	target_name = name;
	dispatch().push_back(this);
	known_if_dumper = false;
	marked_as_dumper = false;
	notified_as_dumper = false;
}

information_dispatch::~information_dispatch() throw()
{
	for(auto i = dispatch().begin(); i != dispatch().end(); ++i) {
		if(*i == this) {
			dispatch().erase(i);
			break;
		}
	}
	for(auto i = dispatch_audio().begin(); i != dispatch_audio().end(); ++i) {
		if(*i == this) {
			dispatch_audio().erase(i);
			break;
		}
	}
	if(notified_as_dumper)
		for(auto& i : dispatch()) {
			START_EH_BLOCK
			i->on_destroy_dumper(target_name);
			END_EH_BLOCK(i, "on_destroy_dumper");
		}
}

void information_dispatch::on_frame(struct framebuffer_raw& _frame, uint32_t fps_n, uint32_t fps_d)
{
	//Do nothing.
}

void information_dispatch::do_frame(struct framebuffer_raw& _frame, uint32_t fps_n, uint32_t fps_d) throw()
{
	update_dumpers();
	for(auto& i : dispatch()) {
		START_EH_BLOCK
		i->on_frame(_frame, fps_n, fps_d);
		END_EH_BLOCK(i, "on_frame");
	}
}

void information_dispatch::on_sample(short l, short r)
{
	//Do nothing.
}

void information_dispatch::do_sample(short l, short r) throw()
{
	update_dumpers();
	for(auto& i : dispatch_audio()) {
		START_EH_BLOCK
		i->on_sample(l, r);
		END_EH_BLOCK(i, "on_sample");
	}
}

void information_dispatch::on_dump_end()
{
	//Do nothing.
}

void information_dispatch::do_dump_end() throw()
{
	update_dumpers();
	for(auto& i : dispatch()) {
		START_EH_BLOCK
		i->on_dump_end();
		END_EH_BLOCK(i, "on_dump_end");
	}
}

void information_dispatch::on_sound_rate(uint32_t rate_n, uint32_t rate_d)
{
	if(!known_if_dumper) {
		marked_as_dumper = get_dumper_flag();
		known_if_dumper = true;
	}
	if(marked_as_dumper) {
		messages << "[dumper " << get_name() << "] Warning: Sound sample rate changing not supported!"
			<< std::endl;
	}
}

void information_dispatch::do_sound_rate(uint32_t rate_n, uint32_t rate_d) throw()
{
	update_dumpers();
	uint32_t g = gcd(rate_n, rate_d);
	rate_n /= g;
	rate_d /= g;
	if(rate_n == srate_n && rate_d == srate_d)
		return;
	srate_n = rate_n;
	srate_d = rate_d;
	for(auto& i : dispatch()) {
		START_EH_BLOCK
		i->on_sound_rate(rate_n, rate_d);
		END_EH_BLOCK(i, "on_sound_rate");
	}
}

std::pair<uint32_t, uint32_t> information_dispatch::get_sound_rate() throw()
{
	return std::make_pair(srate_n, srate_d);
}

void information_dispatch::on_gameinfo(const struct gameinfo_struct& gi)
{
	//Do nothing.
}

void information_dispatch::do_gameinfo(const struct gameinfo_struct& gi) throw()
{
	update_dumpers();
	try {
		sgi = gi;
	} catch(...) {
		OOM_panic();
	}
	for(auto& i : dispatch()) {
		START_EH_BLOCK
		i->on_gameinfo(sgi);
		END_EH_BLOCK(i, "on_gameinfo");
	}
}

const struct gameinfo_struct& information_dispatch::get_gameinfo() throw()
{
	return sgi;
}

bool information_dispatch::get_dumper_flag() throw()
{
	return false;
}

void information_dispatch::on_new_dumper(const std::string& dumper)
{
	//Do nothing.
}

void information_dispatch::on_destroy_dumper(const std::string& dumper)
{
	//Do nothing.
}

unsigned information_dispatch::get_dumper_count() throw()
{
	update_dumpers(true);
	unsigned count = 0;
	for(auto& i : dispatch())
		if(i->marked_as_dumper)
			count++;
	if(!recursive) {
		recursive = true;
		update_dumpers();
		recursive = false;
	}
	return count;
}

std::set<std::string> information_dispatch::get_dumpers() throw(std::bad_alloc)
{
	update_dumpers();
	std::set<std::string> r;
	try {
		for(auto& i : dispatch())
			if(i->notified_as_dumper)
				r.insert(i->get_name());
	} catch(...) {
		OOM_panic();
	}
	return r;
}


const std::string& information_dispatch::get_name() throw()
{
	return target_name;
}

void information_dispatch::update_dumpers(bool nocalls) throw()
{
	for(auto& i : dispatch()) {
		if(!i->known_if_dumper) {
			i->marked_as_dumper = i->get_dumper_flag();
			i->known_if_dumper = true;
		}
		if(i->marked_as_dumper && !i->notified_as_dumper && !nocalls) {
			for(auto& j : dispatch()) {
				START_EH_BLOCK
				j->on_new_dumper(i->target_name);
				END_EH_BLOCK(j, "on_new_dumper");
			}
			i->notified_as_dumper = true;
		}
	}
}

void information_dispatch::enable_send_sound() throw(std::bad_alloc)
{
	dispatch_audio().push_back(this);
}

void information_dispatch::on_dumper_update()
{
	//Do nothing.
}

void information_dispatch::do_dumper_update() throw()
{
	if(in_global_ctors())
		return;
	for(auto& i : dispatch()) {
		START_EH_BLOCK
		i->on_dumper_update();
		END_EH_BLOCK(i, "on_dumper_update");
	}
}

void dispatch_set_error_streams(std::ostream* stream)
{
	notify_autofire_update.errors_to(stream);
	notify_autohold_reconfigure.errors_to(stream);
	notify_autohold_update.errors_to(stream);
	notify_close.errors_to(stream);
	notify_core_change.errors_to(stream);
	notify_mode_change.errors_to(stream);
	notify_new_core.errors_to(stream);
	notify_screen_update.errors_to(stream);
	notify_set_screen.errors_to(stream);
	notify_sound_change.errors_to(stream);
	notify_sound_unmute.errors_to(stream);
	notify_status_update.errors_to(stream);
	notify_subtitle_change.errors_to(stream);
	notify_voice_stream_change.errors_to(stream);
	notify_vu_change.errors_to(stream);
}

struct dispatcher<> notify_autohold_reconfigure("autohold_reconfigure");
struct dispatcher<unsigned, unsigned, unsigned, bool> notify_autohold_update("autohold_update");
struct dispatcher<unsigned, unsigned, unsigned, unsigned, unsigned> notify_autofire_update("autofire_update");
struct dispatcher<> notify_close("notify_close");
struct dispatcher<framebuffer<false>&> notify_set_screen("set_screen");
struct dispatcher<std::pair<std::string, std::string>> notify_sound_change("sound_change");
struct dispatcher<> notify_screen_update("screen_update");
struct dispatcher<> notify_status_update("status_update");
struct dispatcher<bool> notify_sound_unmute("sound_unmute");
struct dispatcher<bool> notify_mode_change("mode_change");
struct dispatcher<> notify_core_change("core_change");
struct dispatcher<bool> notify_core_changed("core_changed");
struct dispatcher<> notify_new_core("new_core");
struct dispatcher<> notify_voice_stream_change("voice_stream_change");
struct dispatcher<> notify_vu_change("vu_change");
struct dispatcher<> notify_subtitle_change("subtitle_change");
