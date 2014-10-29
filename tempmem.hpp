#pragma once

//Temporary memory.
std::map<unsigned, std::pair<void*, size_t>> tmpalloc_memory;

void* tmpalloc_resize(unsigned handle, size_t a, size_t b)
{
	if(!tmpalloc_memory.count(handle)) {
		tmpalloc_memory[handle].first = calloc(a, b);
		if(!tmpalloc_memory[handle].first)
			throw std::bad_alloc();
		tmpalloc_memory[handle].second = a * b;
	} else if(!a || tmpalloc_memory[handle].second / a >= b) {
		//Enough, do nothing.
	} else {
		free(tmpalloc_memory[handle].first);
		tmpalloc_memory[handle].first = calloc(a, b);
		if(!tmpalloc_memory[handle].first)
			throw std::bad_alloc();
		tmpalloc_memory[handle].second = a * b;
	}
	return tmpalloc_memory[handle].first;
}

template<typename T, typename... A> T* tmpalloc(unsigned handle, A... args)
{
	T* mem = (T*)tmpalloc_resize(handle, 1, sizeof(T));
	new(mem) T(args...);
	return mem;
}

template<typename T> T* tmpalloc_array(unsigned handle, size_t size)
{
	T* mem = (T*)tmpalloc_resize(handle, size, sizeof(T));
	for(size_t i = 0; i < size; i++)
		new(mem + i) T();
	return mem;
}

const char* tmpalloc_str(unsigned handle, const std::string& orig)
{
	auto len = orig.length();
	char* mem = tmpalloc_array<char>(handle, len + 1);
	std::copy(orig.begin(), orig.end(), mem);
	mem[len] = '\0';
	return mem;
}

const char* tmpalloc_data(unsigned handle, const void* data, size_t datalen)
{
	char* mem = tmpalloc_array<char>(handle, datalen);
	std::copy((const char*)data, (const char*)data + datalen, mem);
	return mem;
}


const char* tmp_sprintf(unsigned handle, const char* format, ...) __attribute__((format(printf, 2, 3)));

const char* tmp_sprintf(unsigned handle, const char* format, ...)
{
	char dummy[2];
	va_list args;
	va_list args2;
	va_start(args, format);
	size_t sreq = vsnprintf(dummy, 1, format, args);
	va_end(args);
	char* buf = tmpalloc_array<char>(handle, sreq + 2);
	va_start(args2, format);
	vsnprintf(buf, sreq + 1, format, args2);
	va_end(args2);
	return buf;
}
