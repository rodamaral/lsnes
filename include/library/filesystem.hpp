#ifndef _library__filesystem__hpp__included
#define _library__filesystem__hpp__included

#include <cstdint>
#include <map>
#include <string>
#include <fstream>
#include "threads.hpp"

#define CLUSTER_SIZE 8192
#define CLUSTERS_PER_SUPER (CLUSTER_SIZE / 4)
#define SUPERCLUSTER_SIZE (static_cast<uint64_t>(CLUSTER_SIZE) * CLUSTERS_PER_SUPER)
#define FILESYSTEM_SUPERBLOCK 1
#define FILESYSTEM_ROOTDIR 2

/**
 * A filesystem.
 */
class filesystem
{
public:
/**
 * Create a new filesystem or open existing one, backed by specified file.
 *
 * Parameters backingfile: The backing file name.
 */
	filesystem(const std::string& backingfile);
/**
 * Allocate a new file.
 *
 * Returns: The initial cluster for the new file.
 */
	uint32_t allocate_cluster();
/**
 * Delete a file.
 *
 * Parameter cluster: The initial cluster of file to delete.
 */
	void free_cluster_chain(uint32_t cluster);
/**
 * Read and discard specified amount of data.
 *
 * Parameter cluster: Cluster to start the read from. Updated to be cluster for following data.
 * Parameter ptr: The offset in cluster. Updated to be offset for following data.
 * Parameter length: The length to read.
 * Returns: The actual amount read. Can only be smaller than length if EOF was seen first.
 *
 * Note: If this runs off the end of file, cluster will be left pointing to last cluster in file and ptr will be
 *	left at CLUSTER_SIZE.
 */
	size_t skip_data(uint32_t& cluster, uint32_t& ptr, uint32_t length);
/**
 * Read specified amount of data.
 *
 * Parameter cluster: Cluster to start the read from. Updated to be cluster for following data.
 * Parameter ptr: The offset in cluster. Updated to be offset for following data.
 * Parameter data: The buffer to store the read data to.
 * Parameter length: The length to read.
 * Returns: The actual amount read. Can only be smaller than length if EOF was seen first.
 *
 * Note: If this runs off the end of file, cluster will be left pointing to last cluster in file and ptr will be
 *	left at CLUSTER_SIZE.
 */
	size_t read_data(uint32_t& cluster, uint32_t& ptr, void* data, uint32_t length);
/**
 * Write specified amount of data.
 *
 * Parameter cluster: Cluster to start the write from. Update to be cluster for following data.
 * Parameter ptr: The offset in cluster. Updated to be offset for following data.
 * Parameter data: The buffer to read the data to write.
 * Parameter length: The length to write.
 * Parameter real_cluster: The real cluster the write started from is stored here.
 * Parameter real_ptr: The real offset the write started from is stored here.
 *
 * Note: If the write exactly fills the last cluster, ptr will be left as CLUSTER_SIZE.
 */
	void write_data(uint32_t& cluster, uint32_t& ptr, const void* data, uint32_t length,
		uint32_t& real_cluster, uint32_t& real_ptr);
/**
 * A reference-counted refernece to a filesystem.
 */
	class ref
	{
	public:
/**
 * Create a reference to NULL filesystem.
 */
		ref()
		{
			refcnt = NULL;
			mlock = NULL;
			fs = NULL;
		}
/**
 * Create/Open a new filesystem and take reference to that.
 *
 * Parameters backingfile: The backing file.
 */
		ref(const std::string& backingfile)
		{
			refcnt = NULL;
			mlock = NULL;
			try {
				refcnt = new unsigned;
				mlock = new threads::lock;
				fs = new filesystem(backingfile);
			} catch(...) {
				delete refcnt;
				delete mlock;
				throw;
			}
			*refcnt = 1;
		}
/**
 * Destructor.
 */
		~ref()
		{
			threads::lock* mtodelete = NULL;
			if(!mlock)
				return;
			{
				threads::alock m(*mlock);
				--*refcnt;
				if(!*refcnt) {
					delete fs;
					delete refcnt;
					mtodelete = mlock;
				}
			}
			if(mtodelete)
				delete mtodelete;
		}
/**
 * Copy constructor.
 */
		ref(const ref& r)
		{
			threads::alock m(*r.mlock);
			++*(r.refcnt);
			refcnt = r.refcnt;
			mlock = r.mlock;
			fs = r.fs;
		}
/**
 * Assignment operator.
 */
		ref& operator=(const ref& r);
/**
 * Call allocate_cluster() on underlying filesystem.
 *
 * Note: See filesystem::allocate_cluster() for description.
 */
		uint32_t allocate_cluster()
		{
			threads::alock m(*mlock);
			return fs->allocate_cluster();
		}
/**
 * Call free_cluster_chain() on underlying filesystem.
 *
 * Note: See filesystem::free_cluster_chain() for description.
 */
		void free_cluster_chain(uint32_t cluster)
		{
			threads::alock m(*mlock);
			fs->free_cluster_chain(cluster);
		}
/**
 * Call skip_data() on underlying filesystem.
 *
 * Note: See filesystem::skip_data() for description.
 */
		size_t skip_data(uint32_t& cluster, uint32_t& ptr, uint32_t length)
		{
			threads::alock m(*mlock);
			return fs->skip_data(cluster, ptr, length);
		}
/**
 * Call read_data() on underlying filesystem.
 *
 * Note: See filesystem::read_data() for description.
 */
		size_t read_data(uint32_t& cluster, uint32_t& ptr, void* data, uint32_t length)
		{
			threads::alock m(*mlock);
			return fs->read_data(cluster, ptr, data, length);
		}
/**
 * Call write_data() on underlying filesystem.
 *
 * Note: See filesystem::write_data() for description.
 */
		void write_data(uint32_t& cluster, uint32_t& ptr, const void* data, uint32_t length,
			uint32_t& real_cluster, uint32_t& real_ptr)
		{
			threads::alock m(*mlock);
			fs->write_data(cluster, ptr, data, length, real_cluster, real_ptr);
		}
	private:
		filesystem* fs;
		unsigned* refcnt;
		threads::lock* mlock;
	};
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


#endif
