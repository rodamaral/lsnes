#include "core/advdumper.hpp"
#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/instance.hpp"
#include "core/moviedata.hpp"
#include "core/rom.hpp"
#include "library/globalwrap.hpp"
#include "lua/lua.hpp"
#include "library/string.hpp"

#include <map>
#include <string>


namespace
{
	globalwrap<std::map<std::string, dumper_factory_base*>> dumpers;
	globalwrap<std::set<dumper_factory_base::notifier*>> notifiers;
}

master_dumper::gameinfo::gameinfo() throw(std::bad_alloc)
{
	length = 0;
	rerecords = "0";
}

std::string master_dumper::gameinfo::get_readable_time(unsigned digits) const throw(std::bad_alloc)
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

size_t master_dumper::gameinfo::get_author_count() const throw()
{
	return authors.size();
}

std::string master_dumper::gameinfo::get_author_short(size_t idx) const throw(std::bad_alloc)
{
	if(idx >= authors.size())
		return "";
	const std::pair<std::string, std::string>& x = authors[idx];
	if(x.second != "")
		return x.second;
	else
		return x.first;
}

std::string master_dumper::gameinfo::get_author_long(size_t idx) const throw(std::bad_alloc)
{
	if(idx >= authors.size())
		return "";
	const std::pair<std::string, std::string>& x = authors[idx];
	if(x.first != "") {
		if(x.second != "")
			return x.first + " (" + x.second + ")";
		else
			return x.first;
	} else {
		if(x.second != "")
			return "(" + x.second + ")";
		else
			return "";
	}
}

uint64_t master_dumper::gameinfo::get_rerecords() const throw()
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

dumper_factory_base::notifier::~notifier()
{
}

const std::string& dumper_factory_base::id() throw()
{
	return d_id;
}

dumper_factory_base::~dumper_factory_base()
{
	dumpers().erase(d_id);
	run_notify();
}

std::set<dumper_factory_base*> dumper_factory_base::get_dumper_set() throw(std::bad_alloc)
{
	std::set<dumper_factory_base*> d;
	for(auto i : dumpers())
		d.insert(i.second);
	return d;
}

dumper_factory_base::dumper_factory_base(const std::string& id) throw(std::bad_alloc)
{
	d_id = id;
	dumpers()[d_id] = this;
}

void dumper_factory_base::ctor_notify()
{
	run_notify();
}

void dumper_factory_base::add_notifier(dumper_factory_base::notifier& n)
{
	notifiers().insert(&n);
}

void dumper_factory_base::drop_notifier(dumper_factory_base::notifier& n)
{
	notifiers().erase(&n);
}

void dumper_factory_base::run_notify()
{
	for(auto i : notifiers())
		i->dumpers_updated();
}

unsigned dumper_factory_base::target_type_mask = 3;
unsigned dumper_factory_base::target_type_file = 0;
unsigned dumper_factory_base::target_type_prefix = 1;
unsigned dumper_factory_base::target_type_special = 2;

dumper_base::dumper_base()
{
	mdumper = NULL;
	fbase = NULL;
}

dumper_base::dumper_base(master_dumper& _mdumper, dumper_factory_base& _fbase)
	: mdumper(&_mdumper), fbase(&_fbase)
{
	threads::arlock h(mdumper->lock);
	mdumper->dumpers[fbase] = this;
}

dumper_base::~dumper_base() throw()
{
	if(!mdumper) return;
	threads::arlock h(mdumper->lock);
	mdumper->dumpers.erase(fbase);
	mdumper->statuschange();
}

master_dumper::notifier::~notifier()
{
}

master_dumper::master_dumper()
{
	current_rate_n = 48000;
	current_rate_d = 1;
	output = &std::cerr;
}

dumper_base* master_dumper::get_instance(dumper_factory_base* f) throw()
{
	threads::arlock h(lock);
	return dumpers.count(f) ? dumpers[f] : NULL;
}

dumper_base* master_dumper::start(dumper_factory_base& factory, const std::string& mode,
	const std::string& targetname) throw(std::bad_alloc, std::runtime_error)
{
	threads::arlock h(lock);
	auto f = factory.start(*this, mode, targetname);
	statuschange();
	return f;
}

void master_dumper::add_notifier(master_dumper::notifier& n)
{
	threads::arlock h(lock);
	notifications.insert(&n);
}

void master_dumper::drop_notifier(master_dumper::notifier& n)
{
	threads::arlock h(lock);
	notifications.erase(&n);
}

void master_dumper::add_dumper(dumper_base& n)
{
	threads::arlock h(lock);
	sdumpers.insert(&n);
}

void master_dumper::drop_dumper(dumper_base& n)
{
	threads::arlock h(lock);
	sdumpers.erase(&n);
}

void master_dumper::statuschange()
{
	for(auto i : notifications)
		i->dump_status_change();
}

std::pair<uint32_t, uint32_t> master_dumper::get_rate()
{
	threads::arlock h(lock);
	return std::make_pair(current_rate_n, current_rate_d);
}

const master_dumper::gameinfo& master_dumper::get_gameinfo()
{
	return current_gi;
}

unsigned master_dumper::get_dumper_count() throw()
{
	threads::arlock h(lock);
	return sdumpers.size();
}

void master_dumper::on_frame(struct framebuffer::raw& _frame, uint32_t fps_n, uint32_t fps_d)
{
	threads::arlock h(lock);
	for(auto i : sdumpers)
		try {
			i->on_frame(_frame, fps_n, fps_d);
		} catch(std::exception& e) {
			(*output) << "Error in on_frame: " << e.what() << std::endl;
		} catch(...) {
			(*output) << "Error in on_frame: <unknown error>" << std::endl;
		}
}

void master_dumper::on_sample(short l, short r)
{
	threads::arlock h(lock);
	for(auto i : sdumpers)
		try {
			i->on_sample(l, r);
		} catch(std::exception& e) {
			(*output) << "Error in on_sample: " << e.what() << std::endl;
		} catch(...) {
			(*output) << "Error in on_sample: <unknown error>" << std::endl;
		}
}

void master_dumper::on_rate_change(uint32_t n, uint32_t d)
{
	threads::arlock h(lock);
	uint32_t ga = gcd(n, d);
	n /= ga;
	d /= ga;
	if(n != current_rate_n || d != current_rate_d) {
		current_rate_n = n;
		current_rate_d = d;
	} else
		return;

	for(auto i : sdumpers)
		try {
			i->on_rate_change(current_rate_n, current_rate_d);
		} catch(std::exception& e) {
			(*output) << "Error in on_rate_change: " << e.what() << std::endl;
		} catch(...) {
			(*output) << "Error in on_rate_change: <unknown error>" << std::endl;
		}
}

void master_dumper::on_gameinfo_change(const gameinfo& gi)
{
	threads::arlock h(lock);
	current_gi = gi;
	for(auto i : sdumpers)
		try {
			i->on_gameinfo_change(current_gi);
		} catch(std::exception& e) {
			(*output) << "Error in on_gameinfo_change: " << e.what() << std::endl;
		} catch(...) {
			(*output) << "Error in on_gameinfo_change: <unknown error>" << std::endl;
		}
}

void master_dumper::end_dumps()
{
	threads::arlock h(lock);
	while(sdumpers.size() > 0) {
		auto d = *sdumpers.begin();
		try {
			d->on_end();
		} catch(std::exception& e) {
			(*output) << "Error in on_end: " << e.what() << std::endl;
			sdumpers.erase(d);
		} catch(...) {
			(*output) << "Error in on_end: <unknown error>" << std::endl;
			sdumpers.erase(d);
		}
	}
}

void master_dumper::set_output(std::ostream* _output)
{
	threads::arlock h(lock);
	output = _output;
}

template<bool X> bool render_video_hud(struct framebuffer::fb<X>& target, struct framebuffer::raw& source,
	uint32_t hscl, uint32_t vscl, uint32_t lgap, uint32_t tgap, uint32_t rgap, uint32_t bgap,
	std::function<void()> fn)
{
	bool lua_kill_video = false;
	struct lua::render_context lrc;
	framebuffer::queue rq;
	lrc.left_gap = lgap;
	lrc.right_gap = rgap;
	lrc.bottom_gap = bgap;
	lrc.top_gap = tgap;
	lrc.queue = &rq;
	lrc.width = source.get_width();
	lrc.height = source.get_height();
	lua_callback_do_video(&lrc, lua_kill_video, hscl, vscl);
	if(fn)
		fn();
	target.reallocate(lrc.left_gap + source.get_width() * hscl + lrc.right_gap, lrc.top_gap +
		source.get_height() * vscl + lrc.bottom_gap, false);
	target.set_origin(lrc.left_gap, lrc.top_gap);
	target.copy_from(source, hscl, vscl);
	rq.run(target);
	return !lua_kill_video;
}

uint64_t killed_audio_length(uint32_t fps_n, uint32_t fps_d, double& fraction)
{
	auto r = CORE().mdumper->get_rate();
	double x = 1.0 * fps_d * r.first / (fps_n * r.second) + fraction;
	uint64_t y = x;
	fraction = x - y;
	return y;
}

template bool render_video_hud(struct framebuffer::fb<false>& target, struct framebuffer::raw& source, uint32_t hscl,
	uint32_t vscl, uint32_t lgap, uint32_t tgap, uint32_t rgap, uint32_t bgap, std::function<void()> fn);
template bool render_video_hud(struct framebuffer::fb<true>& target, struct framebuffer::raw& source, uint32_t hscl,
	uint32_t vscl, uint32_t lgap, uint32_t tgap, uint32_t rgap, uint32_t bgap, std::function<void()> fn);
