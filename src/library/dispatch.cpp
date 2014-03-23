#include "dispatch.hpp"

namespace dispatch
{
threads::lock& global_init_lock()
{
	static threads::lock m;
	return m;
}
}
