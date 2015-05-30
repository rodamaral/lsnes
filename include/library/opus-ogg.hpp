#ifndef _library__opus_ogg__hpp__included__
#define _library__opus_ogg__hpp__included__

#include <functional>
#include <cstdint>
#include "ogg.hpp"
#include "text.hpp"

namespace opus
{
/**
 * OggOpus header structure.
 */
struct ogg_header
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
/**
 * Parse Ogg packet as OggOpus header.
 *
 * Parameter pacekt: The packet to parse.
 * Throws std::runtime_error: Not valid OggOpus header page.
 */
	void parse(struct ogg::packet& packet) throw(std::runtime_error);
/**
 * Serialize OggOpus header as an Ogg page.
 *
 * Returns: The serialized page.
 * Throws std::runtime_error: Not valid OggOpus header packet.
 */
	struct ogg::page serialize() throw(std::runtime_error);
};

/**
 * OggOpus tags structure
 */
struct ogg_tags
{
	text vendor;
	std::vector<text> comments;
/**
 * Parse Ogg packet as OggOpus comment.
 *
 * Parameter packet: The packet to parse.
 * Throws std::runtime_error: Not valid OggOpus comment packet.
 */
	void parse(struct ogg::packet& packet) throw(std::bad_alloc, std::runtime_error);
/**
 * Serialize OggOpus comments as Ogg pages.
 *
 * Parameter output: Callback to call on each serialized page on turn.
 * Parameter strmid: The stream id to use.
 * Returns: Next sequence number to use.
 * Throws std::runtime_error: Not valid OggOpus comments.
 */
	uint32_t serialize(std::function<void(const ogg::page& p)> output, uint32_t strmid)
		throw(std::bad_alloc, std::runtime_error);
};
}
#endif
