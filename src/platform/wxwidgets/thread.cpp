#include "core/window.hpp"

#include <wx/thread.h>

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

struct wxw_thread;

struct wxw_thread_inner : public wxThread
{
	wxw_thread_inner(wxw_thread* _up);
	void* (*entry)(void* arg);
	void* entry_arg;
	wxThread::ExitCode Entry();
	wxw_thread* up;
};

struct wxw_thread : public thread
{
	wxw_thread(void* (*fn)(void* arg), void* arg) throw(std::bad_alloc, std::runtime_error);
	~wxw_thread() throw();
	void _join() throw();
	bool has_waited;
	wxw_thread_inner* inner;
	void notify_quit2(void* ret);
};

wxw_thread_inner::wxw_thread_inner(wxw_thread* _up)
	: wxThread(wxTHREAD_JOINABLE)
{
	up = _up;
}

void wxw_thread::notify_quit2(void* ret)
{
	notify_quit(ret);
}

wxThread::ExitCode wxw_thread_inner::Entry()
{
	up->notify_quit2(entry(entry_arg));
	return 0;
}

wxw_thread::wxw_thread(void* (*fn)(void* arg), void* arg) throw(std::bad_alloc, std::runtime_error)
{
	has_waited = false;
	inner = new wxw_thread_inner(this);
	inner->entry = fn;
	inner->entry_arg = arg;
	wxThreadError e = inner->Create(8 * 1024 * 1024);
	if(e != wxTHREAD_NO_ERROR)
		throw std::bad_alloc();
	e = inner->Run();
	if(e != wxTHREAD_NO_ERROR)
		throw std::bad_alloc();
}

wxw_thread::~wxw_thread() throw()
{
	if(inner) {
		inner->Wait();
		inner = NULL;
	}
}

void wxw_thread::_join() throw()
{
	if(inner) {
		inner->Wait();
		inner = NULL;
	}
}

thread& thread::create(void* (*entrypoint)(void* arg), void* arg) throw(std::bad_alloc, std::runtime_error)
{
	return *new wxw_thread(entrypoint, arg);
}
