#ifndef _library_threads__hpp__included__
#define _library_threads__hpp__included__

#include <cstdint>
#include <vector>

#ifdef NATIVE_THREADS
#include <thread>
#include <condition_variable>
#include <mutex>
#else
#include <boost/thread.hpp>
#include <boost/thread/locks.hpp>
#endif

namespace threads
{
#ifdef NATIVE_THREADS
typedef std::thread thread;
typedef std::condition_variable cv;
typedef std::mutex lock;
typedef std::recursive_mutex rlock;
typedef std::unique_lock<std::mutex> alock;
typedef std::unique_lock<std::recursive_mutex> arlock;
typedef std::chrono::microseconds ustime;
typedef std::thread::id id;
inline void cv_timed_wait(cv& c, alock& m, const ustime& t)
{
	c.wait_for(m, t);
}
inline id this_id()
{
	return std::this_thread::get_id();
}
#else
typedef boost::thread thread;
typedef boost::condition_variable cv;
typedef boost::mutex lock;
typedef boost::recursive_mutex rlock;
typedef boost::unique_lock<boost::mutex> alock;
typedef boost::unique_lock<boost::recursive_mutex> arlock;
typedef boost::posix_time::microseconds ustime;
typedef boost::thread::id id;
inline void cv_timed_wait(cv& c, alock& m, const ustime& t)
{
	c.timed_wait(m, t);
}
inline id this_id()
{
	return boost::this_thread::get_id();
}
#endif

/**
 * Lock multiple locks.
 *
 * The locks are always locked in address order. Duplicate locks are only locked once.
 */
void lock_multiple(std::initializer_list<lock*> locks);
/**
 * Unlock multiple locks.
 *
 * Duplicate locks are only unlocked once.
 */
void unlock_multiple(std::initializer_list<lock*> locks);
void unlock_multiple(std::vector<lock*> locks);

class alock_multiple
{
public:
	alock_multiple(std::initializer_list<lock*> locks)
	{
		_locks = std::vector<lock*>(locks);
		lock_multiple(locks);
	}
	~alock_multiple()
	{
		unlock_multiple(_locks);
	}
private:
	std::vector<lock*> _locks;
};
}

#endif
