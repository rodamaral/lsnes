#ifndef _filedownload__hpp__included__
#define _filedownload__hpp__included__

#include "library/threadtypes.hpp"
#include "library/httpreq.hpp"
#include <string>
#include <list>
#include <vector>

struct file_download
{
	//Variables.
	std::string url;
	std::string target_slot;
	//Ctor
	file_download();
	~file_download();
	//Lauch.
	void do_async();
	void cancel();
	//Status.
	volatile bool finished;  //This signals download finishing, call finish().
	std::string errormsg;
	http_async_request req;
	std::string statusmsg();
	cv_class cond;
	mutex_class m;
	//Internal.
	void _do_async();
	std::string tempname;
	std::string tempname2;
};


#endif
