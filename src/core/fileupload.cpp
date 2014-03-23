#include "core/fileupload.hpp"
#include "core/misc.hpp"
#include "library/streamcompress.hpp"
#include "library/httpreq.hpp"
#include "library/httpauth.hpp"
#include "library/string.hpp"
#include "library/skein.hpp"
#include "library/curve25519.hpp"
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
}

#ifdef TEST_FILEUPLOAD_CODE

namespace
{
	class file_input
	{
	public:
		typedef char char_type;
		typedef boost::iostreams::source_tag category;

		file_input(const std::string& file)
		{
			fp = new std::ifstream(file, std::ios::binary);
			if(!*fp) throw std::runtime_error("Can't open input file");
		}

		void close()
		{
			delete fp;
		}

		std::streamsize read(char* s, std::streamsize x)
		{
			fp->read(s, x);
			if(!fp->gcount() && !*fp) return -1;
			return fp->gcount();
		}

		~file_input()
		{
		}
	private:
		file_input& operator=(const file_input& f);
		std::ifstream* fp;
	};

	std::vector<char> load_vec(const std::string& fn)
	{
		std::vector<char> out;
		boost::iostreams::filtering_istream* s = new boost::iostreams::filtering_istream();
		s->push(file_input(fn));
		boost::iostreams::back_insert_device<std::vector<char>> rd(out);
		boost::iostreams::copy(*s, rd);
		delete s;
		return out;
	}

	std::string load_str(const std::string& fn)
	{
		std::vector<char> out = load_vec(fn);
		return std::string(&out[0], out.size());
	}
}

int main(int argc, char** argv)
{
	uint8_t privkey[32] = {0x76,0x5d,0x92,0xdf,0x54,0x9a,0xcb,0x67,0xb5,0x0b,0x72,0x62,0xeb,0xd9,0x99,0x2d,
		0xa6,0xb6,0xee,0x97,0x86,0x2a,0x91,0xa6,0x55,0x30,0xac,0x90,0xe1,0xef,0x6d,0x05};
	file_upload upload;
	memcpy(upload.dh25519_privkey, privkey, 32);
	upload.base_url = "http://dev.tasvideos.org/userfiles/api";
	upload.filename = argv[1];
	auto r = regex(".*/([^/]+)", upload.filename);
	if(r) upload.filename = r[1];
	upload.title = argv[2];
	if(argc > 3 && argv[3][0]) upload.description = load_str(argv[3]);
	upload.content = load_vec(argv[1]);
	if(argc > 4 && argv[4][0]) upload.gamename = argv[4];
	if(argc > 5 && argv[5][0]) upload.hidden = (argv[5][0] != '0');

	upload.do_async();
	bool last_progress = false;
	while(!upload.finished) {
		usleep(100000);
		for(auto i : upload.get_messages()) {
			if(last_progress) std::cout << std::endl;
			std::cout << i << std::endl;
			last_progress = false;
		}
		auto progress = upload.get_progress_ppm();
		if(progress >= 0) {
			std::cout << "\e[1GUpload: " << (double)progress / 10000 << "%           " << std::flush;
			last_progress = true;
		}
	}
	for(auto i : upload.get_messages())
		std::cout << i << std::endl;
	return upload.success ? 0 : 1;
}

#endif
