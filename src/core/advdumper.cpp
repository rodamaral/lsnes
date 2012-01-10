#include "core/advdumper.hpp"
#include "core/dispatch.hpp"
#include "core/globalwrap.hpp"

#include <map>
#include <string>

namespace
{
	globalwrap<std::map<std::string, adv_dumper*>> dumpers;
}

const std::string& adv_dumper::id() throw()
{
	return d_id;
}

adv_dumper::~adv_dumper()
{
	dumpers().erase(d_id);
	information_dispatch::do_dumper_update();
}

std::set<adv_dumper*> adv_dumper::get_dumper_set() throw(std::bad_alloc)
{
	std::set<adv_dumper*> d;
	for(auto i : dumpers())
		d.insert(i.second);
	return d;
}

adv_dumper::adv_dumper(const std::string& id) throw(std::bad_alloc)
{
	d_id = id;
	dumpers()[d_id] = this;
}
