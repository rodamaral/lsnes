#include "lsnes.hpp"

#include "core/misc.hpp"
#include "core/movie.hpp"
#include "core/rom.hpp"

#include <stdexcept>
#include <cassert>
#include <cstring>
#include <fstream>

#define FLAG_SYNC CONTROL_FRAME_SYNC

//std::ofstream debuglog("movie-debugging-log", std::ios::out | std::ios::app);

namespace
{
	bool movies_compatible(const std::vector<controls_t>& old_movie, const std::vector<controls_t>& new_movie,
		uint64_t frame, const uint32_t* polls, const std::string& old_projectid,
		const std::string& new_projectid)
	{
		//Project IDs have to match.
		if(old_projectid != new_projectid)
			return false;
		//If new movie is before first frame, anything with same project_id is compatible.
		if(frame == 0)
			return true;
		//Scan both movies until frame syncs are seen. Out of bounds reads behave as all neutral but frame
		//sync done.
		uint64_t syncs_seen = 0;
		uint64_t frames_read = 0;
		while(syncs_seen < frame) {
			controls_t oldc(true), newc(true);
			//Due to way subframes are stored, we can ignore syncing when comparing.
			if(frames_read < old_movie.size())
				oldc = old_movie[frames_read];
			if(frames_read < new_movie.size())
				newc = new_movie[frames_read];
			if(memcmp(oldc.controls, newc.controls, sizeof(oldc.controls)))
				return false;	//Mismatch.
			frames_read++;
			if(newc(CONTROL_FRAME_SYNC))
				syncs_seen++;
		}
		//We increment the counter one time too many.
		frames_read--;
		//Current frame. We need to compare each control up to poll counter.
		uint64_t readable_old_subframes = 0, readable_new_subframes = 0;
		if(frames_read < old_movie.size())
			readable_old_subframes = old_movie.size() - frames_read;
		if(frames_read < new_movie.size())
			readable_new_subframes = new_movie.size() - frames_read;
		for(unsigned i = 0; i < TOTAL_CONTROLS; i++) {
			uint32_t p = polls[i] & 0x7FFFFFFFUL;
			short ov = 0, nv = 0;
			for(uint32_t i = 0; i < p; i++) {
				if(i < readable_old_subframes)
					ov = old_movie[i + frames_read](i);
				if(i < readable_new_subframes)
					nv = new_movie[i + frames_read](i);
				if(ov != nv)
					return false;
			}
		}
		return true;
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

uint32_t movie::count_changes(uint64_t first_subframe) throw()
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
	for(auto i : input)
		if(i(CONTROL_FRAME_SYNC))
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
			frames_in_movie = 0;
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
void movie::save_state(std::string& proj_id, uint64_t& curframe, uint64_t& lagframes, std::vector<uint32_t>& pcounters)
	throw(std::bad_alloc)
{
	pcounters.resize(TOTAL_CONTROLS);
	proj_id = _project_id;
	curframe = current_frame;
	lagframes = lag_frames;
	memcpy(&pcounters[0], pollcounters, TOTAL_CONTROLS * sizeof(uint32_t));
}

//Restore state of movie code. Throws if state is invalid. Flag gives new state of readonly flag.
size_t movie::restore_state(uint64_t curframe, uint64_t lagframe, const std::vector<uint32_t>& pcounters, bool ro,
	std::vector<controls_t>* old_movie, const std::string& old_projectid) throw(std::bad_alloc,
	std::runtime_error)
{
	if(pcounters.size() != TOTAL_CONTROLS)
		throw std::runtime_error("Wrong number of poll counters");
	if(old_movie && !movies_compatible(*old_movie, movie_data, curframe, &pcounters[0], old_projectid,
		_project_id))
		throw std::runtime_error("Save is not from this movie");
	uint64_t tmp_firstsubframe = 0;
	for(uint64_t i = 1; i < curframe; i++)
		tmp_firstsubframe = tmp_firstsubframe + count_changes(tmp_firstsubframe);
	//Checks have passed, copy the data.
	readonly = true;
	current_frame = curframe;
	current_frame_first_subframe = tmp_firstsubframe;
	lag_frames = lagframe;
	memcpy(pollcounters, &pcounters[0], TOTAL_CONTROLS * sizeof(uint32_t));
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

uint64_t movie::frame_subframes(uint64_t frame) throw()
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

void movie::clear_caches() throw()
{
	cached_frame = 1;
	cached_subframe = 0;
}

controls_t movie::read_subframe(uint64_t frame, uint64_t subframe) throw()
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
