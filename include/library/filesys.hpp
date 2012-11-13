#ifndef _library__filesys__hpp__included
#define _library__filesys__hpp__included

#include <cstdint>
#include <map>
#include <string>
#include <fstream>
#include "threadtypes.hpp"

#define CLUSTER_SIZE 8192
#define CLUSTERS_PER_SUPER (CLUSTER_SIZE / 4)
#define SUPERCLUSTER_SIZE (static_cast<uint64_t>(CLUSTER_SIZE) * CLUSTERS_PER_SUPER)
#define FILESYSTEM_SUPERBLOCK 1
#define FILESYSTEM_ROOTDIR 2

class filesystem
{
public:
	//Create a new or open existing filesystem backed by specified file.
	filesystem(const std::string& backingfile);
	//Allocate a new file.
	uint32_t allocate_cluster();
	//Delete specified file.
	void free_cluster_chain(uint32_t cluster);
	//Skip specfied amount of data. If skip goes to end of file, ptr will be left at CLUSTER_SIZE.
	size_t skip_data(uint32_t& cluster, uint32_t& ptr, uint32_t length);
	//Read specified amount of data. If read goes to end of file, ptr will be left at CLUSTER_SIZE.
	//Returns the number of bytes read.
	size_t read_data(uint32_t& cluster, uint32_t& ptr, void* data, uint32_t length);
	//Write specified amount of data. real_cluster and real_ptr point to location data was written to.
	//If write exactly fills the last cluster, ptr will be left at CLUSTER_SIZE.
	void write_data(uint32_t& cluster, uint32_t& ptr, const void* data, uint32_t length,
		uint32_t& real_cluster, uint32_t& real_ptr);
private:
	filesystem(const filesystem&);
	filesystem& operator=(const filesystem&);
	void link_cluster(uint32_t cluster, uint32_t linkto);
	struct supercluster
	{
		unsigned free_clusters;
		uint32_t clusters[CLUSTERS_PER_SUPER];
		void load(std::fstream& s, uint32_t index);
		void save(std::fstream& s, uint32_t index);
	};
	uint32_t supercluster_count;
	std::map<uint32_t, supercluster> superclusters;
	std::fstream backing;
};

class filesystem_ref
{
public:
	filesystem_ref()
	{
		refcnt = NULL;
		mutex = NULL;
		fs = NULL;
	}
	filesystem_ref(const std::string& backingfile)
	{
		refcnt = NULL;
		mutex = NULL;
		try {
			refcnt = new unsigned;
			mutex = new mutex_class;
			fs = new filesystem(backingfile);
		} catch(...) {
			delete refcnt;
			delete mutex;
			throw;
		}
		*refcnt = 1;
	}
	~filesystem_ref()
	{
		mutex_class* mtodelete = NULL;
		if(!mutex)
			return;
		{
			umutex_class m(*mutex);
			--*refcnt;
			if(!*refcnt) {
				delete fs;
				delete refcnt;
				mtodelete = mutex;
			}
		}
		if(mtodelete)
			delete mtodelete;
	}
	filesystem_ref(const filesystem_ref& r)
	{
		umutex_class m(*r.mutex);
		++*(r.refcnt);
		refcnt = r.refcnt;
		mutex = r.mutex;
		fs = r.fs;
	}
	filesystem_ref& operator=(const filesystem_ref& r)
	{
		if(this == &r)
			return *this;
		//This is tricky, due to having to lock two objects.
		size_t A = (size_t)this;
		size_t B = (size_t)&r;
		mutex_class* mtodelete = NULL;
		if(!refcnt) {
			//We just have to grab a ref and copy.
			umutex_class m(*r.mutex);
			++*(r.refcnt);
			refcnt = r.refcnt;
			mutex = r.mutex;
			fs = r.fs;
		} else if(A < B) {
			//Two-object case.
			umutex_class m1(*mutex);
			umutex_class m2(*r.mutex);
			--*refcnt;
			if(!*refcnt) {
				delete fs;
				delete refcnt;
				mtodelete = mutex;;
			}
			++*(r.refcnt);
			refcnt = r.refcnt;
			mutex = r.mutex;
			fs = r.fs;
		} else {
			//Two-object case.
			umutex_class m1(*r.mutex);
			umutex_class m2(*mutex);
			--*refcnt;
			if(!*refcnt) {
				delete fs;
				delete refcnt;
				mtodelete = mutex;;
			}
			++*(r.refcnt);
			refcnt = r.refcnt;
			mutex = r.mutex;
			fs = r.fs;
		}
		if(mtodelete)
			delete mtodelete;
		return *this;
	}
	uint32_t allocate_cluster()
	{
		umutex_class m(*mutex);
		return fs->allocate_cluster();
	}
	void free_cluster_chain(uint32_t cluster)
	{
		umutex_class m(*mutex);
		fs->free_cluster_chain(cluster);
	}
	size_t skip_data(uint32_t& cluster, uint32_t& ptr, uint32_t length)
	{
		umutex_class m(*mutex);
		return fs->skip_data(cluster, ptr, length);
	}
	size_t read_data(uint32_t& cluster, uint32_t& ptr, void* data, uint32_t length)
	{
		umutex_class m(*mutex);
		return fs->read_data(cluster, ptr, data, length);
	}
	void write_data(uint32_t& cluster, uint32_t& ptr, const void* data, uint32_t length,
		uint32_t& real_cluster, uint32_t& real_ptr)
	{
		umutex_class m(*mutex);
		fs->write_data(cluster, ptr, data, length, real_cluster, real_ptr);
	}
private:
	filesystem* fs;
	unsigned* refcnt;
	mutex_class* mutex;
};

#endif
