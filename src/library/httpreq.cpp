#define CURL_STATICLIB
#include <cstdio>
#include "httpreq.hpp"
#include "httpauth.hpp"
#include "string.hpp"
#include "minmax.hpp"
#include "threadtypes.hpp"
#include "streamcompress.hpp"
#include <curl/curl.h>
#include <cstring>
#include <fstream>
#include <boost/iostreams/categories.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/stream_buffer.hpp>
#include <boost/iostreams/filter/symmetric.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/device/back_inserter.hpp>

http_request::input_handler::~input_handler()
{
}

size_t http_request::input_handler::read_fn(char* ptr, size_t size, size_t nmemb, void* userdata)
{
	if(reinterpret_cast<http_request::input_handler*>(userdata)->canceled) return CURL_READFUNC_ABORT;
	try {
		return reinterpret_cast<http_request::input_handler*>(userdata)->read(ptr, size * nmemb);
	} catch(...) {
		return CURL_READFUNC_ABORT;
	}
}

http_request::null_input_handler::~null_input_handler()
{
}

uint64_t http_request::null_input_handler::get_length()
{
	return 0;
}

size_t http_request::null_input_handler::read(char* target, size_t maxread)
{
	return 0;
}

http_request::output_handler::~output_handler()
{
}

size_t http_request::output_handler::write_fn(char* ptr, size_t size, size_t nmemb, void* userdata)
{
	if(reinterpret_cast<http_request::output_handler*>(userdata)->canceled) return 0;
	try {
		reinterpret_cast<http_request::output_handler*>(userdata)->write(ptr, size * nmemb);
		return size * nmemb;
	} catch(...) {
		return 0;
	}
}

size_t http_request::output_handler::header_fn(void* _ptr, size_t size, size_t nmemb, void* userdata)
{
	size_t hsize = size * nmemb;
	char* ptr = (char*)_ptr;
	while(hsize > 0 && (ptr[hsize - 1] == '\r' || ptr[hsize - 1] == '\n')) hsize--;
	char* split = strchr((char*)ptr, ':');
	char* firstns = split;
	if(firstns) {
		firstns++;
		while(*firstns && (*firstns == '\t' || *firstns == ' ')) firstns++;
	}
	char* end = (char*)ptr + hsize;
	if(split == NULL)
		reinterpret_cast<http_request::output_handler*>(userdata)->header("", std::string((char*)ptr, hsize));
	else
		reinterpret_cast<http_request::output_handler*>(userdata)->header(std::string((char*)ptr,
			split - (char*)ptr), std::string(firstns, end - firstns));
	return size * nmemb;
}

http_request::www_authenticate_extractor::www_authenticate_extractor(
	std::function<void(const std::string& value)> _callback)
{
	callback = _callback;
}

http_request::www_authenticate_extractor::~www_authenticate_extractor()
{
}

std::string http_strlower(const std::string& name)
{
	std::string name2 = name;
	for(size_t i = 0; i < name2.length(); i++)
		if(name2[i] >= 65 && name2[i] <= 90) name2[i] = name2[i] + 32;
	return name2;
}

void http_request::www_authenticate_extractor::header(const std::string& name, const std::string& content)
{
	if(http_strlower(name) == "www-authenticate") callback(content);
}

void http_request::www_authenticate_extractor::write(const char* source, size_t srcsize)
{
	//Do nothing.
}


http_request::~http_request()
{
	if(handle)
		curl_easy_cleanup((CURL*)handle);
}

http_request::http_request(const std::string& verb, const std::string& url)
{
	dlnow = dltotal = ulnow = ultotal = 0;
	handle = curl_easy_init();
	if(!handle)
		throw std::runtime_error("Can't initialize HTTP transfer");
	has_body = false;
	if(verb == "GET") {
	} else if(verb == "HEAD") {
		auto err = curl_easy_setopt((CURL*)handle, CURLOPT_NOBODY, 1);
		if(err) throw std::runtime_error(curl_easy_strerror(err));
	} else if(verb == "POST") {
		auto err = curl_easy_setopt((CURL*)handle, CURLOPT_POST, 1);
		if(err) throw std::runtime_error(curl_easy_strerror(err));
		has_body = true;
	} else if(verb == "PUT") {
		auto err = curl_easy_setopt((CURL*)handle, CURLOPT_PUT, 1);
		if(err) throw std::runtime_error(curl_easy_strerror(err));
		has_body = true;
	} else
		throw std::runtime_error("Unknown HTTP verb");
		auto err = curl_easy_setopt((CURL*)handle, CURLOPT_URL, url.c_str());
		if(err) throw std::runtime_error(curl_easy_strerror(err));
}

void http_request::do_transfer(input_handler* inhandler, output_handler* outhandler)
{
	auto err = curl_easy_setopt((CURL*)handle, CURLOPT_NOPROGRESS, 0);
	if(err) throw std::runtime_error(curl_easy_strerror(err));
	err = curl_easy_setopt((CURL*)handle, CURLOPT_WRITEFUNCTION, http_request::output_handler::write_fn);
	if(err) throw std::runtime_error(curl_easy_strerror(err));
	err = curl_easy_setopt((CURL*)handle, CURLOPT_WRITEDATA, (void*)outhandler);
	if(err) throw std::runtime_error(curl_easy_strerror(err));
	if(has_body) {
		err = curl_easy_setopt((CURL*)handle, CURLOPT_READFUNCTION,
			http_request::input_handler::read_fn);
		if(err) throw std::runtime_error(curl_easy_strerror(err));
		err = curl_easy_setopt((CURL*)handle, CURLOPT_READDATA, (void*)inhandler);
		if(err) throw std::runtime_error(curl_easy_strerror(err));
		err = curl_easy_setopt((CURL*)handle, CURLOPT_INFILESIZE_LARGE,
			(curl_off_t)inhandler->get_length());
		if(err) throw std::runtime_error(curl_easy_strerror(err));
	}
	err = curl_easy_setopt((CURL*)handle, CURLOPT_PROGRESSFUNCTION, &http_request::progress);
	if(err) throw std::runtime_error(curl_easy_strerror(err));
	err = curl_easy_setopt((CURL*)handle, CURLOPT_PROGRESSDATA, (void*)this);
	if(err) throw std::runtime_error(curl_easy_strerror(err));
	err = curl_easy_setopt((CURL*)handle, CURLOPT_HEADERFUNCTION, &http_request::output_handler::header_fn);
	if(err) throw std::runtime_error(curl_easy_strerror(err));
	err = curl_easy_setopt((CURL*)handle, CURLOPT_HEADERDATA, (void*)outhandler);
	if(err) throw std::runtime_error(curl_easy_strerror(err));

	struct curl_slist* list = NULL;
	if(authorization != "") {
		std::string foo = "Authorization: " + authorization;
		list = curl_slist_append(list, foo.c_str());
	}
	if(list) {
		curl_easy_setopt((CURL*)handle, CURLOPT_HTTPHEADER, list);
	}

	err = curl_easy_perform((CURL*)handle);
	if(err) throw std::runtime_error(curl_easy_strerror(err));

	if(list)
		curl_slist_free_all(list);
}

void http_request::global_init()
{
	curl_global_init(CURL_GLOBAL_ALL);
}

int http_request::progress(void* userdata, double dltotal, double dlnow, double ultotal, double ulnow)
{
	return reinterpret_cast<http_request*>(userdata)->_progress(dltotal, dlnow, ultotal, ulnow);
}

int http_request::_progress(double _dltotal, double _dlnow, double _ultotal, double _ulnow)
{
	dltotal = _dltotal;
	dlnow = _dlnow;
	ultotal = _ultotal;
	ulnow = _ulnow;
	return 0;
}

void http_request::get_xfer_status(int64_t& dnow, int64_t& dtotal, int64_t& unow, int64_t& utotal)
{
	dnow = dlnow;
	dtotal = dltotal;
	unow = ulnow;
	utotal = ultotal;
}

long http_request::get_http_code()
{
	long ret = 0;
	curl_easy_getinfo((CURL*)handle, CURLINFO_RESPONSE_CODE, &ret);
	return ret;
}

http_async_request::http_async_request()
{
	ihandler = NULL;
	ohandler = NULL;
	final_dl = 0;
	final_ul = 0;
	finished = false;
	req = NULL;
}

void http_async_request::get_xfer_status(int64_t& dnow, int64_t& dtotal, int64_t& unow, int64_t& utotal)
{
	umutex_class h(m);
	if(req) {
		req->get_xfer_status(dnow, dtotal, unow, utotal);
	} else {
		dnow = dtotal = final_dl;
		unow = utotal = final_ul;
	}
}

namespace
{
	void async_http_trampoline(http_async_request* r)
	{
		try {
			r->req->do_transfer(r->ihandler, r->ohandler);
		} catch(std::exception& e) {
			umutex_class h(r->m);
			r->finished_cond.notify_all();
			r->finished = true;
			delete r->req;
			r->req = NULL;
			r->errormsg = e.what();
			return;
		}
		int64_t tmp1, tmp2;
		umutex_class h(r->m);
		r->http_code = r->req->get_http_code();
		r->req->get_xfer_status(r->final_dl, tmp1, r->final_ul, tmp2);
		r->finished_cond.notify_all();
		r->finished = true;
		delete r->req;
		r->req = NULL;
	}
}

void http_async_request::lauch_async()
{
	try {
		{
			umutex_class h(m);
			req = new http_request(verb, url);
			if(authorization != "") req->set_authorization(authorization);
		}
		(new thread_class(async_http_trampoline, this))->detach();
	} catch(std::exception& e) {
		umutex_class h(m);
		finished_cond.notify_all();
		finished = true;
		delete req;
		req = NULL;
		errormsg = e.what();
	}
}

void http_async_request::cancel()
{
	if(ihandler) ihandler->cancel();
	if(ohandler) ohandler->cancel();
}

property_upload_request::property_upload_request()
{
	state = 0;
	sent = 0;
}

property_upload_request::~property_upload_request()
{
}

uint64_t property_upload_request::get_length()
{
	uint64_t tmp = 0;
	for(auto j : data) {
		std::string X = (stringfmt() << "," << j.second.length() << ":").str();
		tmp = tmp + X.length() + j.first.length() + j.second.length();
	}
	return tmp;
}

void property_upload_request::rewind()
{
	state = 0;
	sent = 0;
}

void property_upload_request::str_helper(const std::string& str, char*& target, size_t& maxread, size_t& x,
	unsigned next)
{
	size_t y = min((uint64_t)maxread, (uint64_t)(str.length() - sent));
	if(y == 0) {
		state = next;
		sent = 0;
		return;
	}
	std::copy(str.begin() + sent, str.begin() + sent + y, target);
	target += y;
	maxread -= y;
	x += y;
	sent += y;
}

void property_upload_request::chr_helper(char ch, char*& target, size_t& maxread, size_t& x, unsigned next)
{
	*(target++) = ch;
	maxread--;
	x++;
	state = next;
}

void property_upload_request::len_helper(size_t len, char*& target, size_t& maxread, size_t& x, unsigned next)
{
	std::string tmp = (stringfmt() << len).str();
	str_helper(tmp, target, maxread, x, next);
}

size_t property_upload_request::read(char* target, size_t maxread)
{
	size_t x = 0;
	size_t y;
	while(maxread > 0) {
		switch(state) {
		case 0:
			state = 1;
			itr = data.begin();
			break;
		case 1:
			if(itr == data.end()) {
				state = 7;
			} else
				str_helper(itr->first, target, maxread, x, 2);
			break;
		case 2:
			chr_helper('=', target, maxread, x, 3);
			break;
		case 3:		//Length of value.
			len_helper(itr->second.length(), target, maxread, x, 4);
			break;
		case 4:		//The separator of value.
			chr_helper(':', target, maxread, x, 5);
			break;
		case 5:		//Value.
			str_helper(itr->second, target, maxread, x, 6);
			break;
		case 6:		//End of entry.
			itr++;
			state = 1;
			break;
		case 7:
			return x;
		}
	}
	return x;
}
