#ifndef _globalwrap__hpp__included__
#define _globalwrap__hpp__included__

template<class T>
class globalwrap
{
public:
	T& operator()() throw(std::bad_alloc)
	{
		if(!storage)
			storage = new T();
		return *storage;
	}
private:
	T* storage;
};

#endif
