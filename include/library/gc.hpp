#ifndef _library__gc__hpp__included__
#define _library__gc__hpp__included__

#include <cstdlib>

namespace GC
{
class item
{
public:
	item();
	virtual ~item();
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

struct obj_tag {};
template<class T> class pointer
{
public:
	pointer()
	{
		ptr = NULL;
	}
	pointer(T* obj)
	{
		ptr = obj;
	}
	template<typename... U> pointer(obj_tag tag, U... args)
	{
		ptr = new T(args...);
	}
	pointer(const pointer& p)
	{
		if(p.ptr) p.ptr->mark_root();
		ptr = p.ptr;
	}
	pointer& operator=(const pointer& p)
	{
		if(ptr == p.ptr) return *this;
		if(ptr) ptr->unmark_root();
		if(p.ptr) p.ptr->mark_root();
		ptr = p.ptr;
		return *this;
	}
	~pointer()
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
}

#endif
