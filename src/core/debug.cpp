#include "cmdhelp/debug.hpp"
#include "core/command.hpp"
#include "core/debug.hpp"
#include "core/dispatch.hpp"
#include "core/instance.hpp"
#include "core/mainloop.hpp"
#include "core/messages.hpp"
#include "core/moviedata.hpp"
#include "core/rom.hpp"
#include "library/directory.hpp"
#include "library/memoryspace.hpp"

#include <functional>
#include <stdexcept>
#include <list>
#include <map>
#include <fstream>


namespace
{

	unsigned debug_flag(debug_context::etype type)
	{
		switch(type) {
		case debug_context::DEBUG_READ: return 1;
		case debug_context::DEBUG_WRITE: return 2;
		case debug_context::DEBUG_EXEC: return 4;
		case debug_context::DEBUG_TRACE: return 8;
		case debug_context::DEBUG_FRAME: return 0;
		default: throw std::runtime_error("Invalid debug callback type");
		}
	}
}

namespace
{
	template<class T> void kill_hooks(T& cblist, debug_context::etype type)
	{
		while(!cblist.empty()) {
			if(cblist.begin()->second.empty()) {
				cblist.erase(cblist.begin()->first);
				continue;
			}
			auto key = cblist.begin()->first;
			auto tmp = cblist.begin()->second.begin();
			(*tmp)->killed(key, type);
			cblist.begin()->second.erase(cblist.begin()->second.begin());
		}
		cblist.clear();
	}
}

debug_context::debug_context(emulator_dispatch& _dispatch, loaded_rom& _rom, memory_space& _mspace,
	command::group& _cmd)
	: edispatch(_dispatch), rom(_rom), mspace(_mspace), cmd(_cmd),
	showhooks(cmd, CDEBUG::scb, [this]() { this->do_showhooks(); }),
	genevent(cmd, CDEBUG::genevt, [this](const std::string& a) { this->do_genevent(a); }),
	tracecmd(cmd, CDEBUG::tr, [this](const std::string& a) { this->do_tracecmd(a); })
{
}

debug_context::callback_base::~callback_base()
{
}

const uint64_t debug_context::all_addresses = 0xFFFFFFFFFFFFFFFFULL;

void debug_context::add_callback(uint64_t addr, debug_context::etype type, debug_context::callback_base& cb)
{
	auto& core = CORE();
	std::map<uint64_t, cb_list>& xcb = get_lists(type);
	if(!corechange_r) {
		corechange.set(edispatch.core_change, [this]() { this->core_change(); });
		corechange_r = true;
	}
	if(!xcb.count(addr) && type != DEBUG_FRAME)
		core.rom->set_debug_flags(addr, debug_flag(type), 0);
	auto& lst = xcb[addr];
	lst.push_back(&cb);
}

void debug_context::remove_callback(uint64_t addr, debug_context::etype type, debug_context::callback_base& cb)
{
	std::map<uint64_t, cb_list>& xcb = get_lists(type);
	if(type == DEBUG_FRAME) addr = 0;
	if(!xcb.count(addr)) return;
	auto& l = xcb[addr];
	for(auto i = l.begin(); i != l.end(); i++) {
		if(*i == &cb) {
			l.erase(i);
			break;
		}
	}
	if(xcb[addr].empty()) {
		xcb.erase(addr);
		if(type != DEBUG_FRAME)
			rom.set_debug_flags(addr, 0, debug_flag(type));
	}
}

void debug_context::do_callback_read(uint64_t addr, uint64_t value)
{
	params p;
	p.type = DEBUG_READ;
	p.rwx.addr = addr;
	p.rwx.value = value;

	requesting_break = false;
	cb_list* cb1 = read_cb.count(all_addresses) ? &read_cb[all_addresses] : &dummy_cb;
	cb_list* cb2 = read_cb.count(addr) ? &read_cb[addr] : &dummy_cb;
	auto _cb1 = *cb1;
	auto _cb2 = *cb2;
	for(auto& i : _cb1) i->callback(p);
	for(auto& i : _cb2) i->callback(p);
	if(requesting_break)
		do_break_pause();
}

void debug_context::do_callback_write(uint64_t addr, uint64_t value)
{
	params p;
	p.type = DEBUG_WRITE;
	p.rwx.addr = addr;
	p.rwx.value = value;

	requesting_break = false;
	cb_list* cb1 = write_cb.count(all_addresses) ? &write_cb[all_addresses] : &dummy_cb;
	cb_list* cb2 = write_cb.count(addr) ? &write_cb[addr] : &dummy_cb;
	auto _cb1 = *cb1;
	auto _cb2 = *cb2;
	for(auto& i : _cb1) i->callback(p);
	for(auto& i : _cb2) i->callback(p);
	if(requesting_break)
		do_break_pause();
}

void debug_context::do_callback_exec(uint64_t addr, uint64_t cpu)
{
	params p;
	p.type = DEBUG_EXEC;
	p.rwx.addr = addr;
	p.rwx.value = cpu;

	requesting_break = false;
	cb_list* cb1 = exec_cb.count(all_addresses) ? &exec_cb[all_addresses] : &dummy_cb;
	cb_list* cb2 = exec_cb.count(addr) ? &exec_cb[addr] : &dummy_cb;
	auto _cb1 = *cb1;
	auto _cb2 = *cb2;
	if((1ULL << cpu) & xmask)
		for(auto& i : _cb1) i->callback(p);
	for(auto& i : _cb2) i->callback(p);
	if(requesting_break)
		do_break_pause();
}

void debug_context::do_callback_trace(uint64_t cpu, const char* str, bool true_insn)
{
	params p;
	p.type = DEBUG_TRACE;
	p.trace.cpu = cpu;
	p.trace.decoded_insn = str;
	p.trace.true_insn = true_insn;

	requesting_break = false;
	cb_list* cb = trace_cb.count(cpu) ? &trace_cb[cpu] : &dummy_cb;
	auto _cb = *cb;
	for(auto& i : _cb) i->callback(p);
	if(requesting_break)
		do_break_pause();
}

void debug_context::do_callback_frame(uint64_t frame, bool loadstate)
{
	params p;
	p.type = DEBUG_FRAME;
	p.frame.frame = frame;
	p.frame.loadstated = loadstate;

	cb_list* cb = frame_cb.count(0) ? &frame_cb[0] : &dummy_cb;
	auto _cb = *cb;
	for(auto& i : _cb) i->callback(p);
}

void debug_context::set_cheat(uint64_t addr, uint64_t value)
{
	rom.set_cheat(addr, value, true);
}

void debug_context::clear_cheat(uint64_t addr)
{
	rom.set_cheat(addr, 0, false);
}

void debug_context::setxmask(uint64_t mask)
{
	xmask = mask;
}

bool debug_context::is_tracelogging(uint64_t cpu)
{
	return (trace_outputs.count(cpu) != 0);
}

void debug_context::set_tracelog_change_cb(std::function<void()> cb)
{
	tracelog_change_cb = cb;
}

void debug_context::core_change()
{
	rom.debug_reset();
	kill_hooks(read_cb, DEBUG_READ);
	kill_hooks(write_cb, DEBUG_WRITE);
	kill_hooks(exec_cb, DEBUG_EXEC);
	kill_hooks(trace_cb, DEBUG_TRACE);
}

void debug_context::request_break()
{
	requesting_break = true;
}

debug_context::tracelog_file::tracelog_file(debug_context& _parent)
	: parent(_parent)
{
}

debug_context::tracelog_file::~tracelog_file()
{
}

void debug_context::tracelog_file::callback(const debug_context::params& p)
{
	if(!parent.trace_outputs.count(p.trace.cpu)) return;
	parent.trace_outputs[p.trace.cpu]->stream << p.trace.decoded_insn << std::endl;
}

void debug_context::tracelog_file::killed(uint64_t addr, debug_context::etype type)
{
	refcnt--;
	if(!refcnt)
		delete this;
}

void debug_context::tracelog(uint64_t proc, const std::string& filename)
{
	if(filename == "") {
		if(!trace_outputs.count(proc))
			return;
		remove_callback(proc, DEBUG_TRACE, *trace_outputs[proc]);
		trace_outputs[proc]->refcnt--;
		if(!trace_outputs[proc]->refcnt)
			delete trace_outputs[proc];
		trace_outputs.erase(proc);
		messages << "Stopped tracelogging processor #" << proc << std::endl;
		if(tracelog_change_cb) tracelog_change_cb();
		return;
	}
	if(trace_outputs.count(proc)) throw std::runtime_error("Already tracelogging");
	std::string full_filename = directory::absolute_path(filename);
	bool found = false;
	for(auto i : trace_outputs) {
		if(i.second->full_filename == full_filename) {
			i.second->refcnt++;
			trace_outputs[proc] = i.second;
			found = true;
			break;
		}
	}
	if(!found) {
		trace_outputs[proc] = new tracelog_file(*this);
		trace_outputs[proc]->refcnt = 1;
		trace_outputs[proc]->full_filename = full_filename;
		trace_outputs[proc]->stream.open(full_filename);
		if(!trace_outputs[proc]->stream) {
			delete trace_outputs[proc];
			trace_outputs.erase(proc);
			throw std::runtime_error("Can't open '" + full_filename + "'");
		}
	}
	try {
		add_callback(proc, DEBUG_TRACE, *trace_outputs[proc]);
	} catch(std::exception& e) {
		messages << "Error starting tracelogging: " << e.what() << std::endl;
		trace_outputs[proc]->refcnt--;
		if(!trace_outputs[proc]->refcnt)
			delete trace_outputs[proc];
		trace_outputs.erase(proc);
		throw;
	}
	messages << "Tracelogging processor #" << proc << " to '" << filename << "'" << std::endl;
	if(tracelog_change_cb) tracelog_change_cb();
}

void debug_context::do_showhooks()
{
	for(auto& i : read_cb)
		for(auto& j : i.second)
			messages << "READ addr=" << mspace.address_to_textual(i.first) << " handle=" << &j
				<< std::endl;
	for(auto& i : write_cb)
		for(auto& j : i.second)
			messages << "WRITE addr=" << mspace.address_to_textual(i.first) << " handle=" << &j
				<< std::endl;
	for(auto& i : exec_cb)
		for(auto& j : i.second)
			messages << "EXEC addr=" << mspace.address_to_textual(i.first) << " handle=" << &j
				<< std::endl;
	for(auto& i : trace_cb)
		for(auto& j : i.second)
			messages << "TRACE proc=" << i.first << " handle=" << &j << std::endl;
	for(auto& i : frame_cb)
		for(auto& j : i.second)
			messages << "FRAME handle=" << &j << std::endl;
}

void debug_context::do_genevent(const std::string& args)
{
	regex_results r = regex("([^ \t]+) ([^ \t]+) (.+)", args);
	if(!r) throw std::runtime_error("generate-memory-event: Bad arguments");
	if(r[1] == "r") {
		uint64_t addr = parse_value<uint64_t>(r[2]);
		uint64_t val = parse_value<uint64_t>(r[3]);
		do_callback_read(addr, val);
	} else if(r[1] == "w") {
		uint64_t addr = parse_value<uint64_t>(r[2]);
		uint64_t val = parse_value<uint64_t>(r[3]);
		do_callback_write(addr, val);
	} else if(r[1] == "x") {
		uint64_t addr = parse_value<uint64_t>(r[2]);
		uint64_t val = parse_value<uint64_t>(r[3]);
		do_callback_exec(addr, val);
	} else if(r[1] == "t") {
		uint64_t proc = parse_value<uint64_t>(r[2]);
		std::string str = r[3];
		do_callback_trace(proc, str.c_str());
	} else
		throw std::runtime_error("Invalid operation");
}

void debug_context::do_tracecmd(const std::string& args)
{
	regex_results r = regex("([^ \t]+)([ \t]+(.+))?", args);
	if(!r) throw std::runtime_error("tracelog: Bad arguments");
	std::string cpu = r[1];
	std::string filename = r[3];
	uint64_t _cpu = 0;
	for(auto i : rom.get_trace_cpus()) {
		if(cpu == i)
			goto out;
		_cpu++;
	}
	throw std::runtime_error("tracelog: Invalid CPU");
out:
	tracelog(_cpu, filename);
}
