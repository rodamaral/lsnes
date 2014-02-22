#include "core/command.hpp"
#include "core/debug.hpp"
#include "core/dispatch.hpp"
#include "core/mainloop.hpp"
#include "core/moviedata.hpp"
#include "library/directory.hpp"
#include <stdexcept>
#include <list>
#include <map>
#include <fstream>


namespace
{
	struct cb_rwx
	{
		std::function<void(uint64_t addr, uint64_t value)> cb;
		std::function<void()> dtor;
	};
	struct cb_trace
	{
		std::function<void(uint64_t proc, const char* str, bool true_insn)> cb;
		std::function<void()> dtor;
	};
	typedef std::list<cb_rwx> cb_list;
	typedef std::list<cb_trace> cb2_list;
	std::map<uint64_t, cb_list> read_cb;
	std::map<uint64_t, cb_list> write_cb;
	std::map<uint64_t, cb_list> exec_cb;
	std::map<uint64_t, cb2_list> trace_cb;
	cb_list dummy_cb;  //Always empty.
	cb2_list dummy_cb2;  //Always empty.
	uint64_t xmask = 1;
	std::function<void()> tracelog_change_cb;
	struct dispatch::target<> corechange;
	bool corechange_r = false;
	bool requesting_break = false;

	struct tracelog_file
	{
		std::ofstream stream;
		std::string full_filename;
		unsigned refcnt;
	};
	std::map<uint64_t, std::pair<tracelog_file*, debug_handle>> trace_outputs;

	std::map<uint64_t, cb_list>& get_lists(debug_type type)
	{
		switch(type) {
		case DEBUG_READ: return read_cb;
		case DEBUG_WRITE: return write_cb;
		case DEBUG_EXEC: return exec_cb;
		default: throw std::runtime_error("Invalid debug callback type");
		}
	}

	unsigned debug_flag(debug_type type)
	{
		switch(type) {
		case DEBUG_READ: return 1;
		case DEBUG_WRITE: return 2;
		case DEBUG_EXEC: return 4;
		case DEBUG_TRACE: return 8;
		default: throw std::runtime_error("Invalid debug callback type");
		}
	}
}

const uint64_t debug_all_addr = 0xFFFFFFFFFFFFFFFFULL;

namespace
{
	template<class T> debug_handle _debug_add_callback(std::map<uint64_t, std::list<T>>& cb, uint64_t addr,
		debug_type type, T fn, std::function<void()> dtor)
	{
		if(!corechange_r) {
			corechange.set(notify_core_change, []() { debug_core_change(); });
			corechange_r = true;
		}
		if(!cb.count(addr))
			our_rom.rtype->set_debug_flags(addr, debug_flag(type), 0);

		auto& lst = cb[addr];
		lst.push_back(fn);
		debug_handle h;
		h.handle = &*lst.rbegin();
		return h;
	}

	template<class T> void _debug_remove_callback(T& cb, uint64_t addr, debug_type type, debug_handle handle)
	{
		if(!cb.count(addr)) return;
		auto& l = cb[addr];
		for(auto i = l.begin(); i != l.end(); i++) {
			if(&*i == handle.handle) {
				l.erase(i);
				break;
			}
		}
		if(cb[addr].empty()) {
			cb.erase(addr);
			our_rom.rtype->set_debug_flags(addr, 0, debug_flag(type));
		}
	}

	template<class T> void kill_hooks(T& cblist)
	{
		while(!cblist.empty()) {
			if(cblist.begin()->second.empty()) {
				cblist.erase(cblist.begin()->first);
				continue;
			}
			auto tmp = cblist.begin()->second.begin();
			tmp->dtor();
		}
		cblist.clear();
	}
}

debug_handle debug_add_callback(uint64_t addr, debug_type type, std::function<void(uint64_t addr, uint64_t value)> fn,
	std::function<void()> dtor)
{
	std::map<uint64_t, cb_list>& cb = get_lists(type);
	cb_rwx t;
	t.cb = fn;
	t.dtor = dtor;
	return _debug_add_callback(cb, addr, type, t, dtor);
}

debug_handle debug_add_trace_callback(uint64_t proc, std::function<void(uint64_t proc, const char* str,
	bool true_insn)> fn, std::function<void()> dtor)
{
	cb_trace t;
	t.cb = fn;
	t.dtor = dtor;
	return _debug_add_callback(trace_cb, proc, DEBUG_TRACE, t, dtor);
}

void debug_remove_callback(uint64_t addr, debug_type type, debug_handle handle)
{
	if(type == DEBUG_TRACE) {
		_debug_remove_callback(trace_cb, addr, DEBUG_TRACE, handle);
	} else {
		_debug_remove_callback(get_lists(type), addr, type, handle);
	}
}

void debug_fire_callback_read(uint64_t addr, uint64_t value)
{
	requesting_break = false;
	cb_list* cb1 = read_cb.count(debug_all_addr) ? &read_cb[debug_all_addr] : &dummy_cb;
	cb_list* cb2 = read_cb.count(addr) ? &read_cb[addr] : &dummy_cb;
	for(auto& i : *cb1) i.cb(addr, value);
	for(auto& i : *cb2) i.cb(addr, value);
	if(requesting_break)
		do_break_pause();
}

void debug_fire_callback_write(uint64_t addr, uint64_t value)
{
	requesting_break = false;
	cb_list* cb1 = write_cb.count(debug_all_addr) ? &write_cb[debug_all_addr] : &dummy_cb;
	cb_list* cb2 = write_cb.count(addr) ? &write_cb[addr] : &dummy_cb;
	for(auto& i : *cb1) i.cb(addr, value);
	for(auto& i : *cb2) i.cb(addr, value);
	if(requesting_break)
		do_break_pause();
}

void debug_fire_callback_exec(uint64_t addr, uint64_t value)
{
	requesting_break = false;
	cb_list* cb1 = exec_cb.count(debug_all_addr) ? &exec_cb[debug_all_addr] : &dummy_cb;
	cb_list* cb2 = exec_cb.count(addr) ? &exec_cb[addr] : &dummy_cb;
	if(value & xmask)
		for(auto& i : *cb1) i.cb(addr, value);
	for(auto& i : *cb2) i.cb(addr, value);
	if(requesting_break)
		do_break_pause();
}

void debug_fire_callback_trace(uint64_t proc, const char* str, bool true_insn)
{
	requesting_break = false;
	cb2_list* cb = trace_cb.count(proc) ? &trace_cb[proc] : &dummy_cb2;
	for(auto& i : *cb) i.cb(proc, str, true_insn);
	if(requesting_break)
		do_break_pause();
}

void debug_set_cheat(uint64_t addr, uint64_t value)
{
	our_rom.rtype->set_cheat(addr, value, true);
}

void debug_clear_cheat(uint64_t addr)
{
	our_rom.rtype->set_cheat(addr, 0, false);
}

void debug_setxmask(uint64_t mask)
{
	xmask = mask;
}

void debug_tracelog(uint64_t proc, const std::string& filename)
{
	if(filename == "") {
		if(!trace_outputs.count(proc))
			return;
		debug_remove_callback(proc, DEBUG_TRACE, trace_outputs[proc].second);
		trace_outputs[proc].first->refcnt--;
		if(!trace_outputs[proc].first->refcnt) {
			delete trace_outputs[proc].first;
		}
		trace_outputs.erase(proc);
		messages << "Stopped tracelogging processor #" << proc << std::endl;
		if(tracelog_change_cb) tracelog_change_cb();
		return;
	}
	if(trace_outputs.count(proc)) throw std::runtime_error("Already tracelogging");
	std::string full_filename = get_absolute_path(filename);
	bool found = false;
	for(auto i : trace_outputs) {
		if(i.second.first->full_filename == full_filename) {
			i.second.first->refcnt++;
			trace_outputs[proc].first = i.second.first;
			found = true;
			break;
		}
	}
	if(!found) {
		trace_outputs[proc].first = new tracelog_file;
		trace_outputs[proc].first->refcnt = 1;
		trace_outputs[proc].first->full_filename = full_filename;
		trace_outputs[proc].first->stream.open(full_filename);
		if(!trace_outputs[proc].first->stream) {
			delete trace_outputs[proc].first;
			trace_outputs.erase(proc);
			throw std::runtime_error("Can't open '" + full_filename + "'");
		}
	}
	trace_outputs[proc].second = debug_add_trace_callback(proc, [](uint64_t proc, const char* str, bool dummy) {
		if(!trace_outputs.count(proc)) return;
		trace_outputs[proc].first->stream << str << std::endl;
	}, [proc]() { debug_tracelog(proc, ""); });
	messages << "Tracelogging processor #" << proc << " to '" << filename << "'" << std::endl;
	if(tracelog_change_cb) tracelog_change_cb();
}

bool debug_tracelogging(uint64_t proc)
{
	return (trace_outputs.count(proc) != 0);
}

void debug_set_tracelog_change_cb(std::function<void()> cb)
{
	tracelog_change_cb = cb;
}

void debug_core_change()
{
	our_rom.rtype->debug_reset();
	kill_hooks(read_cb);
	kill_hooks(write_cb);
	kill_hooks(exec_cb);
	kill_hooks(trace_cb);
}

void debug_request_break()
{
	requesting_break = true;
}

namespace
{
	command::fnptr<> callbacks_show(lsnes_cmd, "show-callbacks", "", "",
		[]() throw(std::bad_alloc, std::runtime_error) {
		for(auto& i : read_cb)
			for(auto& j : i.second)
				messages << "READ addr=" << i.first << " handle=" << &j << std::endl;
		for(auto& i : write_cb)
			for(auto& j : i.second)
				messages << "WRITE addr=" << i.first << " handle=" << &j << std::endl;
		for(auto& i : exec_cb)
			for(auto& j : i.second)
				messages << "EXEC addr=" << i.first << " handle=" << &j << std::endl;
		for(auto& i : trace_cb)
			for(auto& j : i.second)
				messages << "TRACE proc=" << i.first << " handle=" << &j << std::endl;
	});

	command::fnptr<const std::string&> generate_event(lsnes_cmd, "generate-memory-event", "", "",
		[](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
		regex_results r = regex("([^ \t]+) ([^ \t]+) (.+)", args);
		if(!r) throw std::runtime_error("generate-memory-event: Bad arguments");
		if(r[1] == "r") {
			uint64_t addr = parse_value<uint64_t>(r[2]);
			uint64_t val = parse_value<uint64_t>(r[3]);
			debug_fire_callback_read(addr, val);
		} else if(r[1] == "w") {
			uint64_t addr = parse_value<uint64_t>(r[2]);
			uint64_t val = parse_value<uint64_t>(r[3]);
			debug_fire_callback_write(addr, val);
		} else if(r[1] == "x") {
			uint64_t addr = parse_value<uint64_t>(r[2]);
			uint64_t val = parse_value<uint64_t>(r[3]);
			debug_fire_callback_exec(addr, val);
		} else if(r[1] == "t") {
			uint64_t proc = parse_value<uint64_t>(r[2]);
			std::string str = r[3];
			debug_fire_callback_trace(proc, str.c_str());
		} else
			throw std::runtime_error("Invalid operation");
	});

	command::fnptr<const std::string&> tracelog(lsnes_cmd, "tracelog", "Trace log control",
		"Trace log control\nSyntax: tracelog <cpuid> <file>  Start tracing\nSyntax: tracelog <cpuid>  "
		"End tracing", [](const std::string& args) throw(std::bad_alloc, std::runtime_error) {
		regex_results r = regex("([^ \t]+)([ \t]+(.+))?", args);
		if(!r) throw std::runtime_error("tracelog: Bad arguments");
		std::string cpu = r[1];
		std::string filename = r[3];
		uint64_t _cpu = 0;
		for(auto i : our_rom.rtype->get_trace_cpus()) {
			if(cpu == i)
				goto out;
			_cpu++;
		}
		throw std::runtime_error("tracelog: Invalid CPU");
out:
		debug_tracelog(_cpu, filename);
	});

}
