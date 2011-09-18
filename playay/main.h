/* aylet 0.1, a .AY music file player.
 * Copyright (C) 2001 Russell Marks and Ian Collier. See main.c for licence.
 *
 * main.h
 */

#ifdef STIAN
enum cb_action_tag
  {
  cb_none,
  cb_quit,
  cb_highspeed,
  cb_prev_file,
  cb_next_file,
  cb_prev_track,
  cb_next_track,
  cb_play,
  cb_pause,
  cb_stop,
  cb_restart,
  cb_dec_stopafter,
  cb_inc_stopafter,
  cb_dec_fadetime,
  cb_inc_fadetime,
  cb_dec_vol,
  cb_inc_vol
  };


extern int action_callback(enum cb_action_tag action);
#endif

extern int __attribute__ ((visibility ("internal"))) ay_do_interrupt(void);
extern unsigned int __attribute__ ((visibility ("internal"))) ay_in(int h,int l);
extern unsigned int __attribute__ ((visibility ("internal"))) ay_out(int h,int l,int a);

extern __attribute__ ((visibility ("internal"))) unsigned char ay_mem[];
extern __attribute__ ((visibility ("internal"))) unsigned long ay_tstates,ay_tsmax;
#ifdef STIAN
extern int highspeed,playing,paused,want_quit;
extern int stopafter,fadetime;
extern int use_ui,play_to_stdout;
extern char **ay_filenames;
extern int ay_file,ay_num_files;
extern __attribute__ ((visibility ("internal"))) int ay_track;
#endif

struct aydata_tag
{
	unsigned char *filedata;
	int filelen;
	struct ay_track_tag *tracks;

	int filever,playerver;
	unsigned char *authorstr,*miscstr;
	int num_tracks;
	int first_track;
};

struct ay_track_tag
{
	unsigned char *namestr;
	unsigned char *data;
	unsigned char *data_stacketc,*data_memblocks;
	int fadestart,fadelen;
};

#ifdef STIAN

extern __attribute__ ((visibility ("internal"))) struct aydata_tag aydata;
#endif

struct time_tag { int min,sec,subsecframes; };
extern __attribute__ ((visibility ("internal"))) struct time_tag ay_tunetime;

