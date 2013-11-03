#ifndef _library__zip__hpp__included__
#define _library__zip__hpp__included__

#include <boost/iostreams/filtering_stream.hpp>
#include <iostream>
#include <iterator>
#include <string>
#include <map>
#include <fstream>
#include <sstream>
#include <zlib.h>

/**
 * This class opens ZIP archive and offers methods to read members off it.
 */
class zip_reader
{
public:
/**
 * This iterator iterates members of ZIP archive.
 */
	template<typename T, typename V>
	class iterator_class
	{
	public:
/**
 * C++ iterators stuff.
 */
		typedef std::bidirectional_iterator_tag iterator_category;
		typedef V value_type;
		typedef int difference_type;
		typedef const V& reference;
		typedef const V* pointer;

/**
 * This constructs new iteration sequence. Only the first component (keys) are taken into
 * account, the second component (values) are ignored.
 *
 * parameter _itr: The underlying map iterator.
 * throws std::bad_alloc: Not enough memory.
 */
		iterator_class(T _itr) throw(std::bad_alloc)
			: itr(_itr)
		{
		}

/**
 * Get name of current member.
 *
 * returns: Name of member.
 * throws std::bad_alloc: Not enough memory.
 */
		reference operator*() throw(std::bad_alloc)
		{
			return itr->first;
		}

/**
 * Get name of current member.
 *
 * returns: Name of member.
 * throws std::bad_alloc: Not enough memory.
 */
		pointer operator->() throw(std::bad_alloc)
		{
			return &(itr->first);
		}

/**
 * Are these two iterators the same?
 *
 * parameter i: The another iterator
 * returns: True if iterators are the same, false otherwise.
 */
		bool operator==(const iterator_class<T, V>& i) const throw()
		{
			return itr == i.itr;
		}

/**
 * Are these two iterators diffrent?
 *
 * paramer i: The another iterator
 * returns: True if iterators are diffrent, false otherwise.
 */
		bool operator!=(const iterator_class<T, V>& i) const throw()
		{
			return itr != i.itr;
		}

/**
 * Advance iterator one step.
 *
 * returns: The old value of iterator.
 * throws std::bad_alloc: Not enough memory.
 */
		const iterator_class<T, V> operator++(int) throw(std::bad_alloc)
		{
			iterator_class<T, V> c(*this);
			++itr;
			return c;
		}

/**
 * Regress iterator one step.
 *
 * returns: The old value of iterator.
 * throws std::bad_alloc: Not enough memory.
 */
		const iterator_class<T, V> operator--(int) throw(std::bad_alloc)
		{
			iterator_class<T, V> c(*this);
			--itr;
			return c;
		}

/**
 * Advance iterator one step.
 *
 * returns: Reference to this iterator.
 */
		iterator_class<T, V>& operator++() throw()
		{
			++itr;
			return *this;
		}

/**
 * Regress iterator one step.
 *
 * returns: Reference to this iterator.
 */
		iterator_class<T, V>& operator--() throw()
		{
			--itr;
			return *this;
		}
	private:
		T itr;
	};

/**
 * This iterator iterates members of ZIP archive in forward order.
 */
	typedef iterator_class<std::map<std::string, unsigned long long>::iterator, std::string> iterator;

/**
 * This iterator iterates members of ZIP archive in reverse order
 */
	typedef iterator_class<std::map<std::string, unsigned long long>::reverse_iterator, std::string>
		riterator;

/**
 * Opens specified ZIP archive for reading.
 *
 * parameter zipfile: The name of ZIP file to open.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Can't open the ZIP file.
 */
	zip_reader(const std::string& zipfile) throw(std::bad_alloc, std::runtime_error);

/**
 * Destroy the ZIP reader. Opened input streams continue to be valid.
 */
	~zip_reader() throw();

/**
 * Gives the name of the first member, or "" if empty archive.
 *
 * returns: The member name
 * throws std::bad_alloc: Not enough memory.
 */
	std::string find_first() throw(std::bad_alloc);

/**
 * Gives the name of the next member after specified, or "" if that member is the last.
 *
 * parameter name: The name to start the search from.
 * returns: The member name
 * throws std::bad_alloc: Not enough memory.
 */
	std::string find_next(const std::string& name) throw(std::bad_alloc);

/**
 * Starting iterator
 *
 * returns: The iterator pointing to first name.
 * throws std::bad_alloc: Not enough memory.
 */
	iterator begin() throw(std::bad_alloc);

/**
 * Ending iterator (one past the end).
 *
 * returns: The iterator pointing to one past the last name.
 * throws std::bad_alloc: Not enough memory.
 */
	iterator end() throw(std::bad_alloc);

/**
 * Starting reverse iterator
 *
 * returns: The iterator pointing to last name and acting in reverse.
 * throws std::bad_alloc: Not enough memory.
 */
	riterator rbegin() throw(std::bad_alloc);

/**
 * Ending reverse iterator (one past the start).
 * returrns: The iterator pointing to one before the first name and acting in reverse.
 * throws std::bad_alloc: Not enough memory.
 */
	riterator rend() throw(std::bad_alloc);

/**
 * Check if member with specified name exists.
 *
 * parameter name: The name of the member to check
 * returns: True if specified member exists, false otherwise.
 */
	bool has_member(const std::string& name) throw();

/**
 * Opens specified member. The resulting stream is not seekable, allocated using new and continues to be valid
 * after ZIP reader has been destroyed.
 *
 * parameter name: The name of member to open.
 * returns: The stream corresponding to member.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: The specified member does not exist
 */
	std::istream& operator[](const std::string& name) throw(std::bad_alloc, std::runtime_error);
private:
	zip_reader(zip_reader&);
	zip_reader& operator=(zip_reader&);
	std::map<std::string, unsigned long long> offsets;
	std::ifstream* zipstream;
	size_t* refcnt;
};

/**
 * Opens the file named by name parameter, which is interpretted relative to file designated by referencing_path.
 * The file can be inside ZIP archive. The resulting stream may or may not be seekable.
 *
 * If referencing_path is "", then name is traditional relative/absolute path. Otherwise if name is relative,
 * it is relative to directory containing referencing_path, not current directory.
 *
 * parameter name: The name of file to open.
 * parameter referencing_path: The path to file name is interpretted against.
 * returns: The new stream, allocated by new.
 * throw std::bad_alloc: Not enough memory.
 * throw std::runtime_error: The file does not exist or can't be opened.
 */
std::istream& open_file_relative(const std::string& name, const std::string& referencing_path) throw(std::bad_alloc,
	std::runtime_error);

/**
 * As open_file_relative, but instead of returning handle to file, reads the entiere contents of the file and returns
 * that.
 *
 * parameter name: As in open_file_relative
 * parameter referencing_path: As in open_file_relative.
 * returns: The file contents.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: The file does not exist or can't be opened.
 */
std::vector<char> read_file_relative(const std::string& name, const std::string& referencing_path)
	throw(std::bad_alloc, std::runtime_error);

/**
 * Resolves the final file path that open_file_relative/read_file_relative would open.
 *
 * parameter name: As in open_file_relative
 * parameter referencing_path: As in open_file_relative
 * returns: The file absolute path.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Bad path.
 */
std::string resolve_file_relative(const std::string& name, const std::string& referencing_path) throw(std::bad_alloc,
	std::runtime_error);

/**
 * Does the specified file (maybe inside .zip) exist?
 *
 * parameter name: The name of file.
 * returns: True if file exists, false if not.
 * throws std::bad_alloc: Not enough memory.
 */
bool file_exists_zip(const std::string& name) throw(std::bad_alloc);

/**
 * This class handles writing a ZIP archives.
 */
class zip_writer
{
public:
/**
 * Creates new empty ZIP archive. The members will be compressed according to specified compression.
 *
 * parameter zipfile: The zipfile to create.
 * parameter stream: The stream to write the ZIP to.
 * parameter _compression: Compression. 0 is uncompressed, 1-9 are deflate compression levels.
 * throws std::bad_alloc: Not enough memory.
 * throws std::runtime_error: Can't open archive or invalid argument.
 */
	zip_writer(const std::string& zipfile, unsigned _compression) throw(std::bad_alloc, std::runtime_error);
	zip_writer(std::ostream& stream, unsigned _compression) throw(std::bad_alloc, std::runtime_error);
/**
 * Destroys ZIP writer, aborting the transaction (unless commit() has been called).
 */
	~zip_writer() throw();

/**
 * Commits the ZIP file. Does atomic replace of existing file if possible.
 *
 * throws std::bad_alloc: Not enough memory.
 * throws std::logic_error: Existing file open.
 * throws std::runtime_error: Can't commit archive (OS error or member open).
 */
	void commit() throw(std::bad_alloc, std::logic_error, std::runtime_error);

/**
 * Create a new member inside ZIP file. No existing member may be open.
 *
 * parameter name: The name for new member.
 * returns: Writing stream for the file (don't free).
 * throws std::bad_alloc: Not enough memory.
 * throws std::logic_error: Existing file open.
 * throws std::runtime_error: Illegal name.
 */
	std::ostream& create_file(const std::string& name) throw(std::bad_alloc, std::logic_error, std::runtime_error);

/**
 * Closes open member and destroys stream corresponding to it.
 *
 * throws std::bad_alloc: Not enough memory.
 * throws std::logic_error: No file open.
 * throws std::runtime_error: Error from operating system.
 */
	void close_file() throw(std::bad_alloc, std::logic_error, std::runtime_error);
private:
	struct zip_file_info
	{
		unsigned long crc;
		unsigned long uncompressed_size;
		unsigned long compressed_size;
		unsigned long offset;
	};

	zip_writer(zip_writer&);
	zip_writer& operator=(zip_writer&);
	std::ostream* zipstream;
	bool system_stream;
	std::string temp_path;
	std::string zipfile_path;
	std::string open_file;
	uint32_t base_offset;
	std::vector<char> current_compressed_file;
	std::map<std::string, zip_file_info> files;
	unsigned compression;
	boost::iostreams::filtering_ostream* s;
	uint32_t basepos;
	bool committed;
};

int rename_file_overwrite(const char* oldname, const char* newname);

#endif
