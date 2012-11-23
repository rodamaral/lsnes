#ifndef _library__ogg__hpp__included__
#define _library__ogg__hpp__included__

#include <cstdint>
#include <stdexcept>
#include <vector>
#include <map>

/**
 * A page in Ogg bitstream.
 */
class ogg_page
{
public:
/**
 * Create a new blank page.
 */
	ogg_page() throw();
/**
 * Create a page, reading a buffer.
 *
 * Parameter buffer: The buffer to read.
 * Parameter advance: The number of bytes in packet is stored here.
 * Throws std::runtime_error: Bad packet.
 */
	ogg_page(const char* buffer, size_t& advance) throw(std::runtime_error);
/**
 * Scan a buffer for pages.
 *
 * Parameter buffer: The buffer to scan.
 * Parameter bufferlen: The length of buffer. Should be at least 65307, unless there's not that much available.
 * Parameter eof: If set, assume physical stream ends after this buffer.
 * Parameter advance: Amount to advance the pointer is stored here.
 * Returns: True if packet was found, false if not.
 */
	static bool scan(const char* buffer, size_t bufferlen, bool eof, size_t& advance) throw();
/**
 * Get the continue flag of packet.
 */
	bool get_continue() const throw() { return flag_continue; }
/**
 * Set the continue flag of packet.
 */
	void set_continue(bool c) throw() { flag_continue = c; }
/**
 * Get the BOS flag of packet.
 */
	bool get_bos() const throw() { return flag_bos; }
/**
 * Set the BOS flag of packet.
 */
	void set_bos(bool b) throw() { flag_bos = b; }
/**
 * Get the EOS flag of packet.
 */
	bool get_eos() const throw() { return flag_eos; }
/**
 * Set the EOS flag of packet.
 */
	void set_eos(bool e) throw() { flag_eos = e; }
/**
 * Get the granulepos of packet.
 */
	uint64_t get_granulepos() const throw() { return granulepos; }
/**
 * Set the granulepos of packet.
 */
	void set_granulepos(uint64_t g) throw() { granulepos = g; }
/**
 * Get stream identifier.
 */
	uint32_t get_stream() const throw() { return stream; }
/**
 * Set stream identifier.
 */
	void set_stream(uint32_t s) throw() { stream = s; }
/**
 * Get stream identifier.
 */
	uint32_t get_sequence() const throw() { return sequence; }
/**
 * Set stream identifier.
 */
	void set_sequence(uint32_t s) throw() { sequence = s; }
/**
 * Get number of packets.
 */
	uint8_t get_packet_count() const throw() { return packet_count; }
/**
 * Get the packet.
 */
	std::pair<const uint8_t*, size_t> get_packet(size_t packetno) const throw()
	{
		if(packetno >= packet_count)
			return std::make_pair(reinterpret_cast<const uint8_t*>(NULL), 0);
		else
			return std::make_pair(data + packets[packetno], packets[packetno + 1] - packets[packetno]);
	}
/**
 * Get the last packet incomplete flag.
 */
	bool get_last_packet_incomplete() const throw() { return last_incomplete; }
/**
 * Append a complete packet to page.
 *
 * Parameter d: The data to append.
 * Parameter dlen: The length of data to append.
 * Returns: True on success, false on failure.
 */
	bool append_packet(const uint8_t* d, size_t dlen) throw();
/**
 * Append a possibly incomplete packet to page.
 *
 * Parameter d: The data to append. Adjusted.
 * Parameter dlen: The length of data to append. Adjusted
 * Returns: True if write was complete, false if incomplete.
 */
	bool append_packet_incomplete(const uint8_t*& d, size_t& dlen) throw();
/**
 * Get number of octets it takes to serialize this.
 */
	size_t serialize_size() const throw() { return 27 + segment_count + data_count; }
/**
 * Serialize this packet.
 *
 * Parameter buffer: Buffer to serialize to (use serialize_size() to find the size).
 */
	void serialize(char* buffer) const throw();
/**
 * The special granule pos for nothing.
 */
	const static uint64_t granulepos_none;
private:
	uint8_t version;
	bool flag_continue;
	bool flag_bos;
	bool flag_eos;
	bool last_incomplete;
	uint64_t granulepos;
	uint32_t stream;
	uint32_t sequence;
	uint8_t segment_count;
	uint8_t packet_count;
	uint16_t data_count;
	uint8_t data[65025];
	uint8_t segments[255];
	uint16_t packets[256];
};

/**
 * Ogg stream reader.
 */
class ogg_stream_reader
{
public:
/**
 * Constructor.
 */
	ogg_stream_reader() throw();
/**
 * Destructor.
 */
	virtual ~ogg_stream_reader() throw();
/**
 * Read some data.
 *
 * Parameter buffer: The buffer to store the data to.
 * Parameter size: The maximum size to read.
 * Returns: The number of bytes actually read.
 */
	virtual size_t read(char* buffer, size_t size) throw(std::exception) = 0;
/**
 * Read a page from stream.
 *
 * Parameter page: The page is assigned here if successful.
 * Returns: True if page was obtained, false if not.
 */
	bool get_page(ogg_page& page) throw(std::exception);
/**
 * Set stream to report errors to.
 *
 * Parameter strm: The stream.
 */
	void set_errors_to(std::ostream& os);
private:
	ogg_stream_reader(const ogg_stream_reader&);
	ogg_stream_reader& operator=(const ogg_stream_reader&);
	void fill_buffer();
	void discard_buffer(size_t amount);
	bool eof;
	char buffer[65536];
	size_t left;
	std::ostream* errors_to;
};

/**
 * Ogg stream reader based on std::istream.
 */
class ogg_stream_reader_iostreams : public ogg_stream_reader
{
public:
/**
 * Constructor.
 *
 * Parameter stream: The stream to read the data from.
 */
	ogg_stream_reader_iostreams(std::istream& stream);
/**
 * Destructor.
 */
	~ogg_stream_reader_iostreams() throw();

	size_t read(char* buffer, size_t size) throw(std::exception);
private:
	std::istream& is;
};

/**
 * Ogg stream writer.
 */
class ogg_stream_writer
{
public:
/**
 * Constructor.
 */
	ogg_stream_writer() throw();
/**
 * Destructor.
 */
	virtual ~ogg_stream_writer() throw();
/**
 * Write data.
 *
 * Parameter data: The data to write.
 * Parameter size: The size to write.
 */
	virtual void write(const char* buffer, size_t size) throw(std::exception) = 0;
/**
 * Write a page to stream.
 *
 * Parameter page: The page to write.
 */
	void put_page(const ogg_page& page) throw(std::exception);
private:
	ogg_stream_writer(const ogg_stream_writer&);
	ogg_stream_writer& operator=(const ogg_stream_writer&);
};

/**
 * Ogg stream writer based on std::istream.
 */
class ogg_stream_writer_iostreams : public ogg_stream_writer
{
public:
/**
 * Constructor.
 *
 * Parameter stream: The stream to read the data from.
 */
	ogg_stream_writer_iostreams(std::ostream& stream);
/**
 * Destructor.
 */
	~ogg_stream_writer_iostreams() throw();

	void write(const char* buffer, size_t size) throw(std::exception);
private:
	std::ostream& os;
};

/**
 * OggOpus header structure.
 */
struct oggopus_header
{
	uint8_t version;
	uint8_t channels;
	uint16_t preskip;
	uint32_t rate;
	int16_t gain;
	uint8_t map_family;
	uint8_t streams;
	uint8_t coupled;
	uint8_t chanmap[255];
};

/**
 * OggOpus tags structure
 */
struct oggopus_tags
{
	std::string vendor;
	std::vector<std::string> comments;
};

/**
 * Parse Ogg page as OggOpus header.
 *
 * Parameter page: The page to parse.
 * Returns: Parsed data.
 * Throws std::runtime_error: Not valid OggOpus header page.
 */
struct oggopus_header parse_oggopus_header(struct ogg_page& page) throw(std::runtime_error);

/**
 * Serialize OggOpus header as an Ogg page.
 *
 * Parameter header: The header.
 * Returns: The serialized page.
 * Throws std::runtime_error: Not valid OggOpus header.
 */
struct ogg_page serialize_oggopus_header(struct oggopus_header& header) throw(std::runtime_error);

/**
 * Parse Ogg page as OggOpus comment.
 *
 * Parameter page: The page to parse.
 * Returns: Parsed data.
 * Throws std::runtime_error: Not valid OggOpus comment page.
 */
struct oggopus_tags parse_oggopus_tags(struct ogg_page& page) throw(std::bad_alloc, std::runtime_error);

/**
 * Serialize OggOpus comments as an Ogg page.
 *
 * Parameter tags: The comments.
 * Returns: The serialized page.
 * Throws std::runtime_error: Not valid OggOpus comments.
 */
struct ogg_page serialize_oggopus_tags(struct oggopus_tags& tags) throw(std::runtime_error);

#endif
