#ifndef _filedownload__hpp__included__
#define _filedownload__hpp__included__

#include "library/threads.hpp"
#include "library/httpreq.hpp"
#include "library/urirewrite.hpp"
#include <string>
#include <list>
#include <vector>

class loaded_rom;

struct file_download
{
	//Variables.
	std::string url;
	std::string target_slot;
	//Ctor
	file_download();
	~file_download();
	//Lauch.
	void do_async(loaded_rom& rom);
	void cancel();
	//Status.
	volatile bool finished;  //This signals download finishing, call finish().
	std::string errormsg;
	http_async_request req;
	std::string statusmsg();
	threads::cv cond;
	threads::lock m;
	//Internal.
	void _do_async(loaded_rom& rom);
	std::string tempname;
	std::string tempname2;
};

extern urirewrite::rewriter lsnes_uri_rewrite;

#endif
