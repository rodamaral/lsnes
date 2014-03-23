#ifndef _library_threads__hpp__included__
#define _library_threads__hpp__included__

#include <cstdint>

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
typedef std::unique_lock<std::mutex> alock;
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
typedef boost::unique_lock<boost::mutex> alock;
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
}

#endif
