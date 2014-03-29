#include "filesystem.hpp"
#include "minmax.hpp"
#include "serialization.hpp"
#include <cstring>
#include <stdexcept>
#include <iostream>

filesystem::filesystem(const std::string& file)
{
	backing.open(file, std::ios_base::out | std::ios_base::app);
	backing.close();
	backing.open(file, std::ios_base::in | std::ios_base::out | std::ios_base::binary | std::ios_base::ate);
	if(!backing)
		throw std::runtime_error("Can't open file '" + file + "'");
	uint64_t backing_size = backing.tellp();
	backing.seekp(0, std::ios_base::beg);
	if(!backing)
		throw std::runtime_error("Can't get file size.");
	supercluster_count = (backing_size + SUPERCLUSTER_SIZE - 1) / SUPERCLUSTER_SIZE;
	for(unsigned i = 0; i < supercluster_count; i++)
		superclusters[i].load(backing, i);
	if(supercluster_count == 0) {
		allocate_cluster();	//Will allocate cluster 2 (main directory).
		//Write superblock to cluster 1.
		char superblock[CLUSTER_SIZE];
		memset(superblock, 0, CLUSTER_SIZE);
		uint32_t c = 2;
		uint32_t p = 0;
		uint32_t c2, p2;
		write_data(c, p, superblock, CLUSTER_SIZE, c2, p2);
		strcpy(superblock, "sefs-magic");
		c = 1;
		p = 0;
		write_data(c, p, superblock, CLUSTER_SIZE, c2, p2);
	} else {
		//Read superblock from cluster 1.
		char superblock[CLUSTER_SIZE];
		uint32_t c = 1;
		uint32_t p = 0;
		read_data(c, p, superblock, CLUSTER_SIZE);
		if(strcmp(superblock, "sefs-magic"))
			throw std::runtime_error("Bad magic");
	}
}


uint32_t filesystem::allocate_cluster()
{
	for(unsigned i = 0; i < supercluster_count; i++) {
		supercluster& c = superclusters[i];
		if(c.free_clusters)
			for(unsigned j = 0; j < CLUSTERS_PER_SUPER; j++)
				if(!c.clusters[j]) {
					c.clusters[j] = 1;
					c.save(backing, i);
					//Write zeroes over the cluster.
					uint32_t cluster = i * CLUSTERS_PER_SUPER + j;
					char buffer[CLUSTER_SIZE];
					memset(buffer, 0, CLUSTER_SIZE);
					backing.seekp(cluster * CLUSTER_SIZE, std::ios_base::beg);
					backing.write(buffer, CLUSTER_SIZE);
					if(!backing)
						throw std::runtime_error("Can't zero out the new cluster");
					return cluster;
				}
	}
	//Create a new supercluster.
	supercluster& c = superclusters[supercluster_count];
	c.free_clusters = 65535;
	c.clusters[0] = 0xFFFFFFFFU;					//Reserved for cluster table.
	for(unsigned i = 1; i < CLUSTERS_PER_SUPER; i++)
		c.clusters[i] = 0;					//Free.
	if(!supercluster_count)
		c.clusters[1] = 0xFFFFFFFFU;				//Reserved for superblock.
	unsigned j = supercluster_count ? 1 : 2;
	if(!supercluster_count)
		c.free_clusters--;					//The superblock.
	c.clusters[j] = 1;						//End of chain.
	c.save(backing, supercluster_count);
	char blankbuf[2 * CLUSTER_SIZE];
	memset(blankbuf, 0, 2 * CLUSTER_SIZE);
	backing.clear();
	backing.seekp(supercluster_count * SUPERCLUSTER_SIZE + CLUSTER_SIZE, std::ios_base::beg);
	backing.write(blankbuf, supercluster_count ? CLUSTER_SIZE : 2 * CLUSTER_SIZE);
	if(!backing)
		throw std::runtime_error("Can't write new supercluster");
	return (supercluster_count++) * CLUSTERS_PER_SUPER + j;
}

void filesystem::link_cluster(uint32_t cluster, uint32_t linkto)
{
	if(cluster / CLUSTERS_PER_SUPER >= supercluster_count)
		throw std::runtime_error("Bad cluster to link from");
	if(linkto / CLUSTERS_PER_SUPER >= supercluster_count)
		throw std::runtime_error("Bad cluster to link to");
	if(superclusters[cluster / CLUSTERS_PER_SUPER].clusters[cluster % CLUSTERS_PER_SUPER] != 1)
		throw std::runtime_error("Only end of chain clusters can be linked");
	superclusters[cluster / CLUSTERS_PER_SUPER].clusters[cluster % CLUSTERS_PER_SUPER] = linkto;
	superclusters[cluster / CLUSTERS_PER_SUPER].save(backing, cluster / CLUSTERS_PER_SUPER);
}

void filesystem::free_cluster_chain(uint32_t cluster)
{
	if(cluster == 2)
		throw std::runtime_error("Cluster 2 can't be freed");
	if(cluster / CLUSTERS_PER_SUPER >= supercluster_count)
		throw std::runtime_error("Bad cluster to free");
	uint32_t oldnext = superclusters[cluster / CLUSTERS_PER_SUPER].clusters[cluster % CLUSTERS_PER_SUPER];
	if(oldnext == 0)
		throw std::runtime_error("Attempted to free free cluster");
	if(oldnext == 0xFFFFFFFFU)
		throw std::runtime_error("Attempted to free system cluster");
	superclusters[cluster / CLUSTERS_PER_SUPER].clusters[cluster % CLUSTERS_PER_SUPER] = 0;
	//If there is no next block, or the next block is in different supercluster, save the cluster table.
	if(oldnext == 1 || oldnext / CLUSTERS_PER_SUPER != cluster / CLUSTERS_PER_SUPER)
		superclusters[cluster / CLUSTERS_PER_SUPER].save(backing, cluster / CLUSTERS_PER_SUPER);
	if(oldnext != 1)
		free_cluster_chain(oldnext);
}

size_t filesystem::skip_data(uint32_t& cluster, uint32_t& ptr, uint32_t length)
{
	size_t r = 0;
	do {
		if(cluster / CLUSTERS_PER_SUPER >= supercluster_count || (cluster % CLUSTERS_PER_SUPER) == 0)
			throw std::runtime_error("Bad cluster to read");
		if(!superclusters[cluster / CLUSTERS_PER_SUPER].clusters[cluster % CLUSTERS_PER_SUPER])
			throw std::runtime_error("Bad cluster to read");
		//Read to end of cluster.
		size_t maxread = min(length, max(static_cast<uint32_t>(CLUSTER_SIZE), ptr) - ptr);
		if(maxread) {
			length -= maxread;
			ptr += maxread;
			r += maxread;
		}
		if(ptr >= CLUSTER_SIZE) {
			uint32_t n = superclusters[cluster / CLUSTERS_PER_SUPER].clusters[cluster %
				CLUSTERS_PER_SUPER];
			if(n == 0)
				throw std::runtime_error("Bad next cluster when reading");
			else if(n == 1 || n == 0xFFFFFFFFU) {
				//Cluster is right.
				ptr = CLUSTER_SIZE;
				return r;
			} else {
				cluster = n;
				ptr = 0;
			}
		}
	} while(length > 0);
	return r;
}

size_t filesystem::read_data(uint32_t& cluster, uint32_t& ptr, void* data, uint32_t length)
{
	char* _data = reinterpret_cast<char*>(data);
	size_t r = 0;
	do {
		if(cluster / CLUSTERS_PER_SUPER >= supercluster_count || (cluster % CLUSTERS_PER_SUPER) == 0)
			throw std::runtime_error("Bad cluster to read");
		if(!superclusters[cluster / CLUSTERS_PER_SUPER].clusters[cluster % CLUSTERS_PER_SUPER])
			throw std::runtime_error("Bad cluster to read");
		//Read to end of cluster.
		size_t maxread = min(length, max(static_cast<uint32_t>(CLUSTER_SIZE), ptr) - ptr);
		if(maxread) {
			backing.clear();
			backing.seekg(cluster * CLUSTER_SIZE + ptr, std::ios_base::beg);
			backing.read(_data, maxread);
			if(!backing)
				throw std::runtime_error("Can't read data");
			length -= maxread;
			_data += maxread;
			ptr += maxread;
			r += maxread;
		}
		if(ptr >= CLUSTER_SIZE) {
			uint32_t n = superclusters[cluster / CLUSTERS_PER_SUPER].clusters[cluster %
				CLUSTERS_PER_SUPER];
			if(n == 0)
				throw std::runtime_error("Bad next cluster when reading");
			else if(n == 1 || n == 0xFFFFFFFFU) {
				//Cluster is right.
				ptr = CLUSTER_SIZE;
				return r;
			} else {
				cluster = n;
				ptr = 0;
			}
		}
	} while(length > 0);
	return r;
}

void filesystem::write_data(uint32_t& cluster, uint32_t& ptr, const void* data, uint32_t length,
	uint32_t& real_cluster, uint32_t& real_ptr)
{
	const char* _data = reinterpret_cast<const char*>(data);
	size_t r = 0;
	bool assigned = false;
	do {
		if(cluster / CLUSTERS_PER_SUPER >= supercluster_count || (cluster % CLUSTERS_PER_SUPER) == 0)
			throw std::runtime_error("Bad cluster to write");
		if(!superclusters[cluster / CLUSTERS_PER_SUPER].clusters[cluster % CLUSTERS_PER_SUPER])
			throw std::runtime_error("Bad cluster to write");
		//Write to end of cluster.
		size_t maxwrite = min(length, max(static_cast<uint32_t>(CLUSTER_SIZE), ptr) - ptr);
		if(maxwrite) {
			char buffer[CLUSTER_SIZE];
			memset(buffer, 0, CLUSTER_SIZE);
			if(!assigned) {
				real_cluster = cluster;
				real_ptr = ptr;
				assigned = true;
			}
			backing.seekp(0, std::ios_base::end);
			uint64_t backing_size = backing.tellp();
			if(backing_size > cluster * CLUSTER_SIZE) {
				backing.seekg(cluster * CLUSTER_SIZE, std::ios_base::beg);
				backing.read(buffer, CLUSTER_SIZE);
			}
			memcpy(buffer + ptr, _data, maxwrite);
			backing.seekp(cluster * CLUSTER_SIZE, std::ios_base::beg);
			backing.write(buffer, CLUSTER_SIZE);
			if(!backing)
				throw std::runtime_error("Can't write data");
			length -= maxwrite;
			_data += maxwrite;
			ptr += maxwrite;
			r += maxwrite;
		}
		if(ptr >= CLUSTER_SIZE) {
			uint32_t n = superclusters[cluster / CLUSTERS_PER_SUPER].clusters[cluster %
				CLUSTERS_PER_SUPER];
			if(n == 0)
				throw std::runtime_error("Bad next cluster when writing");
			else if(n == 0xFFFFFFFFU) {
				if(length > 0)
					throw std::runtime_error("Bad next cluster when writing");
				return;
			}
			else if(n == 1) {
				if(!length) {
					ptr = CLUSTER_SIZE;
					return;
				}
				n = allocate_cluster();
				link_cluster(cluster, n);
				cluster = n;
				ptr = 0;
			} else {
				cluster = n;
				ptr = 0;
			}
		}
	} while(length > 0);
}

void filesystem::supercluster::load(std::fstream& s, uint32_t index)
{
	uint64_t offset = SUPERCLUSTER_SIZE * index;
	char buffer[CLUSTER_SIZE];
	s.clear();
	s.seekg(offset, std::ios_base::beg);
	s.read(buffer, CLUSTER_SIZE);
	if(!s)
		throw std::runtime_error("Can't read cluster table");
	free_clusters = 0;
	for(unsigned i = 0; i < CLUSTERS_PER_SUPER; i++) {
		if(!(clusters[i] = serialization::u32b(buffer + 4 * i)))
			free_clusters++;
	}
}

void filesystem::supercluster::save(std::fstream& s, uint32_t index)
{
	uint64_t offset = SUPERCLUSTER_SIZE * index;
	char buffer[CLUSTER_SIZE];
	for(unsigned i = 0; i < CLUSTERS_PER_SUPER; i++)
		serialization::u32b(buffer + 4 * i, clusters[i]);
	s.clear();
	s.seekp(offset, std::ios_base::beg);
	s.write(buffer, CLUSTER_SIZE);
	if(!s)
		throw std::runtime_error("Can't write cluster table");
}

filesystem::ref& filesystem::ref::operator=(const filesystem::ref& r)
{
	if(this == &r)
		return *this;
	//This is tricky, due to having to lock two objects.
	threads::lock* mtodelete = NULL;
	if(!refcnt) {
		//We just have to grab a ref and copy.
		threads::alock m(*r.mlock);
		++*(r.refcnt);
		refcnt = r.refcnt;
		mlock = r.mlock;
		fs = r.fs;
	} else {
		//Two-object case.
		threads::alock_multiple ms({r.mlock, mlock});
		--*refcnt;
		if(!*refcnt) {
			delete fs;
			delete refcnt;
			mtodelete = mlock;
		}
		++*(r.refcnt);
		refcnt = r.refcnt;
		mlock = r.mlock;
		fs = r.fs;
	}
	if(mtodelete)
		delete mtodelete;
	return *this;
}
