#include "lsnes.hpp"
#include "zip.hpp"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
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
	uint32_t read32(const unsigned char* buf, unsigned offset = 0, unsigned modulo = 4) throw()
	{
		return (uint32_t)buf[offset % modulo] |
			((uint32_t)buf[(offset + 1) % modulo] << 8) |
			((uint32_t)buf[(offset + 2) % modulo] << 16) |
			((uint32_t)buf[(offset + 3) % modulo] << 24);
	}

	uint16_t read16(const unsigned char* buf) throw()
	{
		return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
	}

	void write16(unsigned char* buf, uint16_t value) throw()
	{
		buf[0] = (value) & 0xFF;
		buf[1] = (value >> 8) & 0xFF;
	}

	void write32(unsigned char* buf, uint32_t value) throw()
	{
		buf[0] = (value) & 0xFF;
		buf[1] = (value >> 8) & 0xFF;
		buf[2] = (value >> 16) & 0xFF;
		buf[3] = (value >> 24) & 0xFF;
	}

	class file_input
	{
	public:
		typedef char char_type;
		typedef boost::iostreams::source_tag category;
		file_input(std::ifstream& _stream, size_t* _refcnt)
			: stream(_stream), stream_refcnt(*_refcnt)
		{
			stream_refcnt++;
			position = stream.tellg();
			left_unlimited = true;
		}

		file_input(std::ifstream& _stream, uint32_t size, size_t* _refcnt)
			: stream(_stream), stream_refcnt(*_refcnt)
		{
			stream_refcnt++;
			position = stream.tellg();
			left_unlimited = false;
			left = size;
		}

		void close()
		{
		}

		std::streamsize read(char* s, std::streamsize n)
		{
			stream.clear();
			stream.seekg(position, std::ios_base::beg);
			if(stream.fail())
				throw std::runtime_error("Can't seek ZIP file");
			if(!left_unlimited && left == 0)
				return -1;
			if(!left_unlimited && n > left)
				n = left;
			stream.read(s, n);
			std::streamsize r = stream.gcount();
			if(r == 0 && stream.fail())
				throw std::runtime_error("Can't read compressed data from ZIP file");
			if(!stream && r == 0)
				return -1;
			position += r;
			left -= r;
			return r;
		}

		~file_input()
		{
			if(!--stream_refcnt) {
				delete &stream;
				delete &stream_refcnt;
			}
		}

		file_input(const file_input& f)
			: stream(f.stream), stream_refcnt(f.stream_refcnt)
		{
			stream_refcnt++;
			position = f.position;
			left_unlimited = f.left_unlimited;
			left = f.left;
		}
	protected:
		std::ifstream& stream;
		size_t& stream_refcnt;
		std::streamoff position;
		bool left_unlimited;
		uint32_t left;
	private:
		file_input& operator=(const file_input& f);
	};

	class vector_output
	{
	public:
		typedef char char_type;
		typedef boost::iostreams::sink_tag category;
		vector_output(std::vector<char>& _stream)
			: stream(_stream)
		{
		}

		void close()
		{
		}

		std::streamsize write(const char* s, std::streamsize n)
		{
			size_t oldsize = stream.size();
			stream.resize(oldsize + n);
			memcpy(&stream[oldsize], s, n);
			return n;
		}
	protected:
		std::vector<char>& stream;
	};

	class size_and_crc_filter_impl
	{
	public:
		typedef char char_type;

		size_and_crc_filter_impl()
		{
			dsize = 0;
			crc = ::crc32(0, NULL, 0);
		}

		void close()
		{
		}

		bool filter(const char*& src_begin, const char* src_end, char*& dest_begin, char* dest_end,
			bool flush)
		{
			ptrdiff_t amount = src_end - src_begin;
			if(flush && amount == 0)
				return false;
			if(amount > dest_end - dest_begin)
				amount = dest_end - dest_begin;
			dsize += amount;
			crc = ::crc32(crc, reinterpret_cast<const unsigned char*>(src_begin), amount);
			memcpy(dest_begin, src_begin, amount);
			src_begin += amount;
			dest_begin += amount;
			return true;
		}

		uint32_t size()
		{
			return dsize;
		}

		uint32_t crc32()
		{
			return crc;
		}
	private:
		uint32_t dsize;
		uint32_t crc;
	};

	class size_and_crc_filter : public boost::iostreams::symmetric_filter<size_and_crc_filter_impl,
		std::allocator<char>>
	{
		typedef symmetric_filter<size_and_crc_filter_impl, std::allocator<char>> base_type;
	public:
		typedef typename base_type::char_type char_type;
		typedef typename base_type::category category;
		size_and_crc_filter(int bsize)
			: base_type(bsize)
		{
		}

		uint32_t size()
		{
			return filter().size();
		}

		uint32_t crc32()
		{
			return filter().crc32();
		}
	};

	struct zipfile_member_info
	{
		bool central_directory_special;	//Central directory, not real member.
		uint16_t version_needed;
		uint16_t flags;
		uint16_t compression;
		uint16_t mtime_time;
		uint16_t mtime_day;
		uint32_t crc;
		uint32_t compressed_size;
		uint32_t uncompressed_size;
		std::string filename;
		uint32_t header_offset;
		uint32_t data_offset;
		uint32_t next_offset;
	};

	//Parse member starting from current offset.
	zipfile_member_info parse_member(std::ifstream& file)
	{
		zipfile_member_info info;
		info.central_directory_special = false;
		info.header_offset = file.tellg();
		//The file header is 30 bytes (this could also hit central header, but that's even larger).
		unsigned char buffer[30];
		if(!(file.read(reinterpret_cast<char*>(buffer), 30)))
			throw std::runtime_error("Can't read file header from ZIP file");
		uint32_t magic = read32(buffer);
		if(magic == 0x02014b50) {
			info.central_directory_special = true;
			return info;
		}
		if(magic != 0x04034b50)
			throw std::runtime_error("ZIP archive corrupt: Expected file or central directory magic");
		info.version_needed = read16(buffer + 4);
		info.flags = read16(buffer + 6);
		info.compression = read16(buffer + 8);
		info.mtime_time = read16(buffer + 10);
		info.mtime_day = read16(buffer + 12);
		info.crc = read32(buffer + 14);
		info.compressed_size = read32(buffer + 18);
		info.uncompressed_size = read32(buffer + 22);
		uint16_t filename_len = read16(buffer + 26);
		uint16_t extra_len = read16(buffer + 28);
		if(!filename_len)
			throw std::runtime_error("Unsupported ZIP feature: Empty filename not allowed");
		if(info.version_needed > 20) {
			throw std::runtime_error("Unsupported ZIP feature: Only ZIP versions up to 2.0 supported");
		}
		if(info.flags & 0x2001)
			throw std::runtime_error("Unsupported ZIP feature: Encryption is not supported");
		if(info.flags & 0x8)
			throw std::runtime_error("Unsupported ZIP feature: Indeterminate length not supported");
		if(info.flags & 0x20)
			throw std::runtime_error("Unsupported ZIP feature: Binary patching is not supported");
		if(info.compression != 0 && info.compression != 8)
			throw std::runtime_error("Unsupported ZIP feature: Unsupported compression method");
		if(info.compression == 0 && info.compressed_size != info.uncompressed_size)
			throw std::runtime_error("ZIP archive corrupt: csize ≠ usize for stored member");
		std::vector<unsigned char> filename_storage;
		filename_storage.resize(filename_len);
		if(!(file.read(reinterpret_cast<char*>(&filename_storage[0]), filename_len)))
			throw std::runtime_error("Can't read file name from zip file");
		info.filename = std::string(reinterpret_cast<char*>(&filename_storage[0]), filename_len);
		info.data_offset = info.header_offset + 30 + filename_len + extra_len;
		info.next_offset = info.data_offset + info.compressed_size;
		return info;
	}
}

bool zip_reader::has_member(const std::string& name) throw()
{
	return (offsets.count(name) > 0);
}

std::string zip_reader::find_first() throw(std::bad_alloc)
{
	if(offsets.empty())
		return "";
	else
		return offsets.begin()->first;
}

std::string zip_reader::find_next(const std::string& name) throw(std::bad_alloc)
{
	auto i = offsets.upper_bound(name);
	if(i == offsets.end())
		return "";
	else
		return i->first;
}

std::istream& zip_reader::operator[](const std::string& name) throw(std::bad_alloc, std::runtime_error)
{
	if(!offsets.count(name))
		throw std::runtime_error("No such file '" + name + "' in zip archive");
	zipstream->clear();
	zipstream->seekg(offsets[name], std::ios::beg);
	zipfile_member_info info = parse_member(*zipstream);
	zipstream->clear();
	zipstream->seekg(info.data_offset, std::ios::beg);
	if(info.compression == 0) {
		return *new boost::iostreams::stream<file_input>(*zipstream, info.uncompressed_size, refcnt);
	} else if(info.compression == 8) {
		boost::iostreams::filtering_istream* s = new boost::iostreams::filtering_istream();
		boost::iostreams::zlib_params params;
		params.noheader = true;
		s->push(boost::iostreams::zlib_decompressor(params));
		s->push(file_input(*zipstream, info.compressed_size, refcnt));
		return *s;
	} else
		throw std::runtime_error("Unsupported ZIP feature: Unsupported compression method");
}

zip_reader::iterator zip_reader::begin() throw(std::bad_alloc)
{
	return iterator(offsets.begin());
}

zip_reader::iterator zip_reader::end() throw(std::bad_alloc)
{
	return iterator(offsets.end());
}

zip_reader::riterator zip_reader::rbegin() throw(std::bad_alloc)
{
	return riterator(offsets.rbegin());
}

zip_reader::riterator zip_reader::rend() throw(std::bad_alloc)
{
	return riterator(offsets.rend());
}

zip_reader::~zip_reader() throw()
{
	if(!--*refcnt) {
		delete zipstream;
		delete refcnt;
	}
}

zip_reader::zip_reader(const std::string& zipfile) throw(std::bad_alloc, std::runtime_error)
{
	zipfile_member_info info;
	info.next_offset = 0;
	zipstream = new std::ifstream;
	zipstream->open(zipfile.c_str(), std::ios::binary);
	refcnt = new size_t;
	*refcnt = 1;
	if(!*zipstream)
		throw std::runtime_error("Can't open zipfile '" + zipfile + "' for reading");
	do {
		zipstream->clear();
		zipstream->seekg(info.next_offset);
		if(zipstream->fail())
			throw std::runtime_error("Can't seek ZIP file");
		info = parse_member(*zipstream);
		if(info.central_directory_special)
			break;
		offsets[info.filename] = info.header_offset;
	} while(1);
}

zip_writer::zip_writer(const std::string& zipfile, unsigned _compression) throw(std::bad_alloc, std::runtime_error)
{
	compression = _compression;
	zipfile_path = zipfile;
	temp_path = zipfile + ".tmp";
	zipstream.open(temp_path.c_str(), std::ios::binary);
	if(!zipstream)
		throw std::runtime_error("Can't open zipfile '" + temp_path + "' for writing");
	committed = false;
}

zip_writer::~zip_writer() throw()
{
	if(!committed)
		remove(temp_path.c_str());
}

void zip_writer::commit() throw(std::bad_alloc, std::logic_error, std::runtime_error)
{
	if(committed)
		throw std::logic_error("Can't commit twice");
	if(open_file != "")
		throw std::logic_error("Can't commit with file open");
	std::vector<unsigned char> directory_entry;
	uint32_t cdirsize = 0;
	uint32_t cdiroff = zipstream.tellp();
	if(cdiroff == (uint32_t)-1)
		throw std::runtime_error("Can't read current ZIP stream position");
	for(auto i = files.begin(); i != files.end(); ++i) {
		cdirsize += (46 + i->first.length());
		directory_entry.resize(46 + i->first.length());
		write32(&directory_entry[0], 0x02014b50);
		write16(&directory_entry[4], 3);
		write16(&directory_entry[6], 20);
		write16(&directory_entry[8], 8);
		write16(&directory_entry[10], compression ? 8 : 0);
		write16(&directory_entry[12], 0);
		write16(&directory_entry[14], 10273);
		write32(&directory_entry[16], i->second.crc);
		write32(&directory_entry[20], i->second.compressed_size);
		write32(&directory_entry[24], i->second.uncompressed_size);
		write16(&directory_entry[28], i->first.length());
		write16(&directory_entry[30], 0);
		write16(&directory_entry[32], 0);
		write16(&directory_entry[34], 0);
		write16(&directory_entry[36], 0);
		write32(&directory_entry[38], 0);
		write32(&directory_entry[42], i->second.offset);
		memcpy(&directory_entry[46], i->first.c_str(), i->first.length());
		zipstream.write(reinterpret_cast<char*>(&directory_entry[0]), directory_entry.size());
		if(!zipstream)
			throw std::runtime_error("Failed to write central directory entry to output file");
	}
	directory_entry.resize(22);
	write32(&directory_entry[0], 0x06054b50);
	write16(&directory_entry[4], 0);
	write16(&directory_entry[6], 0);
	write16(&directory_entry[8], files.size());
	write16(&directory_entry[10], files.size());
	write32(&directory_entry[12], cdirsize);
	write32(&directory_entry[16], cdiroff);
	write16(&directory_entry[20], 0);
	zipstream.write(reinterpret_cast<char*>(&directory_entry[0]), directory_entry.size());
	if(!zipstream)
		throw std::runtime_error("Failed to write central directory end marker to output file");
	zipstream.close();
	std::string backup = zipfile_path + ".backup";
#if defined(_WIN32) || defined(_WIN64) || defined(TEST_WIN32_CODE)
	//Grumble, Windows seemingly can't do this atomically.
	remove(backup.c_str());
#endif
	rename(zipfile_path.c_str(), backup.c_str());
	if(rename(temp_path.c_str(), zipfile_path.c_str()) < 0)
		throw std::runtime_error("Can't rename '" + temp_path + "' -> '" + zipfile_path + "'");
	committed = true;
}

std::ostream& zip_writer::create_file(const std::string& name) throw(std::bad_alloc, std::logic_error,
	std::runtime_error)
{
	if(open_file != "")
		throw std::logic_error("Can't open file with file open");
	if(name == "")
		throw std::runtime_error("Bad member name");
	current_compressed_file.resize(0);
	s = new boost::iostreams::filtering_ostream();
	s->push(size_and_crc_filter(4096));
	if(compression) {
		boost::iostreams::zlib_params params;
		params.noheader = true;
		s->push(boost::iostreams::zlib_compressor(params));
	}
	s->push(vector_output(current_compressed_file));
	open_file = name;
	return *s;
}

void zip_writer::close_file() throw(std::bad_alloc, std::logic_error, std::runtime_error)
{
	if(open_file == "")
		throw std::logic_error("Can't close file with no file open");
	uint32_t ucs, cs, crc32;
	boost::iostreams::close(*s);
	size_and_crc_filter& f = *s->component<size_and_crc_filter>(0);
	cs = current_compressed_file.size();
	ucs = f.size();
	crc32 = f.crc32();
	delete s;

	base_offset = zipstream.tellp();
	if(base_offset == (uint32_t)-1)
		throw std::runtime_error("Can't read current ZIP stream position");
	unsigned char header[30];
	memset(header, 0, 30);
	write32(header, 0x04034b50);
	header[4] = 20;
	header[6] = 0;
	header[8] = compression ? 8 : 0;
	header[12] = 33;
	header[13] = 40;
	write32(header + 14, crc32);
	write32(header + 18, cs);
	write32(header + 22, ucs);
	write16(header + 26, open_file.length());
	zipstream.write(reinterpret_cast<char*>(header), 30);
	zipstream.write(open_file.c_str(), open_file.length());
	zipstream.write(&current_compressed_file[0], current_compressed_file.size());
	if(!zipstream)
		throw std::runtime_error("Can't write member to ZIP file");
	current_compressed_file.resize(0);
	zip_file_info info;
	info.crc = crc32;
	info.uncompressed_size = ucs;
	info.compressed_size = cs;
	info.offset = base_offset;
	files[open_file] = info;
	open_file = "";
}

namespace
{
#if defined(_WIN32) || defined(_WIN64) || defined(TEST_WIN32_CODE)
	const char* path_splitters = "\\/";
	bool drives_allowed = true;
#else
	//Assume Unix(-like) system.
	const char* path_splitters = "/";
	bool drives_allowed = false;
#endif

	const char* str_index(const char* str, int ch)
	{
		while(*str)
			if(*str == ch)
				return str;
		return NULL;
	}

	bool ispathsep(char ch)
	{
		return (str_index(path_splitters, static_cast<int>(static_cast<unsigned char>(ch))) != NULL);
	}

	bool isroot(const std::string& path)
	{
		if(path.length() == 1 && ispathsep(path[0]))
			return true;
		if(!drives_allowed)
			//NO more cases for this.
			return false;
		if(path.length() == 3 && path[0] >= 'A' && path[0] <= 'Z' && path[1] == ':' && ispathsep(path[2]))
			return true;
		//UNC.
		if(path.length() <= 3 || !ispathsep(path[0]) || !ispathsep(path[1]) ||
			!ispathsep(path[path.length() - 1]))
			return false;
		return (path.find_first_of(path_splitters, 2) == path.length() - 1);
	}

	std::string walk(const std::string& path, const std::string& component)
	{
		if(component == "" || component == ".")
			//Current directory.
			return path;
		else if(component == "..") {
			//Parent directory.
			if(path == "" || isroot(path))
				throw std::runtime_error("Can't rise to containing directory");
			std::string _path = path;
			size_t split = _path.find_last_of(path_splitters);
			if(split < _path.length())
				return _path.substr(0, split);
			else
				return "";
		} else if(path == "" || ispathsep(path[path.length() - 1]))
			return path + component;
		else
			return path + "/" + component;
	}

	std::string combine_path(const std::string& _name, const std::string& _referencing_path)
	{
		std::string name = _name;
		std::string referencing_path = _referencing_path;
		size_t x = referencing_path.find_last_of(path_splitters);
		if(x < referencing_path.length())
			referencing_path = referencing_path.substr(0, x);
		else
			return name;
		//Check if name is absolute.
		if(ispathsep(name[0]))
			return name;
		if(drives_allowed && name.length() >= 3 && name[0] >= 'A' && name[0] <= 'Z' && name[1] == ':' &&
			ispathsep(name[2]))
			return name;
		//It is not absolute.
		std::string path = referencing_path;
		size_t pindex = 0;
		while(true) {
			size_t split = name.find_first_of(path_splitters, pindex);
			std::string c;
			if(split < name.length())
				c = name.substr(pindex, split - pindex);
			else
				c = name.substr(pindex);
			path = walk(path, c);
			if(split < name.length())
				pindex = split + 1;
			else
				break;
		}
		//If path becomes empty, assume it means current directory.
		if(path == "")
			path = ".";
		return path;
	}
}

std::string resolve_file_relative(const std::string& name, const std::string& referencing_path) throw(std::bad_alloc,
	std::runtime_error)
{
	return combine_path(name, referencing_path);
}

std::istream& open_file_relative(const std::string& name, const std::string& referencing_path) throw(std::bad_alloc,
	std::runtime_error)
{
	std::string path_to_open = combine_path(name, referencing_path);
	std::string final_path = path_to_open;
	//Try to open this from the main OS filesystem.
	std::ifstream* i = new std::ifstream(path_to_open.c_str(), std::ios::binary);
	if(i->is_open()) {
		return *i;
	}
	delete i;
	//Didn't succeed. Try to open as ZIP archive.
	std::string membername;
	while(true) {
		size_t split = path_to_open.find_last_of("/");
		if(split >= path_to_open.length())
			throw std::runtime_error("Can't open '" + final_path + "'");
		//Move a component to member name.
		if(membername != "")
			membername = path_to_open.substr(split + 1) + "/" + membername;
		else
			membername = path_to_open.substr(split + 1);
		path_to_open = path_to_open.substr(0, split);
		try {
			zip_reader r(path_to_open);
			return r[membername];
		} catch(std::bad_alloc& e) {
			throw;
		} catch(std::runtime_error& e) {
		}
	}
}

std::vector<char> read_file_relative(const std::string& name, const std::string& referencing_path) throw(std::bad_alloc,
	std::runtime_error)
{
	std::vector<char> out;
	std::istream& s = open_file_relative(name, referencing_path);
	boost::iostreams::back_insert_device<std::vector<char>> rd(out);
	boost::iostreams::copy(s, rd);
	delete &s;
	return out;
}
