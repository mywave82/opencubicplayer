/* aylet 0.2, a .AY music file player.
 * Copyright (C) 2001 Russell Marks and Ian Collier. See main.c for licence.
 *
 * sound.h
 */

#define sound_freq (plrRate)

extern int __attribute__ ((visibility ("internal"))) sound_init(void);
extern void __attribute__ ((visibility ("internal"))) sound_end(void);
extern int __attribute__ ((visibility ("internal"))) sound_frame(int really_play);
extern void __attribute__ ((visibility ("internal"))) sound_frame_blank(void);
extern void __attribute__ ((visibility ("internal"))) sound_start_fade(int fadetime_in_sec);
extern void __attribute__ ((visibility ("internal"))) sound_ay_write(int reg,int val,unsigned long tstates);
extern void __attribute__ ((visibility ("internal"))) sound_ay_reset(void);
extern void __attribute__ ((visibility ("internal"))) sound_ay_reset_cpc(void);
extern void __attribute__ ((visibility ("internal"))) sound_beeper(int on, unsigned long tstates);
