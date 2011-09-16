#include "lsnes.hpp"
#include "movie.hpp"
#include "rom.hpp"
#include "misc.hpp"
#include <stdexcept>
#include <cassert>
#include <cstring>
#include <fstream>

#define FLAG_SYNC CONTROL_FRAME_SYNC

std::string global_rerecord_count = "0";
//std::ofstream debuglog("movie-debugging-log", std::ios::out | std::ios::app);

namespace
{
	void hash_string(uint8_t* res, const std::string& s) throw(std::bad_alloc)
	{
		std::vector<char> t;
		t.resize(s.length());
		std::copy(s.begin(), s.end(), t.begin());
		sha256::hash(res, t);
	}

	uint8_t* enlarge(std::vector<uint8_t>& v, size_t amount) throw(std::bad_alloc)
	{
		size_t i = v.size();
		v.resize(i + amount);
		return &v[i];
	}

	inline void write64(uint8_t* buffer, uint64_t value) throw()
	{
		buffer[0] = value >> 56;
		buffer[1] = value >> 48;
		buffer[2] = value >> 40;
		buffer[3] = value >> 32;
		buffer[4] = value >> 24;
		buffer[5] = value >> 16;
		buffer[6] = value >> 8;
		buffer[7] = value;
	}

	inline void write32(uint8_t* buffer, uint32_t value) throw()
	{
		buffer[0] = value >> 24;
		buffer[1] = value >> 16;
		buffer[2] = value >> 8;
		buffer[3] = value;
	}

	inline uint32_t read32(const uint8_t* buffer) throw()
	{
		return (static_cast<uint32_t>(buffer[0]) << 24) |
			(static_cast<uint32_t>(buffer[1]) << 16) |
			(static_cast<uint32_t>(buffer[2]) << 8) |
			(static_cast<uint32_t>(buffer[3]));
	}

	inline uint64_t read64(const uint8_t* buffer) throw()
	{
		return (static_cast<uint64_t>(buffer[0]) << 56) |
			(static_cast<uint64_t>(buffer[1]) << 48) |
			(static_cast<uint64_t>(buffer[2]) << 40) |
			(static_cast<uint64_t>(buffer[3]) << 32) |
			(static_cast<uint64_t>(buffer[4]) << 24) |
			(static_cast<uint64_t>(buffer[5]) << 16) |
			(static_cast<uint64_t>(buffer[6]) << 8) |
			(static_cast<uint64_t>(buffer[7]));
	}

	inline void write16s(uint8_t* buffer, int16_t x) throw()
	{
		uint16_t y = static_cast<uint16_t>(x);
		buffer[0] = y >> 8;
		buffer[1] = y;
	}

	void hash_subframe(sha256& ctx, const controls_t& ctrl) throw()
	{
		uint8_t buf[2 * TOTAL_CONTROLS];
		for(unsigned i = 0; i < TOTAL_CONTROLS; i++)
			write16s(buf + 2 * i, ctrl(i));
		ctx.write(buf, 2 * TOTAL_CONTROLS);
	}

	//Hashes frame and returns starting subframe of next frame.
	uint64_t hash_frame(sha256& ctx, std::vector<controls_t>& input, uint64_t first_subframe,
		uint32_t bound) throw()
	{
		if(!bound) {
			//Ignore this frame completely.
			if(first_subframe >= input.size())
				return first_subframe;
			first_subframe++;
			while(first_subframe < input.size() && !input[first_subframe](CONTROL_FRAME_SYNC))
				first_subframe++;
			return first_subframe;
		}
		if(first_subframe >= input.size()) {
			//Hash an empty frame.
			hash_subframe(ctx, controls_t(true));
			return first_subframe;
		}

		uint64_t subframes_to_hash = 1;
		uint64_t last_differing = 1;
		uint64_t next;
		controls_t prev = input[first_subframe];
		prev(CONTROL_FRAME_SYNC) = 0;

		while(first_subframe + subframes_to_hash < input.size() && !input[first_subframe + subframes_to_hash]
			(CONTROL_FRAME_SYNC)) {
			if(!(input[first_subframe + subframes_to_hash] == prev))
				last_differing = subframes_to_hash + 1;
			prev = input[first_subframe + subframes_to_hash];
			subframes_to_hash++;
		}
		next = first_subframe + subframes_to_hash;
		subframes_to_hash = last_differing;
		for(uint64_t i = 0; i < subframes_to_hash && i < bound; i++)
			hash_subframe(ctx, input[first_subframe + i]);
		return next;
	}

	void hash_movie(uint8_t* res, uint64_t current_frame, uint32_t* pollcounters,
		std::vector<controls_t>& input) throw(std::bad_alloc)
	{
		sha256 ctx;
		//If current_frame == 0, hash is empty.
		if(!current_frame) {
			ctx.read(res);
			return;
		}
		//Hash past frames.
		uint64_t current_subframe = 0;
		for(uint64_t i = 1; i < current_frame; i++)
			current_subframe = hash_frame(ctx, input, current_subframe, 0x7FFFFFFF);
		//Current frame is special.
		for(size_t i = 0; i < TOTAL_CONTROLS; i++) {
			uint32_t polls = pollcounters[i] & 0x7FFFFFFF;
			uint32_t last_seen = 0;
			for(size_t j = 0; j < polls; j++) {
				if(current_subframe + j < input.size() && !input[current_subframe + j]
					(CONTROL_FRAME_SYNC))
					last_seen = input[current_subframe + j](i);
				uint8_t buf[2];
				write16s(buf, last_seen);
				ctx.write(buf, 2);
			}
		}
		ctx.read(res);
	}
}

void movie::set_all_DRDY() throw()
{
	for(size_t i = 0; i < TOTAL_CONTROLS; i++)
		pollcounters[i] |= 0x80000000UL;
}

std::string movie::rerecord_count() throw(std::bad_alloc)
{
	return rerecords;
}

void movie::rerecord_count(const std::string& count) throw(std::bad_alloc)
{
	rerecords = count;
}

std::string movie::project_id() throw(std::bad_alloc)
{
	return _project_id;
}

void movie::project_id(const std::string& id) throw(std::bad_alloc)
{
	_project_id = id;
}

bool movie::readonly_mode() throw()
{
	return readonly;
}

short movie::next_input(unsigned port, unsigned controller, unsigned index) throw(std::bad_alloc, std::logic_error)
{
	return next_input(ccindex2(port, controller, index));
}

void movie::set_controls(controls_t controls) throw()
{
	current_controls = controls;
}

uint32_t movie::count_changes(uint64_t first_subframe)
{
	if(first_subframe >= movie_data.size())
		return 0;
	uint32_t ret = 1;
	while(first_subframe + ret < movie_data.size() && !movie_data[first_subframe + ret](CONTROL_FRAME_SYNC))
		ret++;
	return ret;
}

controls_t movie::get_controls() throw()
{
	if(!readonly)
		return current_controls;
	controls_t c;
	//Before the beginning? Somebody screwed up (but return released / neutral anyway)...
	if(current_frame == 0)
		return c;
	//Otherwise find the last valid frame of input.
	uint32_t changes = count_changes(current_frame_first_subframe);
	if(!changes)
		return c;	//End of movie.
	for(size_t i = 0; i < TOTAL_CONTROLS; i++) {
		uint32_t polls = pollcounters[i] & 0x7FFFFFFF;
		uint32_t index = (changes > polls) ? polls : changes - 1;
		c(i) = movie_data[current_frame_first_subframe + index](i);
	}
	return c;
}

uint64_t movie::get_current_frame() throw()
{
	return current_frame;
}

uint64_t movie::get_lag_frames() throw()
{
	return lag_frames;
}

uint64_t movie::get_frame_count() throw()
{
	return frames_in_movie;
}

void movie::next_frame() throw(std::bad_alloc)
{
	//If all poll counters are zero for all real controls, this frame is lag.
	bool this_frame_lag = true;
	for(size_t i = MAX_SYSTEM_CONTROLS; i < TOTAL_CONTROLS; i++)
		if(pollcounters[i] & 0x7FFFFFFF)
			this_frame_lag = false;
	//Hack: Reset frames must not be considered lagged, so we abuse pollcounter bit for reset to mark those.
	if(pollcounters[CONTROL_SYSTEM_RESET] & 0x7FFFFFFF)
		this_frame_lag = false;
	//Oh, frame 0 must not be considered lag.
	if(current_frame && this_frame_lag) {
		lag_frames++;
		//debuglog << "Frame " << current_frame << " is lag" << std::endl << std::flush;
		if(!readonly) {
			//If in read-write mode, write a dummy record for the frame. Force sync flag.
			//As index should be movie_data.size(), it is correct afterwards.
			controls_t c = current_controls;
			c(CONTROL_FRAME_SYNC) = 1;
			movie_data.push_back(c);
			frames_in_movie++;
		}
	}

	//Reset the poll counters and DRDY flags.
	for(unsigned i = 0; i < TOTAL_CONTROLS; i++)
		pollcounters[i] = 0;

	//Increment the current frame counter and subframe counter. Note that first subframe is undefined for
	//frame 0 and 0 for frame 1.
	if(current_frame)
		current_frame_first_subframe = current_frame_first_subframe +
			count_changes(current_frame_first_subframe);
	else
		current_frame_first_subframe = 0;
	current_frame++;
}

bool movie::get_DRDY(unsigned controlindex) throw(std::logic_error)
{
	if(controlindex >= TOTAL_CONTROLS)
		throw std::logic_error("movie::get_DRDY: Bad index");
	return ((pollcounters[controlindex] & 0x80000000UL) != 0);
}

bool movie::get_DRDY(unsigned port, unsigned controller, unsigned index) throw(std::logic_error)
{
	return get_DRDY(ccindex2(port, controller, index));
}

short movie::next_input(unsigned controlindex) throw(std::bad_alloc, std::logic_error)
{
	//Check validity of index.
	if(controlindex == FLAG_SYNC)
		return 0;
	if(controlindex >= TOTAL_CONTROLS)
		throw std::logic_error("movie::next_input: Invalid control index");

	//Clear the DRDY flag.
	pollcounters[controlindex] &= 0x7FFFFFFF;

	if(readonly) {
		//In readonly mode...
		//If at the end of the movie, return released / neutral (but also record the poll)...
		if(current_frame_first_subframe >= movie_data.size()) {
			pollcounters[controlindex]++;
			return 0;
		}
		//Before the beginning? Somebody screwed up (but return released / neutral anyway)...
		if(current_frame == 0)
			return 0;
		//Otherwise find the last valid frame of input.
		uint32_t changes = count_changes(current_frame_first_subframe);
		uint32_t polls = (pollcounters[controlindex]++) & 0x7FFFFFFF;
		uint32_t index = (changes > polls) ? polls : changes - 1;
		//debuglog << "Frame=" << current_frame << " Subframe=" << polls << " control=" << controlindex << " value=" << movie_data[current_frame_first_subframe + index](controlindex) << " fetchrow=" << current_frame_first_subframe + index << std::endl << std::flush;
		return movie_data[current_frame_first_subframe + index](controlindex);
	} else {
		//Readwrite mode.
		//Before the beginning? Somebody screwed up (but return released / neutral anyway)...
		//Also, frame 0 must not be added to movie file.
		if(current_frame == 0)
			return 0;
		//If at movie end, insert complete input with frame sync set (this is the first subframe).
		if(current_frame_first_subframe >= movie_data.size()) {
			controls_t c = current_controls;
			c(CONTROL_FRAME_SYNC) = 1;
			movie_data.push_back(c);
			//current_frame_first_subframe should be movie_data.size(), so it is right.
			pollcounters[controlindex]++;
			frames_in_movie++;
			assert(pollcounters[controlindex] == 1);
			//debuglog << "Frame=" << current_frame << " Subframe=" << (pollcounters[controlindex] - 1) << " control=" << controlindex << " value=" << movie_data[current_frame_first_subframe](controlindex) << " fetchrow=" << current_frame_first_subframe << std::endl << std::flush;
			return movie_data[current_frame_first_subframe](controlindex);
		}
		short new_value = current_controls(controlindex);
		//Fortunately, we know this frame is the last one in movie_data.
		uint32_t pollcounter = pollcounters[controlindex] & 0x7FFFFFFF;
		uint64_t fetchrow = movie_data.size() - 1;
		if(current_frame_first_subframe + pollcounter < movie_data.size()) {
			//The index is within existing size. Change the value and propagate to all subsequent
			//subframes.
			for(uint64_t i = current_frame_first_subframe + pollcounter; i < movie_data.size(); i++)
				movie_data[i](controlindex) = new_value;
			fetchrow = current_frame_first_subframe + pollcounter;
		} else if(new_value != movie_data[movie_data.size() - 1](controlindex)) {
			//The index is not within existing size and value does not match. We need to create a new
			//subframes(s), copying the last subframe.
			while(current_frame_first_subframe + pollcounter >= movie_data.size()) {
				controls_t c = movie_data[movie_data.size() - 1];
				c(CONTROL_FRAME_SYNC) = 0;
				movie_data.push_back(c);
			}
			fetchrow = current_frame_first_subframe + pollcounter;
			movie_data[current_frame_first_subframe + pollcounter](controlindex) = new_value;
		}
		pollcounters[controlindex]++;
		//debuglog << "Frame=" << current_frame << " Subframe=" << (pollcounters[controlindex] - 1) << " control=" << controlindex << " value=" << new_value << " fetchrow=" << fetchrow << std::endl << std::flush;
		return new_value;
	}
}

movie::movie() throw(std::bad_alloc)
{
	readonly = false;
	rerecords = "0";
	_project_id = "";
	current_frame = 0;
	frames_in_movie = 0;
	current_frame_first_subframe = 0;
	for(unsigned i = 0; i < TOTAL_CONTROLS; i++) {
		pollcounters[i] = 0;
		current_controls(i) = 0;
	}
	lag_frames = 0;
	clear_caches();
}

void movie::load(const std::string& rerecs, const std::string& project_id, const std::vector<controls_t>& input)
	throw(std::bad_alloc, std::runtime_error)
{
	if(input.size() > 0 && !input[0](CONTROL_FRAME_SYNC))
		throw std::runtime_error("First subframe MUST have frame sync flag set");
	clear_caches();
	frames_in_movie = 0;
	for(auto i = input.begin(); i != input.end(); i++)
		if((*i)(CONTROL_FRAME_SYNC))
			frames_in_movie++;
	readonly = true;
	rerecords = rerecs;
	_project_id = project_id;
	current_frame = 0;
	current_frame_first_subframe = 0;
	for(unsigned i = 0; i < TOTAL_CONTROLS; i++) {
		pollcounters[i] = 0;
	}
	lag_frames = 0;
	movie_data = input;
}

std::vector<controls_t> movie::save() throw(std::bad_alloc)
{
	return movie_data;
}

void movie::commit_reset(long delay) throw(std::bad_alloc)
{
	if(readonly || delay < 0)
		return;
	//If this frame is lagged, we need to write entry for it.
	bool this_frame_lag = true;
	for(size_t i = MAX_SYSTEM_CONTROLS; i < TOTAL_CONTROLS; i++)
		if(pollcounters[i] & 0x7FFFFFFF)
			this_frame_lag = false;
	//Hack: Reset frames must not be considered lagged, so we abuse pollcounter bit for reset to mark those.
	if(pollcounters[CONTROL_SYSTEM_RESET] & 0x7FFFFFFF)
		this_frame_lag = false;
	if(this_frame_lag) {
		controls_t c = current_controls;
		c(CONTROL_FRAME_SYNC) = 1;
		movie_data.push_back(c);
		frames_in_movie++;
		//Current_frame_first_subframe is correct.
	}
	//Also set poll counters on reset cycles to avoid special cases elsewhere.
	pollcounters[CONTROL_SYSTEM_RESET] = 1;
	pollcounters[CONTROL_SYSTEM_RESET_CYCLES_HI] = 1;
	pollcounters[CONTROL_SYSTEM_RESET_CYCLES_LO] = 1;
	//Current frame is always last in rw mode.
	movie_data[current_frame_first_subframe](CONTROL_SYSTEM_RESET) = 1;
	movie_data[current_frame_first_subframe](CONTROL_SYSTEM_RESET_CYCLES_HI) = delay / 10000;
	movie_data[current_frame_first_subframe](CONTROL_SYSTEM_RESET_CYCLES_LO) = delay % 10000;
}

unsigned movie::next_poll_number()
{
	unsigned max = 0;
	for(unsigned i = 0; i < TOTAL_CONTROLS; i++)
		if(max < (pollcounters[i] & 0x7FFFFFFF))
			max = (pollcounters[i] & 0x7FFFFFFF);
	return max + 1;
}

void movie::readonly_mode(bool enable) throw(std::bad_alloc)
{
	bool was_in_readonly = readonly;
	readonly = enable;
	if(was_in_readonly && !readonly) {
		clear_caches();
		//Transitioning to readwrite mode, we have to adjust the length of the movie data.
		if(current_frame == 0) {
			//WTF... At before first frame. Blank the entiere movie.
			movie_data.clear();
			return;
		}
		//Fun special case: Current frame is not in movie (current_frame_first_subframe >= movie_data.size()).
		//In this case, we have to extend the movie data.
		if(current_frame_first_subframe >= movie_data.size()) {
			//Yes, this will insert one extra frame... But we will lose it later if it is not needed.
			while(frames_in_movie < current_frame) {
				controls_t c(true);
				movie_data.push_back(c);
				frames_in_movie++;
			}
			current_frame_first_subframe = movie_data.size() - 1;
		}

		//We have to take the part up to furthest currently readable subframe. Also, we need to propagate
		//forward values with smaller poll counters.
		uint64_t next_frame_first_subframe = current_frame_first_subframe +
			count_changes(current_frame_first_subframe);
		uint64_t max_readable_subframes = current_frame_first_subframe;
		for(size_t i = 0; i < TOTAL_CONTROLS; i++) {
			uint32_t polls = pollcounters[i] & 0x7FFFFFFF;
			if(current_frame_first_subframe + polls >= next_frame_first_subframe)
				max_readable_subframes = next_frame_first_subframe;
			else if(current_frame_first_subframe + polls > max_readable_subframes)
				max_readable_subframes = current_frame_first_subframe + polls;
		}
		movie_data.resize(max_readable_subframes);
		next_frame_first_subframe = max_readable_subframes;
		for(size_t i = 1; i < TOTAL_CONTROLS; i++) {
			uint32_t polls = pollcounters[i] & 0x7FFFFFFF;
			if(!polls)
				polls = 1;
			for(uint64_t j = current_frame_first_subframe + polls; j < next_frame_first_subframe; j++)
				movie_data[j](i) = movie_data[current_frame_first_subframe + polls - 1](i);
		}
		frames_in_movie = current_frame - ((current_frame_first_subframe >= movie_data.size()) ? 1 : 0);
	}
}

//Save state of movie code.
std::vector<uint8_t> movie::save_state() throw(std::bad_alloc)
{
	//debuglog << "--------------------------------------------" << std::endl;
	//debuglog << "SAVING STATE:" << std::endl;
	std::vector<uint8_t> ret;
	hash_string(enlarge(ret, 32), _project_id);
	write64(enlarge(ret, 8), current_frame);
	//debuglog << "Current frame is " << current_frame << std::endl;
	//debuglog << "Poll counters: ";
	for(unsigned i = 0; i < TOTAL_CONTROLS; i++) {
		uint32_t v = pollcounters[i];
		//debuglog << v;
		if(v & 0x80000000UL) {
			//debuglog << "R ";
		} else
			;//debuglog << " ";
		write32(enlarge(ret, 4), v);
	}
	//debuglog << std::endl;
	{
		uint64_t v = lag_frames;
		//debuglog << "Lag frame count: " << lag_frames << std::endl;
		write64(enlarge(ret, 8), v);
	}
	hash_movie(enlarge(ret, 32), current_frame, pollcounters, movie_data);
	uint8_t hash[32];
	sha256::hash(hash, ret);
	memcpy(enlarge(ret, 32), hash, 32);
	//debuglog << "--------------------------------------------" << std::endl;
	//debuglog.flush();
	return ret;
}

//Restore state of movie code. Throws if state is invalid. Flag gives new state of readonly flag.
size_t movie::restore_state(const std::vector<uint8_t>& state, bool ro) throw(std::bad_alloc, std::runtime_error)
{
	//Check the whole-data checksum.
	size_t ptr = 0;
	uint8_t tmp[32];
	if(state.size() != 112+4*TOTAL_CONTROLS)
		throw std::runtime_error("Movie save data corrupt: Wrong length");
	sha256::hash(tmp, &state[0], state.size() - 32);
	if(memcmp(tmp, &state[state.size() - 32], 32))
		throw std::runtime_error("Movie save data corrupt: Checksum does not match");
	//debuglog << "--------------------------------------------" << std::endl;
	//debuglog << "RESTORING STATE:" << std::endl;
	//Check project id.
	hash_string(tmp, _project_id);
	if(memcmp(tmp, &state[ptr], 32))
		throw std::runtime_error("Save is not from this movie");
	ptr += 32;
	//Read current frame.
	uint64_t tmp_curframe = read64(&state[ptr]);
	uint64_t tmp_firstsubframe = 0;
	for(uint64_t i = 1; i < tmp_curframe; i++)
		tmp_firstsubframe = tmp_firstsubframe + count_changes(tmp_firstsubframe);
	ptr += 8;
	//Read poll counters and drdy flags.
	uint32_t tmp_pollcount[TOTAL_CONTROLS];
	for(unsigned i = 0; i < TOTAL_CONTROLS; i++) {
		uint32_t v = read32(&state[ptr]);
		ptr += 4;
		tmp_pollcount[i] = v;
	}
	uint64_t tmp_lagframes = read64(&state[ptr]);
	tmp_lagframes &= 0x7FFFFFFFFFFFFFFFULL;
	ptr += 8;
	hash_movie(tmp, tmp_curframe, tmp_pollcount, movie_data);
	if(memcmp(tmp, &state[ptr], 32))
		throw std::runtime_error("Save is not from this movie");

	//Ok, all checks pass. Copy the state. Do this in readonly mode so we can use normal routine to switch
	//to readwrite mode.
	readonly = true;
	current_frame = tmp_curframe;
	current_frame_first_subframe = tmp_firstsubframe;
	memcpy(pollcounters, tmp_pollcount, sizeof(tmp_pollcount));
	lag_frames = tmp_lagframes;

	//debuglog << "Current frame is " << current_frame << std::endl;
	//debuglog << "Poll counters: ";
	for(unsigned i = 0; i < TOTAL_CONTROLS; i++) {
		uint32_t v = pollcounters[i];
		//debuglog << v;
		if(v & 0x80000000UL) {
			//debuglog << "R ";
		} else
			;//debuglog << " ";
	}
	//debuglog << std::endl;
	{
		//debuglog << "Lag frame count: " << lag_frames << std::endl;
	}

	//debuglog << "--------------------------------------------" << std::endl;
	//debuglog.flush();

	//Move to readwrite mode if needed.
	readonly_mode(ro);
	return 0;
}

long movie::get_reset_status() throw()
{
	if(current_frame == 0 || current_frame_first_subframe >= movie_data.size())
		return -1;	//No resets out of range.
	if(!movie_data[current_frame_first_subframe](CONTROL_SYSTEM_RESET))
		return -1;	//Not a reset.
	//Also set poll counters on reset cycles to avoid special cases elsewhere.
	pollcounters[CONTROL_SYSTEM_RESET] = 1;
	pollcounters[CONTROL_SYSTEM_RESET_CYCLES_HI] = 1;
	pollcounters[CONTROL_SYSTEM_RESET_CYCLES_LO] = 1;
	long hi = movie_data[current_frame_first_subframe](CONTROL_SYSTEM_RESET_CYCLES_HI);
	long lo = movie_data[current_frame_first_subframe](CONTROL_SYSTEM_RESET_CYCLES_LO);
	return hi * 10000 + lo;
}

uint64_t movie::frame_subframes(uint64_t frame)
{
	if(frame < cached_frame)
		clear_caches();
	uint64_t p = cached_subframe;
	for(uint64_t i = cached_frame; i < frame; i++)
		p = p + count_changes(p);
	cached_frame = frame;
	cached_subframe = p;
	return count_changes(p);
}

void movie::clear_caches()
{
	cached_frame = 1;
	cached_subframe = 0;
}

controls_t movie::read_subframe(uint64_t frame, uint64_t subframe)
{
	if(frame < cached_frame)
		clear_caches();
	uint64_t p = cached_subframe;
	for(uint64_t i = cached_frame; i < frame; i++)
		p = p + count_changes(p);
	cached_frame = frame;
	cached_subframe = p;
	uint64_t max = count_changes(p);
	if(!max)
		return controls_t(true);
	if(max <= subframe)
		subframe = max - 1;
	return movie_data[p + subframe];
}


movie_logic::movie_logic() throw()
{
}

movie& movie_logic::get_movie() throw()
{
	return mov;
}

long movie_logic::new_frame_starting(bool dont_poll) throw(std::bad_alloc, std::runtime_error)
{
	mov.next_frame();
	controls_t c = update_controls(false);
	if(!mov.readonly_mode()) {
		mov.set_controls(c);
		if(dont_poll)
			mov.set_all_DRDY();
		if(c(CONTROL_SYSTEM_RESET)) {
			long hi = c(CONTROL_SYSTEM_RESET_CYCLES_HI);
			long lo = c(CONTROL_SYSTEM_RESET_CYCLES_LO);
			mov.commit_reset(hi * 10000 + lo);
		}
	}
	return mov.get_reset_status();
}

short movie_logic::input_poll(bool port, unsigned dev, unsigned id) throw(std::bad_alloc, std::runtime_error)
{
	if(dev >= MAX_CONTROLLERS_PER_PORT || id >= CONTROLLER_CONTROLS)
		return 0;
	if(!mov.get_DRDY(port ? 1 : 0, dev, id)) {
		mov.set_controls(update_controls(true));
		mov.set_all_DRDY();
	}
	int16_t in = mov.next_input(port ? 1 : 0, dev, id);
	//debuglog << "BSNES asking for (" << port << "," << dev << "," << id << ") (frame " << mov.get_current_frame()
	//	<< ") giving " << in << std::endl;
	//debuglog.flush();
	return in;
}
