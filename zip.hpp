#ifndef _zip__hpp__included__
#define _zip__hpp__included__

#include <boost/iostreams/filtering_stream.hpp>
#include <iostream>
#include <iterator>
#include <string>
#include <map>
#include <fstream>
#include <zlib.h>

/**
 * \brief Read files from ZIP archive
 *
 * This class opens ZIP archive and offers methods to read members off it.
 */
class zip_reader
{
public:
	/**
	 * \brief ZIP file iterator
	 *
	 * This iterator iterates members of ZIP archive.
	 */
	template<typename T, typename V>
	class iterator_class
	{
	public:
		/**
		 * \brief C++ iterators stuff.
		 */
		typedef std::bidirectional_iterator_tag iterator_category;
		/**
		 * \brief C++ iterators stuff.
		 */
		typedef V value_type;
		/**
		 * \brief C++ iterators stuff.
		 */
		typedef int difference_type;
		/**
		 * \brief C++ iterators stuff.
		 */
		typedef const V& reference;
		/**
		 * \brief C++ iterators stuff.
		 */
		typedef const V* pointer;

		/**
		 * \brief Construct new iterator with specified names
		 *
		 * This constructs new iteration sequence. Only the first component (keys) are taken into
		 * account, the second component (values) are ignored.
		 *
		 * \param _itr The underlying map iterator.
		 * \throws std::bad_alloc Not enough memory.
		 */
		iterator_class(T _itr) throw(std::bad_alloc)
			: itr(_itr)
		{
		}

		/**
		 * \brief Get name of current member.
		 * \return Name of member.
		 * \throws std::bad_alloc Not enough memory.
		 */
		reference operator*() throw(std::bad_alloc)
		{
			return itr->first;
		}

		/**
		 * \brief Get name of current member.
		 * \return Name of member.
		 * \throws std::bad_alloc Not enough memory.
		 */
		pointer operator->() throw(std::bad_alloc)
		{
			return &(itr->first);
		}

		/**
		 * \brief Are these two iterators the same?
		 * \param i The another iterator
		 * \return True if iterators are the same, false otherwise.
		 */
		bool operator==(const iterator_class<T, V>& i) const throw()
		{
			return itr == i.itr;
		}

		/**
		 * \brief Are these two iterators diffrent?
		 * \param i The another iterator
		 * \return True if iterators are diffrent, false otherwise.
		 */
		bool operator!=(const iterator_class<T, V>& i) const throw()
		{
			return itr != i.itr;
		}

		/**
		 * \brief Advance iterator one step.
		 * \return The old value of iterator.
		 * \throws std::bad_alloc Not enough memory.
		 */
		const iterator_class<T, V> operator++(int) throw(std::bad_alloc)
		{
			iterator_class<T, V> c(*this);
			++itr;
			return c;
		}

		/**
		 * \brief Regress iterator one step.
		 * \return The old value of iterator.
		 * \throws std::bad_alloc Not enough memory.
		 */
		const iterator_class<T, V> operator--(int) throw(std::bad_alloc)
		{
			iterator_class<T, V> c(*this);
			--itr;
			return c;
		}

		/**
		 * \brief Advance iterator one step.
		 * \return Reference to this iterator.
		 */
		iterator_class<T, V>& operator++() throw()
		{
			++itr;
			return *this;
		}

		/**
		 * \brief Regress iterator one step.
		 * \return Reference to this iterator.
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
	 * \brief ZIP file forward iterator
	 *
	 * This iterator iterates members of ZIP archive in forward order.
	 */
	typedef iterator_class<std::map<std::string, unsigned long long>::iterator, std::string> iterator;

	/**
	 * \brief ZIP file reverse iterator
	 *
	 * This iterator iterates members of ZIP archive in reverse order
	 */
	typedef iterator_class<std::map<std::string, unsigned long long>::reverse_iterator, std::string>
		riterator;

	/**
	 * \brief Open a ZIP archive.
	 *
	 * Opens specified ZIP archive.
	 * \param zipfile The ZIP file to open.
	 * \throws std::bad_alloc Not enough memory.
	 * \throws std::runtime_error Can't open the ZIP file.
	 */
	zip_reader(const std::string& zipfile) throw(std::bad_alloc, std::runtime_error);

	/**
	 * \brief Destructor
	 *
	 * Destroy the ZIP reader. Opened input streams continue to be valid.
	 */
	~zip_reader() throw();

	/**
	 * \brief Find the name of first member.
	 *
	 * Gives the name of the first member, or "" if empty archive.
	 *
	 * \return The member name
	 * \throws std::bad_alloc Not enough memory.
	 */
	std::string find_first() throw(std::bad_alloc);

	/**
	 * \brief Find the next member.
	 *
	 * Gives the name of the next member after specified, or "" if that member is the last.
	 *
	 * \param name The name to start the search from.
	 * \return The member name
	 * \throws std::bad_alloc Not enough memory.
	 */
	std::string find_next(const std::string& name) throw(std::bad_alloc);

	/**
	 * \brief Starting iterator
	 * \return The iterator pointing to first name.
	 * \throws std::bad_alloc Not enough memory.
	 */
	iterator begin() throw(std::bad_alloc);

	/**
	 * \brief Ending iterator
	 * \return The iterator pointing to one past the last name.
	 * \throws std::bad_alloc Not enough memory.
	 */
	iterator end() throw(std::bad_alloc);

	/**
	 * \brief Starting reverse iterator
	 * \return The iterator pointing to last name and acting in reverse.
	 * \throws std::bad_alloc Not enough memory.
	 */
	riterator rbegin() throw(std::bad_alloc);

	/**
	 * \brief Ending reverse iterator
	 * \return The iterator pointing to one before the first name and acting in reverse.
	 * \throws std::bad_alloc Not enough memory.
	 */
	riterator rend() throw(std::bad_alloc);

	/**
	 * \brief Does the member exist?
	 * \param name The name of the member.
	 * \return True if specified member exists, false otherwise.
	 */
	bool has_member(const std::string& name) throw();

	/**
	 * \brief Open member
	 *
	 * Opens specified member. The stream is not seekable, allocated using new and continues to be valid after
	 * ZIP reader has been destroyed.
	 *
	 * \param name The name of member to open.
	 * \return The stream corresponding to member.
	 * \throws std::bad_alloc Not enough memory.
	 * \throws std::runtime_error The specified member does not exist
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
 * \brief Open file relative to another
 *
 * Opens the file named by name parameter, which is interpretted relative to file designated by referencing_path.
 * The file can be inside ZIP archive. The resulting stream may or may not be seekable.
 *
 * If referencing_path is "", then name is traditional relative/absolute path. Otherwise if name is relative,
 * it is relative to directory containing referencing_path, not current directory.
 *
 * \param name The name of file to open.
 * \param referencing_path The path to file name is interpretted against.
 * \return The new stream, allocated by new.
 * \throw std::bad_alloc Not enough memory.
 * \throw std::runtime_error The file does not exist or can't be opened.
 */
std::istream& open_file_relative(const std::string& name, const std::string& referencing_path) throw(std::bad_alloc,
	std::runtime_error);

/**
 * \brief Read file relative to another.
 *
 * Reads the entiere content of file named by name parameter, which is interpretted relative to file designated by
 * referencing_path. The file can be inside ZIP archive.
 *
 * If referencing_path is "", then name is traditional relative/absolute path. Otherwise if name is relative,
 * it is relative to directory containing referencing_path, not current directory.
 *
 * \param name The name of file to read.
 * \param referencing_path The path to file name is interpretted against.
 * \return The file contents.
 * \throw std::bad_alloc Not enough memory.
 * \throw std::runtime_error The file does not exist or can't be opened.
 */
std::vector<char> read_file_relative(const std::string& name, const std::string& referencing_path)
	throw(std::bad_alloc, std::runtime_error);

/**
 * \brief Resolve full path of file relative to another.
 * 
 * Resolves the final file path that open_file_relative/read_file_relative would open.
 * 
 * \param name The name of file to read.
 * \param referencing_path The path to file name is interpretted against.
 * \return The file absolute path.
 * \throw std::bad_alloc Not enough memory.
 * \throw std::runtime_error Bad path.
 */
std::string resolve_file_relative(const std::string& name, const std::string& referencing_path) throw(std::bad_alloc,
	std::runtime_error);

/**
 * \brief Write a ZIP archive
 *
 * This class handles writing a ZIP archive.
 */
class zip_writer
{
public:
	/**
	 * \brief Create new empty ZIP archive.
	 *
	 * Creates new empty ZIP archive. The members will be compressed according to specified compression.
	 *
	 * \param zipfile The zipfile to create.
	 * \param _compression Compression. 0 is uncompressed, 1-9 are deflate compression levels.
	 * \throws std::bad_alloc Not enough memory.
	 * \throws std::runtime_error Can't open archive or invalid argument.
	 */
	zip_writer(const std::string& zipfile, unsigned _compression) throw(std::bad_alloc, std::runtime_error);

	/**
	 * \brief Destroy ZIP writer.
	 *
	 * Destroys ZIP writer, aborting the transaction (unless commit() has been called).
	 */
	~zip_writer() throw();

	/**
	 * \brief Commit the transaction
	 *
	 * Commits the ZIP file. Does atomic replace of existing file if possible.
	 *
	 * \throws std::bad_alloc Not enough memory.
	 * \throws std::logic_error Existing file open.
	 * \throws std::runtime_error Can't commit archive (OS error or member open).
	 */
	void commit() throw(std::bad_alloc, std::logic_error, std::runtime_error);

	/**
	 * \brief Create a new member
	 *
	 * Create a new member inside ZIP file. No existing member may be open.
	 *
	 * \param name The name for new member.
	 * \return Writing stream for the file (don't free).
	 *
	 * \throws std::bad_alloc Not enough memory.
	 * \throws std::logic_error Existing file open.
	 * \throws std::runtime_error Illegal name.
	 */
	std::ostream& create_file(const std::string& name) throw(std::bad_alloc, std::logic_error, std::runtime_error);

	/**
	 * \brief Close open member
	 *
	 * Closes open member and destroys stream corresponding to it.
	 *
	 * \throws std::bad_alloc Not enough memory.
	 * \throws std::logic_error No file open.
	 * \throws std::runtime_error Error from operating system.
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
	std::ofstream zipstream;
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

#endif
