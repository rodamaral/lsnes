#include "dispatch.hpp"

namespace dispatch
{
mutex_class& global_init_lock()
{
	static mutex_class m;
	return m;
}
}
