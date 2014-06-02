#ifndef _instance_map__hpp__included__
#define _instance_map__hpp__included__

#include <map>

class emulator_instance;

template<typename T> class instance_map
{
public:
/**
 * Destroy a instance map.
 */
	~instance_map()
	{
		for(auto i : instances)
			delete i.second;
		instances.clear();
	}
/**
 * Does this instance exist?
 */
	bool exists(emulator_instance& inst)
	{
		return instances.count(&inst);
	}
/**
 * Lookup a instance. Returns NULL if none.
 */
	T* lookup(emulator_instance& inst)
	{
		if(!instances.count(&inst))
			return NULL;
		return instances[&inst];
	}
/**
 * Create a new instance.
 */
	template<typename... U> T* create(emulator_instance& inst, U... args)
	{
		T* out = NULL;
		try {
			out = new T(inst, args...);
			instances[&inst] = out;
			return out;
		} catch(...) {
			if(out) delete out;
			throw;
		}
	}
/**
 * Erase a instance.
 */
	void destroy(emulator_instance& inst)
	{
		if(instances.count(&inst)) {
			delete instances[&inst];
			instances.erase(&inst);
		}
	}
/**
 * Remove a instance.
 */
	void remove(emulator_instance& inst)
	{
		if(instances.count(&inst)) {
			instances.erase(&inst);
		}
	}
private:
	std::map<emulator_instance*, T*> instances;
};

#endif
