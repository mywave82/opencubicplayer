/* aylet 0.1, a .AY music file player.
 * Copyright (C) 2001 Russell Marks and Ian Collier. See main.c for licence.
 *
 * main.h
 */

#if 0
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

struct plrDevAPI_t;
OCP_INTERNAL int ay_do_interrupt (const struct plrDevAPI_t *plrDevAPI);
OCP_INTERNAL unsigned int ay_in(int h,int l);
OCP_INTERNAL unsigned int ay_out(int h,int l,int a);

extern OCP_INTERNAL unsigned char ay_mem[];
extern OCP_INTERNAL unsigned long ay_tstates,ay_tsmax;
#if 0
extern int highspeed,playing,paused,want_quit;
extern int stopafter,fadetime;
extern int use_ui,play_to_stdout;
extern char **ay_filenames;
extern int ay_file,ay_num_files;
extern OCP_INTERNAL int ay_track;
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

#if 0
extern OCP_INTERNAL struct aydata_tag aydata;
#endif

struct time_tag { int min,sec,subsecframes; };
extern OCP_INTERNAL struct time_tag ay_tunetime;

