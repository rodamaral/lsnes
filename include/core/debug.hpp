#ifndef _debug__hpp__included__
#define _debug__hpp__included__

#include <functional>
#include <fstream>
#include <cstdint>
#include "library/command.hpp"
#include "library/dispatch.hpp"

class emulator_dispatch;
class loaded_rom;
class memory_space;

/**
 * Debugging context.
 */
class debug_context
{
public:
	debug_context(emulator_dispatch& _dispatch, loaded_rom& _rom, memory_space& _mspace, command::group& _cmd);
/**
 * Type of event.
 */
	enum etype
	{
		DEBUG_READ,
		DEBUG_WRITE,
		DEBUG_EXEC,
		DEBUG_TRACE,
		DEBUG_FRAME,
	};
/**
 * Parameters for read/write/execute event.
 */
	struct params_rwx
	{
		uint64_t addr;			//Address.
		uint64_t value;			//Value/CPU.
	};
/**
 * Parameters for trace event.
 */
	struct params_trace
	{
		uint64_t cpu;			//CPU number.
		const char* decoded_insn;	//Decoded instruction
		bool true_insn;			//True instruction flag.
	};
/**
 * Parameters for frame event.
 */
	struct params_frame
	{
		uint64_t frame;			//Frame number.
		bool loadstated;		//Loadstate flag.
	};
/**
 * Parameters for debug callback.
 */
	struct params
	{
		etype type;
		union {
			params_rwx rwx;		//READ/WRITE/EXECUTE
			params_trace trace;	//TRACE.
			params_frame frame;	//FRAME.
		};
	};
/**
 * Base class of debugging callbacks.
 */
	struct callback_base
	{
/**
 * Destructor.
 */
		virtual ~callback_base();
/**
 * Do a callback.
 */
		virtual void callback(const params& p) = 0;
/**
 * Notify about killed callback.
 */
		virtual void killed(uint64_t addr, etype type) = 0;
	};
/**
 * Placeholder for all addresses.
 */
	static const uint64_t all_addresses;
/**
 * Add a callback.
 */
	void add_callback(uint64_t addr, etype type, callback_base& cb);
/**
 * Remove a callback.
 */
	void remove_callback(uint64_t addr, etype type, callback_base& cb);
/**
 * Fire a read callback.
 */
	void do_callback_read(uint64_t addr, uint64_t value);
/**
 * Fire a write callback.
 */
	void do_callback_write(uint64_t addr, uint64_t value);
/**
 * Fire a exec callback.
 */
	void do_callback_exec(uint64_t addr, uint64_t value);
/**
 * Fire a trace callback.
 */
	void do_callback_trace(uint64_t cpu, const char* str, bool true_insn = true);
/**
 * Fire a frame callback.
 */
	void do_callback_frame(uint64_t frame, bool loadstate);
/**
 * Set a cheat.
 */
	void set_cheat(uint64_t addr, uint64_t value);
/**
 * Clear a cheat.
 */
	void clear_cheat(uint64_t addr);
/**
 * Set execute callback mask.
 */
	void setxmask(uint64_t mask);
/**
 * Set tracelog file.
 */
	void tracelog(uint64_t cpu, const std::string& filename);
/**
 * Tracelogging on?
 */
	bool is_tracelogging(uint64_t cpu);
/**
 * Change tracelog change callback.
 */
	void set_tracelog_change_cb(std::function<void()> cb);
/**
 * Notify a core change.
 */
	void core_change();
/**
 * Request a break.
 */
	void request_break();
	//These are public only for some debugging stuff.
	typedef std::list<callback_base*> cb_list;
	std::map<uint64_t, cb_list> read_cb;
	std::map<uint64_t, cb_list> write_cb;
	std::map<uint64_t, cb_list> exec_cb;
	std::map<uint64_t, cb_list> trace_cb;
	std::map<uint64_t, cb_list> frame_cb;
private:
	void do_showhooks();
	void do_genevent(const std::string& a);
	void do_tracecmd(const std::string& a);
	cb_list dummy_cb;  //Always empty.
	uint64_t xmask = 1;
	std::function<void()> tracelog_change_cb;
	emulator_dispatch& edispatch;
	loaded_rom& rom;
	memory_space& mspace;
	command::group& cmd;
	struct dispatch::target<> corechange;
	bool corechange_r = false;
	bool requesting_break = false;
	command::_fnptr<> showhooks;
	command::_fnptr<const std::string&> genevent;
	command::_fnptr<const std::string&> tracecmd;

	struct tracelog_file : public callback_base
	{
		std::ofstream stream;
		std::string full_filename;
		unsigned refcnt;
		tracelog_file(debug_context& parent);
		~tracelog_file();
		void callback(const params& p);
		void killed(uint64_t addr, etype type);
	private:
		debug_context& parent;
	};
	std::map<uint64_t, tracelog_file*> trace_outputs;

	std::map<uint64_t, cb_list>& get_lists(etype type)
	{
		switch(type) {
		case DEBUG_READ: return read_cb;
		case DEBUG_WRITE: return write_cb;
		case DEBUG_EXEC: return exec_cb;
		case DEBUG_TRACE: return trace_cb;
		case DEBUG_FRAME: return frame_cb;
		default: throw std::runtime_error("Invalid debug callback type");
		}
	}
};

#endif
