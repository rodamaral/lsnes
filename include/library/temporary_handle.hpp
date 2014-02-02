#ifndef _library__temporary_handle__hpp__included__
#define _library__temporary_handle__hpp__included__

template<typename T>
class temporary_handle
{
public:
	template<typename... A> temporary_handle(A... args)
	{
		ptr = new T(args...);
	}
	~temporary_handle()
	{
		delete ptr;
	}
	T* get()
	{
		return ptr;
	}
	T* operator()()
	{
		T* t = NULL;
		std::swap(t, ptr);
		return t;
	}
private:
	T* ptr;
	temporary_handle(const temporary_handle&);
	temporary_handle& operator=(const temporary_handle&);
};

#endif
