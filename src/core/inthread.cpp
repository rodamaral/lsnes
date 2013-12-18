#include <lsnes.hpp>
#include <cstdint>
#include "library/filesys.hpp"
#include "library/minmax.hpp"
#include "library/workthread.hpp"
#include "library/serialization.hpp"
#include "library/string.hpp"
#include "library/ogg.hpp"
#include "library/oggopus.hpp"
#include "library/opus.hpp"
#include "core/audioapi.hpp"
#include "core/command.hpp"
#include "core/dispatch.hpp"
#include "core/framerate.hpp"
#include "core/inthread.hpp"
#include "core/keymapper.hpp"
#include "core/settings.hpp"
#include "core/misc.hpp"
#include <cmath>
#include <list>
#include <iostream>
#include <fstream>
#include <cstring>
#include <unistd.h>
#include <sys/time.h>
#include <zlib.h>

//Farther than this, packets can be fastskipped.
#define OPUS_CONVERGE_MAX 5760
//Maximum size of PCM output for one packet.
#define OPUS_MAX_OUT 5760
//Output block size.
#define OUTPUT_BLOCK 1440
//Main sampling rate.
#define OPUS_SAMPLERATE 48000
//Opus block size
#define OPUS_BLOCK_SIZE 960
//Threshold for decoding additional block
#define BLOCK_THRESHOLD 1200
//Maximum output block size.
#define OUTPUT_SIZE (BLOCK_THRESHOLD + OUTPUT_BLOCK)
//Amount of microseconds per interation.
#define ITERATION_TIME 15000
//Opus bitrate to use.
#define OPUS_BITRATE 48000
//Opus min bitrate to use.
#define OPUS_MIN_BITRATE 8000
//Opus max bitrate to use.
#define OPUS_MAX_BITRATE 255000
//Ogg Opus granule rate.
#define OGGOPUS_GRANULERATE 48000
//Record buffer size threshold divider.
#define REC_THRESHOLD_DIV 40
//Playback buffer size threshold divider.
#define PLAY_THRESHOLD_DIV 30
//Special granule position: None.
#define GRANULEPOS_NONE 0xFFFFFFFFFFFFFFFFULL

namespace
{
	class opus_playback_stream;
	class opus_stream;
	class stream_collection;

	setting_var<setting_var_model_int<OPUS_MIN_BITRATE,OPUS_MAX_BITRATE>> opus_bitrate(lsnes_vset, "opus-bitrate",
		"commentary‣Bitrate", OPUS_BITRATE);
	setting_var<setting_var_model_int<OPUS_MIN_BITRATE,OPUS_MAX_BITRATE>> opus_max_bitrate(lsnes_vset,
		"opus-max-bitrate", "commentary‣Max bitrate", OPUS_MAX_BITRATE);

	//Recording active flag.
	volatile bool active_flag = false;
	//Last seen frame number.
	uint64_t last_frame_number = 0;
	//Last seen rate.
	double last_rate = 0;
	//Mutex protecting current_time and time_jump.
	mutex_class time_mutex;
	//The current time.
	uint64_t current_time;
	//Time jump flag. Set if time jump is detected.
	//If time jump is detected, all current playing streams are stopped, stream locks are cleared and
	//apropriate streams are restarted. If time jump is false, all unlocked streams coming into range
	//are started.
	bool time_jump;
	//Lock protecting active_playback_streams.
	mutex_class active_playback_streams_lock;
	//List of streams currently playing.
	std::list<opus_playback_stream*> active_playback_streams;
	//The collection of streams.
	stream_collection* current_collection;
	//Lock protecting current collection.
	mutex_class current_collection_lock;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Bitrate tracker.
	struct bitrate_tracker
	{
		bitrate_tracker() throw();
		void reset() throw();
		double get_min() throw();
		double get_avg() throw();
		double get_max() throw();
		double get_length() throw();
		uint64_t get_bytes() throw();
		uint64_t get_blocks() throw();
		void submit(uint32_t bytes, uint32_t samples) throw();
	private:
		uint64_t blocks;
		uint64_t samples;
		uint64_t bytes;
		uint32_t minrate;
		uint32_t maxrate;
	};

	bitrate_tracker::bitrate_tracker() throw()
	{
		reset();
	}

	void bitrate_tracker::reset() throw()
	{
		blocks = 0;
		samples = 0;
		bytes = 0;
		minrate = std::numeric_limits<uint32_t>::max();
		maxrate = 0;
	}

	double bitrate_tracker::get_min() throw()
	{
		return blocks ? minrate / 1000.0 : 0.0;
	}

	double bitrate_tracker::get_avg() throw()
	{
		return samples ? bytes / (125.0 * samples / OPUS_SAMPLERATE) : 0.0;
	}

	double bitrate_tracker::get_max() throw()
	{
		return blocks ? maxrate / 1000.0 : 0.0;
	}

	double bitrate_tracker::get_length() throw()
	{
		return 1.0 * samples / OPUS_SAMPLERATE;
	}

	uint64_t bitrate_tracker::get_bytes() throw()
	{
		return bytes;
	}

	uint64_t bitrate_tracker::get_blocks() throw()
	{
		return blocks;
	}

	void bitrate_tracker::submit(uint32_t _bytes, uint32_t _samples) throw()
	{
		blocks++;
		samples += _samples;
		bytes += _bytes;
		uint32_t irate = _bytes * 8 * OPUS_SAMPLERATE / OPUS_BLOCK_SIZE;
		minrate = min(minrate, irate);
		maxrate = max(maxrate, irate);
	}

	std::ostream& operator<<(std::ostream& s, bitrate_tracker& t)
	{
		s << t.get_bytes() << " bytes for " << t.get_length() << "s (" << t.get_blocks() << " blocks)"
			<< std::endl << "Bitrate (kbps): min: " << t.get_min() << " avg: " << t.get_avg() << " max:"
			<< t.get_max() << std::endl;
		return s;
	}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Information about individual opus packet in stream.
	struct opus_packetinfo
	{
		//Length is in units of 1/400th of a second.
		opus_packetinfo(uint16_t datasize, uint8_t length, uint64_t offset)
		{
			descriptor = (offset & 0xFFFFFFFFFFULL) | (static_cast<uint64_t>(length) << 40) |
				(static_cast<uint64_t>(datasize) << 48);
		}
		//Get the data size of the packet.
		uint16_t size() { return descriptor >> 48; }
		//Calculate the length of packet in samples.
		uint16_t length() { return 120 * ((descriptor >> 40) & 0xFF); }
		//Calculate the true offset.
		uint64_t offset() { return descriptor & 0xFFFFFFFFFFULL; }
		//Read the packet.
		//Can throw.
		std::vector<unsigned char> packet(filesystem_ref from_sys);
	private:
		uint64_t descriptor;
	};

	std::vector<unsigned char> opus_packetinfo::packet(filesystem_ref from_sys)
	{
		std::vector<unsigned char> ret;
		uint64_t off = offset();
		uint32_t sz = size();
		uint32_t cluster = off / CLUSTER_SIZE;
		uint32_t coff = off % CLUSTER_SIZE;
		ret.resize(sz);
		size_t r = from_sys.read_data(cluster, coff, &ret[0], sz);
		if(r != sz)
			throw std::runtime_error("Incomplete read");
		return ret;
	}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Information about opus stream.
	struct opus_stream
	{
		//Create new empty stream with specified base time.
		opus_stream(uint64_t base, filesystem_ref filesys);
		//Read stream with specified base time and specified start clusters.
		//Can throw.
		opus_stream(uint64_t base, filesystem_ref filesys, uint32_t ctrl_cluster, uint32_t data_cluster);
		//Import a stream with specified base time.
		//Can throw.
		opus_stream(uint64_t base, filesystem_ref filesys, std::ifstream& data,
			external_stream_format extfmt);
		//Delete this stream (also puts a ref)
		void delete_stream() { deleting = true; put_ref(); }
		//Export a stream.
		//Can throw.
		void export_stream(std::ofstream& data, external_stream_format extfmt);
		//Get length of specified packet in samples.
		uint16_t packet_length(uint32_t seqno)
		{
			return (seqno < packets.size()) ? packets[seqno].length() : 0;
		}
		//Get data of specified packet.
		//Can throw.
		std::vector<unsigned char> packet(uint32_t seqno)
		{
			return (seqno < packets.size()) ? packets[seqno].packet(fs) : std::vector<unsigned char>();
		}
		//Get base time in samples for stream.
		uint64_t timebase() { return s_timebase; }
		//Set base time in samples for stream.
		void timebase(uint64_t ts) { s_timebase = ts; }
		//Get length of stream in samples.
		uint64_t length()
		{
			if(pregap_length + postgap_length > total_len)
				return 0;
			else
				return total_len - pregap_length - postgap_length;
		}
		//Set the pregap length.
		void set_pregap(uint32_t p) { pregap_length = p; }
		//Get the pregap length.
		uint32_t get_pregap() { return pregap_length; }
		//Set the postgap length.
		void set_potsgap(uint32_t p) { postgap_length = p; }
		//Get the postgap length.
		uint32_t get_postgap() { return postgap_length; }
		//Set gain.
		void set_gain(int16_t g) { gain = g; }
		//Get gain.
		int16_t get_gain() { return gain; }
		//Get linear gain.
		float get_gain_linear() { return pow(10, gain / 20); }
		//Get number of packets in stream.
		uint32_t blocks() { return packets.size(); }
		//Is this stream locked?
		bool islocked() { return locked; }
		//Lock a stream.
		void lock() { locked = true; }
		//Unlock a stream.
		void unlock() { locked = false; }
		//Increment reference count.
		void get_ref() { umutex_class m(reflock); refcount++; }
		//Decrement reference count, destroying object if it hits zero.
		void put_ref() { umutex_class m(reflock); refcount--; if(!refcount) destroy(); }
		//Add new packet into stream.
		//Not safe to call simultaneously with packet_length() or packet().
		//Can throw.
		void write(uint8_t len, const unsigned char* payload, size_t payload_len);
		//Write stream trailer.
		void write_trailier();
		//Get clusters.
		std::pair<uint32_t, uint32_t> get_clusters() { return std::make_pair(ctrl_cluster, data_cluster); }
	private:
		void export_stream_sox(std::ofstream& data);
		void export_stream_oggopus(std::ofstream& data);
		void import_stream_sox(std::ifstream& data);
		void import_stream_oggopus(std::ifstream& data);

		opus_stream(const opus_stream&);
		opus_stream& operator=(const opus_stream&);
		void destroy();
		filesystem_ref fs;
		std::vector<opus_packetinfo> packets;
		uint64_t total_len;
		uint64_t s_timebase;
		uint32_t next_cluster;
		uint32_t next_offset;
		uint32_t next_mcluster;
		uint32_t next_moffset;
		uint32_t ctrl_cluster;
		uint32_t data_cluster;
		uint32_t pregap_length;
		uint32_t postgap_length;
		int16_t gain;
		bool locked;
		mutex_class reflock;
		unsigned refcount;
		bool deleting;
	};

	opus_stream::opus_stream(uint64_t base, filesystem_ref filesys)
		: fs(filesys)
	{
		refcount = 1;
		deleting = false;
		total_len = 0;
		s_timebase = base;
		locked = false;
		next_cluster = 0;
		next_mcluster = 0;
		next_offset = 0;
		next_moffset = 0;
		ctrl_cluster = 0;
		data_cluster = 0;
		pregap_length = 0;
		postgap_length = 0;
		gain = 0;
	}

	opus_stream::opus_stream(uint64_t base, filesystem_ref filesys, uint32_t _ctrl_cluster,
		uint32_t _data_cluster)
		: fs(filesys)
	{
		refcount = 1;
		deleting = false;
		total_len = 0;
		s_timebase = base;
		locked = false;
		next_cluster = data_cluster = _data_cluster;
		next_mcluster = ctrl_cluster = _ctrl_cluster;
		next_offset = 0;
		next_moffset = 0;
		pregap_length = 0;
		postgap_length = 0;
		gain = 0;
		//Read the data buffers.
		char buf[CLUSTER_SIZE];
		uint32_t last_cluster_seen = next_mcluster;
		uint64_t total_size = 0;
		uint64_t total_frames = 0;
		bool trailers = false;
		bool saved_pointer_valid = false;
		uint32_t saved_next_mcluster = 0;
		uint32_t saved_next_moffset = 0;
		while(true) {
			last_cluster_seen = next_mcluster;
			size_t r = fs.read_data(next_mcluster, next_moffset, buf, CLUSTER_SIZE);
			if(!r) {
				//The stream ends here.
				break;
			}
			//Find the first unused entry if any.
			for(unsigned i = 0; i < CLUSTER_SIZE; i += 4)
				if(!buf[i + 3] || trailers) {
					//This entry is unused. If the next entry is also unused, that is the end.
					//Otherwise, there might be stream trailers.
					if(trailers && !buf[i + 3]) {
						goto out_parsing;		//Ends for real.
					}
					if(!trailers) {
						//Set the trailer flag and continue parsing.
						//The saved offset must be placed here.
						saved_next_mcluster = last_cluster_seen;
						saved_next_moffset = i;
						saved_pointer_valid = true;
						trailers = true;
						continue;
					}
					//This is a trailer entry.
					if(buf[i + 3] == 2) {
						//Pregap.
						pregap_length = read32ube(buf + i) >> 8;
					} else if(buf[i + 3] == 3) {
						//Postgap.
						postgap_length = read32ube(buf + i) >> 8;
					} else if(buf[i + 3] == 4) {
						//Gain.
						gain = read16sbe(buf + i);
					}
				} else {
					uint16_t psize = read16ube(buf + i);
					uint8_t plen = read8ube(buf + i + 2);
					total_size += psize;
					total_len += 120 * plen;
					opus_packetinfo p(psize, plen, 1ULL * next_cluster * CLUSTER_SIZE +
						next_offset);
					size_t r2 = fs.skip_data(next_cluster, next_offset, psize);
					if(r2 < psize)
						throw std::runtime_error("Incomplete data stream");
					packets.push_back(p);
					total_frames++;
				}
		}
out_parsing:
		//If saved pointer is valid, restore to that.
		if(saved_pointer_valid) {
			next_mcluster = saved_next_mcluster;
			next_moffset = saved_next_moffset;
		}
	}

	opus_stream::opus_stream(uint64_t base, filesystem_ref filesys, std::ifstream& data,
		external_stream_format extfmt)
		: fs(filesys)
	{
		refcount = 1;
		deleting = false;
		total_len = 0;
		s_timebase = base;
		locked = false;
		next_cluster = 0;
		next_mcluster = 0;
		next_offset = 0;
		next_moffset = 0;
		ctrl_cluster = 0;
		data_cluster = 0;
		pregap_length = 0;
		postgap_length = 0;
		gain = 0;
		if(extfmt == EXTFMT_OGGOPUS)
			import_stream_oggopus(data);
		else if(extfmt == EXTFMT_SOX)
			import_stream_sox(data);
	}

	void opus_stream::import_stream_oggopus(std::ifstream& data)
	{
		ogg_stream_reader_iostreams reader(data);
		reader.set_errors_to(messages);
		struct oggopus_header h;
		struct oggopus_tags t;
		ogg_page page;
		ogg_demuxer d(messages);
		int state = 0;
		postgap_length = 0;
		uint64_t datalen = 0;
		uint64_t last_datalen = 0;
		uint64_t last_granulepos = 0;
		try {
			while(true) {
				ogg_packet p;
				if(!d.wants_packet_out()) {
					if(!reader.get_page(page))
						break;
					d.page_in(page);
					continue;
				} else
					d.packet_out(p);
				switch(state) {
				case 0:		//Not locked.
					h = parse_oggopus_header(p);
					if(h.streams != 1)
						throw std::runtime_error("Multistream OggOpus streams are not "
							"supported");
					state = 1;	//Expecting comment.
					pregap_length = h.preskip;
					gain = h.gain;
					break;
				case 1:		//Expecting comment.
					t = parse_oggopus_tags(p);
					state = 2;	//Data page.
					if(page.get_eos())
						throw std::runtime_error("Empty OggOpus stream");
					break;
				case 2:		//Data page.
				case 3:		//Data page.
					const std::vector<uint8_t>& pkt = p.get_vector();
					uint8_t tcnt = opus_packet_tick_count(&pkt[0], pkt.size());
					if(tcnt) {
						write(tcnt, &pkt[0], pkt.size());
						datalen += tcnt * 120;
					}
					if(p.get_last_page()) {
						uint64_t samples = p.get_granulepos() - last_granulepos;
						if(samples > p.get_granulepos())
						samples = 0;
						uint64_t rsamples = datalen - last_datalen;
						if((samples > rsamples && state == 3) || (samples <
							rsamples && !p.get_on_eos_page()))
							messages << "Warning: Granulepos says there are "
								<< samples << " samples, found " << rsamples
								<< std::endl;
						last_datalen = datalen;
						last_granulepos = p.get_granulepos();
						if(p.get_on_eos_page()) {
							if(samples < rsamples)
								postgap_length = rsamples - samples;
							state = 4;
							goto out;
						}
					}
					state = 3;
					break;
				}
			}
out:
			if(state == 0)
				throw std::runtime_error("No OggOpus stream found");
			if(state == 1)
				throw std::runtime_error("Oggopus stream missing required tags pages");
			if(state == 2 || state == 3)
				messages << "Warning: Incomplete Oggopus stream." << std::endl;
			if(datalen <= pregap_length)
				throw std::runtime_error("Stream too short (entiere pregap not present)");
			write_trailier();
		} catch(...) {
			if(ctrl_cluster) fs.free_cluster_chain(ctrl_cluster);
			if(data_cluster) fs.free_cluster_chain(data_cluster);
			throw;
		}
	}

	void opus_stream::import_stream_sox(std::ifstream& data)
	{
		bitrate_tracker brtrack;
		int err;
		unsigned char tmpi[65536];
		float tmp[OPUS_MAX_OUT];
		char header[260];
		data.read(header, 32);
		if(!data)
			throw std::runtime_error("Can't read .sox header");
		if(read32ule(header + 0) != 0x586F532EULL)
			throw std::runtime_error("Bad .sox header magic");
		if(read8ube(header + 4) > 28)
			data.read(header + 32, read8ube(header + 4) - 28);
		if(!data)
			throw std::runtime_error("Can't read .sox header");
		if(read64ule(header + 16) != 4676829883349860352ULL)
			throw std::runtime_error("Bad .sox sampling rate");
		if(read32ule(header + 24) != 1)
			throw std::runtime_error("Only mono streams are supported");
		uint64_t samples = read64ule(header + 8);
		opus::encoder enc(opus::samplerate::r48k, false, opus::application::voice);
		enc.ctl(opus::bitrate(opus_bitrate.get()));
		int32_t pregap = enc.ctl(opus::lookahead);
		pregap_length = pregap;
		for(uint64_t i = 0; i < samples + pregap; i += OPUS_BLOCK_SIZE) {
			size_t bs = OPUS_BLOCK_SIZE;
			if(i + bs > samples + pregap)
				bs = samples + pregap - i;
			//We have to read zero bytes after the end of stream.
			size_t readable = bs;
			if(readable + i > samples)
				readable = max(samples, i) - i;
			if(readable > 0)
				data.read(reinterpret_cast<char*>(tmpi), 4 * readable);
			if(readable < bs)
				memset(tmpi + 4 * readable, 0, 4 * (bs - readable));
			if(!data) {
				if(ctrl_cluster) fs.free_cluster_chain(ctrl_cluster);
				if(data_cluster) fs.free_cluster_chain(data_cluster);
				throw std::runtime_error("Can't read .sox data");
			}
			for(size_t j = 0; j < bs; j++)
				tmp[j] = static_cast<float>(read32sle(tmpi + 4 * j)) / 268435456;
			if(bs < OPUS_BLOCK_SIZE)
				postgap_length = OPUS_BLOCK_SIZE - bs;
			for(size_t j = bs; j < OPUS_BLOCK_SIZE; j++)
				tmp[j] = 0;
			try {
				const size_t opus_out_max2 = opus_max_bitrate.get() * OPUS_BLOCK_SIZE / 384000;
				size_t r = enc.encode(tmp, OPUS_BLOCK_SIZE, tmpi, opus_out_max2);
				write(OPUS_BLOCK_SIZE / 120, tmpi, r);
				brtrack.submit(r, bs);
			} catch(std::exception& e) {
				if(ctrl_cluster) fs.free_cluster_chain(ctrl_cluster);
				if(data_cluster) fs.free_cluster_chain(data_cluster);
				(stringfmt() << "Error encoding opus packet: " << e.what()).throwex();
			}
		}
		messages << "Imported stream: " << brtrack;
		try {
			write_trailier();
		} catch(...) {
			if(ctrl_cluster) fs.free_cluster_chain(ctrl_cluster);
			if(data_cluster) fs.free_cluster_chain(data_cluster);
			throw;
		}
	}

	void opus_stream::destroy()
	{
		if(deleting) {
			//We catch the errors and print em, because otherwise put_ref could throw, which would
			//be too much.
			try {
				fs.free_cluster_chain(ctrl_cluster);
			} catch(std::exception& e) {
				messages << "Failed to delete stream control file: " << e.what();
			}
			try {
				fs.free_cluster_chain(data_cluster);
			} catch(std::exception& e) {
				messages << "Failed to delete stream data file: " << e.what();
			}
		}
		delete this;
	}

	void opus_stream::export_stream_oggopus(std::ofstream& data)
	{
		if(!packets.size())
			throw std::runtime_error("Empty oggopus stream is not valid");
		oggopus_header header;
		oggopus_tags tags;
		ogg_stream_writer_iostreams writer(data);
		unsigned stream_id = 1;
		uint64_t true_granule = 0;
		uint32_t seq = 2;
		//Headers / Tags.
		header.version = 1;
		header.channels = 1;
		header.preskip = pregap_length;
		header.rate = OPUS_SAMPLERATE;
		header.gain = 0;
		header.map_family = 0;
		header.streams = 1;
		header.coupled = 0;
		header.chanmap[0] = 0;
		memset(header.chanmap + 1, 255, 254);
		tags.vendor = "unknown";
		tags.comments.push_back((stringfmt() << "ENCODER=lsnes rr" + lsnes_version).str());
		tags.comments.push_back((stringfmt() << "LSNES_STREAM_TS=" << s_timebase).str());

		struct ogg_page hpage = serialize_oggopus_header(header);
		hpage.set_stream(stream_id);
		writer.put_page(hpage);
		seq = serialize_oggopus_tags(tags, [&writer](const ogg_page& p) { writer.put_page(p); }, stream_id);

		struct ogg_page ppage;
		ogg_muxer mux(stream_id, seq);
		for(size_t i = 0; i < packets.size(); i++) {
			std::vector<unsigned char> p;
			try {
				p = packet(i);
			} catch(std::exception& e) {
				(stringfmt() << "Error reading opus packet: " << e.what()).throwex();
			}
			if(!p.size())
				(stringfmt() << "Empty Opus packet is not valid").throwex();
			uint32_t samples = static_cast<uint32_t>(opus_packet_tick_count(&p[0], p.size())) * 120;
			if(i + 1 < packets.size())
				true_granule += samples;
			else
				true_granule = max(true_granule, true_granule + samples - postgap_length);
			if(!mux.wants_packet_in() || !mux.packet_fits(p.size()))
				while(mux.has_page_out()) {
					mux.page_out(ppage);
					writer.put_page(ppage);
				}
			mux.packet_in(p, true_granule);
		}
		mux.signal_eos();
		while(mux.has_page_out()) {
			mux.page_out(ppage);
			writer.put_page(ppage);
		}
	}

	void opus_stream::export_stream_sox(std::ofstream& data)
	{
		int err;
		opus::decoder dec(opus::samplerate::r48k, false);
		std::vector<unsigned char> p;
		float tmp[OPUS_MAX_OUT];
		char header[32];
		write64ule(header, 0x1C586F532EULL);			//Magic and header size.
		write64ule(header + 16, 4676829883349860352ULL);	//Sampling rate.
		write32ule(header + 24, 1);
		uint64_t tlen = 0;
		uint32_t lookahead_thrown = 0;
		data.write(header, 32);
		if(!data)
			throw std::runtime_error("Error writing PCM data.");
		float lgain = get_gain_linear();
		for(size_t i = 0; i < packets.size(); i++) {
			char blank[4] = {0, 0, 0, 0};
			try {
				uint32_t pregap_throw = 0;
				uint32_t postgap_throw = 0;
				std::vector<unsigned char> p = packet(i);
				uint32_t len = packet_length(i);
				size_t r = dec.decode(&p[0], p.size(), tmp, OPUS_MAX_OUT);
				bool is_last = (i == packets.size() - 1);
				if(lookahead_thrown < pregap_length) {
					//We haven't yet thrown the full pregap. Throw some.
					uint32_t maxthrow = pregap_length - lookahead_thrown;
					pregap_throw = min(len, maxthrow);
					lookahead_thrown += pregap_length;
				}
				if(is_last)
					postgap_throw = min(len - pregap_throw, postgap_length);
				tlen += (len - pregap_throw - postgap_throw);
				for(uint32_t j = pregap_throw; j < len - postgap_throw; j++) {
					int32_t s = (int32_t)(tmp[j] * lgain * 268435456.0);
					write32sle(blank, s);
					data.write(blank, 4);
					if(!data)
						throw std::runtime_error("Error writing PCM data.");
				}
			} catch(std::exception& e) {
				(stringfmt() << "Error decoding opus packet: " << e.what()).throwex();
			}
		}
		data.seekp(0, std::ios_base::beg);
		write64ule(header + 8, tlen);
		data.write(header, 32);
		if(!data) {
			throw std::runtime_error("Error writing PCM data.");
		}
	}

	void opus_stream::export_stream(std::ofstream& data, external_stream_format extfmt)
	{
		if(extfmt == EXTFMT_OGGOPUS)
			export_stream_oggopus(data);
		else if(extfmt == EXTFMT_SOX)
			export_stream_sox(data);
	}

	void opus_stream::write(uint8_t len, const unsigned char* payload, size_t payload_len)
	{
		try {
			char descriptor[4];
			uint32_t used_cluster, used_offset;
			uint32_t used_mcluster, used_moffset;
			if(!next_cluster)
				next_cluster = data_cluster = fs.allocate_cluster();
			if(!next_mcluster)
				next_mcluster = ctrl_cluster = fs.allocate_cluster();
			write16ube(descriptor, payload_len);
			write8ube(descriptor + 2, len);
			write8ube(descriptor + 3, 1);
			fs.write_data(next_cluster, next_offset, payload, payload_len, used_cluster, used_offset);
			fs.write_data(next_mcluster, next_moffset, descriptor, 4, used_mcluster, used_moffset);
			uint64_t off = static_cast<uint64_t>(used_cluster) * CLUSTER_SIZE + used_offset;
			opus_packetinfo p(payload_len, len, off);
			total_len += p.length();
			packets.push_back(p);
		} catch(std::exception& e) {
			(stringfmt() << "Can't write opus packet: " << e.what()).throwex();
		}
	}

	void opus_stream::write_trailier()
	{
		try {
			char descriptor[16];
			uint32_t used_mcluster, used_moffset;
			//The allocation must be done for real.
			if(!next_mcluster)
				next_mcluster = ctrl_cluster = fs.allocate_cluster();
			//But the write must not update the pointers..
			uint32_t tmp_mcluster = next_mcluster;
			uint32_t tmp_moffset = next_moffset;
			write32ube(descriptor, 0);
			write32ube(descriptor + 4, (pregap_length << 8) | 0x02);
			write32ube(descriptor + 8, (postgap_length << 8) | 0x03);
			write16sbe(descriptor + 12, gain);
			write16ube(descriptor + 14, 0x0004);
			fs.write_data(tmp_mcluster, tmp_moffset, descriptor, 16, used_mcluster, used_moffset);
		} catch(std::exception& e) {
			(stringfmt() << "Can't write stream trailer: " << e.what()).throwex();
		}
	}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Playing opus stream.
	struct opus_playback_stream
	{
		//Create a new playing stream from given opus stream.
		opus_playback_stream(opus_stream& data);
		//Destroy playing opus stream.
		~opus_playback_stream();
		//Read samples from stream.
		//Can throw.
		void read(float* data, size_t samples);
		//Skip samples from stream.
		//Can throw.
		void skip(uint64_t samples);
		//Has the stream already ended?
		bool eof();
	private:
		opus_playback_stream(const opus_playback_stream&);
		opus_playback_stream& operator=(const opus_playback_stream&);
		//Can throw.
		void decode_block();
		float output[OPUS_MAX_OUT];
		unsigned output_left;
		uint32_t pregap_thrown;
		bool postgap_thrown;
		opus::decoder* decoder;
		opus_stream& stream;
		uint32_t next_block;
		uint32_t blocks;
	};

	opus_playback_stream::opus_playback_stream(opus_stream& data)
		: stream(data)
	{
		int err;
		stream.get_ref();
		stream.lock();
		next_block = 0;
		output_left = 0;
		pregap_thrown = 0;
		postgap_thrown = false;
		blocks = stream.blocks();
		decoder = new opus::decoder(opus::samplerate::r48k, false);
		if(!decoder)
			throw std::bad_alloc();
	}

	opus_playback_stream::~opus_playback_stream()
	{
		//No, we don't unlock the stream.
		stream.put_ref();
		delete decoder;
	}

	bool opus_playback_stream::eof()
	{
		return (next_block >= blocks && !output_left);
	}

	void opus_playback_stream::decode_block()
	{
		if(next_block >= blocks)
			return;
		if(output_left >= OPUS_MAX_OUT)
			return;
		unsigned plen = stream.packet_length(next_block);
		if(plen + output_left > OPUS_MAX_OUT)
			return;
		std::vector<unsigned char> pdata = stream.packet(next_block);
		try {
			size_t c = decoder->decode(&pdata[0], pdata.size(), output + output_left,
				OPUS_MAX_OUT - output_left);
			output_left = min(output_left + c, static_cast<size_t>(OPUS_MAX_OUT));
		} catch(...) {
			//Bad packet, insert silence.
			for(unsigned i = 0; i < plen; i++)
				output[output_left++] = 0;
		}
		//Throw the pregap away if needed.
		if(pregap_thrown < stream.get_pregap()) {
			uint32_t throw_amt = min(stream.get_pregap() - pregap_thrown, (uint32_t)output_left);
			if(throw_amt && throw_amt < output_left)
				memmove(output, output + throw_amt, (output_left - throw_amt) * sizeof(float));
			output_left -= throw_amt;
			pregap_thrown += throw_amt;
		}
		next_block++;
	}

	void opus_playback_stream::read(float* data, size_t samples)
	{
		float lgain = stream.get_gain_linear();
		while(samples > 0) {
			decode_block();
			if(next_block >= blocks && !postgap_thrown) {
				//This is the final packet. Throw away postgap samples at the end.
				uint32_t thrown = min(stream.get_postgap(), (uint32_t)output_left);
				output_left -= thrown;
				postgap_thrown = true;
			}
			if(next_block >= blocks && !output_left) {
				//Zerofill remainder.
				for(size_t i = 0; i < samples; i++)
					data[i] = 0;
				return;
			}
			unsigned maxcopy = min(static_cast<unsigned>(samples), output_left);
			if(maxcopy) {
				memcpy(data, output, maxcopy * sizeof(float));
				for(size_t i = 0; i < maxcopy; i++)
					data[i] *= lgain;
			}
			if(maxcopy < output_left && maxcopy)
				memmove(output, output + maxcopy, (output_left - maxcopy) * sizeof(float));
			output_left -= maxcopy;
			samples -= maxcopy;
			data += maxcopy;
		}
	}

	void opus_playback_stream::skip(uint64_t samples)
	{
		//Adjust for preskip and declare all preskip already thrown away.
		pregap_thrown = stream.get_pregap();
		samples += pregap_thrown;
		postgap_thrown = false;
		//First, skip inside decoded samples.
		if(samples < output_left) {
			//Skipping less than amount in output buffer. Just discard from output buffer and try
			//to decode a new block.
			memmove(output, output + samples, (output_left - samples) * sizeof(float));
			output_left -= samples;
			decode_block();
			return;
		} else {
			//Skipping at least the amount of samples in output buffer. First, blank the output buffer
			//and count those towards samples discarded.
			samples -= output_left;
			output_left = 0;
		}
		//While number of samples is so great that adequate convergence period can be ensured without
		//decoding this packet, just skip the samples from the packet.
		while(samples > OPUS_CONVERGE_MAX) {
			samples -= stream.packet_length(next_block++);
			//Did we hit EOF?
			if(next_block >= blocks)
				return;
		}
		//Okay, we are near the point. Start decoding packets.
		while(samples > 0) {
			decode_block();
			//Did we hit EOF?
			if(next_block >= blocks && !output_left)
				return;
			//Skip as many samples as possible.
			unsigned maxskip = min(static_cast<unsigned>(samples), output_left);
			if(maxskip < output_left)
				memmove(output, output + maxskip, (output_left - maxskip) * sizeof(float));
			output_left -= maxskip;
			samples -= maxskip;
		}
		//Just to be nice, decode a extra block.
		decode_block();
	}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Collection of streams.
	struct stream_collection
	{
	public:
		//Create a new collection.
		//Can throw.
		stream_collection(filesystem_ref filesys);
		//Destroy a collection. All streams are destroyed but not deleted.
		~stream_collection();
		//Get list of streams active at given point.
		std::list<uint64_t> streams_at(uint64_t point);
		//Add a stream into collection.
		//Can throw.
		uint64_t add_stream(opus_stream& stream);
		//Get the filesystem this collection is for.
		filesystem_ref get_filesystem() { return fs; }
		//Unlock all streams in collection.
		void unlock_all();
		//Get stream with given index (NULL if not found).
		opus_stream* get_stream(uint64_t index)
		{
			umutex_class m(mutex);
			if(streams.count(index)) {
				streams[index]->get_ref();
				return streams[index];
			}
			return NULL;
		}
		//Delete a stream.
		//Can throw.
		void delete_stream(uint64_t index);
		//Alter stream timebase.
		//Can throw.
		void alter_stream_timebase(uint64_t index, uint64_t newts);
		//Alter stream gain.
		void alter_stream_gain(uint64_t index, uint16_t newgain);
		//Enumerate all valid stream indices, in time order.
		std::list<uint64_t> all_streams();
		//Export the entiere superstream.
		//Can throw.
		void export_superstream(std::ofstream& out);
	private:
		filesystem_ref fs;
		uint64_t next_index;
		unsigned next_stream;
		mutex_class mutex;
		std::set<uint64_t> free_indices;
		std::map<uint64_t, uint64_t> entries;
		std::multimap<uint64_t, uint64_t> streams_by_time;
		//FIXME: Something more efficient.
		std::map<uint64_t, opus_stream*> streams;
	};

	stream_collection::stream_collection(filesystem_ref filesys)
		: fs(filesys)
	{
		next_stream = 0;
		next_index = 0;
		//The stream index table is in cluster 2.
		uint32_t next_cluster = 2;
		uint32_t next_offset = 0;
		uint32_t i = 0;
		try {
			while(true) {
				char buffer[16];
				size_t r = fs.read_data(next_cluster, next_offset, buffer, 16);
				if(r < 16)
					break;
				uint64_t timebase = read64ube(buffer);
				uint32_t ctrl_cluster = read32ube(buffer + 8);
				uint32_t data_cluster = read32ube(buffer + 12);
				if(ctrl_cluster) {
					opus_stream* x = new opus_stream(timebase, fs, ctrl_cluster, data_cluster);
					entries[next_index] = i;
					streams_by_time.insert(std::make_pair(timebase, next_index));
					streams[next_index++] = x;
				} else
					free_indices.insert(i);
				next_stream = ++i;
			}
		} catch(std::exception& e) {
			for(auto i : streams)
				i.second->put_ref();
			(stringfmt() << "Failed to parse LSVS: " << e.what()).throwex();
		}
	}

	stream_collection::~stream_collection()
	{
		umutex_class m(mutex);
		for(auto i : streams)
			i.second->put_ref();
		streams.clear();
	}

	std::list<uint64_t> stream_collection::streams_at(uint64_t point)
	{
		umutex_class m(mutex);
		std::list<uint64_t> s;
		for(auto i : streams) {
			uint64_t start = i.second->timebase();
			uint64_t end = start + i.second->length();
			if(point >= start && point < end) {
				i.second->get_ref();
				s.push_back(i.first);
			}
		}
		return s;
	}

	uint64_t stream_collection::add_stream(opus_stream& stream)
	{
		uint64_t idx;
		try {
			umutex_class m(mutex);
			//Lock the added stream so it doesn't start playing back immediately.
			stream.lock();
			idx = next_index++;
			streams[idx] = &stream;
			char buffer[16];
			write64ube(buffer, stream.timebase());
			auto r = stream.get_clusters();
			write32ube(buffer + 8, r.first);
			write32ube(buffer + 12, r.second);
			uint64_t entry_number = 0;
			if(free_indices.empty())
				entry_number = next_stream++;
			else {
				entry_number = *free_indices.begin();
				free_indices.erase(entry_number);
			}
			uint32_t write_cluster = 2;
			uint32_t write_offset = 0;
			uint32_t dummy1, dummy2;
			fs.skip_data(write_cluster, write_offset, 16 * entry_number);
			fs.write_data(write_cluster, write_offset, buffer, 16, dummy1, dummy2);
			streams_by_time.insert(std::make_pair(stream.timebase(), idx));
			entries[idx] = entry_number;
			return idx;
		} catch(std::exception& e) {
			(stringfmt() << "Failed to add stream: " << e.what()).throwex();
		}
		return idx;
	}

	void stream_collection::unlock_all()
	{
		umutex_class m(mutex);
		for(auto i : streams)
			i.second->unlock();
	}

	void stream_collection::delete_stream(uint64_t index)
	{
		umutex_class m(mutex);
		if(!entries.count(index))
			return;
		uint64_t entry_number = entries[index];
		uint32_t write_cluster = 2;
		uint32_t write_offset = 0;
		uint32_t dummy1, dummy2;
		char buffer[16] = {0};
		fs.skip_data(write_cluster, write_offset, 16 * entry_number);
		fs.write_data(write_cluster, write_offset, buffer, 16, dummy1, dummy2);
		auto itr = streams_by_time.lower_bound(streams[index]->timebase());
		auto itr2 = streams_by_time.upper_bound(streams[index]->timebase());
		for(auto x = itr; x != itr2; x++)
			if(x->second == index) {
				streams_by_time.erase(x);
				break;
			}
		streams[index]->delete_stream();
		streams.erase(index);
	}

	void stream_collection::alter_stream_timebase(uint64_t index, uint64_t newts)
	{
		try {
			umutex_class m(mutex);
			if(!streams.count(index))
				return;
			if(entries.count(index)) {
				char buffer[8];
				uint32_t write_cluster = 2;
				uint32_t write_offset = 0;
				uint32_t dummy1, dummy2;
				write64ube(buffer, newts);
				fs.skip_data(write_cluster, write_offset, 16 * entries[index]);
				fs.write_data(write_cluster, write_offset, buffer, 8, dummy1, dummy2);
			}
			auto itr = streams_by_time.lower_bound(streams[index]->timebase());
			auto itr2 = streams_by_time.upper_bound(streams[index]->timebase());
			for(auto x = itr; x != itr2; x++)
				if(x->second == index) {
					streams_by_time.erase(x);
					break;
				}
			streams[index]->timebase(newts);
			streams_by_time.insert(std::make_pair(newts, index));
		} catch(std::exception& e) {
			(stringfmt() << "Failed to alter stream timebase: " << e.what()).throwex();
		}
	}

	void stream_collection::alter_stream_gain(uint64_t index, uint16_t newgain)
	{
		try {
			umutex_class m(mutex);
			if(!streams.count(index))
				return;
			streams[index]->set_gain(newgain);
			streams[index]->write_trailier();
		} catch(std::exception& e) {
			(stringfmt() << "Failed to alter stream gain: " << e.what()).throwex();
		}
	}

	std::list<uint64_t> stream_collection::all_streams()
	{
		umutex_class m(mutex);
		std::list<uint64_t> s;
		for(auto i : streams_by_time)
			s.push_back(i.second);
		return s;
	}

	void stream_collection::export_superstream(std::ofstream& out)
	{
		std::list<uint64_t> slist = all_streams();
		//Find the total length of superstream.
		uint64_t len = 0;
		for(auto i : slist) {
			opus_stream* s = get_stream(i);
			if(s) {
				len = max(len, s->timebase() + s->length());
				s->put_ref();
			}
		}
		char header[32];
		write64ule(header, 0x1C586F532EULL);			//Magic and header size.
		write64ule(header + 8, len);
		write64ule(header + 16, 4676829883349860352ULL);	//Sampling rate.
		write64ule(header + 24, 1);
		out.write(header, 32);
		if(!out)
			throw std::runtime_error("Error writing PCM output");

		//Find the first valid stream.
		auto next_i = slist.begin();
		opus_stream* next_stream = NULL;
		while(next_i != slist.end()) {
			next_stream = get_stream(*next_i);
			next_i++;
			if(next_stream)
				break;
		}
		uint64_t next_ts;
		next_ts = next_stream ? next_stream->timebase() : len;

		std::list<opus_playback_stream*> active;
		try {
			for(uint64_t s = 0; s < len;) {
				if(s == next_ts) {
					active.push_back(new opus_playback_stream(*next_stream));
					next_stream->put_ref();
					next_stream = NULL;
					while(next_i != slist.end()) {
						next_stream = get_stream(*next_i);
						next_i++;
						if(!next_stream)
							continue;
						uint64_t next_ts = next_stream->timebase();
						if(next_ts > s)
							break;
						//Okay, this starts too...
						active.push_back(new opus_playback_stream(*next_stream));
						next_stream->put_ref();
						next_stream = NULL;
					};
					next_ts = next_stream ? next_stream->timebase() : len;
				}
				uint64_t maxsamples = min(next_ts - s, static_cast<uint64_t>(OUTPUT_BLOCK));
				maxsamples = min(maxsamples, len - s);
				char outbuf[4 * OUTPUT_BLOCK];
				float buf1[OUTPUT_BLOCK];
				float buf2[OUTPUT_BLOCK];
				for(size_t t = 0; t < maxsamples; t++)
					buf1[t] = 0;
				for(auto t : active) {
					t->read(buf2, maxsamples);
					for(size_t u = 0; u < maxsamples; u++)
						buf1[u] += buf2[u];
				}
				for(auto t = active.begin(); t != active.end();) {
					if((*t)->eof()) {
						auto todel = t;
						t++;
						delete *todel;
						active.erase(todel);
					} else
						t++;
				}
				for(size_t t = 0; t < maxsamples; t++)
					write32sle(outbuf + 4 * t, buf1[t] * 268435456);
				out.write(outbuf, 4 * maxsamples);
				if(!out)
					throw std::runtime_error("Failed to write PCM");
				s += maxsamples;
			}
		} catch(std::exception& e) {
			(stringfmt() << "Failed to export PCM: " << e.what()).throwex();
		}
		for(auto t = active.begin(); t != active.end();) {
			if((*t)->eof()) {
				auto todelete = t;
				t++;
				delete *todelete;
				active.erase(todelete);
			} else
				t++;
		}
	}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	void start_management_stream(opus_stream& s)
	{
		opus_playback_stream* p = new opus_playback_stream(s);
		umutex_class m(active_playback_streams_lock);
		active_playback_streams.push_back(p);
	}

	void advance_time(uint64_t newtime)
	{
		umutex_class m2(current_collection_lock);
		if(!current_collection) {
			//Clear all.
			umutex_class m(active_playback_streams_lock);
			for(auto i : active_playback_streams)
				delete i;
			active_playback_streams.clear();
			return;
		}
		std::list<uint64_t> sactive = current_collection->streams_at(newtime);
		for(auto j : sactive) {
			opus_stream* i = current_collection->get_stream(j);
			if(!i)
				continue;
			//Don't play locked streams in order to avoid double playing.
			umutex_class m(active_playback_streams_lock);
			try {
				if(!i->islocked())
					active_playback_streams.push_back(new opus_playback_stream(*i));
			} catch(std::exception& e) {
				messages << "Can't start stream: " << e.what() << std::endl;
			}
			i->put_ref();
		}
	}

	void jump_time(uint64_t newtime)
	{
		umutex_class m2(current_collection_lock);
		if(!current_collection) {
			//Clear all.
			umutex_class m(active_playback_streams_lock);
			for(auto i : active_playback_streams)
				delete i;
			active_playback_streams.clear();
			return;
		}
		//Close all currently playing streams.
		{
			umutex_class m(active_playback_streams_lock);
			for(auto i : active_playback_streams)
				delete i;
			active_playback_streams.clear();
		}
		//Unlock all streams, so they will play.
		current_collection->unlock_all();
		//Reopen all streams that should be open (with seeking)
		std::list<uint64_t> sactive = current_collection->streams_at(newtime);
		for(auto j : sactive) {
			opus_stream* i = current_collection->get_stream(j);
			if(!i)
				continue;
			//No need to check for locks, because we just busted all of those.
			uint64_t p = newtime - i->timebase();
			opus_playback_stream* s;
			try {
				s = new opus_playback_stream(*i);
			} catch(std::exception& e) {
				messages << "Can't start stream: " << e.what() << std::endl;
			}
			i->put_ref();
			if(!s)
				continue;
			s->skip(p);
			umutex_class m(active_playback_streams_lock);
			active_playback_streams.push_back(s);
		}
	}

	//Resample.
	void do_resample(audioapi_resampler& r, float* srcbuf, size_t& srcuse, float* dstbuf, size_t& dstuse,
		size_t dstmax, double ratio)
	{
		if(srcuse == 0 || dstuse >= dstmax)
			return;
		float* in = srcbuf;
		size_t in_u = srcuse;
		float* out = dstbuf + dstuse;
		size_t out_u = dstmax - dstuse;
		r.resample(in, in_u, out, out_u, ratio, false);
		size_t offset = in - srcbuf;
		if(offset < srcuse)
			memmove(srcbuf, srcbuf + offset, sizeof(float) * (srcuse - offset));
		srcuse -= offset;
		dstuse = dstmax - out_u;
	}

	//Drain the input buffer.
	void drain_input()
	{
		while(audioapi_voice_r_status() > 0) {
			float buf[256];
			unsigned size = min(audioapi_voice_r_status(), 256u);
			audioapi_record_voice(buf, size);
		}
	}

	//Read the input buffer.
	void read_input(float* buf, size_t& use, size_t maxuse)
	{
		size_t rleft = audioapi_voice_r_status();
		unsigned toread = min(rleft, max(maxuse, use) - use);
		if(toread > 0) {
			audioapi_record_voice(buf + use, toread);
			use += toread;
		}
	}

	//Compress Opus block.
	void compress_opus_block(opus::encoder& e, float* buf, size_t& use, opus_stream& active_stream,
		bitrate_tracker& brtrack)
	{
		const size_t opus_out_max = 1276;
		unsigned char opus_output[opus_out_max];
		size_t cblock = 0;
		if(use >= 960)
			cblock = 960;
		else if(use >= 480)
			cblock = 480;
		else if(use >= 240)
			cblock = 240;
		else if(use >= 120)
			cblock = 120;
		else
			return;		//No valid data to compress.
		const size_t opus_out_max2 = opus_max_bitrate.get() * cblock / 384000;
		try {
			size_t c = e.encode(buf, cblock, opus_output, opus_out_max2);
			//Successfully compressed a block.
			size_t opus_output_len = c;
			brtrack.submit(c, cblock);
			try {
				active_stream.write(cblock / 120, opus_output, opus_output_len);
			} catch(std::exception& e) {
				messages << "Error writing data: " << e.what() << std::endl;
			}
		} catch(std::exception& e) {
			messages << "Opus encoder error: " << e.what() << std::endl;
		}
		use -= cblock;
	}

	void update_time()
	{
		uint64_t sampletime;
		bool jumping;
		{
			umutex_class m(time_mutex);
			sampletime = current_time;
			jumping = time_jump;
			time_jump = false;
		}
		if(jumping)
			jump_time(sampletime);
		else
			advance_time(sampletime);
	}

	void decompress_active_streams(float* out, size_t& use)
	{
		size_t base = use;
		use += OUTPUT_BLOCK;
		for(unsigned i = 0; i < OUTPUT_BLOCK; i++)
			out[i + base] = 0;
		//Do it this way to minimize the amount of time playback streams lock
		//is held.
		std::list<opus_playback_stream*> stmp;
		{
			umutex_class m(active_playback_streams_lock);
			stmp = active_playback_streams;
		}
		std::set<opus_playback_stream*> toerase;
		for(auto i : stmp) {
			float tmp[OUTPUT_BLOCK];
			try {
				i->read(tmp, OUTPUT_BLOCK);
			} catch(std::exception& e) {
				messages << "Failed to decompress: " << e.what() << std::endl;
				for(unsigned j = 0; j < OUTPUT_BLOCK; j++)
					tmp[j] = 0;
			}
			for(unsigned j = 0; j < OUTPUT_BLOCK; j++)
				out[j + base] += tmp[j];
			if(i->eof())
				toerase.insert(i);
		}
		{
			umutex_class m(active_playback_streams_lock);
			for(auto i = active_playback_streams.begin(); i != active_playback_streams.end();) {
				if(toerase.count(*i)) {
					auto toerase = i;
					i++;
					delete *toerase;
					active_playback_streams.erase(toerase);
				} else
					i++;
			}
		}
	}

	void handle_tangent_positive_edge(opus::encoder& e, opus_stream*& active_stream, bitrate_tracker& brtrack)
	{
		umutex_class m2(current_collection_lock);
		if(!current_collection)
			return;
		try {
			e.ctl(opus::reset);
			e.ctl(opus::bitrate(opus_bitrate.get()));
			brtrack.reset();
			uint64_t ctime;
			{
				umutex_class m(time_mutex);
				ctime = current_time;
			}
			active_stream = NULL;
			active_stream = new opus_stream(ctime, current_collection->get_filesystem());
			int32_t pregap = e.ctl(opus::lookahead);
			active_stream->set_pregap(pregap);
		} catch(std::exception& e) {
			messages << "Can't start stream: " << e.what() << std::endl;
			return;
		}
		messages << "Tangent enaged." << std::endl;
	}

	void handle_tangent_negative_edge(opus_stream*& active_stream, bitrate_tracker& brtrack)
	{
		umutex_class m2(current_collection_lock);
		messages << "Tangent disenaged: " << brtrack;
		try {
			active_stream->write_trailier();
		} catch(std::exception& e) {
			messages << e.what() << std::endl;
		}
		if(current_collection) {
			try {
				current_collection->add_stream(*active_stream);
			} catch(std::exception& e) {
				messages << "Can't add stream: " << e.what() << std::endl;
				active_stream->put_ref();
			}
			notify_voice_stream_change();
		} else
			active_stream->put_ref();
		active_stream = NULL;
	}

	class inthread_th : public worker_thread
	{
	public:
		inthread_th()
		{
			quit = false;
			quit_ack = false;
			rptr = 0;
			fire();
		}
		void kill()
		{
			quit = true;
			{
				umutex_class h(lmut);
				lcond.notify_all();
			}
			while(!quit_ack)
				usleep(100000);
			usleep(100000);
		}
	protected:
		void entry()
		{
			try {
				entry2();
			} catch(std::bad_alloc& e) {
				OOM_panic();
			} catch(std::exception& e) {
				messages << "AIEEE... Fatal exception in voice thread: " << e.what() << std::endl;
			}
			quit_ack = true;
		}
		void entry2()
		{
			//Wait for libopus to load...
			size_t cbh = opus::add_callback([this]() {
				umutex_class h(this->lmut);
				this->lcond.notify_all();
			});
			while(true) {
				umutex_class h(lmut);
				if(opus::libopus_loaded() || quit)
					break;
				lcond.wait(h);
			}
			opus::cancel_callback(cbh);
			if(quit)
				return;

			int err;
			opus::encoder oenc(opus::samplerate::r48k, false, opus::application::voice);
			oenc.ctl(opus::bitrate(opus_bitrate.get()));
			audioapi_resampler rin;
			audioapi_resampler rout;
			const unsigned buf_max = 6144;	//These buffers better be large.
			size_t buf_in_use = 0;
			size_t buf_inr_use = 0;
			size_t buf_outr_use = 0;
			size_t buf_out_use = 0;
			float buf_in[buf_max];
			float buf_inr[OPUS_BLOCK_SIZE];
			float buf_outr[OUTPUT_SIZE];
			float buf_out[buf_max];
			bitrate_tracker brtrack;
			opus_stream* active_stream = NULL;

			drain_input();
			while(1) {
				if(clear_workflag(WORKFLAG_QUIT_REQUEST) & WORKFLAG_QUIT_REQUEST) {
					if(!active_flag && active_stream)
						handle_tangent_negative_edge(active_stream, brtrack);
					break;
				}
				uint64_t ticks = get_utime();
				//Handle tangent edgets.
				if(active_flag && !active_stream) {
					drain_input();
					buf_in_use = 0;
					buf_inr_use = 0;
					handle_tangent_positive_edge(oenc, active_stream, brtrack);
				}
				else if((!active_flag || quit) && active_stream)
					handle_tangent_negative_edge(active_stream, brtrack);
				if(quit)
					break;

				//Read input, up to 25ms.
				unsigned rate_in = audioapi_voice_rate().first;
				unsigned rate_out = audioapi_voice_rate().second;
				size_t dbuf_max = min(buf_max, rate_in / REC_THRESHOLD_DIV);
				read_input(buf_in, buf_in_use, dbuf_max);

				//Resample up to full opus block.
				do_resample(rin, buf_in, buf_in_use, buf_inr, buf_inr_use, OPUS_BLOCK_SIZE,
					1.0 * OPUS_SAMPLERATE / rate_in);

				//If we have full opus block and recording is enabled, compress it.
				if(buf_inr_use >= OPUS_BLOCK_SIZE && active_stream)
					compress_opus_block(oenc, buf_inr, buf_inr_use, *active_stream, brtrack);

				//Update time, starting/ending streams.
				update_time();

				//Decompress active streams.
				if(buf_outr_use < BLOCK_THRESHOLD)
					decompress_active_streams(buf_outr, buf_outr_use);

				//Resample to output rate.
				do_resample(rout, buf_outr, buf_outr_use, buf_out, buf_out_use, buf_max,
					1.0 * rate_out / OPUS_SAMPLERATE);

				//Output stuff.
				if(buf_out_use > 0 && audioapi_voice_p_status2() < rate_out / PLAY_THRESHOLD_DIV) {
					audioapi_play_voice(buf_out, buf_out_use);
					buf_out_use = 0;
				}

				//Sleep a bit to save CPU use.
				uint64_t ticks_spent = get_utime() - ticks;
				if(ticks_spent < ITERATION_TIME)
					usleep(ITERATION_TIME - ticks_spent);
			}
			delete current_collection;
		}
	private:
		size_t rptr;
		double position;
		volatile bool quit;
		volatile bool quit_ack;
		mutex_class lmut;
		cv_class lcond;
	};

	//The tangent function.
	command::fnptr<> ptangent(lsnes_cmd, "+tangent", "Voice tangent",
		"Syntax: +tangent\nVoice tangent.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			active_flag = true;
		});
	command::fnptr<> ntangent(lsnes_cmd, "-tangent", "Voice tangent",
		"Syntax: -tangent\nVoice tangent.\n",
		[]() throw(std::bad_alloc, std::runtime_error) {
			active_flag = false;
		});
	inverse_bind itangent(lsnes_mapper, "+tangent", "Movie‣Voice tangent");
	inthread_th* int_task;
}

//Rate is not sampling rate!
void voice_frame_number(uint64_t newframe, double rate)
{
	if(rate == last_rate && last_frame_number == newframe)
		return;
	umutex_class m(time_mutex);
	current_time = newframe / rate * OPUS_SAMPLERATE;
	if(fabs(rate - last_rate) > 1e-6 || last_frame_number + 1 != newframe)
		time_jump = true;
	last_frame_number = newframe;
	last_rate = rate;
}

void voicethread_task()
{
	int_task = new inthread_th;
}

void voicethread_kill()
{
	int_task->kill();
	int_task = NULL;
}

uint64_t voicesub_parse_timebase(const std::string& n)
{
	std::string x = n;
	if(x.length() > 0 && x[x.length() - 1] == 's') {
		x = x.substr(0, x.length() - 1);
		return 48000 * parse_value<double>(x);
	} else
		return parse_value<uint64_t>(x);
}

bool voicesub_collection_loaded()
{
	umutex_class m2(current_collection_lock);
	return (current_collection != NULL);
}

std::list<playback_stream_info> voicesub_get_stream_info()
{
	umutex_class m2(current_collection_lock);
	std::list<playback_stream_info> in;
	if(!current_collection)
		return in;
	for(auto i : current_collection->all_streams()) {
		opus_stream* s = current_collection->get_stream(i);
		playback_stream_info pi;
		if(!s)
			continue;
		pi.id = i;
		pi.base = s->timebase();
		pi.length = s->length();
		try {
			in.push_back(pi);
		} catch(...) {
		}
		s->put_ref();
	}
	return in;
}

void voicesub_play_stream(uint64_t id)
{
	umutex_class m2(current_collection_lock);
	if(!current_collection)
		throw std::runtime_error("No collection loaded");
	opus_stream* s = current_collection->get_stream(id);
	if(!s)
		return;
	try {
		start_management_stream(*s);
	} catch(...) {
		s->put_ref();
		throw;
	}
	s->put_ref();
}

void voicesub_export_stream(uint64_t id, const std::string& filename, external_stream_format fmt)
{
	umutex_class m2(current_collection_lock);
	if(!current_collection)
		throw std::runtime_error("No collection loaded");
	opus_stream* st = current_collection->get_stream(id);
	if(!st)
		return;
	std::ofstream s(filename, std::ios_base::out | std::ios_base::binary);
	if(!s) {
		st->put_ref();
		throw std::runtime_error("Can't open output file");
	}
	try {
		st->export_stream(s, fmt);
	} catch(std::exception& e) {
		st->put_ref();
		(stringfmt() << "Export failed: " << e.what()).throwex();
	}
	st->put_ref();
}

uint64_t voicesub_import_stream(uint64_t ts, const std::string& filename, external_stream_format fmt)
{
	umutex_class m2(current_collection_lock);
	if(!current_collection)
		throw std::runtime_error("No collection loaded");

	std::ifstream s(filename, std::ios_base::in | std::ios_base::binary);
	if(!s)
		throw std::runtime_error("Can't open input file");
	opus_stream* st = new opus_stream(ts, current_collection->get_filesystem(), s, fmt);
	uint64_t id;
	try {
		id = current_collection->add_stream(*st);
	} catch(...) {
		st->delete_stream();
		throw;
	}
	st->unlock();	//Not locked.
	notify_voice_stream_change();
	return id;
}

void voicesub_delete_stream(uint64_t id)
{
	umutex_class m2(current_collection_lock);
	if(!current_collection)
		throw std::runtime_error("No collection loaded");
	current_collection->delete_stream(id);
	notify_voice_stream_change();
}

void voicesub_export_superstream(const std::string& filename)
{
	umutex_class m2(current_collection_lock);
	if(!current_collection)
		throw std::runtime_error("No collection loaded");
	std::ofstream s(filename, std::ios_base::out | std::ios_base::binary);
	if(!s)
		throw std::runtime_error("Can't open output file");
	current_collection->export_superstream(s);
}

void voicesub_load_collection(const std::string& filename)
{
	umutex_class m2(current_collection_lock);
	filesystem_ref newfs;
	stream_collection* newc;
	newfs = filesystem_ref(filename);
	newc = new stream_collection(newfs);
	if(current_collection)
		delete current_collection;
	current_collection = newc;
	notify_voice_stream_change();
}

void voicesub_unload_collection()
{
	umutex_class m2(current_collection_lock);
	if(current_collection)
		delete current_collection;
	current_collection = NULL;
	notify_voice_stream_change();
}

void voicesub_alter_timebase(uint64_t id, uint64_t ts)
{
	umutex_class m2(current_collection_lock);
	if(!current_collection)
		throw std::runtime_error("No collection loaded");
	current_collection->alter_stream_timebase(id, ts);
	notify_voice_stream_change();
}

float voicesub_get_gain(uint64_t id)
{
	umutex_class m2(current_collection_lock);
	if(!current_collection)
		throw std::runtime_error("No collection loaded");
	return current_collection->get_stream(id)->get_gain() / 256.0;
}

void voicesub_set_gain(uint64_t id, float gain)
{
	umutex_class m2(current_collection_lock);
	if(!current_collection)
		throw std::runtime_error("No collection loaded");
	int64_t _gain = gain * 256;
	if(_gain < -32768 || _gain > 32767)
		throw std::runtime_error("Gain out of range (+-128dB)");
	current_collection->alter_stream_gain(id, _gain);
	notify_voice_stream_change();
}

double voicesub_ts_seconds(uint64_t ts)
{
	return ts / 48000.0;
}
