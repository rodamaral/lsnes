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
#else
#include <boost/thread.hpp>
#include <boost/thread/locks.hpp>
typedef boost::thread thread_class;
typedef boost::condition_variable cv_class;
typedef boost::mutex mutex_class;
typedef boost::unique_lock<boost::mutex> umutex_class;
#endif

#endif
