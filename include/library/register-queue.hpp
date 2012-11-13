#ifndef _library__register_queue__hpp__included__
#define _library__register_queue__hpp__included__

#include "threadtypes.hpp"
#include <string>
#include <list>
#include <set>

/**
 * Object registration queue.
 */
template<class G, class O>
class register_queue
{
public:
/**
 * Register an object.
 *
 * Parameter group: The group to register to.
 * Parameter name: The name for object.
 * Parameter object: The object to register.
 */
	static void do_register(G& group, const std::string& name, O& object)
	{
		{
			umutex_class h(get_mutex());
			if(get_ready().count(&group)) {
				group.do_register(name, object);
				return;
			}
			queue_entry<G, O> ent;
			ent.group = &group;
			ent.name = name;
			ent.object = &object;
			get_pending().push_back(ent);
		}
		run();
	}
/**
 * Unregister an object.
 *
 * Parameter group: The group to unregister from.
 * Parameter name: The name for object.
 */
	static void do_unregister(G& group, const std::string& name)
	{
		umutex_class h(get_mutex());
		auto& x = get_pending();
		auto i = x.begin();
		while(i != x.end()) {
			auto e = i++;
			if(&group == e->group && name == e->name)
				x.erase(e);
		}
		if(get_ready().count(&group))
			group.do_unregister(name);
	}
/**
 * Mark group ready/not ready.
 *
 * Parameter group: The group.
 * Parameter ready: True if ready, false if not.
 */
	static void do_ready(G& group, bool ready)
	{
		{
			umutex_class h(get_mutex());
			if(ready)
				get_ready().insert(&group);
			else
				get_ready().erase(&group);
		}
		run();
	}
private:
	template<class G2, class O2>
	struct queue_entry
	{
		G2* group;
		std::string name;
		O2* object;
	};
	static std::set<G*>& get_ready()
	{
		static std::set<G*> x;
		return x;
	}
	static std::list<queue_entry<G, O>>& get_pending()
	{
		static std::list<queue_entry<G, O>> x;
		return x;
	}
	static mutex_class& get_mutex()
	{
		static mutex_class x;
		return x;
	}
	static void run()
	{
		umutex_class h(get_mutex());
		auto& x = get_pending();
		auto i = x.begin();
		while(i != x.end()) {
			auto e = i++;
			if(get_ready().count(e->group)) {
				e->group->do_register(e->name, *e->object);
				x.erase(e);
			}
		}
	}
};

#endif
