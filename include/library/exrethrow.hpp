#ifndef _library__exrethrow__hpp__included__
#define _library__exrethrow__hpp__included__

#include <stdexcept>
#include <functional>

namespace exrethrow
{
/**
 * Add a exception type.
 */
void add_ex_spec(unsigned prio, std::function<bool(std::exception& e)> identify,
	std::function<void()> (*throwfn)(std::exception& e));
std::function<void()> get_throw_fn(std::exception& e);

/**
 * Exception type specifier.
 */
template<typename T, unsigned prio> class ex_spec
{
public:
	ex_spec()
	{
		add_ex_spec(prio, [](std::exception& e) -> bool { return (dynamic_cast<T*>(&e) != NULL); },
			[](std::exception& e) -> std::function<void()>  { T _ex = *dynamic_cast<T*>(&e);
			return [_ex]() -> void { throw _ex; }; });
	}
};

/**
 * Exception storage
 */
class storage
{
public:
/**
 * Null object.
 */
	storage();
/**
 * Store an exception.
 *
 * Parameter e: The exception.
 */
	storage(std::exception& e);
/**
 * Rethrow the exception.
 */
	void rethrow();
/**
 * Is anything here?
 */
	operator bool();
private:
	bool null;
	bool oom;
	std::function<void()> do_rethrow;
};
}

#endif
