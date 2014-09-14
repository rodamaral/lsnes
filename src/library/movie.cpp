#include "movie.hpp"
#include "minmax.hpp"

#include <stdexcept>
#include <cassert>
#include <cstring>
#include <fstream>


//std::ofstream debuglog("movie-debugging-log", std::ios::out | std::ios::app);

namespace
{
	bool movies_compatible(controller_frame_vector& old_movie, controller_frame_vector& new_movie,
		uint64_t frame, const uint32_t* polls, const std::string& old_projectid,
		const std::string& new_projectid)
	{
		//Project IDs have to match.
		if(old_projectid != new_projectid)
			return false;
		return old_movie.compatible(new_movie, frame, polls);
	}
}

movie::~movie()
{
	if(movie_data)
		movie_data->clear_framecount_notification(_listener);
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
	return movie_data->subframe_count(first_subframe);
}

controller_frame movie::get_controls() throw()
{
	if(!readonly)
		return current_controls;
	controller_frame c = movie_data->blank_frame(false);
	//Before the beginning? Somebody screwed up (but return released / neutral anyway)...
	if(current_frame == 0)
		return c;
	//Otherwise find the last valid frame of input.
	uint32_t changes = count_changes(current_frame_first_subframe);
	if(!changes)
		return c;	//End of movie.
	for(size_t i = 0; i < movie_data->get_types().indices(); i++) {
		uint32_t polls = pollcounters.get_polls(i);
		uint32_t index = (changes > polls) ? polls : changes - 1;
		c.axis2(i, (*movie_data)[current_frame_first_subframe + index].axis2(i));
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
			//As index should be movie_data->size(), it is correct afterwards.
			movie_data->append(current_controls.copy(true));
		}
	}

	//Reset the poll counters and DRDY flags.
	pollcounters.clear_unmasked();

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
		if(current_frame_first_subframe >= movie_data->size()) {
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
		int16_t data = (*movie_data)[current_frame_first_subframe + index].axis3(port, controller, ctrl);
		pollcounters.increment_polls(port, controller, ctrl);
		return data;
	} else {
		//Readwrite mode.
		//Before the beginning? Somebody screwed up (but return released / neutral anyway)...
		//Also, frame 0 must not be added to movie file.
		if(current_frame == 0)
			return 0;
		//If at movie end, insert complete input with frame sync set (this is the first subframe).
		if(current_frame_first_subframe >= movie_data->size()) {
			movie_data->append(current_controls.copy(true));
			//current_frame_first_subframe should be movie_data->size(), so it is right.
			pollcounters.increment_polls(port, controller, ctrl);
			return (*movie_data)[current_frame_first_subframe].axis3(port, controller, ctrl);
		}
		short new_value = current_controls.axis3(port, controller, ctrl);
		//Fortunately, we know this frame is the last one in movie_data.
		uint32_t pollcounter = pollcounters.get_polls(port, controller, ctrl);
		if(current_frame_first_subframe + pollcounter < movie_data->size()) {
			//The index is within existing size. Change the value and propagate to all subsequent
			//subframes.
			for(uint64_t i = current_frame_first_subframe + pollcounter; i < movie_data->size(); i++)
				(*movie_data)[i].axis3(port, controller, ctrl, new_value);
		} else if(new_value != (*movie_data)[movie_data->size() - 1].axis3(port, controller, ctrl)) {
			//The index is not within existing size and value does not match. We need to create a new
			//subframes(s), copying the last subframe.
			while(current_frame_first_subframe + pollcounter >= movie_data->size())
				movie_data->append((*movie_data)[movie_data->size() - 1].copy(false));
			(*movie_data)[current_frame_first_subframe + pollcounter].axis3(port, controller, ctrl,
				new_value);
		}
		pollcounters.increment_polls(port, controller, ctrl);
		return new_value;
	}
}

movie::movie() throw(std::bad_alloc)
	: _listener(*this)
{
	movie_data = NULL;
	seqno = 0;
	readonly = false;
	rerecords = "0";
	_project_id = "";
	current_frame = 0;
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
	readonly = true;
	rerecords = rerecs;
	_project_id = project_id;
	current_frame = 0;
	current_frame_first_subframe = 0;
	pollcounters = pollcounter_vector(input.get_types());
	lag_frames = 0;
	//This is to force internal type of current_controls to become correct.
	current_controls = input.blank_frame(false);
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
			movie_data->clear();
			return;
		}
		//Fun special case: Current frame is not in movie (current_frame_first_subframe >=
		//movie_data->size()). In this case, we have to extend the movie data.
		if(current_frame_first_subframe >= movie_data->size()) {
			//Yes, this will insert one extra frame... But we will lose it later if it is not needed.
			while(movie_data->count_frames() < current_frame)
				movie_data->append(movie_data->blank_frame(true));
			current_frame_first_subframe = movie_data->size() - 1;
		}

		//We have to take the part up to furthest currently readable subframe. Also, we need to propagate
		//forward values with smaller poll counters.
		uint64_t next_frame_first_subframe = current_frame_first_subframe +
			count_changes(current_frame_first_subframe);
		uint64_t max_readable_subframes = current_frame_first_subframe + pollcounters.max_polls();
		if(max_readable_subframes > next_frame_first_subframe)
			max_readable_subframes = next_frame_first_subframe;

		movie_data->resize(max_readable_subframes);
		next_frame_first_subframe = max_readable_subframes;
		//Propagate buttons. The only one that needs special handling is sync flag (index 0, tuple 0,0,0).
		for(size_t i = 1; i < movie_data->get_types().indices(); i++) {
			uint32_t polls = pollcounters.get_polls(i);
			polls = polls ? polls : 1;
			for(uint64_t j = current_frame_first_subframe + polls; j < next_frame_first_subframe; j++)
				(*movie_data)[j].axis2(i, (*movie_data)[current_frame_first_subframe + polls - 1].
					axis2(i));
		}
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
	if(old_movie && !movies_compatible(*old_movie, *movie_data, curframe, &pcounters[0], old_projectid,
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
		return movie_data->blank_frame(true);
	}
	if(max <= subframe)
		subframe = max - 1;
	return (*movie_data)[p + subframe];
}

void movie::reset_state() throw()
{
	readonly = true;
	current_frame = 0;
	current_frame_first_subframe = 0;
	pollcounters.clear_all();
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
	current_frame_first_subframe = (_ptr <= movie_data->size()) ? _ptr : movie_data->size();
	lag_frames = _lagc;
	pollcounters.load_state(_counters);
	readonly_mode(false);
}

void movie::set_pflag_handler(poll_flag* handler)
{
	pflag_handler = handler;
}

int16_t movie::read_subframe_at_index(uint32_t subframe, unsigned port, unsigned controller, unsigned ctrl)
{
	//Readwrite, Past the end of movie or before the beginning?
	if(!readonly || current_frame_first_subframe >= movie_data->size() || current_frame == 0)
		return 0;
	uint32_t changes = count_changes(current_frame_first_subframe);
	uint32_t index = (changes > subframe) ? subframe : changes - 1;
	return (*movie_data)[current_frame_first_subframe + index].axis3(port, controller, ctrl);
}

void movie::write_subframe_at_index(uint32_t subframe, unsigned port, unsigned controller, unsigned ctrl,
	int16_t x)
{
	if(!readonly || current_frame == 0)
		return;
	bool extended = false;
	while(current_frame > movie_data->count_frames()) {
		//Extend the movie by a blank frame.
		extended = true;
		movie_data->append(movie_data->blank_frame(true));
	}
	if(extended) {
		clear_caches();
		current_frame_first_subframe = movie_data->size() - 1;
	}
	if(current_frame < movie_data->count_frames()) {
		//If we are not on the last frame, write is possible if it is not on extension.
		uint32_t changes = count_changes(current_frame_first_subframe);
		if(subframe < changes)
			(*movie_data)[current_frame_first_subframe + subframe].axis3(port, controller, ctrl, x);
	} else  {
		//Writing to the last frame. If not on extension, handle like non-last frame.
		//Note that if movie had to be extended, it was done before, resulting movie like in state with
		//0 stored subframes.
		uint32_t changes = count_changes(current_frame_first_subframe);
		if(subframe < changes)
			(*movie_data)[current_frame_first_subframe + subframe].axis3(port, controller, ctrl, x);
		else {
			//If there is no frame at all, create one.
			if(current_frame_first_subframe >= movie_data->size()) {
				movie_data->append(movie_data->blank_frame(true));
			}
			//Create needed subframes.
			while(count_changes(current_frame_first_subframe) <= subframe)
				movie_data->append(movie_data->blank_frame(false));
			//Write it.
			(*movie_data)[current_frame_first_subframe + subframe].axis3(port, controller, ctrl, x);
		}
	}
}

movie::poll_flag::~poll_flag()
{
}

void movie::set_movie_data(controller_frame_vector* data)
{
	controller_frame_vector* old = movie_data;
	if(data)
		data->set_framecount_notification(_listener);
	movie_data = data;
	clear_caches();
	if(old)
		old->clear_framecount_notification(_listener);
}

movie::fchange_listener::fchange_listener(movie& m)
	: mov(m)
{
}

movie::fchange_listener::~fchange_listener()
{
}

void movie::fchange_listener::notify(controller_frame_vector& src, uint64_t old)
{
	//Recompute frame_first_subframe.
	while(mov.current_frame_first_subframe < mov.movie_data->size() && mov.current_frame > old + 1) {
		//OK, movie has been extended.
		mov.current_frame_first_subframe += mov.count_changes(mov.current_frame_first_subframe);
		old++;
	}
	//Nobody is this stupid, right?
	mov.current_frame_first_subframe = min(mov.current_frame_first_subframe,
		static_cast<uint64_t>(mov.movie_data->size()));
}
