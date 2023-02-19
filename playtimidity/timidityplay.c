/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2005-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Timidity glue logic
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "config.h"
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "timidity.h"
#include "controls.h"
#include "instrum.h"
#include "output.h"
#include "playmidi.h"
#include "readmidi.h"
#include "wrd.h"
#include "aq.h"
#include "recache.h"
#include "resample.h"
#include "arc.h"
#include "boot/psetting.h"
#include "cpikaraoke.h"
#include "cpiface/cpiface.h"
#include "dev/mcp.h"
#include "dev/player.h"
#include "dev/ringbuffer.h"
#include "filesel/mdb.h"
#include "stuff/err.h"
#include "stuff/imsrtns.h"

#include "timidityplay.h"

#include "timidity-git/timidity/playmidi.c"

#define RC_ASYNC_HACK 0x31337

#ifdef PLAYTIMIDITY_DEBUG
# define  PRINT(fmt, args...) fprintf(stderr, "[TiMidity++ MID] %s %s: " fmt, __FILE__, __func__, ##args)
# define _PRINT(fmt, args...) fprintf(stderr, fmt, ##args)
#else
# define  PRINT(a, ...) do {} while(0)
# define _PRINT(a, ...) do {} while(0)
#endif

struct emulate_play_midi_file_session
{
	MidiEvent *event;
	int32 nsamples;
	int first;
};
static struct emulate_play_midi_file_session timidity_main_session;
struct timiditycontext_t tc;

static int gmi_inpause;

static uint64_t samples_committed;
static uint64_t samples_lastui;
static uint64_t samples_lastdelay;

static int bal;
static int vol;
static unsigned long voll,volr;
static int pan;
static int srnd;
static uint32_t speed;
static uint32_t dspeed;
static int loading;

/* devp buffer zone */
static int output_counter;
static int donotloop=1;
static uint32_t gmiRate; /* devp rate */

/* timidityIdler dumping locations */
static int16_t *gmibuf = 0;     /* the buffer */
static struct ringbuffer_t *gmibufpos;
static int32_t gmibuffree; /* we track the ideal buffer-fill and to not request overfill, timidity needs lots of space during EOF */
static uint32_t gmibuffill = 0;

static uint32_t gmibuffpos; /* read fine-pos.. when rate has a fraction */
static uint32_t gmibufrate = 0x10000; /* re-sampling rate.. fixed point 0x10000 => 1.0 */

#define PANPROC \
do { \
	float _rs = rs, _ls = ls; \
	if(pan==-64) \
	{ \
		float t=_ls; \
		_ls = _rs; \
		_rs = t; \
	} else if(pan==64) \
	{ \
	} else if(pan==0) \
		_rs=_ls=(_rs+_ls) / 2.0; \
	else if(pan<0) \
	{ \
		_ls = _ls / (-pan/-64.0+2.0) + _rs*(64.0+pan)/128.0; \
		_rs = _rs / (-pan/-64.0+2.0) + _ls*(64.0+pan)/128.0; \
	} else if(pan<64) \
	{ \
		_ls = _ls / (pan/-64.0+2.0) + _rs*(64.0-pan)/128.0; \
		_rs = _rs / (pan/-64.0+2.0) + _ls*(64.0-pan)/128.0; \
	} \
	rs = _rs * volr / 256.0; \
	ls = _ls * voll / 256.0; \
	if (srnd) \
	{ \
		ls ^= 0xffff; \
	} \
} while(0)

/* clipper threadlock since we use a timer-signal */
static volatile int clipbusy=0;
static char *current_path=0;

static int gmi_eof;
static int gmi_looped;

static int ctl_next_result = RC_NONE;
static int ctl_next_value = 0;

/* main interfaces (To be used another main) */
#if defined(main) || defined(ANOTHER_MAIN) || defined ( IA_W32GUI ) || defined ( IA_W32G_SYN )
#define MAIN_INTERFACE
#else
#define MAIN_INTERFACE static
#endif /* main */
MAIN_INTERFACE void timidity_start_initialize(struct timiditycontext_t *c);
MAIN_INTERFACE int read_config_file(struct timiditycontext_t *c, char *name, int self, int allow_missing_file);
MAIN_INTERFACE int timidity_post_load_configuration(struct timiditycontext_t *c);
MAIN_INTERFACE void timidity_init_player(struct timiditycontext_t *c);
MAIN_INTERFACE int timidity_play_main(struct timiditycontext_t *c, int nfiles, char **files);
MAIN_INTERFACE void tmdy_free_config(struct timiditycontext_t *c);
extern void init_effect(struct timiditycontext_t *c);
void timidity_init_aq_buff(struct timiditycontext_t *c);

struct _CtlEventDelayed;
typedef struct _CtlEventDelayed
{
	struct _CtlEventDelayed *next;
	int delay_samples;
	CtlEvent event;
} CtlEventDelayed;

static CtlEventDelayed *EventDelayed_PlrBuf_head = 0; /* in devp space */
static CtlEventDelayed *EventDelayed_PlrBuf_tail = 0;

static CtlEventDelayed *EventDelayed_gmibuf_head = 0; /* in gmibuf space */
static CtlEventDelayed *EventDelayed_gmibuf_tail = 0;

static void free_EventDelayed (CtlEventDelayed *self)
{
	if (self->event.type == CTLE_PROGRAM)
	{
		free ((char *)self->event.v3);
		self->event.v3 = 0;
	}
	free (self);
}

struct mchaninfo channelstat[16] = {{{0}}};

void __attribute__ ((visibility ("internal"))) timidityGetChanInfo(uint8_t ch, struct mchaninfo *ci)
{
	assert (ch < 16);
	*ci = channelstat[ch];
}

static void timidity_apply_EventDelayed (struct cpifaceSessionAPI_t *cpifaceSession, CtlEvent *event)
{
	switch (event->type)
	{
		case CTLE_NOTE:
			if ((event->v2 < 0) || (event->v2 >= 16))
			{
				return; /* Outside the normal MIDI range */
			}
			if (event->v1 == 2) /* NOTE-ON */
			{
				int i;
				for (i=0; i < channelstat[event->v2].notenum; i++)
				{
					if (channelstat[event->v2].note[i] == event->v3)
					{
						PRINT ("RESTART ch=%ld note=%ld\n", event->v2, event->v3);
						channelstat[event->v2].vol[i] = event->v4;
						channelstat[event->v2].opt[i] = 1; /* no idea */
						return; /* already playing? */
					}
				}

				if (channelstat[event->v2].notenum >= 32)
				{
					return; /* FULL */
				}

				for (i=0; i < (channelstat[event->v2].notenum); i++)
				{
					if (channelstat[event->v2].note[i] > event->v3)
					{ /* INSERT */
						PRINT ("INSERT ch=%ld note=%ld\n", event->v2, event->v3);
						memmove (channelstat[event->v2].note + i + 1, channelstat[event->v2].note + i, channelstat[event->v2].notenum - i);
						memmove (channelstat[event->v2].vol + i + 1, channelstat[event->v2].vol + i, channelstat[event->v2].notenum - i);
						memmove (channelstat[event->v2].opt + i + 1, channelstat[event->v2].opt + i, channelstat[event->v2].notenum - i);

						channelstat[event->v2].note[i] = event->v3;
						channelstat[event->v2].vol[i] = event->v4;
						channelstat[event->v2].opt[i] = 1; /* no idea */
						channelstat[event->v2].notenum++;
						return;
					}
				}

				/* APPEND */
				PRINT ("APPEND ch=%ld note=%ld\n", event->v2, event->v3);
				channelstat[event->v2].note[channelstat[event->v2].notenum] = event->v3;
				channelstat[event->v2].vol[channelstat[event->v2].notenum] = event->v4;
				channelstat[event->v2].opt[channelstat[event->v2].notenum] = 1; /* no idea */
				channelstat[event->v2].notenum++;
				return;

			} else if (event->v1 == 4) /* SUSTAIN */
			{
				int i;
				for (i=0; i < channelstat[event->v2].notenum; i++)
				{
					if (channelstat[event->v2].note[i] == event->v3)
					{
						channelstat[event->v2].opt[i] &= ~1; /* no idea */
						return; /* already playing? */
					}
				}
			} else if ((event->v1 == 1) || (event->v1 == 8) || (event->v1 == 16)) /* NOTE-OFF */
			{
				int i;
				for (i=0; i < channelstat[event->v2].notenum; i++)
				{
					if (channelstat[event->v2].note[i] == event->v3)
					{ /* REMOVE */
						PRINT ("REMOVE ch=%ld note=%ld\n", event->v2, event->v3);
						memmove (channelstat[event->v2].note + i, channelstat[event->v2].note + i + 1, channelstat[event->v2].notenum - i - 1);
						memmove (channelstat[event->v2].vol + i, channelstat[event->v2].vol + i + 1, channelstat[event->v2].notenum - i - 1);
						memmove (channelstat[event->v2].opt + i, channelstat[event->v2].opt + i + 1, channelstat[event->v2].notenum - i - 1);
						channelstat[event->v2].notenum--;
						return;
					}
				}
				PRINT ("NOT FOUND REMOVE ch=%ld note=%ld\n", event->v2, event->v3);
			}
			return;

		case CTLE_PROGRAM:
			if ((event->v1 < 0) || (event->v1 >= 16))
			{
				return; /* Outside the normal MIDI range */
			}
			snprintf (channelstat[event->v1].instrument, sizeof (channelstat[event->v1].instrument), "%s", (char *)event->v3);
			channelstat[event->v1].program = event->v2;
			channelstat[event->v1].bank_msb = event->v4>>8;
			channelstat[event->v1].bank_lsb = event->v4&0xff;
			return;

		case CTLE_VOLUME:
			if ((event->v1 < 0) || (event->v1 >= 16))
			{
				return; /* Outside the normal MIDI range */
			}
			channelstat[event->v1].gvol = event->v2;
			return;

		case CTLE_PANNING:
			if ((event->v1 < 0) || (event->v1 >= 16))
			{
				return; /* Outside the normal MIDI range */
			}
			channelstat[event->v1].pan = event->v2 & 0x7f;
			return;

		case CTLE_PITCH_BEND:
			if ((event->v1 < 0) || (event->v1 >= 16))
			{
				return; /* Outside the normal MIDI range */
			}
			channelstat[event->v1].pitch = event->v2;
			return;

		case CTLE_CHORUS_EFFECT:
			if ((event->v1 < 0) || (event->v1 >= 16))
			{
				return; /* Outside the normal MIDI range */
			}
			channelstat[event->v1].chorus = event->v2;
			return;

		case CTLE_REVERB_EFFECT:
			if ((event->v1 < 0) || (event->v1 >= 16))
			{
				return; /* Outside the normal MIDI range */
			}
			channelstat[event->v1].reverb = event->v2;
			return;

		case CTLE_SUSTAIN:
			if ((event->v1 < 0) || (event->v1 >= 16))
			{
				return; /* Outside the normal MIDI range */
			}
			channelstat[event->v1].pedal = event->v2;
			return;
		case CTLE_LYRIC:
			PRINT ("Lyric time=%u id=%u\n", (unsigned int)event->v2, (unsigned int)event->v1);
			cpiKaraokeSetTimeCode (cpifaceSession, (uint32_t)event->v2);
			return;
	}
}

static void timidity_append_EventDelayed_PlrBuf (struct cpifaceSessionAPI_t *cpifaceSession, CtlEventDelayed *self, signed int delay_samples)
{
	if (delay_samples <= 0)
	{
		timidity_apply_EventDelayed (cpifaceSession, &self->event);

		free_EventDelayed (self);

		return;
	}

	self->delay_samples = delay_samples;

	if (EventDelayed_PlrBuf_head)
	{
#if 0
		assert (EventDelayed_PlrBuf_tail->delay_samples <= self->delay_samples);
#else
		// just in case above assert do hit, we ensure that the events the cue are in rising order
		if (self->delay_samples < EventDelayed_PlrBuf_tail->delay_samples)
		{
			EventDelayed_PlrBuf_tail->delay_samples = self->delay_samples;
		}
#endif
		EventDelayed_PlrBuf_tail->next = self;
	} else {
		EventDelayed_PlrBuf_head = self;
	}
	EventDelayed_PlrBuf_tail = self;
}

static void timidity_play_target_EventDelayed_gmibuf (struct cpifaceSessionAPI_t *cpifaceSession, unsigned int delta)
{
	CtlEventDelayed *iter, *next;

	for (iter = EventDelayed_PlrBuf_head; iter; iter = next)
	{
		next = iter->next;

		if (iter->delay_samples > delta)
		{
			iter->delay_samples -= delta;
		} else {
			assert (EventDelayed_PlrBuf_head == iter);

			iter->delay_samples = 0;

			EventDelayed_PlrBuf_head = next;

			iter->next = 0;

			if (!next)
			{
				EventDelayed_PlrBuf_tail = 0; /* we are always deleting the head entry if any.. so list is empty if no next */
			}

			timidity_apply_EventDelayed (cpifaceSession, &iter->event);

			free_EventDelayed (iter);
		}
	}
}

static void timidity_append_EventDelayed_gmibuf (CtlEvent *event)
{
	CtlEventDelayed *self = calloc (sizeof (*self), 1);

	if (!self)
	{
		perror ("timidity_append_EventDelayed_gmibuf malloc");
		_exit(1);
	}

	self->event = *event;
	self->delay_samples = gmibuffill;

	if (self->event.type == CTLE_PROGRAM)
	{
		self->event.v3 = (long)strdup(self->event.v3?(char *)self->event.v3:"");
	}

	if (EventDelayed_gmibuf_head)
	{
		assert (EventDelayed_gmibuf_tail->delay_samples <= self->delay_samples);
		EventDelayed_gmibuf_tail->next = self;
		EventDelayed_gmibuf_tail = self;
	} else {
		EventDelayed_gmibuf_head = self;
		EventDelayed_gmibuf_tail = self;
	}
}

static void timidity_play_source_EventDelayed_gmibuf (struct cpifaceSessionAPI_t *cpifaceSession, uint32_t samplesin, uint32_t samplesout)
{
	CtlEventDelayed *iter, *next;

	for (iter = EventDelayed_gmibuf_head; iter; iter = next)
	{
		next = iter->next;

		if (iter->delay_samples <= samplesin)
		{
			assert (iter == EventDelayed_gmibuf_head);

			EventDelayed_gmibuf_head = next;

			if (!iter->next)
			{
				EventDelayed_gmibuf_tail = 0; /* we are always deleting the head entry if any.. so list is empty if no next */
			} else {
				iter->next = 0;
			}
			timidity_append_EventDelayed_PlrBuf (cpifaceSession, iter, (int)samples_lastdelay + samplesout - (samplesin - iter->delay_samples));
		} else {
			iter->delay_samples -= samplesin;
		}
	}
}

static int read_user_config_file(void)
{
    char *home;
    char path[BUFSIZ];
    int status;

    home = getenv("HOME");
#ifdef __W32__
/* HOME or home */
    if(home == NULL)
	home = getenv("HOMEPATH");
    if(home == NULL)
	home = getenv("home");
#endif
    if(home == NULL)
    {
	ctl->cmsg(CMSG_INFO, VERB_NOISY,
		  "Warning: HOME environment is not defined.");
	return 0;
    }

#ifdef __W32__
/* timidity.cfg or _timidity.cfg or .timidity.cfg*/
    snprintf(path, sizeof (path), "%s" PATH_STRING "timidity.cfg", home);
    status = read_config_file(&tc, path, 0, 1);
    if (status != READ_CONFIG_FILE_NOT_FOUND)
        return status;

    snprintf(path, sizeof (path), "%s" PATH_STRING "_timidity.cfg", home);
    status = read_config_file(&tc, path, 0, 1);
    if (status != READ_CONFIG_FILE_NOT_FOUND)
        return status;
#endif

    snprintf(path, sizeof (path), "%s" PATH_STRING ".timidity.cfg", home);
    status = read_config_file(&tc, path, 0, 1);
    if (status != 3 /*READ_CONFIG_FILE_NOT_FOUND*/)
        return status;

    return 0;
}

/* Killing Module 2 MIDI support with dummy calls */

int __attribute__ ((visibility ("internal"))) get_module_type (char *fn)
{
	return IS_OTHER_FILE; /* Open Cubic Player will provide module support */
}

int __attribute__ ((visibility ("internal"))) convert_mod_to_midi_file(struct timiditycontext_t *c, MidiEvent * ev)
{
	ctl->cmsg(CMSG_INFO, VERB_NORMAL,
	          "Aborting!  timidity attempted to convert module to midi file\n");
	return 1;
}

char __attribute__ ((visibility ("internal"))) *get_module_title (struct timidity_file *tf, int mod_type)
{
	return NULL;
}

int __attribute__ ((visibility ("internal"))) load_module_file (struct timiditycontext_t *c, struct timidity_file *tf, int mod_type)
{
	return 1; /* Fail to load */
}

void __attribute__ ((visibility ("internal"))) ML_RegisterAllLoaders ()
{
}

/* Our virtual Control Mode */


static int ocp_ctl_open (int using_stdin, int using_stdout)
{
	return 0;
}

static void ocp_ctl_close (void)
{
}

static int ocp_ctl_pass_playing_list (int number_of_files, char *list_of_files[])
{
	return 0;
}

void __attribute__ ((visibility ("internal"))) timidityRestart (void)
{
	ctl_next_value = 0;
	ctl_next_result = RC_RESTART;
}

void __attribute__ ((visibility ("internal"))) timiditySetRelPos(int pos)
{
	if (pos > 0)
	{ /* async, so set the value, before result */
		ctl_next_value = gmiRate * pos;
		ctl_next_result = RC_FORWARD;
	} else {
		ctl_next_value = gmiRate * -pos;
		ctl_next_result = RC_BACK;
	}
}

static int ocp_ctl_read (int32 *valp)
{
	int retval = ctl_next_result;
	*valp = ctl_next_value;

	if (retval != RC_NONE)
	{
		ctl_next_result = RC_NONE;
		ctl_next_value = 0;

		PRINT ("ctl->read=%d (val=%d)\n", retval, *valp);
	}

	return retval;
}

static int ocp_ctl_write (char *buf, int32 size)
{
	/* stdout redirects ? */
	PRINT ("CTL WRITE %.*s", (int)size, buf);
	return size;
}


static int ocp_ctl_cmsg(int type, int verbosity_level, char *fmt, ...)
{
#ifdef PLAYTIMIDITY_DEBUG
	va_list ap;

	if (verbosity_level == VERB_DEBUG_SILLY)
	{
		if (!loading)
		{
			return 0;
		}
	} else if (!((type == CMSG_WARNING) || (type == CMSG_ERROR) || (type == CMSG_FATAL)))
	{
		if (!loading)
		{
			return 0;
		}
	}

	fputs("[TiMidity++ MID] CTL CMSG ", stderr);

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	fputs ("\n", stderr);
#endif
	return 0;
}

static void ocp_ctl_event(CtlEvent *event)
{
	switch (event->type)
	{
		case CTLE_NOW_LOADING:
			PRINT ("ctl->event (event=CTLE_NOW_LOADING, v1=\"%s\")\n", (char *)event->v1);
			break;
		case CTLE_LOADING_DONE:
			PRINT ("ctl->event (event=CTLE_LOADING_DONE, v1=%ld %s)\n", event->v1, (event->v1<0)?"error":(event->v1>0)?"terminated":"success");
			break;
		case CTLE_PLAY_START:
			PRINT ("ctl->event (event=CTLE_PLAY_START, v1=%ld samples)\n", event->v1);
			break;
		case CTLE_PLAY_END:
			PRINT ("ctl->event (event=CTLE_PLAY_END)\n");
			break;
		case CTLE_CURRENT_TIME:
			PRINT ("ctl->event (event=CTLE_CURRENT_TIME, v1=%ld seconds, v2=%ld voices)\n", event->v1, event->v2);
			break;
		case CTLE_NOTE:
			PRINT ("ctl->event (event=CTLE_NOTE, v1=%ld status, v2=%ld ch, v3=%ld note, v4=%ld velocity)\n", event->v1, event->v2, event->v3, event->v4);
			break;
		case CTLE_MASTER_VOLUME:
			PRINT ("ctl->event (event=CTLE_MASTER_VOLUME, v1=%ld amp %%)\n", event->v1);
			break;
		case CTLE_METRONOME:
			PRINT ("ctl->event (event=CTLE_METRONOME, v1=%ld measure, v2=%ld beat)\n", event->v1, event->v2);
			break;
		case CTLE_KEYSIG:
			PRINT ("ctl->event (event=CTLE_KEYSIG, v1=%ld key sig)\n", event->v1);
			break;
		case CTLE_KEY_OFFSET:
			PRINT ("ctl->event (event=CTLE_KEY_OFFSET, v1=%ld key offset)\n", event->v1);
			break;
		case CTLE_TEMPO:
			PRINT ("ctl->event (event=CTLE_TEMPO, v1=%ld tempo)\n", event->v1);
			break;
		case CTLE_TIME_RATIO:
			PRINT ("ctl->event (event=CTLE_TIME_RATIO, v1=%ld time ratio %%)\n", event->v1);
			break;
		case CTLE_TEMPER_KEYSIG:
			PRINT ("ctl->event (event=CTLE_TEMPER_KEYSIG, v1=%ld tuning key sig)\n", event->v1);
			break;
		case CTLE_TEMPER_TYPE:
			PRINT ("ctl->event (event=CTLE_TEMPER_TYPE, v1=%ld ch, v2=%ld tuning type)\n", event->v1, event->v2);
			break;
		case CTLE_MUTE:
			PRINT ("ctl->event (event=CTLE_MUTE, v1=%ld ch, v2=%ld is_mute)\n", event->v1, event->v2);
			break;
		case CTLE_PROGRAM:
			PRINT ("ctl->event (event=CTLE_PROGRAM, v1=%ld ch, v2=%ld prog, v3=name \"%s\", v4=%ld bank %dmsb.%dlsb)\n", event->v1, event->v2, (char *)event->v3, event->v4, (int)(event->v4>>8), (int)(event->v4&0xff));
			break;
		case CTLE_VOLUME:
			PRINT ("ctl->event (event=CTLE_VOLUME, v1=%ld ch, v2=%ld value)\n", event->v1, event->v2);
			break;
		case CTLE_EXPRESSION:
			PRINT ("ctl->event (event=CTLE_EXPRESSION, v1=%ld ch, v2=%ld value)\n", event->v1, event->v2);
			break;
		case CTLE_PANNING:
			PRINT ("ctl->event (event=CTLE_PANNING, v1=%ld ch, v2=%ld value)\n", event->v1, event->v2);
			break;
		case CTLE_SUSTAIN:
			PRINT ("ctl->event (event=CTLE_SUSTAIN, v1=%ld ch, v2=%ld value)\n", event->v1, event->v2);
			break;
		case CTLE_PITCH_BEND:
			PRINT ("ctl->event (event=CTLE_PITCH_BEND, v1=%ld ch, v2=%ld value)\n", event->v1, event->v2);
			break;
		case CTLE_MOD_WHEEL:
			PRINT ("ctl->event (event=CTLE_MOD_WHEEL, v1=%ld ch, v2=%ld value)\n", event->v1, event->v2);
			break;
		case CTLE_CHORUS_EFFECT:
			PRINT ("ctl->event (event=CTLE_CHORUS_EFFECT, v1=%ld ch, v2=%ld value)\n", event->v1, event->v2);
			break;
		case CTLE_REVERB_EFFECT:
			PRINT ("ctl->event (event=CTLE_REVERB_EFFECT, v1=%ld ch, v2=%ld value)\n", event->v1, event->v2);
			break;
		case CTLE_LYRIC:
			PRINT ("ctl->event (event=CTLE_LYRIC, v1=%ld lyric-ID)\n", event->v1);
			break;
		case CTLE_REFRESH:
			PRINT ("ctl->event (event=CTLE_REFRESH)\n");
			break;
		case CTLE_RESET:
			PRINT ("ctl->event (event=CTLE_RESET)\n");
			break;
		case CTLE_SPEANA:
#ifdef PLAYTIMIDITY_DEBUG
			{
				double *v1 = (double *)event->v1;
				int i;
				PRINT ("[TiMidity++ MID] ctl->event (event=CTLE_SPEANA, v1=[)");
				for (i=0; i < event->v2; i++)
				{
					_PRINT ("%s%lf", i?",":"", v1[i]);
				}
				_PRINT ("], v2=%ld len)\n", event->v2);
			}
#endif
			break;
		case CTLE_PAUSE:
			PRINT ("ctl->event (event=CTLE_PAUSE, v1=%ld on/off, v2=%ld time of pause)\n", event->v1, event->v2);
			break;
		case CTLE_GSLCD:
			PRINT ("ctl->event (event=CTLE_GSLCD, v1=%ld ?)\n", event->v1);
			break;
		case CTLE_MAXVOICES:
			PRINT ("ctl->event (event=CTLE_MAXVOICES, v1=%ld voices)\n", event->v1);
			break;
		case CTLE_DRUMPART:
			PRINT ("ctl->event (event=CTLE_DRUMPART, v1=%ld ch, v2=%ld is_drum)\n", event->v1, event->v2);
			break;
		default:
			PRINT ("ctl->event (event=%d unknown)\n", event->type);
			break;
	}

	switch (event->type)
	{
		case CTLE_NOTE:
			if ( (event->v1 ==  1) || /* NOTE FREE */
			     (event->v1 ==  2) || /* NOTE ON */
			     (event->v1 ==  4) || /* NOTE SUSTAIN */
			     (event->v1 ==  8) || /* NOTE OFF */
			     (event->v1 == 16))   /* NOTE DIE */
			{
				timidity_append_EventDelayed_gmibuf (event);
			}
			break;
		//case CTLE_MUTE:
		case CTLE_PROGRAM:
		case CTLE_PANNING:
		case CTLE_PITCH_BEND:
		//case CTLE_DRUMPART:
		case CTLE_VOLUME:
		case CTLE_CHORUS_EFFECT:
		case CTLE_REVERB_EFFECT:
		case CTLE_SUSTAIN:
		case CTLE_LYRIC:
			timidity_append_EventDelayed_gmibuf (event);
			break;

	}
}

static ControlMode ocp_ctl =
{
	.id_name           = "Open Cubic Player Control API",
	.id_character      = '_',
	.id_short_name     = "OCP",
	.verbosity         = 0,
	.trace_playing     = 0,
	.opened            = 1,
	.flags             = 0,
	.open              = ocp_ctl_open,
	.close             = ocp_ctl_close,
	.pass_playing_list = ocp_ctl_pass_playing_list,
	.read              = ocp_ctl_read,
	.write             = ocp_ctl_write,
	.cmsg              = ocp_ctl_cmsg,
	.event             = ocp_ctl_event
};

/* Our virtual PlayMode */

static int ocp_playmode_open_output(void)
{
	/* 0=success, 1=warning, -1=fatal error */
	PRINT ("playmode->open_output()\n");
	output_counter = 0;
	return 0;
}

static void ocp_playmode_close_output(void)
{
	PRINT ("playmode->close_output\n");
}


static int ocp_playmode_output_data(struct timiditycontext_t *c, char *buf, int32 bytes)
{
	int pos1, pos2;
	int length1, length2;

	struct cpifaceSessionAPI_t *cpifaceSession = c->contextowner;

	PRINT ("playmode->output_data (bytes=%d)\n", bytes);

	/* convert bytes into samples */
	bytes >>= 2; /* stereo + bit16 */

	output_counter += bytes;

	cpifaceSession->ringbufferAPI->get_head_samples (gmibufpos, &pos1, &length1, &pos2, &length2);

	while (length1 && bytes)
	{
		if (length1>bytes)
		{
			length1=bytes;
		}
		memcpy (gmibuf + (pos1<<1), buf, length1<<2);
		buf += length1<<2;
		bytes -= length1;

		gmibuffill += length1;
		gmibuffree -= length1;

		cpifaceSession->ringbufferAPI->head_add_samples (gmibufpos, length1);
		cpifaceSession->ringbufferAPI->get_head_samples (gmibufpos, &pos1, &length1, &pos2, &length2);
	}

	if (bytes)
	{
		PRINT ("warning we lost %u samples\n", (unsigned int)bytes);
	}

	return 0;
}

static int ocp_playmode_acntl(int request, void *arg)
{
	switch(request)
	{
		/* case PM_REQ_MIDI - we have not enabled this flag */
		/* case PM_REQ_INST_NAME - ??? */

		case PM_REQ_DISCARD: /* what-ever... we are the actual master of this */
			PRINT ("playmode->acntl(request=DISCARD)\n");
			output_counter = 0;
			return 0;

		case PM_REQ_FLUSH: /* what-ever... we are the actual master of this */
			PRINT ("playmode->acntl(request=FLUSH)\n");
			output_counter = 0;
			return 0;

		case PM_REQ_GETQSIZ:
			PRINT ("playmode->acntl(request=GETQSIZ) => %d\n", gmibuffree>>1);
			if (gmibuffree <= 0)
			{
				*((int *)arg) = 0;
			} else {
				*((int *)arg) = gmibuffree >> 1; //   buflen>>1 /* >>1 is due to STEREO */;
			}
			return 0;

		/* case PM_REQ_SETQSIZ */
		/* case PM_REQ_GETFRAGSIZ */

		case PM_REQ_RATE:
			/* sample rate in and out */
			PRINT ("playmode->acntl(request=RATE, rate=%d)\n", *(int *)arg);
			/* but we ignore it... */
			return 0;

		case PM_REQ_GETSAMPLES:
			PRINT ("playmode->acntl(request=GETSAMPLES) => 0 (fixed)\n");
			*((int *)arg) = output_counter; /* samples */
			return 0;

		case PM_REQ_PLAY_START: /* Called just before playing */
			PRINT ("playmode->acntl(request=PLAY_START)\n");
			break;

		case PM_REQ_PLAY_END: /* Called just after playing */
			PRINT ("playmode->acntl(request=PLAY_END)\n");
			return 0;

		case PM_REQ_GETFILLABLE:
			{
				ssize_t clean = gmibuffree;
				/* Timidity always tries to overfill the buffer, since it normally waits for completion... */
				if (clean < 0)
				{
					clean = 0;
				}
				PRINT ("playmode->acntl(request=GETFILLABLE) => %d\n", (int)clean);
				*((int *)arg) = clean; /* samples */;
				return 0;
			}

		case PM_REQ_GETFILLED:
			*((int *)arg) = gmibuffill;
			PRINT ("playmode->acntl(request=GETFILLED) => %d\n", *((int *)arg));
			return 0;

		/* case PM_REQ_OUTPUT_FINISH: called just after the last output_data(); */

		case PM_REQ_DIVISIONS:
			/* sample rate in and out */
			PRINT ("playmode->acntl(request=DIVISIONS, bpm=%d\n", *(int *)arg);
			/* but we ignore it... */
			return 0;
	}
	return -1;
}


static int ocp_playmode_detect(void)
{
	/* 0=not available, 1=available */
	return 1;
}

static PlayMode ocp_playmode =
{
	.encoding = /*PE_MONO*/ PE_SIGNED | PE_16BIT, /* TODO PE_24BIT */
	.flag = PF_PCM_STREAM | PF_CAN_TRACE,
	.rate = 44100,
	.fd = -1,
	.extra_param = {0},
	.id_name = "Open Cubic Player Output API",
	.id_character = '_',
	.name = "NULL",
	.open_output = ocp_playmode_open_output,
	.close_output = ocp_playmode_close_output,
	.output_data = ocp_playmode_output_data,
	.acntl = ocp_playmode_acntl,
	.detect= ocp_playmode_detect
};

PlayMode *play_mode_list[2] = {&ocp_playmode, 0}, *target_play_mode = &ocp_playmode, *play_mode = &ocp_playmode;

ControlMode *ctl_list[2] = {&ocp_ctl, 0}, *ctl = &ocp_ctl;

static void emulate_main_start(struct timiditycontext_t *c, struct cpifaceSessionAPI_t *cpifaceSession)
{
	const char *configfile;
	c->free_instruments_afterwards = 1;

#ifdef SUPPORT_SOCKET
	init_mail_addr();
	if(c->url_user_agent == NULL)
	{
		c->url_user_agent = (char *)safe_malloc(10 + strlen(timidity_version));
		strcpy(c->url_user_agent, "TiMidity-");
		strcat(c->url_user_agent, timidity_version);
	}
#endif /* SUPPORT_SOCKET */

	timidity_start_initialize(&tc);

	configfile = cpifaceSession->configAPI->GetProfileString ("timidity", "configfile", "");

	if (configfile[0])
	{
		int len = strlen (configfile);
		if (len > 5 && (!strcasecmp (&configfile[len - 4], ".sf2")))
		{
			/* add_soundfont lacks constification of filename */
			add_soundfont (&tc, (char *)configfile, -1, -1, -1, -1);
			goto ready;
		} else {
			/* read_config_file lacks constification of filename */
			if (read_config_file(&tc, (char *)configfile, 0, 0))
			{
				cpifaceSession->cpiDebug (cpifaceSession, "[TiMidity++ MID] Failed to load \"%s\", defaulting to global loading\n", configfile);
			} else {
				c->got_a_configuration = 1;
				goto ready;
			}
		}
	}
	/* (timidity_pre_load_configuration ())
	{
		fprintf (stderr, "[TiMidity++ MID] pre-load config failed\n");
	}*/
	if (!c->got_a_configuration)
	{
		/* test CONFIG_FILE first if it is defined to be something else than one of our standard paths */
		if (strcmp(CONFIG_FILE, "/etc/timidity/timidity.cfg") &&
		    strcmp(CONFIG_FILE, "/etc/timidity.cfg") &&
		    strcmp(CONFIG_FILE, "/usr/local/share/timidity/timidity.cfg") &&
		    strcmp(CONFIG_FILE, "/usr/share/timidity/timidity.cfg") && !read_config_file(&tc, CONFIG_FILE, 0, 0))
		{
			c->got_a_configuration = 1;
		} else if (!read_config_file(&tc, "/etc/timidity/timidity.cfg", 0, 0))
		{
			c->got_a_configuration = 1;
		} else if (!read_config_file(&tc, "/etc/timidity.cfg", 0, 0))
		{
			c->got_a_configuration = 1;
		} else if (!read_config_file(&tc, "/usr/local/share/timidity/timidity.cfg", 0, 0))
		{
			c->got_a_configuration = 1;
		} else if (!read_config_file(&tc, "/usr/share/timidity/timidity.cfg", 0, 0))
		{
		c->got_a_configuration = 1;
		} else {
			cpifaceSession->cpiDebug (cpifaceSession, "[TiMidity++ MID] Warning: Unable to find global timidity.cfg file");
		}

		if (read_user_config_file())
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[TiMidity++ MID] Error: Syntax error in ~/.timidity.cfg");
		}
	}

ready:
	/* we need any emulated configuration, perform them now... */
	if (timidity_post_load_configuration (&tc))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[TiMidity++ MID] post-load config failed\n");
	}

	timidity_init_player (&tc);
}

static void emulate_main_end(struct timiditycontext_t *c)
{
	int i;

#ifdef SUPPORT_SOCKET
	if (c->url_user_agent)
		free(c->url_user_agent);
	if (c->url_http_proxy_host)
		free(c->url_http_proxy_host);
	if (c->url_ftp_proxy_host)
		free(c->url_ftp_proxy_host);
	if (c->user_mailaddr)
		free(c->user_mailaddr);
	c->url_user_agent = NULL;
	c->url_http_proxy_host = NULL;
	c->url_ftp_proxy_host = NULL;
	c->user_mailddr = NULL;
#endif

	if (c->opt_aq_max_buff)
		free(c->opt_aq_max_buff);
	c->opt_aq_max_buff = NULL;

	if (c->opt_aq_fill_buff && !c->opt_aq_fill_buff_free_not_needed)
		free(c->opt_aq_fill_buff);
	c->opt_aq_fill_buff_free_not_needed = 0;
	c->opt_aq_fill_buff = NULL;

	if (c->output_text_code)
		free(c->output_text_code);
	c->output_text_code = NULL;

	free_soft_queue(c);
	free_instruments(c, 0);
	playmidi_stream_free(c);
	free_soundfonts(c);
	free_cache_data(c);
	free_wrd(c);
	free_readmidi(c);
	free_global_mblock(c);
	tmdy_free_config(c);
	free_reverb_buffer(c);
	free_effect_buffers(c);
	free(c->voice); c->voice=0;
	free_gauss_table(c);
	for (i = 0; i < MAX_CHANNELS; i++)
		free_drum_effect(c, i);
}

int emulate_timidity_play_main_start (struct timiditycontext_t *c)
{
	if(wrdt->open(NULL))
	{
		PRINT ("Couldn't open WRD Tracer: %s (`%c')\n", wrdt->name, wrdt->id);
		return 1;
	}

	/* Open output device */
#if 0
	ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
	          "Open output: %c, %s",
	           play_mode->id_character,
	           play_mode->id_name);
#endif

	if (play_mode->flag & PF_PCM_STREAM)
	{
		play_mode->extra_param[1] = aq_calc_fragsize(c);
		ctl->cmsg(CMSG_INFO, VERB_DEBUG_SILLY,
		          "requesting fragment size: %d",
		          play_mode->extra_param[1]);
	}
	if(play_mode->open_output() < 0)
	{
		ctl->cmsg(CMSG_FATAL, VERB_NORMAL,
		          "Couldn't open %s (`%c')",
		          play_mode->id_name, play_mode->id_character);
		ctl->close();
		return 2;
	}

	if(!tc.control_ratio)
	{
	    tc.control_ratio = play_mode->rate / CONTROLS_PER_SECOND;
	    if(tc.control_ratio < 1)
		tc.control_ratio = 1;
	    else if (tc.control_ratio > MAX_CONTROL_RATIO)
		tc.control_ratio = MAX_CONTROL_RATIO;
	}

	init_load_soundfont(&tc);
	aq_setup(&tc);
	timidity_init_aq_buff(&tc);

	if(tc.allocate_cache_size > 0)
		resamp_cache_reset(&tc);

	return 0;
}

static void emulate_timidity_play_main_end (struct timiditycontext_t *c)
{
	// if (intr) aq_flush(1);

	play_mode->close_output();
	ctl->close();
	wrdt->close();

#ifdef SUPPORT_SOUNDSPEC
	if(view_soundspec_flag)
		close_soundspec();
#endif /* SUPPORT_SOUNDSPEC */

	free_archive_files(c);
#ifdef SUPPORT_SOCKET
	url_news_connection_cache(URL_NEWS_CLOSE_CACHE);
#endif /* SUPPORT_SOCKET */
}

static int emulate_play_midi_file_start(const char *fn, uint8_t *data, size_t datalen, struct emulate_play_midi_file_session *s)
{
	int i, j, rc;

	s->first = 1;
	s->event = NULL;

	/* Set current file information */
	tc.current_file_info = get_midi_file_info(&tc, (char *)fn, 1);
	tc.current_file_info->midi_data = (char *)data;
	tc.current_file_info->midi_data_size = datalen;

	rc = check_apply_control(&tc);
	if(RC_IS_SKIP_FILE(rc) && rc != RC_RELOAD)
		return rc;

	/* Reset key & speed each files */
	tc.current_keysig = (tc.opt_init_keysig == 8) ? 0 : tc.opt_init_keysig;
	tc.note_key_offset = tc.key_adjust;
	tc.midi_time_ratio = tc.tempo_adjust;
	for (i = 0; i < MAX_CHANNELS; i++)
	{
		for (j = 0; j < 12; j++)
			tc.channel[i].scale_tuning[j] = 0;
		tc.channel[i].prev_scale_tuning = 0;
		tc.channel[i].temper_type = 0;
	}
	CLEAR_CHANNELMASK(tc.channel_mute);
	if (tc.temper_type_mute & 1)
		FILL_CHANNELMASK(tc.channel_mute);

	/* Reset restart offset */
	tc.midi_restart_time = 0;

#ifdef REDUCE_VOICE_TIME_TUNING
	/* Reset voice reduction stuff */
	tc.min_bad_nv = 256;
	tc.max_good_nv = 1;
	tc.ok_nv_total = 32;
	tc.ok_nv_counts = 1;
	tc.ok_nv = 32;
	tc.ok_nv_sample = 0;
	tc.old_rate = -1;
	tc.reduce_quality_flag = tc.no_4point_interpolation;
	restore_voices(&tc, 0);
#endif

	ctl_mode_event(&tc, CTLE_METRONOME, 0, 0, 0);
	ctl_mode_event(&tc, CTLE_KEYSIG, 0, tc.current_keysig, 0);
	ctl_mode_event(&tc, CTLE_TEMPER_KEYSIG, 0, 0, 0);
	ctl_mode_event(&tc, CTLE_KEY_OFFSET, 0, tc.note_key_offset, 0);
	i = tc.current_keysig + ((tc.current_keysig < 8) ? 7 : -9), j = 0;
	while (i != 7)
		i += (i < 7) ? 5 : -7, j++;
	j += tc.note_key_offset, j -= floor(j / 12.0) * 12;
	tc.current_freq_table = j;
	ctl_mode_event(&tc, CTLE_TEMPO, 0, tc.current_play_tempo, 0);
	ctl_mode_event(&tc, CTLE_TIME_RATIO, 0, 100 / tc.midi_time_ratio + 0.5, 0);
	for (i = 0; i < MAX_CHANNELS; i++)
	{
		ctl_mode_event(&tc, CTLE_TEMPER_TYPE, 0, i, tc.channel[i].temper_type);
		ctl_mode_event(&tc, CTLE_MUTE, 0, i, tc.temper_type_mute & 1);
	}

	return rc;
}

static int emulate_play_midi_start(struct timiditycontext_t *c, MidiEvent *eventlist, int32 samples)
{
	int rc = RC_NONE;

	c->sample_count = samples;
	c->event_list = eventlist;
	c->lost_notes = c->cut_notes = 0;
	c->check_eot_flag = 1;

	wrd_midi_event(&tc, -1, -1); /* For initialize */

	reset_midi (&tc, 0);
#define c (&tc)
	if(!tc.opt_realtime_playing && tc.allocate_cache_size > 0 && !IS_CURRENT_MOD_FILE && (play_mode->flag&PF_PCM_STREAM))
#undef c
	{
		play_midi_prescan(&tc, eventlist);
		reset_midi(&tc, 0);
	}

#if 0
	rc = aq_flush(0);
	if(RC_IS_SKIP_FILE(rc))
		return rc;
#else
	init_effect (&tc);
#endif

	skip_to(&tc, c->midi_restart_time);

	if(c->midi_restart_time > 0)
	{
		/* Need to update interface display */
		int i;
		for(i = 0; i < MAX_CHANNELS; i++)
		{
			redraw_controllers(&tc, i);
		}
	}

	return rc;
}

static int dump_rc(int rc)
{
	switch (rc)
	{
		case RC_ASYNC_HACK:        PRINT ("RC_ASYNC_HACK\n"); break;

		case RC_ERROR:             PRINT ("RC_ERROR\n"); break;
		case RC_NONE:              PRINT ("RC_NONE\n"); break;
		case RC_QUIT:              PRINT ("RC_QUIT\n"); break;
		case RC_NEXT:              PRINT ("RX_NEXT\n"); break;
		case RC_PREVIOUS:          PRINT ("RC_PREVIOUS\n"); break;
		case RC_FORWARD:           PRINT ("RC_FORWARD\n"); break;
		case RC_BACK:              PRINT ("RC_BACK\n"); break;
		case RC_JUMP:              PRINT ("RC_JUMP\n"); break;
		case RC_TOGGLE_PAUSE:      PRINT ("RC_TOGGLE_PAUSE\n"); break;
		case RC_RESTART:           PRINT ("RC_RESTART\n"); break;
		case RC_PAUSE:             PRINT ("RC_PAUSE\n"); break;
		case RC_CONTINUE:          PRINT ("RC_CONTINUE\n"); break;
		case RC_REALLY_PREVIOUS:   PRINT ("RC_REALLY_PREVIOUS\n"); break;
		case RC_CHANGE_VOLUME:     PRINT ("RC_CHANGE_VOLUME\n"); break;
		case RC_LOAD_FILE:         PRINT ("RC_LOAD_FILE\n"); break;
		case RC_TUNE_END:          PRINT ("RC_TUNE_END\n"); break;
		case RC_KEYUP:             PRINT ("RC_KEYUP\n"); break;
		case RC_KEYDOWN:           PRINT ("RC_KEYDOWN\n"); break;
		case RC_SPEEDUP:           PRINT ("RC_SPEEDUP\n"); break;
		case RC_SPEEDDOWN:         PRINT ("RC_SPEEDDOWN\n"); break;
		case RC_VOICEINCR:         PRINT ("RC_VOICEINCR\n"); break;
		case RC_VOICEDECR:         PRINT ("RC_VOICEDECR\n"); break;
		case RC_TOGGLE_DRUMCHAN:   PRINT ("RC_TOGGLE_DRUMCHAN\n"); break;
		case RC_RELOAD:            PRINT ("RC_RELOAD\n"); break;
		case RC_TOGGLE_SNDSPEC:    PRINT ("RC_TOGGLE_SNDSPEC\n"); break;
		case RC_CHANGE_REV_EFFB:   PRINT ("RC_CHANGE_REV_EFFB\n"); break;
		case RC_CHANGE_REV_TIME:   PRINT ("RC_CHANGE_REV_TIME\n"); break;
		case RC_SYNC_RESTART:      PRINT ("RC_SYNC_RESTART\n"); break;
		case RC_TOGGLE_CTL_SPEANA: PRINT ("RC_TOGGLE_CTL_SPEANA\n"); break;
		case RC_CHANGE_RATE:       PRINT ("RC_CHANGE_RATE\n"); break;
		case RC_OUTPUT_CHANGED:    PRINT ("RC_OUTPUT_CHANGED\n"); break;
		case RC_STOP:              PRINT ("RC_STOP\n"); break;
		case RC_TOGGLE_MUTE:       PRINT ("RC_TOGGLE_MUTE\n"); break;
		case RC_SOLO_PLAY:         PRINT ("RC_SOLO_PLAY\n"); break;
		case RC_MUTE_CLEAR:        PRINT ("RC_MUTE_CLEAR\n"); break;
		default:                   PRINT ("RC_UKN_%d\n", rc); break;

	}

	return rc;
}

static int emulate_play_event (struct timiditycontext_t *c, MidiEvent *ev)
{
	int32 cet;

	/* Open Cubic Player always stream, so we skip some checks */

#define c (&tc)
	cet = MIDI_EVENT_TIME(ev);
#undef c

	if(cet > tc.current_sample)
	{
		int32 needed = cet - tc.current_sample;
		int32 canfit = aq_fillable(&tc);

		PRINT ("emulate_play_event, needed=%d canfit=%d -- ", needed, canfit);

		if ((canfit <= 0) || (gmibuffree <= (audio_buffer_size*2)))
		{
			_PRINT ("short exit.. we want more buffer to be free\n");
			return RC_ASYNC_HACK;
		}

		if (needed > canfit)
		{
			int rc;

			_PRINT ("needed > canfit\n");

			rc = compute_data(&tc, canfit);

			ctl_mode_event(&tc, CTLE_REFRESH, 0, 0, 0);
			if (rc == RC_NONE)
				rc = RC_ASYNC_HACK;
			return rc;
		}
	}

	_PRINT ("Allow\n");

	PRINT ("emulate_play_event calling play_event\n");

	return dump_rc(play_event (&tc, ev));
}

static int emulate_play_midi_iterate(struct timiditycontext_t *c)
{
	int rc;

	for(;;)
	{
		if (gmibuffree > (audio_buffer_size*2))
		{
#define c (&tc)
			int32_t cet = MIDI_EVENT_TIME (tc.current_event);
#undef c

			tc.midi_restart_time = 1;

			/* detect if the next event is more that buffer-size away */
			if ((cet > tc.current_sample) && (cet - tc.current_sample) > audio_buffer_size)
			{ /* inject a dummy event to just push audio samples... */
				MidiEvent *pushed_event = tc.current_event;
				MidiEvent dummy[2];
				dummy[0].time = (float)(tc.current_sample + (audio_buffer_size/2))/tc.midi_time_ratio;
				dummy[0].type = ME_NONE;
				dummy[0].channel = 0;
				dummy[0].a = 0;
				dummy[0].b = 0;
				dummy[1].time = dummy[0].time; /* we need this second event, so check_midi_play_end is satisfied */
				dummy[1].type = ME_LAST;
				dummy[1].a = 0;
				dummy[1].b = 0;
				PRINT ("emulate_play_midi_iterate calling FAKED emulate_play_event (estimate samples  %d)\n", MIDI_EVENT_TIME (dummy) - tc.current_sample);
				rc = emulate_play_event (&tc, dummy);
				dump_rc (rc);
				tc.current_event = pushed_event;
				if (rc == RC_NONE)
				{
					rc = RC_ASYNC_HACK;
				}
				break;
			} else {
				PRINT ("emulate_play_midi_iterate calling emulate_play_event\n");
				rc = emulate_play_event(&tc, tc.current_event);
				dump_rc (rc);
				if(rc != RC_NONE)
					break;
				if (tc.midi_restart_time)  /* don't skip the first event if == 0 */
				{
					if (speed != 0x10000)
					{
						int32_t diff = (tc.current_event[1].time - tc.current_sample);
						int32_t newdiff = diff * 0x10000 / speed;
						tc.current_sample += (diff - newdiff);
						PRINT ("tc.current_sample override %d - %d => %d\n", diff, newdiff, diff - newdiff);
					}
					tc.current_event++;
				}
			}
		} else {
			rc = RC_ASYNC_HACK;
			break;
		}

	}

	return rc;
}

static void debug_events(MidiEvent *events)
{
#ifdef PLAYTIMIDITY_DEBUG
	while (events->type != ME_EOT)
	{
		switch (events->type)
		{
			case ME_NONE:                PRINT ("%.8d ME_NONE\n", events->time); break;
			case ME_NOTEOFF:             PRINT ("%.8d ME_NOTEOFF             ch=%d note=%d\n", events->time, events->channel, (int)(events->a)); break;
			case ME_NOTEON:              PRINT ("%.8d ME_NOTEON              ch=%d note=%d velocity=%d\n", events->time, events->channel, (int)(events->a), (uint8_t)(events->b)); break;
			case ME_KEYPRESSURE:         PRINT ("%.8d ME_KEYPRESSURE         ch=%d note=%d pressure=%d\n", events->time, events->channel, (int)(events->a), (int16_t)(events->b)); break;
			case ME_PROGRAM:             PRINT ("%.8d ME_PROGRAM             ch=%d program=%d\n", events->time, events->channel, (int)(events->a)); break;
			case ME_CHANNEL_PRESSURE:    PRINT ("%.8d ME_CHANNEL_PRESSURE    ch=%d pressure=%d\n", events->time, events->channel, (int16_t)(events->a)); break;
			case ME_PITCHWHEEL:          PRINT ("%.8d ME_PITCHWHEEL          ch=%d bend=%d\n", events->time, events->channel, (int)(events->a + events->b * 128)); break;
			case ME_TONE_BANK_MSB:       PRINT ("%.8d ME_TONE_BANK_MSB       ch=%d bank.msb=%d\n", events->time, events->channel, (int8_t)(events->a)); break;
			case ME_TONE_BANK_LSB:       PRINT ("%.8d ME_TONE_BANK_LSB       ch=%d bank.lsb=%d\n", events->time, events->channel, (int8_t)(events->a)); break;
			case ME_MODULATION_WHEEL:    PRINT ("%.8d ME_MODULATION_WHEEL    ch=%d modulation.wheel=%d\n", events->time, events->channel, (int16_t)(events->a)); break;
			case ME_BREATH:              PRINT ("%.8d ME_BREATH ?\n", events->time); break;
			case ME_FOOT:                PRINT ("%.8d ME_FOOT ?\n", events->time); break;
			case ME_MAINVOLUME:          PRINT ("%.8d ME_MAINVOLUME          volume=%d\n", events->time, (int)events->a); break;
			case ME_BALANCE:             PRINT ("%.8d ME_BALANCE ?\n", events->time); break;
			case ME_PAN:                 PRINT ("%.8d ME_PAN                 ch=%d pan=%d\n", events->time, events->channel, (int8_t)(events->a)); break;
			case ME_EXPRESSION:          PRINT ("%.8d ME_EXPRESSION          ch=%d expression=%d\n", events->time, events->channel, (int8_t)(events->a)); break;
			case ME_SUSTAIN:             PRINT ("%.8d ME_SUSTAIN             ch=%d sustain=%d\n", events->time, events->channel, (int8_t)(events->a)); break;
			case ME_PORTAMENTO_TIME_MSB: PRINT ("%.8d ME_PORTAMENTO_TIME_MSB ch=%d portamento_time.msb=%d\n", events->time, events->channel, (uint8_t)(events->a)); break;
			case ME_PORTAMENTO_TIME_LSB: PRINT ("%.8d ME_PORTAMENTO_TIME_LSB ch=%d portamento_time.lsb=%d\n", events->time, events->channel, (uint8_t)(events->a)); break;
			case ME_PORTAMENTO:          PRINT ("%.8d ME_PORTAMENTO          ch=%d enabled=%d value=%d\n", events->time, events->channel, events->a >= 64, (int)events->a); break;
			case ME_PORTAMENTO_CONTROL:  PRINT ("%.8d ME_PORTAMENTO_CONTROL ?\n", events->time); break;
			case ME_DATA_ENTRY_MSB:      PRINT ("%.8d ME_DATA_ENTRY_MSB      ch=%d data_entry.msb=%d\n", events->time, events->channel, (uint8_t)(events->a)); break;
			case ME_DATA_ENTRY_LSB:      PRINT ("%.8d ME_DATA_ENTRY_LSB      ch=%d data_entry.lsb=%d\n", events->time, events->channel, (uint8_t)(events->a)); break;
			case ME_SOSTENUTO:           PRINT ("%.8d ME_SOSTENUTO           ch=%d enabled=%d\n", events->time, events->channel, events->a >= 64); break;
			case ME_SOFT_PEDAL:          PRINT ("%.8d ME_SOFT_PEDAL          ch=%d soft_pedal=%d\n", events->time, events->channel, (int8_t)(events->a)); break;
			case ME_LEGATO_FOOTSWITCH:   PRINT ("%.8d ME_LEGATO_FOOTSWITCH   ch=%d enabled=%d\n", events->time, events->channel, events->a >= 64); break;
			case ME_HOLD2:               PRINT ("%.8d ME_HOLD2 ?\n", events->time);
			case ME_HARMONIC_CONTENT:    PRINT ("%.8d ME_HARMONIC_CONTENT    ch=%d param_resonance=%d\n", events->time, events->channel, (int8_t)(events->a - 64)); break;
			case ME_RELEASE_TIME:        PRINT ("%.8d ME_RELEASE_TIME        ch=%d release_time=%d\n", events->time, events->channel, (int32_t)(events->a)); break;
			case ME_ATTACK_TIME:         PRINT ("%.8d ME_ATTACK_TIME         ch=%d attack_time=%d\n", events->time, events->channel, (int32_t)(events->a)); break;
			case ME_BRIGHTNESS:          PRINT ("%.8d ME_BRIGHTNESS          ch=%d cutoff_freq=%d\n", events->time, events->channel, (int8_t)(events->a - 64)); break;
			case ME_REVERB_EFFECT:       PRINT ("%.8d ME_REVERB_EFFECT       ch=%d reverb_level=%d\n", events->time, events->channel, (int)(events->a)); break;
			case ME_TREMOLO_EFFECT:      PRINT ("%.8d ME_TREMOLO_EFFECT ?    ch=%d tremolo_level=%d\n", events->time, events->channel, (int)(events->a)); break;
			case ME_CHORUS_EFFECT:       PRINT ("%.8d ME_CHORUS_EFFECT       ch=%d chorus_level=%d\n", events->time, events->channel, (int8_t)(events->a)); break;
			case ME_CELESTE_EFFECT:      PRINT ("%.8d ME_CELESTE_EFFECT      ch=%d delay_level=%d\n", events->time, events->channel, (int8_t)(events->a)); break;
			case ME_PHASER_EFFECT:       PRINT ("%.8d ME_PHASER_EFFECT ?     ch=%d phaser_level=%d\n", events->time, events->channel, (int)(events->a)); break;
			case ME_RPN_INC:             PRINT ("%.8d ME_RPN_INC             ch=%d\n", events->time, events->channel); break;
			case ME_RPN_DEC:             PRINT ("%.8d ME_RPN_DEC             ch=%d\n", events->time, events->channel); break;
			case ME_NRPN_LSB:            PRINT ("%.8d ME_NRPN_LSB            ch=%d nrpn.lsb=%d\n", events->time, events->channel, (uint8_t)(events->a)); break;
			case ME_NRPN_MSB:            PRINT ("%.8d ME_NRPN_MSB            ch=%d nrpn.msb=%d\n", events->time, events->channel, (uint8_t)(events->a)); break;
			case ME_RPN_LSB:             PRINT ("%.8d ME_RPN_LSB             ch=%d rpn.lsb=%d\n", events->time, events->channel, (uint8_t)(events->a)); break;
			case ME_RPN_MSB:             PRINT ("%.8d ME_RPN_MSB             ch=%d rpn.msb=%d\n", events->time, events->channel, (uint8_t)(events->a)); break;
			case ME_ALL_SOUNDS_OFF:      PRINT ("%.8d ME_ALL_SOUNDS_OFF      ch=%d\n", events->time, events->channel); break;
			case ME_RESET_CONTROLLERS:   PRINT ("%.8d ME_RESET_CONTROLLERS   ch=%d\n", events->time, events->channel); break;
			case ME_ALL_NOTES_OFF:       PRINT ("%.8d ME_ALL_NOTES_OFF       ch=%d\n", events->time, events->channel); break;
			case ME_MONO:                PRINT ("%.8d ME_MONO                ch=%d\n", events->time, events->channel); break;
			case ME_POLY:                PRINT ("%.8d ME_POLY                ch=%d\n", events->time, events->channel); break;
	/* TiMidity Extensionals */
#if 0
			case ME_VOLUME_ONOFF: /* Not supported */
#endif
			case ME_MASTER_TUNING:       PRINT ("%.8d ME_MASTER_TUNING       ch=%d tuning=%d\n", events->time, events->channel, (int)((events->b << 8) | events->a)); break;
			case ME_SCALE_TUNING:        PRINT ("%.8d ME_SCALE_TUNING        ch=%d scale=%d tuning=%d\n", events->time, events->channel, (int)(events->a), (int8_t)(events->b)); break;
			case ME_BULK_TUNING_DUMP:    PRINT ("%.8d ME_BULK_TUNING_DUMP    part=%d param1=%d param2=%d", events->time, events->channel, (int)(events->a), (int)(events->b)); break;
			case ME_SINGLE_NOTE_TUNING:  PRINT ("%.8d ME_SINGLE_NOTE_TUNING  part=%d param1=%d param2=%d", events->time, events->channel, (int)(events->a), (int)(events->b)); break;
			case ME_RANDOM_PAN:          PRINT ("%.8d ME_RANDOM_PAN          ch=%d\n", events->time, events->channel); break;
			case ME_SET_PATCH:           PRINT ("%.8d ME_SET_PATCH           ch=%d special_sample=%d\n", events->time, events->channel, (uint8_t)(events->a)); break;
			case ME_DRUMPART:            PRINT ("%.8d ME_DRUMPART            ch=%d isrdum=%d\n", events->time, events->channel, !!events->a); break;
			case ME_KEYSHIFT:            PRINT ("%.8d ME_KEYSHIFT            ch=%d keyshift=%d\n", events->time, events->channel, (int)(events->a) - 0x40); break;
			case ME_PATCH_OFFS:          PRINT ("%.8d ME_PATCH_OFFS          ch=%d offset=%d\n", events->time, events->channel, (events->a | 256 * events->b)); break;
			case ME_TEMPO:               PRINT ("%.8d ME_TEMPO               tempo=%d\n", events->time, events->channel + 256 * events->b + 65536 * events->a); break;

			case ME_CHORUS_TEXT:         PRINT ("%.8d ME_CHORUS_TEXT         ref=%d str=\"%s\"\n", events->time, events->a | (int)(events->b << 8), event2string(&tc, events->a | (int)(events->b << 8))); break;
			case ME_LYRIC:               PRINT ("%.8d ME_LYRIC               ref=%d str=\"%s\"\n", events->time, events->a | (int)(events->b << 8), event2string(&tc, events->a | (int)(events->b << 8))); break;
			case ME_MARKER:              PRINT ("%.8d ME_MARKER              ref=%d str=\"%s\"\n", events->time, events->a | (int)(events->b << 8), event2string(&tc, events->a | (int)(events->b << 8))); break;
			case ME_INSERT_TEXT: /* SC */PRINT ("%.8d ME_INSERT_TEXT         ref=%d str=\"%s\"\n", events->time, events->a | (int)(events->b << 8), event2string(&tc, events->a | (int)(events->b << 8))); break;
			case ME_TEXT:                PRINT ("%.8d ME_TEXT                ref=%d str=\"%s\"\n", events->time, events->a | (int)(events->b << 8), event2string(&tc, events->a | (int)(events->b << 8))); break;
			case ME_KARAOKE_LYRIC:/*KAR*/PRINT ("%.8d ME_KARAOKE_LYRIC       ref=%d str=\"%s\"\n", events->time, events->a | (int)(events->b << 8), event2string(&tc, events->a | (int)(events->b << 8))); break;
			case ME_GSLCD:/* GS L.C.D.*/ PRINT ("%.8d ME_GSLCD               ref=%d str=\"%s\"\n", events->time, events->a | (int)(events->b << 8), event2string(&tc, events->a | (int)(events->b << 8))); break;

			case ME_MASTER_VOLUME:       PRINT ("%.8d ME_MASTER_VOLUME       master_volume=%d\n", events->time, (int32_t)events->a + 256 * (int32_t)events->b); break;
			case ME_RESET:               PRINT ("%.8d ME_RESET               mode=%d\n", events->time, (int)(events->a)); break;
			case ME_NOTE_STEP:           PRINT ("%.8d ME_NOTE_STEP           metronome=%d,%d\n", events->time, events->a + ((events->b & 0x0f) << 8), events->b >> 4); break;
			case ME_CUEPOINT:            PRINT ("%.8d ME_CUEPOINT            part=%d param1=%d param2=%d\n", events->time, events->channel, (int)events->a, (int)events->b); break;
			case ME_TIMESIG:             PRINT ("%.8d ME_TIMESIG             %s=%d %s=%d\n", events->time, events->channel==0?"numerator":"midi_clocks_per_metronome_click", events->a, events->channel==0?"denominator":"number_of_notated_32nd_notes_in_MIDI_quarter_note(24 MIDI clocks)", events->b); break;
			case ME_KEYSIG:              PRINT ("%.8d ME_KEYSID              %d %s %s\n", events->time, events->a < 0 ? -events->a : events->a, events->a < 0 ? "flat(s)" : "sharp(s)", events->b ? "minor" : "major"); break;
			case ME_TEMPER_KEYSIG:       PRINT ("%.8d ME_TEMPER_KEYSIG       keysig=%d adjust=%d upper=%d\n", events->time, (events->a + 8) % 32 - 8, (events->a + 8) & 0x20 ? 1 : 0, !!events->b); break;
			case ME_TEMPER_TYPE:         PRINT ("%.8d ME_TEMPER_TYPE         ch=%d temper_type=%d upper=%d\n", events->time, events->channel, (int8_t)events->a, !!events->b); break;
			case ME_MASTER_TEMPER_TYPE:  PRINT ("%.8d ME_MASTER_TEMPER_TYPE  temper_type=%d upper=%d\n", events->time, (int8_t)events->a, !!events->b); break;
			case ME_USER_TEMPER_ENTRY:   PRINT ("%.8d ME_USER_TEMPER_ENTRY   part=%d param1=%d param2=%d\n", events->time, events->channel, events->a, events->b); break;
			case ME_SYSEX_LSB:           PRINT ("%.8d ME_SYSEX_LSB           ch=%d value=%d controller=%d\n", events->time, events->channel, events->a, events->b); break;
			case ME_SYSEX_MSB:           PRINT ("%.8d ME_SYSEX_MSB           ch=%d note=%d (msb=%d)\n", events->time, events->channel, events->a, events->b); break;
			case ME_SYSEX_GS_LSB:        PRINT ("%.8d ME_SYSEX_GS_LSB        ch=%d value=%d controller=%d\n", events->time, events->channel, events->a, events->b); break;
			case ME_SYSEX_GS_MSB:        PRINT ("%.8d ME_SYSEX_GS_MSB        ch=%d note=%d (msb=%d)\n", events->time, events->channel, events->a, events->b); break;
			case ME_SYSEX_XG_LSB:        PRINT ("%.8d ME_SYSEX_XG_LSB        ch=%d value=%d controller=%d\n", events->time, events->channel, events->a, events->b); break;
			case ME_SYSEX_XG_MSB:        PRINT ("%.8d ME_SYSEX_XG_MSB        ch=%d note=%d (msb=%d)\n", events->time, events->channel, events->a, events->b); break;
			case ME_WRD:                 PRINT ("%.8d ME_WRD                 cmd=%d arg=%d\n", events->time, events->channel, events->a | (events->b<<8)); break;
			case ME_SHERRY:              PRINT ("%.8d ME_SHERRY              addr=%d\n", events->time, events->channel | (events->a << 8) | (events->b<<16)); break;
			case ME_BARMARKER:           PRINT ("%.8d ME_BARMARKER ?\n", events->time); break; /* not in use */
			case ME_STEP:                PRINT ("%.8d ME_STEP ?\n", events->time); break; /* not in use */
			case ME_LAST:                PRINT ("%.8d ME_LAST ?\n", events->time); break; /* not in use */
			case ME_EOT:                 PRINT ("%.8d ME_EOT\n", events->time); break;
			default:                     PRINT ("%.8d ?\n", events->time);
		}
		events++;
	}
#endif
}

struct lyric_t lyrics[2];

static void scan_lyrics (struct cpifaceSessionAPI_t *cpifaceSession, const MidiEvent *events)
{
	const MidiEvent *event;

	for (event = events; event->type != ME_EOT; event++)
	{
		if (event->type == ME_KARAOKE_LYRIC)
		{
			const char *t = event2string (&tc, event->a | (int)(event->b << 8));
			if (t && t[0] && ((event->time != 0) || (t[1] != '@')))
			{
				const char *e;
				for (t++; t[0]; t = e)
				{
					char *p, *n;
					e = t + strlen(t);
					if ((p = strchr (t, '/')) /* && (p < e) */)
					{
						e = p + 1;
					}
					if ((n = strchr (t, '\\')) && (n < e))
					{
						e = n + 1;
					}
					if (t[0] == '/')
					{
						karaoke_new_paragraph (&lyrics[0]);
					} else if (t[0] == '\\')
					{
						karaoke_new_line (&lyrics[0]);
					} else {
						karaoke_new_syllable (cpifaceSession, &lyrics[0], event->time, t, e - t);
					}
				}
			}
		}
	}

	for (event = events; event->type != ME_EOT; event++)
	{
		if ((event->type == ME_LYRIC) || (event->type == ME_TEXT) || (event->type == ME_CHORUS_TEXT))
		{
			const char *t = event2string (&tc, event->a | (int)(event->b << 8));
			if (t && t[0] && t[1] != '%' /* chords */)
			{
				const char *e;
				for (t++; t[0]; t = e)
				{
					char *p, *n;
					e = t + strlen(t);
					if ((p = strchr (t, '\n')) /* && (p < e) */)
					{
						e = p + 1;
					}
					if ((n = strchr (t, '\r')) && (n < e))
					{
						e = n + 1;
					}
					if (t[0] == '\n')
					{
						karaoke_new_paragraph (&lyrics[1]);
					} else if (t[0] == '\r')
					{
						karaoke_new_line (&lyrics[1]);
					} else {
						karaoke_new_syllable (cpifaceSession, &lyrics[1], event->time, t, e - t);
					}
				}
			}
		}
	}
}

static int emulate_play_midi_file_iterate (struct cpifaceSessionAPI_t *cpifaceSession, struct timiditycontext_t *c, const char *fn, struct emulate_play_midi_file_session *s)
{
	int i, rc;

	if (s->first)
	{
play_reload: /* Come here to reload MIDI file */
		PRINT ("RELOAD RELOAD RELOAD RC_TUNE_END perhaps?\n");
		s->first = 0;
		rc = play_midi_load_file(&tc, (char *)fn, &s->event, &s->nsamples);
		if (s->event)
		{
			debug_events (s->event);
		}
		scan_lyrics (cpifaceSession, s->event);

		if (lyrics[0].lines && (lyrics[0].lines > lyrics[1].lines))
		{
			cpiKaraokeInit (cpifaceSession, lyrics + 0);
		} else if (lyrics[1].lines)
		{
			cpiKaraokeInit (cpifaceSession, lyrics + 1);
		}

		dump_rc (rc);
		if (RC_IS_SKIP_FILE(rc))
			goto play_end; /* skip playing */

		init_mblock(&c->playmidi_pool);

		ctl_mode_event(&tc, CTLE_PLAY_START, 0, s->nsamples, 0);
		play_mode->acntl(PM_REQ_PLAY_START, NULL);

		rc = emulate_play_midi_start(&tc, s->event, s->nsamples);

		if (rc != RC_NONE)
			return rc;
	}

	rc = emulate_play_midi_iterate(c);

	if (rc == RC_ASYNC_HACK)
		return rc;

	/* shut down */

	play_mode->acntl(PM_REQ_PLAY_END, NULL);
	ctl_mode_event(&tc, CTLE_PLAY_END, 0, 0, 0);
	reuse_mblock(c, &c->playmidi_pool);

	for(i = 0; i < MAX_CHANNELS; i++)
	{
		memset(tc.channel[i].drums, 0, sizeof(tc.channel[i].drums));
	}

play_end:
	free_all_midi_file_info (&tc);

	if(wrdt->opened)
		wrdt->end();

	if(c->free_instruments_afterwards)
	{
		int cnt;
		free_instruments(&tc, 0);
		cnt = free_global_mblock(c); /* free unused memory */
		if(cnt > 0)
		{
			ctl->cmsg(CMSG_INFO, VERB_VERBOSE, "%d memory blocks are free", cnt);
		}
	}

	free_special_patch(c, -1);

	if(s->event != NULL)
	{
		free(s->event);
		s->event = NULL;
	}
	if ((rc == RC_JUMP) || (rc == RC_RELOAD) || ((rc == RC_TUNE_END) && (!donotloop)) )
	{
		goto play_reload;
	} else {
		gmi_eof = 1;
	}

	return rc;
}

static void timidityIdler(struct cpifaceSessionAPI_t *cpifaceSession, struct timiditycontext_t *c)
{
	int rc;

	while (1)
	{
		if (gmi_eof)
			break;

		if (gmibuffree > (audio_buffer_size*2))
		{
			rc = emulate_play_midi_file_iterate (cpifaceSession, &tc, current_path, &timidity_main_session);
			if (rc == RC_ASYNC_HACK)
				break;
		} else {
			break;
		}
	}
}

int __attribute__ ((visibility ("internal"))) timidityIsLooped(void)
{
	return gmi_looped && gmi_eof;
}

void __attribute__ ((visibility ("internal"))) timiditySetLoop(unsigned char s)
{
	donotloop=!s;
}

void __attribute__ ((visibility ("internal"))) timidityPause(unsigned char p)
{
	gmi_inpause=p;
}

void __attribute__ ((visibility ("internal"))) timidityMute (struct cpifaceSessionAPI_t *cpifaceSession, int ch, int m)
{
	//sync_restart (&tc, 0);
	cpifaceSession->MuteChannel[ch] = m;
	if (m)
	{
		SET_CHANNELMASK(tc.channel_mute, ch);
	} else {
		UNSET_CHANNELMASK(tc.channel_mute, ch);
	}
	//ctl_mode_event (&tc, CTLE_MUTE, 0, ch, m);
}

static void timiditySetVolume(void)
{
	volr=voll=vol*4;
	if (bal<0)
		voll=(voll*(64+bal))>>6;
	else
		volr=(volr*(64-bal))>>6;
}

static void timiditySet (struct cpifaceSessionAPI_t *cpifaceSession, int ch, int opt, int val)
{
	switch (opt)
	{
		case mcpMasterSpeed:
			if (val < 4)
				val = 4;
			dspeed = val * 0x100;
			speed = (float)dspeed * ((float)0x10000 / gmibufrate);
			PRINT ("#1  dspeed=0x%08x gmibufrate=0x%08x speed=0x%08x\n", dspeed, gmibufrate, speed);
			break;
		case mcpMasterPitch:
			if (val < 4)
				val = 4;
			gmibufrate = val * 0x100;
			speed = (float)dspeed * ((float)0x10000 / gmibufrate);
			PRINT ("#2  dspeed=0x%08x gmibufrate=0x%08x speed=0x%08x\n", dspeed, gmibufrate, speed);
			break;
		case mcpMasterSurround:
			srnd=val;
			break;
		case mcpMasterPanning:
			pan=val;
			timiditySetVolume();
			break;
		case mcpMasterVolume:
			vol=val;
			timiditySetVolume();
			break;
		case mcpMasterBalance:
			bal=val;
			timiditySetVolume();
			break;
	}
}

static int timidityGet (struct cpifaceSessionAPI_t *cpifaceSession, int ch, int opt)
{
	return 0;
}

void __attribute__ ((visibility ("internal"))) timidityGetGlobInfo(struct mglobinfo *gi)
{
	int32_t curtick = tc.current_sample
			- aq_soft_filled(&tc)
			- (samples_committed - samples_lastui);
	if (curtick < 0)
	{
		curtick = 0;
	}
	gi->curtick = curtick;
	gi->ticknum = timidity_main_session.nsamples;
}

void __attribute__ ((visibility ("internal"))) timidityIdle(struct cpifaceSessionAPI_t *cpifaceSession)
{
	if (clipbusy++)
	{
		clipbusy--;
		return;
	}

	if (gmi_inpause || (gmi_looped && gmi_eof))
	{
		cpifaceSession->plrDevAPI->Pause (1);
	} else {
		void *targetbuf;
		unsigned int targetlength; /* in samples */

		cpifaceSession->plrDevAPI->Pause (0);

		cpifaceSession->plrDevAPI->GetBuffer (&targetbuf, &targetlength);

		if (targetlength)
		{
			int16_t *t = targetbuf;
			unsigned int accumulated_target = 0;
			unsigned int accumulated_source = 0;
			int pos1, length1, pos2, length2;

			timidityIdler (cpifaceSession, &tc);

			/* how much data is available.. we are using a ringbuffer, so we might receive two fragments */
			cpifaceSession->ringbufferAPI->get_tail_samples (gmibufpos, &pos1, &length1, &pos2, &length2);

			if (gmibufrate==0x10000)
			{
				if (targetlength>(length1+length2))
				{
					targetlength=(length1+length2);
					gmi_looped |= 2;
				} else {
					gmi_looped &= ~2;
				}

				// limit source to not overrun target buffer
				if (length1 > targetlength)
				{
					length1 = targetlength;
					length2 = 0;
				} else if ((length1 + length2) > targetlength)
				{
					length2 = targetlength - length1;
				}

				accumulated_source = accumulated_target = length1 + length2;

				while (length1)
				{
					while (length1)
					{
						int16_t rs, ls;

						rs = gmibuf[(pos1<<1) + 0];
						ls = gmibuf[(pos1<<1) + 1];

						PANPROC;

						*(t++) = rs;
						*(t++) = ls;

						pos1++;
						length1--;

						//accumulated_target++;
					}
					length1 = length2;
					length2 = 0;
					pos1 = pos2;
					pos2 = 0;
				}
				//accumulated_source = accumulated_target;
			} else {
				/* We are going to perform cubic interpolation of rate conversion... this bit is tricky */
				gmi_looped &= ~2;

				while (targetlength && length1)
				{
					while (targetlength && length1)
					{
						uint32_t wpm1, wp0, wp1, wp2;
						int32_t rc0, rc1, rc2, rc3, rvm1,rv1,rv2;
						int32_t lc0, lc1, lc2, lc3, lvm1,lv1,lv2;
						unsigned int progress;
						int16_t rs, ls;

						/* will the interpolation overflow? */
						if ((length1+length2) <= 3)
						{
							gmi_looped |= 2;
							break;
						}
						/* will we overflow the wavebuf if we advance? */
						if ((length1+length2) < ((gmibufrate+gmibuffpos)>>16))
						{
							gmi_looped |= 2;
							break;
						}

						switch (length1) /* if we are close to the wrap between buffer segment 1 and 2, len1 will grow down to a small number */
						{
							case 1:  wpm1 = pos1; wp0 = pos2;     wp1 = pos2 + 1; wp2 = pos2 + 2; break;
							case 2:  wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos2;     wp2 = pos2 + 1; break;
							case 3:  wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos1 + 2; wp2 = pos2;     break;
							default: wpm1 = pos1; wp0 = pos1 + 1; wp1 = pos1 + 2; wp2 = pos1 + 3; break;
						}
						rvm1 = (uint16_t)gmibuf[(wpm1<<1)+0]^0x8000; /* we temporary need data to be unsigned - hence the ^0x8000 */
						lvm1 = (uint16_t)gmibuf[(wpm1<<1)+1]^0x8000;
						 rc0 = (uint16_t)gmibuf[(wp0 <<1)+0]^0x8000;
						 lc0 = (uint16_t)gmibuf[(wp0 <<1)+1]^0x8000;
						 rv1 = (uint16_t)gmibuf[(wp1 <<1)+0]^0x8000;
						 lv1 = (uint16_t)gmibuf[(wp1 <<1)+1]^0x8000;
						 rv2 = (uint16_t)gmibuf[(wp2 <<1)+0]^0x8000;
						 lv2 = (uint16_t)gmibuf[(wp2 <<1)+1]^0x8000;

						rc1 = rv1-rvm1;
						rc2 = 2*rvm1-2*rc0+rv1-rv2;
						rc3 = rc0-rvm1-rv1+rv2;
						rc3 =  imulshr16(rc3,gmibuffpos);
						rc3 += rc2;
						rc3 =  imulshr16(rc3,gmibuffpos);
						rc3 += rc1;
						rc3 =  imulshr16(rc3,gmibuffpos);
						rc3 += rc0;
						if (rc3<0)
							rc3=0;
						if (rc3>65535)
							rc3=65535;

						lc1 = lv1-lvm1;
						lc2 = 2*lvm1-2*lc0+lv1-lv2;
						lc3 = lc0-lvm1-lv1+lv2;
						lc3 =  imulshr16(lc3,gmibuffpos);
						lc3 += lc2;
						lc3 =  imulshr16(lc3,gmibuffpos);
						lc3 += lc1;
						lc3 =  imulshr16(lc3,gmibuffpos);
						lc3 += lc0;
						if (lc3<0)
							lc3=0;
						if (lc3>65535)
							lc3=65535;

						rs = rc3 ^ 0x8000;
						ls = lc3 ^ 0x8000;

						PANPROC;

						*(t++) = rs;
						*(t++) = ls;

						gmibuffpos+=gmibufrate;
						progress = gmibuffpos>>16;
						gmibuffpos &= 0xffff;
						accumulated_source+=progress;
						pos1+=progress;
						length1-=progress;
						targetlength--;

						accumulated_target++;
					} /* while (targetlength && length1) */
					length1 = length2;
					length2 = 0;
					pos1 = pos2;
					pos2 = 0;
				} /* while (targetlength && length1) */
			} /* if (gmibufrate==0x10000) */
			cpifaceSession->ringbufferAPI->tail_consume_samples (gmibufpos, accumulated_source);
			timidity_play_source_EventDelayed_gmibuf (cpifaceSession, accumulated_source, accumulated_target);
			cpifaceSession->plrDevAPI->CommitBuffer (accumulated_target);
			samples_committed += accumulated_target;
			gmibuffill-=accumulated_source;
			gmibuffree+=accumulated_source;
		} /* if (targetlength) */
	}

	{
		uint64_t delay = cpifaceSession->plrDevAPI->Idle();
		uint64_t new_ui = samples_committed - delay;
		if (new_ui > samples_lastui)
		{
			timidity_play_target_EventDelayed_gmibuf (cpifaceSession, new_ui - samples_lastui);
			samples_lastui = new_ui;
		}
		samples_lastdelay = delay;
	}

	clipbusy--;
}

static void doTimidityClosePlayer(struct cpifaceSessionAPI_t *cpifaceSession, int CloseDriver)
{
	if (CloseDriver && cpifaceSession->plrDevAPI)
	{
		cpifaceSession->plrDevAPI->Stop (cpifaceSession);
	}

	free(gmibuf);
	gmibuf = 0;

	emulate_timidity_play_main_end (&tc);

	emulate_main_end (&tc);

	free (timidity_main_session.event);
	timidity_main_session.event = NULL;

	free (current_path);
	current_path = 0;

	while (EventDelayed_PlrBuf_head)
	{
		CtlEventDelayed *next = EventDelayed_PlrBuf_head->next;
		free_EventDelayed (EventDelayed_PlrBuf_head);
		EventDelayed_PlrBuf_head = next;
	}
	EventDelayed_PlrBuf_tail = 0;

	while (EventDelayed_gmibuf_head)
	{
		CtlEventDelayed *next = EventDelayed_gmibuf_head->next;
		free_EventDelayed (EventDelayed_gmibuf_head);
		EventDelayed_gmibuf_head = next;
	}
	EventDelayed_gmibuf_tail = 0;

	if (gmibufpos)
	{
		cpifaceSession->ringbufferAPI->free (gmibufpos);
		gmibufpos = 0;
	}

	free_all_midi_file_info (&tc);
}

int __attribute__ ((visibility ("internal"))) timidityOpenPlayer(const char *path, uint8_t *buffer, size_t bufferlen, struct ocpfilehandle_t *file, struct cpifaceSessionAPI_t *cpifaceSession)
{
	uint32_t gmibuflen;
	enum plrRequestFormat format;

	bzero (&tc, sizeof (tc));
	tc.contextowner = cpifaceSession;
#ifdef ALWAYS_TRACE_TEXT_META_EVENT
	tc.opt_trace_text_meta_event = 1;
#endif
	tc.ignore_midi_error = 1;
	tc.play_system_mode = DEFAULT_SYSTEM_MODE;
	tc.xg_reverb_type_msb = 0x01;
	tc.xg_chorus_type_msb = 0x41;
	tc.readmidi_read_init_first = 1;
	tc.opt_default_module = MODULE_TIMIDITY_DEFAULT;
	tc.amplification = DEFAULT_AMPLIFICATION;
	tc.adjust_panning_immediately = 1;
	tc.max_voices = DEFAULT_VOICES;
	tc.voices = DEFAULT_VOICES;
	tc.midi_time_ratio = 1.0;
#ifdef MODULATION_WHEEL_ALLOW
	tc.opt_modulation_wheel = 1;
#endif
#ifdef PORTAMENTO_ALLOW
	tc.opt_portamento = 1;
#endif
#ifdef NRPN_VIBRATO_ALLOW
	tc.opt_nrpn_vibrato = 1;
#endif
#ifdef REVERB_CONTROL_ALLOW
	tc.opt_reverb_control = 1;
#else
# ifdef FREEVERB_CONTROL_ALLOW
	tc.opt_reverb_control = 3;
# endif
#endif
#ifdef CHORUS_CONTROL_ALLOW
	tc.opt_chorus_control = 1;
#endif
#ifdef SURROUND_CHORUS_ALLOW
	tc.opt_surround_chorus = 1;
#endif
#ifdef GM_CHANNEL_PRESSURE_ALLOW
	tc.opt_channel_pressure = 1;
#endif
#ifdef VOICE_CHAMBERLIN_LPF_ALLOW
	tc.opt_lpf_def = 1;
#else
#ifdef VOICE_MOOG_LPF_ALLOW
	tc.opt_lpf_def = 2;
#endif /* VOICE_MOOG_LPF_ALLOW */
#endif /* VOICE_CHAMBERLIN_LPF_ALLOW */
#ifdef OVERLAP_VOICE_ALLOW
	tc.opt_overlap_voice_allow = 1;
#endif
#ifdef TEMPER_CONTROL_ALLOW
	tc.opt_temper_control = 1;
#endif
	tc.noise_sharp_type = 4;
	tc.special_tonebank = -1;
	tc.effect_lr_mode = -1;
	tc.effect_lr_delay_msec = 25;
	tc.opt_init_keysig = 8;
	tc.opt_force_keysig = 8;
	tc.tempo_adjust = 1.0;
	tc.opt_drum_power = 100;
	tc.opt_buffer_fragments = -1;
	tc.stream_max_compute = 500;
	tc.master_volume_ratio = 0xffff;
	tc.compensation_ratio = 1.0;
#ifdef DEFAULT_PATH
	tc.defaultpathlist.path = DEFAULT_PATH;
	tc.pathlist = &tc.defaultpathlist;
#endif
	tc.open_file_noise_mode = OF_NORMAL;
	tc.tonebank[0] = &tc.standard_tonebank;
	tc.drumset[0] = &tc.standard_drumset;
#ifdef FAST_DECAY
	tc.fast_decay = 1;
#endif
	tc.opt_sf_close_each_file = SF_CLOSE_EACH_FILE;
	tc.min_sustain_time = 5000;
	tc.allocate_cache_size = DEFAULT_CACHE_DATA_SIZE;
	tc.gauss_n = DEFAULT_GAUSS_ORDER;
	tc.newt_n = 11;
	tc.newt_old_trunc_x = -1;
	tc.newt_grow = -1;
	tc.newt_max = 13;
#ifndef FIXED_RESAMPLATION
	tc.cur_resample = DEFAULT_RESAMPLATION;
#endif
	tc.reverb_predelay_factor = 1.0;
	tc.REV_INP_LEV = 1.0;
	memcpy (tc.layer_items, layer_items_default, sizeof (layer_items_default));
	tc.vol_table = tc.def_vol_table;
	tc.xg_vol_table = tc.gs_vol_table;
	tc.pan_table = sc_pan_table;
#ifdef LOOKUP_HACK
	tc._l2u = _l2u_ + 4096;
#endif
	tc.mimpi_bug_emulation_level = MIMPI_BUG_EMULATION_LEVEL;
	tc.trace_loop_lasttime = -1;
	tc.mag01[1] = MATRIX_A;
	tc.def_prog = -1;
	tc.audio_buffer_bits = DEFAULT_AUDIO_BUFFER_BITS;
#ifdef REDUCE_VOICE_TIME_TUNING
	tc.restore_voices_old_voices = -1;
#endif
	tc.ctl_timestamp_last_secs = -1;
	tc.ctl_timestamp_last_voices = -1;
	tc.play_midi_file_last_rc = RC_NONE;

	if (!cpifaceSession->plrDevAPI)
	{
		return errPlay;
	}

	loading = 1;

	samples_committed = 0;
	samples_lastui = 0;
	samples_lastdelay = 0;

	gmiRate=0;
	format=PLR_STEREO_16BIT_SIGNED;
	if (!cpifaceSession->plrDevAPI->Play (&gmiRate, &format, file, cpifaceSession))
	{
		return errPlay;
	}
	ocp_playmode.rate = gmiRate;

	emulate_main_start(&tc, cpifaceSession);
	gmi_inpause=0;
	voll=256;
	volr=256;
	pan=64;
	srnd=0;
	speed=0x100;
	loading = 0;

	gmibuffree=1;
#define c (&tc)
	while ((gmibuffree < (gmiRate/8+1)) && (gmibuffree < (audio_buffer_size*3)))
	{
		gmibuffree<<=1;
	}
	gmibuflen=gmibuffree + ((gmiRate*4/audio_buffer_size)+1)*audio_buffer_size; /* timidity force-flushes multiple seconds at the EOF */
#undef c
	gmibuffill=0;
	gmibuf=malloc(gmibuflen << 2 /* stereo + 16bit */);
	if (!gmibuf)
	{
		return errAllocMem;
	}
	gmibufpos = cpifaceSession->ringbufferAPI->new_samples (RINGBUFFER_FLAGS_STEREO | RINGBUFFER_FLAGS_16BIT | RINGBUFFER_FLAGS_SIGNED, gmibuflen);
	if (!gmibufpos)
	{
		return errAllocMem;
	}
	gmibuffpos=0;

	gmi_looped=0;
	gmi_eof=0;

	if (emulate_timidity_play_main_start (&tc))
	{
		return errGen;
	}

	current_path = strdup (path);
	emulate_play_midi_file_start(current_path, buffer, bufferlen, &timidity_main_session); /* gmibuflen etc must be set, since we will start to get events already here... */

	cpifaceSession->mcpSet = timiditySet;
	cpifaceSession->mcpGet = timidityGet;

	cpifaceSession->Normalize (cpifaceSession, mcpNormalizeNoFilter | mcpNormalizeCanSpeedPitchUnlock | mcpNormalizeCannotEcho | mcpNormalizeCannotAmplify);

	timidityIdler (cpifaceSession, &tc); /* trigger the file to load as soon as possible */
	return errOk;
}

void __attribute__ ((visibility ("internal"))) timidityClosePlayer(struct cpifaceSessionAPI_t *cpifaceSession)
{
#warning we need to break idle loop with EVENT set to quit, in order to make a clean exit..
	doTimidityClosePlayer (cpifaceSession, 1);

	cpiKaraokeDone (cpifaceSession);

	karaoke_clear (&lyrics[0]);
	karaoke_clear (&lyrics[1]);
}
