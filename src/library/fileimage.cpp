#include "fileimage.hpp"
#include "sha256.hpp"
#include "patch.hpp"
#include "zip.hpp"
#include <boost/filesystem.hpp>
#include <sstream>

#ifdef BOOST_FILESYSTEM3
namespace boost_fs = boost::filesystem3;
#else
namespace boost_fs = boost::filesystem;
#endif

namespace
{
	std::map<std::string, std::pair<time_t, std::string>> cached_entries;

	std::mutex& global_queue_mutex()
	{
		static std::mutex m;
		return m;
	}

	void* thread_trampoline(sha256_hasher* h)
	{
		h->entrypoint();
		return NULL;
	}

	std::string lookup_cache(const std::string& filename)
	{
		std::string cache = filename + ".sha256";
		time_t filetime = boost_fs::last_write_time(boost_fs::path(filename));
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

	void store_cache(const std::string& filename, const std::string& value)
	{
		std::string cache = filename + ".sha256";
		time_t filetime = boost_fs::last_write_time(boost_fs::path(filename));
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
		uintmax_t size = boost_fs::file_size(boost_fs::path(filename));
		if(size == static_cast<uintmax_t>(-1))
			return 0;
		return size;
	}
}

sha256_future::sha256_future()
{
	is_ready = false;
	cbid = 0;
	prev = next = NULL;
	hasher = NULL;
}

sha256_future::sha256_future(const std::string& _value)
{
	is_ready = true;
	value = _value;
	cbid = 0;
	prev = next = NULL;
	hasher = NULL;
}

sha256_future::sha256_future(sha256_hasher& h, unsigned id)
{
	umutex_class h2(global_queue_mutex());
	is_ready = false;
	cbid = id;
	prev = next = NULL;
	hasher = &h;
	hasher->link(*this);
}

sha256_future::~sha256_future()
{
	umutex_class h2(global_queue_mutex());
	umutex_class h(mutex);
	if(hasher)
		hasher->unlink(*this);
}

bool sha256_future::ready() const
{
	umutex_class h(mutex);
	return is_ready;
}

std::string sha256_future::read() const
{
	umutex_class h(mutex);
	while(!is_ready)
		condition.wait(h);
	if(error != "")
		throw std::runtime_error(error);
	return value;
}

sha256_future::sha256_future(const sha256_future& f)
{
	umutex_class h2(global_queue_mutex());
	umutex_class h(f.mutex);
	is_ready = f.is_ready;
	cbid = f.cbid;
	value = f.value;
	error = f.error;
	prev = next = NULL;
	hasher = f.hasher;
	if(!is_ready && hasher)
		hasher->link(*this);
}

sha256_future& sha256_future::operator=(const sha256_future& f)
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
	prev = next = NULL;
	hasher = f.hasher;
	if(!is_ready && hasher)
		hasher->link(*this);
	mutex.unlock();
	f.mutex.unlock();
}

void sha256_future::resolve(unsigned id, const std::string& hash)
{
	umutex_class h(mutex);
	hasher->unlink(*this);
	if(id != cbid)
		return;
	is_ready = true;
	value = hash;
	condition.notify_all();
}

void sha256_future::resolve_error(unsigned id, const std::string& err)
{
	umutex_class h(mutex);
	hasher->unlink(*this);
	if(id != cbid)
		return;
	is_ready = true;
	error = err;
	condition.notify_all();
}

void sha256_hasher::link(sha256_future& future)
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

void sha256_hasher::unlink(sha256_future& future)
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

sha256_future sha256_hasher::operator()(const std::string& filename)
{
	queue_job j;
	j.filename = filename;
	j.size = get_file_size(filename);
	j.cbid = next_cbid++;
	j.interested = 1;
	sha256_future future(*this, j.cbid);
	queue.push_back(j);
	umutex_class h(mutex);
	total_work += j.size;
	condition.notify_all();
	return future;
}

void sha256_hasher::set_callback(std::function<void(uint64_t)> cb)
{
	umutex_class h(mutex);
	progresscb = cb;
}

sha256_hasher::sha256_hasher()
{
	quitting = false;
	first_future = NULL;
	last_future = NULL;
	next_cbid = 0;
	total_work = 0;
	progresscb = [](uint64_t x) -> void {};
	hash_thread = new thread_class(thread_trampoline, this);
}

sha256_hasher::~sha256_hasher()
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

void sha256_hasher::entrypoint()
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
		cached_hash = lookup_cache(current_job->filename);
		if(cached_hash != "") {
			umutex_class h2(global_queue_mutex());
			for(sha256_future* fut = first_future; fut != NULL; fut = fut->next)
				fut->resolve(current_job->cbid, cached_hash);
			goto finished;
		}
		fp = fopen(current_job->filename.c_str(), "rb");
		if(!fp) {
			umutex_class h2(global_queue_mutex());
			for(sha256_future* fut = first_future; fut != NULL; fut = fut->next)
				fut->resolve_error(current_job->cbid, "Can't open file");
		} else {
			sha256 hash;
			while(!feof(fp) && !ferror(fp)) {
				{
					umutex_class h(mutex);
					if(!current_job->interested)
						goto finished; //Aborted.
				}
				unsigned char buf[16384];
				size_t s = fread(buf, 1, sizeof(buf), fp);
				progress += s;
				hash.write(buf, s);
				send_callback(progress);
			}
			if(ferror(fp)) {
				umutex_class h2(global_queue_mutex());
				for(sha256_future* fut = first_future; fut != NULL; fut = fut->next)
					fut->resolve_error(current_job->cbid, "Can't read file");
			} else {
				std::string hval = hash.read();
				umutex_class h2(global_queue_mutex());
				for(sha256_future* fut = first_future; fut != NULL; fut = fut->next)
					fut->resolve(current_job->cbid, hval);
				store_cache(current_job->filename, hval);
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

void sha256_hasher::send_callback(uint64_t this_completed)
{
	uint64_t amount;
	{
		umutex_class h(mutex);
		if(this_completed > total_work)
			amount = 0;
		else
			amount = total_work - this_completed;
	}
	progresscb(amount);
}

void sha256_hasher::send_idle()
{
	progresscb(0xFFFFFFFFFFFFFFFFULL);
}

loaded_image::loaded_image() throw(std::bad_alloc)
{
	type = info::IT_NONE;
	valid = false;
	sha_256 = sha256_future("");
	filename = "";
}

loaded_image::loaded_image(sha256_hasher& h, const std::string& _filename, const std::string& base,
	const struct loaded_image::info& info) throw(std::bad_alloc, std::runtime_error)
{
	if(info.type == info::IT_NONE && _filename != "")
		throw std::runtime_error("Tried to load NULL image");
	if(_filename == "") {
		//NULL.
		type = info::IT_NONE;
		valid = false;
		sha_256 = sha256_future("");
		return;
	}

	//Load markups and memory images.
	if(info.type == info::IT_MEMORY || info.type == info::IT_MARKUP) {
		unsigned headered = 0;
		filename = resolve_file_relative(_filename, base);
		type = info.type;
		data = read_file_relative(_filename, base);
		valid = true;
		if(info.type == info::IT_MEMORY && info.headersize)
			headered = ((data.size() % (2 * info.headersize)) == info.headersize) ? info.headersize : 0;
		if(data.size() >= headered) {
			if(headered) {
				memmove(&data[0], &data[headered], data.size() - headered);
				data.resize(data.size() - headered);
			}
		} else {
			data.resize(0);
		}
		sha_256 = sha256_future(sha256::hash(data));
		if(info.type == info::IT_MARKUP) {
			size_t osize = data.size();
			data.resize(osize + 1);
			data[osize] = 0;
		}
		return;
	}

	if(info.type == info::IT_FILE) {
		filename = resolve_file_relative(_filename, base);
		filename = boost_fs::absolute(boost_fs::path(filename)).string();
		type = info::IT_FILE;
		data.resize(filename.length());
		std::copy(filename.begin(), filename.end(), data.begin());
		valid = true;
		sha_256 = h(filename);
		return;
	}
	throw std::runtime_error("Unknown image type");
}

void loaded_image::patch(const std::vector<char>& patch, int32_t offset) throw(std::bad_alloc, std::runtime_error)
{
	if(type == info::IT_NONE)
		throw std::runtime_error("Not an image");
	if(type != info::IT_MEMORY && type != info::IT_MARKUP)
		throw std::runtime_error("File images can't be patched on the fly");
	try {
		std::vector<char> data2 = data;
		if(type == info::IT_MARKUP)
			data2.resize(data2.size() - 1);
		data2 = do_patch_file(data2, patch, offset);
		//Mark the slot as valid and update hash.
		valid = true;
		std::string new_sha256 = sha256::hash(data2);
		if(type == info::IT_MARKUP) {
			size_t osize = data2.size();
			data2.resize(osize + 1);
			data2[osize] = 0;
		}
		data = data2;
		sha_256 = sha256_future(new_sha256);
	} catch(...) {
		throw;
	}
}
