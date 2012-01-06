#include "core/window.hpp"

#include <wx/thread.h>

struct wxw_mutex : public mutex
{
	wxw_mutex() throw(std::bad_alloc);
	~wxw_mutex() throw();
	void lock() throw();
	void unlock() throw();
	wxMutex* m;
};

wxw_mutex::wxw_mutex() throw(std::bad_alloc)
{
	m = new wxMutex();
}

wxw_mutex::~wxw_mutex() throw()
{
	delete m;
}

void wxw_mutex::lock() throw()
{
	m->Lock();
}

void wxw_mutex::unlock() throw()
{
	m->Unlock();
}

mutex& mutex::aquire() throw(std::bad_alloc)
{
	return *new wxw_mutex;
}

struct wxw_condition : public condition
{
	wxw_condition(mutex& m) throw(std::bad_alloc);
	~wxw_condition() throw();
	bool wait(uint64_t x) throw();
	void signal() throw();
	wxCondition* c;
};

wxw_condition::wxw_condition(mutex& m) throw(std::bad_alloc)
	: condition(m)
{
	wxw_mutex* m2 = dynamic_cast<wxw_mutex*>(&m);
	c = new wxCondition(*m2->m);
}

wxw_condition::~wxw_condition() throw()
{
	delete c;
}

bool wxw_condition::wait(uint64_t x) throw()
{
	wxCondError e = c->WaitTimeout((x + 999) / 1000);
	return (e == wxCOND_NO_ERROR);
}

void wxw_condition::signal() throw()
{
	c->Broadcast();
}

condition& condition::aquire(mutex& m) throw(std::bad_alloc)
{
	return *new wxw_condition(m);
}

struct wxw_thread_id : public thread_id
{
	wxw_thread_id() throw();
	~wxw_thread_id() throw();
	bool is_me() throw();
	uint32_t id;
};

wxw_thread_id::wxw_thread_id() throw()
{
	id = wxThread::GetCurrentId();
}

wxw_thread_id::~wxw_thread_id() throw()
{
}

bool wxw_thread_id::is_me() throw()
{
	return (id == wxThread::GetCurrentId());
}

thread_id& thread_id::me() throw(std::bad_alloc)
{
	return *new wxw_thread_id;
}

struct wxw_thread : public thread, wxThread
{
	wxw_thread(void* (*fn)(void* arg), void* arg) throw(std::bad_alloc, std::runtime_error);
	~wxw_thread() throw();
	void _join() throw();
	void* (*entry)(void* arg);
	void* entry_arg;
	bool has_waited;
	wxThread::ExitCode Entry();
};

wxThread::ExitCode wxw_thread::Entry()
{
	notify_quit(entry(entry_arg));
	Exit();
}

wxw_thread::wxw_thread(void* (*fn)(void* arg), void* arg) throw(std::bad_alloc, std::runtime_error)
	: wxThread(wxTHREAD_JOINABLE)
{
	entry = fn;
	entry_arg = arg;
	has_waited = false;
	wxThreadError e = Create(8 * 1024 * 1024);
	if(e != wxTHREAD_NO_ERROR)
		throw std::bad_alloc();
	e = Run();
	if(e != wxTHREAD_NO_ERROR)
		throw std::bad_alloc();
}

wxw_thread::~wxw_thread() throw()
{
	_join();
}

void wxw_thread::_join() throw()
{
	if(!has_waited)
		Wait();
	has_waited = true;
}

thread& thread::create(void* (*entrypoint)(void* arg), void* arg) throw(std::bad_alloc, std::runtime_error)
{
	return *new wxw_thread(entrypoint, arg);
}
