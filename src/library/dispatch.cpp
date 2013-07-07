#include "dispatch.hpp"

mutex_class& dispatch_global_init_lock()
{
	static mutex_class m;
	return m;
}
