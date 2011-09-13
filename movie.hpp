#ifndef _movie__hpp__included__
#define _movie__hpp__included__

#include <string>
#include <cstdint>
#include <stdexcept>
#include "controllerdata.hpp"

/**
 * \brief Movie being played back or recorded
 */
class movie
{
public:
/**
 * \brief Construct new empty movie.
 * \throws std::bad_alloc Not enough memory.
 */
	movie() throw(std::bad_alloc);

/**
 * \brief Is the movie in readonly mode?
 *
 * \return True if in read-only mode, false if in read-write mode.
 */
	bool readonly_mode() throw();

/**
 * \brief Switch between modes
 *
 * Switches movie to read-only or read-write mode. If switching to read-write mode, the movie is truncated.
 *
 * \param enable If true, switch to read-only mode, else to read-write mode.
 * \throws std::bad_alloc Not enough memory.
 */
	void readonly_mode(bool enable) throw(std::bad_alloc);

/**
 * \brief Get movie rerecord count
 *
 * Returns the movie rerecord count (this is not the same thing as global rerecord count).
 *
 * \return The movie rerecord count
 * \throws std::bad_alloc Not enough memory.
 */
	std::string rerecord_count() throw(std::bad_alloc);

/**
 * \brief Set movie rerecord count
 *
 * Sets the movie rerecord count (this is not the same thing as global rerecord count).
 *
 * \param count The new rerecord count
 * \throws std::bad_alloc Not enough memory.
 */
	void rerecord_count(const std::string& count) throw(std::bad_alloc);

/**
 * \brief Read project ID
 * \return The project ID
 * \throws std::bad_alloc Not enough memory.
 */
	std::string project_id() throw(std::bad_alloc);

/**
 * \brief Set project ID
 * \param id New project ID.
 * \throws std::bad_alloc Not enough memory.
 */
	void project_id(const std::string& id) throw(std::bad_alloc);

/**
 * \brief Get number of frames in movie
 * \return The number of frames.
 */
	uint64_t get_frame_count() throw();

/**
 * \brief Get number of currnet frame in movie
 *
 * The first frame in movie is 1. 0 is "before first frame" value.
 *
 * \return The number of frame
 */
	uint64_t get_current_frame() throw();

/**
 * \brief Get number of lag frames so far
 * \return The number of lag frames.
 */
	uint64_t get_lag_frames() throw();

/**
 * \brief Advance to next frame.
 *
 * This function advances to next frame in movie, discarding subframes not used. If the frame is lag frame, it is
 * counted as lag frame and subframe entry for it is made (if in readwrite mode).
 *
 * \throw std::bad_alloc Not enough memory.
 */
	void next_frame() throw(std::bad_alloc);

/**
 * \brief Get data ready flag for control index
 *
 * Reads the data ready flag. On new frame, all data ready flags are unset. On reading control, its data ready
 * flag is unset.
 *
 * \param controlindex The index of control to read it for.
 * \return The read value.
 * \throws std::logic_error Invalid control index.
 */
	bool get_DRDY(unsigned controlindex) throw(std::logic_error);

/**
 * \brief Get data ready flag for given (port,controller,index) index
 *
 * Reads the data ready flag. On new frame, all data ready flags are unset. On reading control, its data ready
 * flag is unset.
 *
 * This differs from get_DRDY(unsigned) in that this takes (port, controller,index) tuple.
 *
 * \param port The port controller is connected to (0 or 1)
 * \param controller The controller number within port (0 to 3)
 * \param index The index of control in controller (0 to 11)
 * \return The read value.
 * \throws std::logic_error Invalid control index.
 */
	bool get_DRDY(unsigned port, unsigned controller, unsigned index) throw(std::logic_error);

/**
 * \brief Set all data ready flags
 */
	void set_all_DRDY() throw();

/**
 * \brief Poll next value for given control index
 *
 * Poll a control. Note that index 0 (sync flag) always reads as released.
 *
 * \param controlindex The index
 * \return The read value
 * \throws std::bad_alloc Not enough memory.
 * \throws std::logic_error Invalid control index or before movie start.
 */
	short next_input(unsigned controlindex) throw(std::bad_alloc, std::logic_error);

/**
 * \brief Poll next value for given (port, controller, index) index
 *
 * Poll a control.
 *
 * \param port The port controller is connected to (0 or 1)
 * \param controller The controller number within port (0 to 3)
 * \param index The index of control in controller (0 to 11)
 * \return The read value
 * \throws std::bad_alloc Not enough memory.
 * \throws std::logic_error Invalid port, controller or index or before movie start.
 */
	short next_input(unsigned port, unsigned controller, unsigned index) throw(std::bad_alloc, std::logic_error);

/**
 * \brief Set values of current controls
 *
 * Set current control values. These are read in readwrite mode.
 *
 * \param controls The new controls.
 */
	void set_controls(controls_t controls) throw();

/**
 * \brief Get values of current controls
 *
 * Get current control values in effect.
 *
 * \return Controls
 */
	controls_t get_controls() throw();

/**
 * \brief Load a movie.
 *
 * Loads a movie plus some other parameters. The playback pointer is positioned to start of movie and readonly
 * mode is enabled.
 *
 * \param rerecs Movie rerecord count.
 * \param project_id Project ID of movie.
 * \param input The input track.
 * \throws std::bad_alloc Not enough memory.
 * \throws std::runtime_error Bad movie data.
 */
	void load(const std::string& rerecs, const std::string& project_id, const std::vector<controls_t>& input)
		throw(std::bad_alloc, std::runtime_error);

/**
 * \brief Save a movie.
 *
 * Saves the movie data.
 *
 * \return The movie data.
 * \throws std::bad_alloc Not enough memory.
 */
	std::vector<controls_t> save() throw(std::bad_alloc);

/**
 * \brief Save the state of movie code
 *
 * This method serializes the state of movie code.
 *
 * \return The serialized state.
 * \throws std::bad_alloc Not enough memory.
 */
	std::vector<uint8_t> save_state() throw(std::bad_alloc);

/**
 * \brief Restore the state of movie code
 *
 * Given previous serialized state from this movie, restore the state.
 *
 * \param state The state to restore.
 * \param ro If true, restore in readonly mode, otherwise in readwrite mode.
 * \throw std::bad_alloc Not enough memory.
 * \throw std::runtime_error State is not from this movie or states is corrupt.
 */
	size_t restore_state(const std::vector<uint8_t>& state, bool ro) throw(std::bad_alloc, std::runtime_error);

/**
 * \brief Get reset status for current frame.
 * 
 * \return -1 if current frame doesn't have a reset. Otherwise number of cycles to wait for delayed reset (0 is
 *	immediate reset).
 */
	long get_reset_status() throw();

/**
 * \brief Commit a reset.
 */
	void commit_reset(long delay) throw(std::bad_alloc);

/**
 * \brief Get how manyth poll in the frame next poll would be.
 * 
 * \return Poll number.
 */
	unsigned next_poll_number();

	uint64_t frame_subframes(uint64_t frame);
	controls_t read_subframe(uint64_t frame, uint64_t subframe);
private:
	//TRUE if readonly mode is active.
	bool readonly;
	//Movie (not global!) rerecord count.
	std::string rerecords;
	//Project ID.
	std::string _project_id;
	//The actual controller data.
	std::vector<controls_t> movie_data;
	//Current frame + 1 (0 before next_frame() has been called.
	uint64_t current_frame;
	//First subframe in current frame (movie_data.size() if no subframes have been stored).
	uint64_t current_frame_first_subframe;
	//How many times has each control been polled (bit 31 is data ready bit)?
	uint32_t pollcounters[TOTAL_CONTROLS];
	//Current state of buttons.
	controls_t current_controls;
	//Number of known lag frames.
	uint64_t lag_frames;
	//Number of frames in movie.
	uint64_t frames_in_movie;
	//Cached subframes.
	void clear_caches();
	uint64_t cached_frame;
	uint64_t cached_subframe;
	//Count present subframes in frame starting from first_subframe (returns 0 if out of movie).
	uint32_t count_changes(uint64_t first_subframe);
};

/**
 * \brief Class encapsulating bridge logic between bsnes interface and movie code.
 */
class movie_logic
{
public:
/**
 * \brief Create new bridge.
 * 
 * \param m The movie to manipulate.
 */
	movie_logic(movie& m) throw();

/**
 * \brief Destructor.
 */
	virtual ~movie_logic() throw();

/**
 * \brief Get the movie instance associated.
 */
	movie& get_movie() throw();

/**
 * \brief Notify about new frame starting.
 * 
 * \return Reset status for the new frame.
 */
	long new_frame_starting(bool dont_poll) throw(std::bad_alloc, std::runtime_error);

/**
 * \brief Poll for input.
 * 
 * \return Value for polled input.
 */
	short input_poll(bool port, unsigned dev, unsigned id) throw(std::bad_alloc, std::runtime_error);

/**
 * \brief Called when movie code needs new controls snapshot.
 * 
 * \param subframe True if this is for subframe update, false if for frame update.
 */
	virtual controls_t update_controls(bool subframe) throw(std::bad_alloc, std::runtime_error) = 0;
private:
	movie& mov;
};


/**
 * \brief Global rerecord count.
 */
extern std::string global_rerecord_count;

#endif
