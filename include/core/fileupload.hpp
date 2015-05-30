#ifndef _fileupload__hpp__included__
#define _fileupload__hpp__included__

#include "library/threads.hpp"
#include "library/httpreq.hpp"
#include "library/httpauth.hpp"
#include "library/text.hpp"
#include <string>
#include <list>
#include <vector>

struct file_upload
{
	//Variables.
	text base_url;
	std::vector<char> content;
	text filename;
	text title;
	text description;
	text gamename;
	bool hidden;
	//Ctor
	file_upload();
	~file_upload();
	//Lauch.
	void do_async();
	void _do_async();
	void cancel();
	//Status.
	std::list<text> get_messages();
	int get_progress_ppm(); //-1 => No progress.
	volatile bool finished;
	volatile bool success;
	text final_url;
	//Vars.
	dh25519_http_auth* dh25519;
	http_async_request* req;
	std::list<text> msgs;
	threads::lock m;
	void add_msg(const text& msg);
};

void get_dh25519_pubkey(uint8_t* out);

#endif
