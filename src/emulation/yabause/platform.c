#include "yabause.h"
#include "yui.h"
#include "peripheral.h"
#include "sh2core.h"
#include "sh2int.h"
#include "vidsoft.h"
#include "cs0.h"
#include "cs2.h"
#include "cdbase.h"
#include "scsp.h"
#include "debug.h"
#include "m68kcore.h"
#include "m68kc68k.h"

extern PerInterface_struct PerLsnes;
extern SoundInterface_struct SNDLsnes;

M68K_struct * M68KCoreList[] = {
&M68KDummy,
#ifdef HAVE_C68K
&M68KC68K,
#endif
#ifdef HAVE_Q68
&M68KQ68,
#endif
NULL
};

SH2Interface_struct *SH2CoreList[] = {
&SH2Interpreter,
&SH2DebugInterpreter,
#ifdef SH2_DYNAREC
&SH2Dynarec,
#endif
NULL
};

PerInterface_struct *PERCoreList[] = {
&PerLsnes,
NULL
};

CDInterface *CDCoreList[] = {
&ISOCD,
NULL
};

SoundInterface_struct *SNDCoreList[] = {
&SNDLsnes,
NULL
};

VideoInterface_struct *VIDCoreList[] = {
&VIDSoft,
NULL
};

OSD_struct *OSDCoreList[] = {
&OSDDummy,
NULL
};

#ifdef HAVE_C68K
bool yabause_c68k_present = true;
#else
bool yabause_c68k_present = false;
#endif
#ifdef HAVE_Q68
bool yabause_q68_present = true;
#else
bool yabause_q68_present = false;
#endif
#ifdef SH2_DYNAREC
bool yabause_sh2_dynarec = true;
#else
bool yabause_sh2_dynarec = false;
#endif


void yabause_lsnes_per_init(void);
void yabause_lsnes_per_deinit(void);
int yabause_lsnes_per_handle_events(void);
void yabause_lsnes_per_set_button_mapping(void);
u32 yabause_lsnes_per_scan(void);
void yabause_lsnes_per_flush(void);
void yabause_lsnes_per_key_name(u32 key, char* name, int size);
void yabause_lsnes_snd_init(void);
void yabause_lsnes_snd_deinit(void);
int yabause_lsnes_snd_reset(void);
int yabause_lsnes_snd_change_video_format(int vfreq);
void yabause_lsnes_snd_update_audio(u32* left, u32* right, u32 samples);
u32 yabause_lsnes_snd_get_audio_space(void);
void yabause_lsnes_snd_mute(void);
void yabause_lsnes_snd_unmute(void);
void yabause_lsnes_snd_set_volume(int volume);
void YuiErrorMsg(const char *string);
void YuiSetVideoAttribute(int type, int val);
int YuiSetVideoMode(int width, int height, int bpp, int fullscreen);
void YuiSwapBuffers(void);

PerInterface_struct PerLsnes = {
	19,
	"lsnes controller slave interface",
	yabause_lsnes_per_init,
	yabause_lsnes_per_deinit,
	yabause_lsnes_per_handle_events,
	yabause_lsnes_per_set_button_mapping,
	yabause_lsnes_per_scan,
	1,
	yabause_lsnes_per_flush,
	yabause_lsnes_per_key_name
};

SoundInterface_struct SNDLsnes = {
	20,
	"lsnes sound slave interface",
	yabause_lsnes_snd_init,
	yabause_lsnes_snd_deinit,
	yabause_lsnes_snd_reset,
	yabause_lsnes_snd_change_video_format,
	yabause_lsnes_snd_update_audio,
	yabause_lsnes_snd_get_audio_space,
	yabause_lsnes_snd_mute,
	yabause_lsnes_snd_unmute,
	yabause_lsnes_snd_set_volume
} SoundInterface_struct;

