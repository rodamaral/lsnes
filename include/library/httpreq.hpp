#ifndef _library__httpreq__hpp__included__
#define _library__httpreq__hpp__included__

#include <string>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <string>
#include "text.hpp"
#include "threads.hpp"

class http_request
{
public:
/**
 * Input handler.
 */
	class input_handler
	{
	public:
		input_handler() { canceled = false; }
		virtual ~input_handler();
		static size_t read_fn(char* ptr, size_t size, size_t nmemb, void* userdata);
/**
 * Get length of input.
 */
		virtual uint64_t get_length() = 0;
/**
 * Read block of input.
 */
		virtual size_t read(char* target, size_t maxread) = 0;
/**
 * Cancel.
 */
		void cancel() { canceled = true; }
/**
 * Cancel flag.
 */
		volatile bool canceled;
	};
/**
 * NULL input handler.
 */
	class null_input_handler : public input_handler
	{
	public:
		~null_input_handler();
		uint64_t get_length();
		size_t read(char* target, size_t maxread);
	};
/**
 * Output handler.
 */
	class output_handler
	{
	public:
		output_handler() { canceled = false; }
		virtual ~output_handler();
		static size_t write_fn(char* ptr, size_t size, size_t nmemb, void* userdata);
		static size_t header_fn(void* ptr, size_t size, size_t nmemb, void* userdata);
/**
 * Sink header.
 */
		virtual void header(const text& name, const text& cotent) = 0;
/**
 * Sink data.
 */
		virtual void write(const char* source, size_t srcsize) = 0;
/**
 * Cancel.
 */
		void cancel() { canceled = true; }
/**
 * Cancel flag.
 */
		volatile bool canceled;
	};
/**
 * Obtain WWW-Authenticate responses.
 */
	class www_authenticate_extractor : public output_handler
	{
	public:
		www_authenticate_extractor(std::function<void(const text& value)> _callback);
		~www_authenticate_extractor();
		void header(const text& name, const text& cotent);
		void write(const char* source, size_t srcsize);
	private:
		std::function<void(const text& value)> callback;
	};
/**
 * Create a new request instance.
 */
	http_request(const text& verb, const text& url);
/**
 * Destructor.
 */
	~http_request();
/**
 * Set the authorization header to use.
 */
	void set_authorization(const text& hdr)
	{
		authorization = hdr;
	}
/**
 * Do the transfer.
 *
 * Input handler is only used if verb is PUT or POST. It may be NULL for GET/HEAD.
 */
	void do_transfer(input_handler* inhandler, output_handler* outhandler);
/**
 * Get status of transfer. Safe to call from another thread while do_transfer is in progress.
 */
	void get_xfer_status(int64_t& dnow, int64_t& dtotal, int64_t& unow, int64_t& utotal);
/**
 * Do global initialization.
 */
	static void global_init();
/**
 * Get final code.
 */
	long get_http_code();
private:
	static int progress(void* userdata, double dltotal, double dlnow, double ultotal, double ulnow);
	int _progress(double dltotal, double dlnow, double ultotal, double ulnow);
	http_request(const http_request&);
	http_request& operator=(const http_request&);
	void* handle;
	text authorization;
	bool has_body;
	double dltotal, dlnow, ultotal, ulnow;
};

/**
 * HTTP asynchronous request structure.
 */
struct http_async_request
{
	http_async_request();
	http_request::input_handler* ihandler;		//Input handler (INPUT).
	http_request::output_handler* ohandler;		//Output handler (INPUT).
	text verb;				//HTTP verb (INPUT)
	text url;				//URL to access (INPUT)
	text authorization;			//Authorization to use (INPUT)
	int64_t final_dl;				//Final amount downloaded (OUTPUT).
	int64_t final_ul;				//Final amound uploaded (OUTPUT).
	text errormsg;				//Final error (OUTPUT).
	long http_code;					//HTTP error code (OUTPUT).
	volatile bool finished;				//Finished flag (semi-transient).
	threads::cv finished_cond;				//This condition variable is fired on finish.
	http_request* req;				//The HTTP request object (TRANSIENT).
	threads::lock m;					//Lock protecting the object (TRANSIENT).
	void get_xfer_status(int64_t& dnow, int64_t& dtotal, int64_t& unow, int64_t& utotal);
	void lauch_async();				//Lauch asynchronous request.
	void cancel();					//Cancel request in flight.
};

/**
 * Property upload.
 */
struct property_upload_request : public http_request::input_handler
{
	//Inherited methods.
	property_upload_request();
	~property_upload_request();
	uint64_t get_length();
	size_t read(char* target, size_t maxread);
	void rewind();
/**
 * Data to upload.
 */
	std::map<text, text> data;
private:
	unsigned state;
	std::map<text, text>::iterator itr;
	size_t sent;
	void str_helper(const text& str, char*& target, size_t& maxread, size_t& x, unsigned next);
	void chr_helper(char ch, char*& target, size_t& maxread, size_t& x, unsigned next);
	void len_helper(size_t len, char*& target, size_t& maxread, size_t& x, unsigned next);
};

//Lowercase a string.
text http_strlower(const text& name);

#endif
