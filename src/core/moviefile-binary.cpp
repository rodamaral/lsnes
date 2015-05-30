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
			text h = s.string_implicit();
			if(n > 2 * ROM_SLOT_COUNT)
				return;
			if(n & 1)
				this->hashxml[n >> 1] = h;
			else
				this->hash[n >> 1] = h;
		}},{TAG_ROMHINT, [this](binarystream::input& s) {
			uint8_t n = s.byte();
			text h = s.string_implicit();
			if(n > ROM_SLOT_COUNT)
				return;
			this->hint[n] = h;
		}}
	}, binarystream::null_default);
}

void moviefile::binary_io(int _stream, rrdata_set& rrd, bool as_state) throw(std::bad_alloc, std::runtime_error)
{
	binarystream::output out(_stream);
	out.string(gametype->get_name());
	moviefile_write_settings<binarystream::output>(out, settings, gametype->get_type().get_settings(),
		[](binarystream::output& s, const text& name, const text& value) -> void {
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
	if(as_state) {
		out.extension(TAG_SAVESTATE, [this](binarystream::output& s) {
			s.number(this->dyn.save_frame);
			s.number(this->dyn.lagged_frames);
			s.number(this->dyn.rtc_second);
			s.number(this->dyn.rtc_subsecond);
			s.number(this->dyn.pollcounters.size());
			for(auto i : this->dyn.pollcounters)
				s.number32(i);
			s.byte(this->dyn.poll_flag ? 0x01 : 0x00);
			s.blob_implicit(this->dyn.savestate);
		}, true, out.numberbytes(dyn.save_frame) + out.numberbytes(dyn.lagged_frames) +
			out.numberbytes(dyn.rtc_second) + out.numberbytes(dyn.rtc_subsecond) +
			out.numberbytes(dyn.pollcounters.size()) + 4 * dyn.pollcounters.size() + 1 +
			dyn.savestate.size());

		out.extension(TAG_HOSTMEMORY, [this](binarystream::output& s) {
			s.blob_implicit(this->dyn.host_memory);
		});

		out.extension(TAG_SCREENSHOT, [this](binarystream::output& s) {
			s.blob_implicit(this->dyn.screenshot);
		}, true, dyn.screenshot.size());

		for(auto i : dyn.sram) {
			out.extension(TAG_SAVE_SRAM, [&i](binarystream::output& s) {
				s.string(i.first);
				s.blob_implicit(i.second);
			});
		}

		for(auto i : dyn.active_macros)
			out.extension(TAG_MACRO, [&i](binarystream::output& s) {
				s.number(i.second);
				s.string_implicit(i.first);
			});
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

	for(auto i : ramcontent) {
		out.extension(TAG_RAMCONTENT, [&i](binarystream::output& s) {
			s.string(i.first);
			s.blob_implicit(i.second);
		});
	}

	int64_t next_bnum = 0;
	std::map<text, uint64_t> branch_table;
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
	text tmp = in.string();
	text next_branch;
	std::map<uint64_t, text> branch_table;
	uint64_t next_bnum = 0;
	try {
		gametype = &romtype.lookup_sysregion(tmp);
	} catch(std::bad_alloc& e) {
		throw;
	} catch(std::exception& e) {
		throw std::runtime_error("Illegal game type '" + tmp + "'");
	}
	while(in.byte()) {
		text name = in.string();
		settings[name] = in.string();
	}
	auto ctrldata = gametype->get_type().controllerconfig(settings);
	portctrl::type_set& ports = portctrl::type_set::make(ctrldata.ports, ctrldata.portindex());
	input = NULL;

	this->dyn.save_frame = 0;	//If no savestate, ensure this is interpretted as a movie.
	in.extension({
		{TAG_ANCHOR_SAVE, [this](binarystream::input& s) {
			s.blob_implicit(this->anchor_savestate);
		}},{TAG_AUTHOR, [this](binarystream::input& s) {
			text a = s.string();
			text b = s.string_implicit();
			this->authors.push_back(std::make_pair(a, b));
		}},{TAG_CORE_VERSION, [this](binarystream::input& s) {
			this->coreversion = s.string_implicit();
		}},{TAG_GAMENAME, [this](binarystream::input& s) {
			this->gamename = s.string_implicit();
		}},{TAG_HOSTMEMORY, [this](binarystream::input& s) {
			s.blob_implicit(this->dyn.host_memory);
		}},{TAG_MACRO, [this](binarystream::input& s) {
			uint64_t n = s.number();
			this->dyn.active_macros[s.string_implicit()] = n;
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
			text a = s.string();
			s.blob_implicit(this->movie_sram[a]);
		}},{TAG_RAMCONTENT, [this](binarystream::input& s) {
			text a = s.string();
			s.blob_implicit(this->ramcontent[a]);
		}},{TAG_MOVIE_TIME, [this](binarystream::input& s) {
			this->movie_rtc_second = s.number();
			this->movie_rtc_subsecond = s.number();
		}},{TAG_PROJECT_ID, [this](binarystream::input& s) {
			this->projectid = s.string_implicit();
		}},{TAG_ROMHASH, [this](binarystream::input& s) {
			uint8_t n = s.byte();
			text h = s.string_implicit();
			if(n > 2 * ROM_SLOT_COUNT)
				return;
			if(n & 1)
				romxml_sha256[n >> 1] = h;
			else
				romimg_sha256[n >> 1] = h;
		}},{TAG_ROMHINT, [this](binarystream::input& s) {
			uint8_t n = s.byte();
			text h = s.string_implicit();
			if(n > ROM_SLOT_COUNT)
				return;
			namehint[n] = h;
		}},{TAG_RRDATA, [this](binarystream::input& s) {
			s.blob_implicit(this->c_rrdata);
			this->rerecords = (stringfmt() << rrdata_set::count(c_rrdata)).str();
		}},{TAG_SAVE_SRAM, [this](binarystream::input& s) {
			text a = s.string();
			s.blob_implicit(this->dyn.sram[a]);
		}},{TAG_SAVESTATE, [this](binarystream::input& s) {
			this->dyn.save_frame = s.number();
			this->dyn.lagged_frames = s.number();
			this->dyn.rtc_second = s.number();
			this->dyn.rtc_subsecond = s.number();
			this->dyn.pollcounters.resize(s.number());
			for(auto& i : this->dyn.pollcounters)
				i = s.number32();
			this->dyn.poll_flag = (s.byte() != 0);
			s.blob_implicit(this->dyn.savestate);
		}},{TAG_SCREENSHOT, [this](binarystream::input& s) {
			s.blob_implicit(this->dyn.screenshot);
		}},{TAG_SUBTITLE, [this](binarystream::input& s) {
			uint64_t f = s.number();
			uint64_t l = s.number();
			text x = s.string_implicit();
			this->subtitles[moviefile_subtiming(f, l)] = x;
		}}
	}, binarystream::null_default);

	create_default_branch(ports);
}

moviefile_branch_extractor_binary::moviefile_branch_extractor_binary(const text& filename)
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

std::set<text> moviefile_branch_extractor_binary::enumerate()
{
	std::set<text> r;
	text name;
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

void moviefile_branch_extractor_binary::read(const text& name, portctrl::frame_vector& v)
{
	text mname;
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

moviefile_sram_extractor_binary::moviefile_sram_extractor_binary(const text& filename)
{
	s = open(filename.c_str(), O_RDONLY | EXTRA_OPENFLAGS);
	if(s < 0) {
		int err = errno;
		(stringfmt() << "Can't open file '" << filename << "' for reading: " << strerror(err)).throwex();
	}
}

moviefile_sram_extractor_binary::~moviefile_sram_extractor_binary()
{
}

std::set<text> moviefile_sram_extractor_binary::enumerate()
{
	std::set<text> r;
	text name;
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
		{TAG_SAVESTATE, [this, &r](binarystream::input& s) {
			r.insert("");
		}},{TAG_SAVE_SRAM, [this, &r](binarystream::input& s) {
			r.insert(s.string());
		}}
	}, binarystream::null_default);

	return r;
}

void moviefile_sram_extractor_binary::read(const text& name, std::vector<char>& v)
{
	//Char and uint8_t are the same representation, right?
	std::vector<char>* _v = &v;
	text mname = name;
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
		{TAG_SAVESTATE, [this, mname, _v, &done](binarystream::input& s) {
			if(mname == "") {
				//This savestate.
				s.number();			//Frame of save.
				s.number();			//Lagged frames.
				s.number();			//RTC second.
				s.number();			//RTC subsecond.
				size_t pctrs = s.number();	//Number of poll counters.
				for(size_t i = 0; i < pctrs; i++)
					s.number32();		//Poll counters.
				s.byte();			//Poll flag.
				s.blob_implicit(*_v);
				done = true;
			}
		}},{TAG_SAVE_SRAM, [this, mname, _v, &done](binarystream::input& s) {
			text sname = s.string();
			if(sname == mname) {
				//This SRAM.
				s.blob_implicit(*_v);
				done = true;
			}
		}}
	}, binarystream::null_default);
	if(!done) {
		if(name != "")
			(stringfmt() << "Can't find branch '" << name << "' in file.").throwex();
		else
			(stringfmt() << "Can't find savestate in file.").throwex();
	}
}
