#ifndef _library__oggopus__hpp__included__
#define _library__oggopus__hpp__included__

#include "ogg.hpp"
#include <functional>

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
 * Parse Ogg packet as OggOpus header.
 *
 * Parameter pacekt: The packet to parse.
 * Returns: Parsed data.
 * Throws std::runtime_error: Not valid OggOpus header page.
 */
struct oggopus_header parse_oggopus_header(struct ogg::packet& packet) throw(std::runtime_error);

/**
 * Serialize OggOpus header as an Ogg page.
 *
 * Parameter header: The header.
 * Returns: The serialized page.
 * Throws std::runtime_error: Not valid OggOpus header packet.
 */
struct ogg::page serialize_oggopus_header(struct oggopus_header& header) throw(std::runtime_error);

/**
 * Parse Ogg packet as OggOpus comment.
 *
 * Parameter packet: The packet to parse.
 * Returns: Parsed data.
 * Throws std::runtime_error: Not valid OggOpus comment packet.
 */
struct oggopus_tags parse_oggopus_tags(struct ogg::packet& packet) throw(std::bad_alloc, std::runtime_error);

/**
 * Serialize OggOpus comments as Ogg pages.
 *
 * Parameter tags: The comments.
 * Parameter output: Callback to call on each serialized page on turn.
 * Parameter strmid: The stream id to use.
 * Returns: Next sequence number to use.
 * Throws std::runtime_error: Not valid OggOpus comments.
 */
uint32_t serialize_oggopus_tags(struct oggopus_tags& tags, std::function<void(const ogg::page& p)> output,
	uint32_t strmid) throw(std::bad_alloc, std::runtime_error);

#endif
