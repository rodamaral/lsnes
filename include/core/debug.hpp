#ifndef _debug__hpp__included__
#define _debug__hpp__included__

#include <functional>
#include <fstream>
#include <cstdint>
#include "library/dispatch.hpp"

enum debug_type
{
	DEBUG_READ,
	DEBUG_WRITE,
	DEBUG_EXEC,
	DEBUG_TRACE,
	DEBUG_FRAME,
};

struct debug_callback_params_rwx
{
	uint64_t addr;			//Address.
	uint64_t value;			//Value/CPU.
};

struct debug_callback_params_trace
{
	uint64_t cpu;			//CPU number.
	const char* decoded_insn;	//Decoded instruction
	bool true_insn;			//True instruction flag.
};

struct debug_callback_params_frame
{
	uint64_t frame;			//Frame number.
	bool loadstated;		//Loadstate flag.
};

struct debug_callback_params
{
	debug_type type;
	union {
		debug_callback_params_rwx rwx;		//READ/WRITE/EXECUTE
		debug_callback_params_trace trace;	//TRACE.
		debug_callback_params_frame frame;	//FRAME.
	};
};

struct debug_callback_base
{
/**
 * Destructor.
 */
	virtual ~debug_callback_base();
/**
 * Do a callback.
 */
	virtual void callback(const debug_callback_params& params) = 0;
/**
 * Notify about killed callback.
 */
	virtual void killed(uint64_t addr, debug_type type) = 0;
};

/**
 * Debugging context.
 */
class debug_context
{
public:
/**
 * Placeholder for all addresses.
 */
	static const uint64_t all_addresses;
/**
 * Add a callback.
 */
	void add_callback(uint64_t addr, debug_type type, debug_callback_base& cb);
/**
 * Remove a callback.
 */
	void remove_callback(uint64_t addr, debug_type type, debug_callback_base& cb);
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
	typedef std::list<debug_callback_base*> cb_list;
	std::map<uint64_t, cb_list> read_cb;
	std::map<uint64_t, cb_list> write_cb;
	std::map<uint64_t, cb_list> exec_cb;
	std::map<uint64_t, cb_list> trace_cb;
	std::map<uint64_t, cb_list> frame_cb;
private:
	cb_list dummy_cb;  //Always empty.
	uint64_t xmask = 1;
	std::function<void()> tracelog_change_cb;
	struct dispatch::target<> corechange;
	bool corechange_r = false;
	bool requesting_break = false;

	struct tracelog_file : public debug_callback_base
	{
		std::ofstream stream;
		std::string full_filename;
		unsigned refcnt;
		tracelog_file(debug_context& parent);
		~tracelog_file();
		void callback(const debug_callback_params& p);
		void killed(uint64_t addr, debug_type type);
	private:
		debug_context& parent;
	};
	std::map<uint64_t, tracelog_file*> trace_outputs;

	std::map<uint64_t, cb_list>& get_lists(debug_type type)
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
