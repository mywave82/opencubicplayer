/* aylet 0.2, a .AY music file player.
 * Copyright (C) 2001 Russell Marks and Ian Collier. See main.c for licence.
 *
 * sound.h
 */

/*
extern int sound_enabled;
extern int sound_freq;
extern int sound_stereo;
extern int sound_stereo_beeper;
*/
extern __attribute__ ((visibility ("internal"))) int sound_stereo_ay;
extern __attribute__ ((visibility ("internal"))) int sound_stereo_ay_abc;
extern __attribute__ ((visibility ("internal"))) int sound_stereo_ay_narrow;
/*
extern int soundfd;
extern int sixteenbit;
extern int play_to_stdout;
*/

#define sound_freq (plrRate)
#define sound_stereo (1)

extern int __attribute__ ((visibility ("internal"))) sound_init(void);
extern void __attribute__ ((visibility ("internal"))) sound_end(void);
extern int __attribute__ ((visibility ("internal"))) sound_frame(int really_play);
extern void __attribute__ ((visibility ("internal"))) sound_frame_blank(void);
extern void __attribute__ ((visibility ("internal"))) sound_start_fade(int fadetime_in_sec);
extern void __attribute__ ((visibility ("internal"))) sound_ay_write(int reg,int val,unsigned long tstates);
extern void __attribute__ ((visibility ("internal"))) sound_ay_reset(void);
extern void __attribute__ ((visibility ("internal"))) sound_ay_reset_cpc(void);
extern void __attribute__ ((visibility ("internal"))) sound_beeper(int on);
