#include "lsnes.hpp"

#include "core/emucore.hpp"
#include "core/misc.hpp"
#include "core/movie.hpp"
#include "core/rom.hpp"

#include <stdexcept>
#include <cassert>
#include <cstring>
#include <fstream>


//std::ofstream debuglog("movie-debugging-log", std::ios::out | std::ios::app);

namespace
{
	uint64_t find_next_sync(controller_frame_vector& movie, uint64_t after)
	{
		if(after >= movie.size())
			return after;
		do {
			after++;
		} while(after < movie.size() && !movie[after].sync());
		return after;
	}

	bool movies_compatible(controller_frame_vector& old_movie, controller_frame_vector& new_movie,
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
		while(syncs_seen < frame - 1) {
			controller_frame oldc = old_movie.blank_frame(true), newc = new_movie.blank_frame(true);
			if(frames_read < old_movie.size())
				oldc = old_movie[frames_read];
			if(frames_read < new_movie.size())
				newc = new_movie[frames_read];
			if(oldc != newc)
				return false;	//Mismatch.
			frames_read++;
			if(newc.sync())
				syncs_seen++;
		}
		//We increment the counter one time too many.
		frames_read--;
		//Current frame. We need to compare each control up to poll counter.
		uint64_t readable_old_subframes = 0, readable_new_subframes = 0;
		uint64_t oldlen = find_next_sync(old_movie, frames_read);
		uint64_t newlen = find_next_sync(new_movie, frames_read);
		if(frames_read < oldlen)
			readable_old_subframes = oldlen - frames_read;
		if(frames_read < newlen)
			readable_new_subframes = newlen - frames_read;
		//Compare reset flags of current frame.
		bool old_reset = false;
		bool new_reset = false;
		std::pair<short, short> old_delay = std::make_pair(0, 0);
		std::pair<short, short> new_delay = std::make_pair(0, 0);
		if(readable_old_subframes) {
			old_reset = old_movie[frames_read].reset();
			old_delay = old_movie[frames_read].delay();
		}
		if(readable_new_subframes) {
			new_reset = new_movie[frames_read].reset();
			new_delay = new_movie[frames_read].delay();
		}
		if(old_reset != new_reset || old_delay != new_delay)
			return false;
		//Then rest of the stuff.
		for(unsigned i = 0; i < MAX_BUTTONS; i++) {
			uint32_t p = polls[i + 4] & 0x7FFFFFFFUL;
			short ov = 0, nv = 0;
			for(uint32_t j = 0; j < p; j++) {
				if(j < readable_old_subframes)
					ov = old_movie[j + frames_read].axis2(i);
				if(j < readable_new_subframes)
					nv = new_movie[j + frames_read].axis2(i);
				if(ov != nv)
					return false;
			}
		}
		return true;
	}
}

void movie::set_all_DRDY() throw()
{
	pollcounters.set_all_DRDY();
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

void movie::set_controls(controller_frame controls) throw()
{
	current_controls = controls;
}

uint32_t movie::count_changes(uint64_t first_subframe) throw()
{
	return movie_data.subframe_count(first_subframe);
}

controller_frame movie::get_controls() throw()
{
	if(!readonly)
		return current_controls;
	controller_frame c = movie_data.blank_frame(false);
	//Before the beginning? Somebody screwed up (but return released / neutral anyway)...
	if(current_frame == 0)
		return c;
	//Otherwise find the last valid frame of input.
	uint32_t changes = count_changes(current_frame_first_subframe);
	if(!changes)
		return c;	//End of movie.
	if(pollcounters.get_system()) {
		c.reset(movie_data[current_frame_first_subframe].reset());
		c.delay(movie_data[current_frame_first_subframe].delay());
	}
	for(size_t i = 0; i < MAX_BUTTONS; i++) {
		uint32_t polls = pollcounters.get_polls(i);
		uint32_t index = (changes > polls) ? polls : changes - 1;
		c.axis2(i, movie_data[current_frame_first_subframe + index].axis2(i));
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
	//Adjust lag count. Frame 0 MUST NOT be considered lag.
	unsigned pflag = core_get_poll_flag();
	if(current_frame && pflag == 0)
		lag_frames++;
	else if(pflag < 2)
		core_set_poll_flag(0);

	//If all poll counters are zero for all real controls, this frame is lag.
	bool this_frame_lag = !pollcounters.has_polled();
	//Oh, frame 0 must not be considered lag.
	if(current_frame && this_frame_lag) {
		if(pflag == 2)
			lag_frames++;	//Legacy compat. behaviour.
		//debuglog << "Frame " << current_frame << " is lag" << std::endl << std::flush;
		if(!readonly) {
			//If in read-write mode, write a dummy record for the frame. Force sync flag.
			//As index should be movie_data.size(), it is correct afterwards.
			movie_data.append(current_controls.copy(true));
			frames_in_movie++;
		}
	}

	//Reset the poll counters and DRDY flags.
	pollcounters.clear();

	//Increment the current frame counter and subframe counter. Note that first subframe is undefined for
	//frame 0 and 0 for frame 1.
	if(current_frame)
		current_frame_first_subframe = current_frame_first_subframe +
			count_changes(current_frame_first_subframe);
	else
		current_frame_first_subframe = 0;
	current_frame++;
}

bool movie::get_DRDY(unsigned pid, unsigned ctrl) throw(std::logic_error)
{
	return pollcounters.get_DRDY(pid, ctrl);
}

short movie::next_input(unsigned pid, unsigned ctrl) throw(std::bad_alloc, std::logic_error)
{
	pollcounters.clear_DRDY(pid, ctrl);

	if(readonly) {
		//In readonly mode...
		//If at the end of the movie, return released / neutral (but also record the poll)...
		if(current_frame_first_subframe >= movie_data.size()) {
			pollcounters.increment_polls(pid, ctrl);
			return 0;
		}
		//Before the beginning? Somebody screwed up (but return released / neutral anyway)...
		if(current_frame == 0)
			return 0;
		//Otherwise find the last valid frame of input.
		uint32_t changes = count_changes(current_frame_first_subframe);
		uint32_t polls = pollcounters.increment_polls(pid, ctrl);
		uint32_t index = (changes > polls) ? polls : changes - 1;
		//debuglog << "Frame=" << current_frame << " Subframe=" << polls << " control=" << controlindex << " value=" << movie_data[current_frame_first_subframe + index](controlindex) << " fetchrow=" << current_frame_first_subframe + index << std::endl << std::flush;
		return movie_data[current_frame_first_subframe + index].axis(pid, ctrl);
	} else {
		//Readwrite mode.
		//Before the beginning? Somebody screwed up (but return released / neutral anyway)...
		//Also, frame 0 must not be added to movie file.
		if(current_frame == 0)
			return 0;
		//If at movie end, insert complete input with frame sync set (this is the first subframe).
		if(current_frame_first_subframe >= movie_data.size()) {
			movie_data.append(current_controls.copy(true));
			//current_frame_first_subframe should be movie_data.size(), so it is right.
			pollcounters.increment_polls(pid, ctrl);
			frames_in_movie++;
			//debuglog << "Frame=" << current_frame << " Subframe=" << (pollcounters[controlindex] - 1) << " control=" << controlindex << " value=" << movie_data[current_frame_first_subframe](controlindex) << " fetchrow=" << current_frame_first_subframe << std::endl << std::flush;
			return movie_data[current_frame_first_subframe].axis(pid, ctrl);
		}
		short new_value = current_controls.axis(pid, ctrl);
		//Fortunately, we know this frame is the last one in movie_data.
		uint32_t pollcounter = pollcounters.get_polls(pid, ctrl);
		uint64_t fetchrow = movie_data.size() - 1;
		if(current_frame_first_subframe + pollcounter < movie_data.size()) {
			//The index is within existing size. Change the value and propagate to all subsequent
			//subframes.
			for(uint64_t i = current_frame_first_subframe + pollcounter; i < movie_data.size(); i++)
				movie_data[i].axis(pid, ctrl, new_value);
			fetchrow = current_frame_first_subframe + pollcounter;
		} else if(new_value != movie_data[movie_data.size() - 1].axis(pid, ctrl)) {
			//The index is not within existing size and value does not match. We need to create a new
			//subframes(s), copying the last subframe.
			while(current_frame_first_subframe + pollcounter >= movie_data.size())
				movie_data.append(movie_data[movie_data.size() - 1].copy(false));
			fetchrow = current_frame_first_subframe + pollcounter;
			movie_data[current_frame_first_subframe + pollcounter].axis(pid, ctrl, new_value);
		}
		pollcounters.increment_polls(pid, ctrl);
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
	lag_frames = 0;
	clear_caches();
}

void movie::load(const std::string& rerecs, const std::string& project_id, controller_frame_vector& input)
	throw(std::bad_alloc, std::runtime_error)
{
	if(input.size() > 0 && !input[0].sync())
		throw std::runtime_error("First subframe MUST have frame sync flag set");
	clear_caches();
	frames_in_movie = 0;
	for(size_t i = 0; i < input.size(); i++)
		if(input[i].sync())
			frames_in_movie++;
	readonly = true;
	rerecords = rerecs;
	_project_id = project_id;
	current_frame = 0;
	current_frame_first_subframe = 0;
	pollcounters.clear();
	lag_frames = 0;
	movie_data = input;
	//This is to force internal type of current_controls to become correct.
	current_controls = input.blank_frame(false);
}

controller_frame_vector movie::save() throw(std::bad_alloc)
{
	return movie_data;
}

void movie::commit_reset(long delay) throw(std::bad_alloc)
{
	if(readonly || delay < 0)
		return;
	//If this frame is lagged, we need to write entry for it.
	if(!pollcounters.has_polled()) {
		movie_data.append(current_controls.copy(true));
		frames_in_movie++;
		//Current_frame_first_subframe is correct.
	}
	//Also set poll counters on reset cycles to avoid special cases elsewhere.
	pollcounters.set_system();
	//Current frame is always last in rw mode.
	movie_data[current_frame_first_subframe].reset(true);
	movie_data[current_frame_first_subframe].delay(std::make_pair(delay / 10000, delay % 10000));
}

unsigned movie::next_poll_number()
{
	return pollcounters.max_polls() + 1;
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
				movie_data.append(movie_data.blank_frame(true));
				frames_in_movie++;
			}
			current_frame_first_subframe = movie_data.size() - 1;
		}

		//We have to take the part up to furthest currently readable subframe. Also, we need to propagate
		//forward values with smaller poll counters.
		uint64_t next_frame_first_subframe = current_frame_first_subframe +
			count_changes(current_frame_first_subframe);
		uint64_t max_readable_subframes = current_frame_first_subframe + pollcounters.max_polls();
		if(max_readable_subframes > next_frame_first_subframe)
			max_readable_subframes = next_frame_first_subframe;

		movie_data.resize(max_readable_subframes);
		next_frame_first_subframe = max_readable_subframes;
		//Propagate RESET. This always has poll count of 0 or 1, which always behaves like 1.
		for(uint64_t j = current_frame_first_subframe + 1; j < next_frame_first_subframe; j++) {
			movie_data[j].reset(movie_data[current_frame_first_subframe].reset());
			movie_data[j].delay(movie_data[current_frame_first_subframe].delay());
		}
		//Then the other buttons.
		for(size_t i = 0; i < MAX_BUTTONS; i++) {
			uint32_t polls = pollcounters.get_polls(i);
			polls = polls ? polls : 1;
			for(uint64_t j = current_frame_first_subframe + polls; j < next_frame_first_subframe; j++)
				movie_data[j].axis2(i, movie_data[current_frame_first_subframe + polls - 1].axis2(i));
		}
		frames_in_movie = current_frame - ((current_frame_first_subframe >= movie_data.size()) ? 1 : 0);
	}
}

//Save state of movie code.
void movie::save_state(std::string& proj_id, uint64_t& curframe, uint64_t& lagframes, std::vector<uint32_t>& pcounters)
	throw(std::bad_alloc)
{
	pollcounters.save_state(pcounters);
	proj_id = _project_id;
	curframe = current_frame;
	lagframes = lag_frames;
}

//Restore state of movie code. Throws if state is invalid. Flag gives new state of readonly flag.
size_t movie::restore_state(uint64_t curframe, uint64_t lagframe, const std::vector<uint32_t>& pcounters, bool ro,
	controller_frame_vector* old_movie, const std::string& old_projectid) throw(std::bad_alloc,
	std::runtime_error)
{
	if(!pollcounters.check(pcounters))
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
	pollcounters.load_state(pcounters);
	readonly_mode(ro);
	return 0;
}

long movie::get_reset_status() throw()
{
	if(current_frame == 0 || current_frame_first_subframe >= movie_data.size())
		return -1;	//No resets out of range.
	if(!movie_data[current_frame_first_subframe].reset())
		return -1;	//Not a reset.
	//Also set poll counters on reset cycles to avoid special cases elsewhere.
	pollcounters.set_system();
	auto g = movie_data[current_frame_first_subframe].delay();
	long hi = g.first, lo = g.second;
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

controller_frame movie::read_subframe(uint64_t frame, uint64_t subframe) throw()
{
	if(frame < cached_frame)
		clear_caches();
	uint64_t p = cached_subframe;
	for(uint64_t i = cached_frame; i < frame; i++)
		p = p + count_changes(p);
	cached_frame = frame;
	cached_subframe = p;
	uint64_t max = count_changes(p);
	if(!max) {
		return movie_data.blank_frame(true);
	}
	if(max <= subframe)
		subframe = max - 1;
	return movie_data[p + subframe];
}

void movie::reset_state() throw()
{
	readonly = true;
	current_frame = 0;
	current_frame_first_subframe = 0;
	pollcounters.clear();
	lag_frames = 0;
	clear_caches();
}

void movie::fast_save(uint64_t& _frame, uint64_t& _ptr, uint64_t& _lagc, std::vector<uint32_t>& _counters)
{
	pollcounters.save_state(_counters);
	_frame = current_frame;
	_ptr = current_frame_first_subframe;
	_lagc = lag_frames;
}

void movie::fast_load(uint64_t& _frame, uint64_t& _ptr, uint64_t& _lagc, std::vector<uint32_t>& _counters)
{
	readonly = true;
	current_frame = _frame;
	current_frame_first_subframe = (_ptr <= movie_data.size()) ? _ptr : movie_data.size();
	lag_frames = _lagc;
	pollcounters.load_state(_counters);
	readonly_mode(false);
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
	controller_frame c = update_controls(false);
	if(!mov.readonly_mode()) {
		mov.set_controls(c);
		if(!dont_poll)
			mov.set_all_DRDY();
		if(c.reset()) {
			auto g = c.delay();
			long hi = g.first, lo = g.second;
			mov.commit_reset(hi * 10000 + lo);
		}
	} else if(!dont_poll)
		mov.set_all_DRDY();
	return mov.get_reset_status();
}

short movie_logic::input_poll(bool port, unsigned dev, unsigned id) throw(std::bad_alloc, std::runtime_error)
{
	unsigned pid = port ? (dev + MAX_CONTROLLERS_PER_PORT) : dev;
	if(!mov.get_DRDY(pid, id)) {
		mov.set_controls(update_controls(true));
		mov.set_all_DRDY();
	}
	int16_t in = mov.next_input(pid, id);
	//debuglog << "BSNES asking for (" << port << "," << dev << "," << id << ") (frame " << mov.get_current_frame()
	//	<< ") giving " << in << std::endl;
	//debuglog.flush();
	return in;
}

unsigned extended_mode = 0;