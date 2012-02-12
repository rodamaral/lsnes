#ifndef _library_workthread__hpp__included__
#define _library_workthread__hpp__included__

#include <cstdint>

#ifdef NATIVE_THREADS
#include <thread>
#include <condition_variable>
#include <mutex>
typedef std::thread thread_class;
typedef std::condition_variable cv_class;
typedef std::mutex mutex_class;
typedef std::unique_lock<std::mutex> umutex_class;
#else
#include <boost/thread.hpp>
#include <boost/thread/locks.hpp>
typedef boost::thread thread_class;
typedef boost::condition_variable cv_class;
typedef boost::mutex mutex_class;
typedef boost::unique_lock<boost::mutex> umutex_class;
#endif

#define WORKFLAG_QUIT_REQUEST 0x80000000U

class worker_thread_reflector;

/**
 * A worker thread.
 *
 * Note: All methods (except entrypoints) are thread-safe.
 */
class worker_thread
{
public:
/**
 * Constructor.
 */
	worker_thread();
/**
 * Destructor.
 */
	~worker_thread();
/**
 * Request quit. Sets quit request workflag.
 */
	void request_quit();
/**
 * Set the busy flag.
 */
	void set_busy();
/**
 * Clear the busy flag.
 */
	void clear_busy();
/**
 * Wait until busy flag cleared.
 */
	void wait_busy();
/**
 * Rethrow caught exception if any.
 */
	void rethrow();
/**
 * Set work flag.
 *
 * Parameter flag: The flags to set.
 */
	void set_workflag(uint32_t flag);
/**
 * Clear work flag.
 *
 * Parameter flag: Work flags to clear.
 * Returns: The workflags before clearing.
 */
	uint32_t clear_workflag(uint32_t flag);
/**
 * Wait until work flags nonzero.
 *
 * Returns: Current work flags.
 */
	uint32_t wait_workflag();
/**
 * Thread raw entrypoint.
 *
 * Note: Don't call from outside workthread code.
 */
	int operator()(int dummy);
protected:
/**
 * Thread entrypoint.
 *
 * Notes: Exceptions thrown are catched.
 */
	virtual void entry() = 0;
/**
 * Start actually running the thread.
 */
	void fire();
private:
	thread_class* thread;
	worker_thread_reflector* reflector;
	cv_class condition;
	mutex_class mutex;
	volatile bool joined;
	volatile uint32_t workflag;
	volatile bool busy;
	volatile bool exception_caught;
	volatile bool exception_oom;
	std::string exception_text;
};

#endif
