#ifndef _library__map_pointer__hpp__
#define _library__map_pointer__hpp__

template<typename T> struct map_pointer
{
	map_pointer()
	{
		p = NULL;
	}
	map_pointer& operator=(T* ptr)
	{
		p = ptr;
		return *this;
	}
	~map_pointer()
	{
		if(p)
			delete p;
	}
	T& operator*()
	{
		return *p;
	}
	T* operator->()
	{
		return p;
	}
	map_pointer(const map_pointer& q)
	{
		if(q.p)
			throw std::runtime_error("Bad map pointer copy");
		p = NULL;
	}
private:
	map_pointer& operator=(const map_pointer&);
	T* p;
};

#endif
