#ifndef _skycore__sound__hpp__included__
#define _skycore__sound__hpp__included__

#include <cstdint>
#include <iostream>
#include <cstring>
#include <stdexcept>
#include <vector>
#include "physics.hpp"

namespace sky
{
	const int sound_explode = 0;		//Ship explodes.
	const int sound_bounce = 1;		//Ship bounces from floor.
	const int sound_blow = 2;		//Ship takes a blow (not fatal).
	const int sound_beep = 3;		//O2/fuel exhausted.
	const int sound_suppiles = 4;		//Suppiles received.

	struct sounds;

	struct sound
	{
		sound();
		sound(struct sounds& _snds, uint8_t _rate, uint32_t ptr, uint32_t len);
		const sounds& get_sounds() const { return *snds; }
		uint8_t get_rate() const { return rate; }
		uint32_t get_pointer() const { return pointer; }
		uint32_t get_length() const { return length; }
	private:
		sounds* snds;
		uint8_t rate;
		uint32_t pointer;
		uint32_t length;
	};

	struct sounds
	{
		sounds();
		sounds(const std::vector<char>& snd, size_t samples);
		sounds(const sounds& s);
		sounds& operator=(const sounds& s);
		const sound& operator[](size_t idx) const { return (idx < sfx.size()) ? sfx[idx] : dummy; }
		uint8_t access(size_t idx) const { return (idx < sounddata.size()) ? sounddata[idx] : 0x80; }
	private:
		std::vector<char> sounddata;
		std::vector<sound> sfx;
		sound dummy;
	};

	struct gstate;
	void fetch_sfx(struct instance& inst, int16_t* buffer, size_t samples);	//Stereo!

	struct active_sfx_dma
	{
		active_sfx_dma();
		void reset(const struct sound& snd);
		void fetch(struct sounds& snds, int16_t* buffer, size_t samples);	//Stereo!
		bool busy() { return (left > 0); }
		bool hipri() { return (_hipri > 0); }
		void hipri(bool hi) { _hipri = hi ? 1 : 0; }
	private:
		uint8_t access(const struct sounds& snds, uint32_t addr) { return snds.access(addr); }
		int64_t left;
		uint32_t pointer;
		uint32_t subsample;
		uint32_t padA;
		uint8_t padB;
		uint8_t _hipri;
		uint8_t mdr;
		uint8_t rate;
	};

	struct sound_noise_maker : public noise_maker
	{
		sound_noise_maker(const sounds& _snds, struct active_sfx_dma& _dma);
		~sound_noise_maker();
		void operator()(int sound, bool hipri = true);
	private:
		const sounds& snds;
		active_sfx_dma& dma;
	};
}
#endif
