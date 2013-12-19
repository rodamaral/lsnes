#ifdef WITH_OPUS_CODEC
#define OPUS_BUILD
#include <opus.h>
#include <opus_defines.h>
#include <opus_multistream.h>
#include "loadlib.hpp"
#include "opus.hpp"

namespace
{
	loadlib::module opus_mod({
		{"opus_encoder_ctl", (void*)opus_encoder_ctl},
		{"opus_decoder_ctl", (void*)opus_decoder_ctl},
		{"opus_multistream_encoder_ctl", (void*)opus_multistream_encoder_ctl},
		{"opus_multistream_decoder_ctl", (void*)opus_multistream_decoder_ctl},
		{"opus_encoder_init", (void*)opus_encoder_init},
		{"opus_decoder_init", (void*)opus_decoder_init},
		{"opus_encoder_get_size", (void*)opus_encoder_get_size},
		{"opus_decoder_get_size", (void*)opus_decoder_get_size},
		{"opus_encode", (void*)opus_encode},
		{"opus_encode_float", (void*)opus_encode_float},
		{"opus_decode", (void*)opus_decode},
		{"opus_decode_float", (void*)opus_decode_float},
		{"opus_decoder_get_nb_samples", (void*)opus_decoder_get_nb_samples},
		{"opus_repacketizer_init", (void*)opus_repacketizer_init},
		{"opus_repacketizer_get_size", (void*)opus_repacketizer_get_size},
		{"opus_repacketizer_cat", (void*)opus_repacketizer_cat},
		{"opus_repacketizer_out_range", (void*)opus_repacketizer_out_range},
		{"opus_repacketizer_out", (void*)opus_repacketizer_out},
		{"opus_repacketizer_get_nb_frames", (void*)opus_repacketizer_get_nb_frames},
		{"opus_get_version_string", (void*)opus_get_version_string},
		{"opus_multistream_decode", (void*)opus_multistream_decode},
		{"opus_multistream_decode_float", (void*)opus_multistream_decode_float},
		{"opus_multistream_decoder_get_size", (void*)opus_multistream_decoder_get_size},
		{"opus_multistream_decoder_init", (void*)opus_multistream_decoder_init},
		{"opus_multistream_encode", (void*)opus_multistream_encode},
		{"opus_multistream_encode_float", (void*)opus_multistream_encode_float},
		{"opus_multistream_encoder_get_size", (void*)opus_multistream_encoder_get_size},
		{"opus_multistream_encoder_init", (void*)opus_multistream_encoder_init},
		{"opus_packet_get_bandwidth", (void*)opus_packet_get_bandwidth},
		{"opus_packet_get_nb_channels", (void*)opus_packet_get_nb_channels},
		{"opus_packet_get_nb_frames", (void*)opus_packet_get_nb_frames},
		{"opus_packet_get_samples_per_frame", (void*)opus_packet_get_samples_per_frame},
		{"opus_packet_parse", (void*)opus_packet_parse},
#ifdef OPUS_SUPPORTS_SURROUND
		{"opus_multistream_surround_encoder_get_size", (void*)opus_multistream_surround_encoder_get_size},
		{"opus_multistream_surround_encoder_init", (void*)opus_multistream_surround_encoder_init},
#endif
	}, opus::load_libopus);

}
#endif
