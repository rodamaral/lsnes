#ifndef _library__gc__hpp__included__
#define _library__gc__hpp__included__

#include <cstdlib>

class garbage_collectable
{
public:
	garbage_collectable();
	virtual ~garbage_collectable();
	void mark_root();
	void unmark_root();
	static void do_gc();
protected:
	virtual void trace() = 0;
	void mark();
private:
	size_t root_count;
	bool reachable;
};

struct gcroot_pointer_object_tag {};
template<class T> class gcroot_pointer
{
public:
	gcroot_pointer()
	{
		ptr = NULL;
	}
	gcroot_pointer(T* obj)
	{
		ptr = obj;
	}
	template<typename... U> gcroot_pointer(gcroot_pointer_object_tag tag, U... args)
	{
		ptr = new T(args...);
	}
	gcroot_pointer(const gcroot_pointer& p)
	{
		if(p.ptr) p.ptr->mark_root();
		ptr = p.ptr;
	}
	gcroot_pointer& operator=(const gcroot_pointer& p)
	{
		if(ptr == p.ptr) return *this;
		if(ptr) ptr->unmark_root();
		if(p.ptr) p.ptr->mark_root();
		ptr = p.ptr;
		return *this;
	}
	~gcroot_pointer()
	{
		if(ptr) ptr->unmark_root();
	}
	operator bool()
	{
		return (ptr != NULL);
	}
	T* operator->() { return ptr; }
	T& operator*() { return *ptr; }
	T* as_pointer() { return ptr; }
private:
	T* ptr;
};

#endif
