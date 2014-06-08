#include "core/filedownload.hpp"
#include "core/moviedata.hpp"
#include "core/rom.hpp"
#include "interface/romtype.hpp"
#include "library/string.hpp"
#include "library/zip.hpp"

#include <fstream>

namespace
{
	void file_download_thread_trampoline(file_download* d, loaded_rom* rom)
	{
		d->_do_async(*rom);
	}

	class file_download_handler : public http_request::output_handler
	{
	public:
		file_download_handler(const std::string& filename)
		{
			fp.open(filename, std::ios::binary);
			tsize = 0;
		}
		~file_download_handler()
		{
		}
		void header(const std::string& name, const std::string& content)
		{
			//Ignore headers.
		}
		void write(const char* source, size_t srcsize)
		{
			fp.write(source, srcsize);
			tsize += srcsize;
		}
	private:
		std::ofstream fp;
		size_t tsize;
	};
}

file_download::file_download()
{
	finished = false;
	req.ohandler = NULL;
}

file_download::~file_download()
{
	if(req.ohandler) delete req.ohandler;
}

void file_download::cancel()
{
	req.cancel();
	errormsg = "Canceled";
	finished = true;
}

void file_download::do_async(loaded_rom& rom)
{
	tempname = get_temp_file();
	req.ihandler = NULL;
	req.ohandler = new file_download_handler(tempname);
	req.verb = "GET";
	req.url = url;
	try {
		req.lauch_async();
		(new threads::thread(file_download_thread_trampoline, this, &rom))->detach();
	} catch(std::exception& e) {
		req.cancel();
		threads::alock h(m);
		errormsg = e.what();
		finished = true;
		cond.notify_all();
	}
}

std::string file_download::statusmsg()
{
	int64_t dn, dt, un, ut;
	if(finished)
		return (stringfmt() << "Downloading finished").str();
	req.get_xfer_status(dn, dt, un, ut);
	if(dn == 0)
		return "Connecting...";
	else if(dt == 0)
		return (stringfmt() << "Downloading (" << dn << "/<unknown>)").str();
	else if(dn < dt)
		return (stringfmt() << "Downloading (" << (100 * dn / dt) << "%)").str();
	else
		return (stringfmt() << "Downloading finished").str();
}

void file_download::_do_async(loaded_rom& rom)
{
	while(!req.finished) {
		threads::alock h(req.m);
		req.finished_cond.wait(h);
		if(!req.finished)
			continue;
		if(req.errormsg != "") {
			remove(tempname.c_str());
			threads::alock h(m);
			errormsg = req.errormsg;
			finished = true;
			cond.notify_all();
			return;
		}
	}
	delete req.ohandler;
	req.ohandler = NULL;
	if(req.http_code > 299) {
		threads::alock h(m);
		errormsg = (stringfmt() << "Got HTTP error " << req.http_code).str();
		finished = true;
		cond.notify_all();
	}
	//Okay, we got the file.
	std::istream* s = NULL;
	try {
		zip::reader r(tempname);
		unsigned count = 0;
		for(auto i : r) {
			count++;
		}
		if(count == 1) {
			std::istream& s = r[*r.begin()];
			std::ofstream out(tempname2 = get_temp_file(), std::ios::binary);
			while(s) {
				char buf[4096];
				s.read(buf, sizeof(buf));
				out.write(buf, s.gcount());
			}
			delete &s;
		} else {
			tempname2 = tempname;
		}
	} catch(...) {
		if(s) delete s;
		tempname2 = tempname;
	}
	if(tempname != tempname2) remove(tempname.c_str());
	try {
		core_type* gametype = NULL;
		if(!rom.isnull())
			gametype = &rom.get_internal_rom_type();
		else {
			moviefile::brief_info info(tempname2);
			auto sysregs = core_sysregion::find_matching(info.sysregion);
			for(auto i : sysregs)
				if(i->get_type().get_core_identifier() == info.corename)
					gametype = &i->get_type();
			if(!gametype)
				for(auto i : sysregs)
					gametype = &i->get_type();
		}
		auto mv = moviefile::memref(target_slot);
		moviefile::memref(target_slot) = new moviefile(tempname2, *gametype);
		delete mv;
		remove(tempname2.c_str());
	} catch(std::exception& e) {
		remove(tempname2.c_str());
		threads::alock h(m);
		errormsg = e.what();
		finished = true;
		cond.notify_all();
		return;
	}
	//We are done!
	threads::alock h(m);
	finished = true;
	cond.notify_all();
}

urirewrite::rewriter lsnes_uri_rewrite;
