#include "core/moviefile-binary.hpp"
#include "core/moviefile-common.hpp"
#include "core/moviefile.hpp"
#include "library/binarystream.hpp"
#include "library/minmax.hpp"
#include "library/serialization.hpp"
#include "library/string.hpp"
#include "library/zip.hpp"

#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#if defined(_WIN32) || defined(_WIN64) || defined(TEST_WIN32_CODE)
//FUCK YOU. SERIOUSLY.
#define EXTRA_OPENFLAGS O_BINARY
#else
#define EXTRA_OPENFLAGS 0
#endif

void moviefile::brief_info::binary_io(int _stream)
{
	binarystream::input in(_stream);
	sysregion = in.string();
	//Discard the settings.
	while(in.byte()) {
		in.string();
		in.string();
	}
	in.extension({
		{TAG_CORE_VERSION, [this](binarystream::input& s) {
			this->corename = s.string_implicit();
		}},{TAG_PROJECT_ID, [this](binarystream::input& s) {
			this->projectid = s.string_implicit();
		}},{TAG_SAVESTATE, [this](binarystream::input& s) {
			this->current_frame = s.number();
		}},{TAG_RRDATA, [this](binarystream::input& s) {
			std::vector<char> c_rrdata;
			s.blob_implicit(c_rrdata);
			this->rerecords = rrdata_set::count(c_rrdata);
		}},{TAG_ROMHASH, [this](binarystream::input& s) {
			uint8_t n = s.byte();
			std::string h = s.string_implicit();
			if(n > 2 * ROM_SLOT_COUNT)
				return;
			if(n & 1)
				this->hashxml[n >> 1] = h;
			else
				this->hash[n >> 1] = h;
		}},{TAG_ROMHINT, [this](binarystream::input& s) {
			uint8_t n = s.byte();
			std::string h = s.string_implicit();
			if(n > ROM_SLOT_COUNT)
				return;
			this->hint[n] = h;
		}}
	}, binarystream::null_default);
}

void moviefile::binary_io(int _stream, rrdata_set& rrd) throw(std::bad_alloc, std::runtime_error)
{
	binarystream::output out(_stream);
	out.string(gametype->get_name());
	moviefile_write_settings<binarystream::output>(out, settings, gametype->get_type().get_settings(),
		[](binarystream::output& s, const std::string& name, const std::string& value) -> void {
			s.byte(0x01);
			s.string(name);
			s.string(value);
		});
	out.byte(0x00);

	out.extension(TAG_MOVIE_TIME, [this](binarystream::output& s) {
		s.number(this->movie_rtc_second);
		s.number(this->movie_rtc_subsecond);
	});

	out.extension(TAG_PROJECT_ID, [this](binarystream::output& s) {
		s.string_implicit(this->projectid);
	});

	out.extension(TAG_CORE_VERSION, [this](binarystream::output& s) {
		this->coreversion = this->gametype->get_type().get_core_identifier();
		s.string_implicit(this->coreversion);
	});

	for(unsigned i = 0; i < ROM_SLOT_COUNT; i++) {
		out.extension(TAG_ROMHASH, [this, i](binarystream::output& s) {
			if(!this->romimg_sha256[i].length()) return;
			s.byte(2 * i);
			s.string_implicit(this->romimg_sha256[i]);
		});
		out.extension(TAG_ROMHASH, [this, i](binarystream::output& s) {
			if(!this->romxml_sha256[i].length()) return;
			s.byte(2 * i + 1);
			s.string_implicit(this->romxml_sha256[i]);
		});
		out.extension(TAG_ROMHINT, [this, i](binarystream::output& s) {
			if(!this->namehint[i].length()) return;
			s.byte(i);
			s.string_implicit(this->namehint[i]);
		});
	}

	out.extension(TAG_RRDATA, [this, &rrd](binarystream::output& s) {
		std::vector<char> _rrd;
		rrd.write(_rrd);
		s.blob_implicit(_rrd);
	});

	for(auto i : movie_sram)
		out.extension(TAG_MOVIE_SRAM, [&i](binarystream::output& s) {
			s.string(i.first);
			s.blob_implicit(i.second);
		});

	out.extension(TAG_ANCHOR_SAVE, [this](binarystream::output& s) {
		s.blob_implicit(this->anchor_savestate);
	});
	if(is_savestate) {
		out.extension(TAG_SAVESTATE, [this](binarystream::output& s) {
			s.number(this->save_frame);
			s.number(this->lagged_frames);
			s.number(this->rtc_second);
			s.number(this->rtc_subsecond);
			s.number(this->pollcounters.size());
			for(auto i : this->pollcounters)
				s.number32(i);
			s.byte(this->poll_flag ? 0x01 : 0x00);
			s.blob_implicit(this->savestate);
		}, true, out.numberbytes(save_frame) + out.numberbytes(lagged_frames) + out.numberbytes(rtc_second) +
			out.numberbytes(rtc_subsecond) + out.numberbytes(pollcounters.size()) +
			4 * pollcounters.size() + 1 + savestate.size());

		out.extension(TAG_HOSTMEMORY, [this](binarystream::output& s) {
			s.blob_implicit(this->host_memory);
		});

		out.extension(TAG_SCREENSHOT, [this](binarystream::output& s) {
			s.blob_implicit(this->screenshot);
		}, true, screenshot.size());

		for(auto i : sram) {
			out.extension(TAG_SAVE_SRAM, [&i](binarystream::output& s) {
				s.string(i.first);
				s.blob_implicit(i.second);
			});
		}
	}

	out.extension(TAG_GAMENAME, [this](binarystream::output& s) {
		s.string_implicit(this->gamename);
	});

	for(auto i : subtitles)
		out.extension(TAG_SUBTITLE, [&i](binarystream::output& s) {
			s.number(i.first.get_frame());
			s.number(i.first.get_length());
			s.string_implicit(i.second);
		});

	for(auto i : authors)
		out.extension(TAG_AUTHOR, [&i](binarystream::output& s) {
			s.string(i.first);
			s.string_implicit(i.second);
		});

	for(auto i : active_macros)
		out.extension(TAG_MACRO, [&i](binarystream::output& s) {
			s.number(i.second);
			s.string_implicit(i.first);
		});

	for(auto i : ramcontent) {
		out.extension(TAG_RAMCONTENT, [&i](binarystream::output& s) {
			s.string(i.first);
			s.blob_implicit(i.second);
		});
	}

	int64_t next_bnum = 0;
	std::map<std::string, uint64_t> branch_table;
	for(auto& i : branches) {
		branch_table[i.first] = next_bnum++;
		out.extension(TAG_BRANCH_NAME, [&i](binarystream::output& s) {
			s.string_implicit(i.first);
		}, false, i.first.length());
		uint32_t tag = (&i.second == input) ? TAG_MOVIE : TAG_BRANCH;
		out.extension(tag, [&i](binarystream::output& s) {
			i.second.save_binary(s);
		}, true, i.second.binary_size());
	}
}

void moviefile::binary_io(int _stream, core_type& romtype) throw(std::bad_alloc, std::runtime_error)
{
	binarystream::input in(_stream);
	std::string tmp = in.string();
	std::string next_branch;
	std::map<uint64_t, std::string> branch_table;
	uint64_t next_bnum = 0;
	try {
		gametype = &romtype.lookup_sysregion(tmp);
	} catch(std::bad_alloc& e) {
		throw;
	} catch(std::exception& e) {
		throw std::runtime_error("Illegal game type '" + tmp + "'");
	}
	while(in.byte()) {
		std::string name = in.string();
		settings[name] = in.string();
	}
	auto ctrldata = gametype->get_type().controllerconfig(settings);
	portctrl::type_set& ports = portctrl::type_set::make(ctrldata.ports, ctrldata.portindex());
	input = NULL;

	in.extension({
		{TAG_ANCHOR_SAVE, [this](binarystream::input& s) {
			s.blob_implicit(this->anchor_savestate);
		}},{TAG_AUTHOR, [this](binarystream::input& s) {
			std::string a = s.string();
			std::string b = s.string_implicit();
			this->authors.push_back(std::make_pair(a, b));
		}},{TAG_CORE_VERSION, [this](binarystream::input& s) {
			this->coreversion = s.string_implicit();
		}},{TAG_GAMENAME, [this](binarystream::input& s) {
			this->gamename = s.string_implicit();
		}},{TAG_HOSTMEMORY, [this](binarystream::input& s) {
			s.blob_implicit(this->host_memory);
		}},{TAG_MACRO, [this](binarystream::input& s) {
			uint64_t n = s.number();
			this->active_macros[s.string_implicit()] = n;
		}},{TAG_BRANCH_NAME, [this, &branch_table, &next_bnum, &next_branch](binarystream::input& s) {
			branch_table[next_bnum++] = next_branch = s.string_implicit();
		}},{TAG_MOVIE, [this, &ports, &next_branch](binarystream::input& s) {
			branches[next_branch].clear(ports);
			branches[next_branch].load_binary(s);
			input = &branches[next_branch];
		}},{TAG_BRANCH, [this, &ports, &next_branch](binarystream::input& s) {
			branches[next_branch].clear(ports);
			branches[next_branch].load_binary(s);
		}},{TAG_MOVIE_SRAM, [this](binarystream::input& s) {
			std::string a = s.string();
			s.blob_implicit(this->movie_sram[a]);
		}},{TAG_RAMCONTENT, [this](binarystream::input& s) {
			std::string a = s.string();
			s.blob_implicit(this->ramcontent[a]);
		}},{TAG_MOVIE_TIME, [this](binarystream::input& s) {
			this->movie_rtc_second = s.number();
			this->movie_rtc_subsecond = s.number();
		}},{TAG_PROJECT_ID, [this](binarystream::input& s) {
			this->projectid = s.string_implicit();
		}},{TAG_ROMHASH, [this](binarystream::input& s) {
			uint8_t n = s.byte();
			std::string h = s.string_implicit();
			if(n > 2 * ROM_SLOT_COUNT)
				return;
			if(n & 1)
				romxml_sha256[n >> 1] = h;
			else
				romimg_sha256[n >> 1] = h;
		}},{TAG_ROMHINT, [this](binarystream::input& s) {
			uint8_t n = s.byte();
			std::string h = s.string_implicit();
			if(n > ROM_SLOT_COUNT)
				return;
			namehint[n] = h;
		}},{TAG_RRDATA, [this](binarystream::input& s) {
			s.blob_implicit(this->c_rrdata);
			this->rerecords = (stringfmt() << rrdata_set::count(c_rrdata)).str();
		}},{TAG_SAVE_SRAM, [this](binarystream::input& s) {
			std::string a = s.string();
			s.blob_implicit(this->sram[a]);
		}},{TAG_SAVESTATE, [this](binarystream::input& s) {
			this->is_savestate = true;
			this->save_frame = s.number();
			this->lagged_frames = s.number();
			this->rtc_second = s.number();
			this->rtc_subsecond = s.number();
			this->pollcounters.resize(s.number());
			for(auto& i : this->pollcounters)
				i = s.number32();
			this->poll_flag = (s.byte() != 0);
			s.blob_implicit(this->savestate);
		}},{TAG_SCREENSHOT, [this](binarystream::input& s) {
			s.blob_implicit(this->screenshot);
		}},{TAG_SUBTITLE, [this](binarystream::input& s) {
			uint64_t f = s.number();
			uint64_t l = s.number();
			std::string x = s.string_implicit();
			this->subtitles[moviefile_subtiming(f, l)] = x;
		}}
	}, binarystream::null_default);

	create_default_branch(ports);
}

moviefile_branch_extractor_binary::moviefile_branch_extractor_binary(const std::string& filename)
{
	s = open(filename.c_str(), O_RDONLY | EXTRA_OPENFLAGS);
	if(s < 0) {
		int err = errno;
		(stringfmt() << "Can't open file '" << filename << "' for reading: " << strerror(err)).throwex();
	}
}

moviefile_branch_extractor_binary::~moviefile_branch_extractor_binary()
{
}

std::set<std::string> moviefile_branch_extractor_binary::enumerate()
{
	std::set<std::string> r;
	std::string name;
	if(lseek(s, 5, SEEK_SET) < 0) {
		int err = errno;
		(stringfmt() << "Can't read the file: " << strerror(err)).throwex();
	}
	binarystream::input b(s);
	//Skip the headers.
	b.string();
	while(b.byte()) {
		b.string();
		b.string();
	}
	//Okay, read the extension packets.
	b.extension({
		{TAG_BRANCH_NAME, [this, &name](binarystream::input& s) {
			name = s.string_implicit();
		}},{TAG_MOVIE, [this, &r, &name](binarystream::input& s) {
			r.insert(name);
		}},{TAG_BRANCH, [this, &r, &name](binarystream::input& s) {
			r.insert(name);
		}}
	}, binarystream::null_default);

	return r;
}

void moviefile_branch_extractor_binary::read(const std::string& name, portctrl::frame_vector& v)
{
	std::string mname;
	if(lseek(s, 5, SEEK_SET) < 0) {
		int err = errno;
		(stringfmt() << "Can't read the file: " << strerror(err)).throwex();
	}
	binarystream::input b(s);
	bool done = false;
	//Skip the headers.
	b.string();
	while(b.byte()) {
		b.string();
		b.string();
	}
	//Okay, read the extension packets.
	b.extension({
		{TAG_BRANCH_NAME, [this, &mname](binarystream::input& s) {
			mname = s.string_implicit();
		}},{TAG_MOVIE, [this, &v, &mname, &name, &done](binarystream::input& s) {
			if(name != mname)
				return;
			v.clear();
			v.load_binary(s);
			done = true;
		}},{TAG_BRANCH, [this, &v, &mname, &name, &done](binarystream::input& s) {
			if(name != mname)
				return;
			v.clear();
			v.load_binary(s);
			done = true;
		}}
	}, binarystream::null_default);
	if(!done)
		(stringfmt() << "Can't find branch '" << name << "' in file.").throwex();
}
