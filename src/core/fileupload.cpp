#include "core/fileupload.hpp"
#include "core/misc.hpp"
#include "library/curve25519.hpp"
#include "library/httpauth.hpp"
#include "library/httpreq.hpp"
#include "library/skein.hpp"
#include "library/streamcompress.hpp"
#include "library/string.hpp"

#include <functional>
#include <fstream>
#include <boost/iostreams/categories.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/stream_buffer.hpp>
#include <boost/iostreams/filter/symmetric.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/device/back_inserter.hpp>

namespace
{
	void file_upload_trampoline(file_upload* x)
	{
		x->_do_async();
	}

	struct upload_output_handler : http_request::output_handler
	{
		upload_output_handler(std::function<void(std::string&)> _output_cb)
		{
			output_cb = _output_cb;
		}
		~upload_output_handler() {}
		void header(const std::string& name, const std::string& content)
		{
			if(http_strlower(name) == "location") location = content;
		}
		void write(const char* source, size_t srcsize)
		{
			std::string x(source, srcsize);
			while(x.find_first_of("\n") < x.length()) {
				size_t split = x.find_first_of("\n");
				std::string line = x.substr(0, split);
				x = x.substr(split + 1);
				incomplete_line += line;
				while(incomplete_line.length() > 0 &&
					incomplete_line[incomplete_line.length() - 1] == '\r')
					incomplete_line = incomplete_line.substr(0, incomplete_line.length() - 1);
				output_cb(incomplete_line);
				incomplete_line = "";
			}
			if(x != "") incomplete_line += x;
		}
		void flush()
		{
			if(incomplete_line != "") output_cb(incomplete_line);
		}
		std::string get_location()
		{
			return location;
		}
	private:
		std::function<void(std::string&)> output_cb;
		std::string location;
		std::string incomplete_line;
	};

	void compress(std::vector<char>& buf, std::string& output, std::string& compression)
	{
		streamcompress::base* X = NULL;
		try {
			if(!X) {
				X = streamcompress::base::create_compressor("xz", "level=7,extreme=true");
				compression = "xz";
			}
		} catch(...) {
		}
		try {
			if(!X) {
				X = streamcompress::base::create_compressor("gzip", "level=7");
				compression = "gzip";
			}
		} catch(...) {
		}

		std::vector<char> out;
		boost::iostreams::filtering_istream* s = new boost::iostreams::filtering_istream();
		if(X) s->push(streamcompress::iostream(X));
		s->push(boost::iostreams::array_source(&buf[0], buf.size()));
		boost::iostreams::back_insert_device<std::vector<char>> rd(out);
		boost::iostreams::copy(*s, rd);
		delete s;
		if(X) delete X;
		output = std::string(&out[0], out.size());
	}

	void load_dh25519_key(uint8_t* out)
	{
		std::string path = get_config_path() + "/dh25519.key";
		std::ifstream fp(path, std::ios::binary);
		if(!fp)
			throw std::runtime_error("Can't open dh25519 keyfile");
		skein::hash h(skein::hash::PIPE_512, 256);
		while(true) {
			char buf[4096];
			fp.read(buf, sizeof(buf));
			if(fp.gcount() == 0) break;
			h.write((const uint8_t*)buf, fp.gcount());
			skein::zeroize(buf, fp.gcount());
		}
		h.read(out);
		curve25519_clamp(out);
	}
}

file_upload::file_upload()
{
	dh25519 = NULL;
	req = NULL;
	finished = false;
	success = false;
}

file_upload::~file_upload()
{
	if(dh25519) delete dh25519;
	if(req) delete req;
}

void file_upload::do_async()
{
	(new threads::thread(file_upload_trampoline, this))->detach();
}

void file_upload::_do_async()
{
	uint8_t key[32];
	load_dh25519_key(key);
	dh25519 = new dh25519_http_auth(key);
	skein::zeroize(key, sizeof(key));
	{
		http_async_request obtainkey;
		{
			threads::alock h(m);
			req = &obtainkey;
		}
		http_request::null_input_handler nullinput;
		auto auth = dh25519;
		http_request::www_authenticate_extractor extractor([auth](const std::string& content) {
			auth->parse_auth_response(content);
		});
		obtainkey.ihandler = &nullinput;
		obtainkey.ohandler = &extractor;
		obtainkey.verb = "PUT";
		obtainkey.url = base_url;
		obtainkey.authorization = dh25519->format_get_session_request();
		add_msg("Obtaining short-term credentials...");
		obtainkey.lauch_async();
		while(!obtainkey.finished) {
			threads::alock hx(obtainkey.m);
			obtainkey.finished_cond.wait(hx);
		}
		if(obtainkey.errormsg != "") {
			add_msg((stringfmt() << "Failed: " << obtainkey.errormsg).str());
			{ threads::alock h(m); req = NULL; }
			finished = true;
			return;
		}
		if(obtainkey.http_code != 401) {
			add_msg((stringfmt() << "Failed: Expected 401, got " << obtainkey.http_code).str());
			{ threads::alock h(m); req = NULL; }
			finished = true;
			return;
		}
		if(!dh25519->is_ready()) {
			add_msg((stringfmt() << "Failed: Authenticator is not ready!").str());
			{ threads::alock h(m); req = NULL; }
			finished = true;
			return;
		}
		add_msg("Got short-term credentials.");
		{ threads::alock h(m); req = NULL; }
	}
	{
		http_async_request upload;
		{
			threads::alock h(m);
			req = &upload;
		}
		property_upload_request input;
		upload_output_handler output([this](const std::string& msg) { add_msg(msg); });

		input.data["filename"] = filename;
		if(title != "") input.data["title"] = title;
		if(description != "") input.data["description"] = description;
		if(gamename != "") input.data["game"] = gamename;
		input.data["hidden"] = hidden ? "1" : "0";
		compress(content, input.data["content"], input.data["compression"]);

		upload.ihandler = &input;
		upload.ohandler = &output;
		upload.verb = "PUT";
		upload.url = base_url;
		add_msg("Hashing file...");
		auto authobj = dh25519->start_request(upload.url, upload.verb);
		while(true) {
			char buf[4096];
			size_t r = input.read(buf, sizeof(buf));
			if(!r) break;
			authobj.hash((const uint8_t*)buf, r);
		}
		upload.authorization = authobj.get_authorization();
		input.rewind();
		add_msg("Uploading file...");
		upload.lauch_async();
		while(!upload.finished) {
			threads::alock hx(upload.m);
			upload.finished_cond.wait(hx);
		}
		output.flush();
		if(upload.errormsg != "") {
			add_msg((stringfmt() << "Failed: " << upload.errormsg).str());
			finished = true;
			{ threads::alock h(m); req = NULL; }
			return;
		}
		if(upload.http_code != 201) {
			add_msg((stringfmt() << "Failed: Expected 201, got " << upload.http_code).str());
			finished = true;
			{ threads::alock h(m); req = NULL; }
			return;
		}
		add_msg((stringfmt() << "Sucessful! URL: " << output.get_location()).str());
		final_url = output.get_location();
		finished = true;
		success = true;
		{ threads::alock h(m); req = NULL; }
	}
}

void file_upload::cancel()
{
	threads::alock h(m);
	if(req) req->cancel();
}

std::list<std::string> file_upload::get_messages()
{
	threads::alock h(m);
	std::list<std::string> x = msgs;
	msgs.clear();
	return x;
}

int file_upload::get_progress_ppm()
{
	threads::alock h(m);
	int ppm = -1;
	if(req) {
		int64_t dnow, dtotal, unow, utotal;
		req->get_xfer_status(dnow, dtotal, unow, utotal);
		if(utotal)
			return 1000000 * unow / utotal;
	}
	return ppm;
}

void file_upload::add_msg(const std::string& msg)
{
	threads::alock h(m);
	msgs.push_back(msg);
}

void get_dh25519_pubkey(uint8_t* out)
{
	uint8_t privkey[32];
	load_dh25519_key(privkey);
	curve25519_clamp(privkey);
	curve25519(out, privkey, curve25519_base);
	skein::zeroize(privkey, sizeof(privkey));
}
