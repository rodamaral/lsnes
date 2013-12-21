#include "fileimage.hpp"
#include "sha256.hpp"
#include "fileimage-patch.hpp"
#include "string.hpp"
#include "minmax.hpp"
#include "zip.hpp"
#include "directory.hpp"
#include <sstream>

namespace fileimage
{
namespace
{
	std::map<std::string, std::pair<time_t, std::string>> cached_entries;

	mutex_class& global_queue_mutex()
	{
		static mutex_class m;
		return m;
	}

	uint64_t calculate_headersize(uint64_t f, uint64_t h)
	{
		if(!h) return 0;
		if(f % (2 * h) == h) return h;
		return 0;
	}

	void* thread_trampoline(hash* h)
	{
		h->entrypoint();
		return NULL;
	}

	std::string lookup_cache(const std::string& filename, uint64_t prefixlen)
	{
		std::string cache = filename + ".sha256";
		if(prefixlen) cache += (stringfmt() << "-" << prefixlen).str();
		time_t filetime = file_get_mtime(filename);
		if(cached_entries.count(cache)) {
			//Found the cache entry...
			if(cached_entries[cache].first == filetime)
				return cached_entries[cache].second;
			else {
				//Stale.
				unlink(cache.c_str());
				cached_entries.erase(cache);
				return "";
			}
		}

		std::string cached_hash;
		time_t rfiletime;

		std::ifstream in(cache);
		if(!in)
			return "";	//Failed.

		std::string tmp;
		std::getline(in, tmp);
		std::istringstream _in(tmp);
		_in >> rfiletime;
		std::getline(in, cached_hash);

		if(rfiletime == filetime) {
			cached_entries[cache] = std::make_pair(rfiletime, cached_hash);
		} else  {
			//Stale.
			unlink(cache.c_str());
			cached_entries.erase(cache);
			return "";
		}
		return cached_hash;
	}

	void store_cache(const std::string& filename, uint64_t prefixlen, const std::string& value)
	{
		std::string cache = filename + ".sha256";
		if(prefixlen) cache += (stringfmt() << "-" << prefixlen).str();
		time_t filetime = file_get_mtime(filename);
		std::ofstream out(cache);
		cached_entries[cache] = std::make_pair(filetime, value);
		if(!out)
			return;		//Failed!
		out << filetime << std::endl;
		out << value << std::endl;
		out.close();
	}

	uint64_t get_file_size(const std::string& filename)
	{
		uintmax_t size = file_get_size(filename);
		if(size == static_cast<uintmax_t>(-1))
			return 0;
		return size;
	}
}

hashval::hashval()
{
	is_ready = false;
	cbid = 0;
	prev = next = NULL;
	hasher = NULL;
}

hashval::hashval(const std::string& _value, uint64_t _prefix)
{
	is_ready = true;
	value = _value;
	cbid = 0;
	prefixv = _prefix;
	prev = next = NULL;
	hasher = NULL;
}

hashval::hashval(hash& h, unsigned id)
{
	umutex_class h2(global_queue_mutex());
	is_ready = false;
	cbid = id;
	prev = next = NULL;
	hasher = &h;
	hasher->link(*this);
}

hashval::~hashval()
{
	umutex_class h2(global_queue_mutex());
	umutex_class h(mutex);
	if(hasher)
		hasher->unlink(*this);
}

bool hashval::ready() const
{
	umutex_class h(mutex);
	return is_ready;
}

std::string hashval::read() const
{
	umutex_class h(mutex);
	while(!is_ready)
		condition.wait(h);
	if(error != "")
		throw std::runtime_error(error);
	return value;
}

uint64_t hashval::prefix() const
{
	umutex_class h(mutex);
	while(!is_ready)
		condition.wait(h);
	if(error != "")
		throw std::runtime_error(error);
	return prefixv;
}

hashval::hashval(const hashval& f)
{
	umutex_class h2(global_queue_mutex());
	umutex_class h(f.mutex);
	is_ready = f.is_ready;
	cbid = f.cbid;
	value = f.value;
	error = f.error;
	prefixv = f.prefixv;
	prev = next = NULL;
	hasher = f.hasher;
	if(!is_ready && hasher)
		hasher->link(*this);
}

hashval& hashval::operator=(const hashval& f)
{
	if(this == &f)
		return *this;
	umutex_class h2(global_queue_mutex());
	if((size_t)this < (size_t)&f) {
		mutex.lock();
		f.mutex.lock();
	} else {
		f.mutex.lock();
		mutex.lock();
	}

	if(!is_ready && hasher)
		hasher->unlink(*this);
	is_ready = f.is_ready;
	cbid = f.cbid;
	value = f.value;
	error = f.error;
	prefixv = f.prefixv;
	prev = next = NULL;
	hasher = f.hasher;
	if(!is_ready && hasher)
		hasher->link(*this);
	mutex.unlock();
	f.mutex.unlock();
}

void hashval::resolve(unsigned id, const std::string& hash, uint64_t _prefix)
{
	umutex_class h(mutex);
	hasher->unlink(*this);
	if(id != cbid)
		return;
	is_ready = true;
	value = hash;
	prefixv = _prefix;
	condition.notify_all();
}

void hashval::resolve_error(unsigned id, const std::string& err)
{
	umutex_class h(mutex);
	hasher->unlink(*this);
	if(id != cbid)
		return;
	is_ready = true;
	error = err;
	prefixv = 0;
	condition.notify_all();
}

void hash::link(hashval& future)
{
	//We assume caller holds global queue lock.
	{
		umutex_class h(mutex);
		unsigned cbid = future.cbid;
		for(auto& i : queue)
			if(i.cbid == cbid)
				i.interested--;
	}
	future.prev = last_future;
	future.next = NULL;
	if(last_future)
		last_future->next = &future;
	last_future = &future;
	if(!first_future)
		first_future = &future;
}

void hash::unlink(hashval& future)
{
	//We assume caller holds global queue lock.
	{
		umutex_class h(mutex);
		unsigned cbid = future.cbid;
		for(auto& i : queue)
			if(i.cbid == cbid)
				i.interested++;
	}
	if(&future == first_future)
		first_future = future.next;
	if(&future == last_future)
		last_future = future.prev;
	if(future.prev)
		future.prev->next = future.next;
	if(future.next)
		future.next->prev = future.prev;
}

hashval hash::operator()(const std::string& filename, uint64_t prefixlen)
{
	queue_job j;
	j.filename = filename;
	j.prefix = prefixlen;
	j.size = get_file_size(filename);
	j.cbid = next_cbid++;
	j.interested = 1;
	hashval future(*this, j.cbid);
	queue.push_back(j);
	umutex_class h(mutex);
	total_work += j.size;
	work_size += j.size;
	condition.notify_all();
	return future;
}

hashval hash::operator()(const std::string& filename, std::function<uint64_t(uint64_t)> prefixlen)
{
	queue_job j;
	j.filename = filename;
	j.size = get_file_size(filename);
	j.prefix = prefixlen(j.size);
	j.cbid = next_cbid++;
	j.interested = 1;
	hashval future(*this, j.cbid);
	queue.push_back(j);
	umutex_class h(mutex);
	total_work += j.size;
	work_size += j.size;
	condition.notify_all();
	return future;
}

void hash::set_callback(std::function<void(uint64_t, uint64_t)> cb)
{
	umutex_class h(mutex);
	progresscb = cb;
}

hash::hash()
{
	quitting = false;
	first_future = NULL;
	last_future = NULL;
	next_cbid = 0;
	total_work = 0;
	work_size = 0;
	progresscb = [](uint64_t x, uint64_t y) -> void {};
	hash_thread = new thread_class(thread_trampoline, this);
}

hash::~hash()
{
	{
		umutex_class h(mutex);
		quitting = true;
		condition.notify_all();
	}
	hash_thread->join();
	delete hash_thread;
	umutex_class h2(global_queue_mutex());
	while(first_future)
		first_future->resolve_error(first_future->cbid, "Hasher deleted");
}

void hash::entrypoint()
{
	FILE* fp;
	while(true) {
		//Wait for work or quit signal.
		{
			umutex_class h(mutex);
			while(!quitting && queue.empty()) {
				send_idle();
				condition.wait(h);
			}
			if(quitting)
				return;
			//We hawe work.
			current_job = queue.begin();
		}

		//Hash this item.
		uint64_t progress = 0;
		std::string cached_hash;
		fp = NULL;
		cached_hash = lookup_cache(current_job->filename, current_job->prefix);
		if(cached_hash != "") {
			umutex_class h2(global_queue_mutex());
			for(hashval* fut = first_future; fut != NULL; fut = fut->next)
				fut->resolve(current_job->cbid, cached_hash, current_job->prefix);
			goto finished;
		}
		fp = fopen(current_job->filename.c_str(), "rb");
		if(!fp) {
			umutex_class h2(global_queue_mutex());
			for(hashval* fut = first_future; fut != NULL; fut = fut->next)
				fut->resolve_error(current_job->cbid, "Can't open file");
		} else {
			sha256 hash;
			uint64_t toskip = current_job->prefix;
			while(!feof(fp) && !ferror(fp)) {
				{
					umutex_class h(mutex);
					if(!current_job->interested)
						goto finished; //Aborted.
				}
				unsigned char buf[16384];
				uint64_t offset = 0;
				size_t s = fread(buf, 1, sizeof(buf), fp);
				progress += s;
				//The first current_job->prefix bytes need to be skipped.
				offset = min(toskip, (uint64_t)s);
				toskip -= offset;
				if(s > offset) hash.write(buf + offset, s - offset);
				send_callback(progress);
			}
			if(ferror(fp)) {
				umutex_class h2(global_queue_mutex());
				for(hashval* fut = first_future; fut != NULL; fut = fut->next)
					fut->resolve_error(current_job->cbid, "Can't read file");
			} else {
				std::string hval = hash.read();
				umutex_class h2(global_queue_mutex());
				for(hashval* fut = first_future; fut != NULL; fut = fut->next)
					fut->resolve(current_job->cbid, hval, current_job->prefix);
				store_cache(current_job->filename, current_job->prefix, hval);
			}
		}
finished:
		if(fp) fclose(fp);
		//Okay, this work item is complete.
		{
			umutex_class h(mutex);
			total_work -= current_job->size;
			queue.erase(current_job);
		}
		send_callback(0);
	}
}

void hash::send_callback(uint64_t this_completed)
{
	uint64_t amount;
	{
		umutex_class h(mutex);
		if(this_completed > total_work)
			amount = 0;
		else
			amount = total_work - this_completed;
	}
	progresscb(amount, work_size);
}

void hash::send_idle()
{
	work_size = 0;	//Delete work when idle.
	progresscb(0xFFFFFFFFFFFFFFFFULL, 0);
}

image::image() throw(std::bad_alloc)
{
	type = info::IT_NONE;
	sha_256 = hashval("");
	filename = "";
}

image::image(hash& h, const std::string& _filename, const std::string& base,
	const struct image::info& info) throw(std::bad_alloc, std::runtime_error)
{
	if(info.type == info::IT_NONE && _filename != "")
		throw std::runtime_error("Tried to load NULL image");
	if(_filename == "") {
		//NULL.
		type = info::IT_NONE;
		sha_256 = hashval("");
		stripped = 0;
		return;
	}

	std::string xfilename = _filename;
#if defined(_WIN32) || defined(_WIN64)
	const char* split = "/\\";
#else
	const char* split = "/";
#endif
	size_t s1 = xfilename.find_last_of(split);
	size_t s2 = xfilename.find_last_of(".");
	if(s1 < xfilename.length()) s1 = s1 + 1; else s1 = 0;
	if(s2 <= s1 || s2 >= xfilename.length()) s2 = xfilename.length();
	namehint = xfilename.substr(s1, s2 - s1);

	//Load markups and memory images.
	if(info.type == info::IT_MEMORY || info.type == info::IT_MARKUP) {
		unsigned headered = 0;
		filename = zip::resolverel(_filename, base);
		type = info.type;

		data.reset(new std::vector<char>(zip::readrel(_filename, base)));
		headered = (info.type == info::IT_MEMORY) ? calculate_headersize(data->size(), info.headersize) : 0;
		if(data->size() >= headered) {
			if(headered) {
				memmove(&(*data)[0], &(*data)[headered], data->size() - headered);
				data->resize(data->size() - headered);
			}
		} else {
			data->resize(0);
		}
		stripped = headered;
		sha_256 = hashval(sha256::hash(*data), headered);
		if(info.type == info::IT_MARKUP) {
			size_t osize = data->size();
			data->resize(osize + 1);
			(*data)[osize] = 0;
		}
		return;
	}

	if(info.type == info::IT_FILE) {
		filename = zip::resolverel(_filename, base);
		filename = get_absolute_path(filename);
		type = info::IT_FILE;
		data.reset(new std::vector<char>(filename.begin(), filename.end()));
		stripped = 0;
		sha_256 = h(filename);
		return;
	}
	throw std::runtime_error("Unknown image type");
}

void image::patch(const std::vector<char>& patch, int32_t offset) throw(std::bad_alloc, std::runtime_error)
{
	if(type == info::IT_NONE)
		throw std::runtime_error("Not an image");
	if(type != info::IT_MEMORY && type != info::IT_MARKUP)
		throw std::runtime_error("File images can't be patched on the fly");
	try {
		std::vector<char> data2 = *data;
		if(type == info::IT_MARKUP)
			data2.resize(data2.size() - 1);
		data2 = ::fileimage::patch(data2, patch, offset);
		//Mark the slot as valid and update hash.
		std::string new_sha256 = sha256::hash(data2);
		if(type == info::IT_MARKUP) {
			size_t osize = data2.size();
			data2.resize(osize + 1);
			data2[osize] = 0;
		}
		data.reset(new std::vector<char>(data2));
		sha_256 = hashval(new_sha256);
	} catch(...) {
		throw;
	}
}

std::function<uint64_t(uint64_t)> std_headersize_fn(uint64_t hdrsize)
{
	uint64_t h = hdrsize;
	return ([h](uint64_t x) -> uint64_t { return calculate_headersize(x, h); });
}
}
