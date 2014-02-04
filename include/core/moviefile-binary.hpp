#ifndef _moviefile_binary__hpp__included__
#define _moviefile_binary__hpp__included__

/**
 * The binary movie extension tags.
 */
enum lsnes_movie_tags
{
	TAG_ANCHOR_SAVE = 0xf5e0fad7,
	TAG_AUTHOR = 0xafff97b4,
	TAG_CORE_VERSION = 0xe4344c7e,
	TAG_GAMENAME = 0xe80d6970,
	TAG_HOSTMEMORY = 0x3bf9d187,
	TAG_MACRO = 0xd261338f,
	TAG_MOVIE = 0xf3dca44b,
	TAG_MOVIE_SRAM = 0xbbc824b7,
	TAG_MOVIE_TIME = 0x18c3a975,
	TAG_PROJECT_ID = 0x359bfbab,
	TAG_ROMHASH = 0x0428acfc,
	TAG_RRDATA = 0xa3a07f71,
	TAG_SAVE_SRAM = 0xae9bfb2f,
	TAG_SAVESTATE = 0x2e5bc2ac,
	TAG_SCREENSHOT = 0xc6760d0e,
	TAG_SUBTITLE = 0x6a7054d3,
	TAG_RAMCONTENT = 0xd3ec3770,
	TAG_ROMHINT = 0x6f715830,
	TAG_BRANCH = 0xf2e60707,
	TAG_BRANCH_NAME = 0x6dcb2155
};

#endif
