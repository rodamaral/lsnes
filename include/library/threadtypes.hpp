#ifndef _library_threadtypes__hpp__included__
#define _library_threadtypes__hpp__included__

#include <cstdint>

#ifdef NATIVE_THREADS
#include <thread>
#include <condition_variable>
#include <mutex>
typedef std::thread thread_class;
typedef std::condition_variable cv_class;
typedef std::mutex mutex_class;
typedef std::unique_lock<std::mutex> umutex_class;
typedef std::chrono::microseconds microsec_class;
inline void cv_timed_wait(cv_class& c, umutex_class& m, const microsec_class& t)
{
	c.wait_for(m, t);
}
#else
#include <boost/thread.hpp>
#include <boost/thread/locks.hpp>
typedef boost::thread thread_class;
typedef boost::condition_variable cv_class;
typedef boost::mutex mutex_class;
typedef boost::unique_lock<boost::mutex> umutex_class;
typedef boost::posix_time::microseconds microsec_class;
inline void cv_timed_wait(cv_class& c, umutex_class& m, const microsec_class& t)
{
	c.timed_wait(m, t);
}
#endif

#endif
