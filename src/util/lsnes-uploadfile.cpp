#include <fstream>
#include <string>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "core/fileupload.hpp"
#include "library/string.hpp"
#include <boost/iostreams/categories.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/stream_buffer.hpp>
#include <boost/iostreams/filter/symmetric.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <sys/time.h>
#include <unistd.h>


namespace
{
	std::vector<char> load_vec(const std::string& fn)
	{
		std::vector<char> out;
		std::ifstream f(fn);
		if(!f)
			throw std::runtime_error("Can't open file '" + fn + "'");
		boost::iostreams::back_insert_device<std::vector<char>> rd(out);
		boost::iostreams::copy(f, rd);
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
	try {
		std::string mainfile;
		file_upload upload;
		upload.hidden = false;
		for(int i = 1; i < argc; i++) {
			std::string arg = argv[i];
			regex_results r;
			if(arg == "--show-pubkey") {
				uint8_t key[32];
				get_dh25519_pubkey(key);
				char out[65];
				out[64] = 0;
				for(unsigned i = 0; i < 32; i++)
					sprintf(out + 2 * i, "%02x", key[i]);
				std::cout << out << std::endl;
				return 0;
			} else if(r = regex("--url=(.+)", arg)) {
				upload.base_url = r[1];
			} else if(r = regex("--filename=(.+)", arg)) {
				upload.filename = r[1];
			} else if(r = regex("--title=(.*)", arg)) {
				upload.title = r[1];
			} else if(r = regex("--description-file=(.+)", arg)) {
				upload.description = load_str(r[1]);
			} else if(r = regex("--gamename=(.+)", arg)) {
				upload.gamename = r[1];
			} else if(r = regex("--hidden", arg)) {
				upload.hidden = true;
			} else if(r = regex("([^-].*)", arg)) {
				mainfile = r[1];
			} else
				throw std::runtime_error("Unrecognized argument '" + arg + "'");
		}
		if(upload.base_url == "")
			throw std::runtime_error("--url=foo is needed");
		if(mainfile == "")
			throw std::runtime_error("File to upload is needed");
		if(upload.filename == "") {
			upload.filename = mainfile;
			auto r = regex(".*/([^/]+)", upload.filename);
			if(r) upload.filename = r[1];
		}
		upload.content = load_vec(mainfile);

		upload.do_async();
		int last_progress = -1;
		while(!upload.finished) {
			usleep(1000000);
			for(auto i : upload.get_messages())
				std::cout << i << std::endl;
			auto progress = upload.get_progress_ppm();
			if(progress >= 0 && progress != last_progress) {
				std::cout << "Upload: " << (double)progress / 10000 << "%" << std::endl;
				last_progress = progress;
			}
		}
		for(auto i : upload.get_messages())
			std::cout << i << std::endl;
		return upload.success ? 0 : 1;
	} catch(std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
}
