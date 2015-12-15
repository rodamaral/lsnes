#include "core/advdumper.hpp"
#include "core/audioapi.hpp"
#include "core/dispatch.hpp"
#include "core/framerate.hpp"
#include "core/instance.hpp"
#include "library/minmax.hpp"
#include "library/threads.hpp"

#include <cstring>
#include <cmath>
#include <iostream>
#include <unistd.h>
#include <sys/time.h>

#define MUSIC_BUFFERS 8
#define MAX_VOICE_ADJUST 200

bool audioapi_instance::vu_disabled = false;

audioapi_instance::dummy_cb_proc::dummy_cb_proc(audioapi_instance& _parent)
	: parent(_parent)
{
}

int audioapi_instance::dummy_cb_proc::operator()()
{
	int16_t buf[16384];
	uint64_t last_ts = framerate_regulator::get_utime();
	while(!parent.dummy_cb_quit) {
		uint64_t cur_ts = framerate_regulator::get_utime();
		uint64_t dt = cur_ts - last_ts;
		last_ts = cur_ts;
		unsigned samples = dt / 25;
		if(samples > 16384)
			samples = 16384;	//Don't get crazy.
		if(parent.dummy_cb_active_play)
			parent.get_mixed(buf, samples, false);
		if(parent.dummy_cb_active_record)
			parent.put_voice(NULL, samples);
		usleep(10000);
	}
	return 0;
}

namespace
{
//  | -1  1 -1  1 | 1  0  0  0 |
//  |  0  0  0  1 | 0  1  0  0 |
//  |  1  1  1  1 | 0  0  1  0 |
//  |  8  4  2  1 | 0  0  0  1 |


//  |  6  0  0  0 |-1  4 -3  1 |           |-1  3 -3  1|
//  |  0  6  0  0 | 3 -6  3  0 |      1/6  | 3 -6  3  0|
//  |  0  0  6  0 |-2 -4  6 -1 |           |-2 -3  6 -1|
//  |  0  0  0  6 | 0  6  0  0 |           | 0  6  0  0|



	void cubicitr_solve(double v1, double v2, double v3, double v4, double& A, double& B, double& C, double& D)
	{
		A = (-v1 + 3 * v2 - 3 * v3 + v4) / 6;
		B = (v1 - 2 * v2 + v3) / 2;
		C = (-2 * v1 - 3 * v2 + 6 * v3 - v4) / 6;
		D = v2;
	}
}

audioapi_instance::resampler::resampler()
{
	position = 0;
	vAl = vBl = vCl = vDl = 0;
	vAr = vBr = vCr = vDr = 0;
}

void audioapi_instance::resampler::resample(float*& in, size_t& insize, float*& out, size_t& outsize, double ratio,
	bool stereo)
{
	double iratio = 1 / ratio;
	while(outsize) {
		double newpos = position + iratio;
		while(newpos >= 1) {
			//Gotta load a new sample.
			if(!insize)
				goto exit;
			vAl = vBl; vBl = vCl; vCl = vDl; vDl = in[0];
			vAr = vBr; vBr = vCr; vCr = vDr; vDr = in[stereo ? 1 : 0];
			--insize;
			in += (stereo ? 2 : 1);
			newpos = newpos - 1;
		}
		position = newpos;
		double A, B, C, D;
		cubicitr_solve(vAl, vBl, vCl, vDl, A, B, C, D);
		*(out++) = ((A * position + B) * position + C) * position + D;
		if(stereo) {
			cubicitr_solve(vAr, vBr, vCr, vDr, A, B, C, D);
			*(out++) = ((A * position + B) * position + C) * position + D;
		}
		--outsize;
	}
exit:
	;
}

audioapi_instance::audioapi_instance()
	: dummyproc(*this)
{
	dummythread = NULL;
	music_ptr = 0;
	last_complete_music_seen = MUSIC_BUFFERS + 1;
	last_complete_music = MUSIC_BUFFERS;
	for(unsigned i = 0; i < MUSIC_BUFFERS; i++) {
		music_size[i] = 0;
		music_rate[i] = 48000;
		music_stereo[i] = false;
	}
	voicep_get = 0;
	voicep_put = 0;
	voicer_get = 0;
	voicer_put = 0;
	voice_rate_play = 40000;
	orig_voice_rate_play = 40000;
	voice_rate_rec = 40000;
	dummy_cb_active_record = false;
	dummy_cb_active_play = false;
	dummy_cb_quit = false;
	_music_volume = 1;
	_voicep_volume = 32767.0;
	_voicer_volume = 1.0/32768;
	last_adjust = false;
}

audioapi_instance::~audioapi_instance()
{
	quit();
}

std::pair<unsigned, unsigned> audioapi_instance::voice_rate()
{
	return std::make_pair(voice_rate_rec, voice_rate_play);
}

unsigned audioapi_instance::orig_voice_rate()
{
	return orig_voice_rate_play;
}

void audioapi_instance::voice_rate(unsigned rate_rec, unsigned rate_play)
{
	if(rate_rec)
		voice_rate_rec = rate_rec;
	else
		voice_rate_rec = 40000;
	dummy_cb_active_record = !rate_rec;
	if(rate_play)
		orig_voice_rate_play = voice_rate_play = rate_play;
	else
		orig_voice_rate_play = voice_rate_play = 40000;
	dummy_cb_active_play = !rate_play;
}

unsigned audioapi_instance::voice_p_status()
{
	unsigned p = voicep_put;
	unsigned g = voicep_get;
	if(g > p)
		return g - p - 1;
	else
		return voicep_bufsize - (p - g) - 1;
}

unsigned audioapi_instance::voice_p_status2()
{
	unsigned p = voicep_put;
	unsigned g = voicep_get;
	if(g > p)
		return voicep_bufsize - (g - p);
	else
		return (p - g);
}

unsigned audioapi_instance::voice_r_status()
{
	unsigned p = voicer_put;
	unsigned g = voicer_get;
	if(g > p)
		return voicer_bufsize - (g - p);
	else
		return (p - g);
}

void audioapi_instance::play_voice(float* samples, size_t count)
{
	unsigned ptr = voicep_put;
	for(size_t i = 0; i < count; i++) {
		voicep_buffer[ptr++] = samples[i];
		if(ptr == voicep_bufsize)
			ptr = 0;
	}
	voicep_put = ptr;
}

void audioapi_instance::record_voice(float* samples, size_t count)
{
	unsigned ptr = voicer_get;
	for(size_t i = 0; i < count; i++) {
		samples[i] = voicer_buffer[ptr++];
		if(ptr == voicer_bufsize)
			ptr = 0;
	}
	voicer_get = ptr;
}

void audioapi_instance::submit_buffer(int16_t* samples, size_t count, bool stereo, double rate)
{
	if(stereo)
		for(unsigned i = 0; i < count; i++)
			CORE().mdumper->on_sample(samples[2 * i + 0], samples[2 * i + 1]);
	else
		for(unsigned i = 0; i < count; i++)
			CORE().mdumper->on_sample(samples[i], samples[i]);
	//Limit buffers to avoid overrunning.
	if(count > music_bufsize / (stereo ? 2 : 1))
		count = music_bufsize / (stereo ? 2 : 1);
	unsigned bidx = last_complete_music;
	bidx = (bidx > (MUSIC_BUFFERS - 2)) ? 0 : bidx + 1;
	memcpy(music_buffer + bidx * music_bufsize, samples, count * (stereo ? 2 : 1) * sizeof(int16_t));
	music_stereo[bidx] = stereo;
	music_rate[bidx] = rate;
	music_size[bidx] = count;
	last_complete_music = bidx;
}

struct audioapi_instance::buffer audioapi_instance::get_music(size_t played)
{
	unsigned midx = last_complete_music_seen;
	unsigned midx2 = last_complete_music;
	if(midx2 >= MUSIC_BUFFERS) {
		//Special case: No buffer.
		struct buffer out;
		out.samples = NULL;
		out.pointer = 0;
		//The rest are arbitrary.
		out.total = 64;
		out.stereo = false;
		out.rate = 48000;
		return out;
	}
	//Handle ACK. If the current buffer is too old, we want to ignore the ACK.
	if(midx >= MUSIC_BUFFERS) {
		//Load initial buffer.
		midx = last_complete_music_seen = 0;
		music_ptr = 0;
	} else {
		music_ptr += played;
		//Otherwise, check if current buffer is not next on the line to be overwritten.
		if((midx2 + 1) % MUSIC_BUFFERS == midx) {
			//It is, bump buffer by one.
			if(!last_adjust && voice_rate_play > orig_voice_rate_play - MAX_VOICE_ADJUST)
				voice_rate_play--;
			last_adjust = true;
			midx = last_complete_music_seen = (midx + 1) % MUSIC_BUFFERS;
			music_ptr = 0;
		} else if(music_ptr >= music_size[midx] && midx != midx2) {
			//It isn't, but current buffer is finished.
			midx = last_complete_music_seen = (midx + 1) % MUSIC_BUFFERS;
			music_ptr = 0;
			last_adjust = false;
		} else if(music_ptr >= music_size[midx] && midx == midx2) {
			if(!last_adjust && voice_rate_play < orig_voice_rate_play + MAX_VOICE_ADJUST)
				voice_rate_play++;
			last_adjust = true;
			//Current buffer is finished, but there is no new buffer.
			//Send silence.
		} else {
			last_adjust = false;
			//Can continue.
		}
	}
	//Fill the structure.
	struct buffer out;
	if(music_ptr < music_size[midx]) {
		out.samples = music_buffer + midx * music_bufsize;
		out.pointer = music_ptr;
		out.total = music_size[midx];
		out.stereo = music_stereo[midx];
		out.rate = music_rate[midx];
	} else {
		//Run out of buffers to play.
		out.samples = NULL;
		out.pointer = 0;
		out.total = 64;		//Arbitrary.
		out.stereo = music_stereo[midx];
		out.rate = music_rate[midx];
		if(out.rate < 100)
			out.rate = 48000;	//Apparently there are buffers with zero rate.
	}
	return out;
}

void audioapi_instance::get_voice(float* samples, size_t count)
{
	unsigned g = voicep_get;
	unsigned p = voicep_put;
	if(samples) {
		for(size_t i = 0; i < count; i++) {
			if(g != p)
				samples[i] = _voicep_volume * voicep_buffer[g++];
			else
				samples[i] = 0.0;
			if(g == voicep_bufsize)
				g = 0;
		}
	} else {
		for(size_t i = 0; i < count; i++) {
			if(g != p)
				g++;
			if(g == voicep_bufsize)
				g = 0;
		}
	}
	voicep_get = g;
}

void audioapi_instance::put_voice(float* samples, size_t count)
{
	unsigned ptr = voicer_put;
	vu_vin(samples, count, false, voice_rate_rec, _voicer_volume);
	for(size_t i = 0; i < count; i++) {
		voicer_buffer[ptr++] = samples ? _voicer_volume * samples[i] : 0.0;
		if(ptr == voicer_bufsize)
			ptr = 0;
	}
	voicer_put = ptr;
}

void audioapi_instance::init()
{
	voicep_get = 0;
	voicep_put = 0;
	voicer_get = 0;
	voicer_put = 0;
	last_complete_music = 3;
	last_complete_music_seen = 4;
	dummy_cb_active_play = true;
	dummy_cb_active_record = true;
	dummy_cb_quit = false;
	dummythread = new threads::thread(dummyproc);
}

void audioapi_instance::quit()
{
	dummy_cb_quit = true;
	if(dummythread) {
		dummythread->join();
		dummythread = NULL;
	}
}

void audioapi_instance::music_volume(float volume)
{
	_music_volume = volume;
}

float audioapi_instance::music_volume()
{
	return _music_volume;
}

void audioapi_instance::voicep_volume(float volume)
{
	_voicep_volume = volume * 32767;
}

float audioapi_instance::voicep_volume()
{
	return _voicep_volume / 32767;
}

void audioapi_instance::voicer_volume(float volume)
{
	_voicer_volume = volume / 32768;
}

float audioapi_instance::voicer_volume()
{
	return _voicer_volume * 32768;
}

void audioapi_instance::get_mixed(int16_t* samples, size_t count, bool stereo)
{
	const size_t intbuf_size = 256;
	float intbuf[intbuf_size];
	float intbuf2[intbuf_size];
	while(count > 0) {
		buffer b = get_music(0);
		float* in = intbuf;
		float* out = intbuf2;
		size_t outdata_used;
		if(b.stereo) {
			size_t indata = min(b.total - b.pointer, intbuf_size / 2);
			size_t outdata = min(intbuf_size / 2, count);
			size_t indata_used = indata;
			outdata_used = outdata;
			if(b.samples)
				for(size_t i = 0; i < 2 * indata; i++)
					intbuf[i] = _music_volume * b.samples[i + 2 * b.pointer];
			else
				for(size_t i = 0; i < 2 * indata; i++)
					intbuf[i] = 0;
			music_resampler.resample(in, indata, out, outdata, (double)voice_rate_play / b.rate, true);
			indata_used -= indata;
			outdata_used -= outdata;
			get_music(indata_used);
			get_voice(intbuf, outdata_used);

			vu_mleft(intbuf2, outdata_used, true, voice_rate_play, 1 / 32768.0);
			vu_mright(intbuf2 + 1, outdata_used, true, voice_rate_play, 1 / 32768.0);
			vu_vout(intbuf, outdata_used, false, voice_rate_play, 1 / 32768.0);

			for(size_t i = 0; i < outdata_used * (stereo ? 2 : 1); i++)
				intbuf2[i] = max(min(intbuf2[i] + intbuf[i / 2], 32766.0f), -32767.0f);
			if(stereo)
				for(size_t i = 0; i < outdata_used * 2; i++)
					samples[i] = intbuf2[i];
			else
				for(size_t i = 0; i < outdata_used; i++)
					samples[i] = (intbuf2[2 * i + 0] + intbuf2[2 * i + 1]) / 2;
		} else {
			size_t indata = min(b.total - b.pointer, intbuf_size);
			size_t outdata = min(intbuf_size, count);
			size_t indata_used = indata;
			outdata_used = outdata;
			if(b.samples)
				for(size_t i = 0; i < indata; i++)
					intbuf[i] = _music_volume * b.samples[i + b.pointer];
			else
				for(size_t i = 0; i < indata; i++)
					intbuf[i] = 0;
			music_resampler.resample(in, indata, out, outdata, (double)voice_rate_play / b.rate, false);
			indata_used -= indata;
			outdata_used -= outdata;
			get_music(indata_used);
			get_voice(intbuf, outdata_used);

			vu_mleft(intbuf2, outdata_used, false, voice_rate_play, 1 / 32768.0);
			vu_mright(intbuf2, outdata_used, false, voice_rate_play, 1 / 32768.0);
			vu_vout(intbuf, outdata_used, false, voice_rate_play, 1 / 32768.0);

			for(size_t i = 0; i < outdata_used; i++)
				intbuf2[i] = max(min(intbuf2[i] + intbuf[i], 32766.0f), -32767.0f);
			if(stereo)
				for(size_t i = 0; i < outdata_used; i++) {
					samples[2 * i + 0] = intbuf2[i];
					samples[2 * i + 1] = intbuf2[i];
				}
			else
				for(size_t i = 0; i < outdata_used; i++)
					samples[i] = intbuf2[i];
		}
		samples += (stereo ? 2 : 1) * outdata_used;
		count -= outdata_used;
	}
}

audioapi_instance::vumeter::vumeter()
{
	accumulator = 0;
	samples = 0;
	vu = -999.0;
}

void audioapi_instance::vumeter::operator()(float* asamples, size_t count, bool stereo, double rate, double scale)
{
	size_t limit = rate / 25;
	//If we already at or exceed limit, cut immediately.
	if(samples >= limit)
		update_vu();
	if(asamples) {
		double sscale = scale * scale;
		size_t j = 0;
		if(stereo)
			for(size_t i = 0; i < count; i++) {
				accumulator += sscale * asamples[j] * asamples[j];
				j += 2;
				samples++;
				if(samples >= limit)
					update_vu();
			}
		else
			for(size_t i = 0; i < count; i++) {
				accumulator += sscale * asamples[i] * asamples[i];
				samples++;
				if(samples >= limit)
					update_vu();
			}
	} else
		for(size_t i = 0; i < count; i++) {
			samples++;
			if(samples >= limit)
				update_vu();
		}
}

void audioapi_instance::vumeter::update_vu()
{
	if(vu_disabled)
		return;
	if(!samples) {
		vu = -999.0;
		accumulator = 0;
	} else {
		double a = accumulator;
		if(a < 1e-120)
			a = 1e-120;	//Don't take log of zero.
		vu = 10 / log(10) * (log(a) - log(samples));
		if(vu < -999.0)
			vu = -999.0;
		accumulator = 0;
		samples = 0;
	}
	CORE().dispatch->vu_change();
}

void audioapi_instance::disable_vu_updates()
{
	vu_disabled = true;
}
