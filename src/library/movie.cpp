#include "movie.hpp"

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
		//Types have to match.
		if(old_movie.get_types() != new_movie.get_types())
			return false;
		const port_type_set& pset = new_movie.get_types();
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
		//Then rest of the stuff.
		for(unsigned i = 0; i < pset.indices(); i++) {
			uint32_t p = polls[i] & 0x7FFFFFFFUL;
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
	for(size_t i = 0; i < movie_data.get_types().indices(); i++) {
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
	bool pflag = pflag_handler ? pflag_handler->get_pflag() : false;
	if(current_frame && !pflag)
		lag_frames++;
	else if(pflag_handler)
		pflag_handler->set_pflag(false);

	//If all poll counters are zero for all real controls, this frame is lag.
	bool this_frame_lag = !pollcounters.has_polled();
	//Oh, frame 0 must not be considered lag.
	if(current_frame && this_frame_lag) {
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

bool movie::get_DRDY(unsigned port, unsigned controller, unsigned ctrl) throw(std::logic_error)
{
	return pollcounters.get_DRDY(port, controller, ctrl);
}

short movie::next_input(unsigned port, unsigned controller, unsigned ctrl) throw(std::bad_alloc, std::logic_error)
{
	pollcounters.clear_DRDY(port, controller, ctrl);

	if(readonly) {
		//In readonly mode...
		//If at the end of the movie, return released / neutral (but also record the poll)...
		if(current_frame_first_subframe >= movie_data.size()) {
			pollcounters.increment_polls(port, controller, ctrl);
			return 0;
		}
		//Before the beginning? Somebody screwed up (but return released / neutral anyway)...
		if(current_frame == 0)
			return 0;
		//Otherwise find the last valid frame of input.
		uint32_t changes = count_changes(current_frame_first_subframe);
		uint32_t polls = pollcounters.get_polls(port, controller, ctrl);
		uint32_t index = (changes > polls) ? polls : changes - 1;
		int16_t data = movie_data[current_frame_first_subframe + index].axis3(port, controller, ctrl);
		pollcounters.increment_polls(port, controller, ctrl);
		return data;
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
			pollcounters.increment_polls(port, controller, ctrl);
			frames_in_movie++;
			return movie_data[current_frame_first_subframe].axis3(port, controller, ctrl);
		}
		short new_value = current_controls.axis3(port, controller, ctrl);
		//Fortunately, we know this frame is the last one in movie_data.
		uint32_t pollcounter = pollcounters.get_polls(port, controller, ctrl);
		if(current_frame_first_subframe + pollcounter < movie_data.size()) {
			//The index is within existing size. Change the value and propagate to all subsequent
			//subframes.
			for(uint64_t i = current_frame_first_subframe + pollcounter; i < movie_data.size(); i++)
				movie_data[i].axis3(port, controller, ctrl, new_value);
		} else if(new_value != movie_data[movie_data.size() - 1].axis3(port, controller, ctrl)) {
			//The index is not within existing size and value does not match. We need to create a new
			//subframes(s), copying the last subframe.
			while(current_frame_first_subframe + pollcounter >= movie_data.size())
				movie_data.append(movie_data[movie_data.size() - 1].copy(false));
			movie_data[current_frame_first_subframe + pollcounter].axis3(port, controller, ctrl,
				new_value);
		}
		pollcounters.increment_polls(port, controller, ctrl);
		return new_value;
	}
}

movie::movie() throw(std::bad_alloc)
{
	seqno = 0;
	readonly = false;
	rerecords = "0";
	_project_id = "";
	current_frame = 0;
	frames_in_movie = 0;
	current_frame_first_subframe = 0;
	lag_frames = 0;
	pflag_handler = NULL;
	clear_caches();
}

void movie::load(const std::string& rerecs, const std::string& project_id, controller_frame_vector& input)
	throw(std::bad_alloc, std::runtime_error)
{
	if(input.size() > 0 && !input[0].sync())
		throw std::runtime_error("First subframe MUST have frame sync flag set");
	seqno++;
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
	pollcounters = pollcounter_vector(input.get_types());
	lag_frames = 0;
	movie_data = input;
	//This is to force internal type of current_controls to become correct.
	current_controls = input.blank_frame(false);
}

controller_frame_vector movie::save() throw(std::bad_alloc)
{
	return movie_data;
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
		//Propagate buttons. The only one that needs special handling is sync flag (index 0, tuple 0,0,0).
		for(size_t i = 1; i < movie_data.get_types().indices(); i++) {
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

movie& movie::operator=(const movie& m)
{
	seqno++;
	readonly = m.readonly;
	rerecords = m.rerecords;
	_project_id = m._project_id;
	movie_data = m.movie_data;
	current_frame = m.current_frame;
	current_frame_first_subframe = m.current_frame_first_subframe;
	pollcounters = m.pollcounters;
	current_controls = m.current_controls;
	lag_frames = m.lag_frames;
	frames_in_movie = m.frames_in_movie;
	cached_frame = m.cached_frame;
	cached_subframe = m.cached_subframe;
	return *this;
}

void movie::set_pflag_handler(poll_flag* handler)
{
	pflag_handler = handler;
}

movie::poll_flag::~poll_flag()
{
}
