#include "sound.hpp"
#include "romimage.hpp"
#include "util.hpp"
#include "state.hpp"
#include "instance.hpp"
#include "library/string.hpp"
#include "library/zip.hpp"
#include "library/minmax.hpp"

namespace sky
{
	sound::sound()
	{
		snds = NULL;
		rate = 128;
		pointer = 0;
		length = 0;
	}

	sound::sound(struct sounds& _snds, uint8_t _rate, uint32_t ptr, uint32_t len)
	{
		snds = &_snds;
		rate = _rate;
		pointer = ptr;
		length = len;
	}

	sounds::sounds()
	{
	}

	sounds::sounds(const std::vector<char>& snd, size_t samples)
	{
		sounddata = snd;
		sfx.resize(samples);
		if(snd.size() < 2 * samples + 2)
			(stringfmt() << "Sound pointer table incomplete").throwex();
		for(unsigned i = 0; i < samples; i++) {
			size_t sptr = combine(snd[2 * i + 0], snd[2 * i + 1]);
			size_t eptr = combine(snd[2 * i + 2], snd[2 * i + 3]);
			if(sptr >= snd.size())
				(stringfmt() << "Sound " << i << " points outside file").throwex();
			if(eptr > snd.size())
				(stringfmt() << "Sound " << i << " extends past the end of file").throwex();
			if(eptr <= sptr)
				(stringfmt() << "Sound " << i << " size invalid (must be >0)").throwex();
			uint8_t rate = snd[sptr];
			sfx[i] = sound(*this, rate, sptr + 1, eptr - sptr - 1);
		}
	}

	sounds::sounds(const sounds& s)
	{
		*this = s;
	}

	sounds& sounds::operator=(const sounds& s)
	{
		if(this == &s)
			return *this;
		sounddata = s.sounddata;
		sfx.resize(s.sfx.size());
		for(unsigned i = 0; i < s.sfx.size(); i++)
			sfx[i] = sound(*this, s.sfx[i].get_rate(), s.sfx[i].get_pointer(), s.sfx[i].get_length());
		return *this;
	}

	active_sfx_dma::active_sfx_dma()
	{
		//End of transfer.
		left = 0;
		pointer = 0;
		subsample = 0;
		mdr = 128;
		rate = 128;
		_hipri = 0;
	}

	void active_sfx_dma::reset(const struct sound& snd)
	{
		rate = snd.get_rate();
		left = 48000ULL * (256 - rate) * snd.get_length();
		subsample = 0;
		pointer = snd.get_pointer();
		mdr = access(snd.get_sounds(), pointer++);
	}

	void active_sfx_dma::fetch(struct sounds& snds, int16_t* buffer, size_t samples)
	{
		while(left > 0 && samples > 0) {
			*(buffer++) = 256 * ((int16_t)mdr - 128);
			*(buffer++) = 256 * ((int16_t)mdr - 128);
			subsample += 1000000;
			while(subsample > 48000ULL * (256 - rate)) {
				mdr = snds.access(pointer++);
				subsample -= 48000ULL * (256 - rate);
			}
			left -= 1000000;
			samples--;
		}
		//Fill the rest with silence.
		for(size_t i = 0; i < samples; i++) {
			*(buffer++) = 0;
			*(buffer++) = 0;
		}
	}

	void fetch_sfx(struct instance& inst, int16_t* buffer, size_t samples)
	{
		std::vector<std::pair<int16_t, int16_t>> buf;
		buf.resize(samples);
		inst.state.dma.fetch(inst.soundfx, buffer, samples);
		try {
			inst.mplayer.decode(&buf[0], samples);
		} catch(...) {
		}
		for(size_t i = 0; i < samples; i++) {
			buffer[2 * i + 0] = max(min((int32_t)buffer[2 * i + 0] + buf[i].first, 32767), -32768);
			buffer[2 * i + 1] = max(min((int32_t)buffer[2 * i + 1] + buf[i].second, 32767), -32768);
		}
	}


	sound_noise_maker::sound_noise_maker(const sounds& _snds, struct active_sfx_dma& _dma)
		: snds(_snds), dma(_dma)
	{
	}

	sound_noise_maker::~sound_noise_maker() {}
	void sound_noise_maker::operator()(int sound, bool hipri)
	{
		if(!hipri && dma.hipri() && dma.busy())
			return;
		dma.reset(snds[sound]);
		dma.hipri(hipri);
	}
}
