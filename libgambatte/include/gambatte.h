/***************************************************************************
 *   Copyright (C) 2007 by Sindre Aamås                                    *
 *   sinamas@users.sourceforge.net                                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License version 2 as     *
 *   published by the Free Software Foundation.                            *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License version 2 for more details.                *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   version 2 along with this program; if not, write to the               *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#ifndef GAMBATTE_H
#define GAMBATTE_H
#define GAMBATTE_SUPPORTS_ADV_DEBUG
#define GAMBATTE_SUPPORTS_EMU_FLAGS

#include "gbint.h"
#include "inputgetter.h"
#include "loadres.h"
#include <cstddef>
#include <string>
#include <vector>
#include <functional>
#include <map>

//
// Modified 2012-07-10 to 2012-07-14 by H. Ilari Liusvaara
//	- Make it rerecording-friendly.

namespace gambatte {

enum { BG_PALETTE = 0, SP1_PALETTE = 1, SP2_PALETTE = 2 };

struct debugbuffer
{
	//1 => Read, 2 => Write, 4 => Execute, 8 => Cheat
	uint8_t* wram;		//32kB, id1.
	uint8_t* ioamhram;	//512 bytes, id2.
	uint8_t* cart;		//As needed, id3.
	uint8_t* sram;		//As needed, id4.
	uint8_t* bus;		//64kB, id0
	std::map<unsigned, uint8_t> wramcheat;
	std::map<unsigned, uint8_t> sramcheat;
	std::map<unsigned, uint8_t> cartcheat;
	std::function<void(unsigned, unsigned, uint8_t, bool)> read;
	std::function<void(unsigned, unsigned, uint8_t)> write;
	std::function<void(uint16_t)> trace;
	bool trace_cpu;
};

class GB {
public:
	GB();
	~GB();

	enum LoadFlag {
		FORCE_DMG        = 1, /**< Treat the ROM as not having CGB support regardless of
		                           what its header advertises. */
		GBA_CGB          = 2, /**< Use GBA intial CPU register values when in CGB mode. */
		MULTICART_COMPAT = 4  /**< Use heuristics to detect and support some multicart
		                           MBCs disguised as MBC1. */
	};

	 /*
	  * Load ROM image.
	  *
	  * @param romfile  Path to rom image file. Typically a .gbc, .gb, or .zip-file (if
	  *                 zip-support is compiled in).
	  * @param flags    ORed combination of LoadFlags.
	  * @return 0 on success, negative value on failure.
	  */
	LoadRes load(std::string const &romfile, unsigned flags = 0);

	/** Load ROM image.
	  *
	  * @param image    Raw ROM image data.
	  * @param isize    Size of raw ROM image data.
	  * @param flags    ORed combination of LoadFlags.
	  * @return 0 on success, negative value on failure.
	  */
	LoadRes load(const unsigned char* image, size_t isize, unsigned flags = 0);

	/**
	  * Emulates until at least 'samples' audio samples are produced in the
	  * supplied audio buffer, or until a video frame has been drawn.
	  *
	  * There are 35112 audio (stereo) samples in a video frame.
	  * May run for up to 2064 audio samples too long.
	  *
	  * An audio sample consists of two native endian 2s complement 16-bit PCM samples,
	  * with the left sample preceding the right one. Usually casting audioBuf to
	  * int16_t* is OK. The reason for using an uint_least32_t* in the interface is to
	  * avoid implementation-defined behavior without compromising performance.
	  * libgambatte is strictly c++98, so fixed-width types are not an option (and even
	  * c99/c++11 cannot guarantee their availability).
	  *
	  * Returns early when a new video frame has finished drawing in the video buffer,
	  * such that the caller may update the video output before the frame is overwritten.
	  * The return value indicates whether a new video frame has been drawn, and the
	  * exact time (in number of samples) at which it was completed.
	  *
	  * @param videoBuf 160x144 RGB32 (native endian) video frame buffer or 0
	  * @param pitch distance in number of pixels (not bytes) from the start of one line
	  *              to the next in videoBuf.
	  * @param audioBuf buffer with space >= samples + 2064
	  * @param samples  in: number of stereo samples to produce,
	  *                out: actual number of samples produced
	  * @return sample offset in audioBuf at which the video frame was completed, or -1
	  *         if no new video frame was completed.
	  */
	signed runFor(gambatte::uint_least32_t *videoBuf, std::ptrdiff_t pitch,
		      gambatte::uint_least32_t *soundBuf, unsigned &samples);

	/**
	  * Reset to initial state.
	  * Equivalent to reloading a ROM image, or turning a Game Boy Color off and on again.
	  */
	void reset();

	/**
	  * @param palNum 0 <= palNum < 3. One of BG_PALETTE, SP1_PALETTE and SP2_PALETTE.
	  * @param colorNum 0 <= colorNum < 4
	  */
	void setDmgPaletteColor(int palNum, int colorNum, uint_least32_t rgb32);

	/** Sets the callback used for getting input state. */
	void setInputGetter(InputGetter *getInput);

	/**
	  * Sets the directory used for storing save data. The default is the same directory as
	  * the ROM Image file.
	  */
	void setSaveDir(std::string const &sdir);

	/** Returns true if the currently loaded ROM image is treated as having CGB support. */
	bool isCgb() const;

	/** Returns true if a ROM image is loaded. */
	bool isLoaded() const;

	/** Writes persistent cartridge data to disk. Done implicitly on ROM close. */
	void saveSavedata();

	/**
	  * Saves emulator state to the state slot selected with selectState().
	  * The data will be stored in the directory given by setSaveDir().
	  *
	  * @param  videoBuf 160x144 RGB32 (native endian) video frame buffer or 0. Used for
	  *                  saving a thumbnail.
	  * @param  pitch distance in number of pixels (not bytes) from the start of one line
	  *               to the next in videoBuf.
	  * @return success
	  */
	bool saveState(gambatte::uint_least32_t const *videoBuf, std::ptrdiff_t pitch);

	/**
	  * Loads emulator state from the state slot selected with selectState().
	  * @return success
	  */
	bool loadState();

	/**
	  * Saves emulator state to the file given by 'filepath'.
	  *
	  * @param  videoBuf 160x144 RGB32 (native endian) video frame buffer or 0. Used for
	  *                  saving a thumbnail.
	  * @param  pitch distance in number of pixels (not bytes) from the start of one line
	  *               to the next in videoBuf.
	  * @return success
	  */
	bool saveState(gambatte::uint_least32_t const *videoBuf, std::ptrdiff_t pitch,
	               std::string const &filepath);

	/**
	  * Loads emulator state from the file given by 'filepath'.
	  * @return success
	  */
	bool loadState(std::string const &filepath);

	/** Save savestate to given buffer.
	  */
	void saveState(std::vector<char>& data, const std::vector<char>& cmpdata);
	/** Save savestate to given buffer.
	  */
	void saveState(std::vector<char>& data);
	/** Load savestate from given buffer.
	  */
	void loadState(const std::vector<char>& data);

	/**
	  * Selects which state slot to save state to or load state from.
	  * There are 10 such slots, numbered from 0 to 9 (periodically extended for all n).
	  */
	void selectState(int n);

	/**
	  * Current state slot selected with selectState().
	  * Returns a value between 0 and 9 inclusive.
	  */
	int currentState() const;

	/** ROM header title of currently loaded ROM image. */
	std::string const romTitle() const;

	/** GamePak/Cartridge info. */
	class PakInfo const pakInfo() const;

	/**
	  * Set Game Genie codes to apply to currently loaded ROM image. Cleared on ROM load.
	  * @param codes Game Genie codes in format HHH-HHH-HHH;HHH-HHH-HHH;... where
	  *              H is [0-9]|[A-F]
	  */
	void setGameGenie(std::string const &codes);

	/**
	  * Set Game Shark codes to apply to currently loaded ROM image. Cleared on ROM load.
	  * @param codes Game Shark codes in format 01HHHHHH;01HHHHHH;... where H is [0-9]|[A-F]
	  */
	void setGameShark(std::string const &codes);

	/** Set RTC base time.
	  */
	void setRtcBase(time_t time);

	/** Get RTC base time.
	  */
	time_t getRtcBase();

	/** Get pointer and size to Work RAM.
	  * @return The pointer and size of Work RAM.
	 */
	std::pair<unsigned char*, size_t> getWorkRam();

	/** Get pointer and size to Save RAM.
	  * @return The pointer and size of Save RAM.
	 */
	std::pair<unsigned char*, size_t> getSaveRam();

	/** Get pointer and size to I/O RAM.
	  * @return The pointer and size of I/O RAM.
	 */
	std::pair<unsigned char*, size_t> getIoRam();

	/** Get pointer and size to Video RAM.
	  * @return The pointer and size of Video RAM.
	 */
	std::pair<unsigned char*, size_t> getVideoRam();

	/** Function to get wall time. */
	void set_walltime_fn(time_t (*_walltime)());

	/** Get version. */
	static std::string version();

	/** CPU registers. */
	enum cpu_register
	{
		REG_CYCLECOUNTER,
		REG_PC,
		REG_SP,
		REG_HF1,
		REG_HF2,
		REG_ZF,
		REG_CF,
		REG_A,
		REG_B,
		REG_C,
		REG_D,
		REG_E,
		REG_F,
		REG_H,
		REG_L
	};
	uint32_t get_cpureg(enum cpu_register reg);
	void set_cpureg(enum cpu_register reg, uint32_t val);
	void set_debug_buffer(debugbuffer& dbgbuf);
	uint8_t bus_read(unsigned addr);
	void bus_write(unsigned addr, uint8_t val);
	void set_emuflags(unsigned flags);
private:
	void preload_common();
	void postload_common(const unsigned flags);
	struct Priv;
	Priv *const p_;
	time_t (*walltime)();

	GB(GB const &);
	GB & operator=(GB const &);
};

}

#define GAMBATTE_USES_LOADRES

#endif
