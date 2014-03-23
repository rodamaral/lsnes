#ifndef _fileupload__hpp__included__
#define _fileupload__hpp__included__

#include "library/threads.hpp"
#include "library/httpreq.hpp"
#include "library/httpauth.hpp"
#include <string>
#include <list>
#include <vector>

struct file_upload
{
	//Variables.
	std::string base_url;
	std::vector<char> content;
	std::string filename;
	std::string title;
	std::string description;
	std::string gamename;
	bool hidden;
	//Ctor
	file_upload();
	~file_upload();
	//Lauch.
	void do_async();
	void _do_async();
	void cancel();
	//Status.
	std::list<std::string> get_messages();
	int get_progress_ppm(); //-1 => No progress.
	volatile bool finished;
	volatile bool success;
	std::string final_url;
	//Vars.
	dh25519_http_auth* dh25519;
	http_async_request* req;
	std::list<std::string> msgs;
	threads::lock m;
	void add_msg(const std::string& msg);
};

void get_dh25519_pubkey(uint8_t* out);

#endif
