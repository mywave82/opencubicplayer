/** OpenCP Module Player
 * copyright (c) '94-'21 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * ALSA (Advanced Linux Sound Architecture) Player device
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
#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
#include <alsa/asoundlib.h>
#include <alsa/mixer.h>
#include <alsa/pcm.h>
#include <alsa/pcm_plugin.h>
#include <string.h>
#include "types.h"
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "cpiface/vol.h"
#include "dev/imsdev.h"
#include "dev/player.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/filesystem-drive.h"
#include "filesel/filesystem-file-mem.h"
#include "filesel/filesystem-setup.h"
#include "filesel/mdb.h"
#include "filesel/modlist.h"
#include "filesel/pfilesel.h"
#include "stuff/framelock.h"
#include "stuff/imsrtns.h"
#include "stuff/poutput.h"

//#define ALSA_DEBUG_OUTPUT 1
//#define ALSA_DEBUG 1
#ifdef ALSA_DEBUG_OUTPUT
static int debug_output = -1;
#endif
#ifdef ALSA_DEBUG
#define debug_printf(...) fprintf (stderr, __VA_ARGS__)
#else
#define debug_printf(format, args...) ((void)0)
#endif

static struct interfacestruct alsaPCMoutIntr;

static snd_pcm_t *alsa_pcm = NULL;
static snd_mixer_t *mixer = NULL;

static snd_pcm_status_t *alsa_pcm_status=NULL;
#if SND_LIB_VERSION < 0x01000e
#error Minimum version of libasound2 is 1.0.14
#endif

static snd_pcm_hw_params_t *hwparams = NULL;
static snd_pcm_sw_params_t *swparams = NULL;

struct sounddevice plrAlsa;
/*static struct deviceinfo currentcard;*/
static struct ocpdir_t dir_alsa;

static void alsaOpenDevice(void);

static char alsaCardName[DEVICE_NAME_MAX+1];
static char alsaMixerName[DEVICE_NAME_MAX+1];

/* stolen from devposs */
#define MAX_ALSA_MIXER 256
static char *playbuf;
static int buflen; /* in bytes */
static volatile int kernpos, cachepos, bufpos; /* in bytes */
static volatile int cachelen, kernlen; /* to make life easier */
static struct ocpvolstruct mixer_entries[MAX_ALSA_MIXER];
static int alsa_mixers_n=0;
/*  playbuf     kernpos  cachepos   bufpos      buflen
 *    |           | kernlen | cachelen |          |
 *
 *  on flush, we update all variables> *  on getbufpos we return kernpos-(1 sample) as safe point to buffer up to
 *  on getplaypos we use last known kernpos if locked, else update kernpos
 */

static volatile uint32_t playpos; /* how many samples have we done totally */
static int stereo;
static int bit16;

static volatile int busy=0;

#warning fix-me!!!!
uint32_t custom_dsp_mdb_ref=0xffffffff;
uint32_t custom_mixer_mdb_ref=0xffffffff;


static int mlDrawBox(void)
{
	int mlTop=plScrHeight/2-2;
	unsigned int i;

	displayvoid(mlTop+1, 5, plScrWidth-10);
	displayvoid(mlTop+2, 5, plScrWidth-10);
	displayvoid(mlTop+3, 5, plScrWidth-10);
	displaystr(mlTop, 4, 0x04, "\xda", 1);
	for (i=5;i<(plScrWidth-5);i++)
		displaystr(mlTop, i, 0x04, "\xc4", 1);
	displaystr(mlTop, plScrWidth-5, 0x04, "\xbf", 1);
	displaystr(mlTop+1, 4, 0x04, "\xb3", 1);
	displaystr(mlTop+2, 4, 0x04, "\xb3", 1);
	displaystr(mlTop+3, 4, 0x04, "\xb3", 1);
	displaystr(mlTop+1, plScrWidth-5, 0x04, "\xb3", 1);
	displaystr(mlTop+2, plScrWidth-5, 0x04, "\xb3", 1);
	displaystr(mlTop+3, plScrWidth-5, 0x04, "\xb3", 1);
	displaystr(mlTop+4, 4, 0x04, "\xc0", 1);
	for (i=5;i<(plScrWidth-5);i++)
		displaystr(mlTop+4, i, 0x04, "\xc4", 1);
	displaystr(mlTop+4, plScrWidth-5, 0x04, "\xd9", 1);

	return mlTop;
}
/* stolen from devposs */
static int getbufpos(void)
{
	int retval;

	if (busy++)
	{
		/* can't escape if already set, and shouldn't happen */
	}

	if (kernpos==bufpos)
	{
		if (cachelen|kernlen)
		{
			retval=kernpos;
			busy--;
			return retval;
		}
	}
/*
	if ((!cachelen)&&(!kernlen))
		retval=(kernpos+buflen-(0<<(bit16+stereo)))%buflen;
	else*/
		retval=(kernpos+buflen-(1<<(bit16+stereo)))%buflen;
	busy--;
	return retval;
}
/* more or less stolen from devposs */
static int getplaypos(void)
{
	int retval;

	if (busy++)
	{
	} else {
		snd_pcm_sframes_t tmp;
		int err;

		debug_printf("ALSA_getplaypos()\n");

		err=snd_pcm_status(alsa_pcm, alsa_pcm_status);
		debug_printf("      snd_pcm_status(alsa_pcm, alsa_pcm_status) = %s\n", snd_strerror(-err));

		if (err < 0)
		{
			fprintf(stderr, "ALSA: snd_pcm_status() failed: %s\n", snd_strerror(-err));
		} else {
			tmp=snd_pcm_status_get_delay(alsa_pcm_status);

			debug_printf("      snd_pcm_status_get_delay(alsa_pcm_status = %d\n", (int)tmp);
			if (tmp<0)
			{ /* we ignore buffer-underruns */
				tmp=0;
			} else if (tmp==0)
			{ /* ALSA sometimes (atlast on Stians Ubuntu laptop) gives odelay==0 always */
				snd_pcm_sframes_t tmp1 = snd_pcm_status_get_avail_max(alsa_pcm_status);
				snd_pcm_sframes_t tmp2 = snd_pcm_status_get_avail(alsa_pcm_status);
				tmp = tmp1 - tmp2;

				debug_printf("      (no delay available) fallback:\n");
				debug_printf("      snd_pcm_status_get_avail_max() = %d\n", (int)tmp1);
				debug_printf("      snd_pcm_status_get_avail() = %d\n", (int)tmp2);
				debug_printf("      => %d\n", (int)tmp);

				if (tmp<0)
				{
					tmp=0;
				}
			}
			tmp<<=(bit16+stereo);

			if (tmp<kernlen)
			{
				kernlen=tmp;
			}
			kernpos=(cachepos-kernlen+buflen)%buflen;
		}
	}
	retval=kernpos;
	busy--;

	debug_printf(" => %d\n", retval);

	return retval;
}

/* more or less stolen from devposs */
static void flush(void)
{
	int result, n, odelay, tmp;
	int err;

	if (busy++)
	{
		busy--;
		return;
	}

	debug_printf("ALSA_flush()\n");

	err=snd_pcm_status(alsa_pcm, alsa_pcm_status);
	debug_printf("      snd_pcm_status(alsa_pcm, alsa_pcm_status) = %s\n", snd_strerror(-err));
	if (err<0)
	{
		fprintf(stderr, "ALSA: snd_pcm_status() failed: %s\n", snd_strerror(-err));
		busy--;
		return;
	}

	odelay=snd_pcm_status_get_delay(alsa_pcm_status);
	debug_printf("      snd_pcm_status_get_delay(alsa_pcm_status) = %d\n", odelay);

	if (odelay<0)
	{ /* we ignore buffer-underruns */
		odelay=0;
	} else if (odelay==0)
	{ /* ALSA sometimes (atlast on Stians Ubuntu laptop) gives odelay==0 always */
		snd_pcm_sframes_t tmp1 = snd_pcm_status_get_avail_max(alsa_pcm_status);
		snd_pcm_sframes_t tmp2 = snd_pcm_status_get_avail(alsa_pcm_status);
		odelay = tmp1 - tmp2;

		debug_printf("      (no delay available) fallback:\n");
		debug_printf("      snd_pcm_status_get_avail_max() = %d\n", (int)tmp1);
		debug_printf("      snd_pcm_status_get_avail() = %d\n", (int)tmp2);
		debug_printf("      => %d\n", (int)odelay);

		if (odelay<0)
		{
			odelay=0;
		}
	}
	odelay<<=(bit16+stereo);

	if (odelay>kernlen)
	{
		odelay=kernlen;
	} else if ((odelay<kernlen))
	{
		kernlen=odelay;
		kernpos=(cachepos-kernlen+buflen)%buflen;
	}

	if (!cachelen)
	{
		busy--;
		return;
	}

	if (bufpos<=cachepos)
		n=buflen-cachepos;
	else
		n=bufpos-cachepos;

	/* TODO, check kernel-size
	if (n>info.bytes)
		n=info.bytes;
	*/

	if (n<=0)
	{
		busy--;
		return;
	}

	tmp = snd_pcm_status_get_avail(alsa_pcm_status);

	debug_printf ("      snd_pcm_status_get_avail() = %d\n", tmp);

	tmp = tmp << (bit16+stereo);

	if (n > tmp)
	{
		n = tmp;
	}
	result=snd_pcm_writei(alsa_pcm, playbuf+cachepos, n>>(bit16+stereo));

	debug_printf ("      snd_pcm_writei (%d) = %d\n", n>>(bit16+stereo), result);

#ifdef ALSA_DEBUG_OUTPUT
	if (result > 0)
	{
		if (debug_output >= 0)
		{
			write (debug_output, playbuf+cachepos, result<<(bit16+stereo));
		}
	}
#endif
	if (result<0)
	{
		if (result==-EPIPE)
		{
			fprintf(stderr, "ALSA: Machine is too slow, calling snd_pcm_prepare()\n");
			snd_pcm_prepare(alsa_pcm); /* TODO, can this fail? */
			debug_printf ("      snd_pcm_prepare()\n");
		}
		busy--;
		return;
	}
	result<<=(bit16+stereo);
	cachepos=(cachepos+result+buflen)%buflen;
	playpos+=result;
	cachelen-=result;
	kernlen+=result;

	busy--;
}
/* stolen from devposs */
static void advance(unsigned int pos)
{
	debug_printf ("ALSA_advance: oldpos=%d newpos=%d add=%d (busy=%d)\n", bufpos, pos, (pos-bufpos+buflen)%buflen, busy);

	busy++;

	cachelen+=(pos-bufpos+buflen)%buflen;
	bufpos=pos;

	debug_printf ("         cachelen=%d kernlen=%d sum=%d len=%d\n", cachelen, kernlen, cachelen+kernlen, buflen);

	busy--;
}
/* stolen from devposs */
static uint32_t gettimer(void)
{
	long tmp=playpos;
	int odelay;

	if (busy++)
	{
		odelay=kernlen;
	} else {
		int err;

		debug_printf("ALSA_gettimer()");

		err=snd_pcm_status(alsa_pcm, alsa_pcm_status);

		debug_printf ("      snd_pcm_status(alsa_pcm, alsa_pcm_status) = %s\n", snd_strerror(-err));

		if (err<0)
		{
			fprintf(stderr, "ALSA: snd_pcm_status() failed: %s\n", snd_strerror(-err));
			odelay=kernlen;
		} else {

			odelay=snd_pcm_status_get_delay(alsa_pcm_status);
			debug_printf ("      snd_pcm_status_get_delay() = %d\n", odelay);

			if (odelay<0)
			{ /* we ignore buffer-underruns */
				odelay=0;
			} else if (odelay==0)
			{ /* ALSA sometimes (atlast on Stians Ubuntu laptop) gives odelay==0 always */
				snd_pcm_sframes_t tmp1 = snd_pcm_status_get_avail_max(alsa_pcm_status);
				snd_pcm_sframes_t tmp2 = snd_pcm_status_get_avail(alsa_pcm_status);
				odelay = tmp1 - tmp2;

				debug_printf("      (no delay available) fallback:\n");
				debug_printf("      snd_pcm_status_get_avail_max() = %d\n", (int)tmp1);
				debug_printf("      snd_pcm_status_get_avail() = %d\n", (int)tmp2);
				debug_printf("      => %d\n", (int)odelay);

				if (odelay<0)
				{
					odelay=0;
				}
			}

			odelay<<=(bit16+stereo);
			if (odelay>kernlen)
			{
				odelay=kernlen;
			} else if ((odelay<kernlen))
			{
				kernlen=odelay;
				kernpos=(cachepos-kernlen+buflen)%buflen;
			}
		}
	}

	tmp-=odelay;
	busy--;
	return imuldiv(tmp, 65536>>(stereo+bit16), plrRate);
}

static void dir_alsa_ref (struct ocpdir_t *self)
{
	debug_printf ("dir_alsa_ref() (dummy)\n");
}
static void dir_alsa_unref (struct ocpdir_t *self)
{
	debug_printf ("dir_alsa_unref() (dummy)\n");
}

struct dirhandle_alsa_t
{
	struct ocpdir_t *owner;
	void *token;
	void(*callback_file)(void *token, struct ocpfile_t *);

	int i;
	int count;
	void **hints;
};

static ocpdirhandle_pt dir_alsa_readdir_start (struct ocpdir_t *self, void(*callback_file)(void *token, struct ocpfile_t *),
                                                                      void(*callback_dir )(void *token, struct ocpdir_t *), void *token)
{
	int result;
	struct dirhandle_alsa_t *retval = calloc (1, sizeof *retval);

	if (!retval)
	{
		return 0;
	}

	debug_printf ("dir_alsa_readdir_start()\n");

	result=snd_device_name_hint(-1, "pcm", &retval->hints);
	debug_printf ("      snd_device_name_hint() = %s\n", snd_strerror(-result));
	if (result)
	{
		free (retval); /* zero cards found */
		return 0;
	}

	retval->owner = self;
	retval->callback_file = callback_file;
	retval->token = token;
	retval->count = 1;

	return retval;
}

static void dir_alsa_readdir_cancel (ocpdirhandle_pt _handle)
{
	struct dirhandle_alsa_t *handle = (struct dirhandle_alsa_t *)_handle;

	debug_printf ("dir_alsa_readdir_cancel()\n");

	snd_device_name_free_hint (handle->hints);
	free (handle);
}

static void dir_alsa_update_mdb (uint32_t dirdb_ref, const char *name, const char *descr)
{
	uint32_t mdb_ref;
	struct moduleinfostruct mi;

	mdb_ref = mdbGetModuleReference2 (dirdb_ref, strlen (alsaPCMoutIntr.name));
	debug_printf (" mdbGetModuleReference2 (0x%08"PRIx32") => 0x%08"PRIx32"\n", dirdb_ref, mdb_ref);
	if (mdb_ref == 0xffffffff)
	{
		return;
	}

	mdbGetModuleInfo (&mi, mdb_ref);
	mi.flags = MDB_VIRTUAL;
	mi.channels=2;
	snprintf(mi.title, sizeof (mi.title), "%s", name);
	mi.composer[0] = 0;
	mi.comment[0] = 0;

	if (descr)
	{
		const char *n = strchr (descr, '\n');
		if (n)
		{
			int len = n - descr + 1;
			if (len >= sizeof(mi.composer))
			{
				len = sizeof(mi.composer);
			}
			snprintf(mi.composer, len, "%s", descr);
			snprintf(mi.comment, sizeof (mi.comment), "%s", n + 1);
		} else {
			if (strlen(descr) > sizeof (mi.composer))
			{
				n = descr+strlen(descr)-1;
				while (n-descr > 0)
				{
					if ((*n) != ' ')
					{
						n--;
						continue;
					}
					if (n-descr >= sizeof(mi.composer))
					{
						n--;
						continue;
					}
					snprintf(mi.composer, n-descr+1, "%s", descr);
					snprintf(mi.comment, sizeof (mi.comment), "%s", n + 1);
					break;
				}
			} else {
				snprintf(mi.composer, sizeof(mi.composer), "%s", descr);
			}
		}
	}
	mi.modtype.integer.i = MODULETYPE("DEVv");
	mdbWriteModuleInfo(mdb_ref, &mi);
}


static int dir_alsa_readdir_iterate (ocpdirhandle_pt _handle)
{
	struct dirhandle_alsa_t *handle = (struct dirhandle_alsa_t *)_handle;

	debug_printf ("dir_alsa_readdir_iterate() (handle->hints[%d]=%p\n", handle->i, handle->hints[handle->i]);

	if (handle->hints[handle->i])
	{
		char *name, *descr, *io;
		char namebuffer[128];
		uint32_t dirdb_ref;
		struct ocpfile_t *child;

		name = snd_device_name_get_hint(handle->hints[handle->i], "NAME");
		descr = snd_device_name_get_hint(handle->hints[handle->i], "DESC");
		io = snd_device_name_get_hint(handle->hints[handle->i], "IOID");

		debug_printf ("snd_device_name_get_hint()\n");
		debug_printf ("       [%d] name=%s\n", handle->i, name?name:"(NULL)");
		debug_printf ("            desc=%s\n", descr?descr:"(NULL)");
		debug_printf ("            io=%s\n", io?io:"(IO)");

		if (!name) /* should never happen */
		{
			goto end;
		}
		if (io && (!strcmp(io, "Input"))) /* ignore input only cards */
		{
			goto end;
		}

#warning Ideal would be to link dirdb_ref and shortname in an API!!!!!!!!
//		snprintf (namebuffer, sizeof (namebuffer), "ALSA-PCM-%s.dev", name);
		snprintf (namebuffer, sizeof (namebuffer), "alsa-%03d.dev", handle->count);

		dirdb_ref = dirdbFindAndRef (handle->owner->dirdb_ref,  namebuffer, dirdb_use_file);

		dir_alsa_update_mdb (handle->owner->dirdb_ref, name, descr);

		child = mem_file_open (handle->owner, dirdb_ref, strdup (alsaPCMoutIntr.name), strlen (alsaPCMoutIntr.name));
		child->is_nodetect = 1;
		handle->callback_file (handle->token, child);
		child->unref (child);
		dirdbUnref (dirdb_ref, dirdb_use_file);
		handle->count++;
end:
		free (name);
		free (descr);
		free (io);

		handle->i++;
		return 1;
	} else {
		return 0;
	}
}

static struct ocpdir_t *dir_alsa_readdir_dir (struct ocpdir_t *_self, uint32_t dirdb_ref)
{
	/* this can not succeed */
	return 0;
}

static struct ocpfile_t *dir_alsa_readdir_file (struct ocpdir_t *_self, uint32_t dirdb_ref)
{
	int result, i;
	char *searchpath = 0;
	uint32_t parent_dirdb_ref;
	void **hints;
	int count = 1;

	debug_printf ("dir_alsa_readdir_file()\n");

/* assertion begin */
	parent_dirdb_ref = dirdbGetParentAndRef (dirdb_ref, dirdb_use_file);
	dirdbUnref (parent_dirdb_ref, dirdb_use_file);
	if (parent_dirdb_ref != _self->dirdb_ref)
	{
		fprintf (stderr, "dir_alsa_readdir_file: dirdb_ref->parent is not the expected value\n");
		return 0;
	}
/* assertion end */

	dirdbGetName_internalstr (dirdb_ref, &searchpath);
	if (!searchpath)
	{
		return 0;
	}

	result=snd_device_name_hint(-1, "pcm", &hints);
	debug_printf ("      snd_device_name_hint() = %s\n", snd_strerror(-result));
	if (result)
	{
		return 0;
	}

	for (i=0; hints[i]; i++)
	{
		char *name, *descr, *io;
		char namebuffer[128];
		struct ocpfile_t *child;

		name = snd_device_name_get_hint (hints[i], "NAME");
		descr = snd_device_name_get_hint (hints[i], "DESC");
		io = snd_device_name_get_hint (hints[i], "IOID");

		debug_printf ("snd_device_name_get_hint()\n");
		debug_printf ("       [%d] name=%s\n", i, name?name:"(NULL)");
		debug_printf ("            desc=%s\n", descr?descr:"(NULL)");
		debug_printf ("            io=%s\n", io?io:"(NULL)");

		if (!name) /* should never happen */
		{
			free (descr);
			free (io);
			continue;
		}
		if (io && (!strcmp(io, "Input"))) /* ignore input only cards */
		{
			free (name);
			free (descr);
			free (io);
			continue;
		}

		//struct modlistentry entry;
		//memset(&entry, 0, sizeof(entry));
#if 0
		snprintf(shortname, 4, "alsa-%03ddev", handle->count);
#else
		//memcpy (shortname, "alsa-", 5);
		//snprintf(shortname+5, 4, "%03d", handle->count);
		//memcpy (shortname + 8, "dev", 3);
#endif
#warning Ideal would be to link dirdb_ref and shortname in an API!!!!!!!!
//		snprintf(namebuffer, sizeof(namebuffer), "ALSA-PCM-%s.dev", name);
		snprintf (namebuffer, sizeof (namebuffer), "alsa-%03d.dev", count);

		if (strcmp (searchpath, namebuffer))
		{
			count++;
			free (name);
			free (descr);
			free (io);
			continue;
		}

		dir_alsa_update_mdb (dirdb_ref, name, descr);

		free (name);
		free (descr);
		free (io);
		snd_device_name_free_hint (hints);

		child = mem_file_open (_self, dirdb_ref, strdup (alsaPCMoutIntr.name), strlen (alsaPCMoutIntr.name));
		child->is_nodetect = 1;
		return child;
	}

	snd_device_name_free_hint (hints);

	return 0;
}

static int detect_cards (void)
{
	int result;
	int retval = 0;
	int i;
	void **hints = 0;

	debug_printf ("ALSA_detect_card\n");

	result=snd_device_name_hint(-1, "pcm", &hints);
	debug_printf ("      snd_device_name_hint() = %s\n", snd_strerror(-result));

	if (result)
	{
		return retval; /* zero cards found */
	}

	for (i=0; hints[i]; i++)
	{
		char *name, *descr, *io;
		name = snd_device_name_get_hint(hints[i], "NAME");
		descr = snd_device_name_get_hint(hints[i], "DESC");
		io = snd_device_name_get_hint(hints[i], "IOID");

		debug_printf ("       [%d] name=%s\n", i, name?name:"(NULL)");
		debug_printf ("            desc=%s\n", descr?descr:"(NULL)");
		debug_printf ("            io=%s\n", io?io:"(IO)");

		if (!name) /* should never happen */
		{
			goto end;
		}
		if (io && (!strcmp(io, "Input"))) /* ignore input only cards */
		{
			goto end;
		}

		if (strcmp(name, "default"))
		{
			retval++;
		}
end:
		free (name);
		free (descr);
		free (io);
	}

	debug_printf ("     snd_device_name_free_hint()\n");

	snd_device_name_free_hint (hints);

	debug_printf (" => %d\n", retval);

	return retval;
}

/* plr API start */

static void SetOptions(unsigned int rate, int opt)
{
	int err;
	snd_pcm_format_t format;
	unsigned int val;
	/* start with setting default values, if we bail out */
	plrRate=rate;
	plrOpt=opt;

	alsaOpenDevice();
	if (!alsa_pcm)
		return;

	debug_printf ("ALSA_SetOptions (rate=%d, opt=0x%x%s%s)\n", rate, opt, (opt&PLR_16BIT)?" 16bit":" 8bit", (opt&PLR_SIGNEDOUT)?" signed":" unsigned");

	err=snd_pcm_hw_params_any(alsa_pcm, hwparams);
	debug_printf("      snd_pcm_hw_params_any(alsa_pcm, hwparams) = %s\n", snd_strerror(err<0?-err:0));
	if (err<0)
	{
		fprintf(stderr, "ALSA: snd_pcm_hw_params_any() failed: %s\n", snd_strerror(-err));
		return;
	}

	err=snd_pcm_hw_params_set_access(alsa_pcm, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED);
	debug_printf("      snd_pcm_hw_params_set_access(alsa_pcm, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED) = %s\n", snd_strerror(-err));
	if (err)
	{
		fprintf(stderr, "ALSA: snd_pcm_hw_params_set_access() failed: %s\n", snd_strerror(-err));
		return;
	}

	if (opt&PLR_16BIT)
	{
		if (opt&PLR_SIGNEDOUT)
		{
			format=SND_PCM_FORMAT_S16;
		} else {
			format=SND_PCM_FORMAT_U16;
		}
	} else {
		if (opt&PLR_SIGNEDOUT)
		{
			format=SND_PCM_FORMAT_S8;
		} else {
			format=SND_PCM_FORMAT_U8;
		}
	}

	err=snd_pcm_hw_params_set_format(alsa_pcm, hwparams, format);
	debug_printf("      snd_pcm_hw_params_set_format(alsa_pcm, hwparams, format %i) = %s\n", format, snd_strerror(-err));
	if (err)
	{
		err=snd_pcm_hw_params_set_format(alsa_pcm, hwparams, SND_PCM_FORMAT_S16);
		debug_printf("      snd_pcm_hw_params_set_format(alsa_pcm, hwparams, SND_PCM_FORMAT_S16) = %s\n", snd_strerror(-err));
		if (err==0)
		{
			opt|=PLR_16BIT|PLR_SIGNEDOUT;
		} else {
			err=snd_pcm_hw_params_set_format(alsa_pcm, hwparams, SND_PCM_FORMAT_U16);
			debug_printf("      snd_pcm_hw_params_set_format(alsa_pcm, hwparams, SND_PCM_FORMAT_U16) = %s\n", snd_strerror(-err));
			if (err==0)
			{
				opt&=~(PLR_16BIT|PLR_SIGNEDOUT);
				opt|=PLR_16BIT;
			} else {
				err=snd_pcm_hw_params_set_format(alsa_pcm, hwparams, SND_PCM_FORMAT_S8);
				debug_printf("      snd_pcm_hw_params_set_format(alsa_pcm, hwparams, SND_PCM_FORMAT_S8) = %s\n", snd_strerror(-err));
				if (err==0)
				{
					opt&=~(PLR_16BIT|PLR_SIGNEDOUT);
					opt|=PLR_SIGNEDOUT;
				} else
				{
					err=snd_pcm_hw_params_set_format(alsa_pcm, hwparams, SND_PCM_FORMAT_U8);
					debug_printf("      snd_pcm_hw_params_set_format(alsa_pcm, hwparams, SND_PCM_FORMAT_U8) = %s\n", snd_strerror(-err));
					if (err==0)
					{
						opt&=~(PLR_16BIT|PLR_SIGNEDOUT);
					} else {
						fprintf(stderr, "ALSA: snd_pcm_hw_params_set_format() failed: %s\n", snd_strerror(-err));
						return;
					}
				}
			}
		}
	}

	bit16=!!(opt&PLR_16BIT);
	if (opt&PLR_STEREO)
	{
		val=2;
	} else {
		val=1;
	}

	err=snd_pcm_hw_params_set_channels_near(alsa_pcm, hwparams, &val);
	debug_printf("      snd_pcm_hw_params_set_channels_near(alsa_pcm, hwparams, &channels=%i) = %s\n", val, snd_strerror(-err));
	if (err<0)
	{
		fprintf(stderr, "ALSA: snd_pcm_hw_params_set_channels_near() failed: %s\n", snd_strerror(-err));
		return;
	}
	if (val==1)
	{
		stereo=0;
		opt&=~PLR_STEREO;
	} else if (val==2)
	{
		stereo=1;
		opt|=PLR_STEREO;
	} else {
		fprintf(stderr, "ALSA: snd_pcm_hw_params_set_channels_near() gave us %d channels\n", val);
		return;
	}

	err=snd_pcm_hw_params_set_rate_near(alsa_pcm, hwparams, &rate, 0);
	debug_printf("      snd_pcm_hw_params_set_rate_near(alsa_pcm, hwparams, &rate = %i, 0) = %s\n", rate, snd_strerror(-err));
	if (err<0)
	{
		fprintf(stderr, "ALSA: snd_pcm_hw_params_set_rate_near() failed: %s\n", snd_strerror(-err));
		return;
	}
	if (rate==0)
	{
		fprintf(stderr, "ALSA: No usable samplerate available.\n");
		return;
	}

	val = 500000;
	err=snd_pcm_hw_params_set_buffer_time_near(alsa_pcm, hwparams, &val, 0);
	debug_printf("      snd_pcm_hw_params_set_buffer_time_near(alsa_pcm, hwparams, 500000 uS => %d, 0) = %s\n", val, snd_strerror(-err));
	if (err)
	{
		fprintf(stderr, "ALSA: snd_pcm_hw_params_set_buffer_time_near() failed: %s\n", snd_strerror(-err));
		return;
	}

	err=snd_pcm_hw_params(alsa_pcm, hwparams);
	debug_printf("      snd_pcm_hw_params(alsa_pcm, hwparams) = %s\n", snd_strerror(-err));
	if (err<0)
	{
		fprintf(stderr, "ALSA: snd_pcm_hw_params() failed: %s\n", snd_strerror(-err));
		return;
	}

	err=snd_pcm_sw_params_current(alsa_pcm, swparams);
	debug_printf("       snd_pcm_sw_params_current(alsa_pcm, swparams) = %s\n", snd_strerror(-err));
	if (err<0)
	{
		fprintf(stderr, "ALSA: snd_pcm_sw_params_any() failed: %s\n", snd_strerror(-err));
		return;
	}

	err=snd_pcm_sw_params(alsa_pcm, swparams);
	debug_printf("      snd_pcm_sw_params(alsa_pcm, swparams) = %s\n", snd_strerror(-err));
	if (err<0)
	{
		fprintf(stderr, "ALSA: snd_pcm_sw_params() failed: %s\n", snd_strerror(-err));
		return;
	}
	plrRate=rate;
	plrOpt=opt;
}

#ifdef PLR_DEBUG
static char *alsaDebug(void)
{
	static char buffer[100];
	strcpy(buffer, "devpalsa: ");
	convnum(cachelen, buffer+9, 10, 5, 1);
	strcat(buffer, "/");
	convnum(kernlen, buffer+15, 10, 5, 1);
	strcat(buffer, "/");
	convnum(buflen, buffer+21, 10, 5, 1);
	return buffer;
}
#endif

static int alsaPlay(void **buf, unsigned int *len, struct ocpfilehandle_t *source_file)
{
	if (!alsa_pcm)
		return 0;

	if ((*len)<(plrRate&~3))
		*len=plrRate&~3;
	if ((*len)>(plrRate*4))
		*len=plrRate*4;
	playbuf=*buf=malloc(*len);

	memsetd(*buf, (plrOpt&PLR_SIGNEDOUT)?0:(plrOpt&PLR_16BIT)?0x80008000:0x80808080, (*len)>>2);

	buflen=*len;
	bufpos=0;
	cachepos=0;
	cachelen=0;
	playpos=0;
	kernpos=0;
	kernlen=0;

	plrGetBufPos=getbufpos;
	plrGetPlayPos=getplaypos;
	plrIdle=flush;
	plrAdvanceTo=advance;
	plrGetTimer=gettimer;
#ifdef PLR_DEBUG
	plrDebug=alsaDebug;
#endif

#ifdef ALSA_DEBUG_OUTPUT
	debug_output = open ("test-alsa.raw", O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
#endif

	return 1;
}

static void alsaStop(void)
{
#ifdef PLR_DEBUG
	plrDebug=0;
#endif
	free(playbuf);

#ifdef ALSA_DEBUG_OUTPUT
	close (debug_output);
	debug_output = -1;
#endif

}

/* plr API stop */


/* driver front API start */

static void alsaOpenDevice(void)
{
	int err;
	snd_mixer_elem_t *current;

	alsa_mixers_n=0;

	/* if any device already open, please close it */
	debug_printf ("alsaOpenDevice()\n");
	if (alsa_pcm)
	{
		err=snd_pcm_drain(alsa_pcm);
		debug_printf("      snd_pcm_drain(alsa_pcm) = %s\n", snd_strerror(-err));

		err=snd_pcm_close(alsa_pcm);
		debug_printf("      snd_pcm_close(alsa_pcm) = %s\n", snd_strerror(-err));
		alsa_pcm=NULL;
	}

	if (mixer)
	{
		err=snd_mixer_close(mixer);
		debug_printf("      snd_mixer_close(mixer) = %s\n", snd_strerror(-err));
		mixer=NULL;
	}

	/* open PCM device */
	err=snd_pcm_open(&alsa_pcm, alsaCardName, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
	debug_printf("      snd_pcm_open(&alsa_pcm, device = \"%s\", SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK) = %s\n", alsaCardName, snd_strerror(-err));
	if (err<0)
	{
		fprintf(stderr, "ALSA: failed to open pcm device (%s): %s\n", alsaCardName, snd_strerror(-err));
		alsa_pcm=NULL;
		return;
	}

	/* Any mixer to open ? */
	if (!strlen(alsaMixerName))
		return;

	err=snd_mixer_open(&mixer, 0);
	debug_printf("      snd_mixer_open(&mixer, 0) = %s\n", snd_strerror(-err));
	if (err<0)
	{
		fprintf(stderr, "ALSA: snd_mixer_open() failed: %s\n", snd_strerror(-err));
		return;
	}

	err=snd_mixer_attach(mixer, alsaMixerName);
	debug_printf("      snd_mixer_attach(mixer, device = \"%s\") = %s\n", alsaMixerName, snd_strerror(-err));
	if (err<0)
	{
		fprintf(stderr, "ALSA: snd_mixer_attach() failed: %s\n", snd_strerror(-err));
		err=snd_mixer_close(mixer);
		debug_printf("      snd_mixer_close(mixer) = %s\n", snd_strerror(-err));
		mixer=NULL;
		return;
	}

	err=snd_mixer_selem_register(mixer, NULL, NULL);
	debug_printf("      snd_mixer_selem_register(mixer, NULL, NULL) = %s\n", snd_strerror(-err));
	if (err<0)
	{
		fprintf(stderr, "ALSA: snd_mixer_selem_register() failed: %s\n", snd_strerror(-err));

		err=snd_mixer_close(mixer);
		debug_printf("      snd_mixer_close(mixer) = %s\n", snd_strerror(-err));
		mixer=NULL;
		return;
	}

	err=snd_mixer_load(mixer);
	debug_printf("      snd_mixer_load(mixer) = %s\n", snd_strerror(-err));
	if (err<0)
	{
		fprintf(stderr, "ALSA: snd_mixer_load() failed: %s\n", snd_strerror(-err));

		err=snd_mixer_close(mixer);
		debug_printf("      snd_mixer_close(mixer) = %s\n", snd_strerror(-err));
		mixer=NULL;
		return;
	}

	current = snd_mixer_first_elem(mixer);
	debug_printf ("      snd_mixer_first_elem(mixer) = %p\n", current);
	while (current)
	{
		debug_printf ("        snd_mixer_selem_is_active(current) = %d\n", snd_mixer_selem_is_active(current));
		debug_printf ("        snd_mixer_selem_has_playback_volume = %d\n", snd_mixer_selem_has_playback_volume(current));
		if (snd_mixer_selem_is_active(current) &&
			snd_mixer_selem_has_playback_volume(current) &&
			(alsa_mixers_n<MAX_ALSA_MIXER))
		{
			long int a, b;
			long int min, max;
			snd_mixer_selem_get_playback_volume(current, SND_MIXER_SCHN_FRONT_LEFT, &a);
			snd_mixer_selem_get_playback_volume(current, SND_MIXER_SCHN_FRONT_RIGHT, &b);
			mixer_entries[alsa_mixers_n].val=(a+b)>>1;
			snd_mixer_selem_get_playback_volume_range(current, &min, &max);
			mixer_entries[alsa_mixers_n].min=min;
			mixer_entries[alsa_mixers_n].max=max;
			mixer_entries[alsa_mixers_n].step=1;
			mixer_entries[alsa_mixers_n].log=0;
			mixer_entries[alsa_mixers_n].name=snd_mixer_selem_get_name(current);

			debug_printf ("          name=%s\n", mixer_entries[alsa_mixers_n].name);
			debug_printf ("          SND_MIXER_SCHN_FRONT_LEFT  = %ld\n", a);
			debug_printf ("          SND_MIXER_SCHN_FRONT_RIGHT = %ld\n", b);
			debug_printf ("          min=%ld max=%ld\n", min, max);

			alsa_mixers_n++;
		}
		current = snd_mixer_elem_next(current);
		debug_printf ("      snd_mixer_elem_next(current) = %p\n", current);
	}
}

static int volalsaGetNumVolume(void)
{
	return alsa_mixers_n;
}

static int volalsaGetVolume(struct ocpvolstruct *v, int n)
{
	if (n<alsa_mixers_n)
	{
		memcpy(v, &mixer_entries[n], sizeof(mixer_entries[n]));
		return 1;
	}
	return 0;
}

static int volalsaSetVolume(struct ocpvolstruct *v, int n)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
	int count=0, err; /* err is only used if debug_printf() is not a dummy call */
#pragma GCC diagnostic pop
	snd_mixer_elem_t *current;

	debug_printf ("volalsaSetVolume(v->val=%d n=%d)\n", v->val, n);

	current = snd_mixer_first_elem(mixer);
	while (current)
	{
		if (snd_mixer_selem_is_active(current) &&
			snd_mixer_selem_has_playback_volume(current))
		{
			if (count==n)
			{
				err = snd_mixer_selem_set_playback_volume(current, SND_MIXER_SCHN_FRONT_LEFT, v->val);
				debug_printf ("      snd_mixer_selem_set_playback_volume(current %s, SND_MIXER_SCHN_FRONT_LEFT,  %d) = %s\n", snd_mixer_selem_get_name (current), v->val, snd_strerror(-err));
				err = snd_mixer_selem_set_playback_volume(current, SND_MIXER_SCHN_FRONT_RIGHT, v->val);
				debug_printf ("      snd_mixer_selem_set_playback_volume(current %s, SND_MIXER_SCHN_FRONT_RIGHT, %d) = %s\n", snd_mixer_selem_get_name (current), v->val, snd_strerror(-err));
				mixer_entries[n].val=v->val;
				return 1;
			}
			count++;
		}
		current = snd_mixer_elem_next(current);
	}
	return 0;
}

static int alsasetupregistered = 0;
static void alsaClose(void);
static int alsaInit(const struct deviceinfo *c)
{
	alsasetupregistered = 1;
	ocpdir_t_fill (&dir_alsa,
	                dir_alsa_ref,
	                dir_alsa_unref,
	                dmSetup->basedir,
	                dir_alsa_readdir_start,
	                0,
	                dir_alsa_readdir_cancel,
	                dir_alsa_readdir_iterate,
	                dir_alsa_readdir_dir,
	                dir_alsa_readdir_file,
	                0,
			dirdbFindAndRef (dmSetup->basedir->dirdb_ref, "alsa", dirdb_use_dir),
	                0, /* refcount, ALSA ignores this */
	                0, /* is_archive */
	                0  /* is_playlist */);
	filesystem_setup_register_dir (&dir_alsa);

	plRegisterInterface (&alsaPCMoutIntr);

	plrSetOptions=SetOptions;
	plrPlay=alsaPlay;
	plrStop=alsaStop;

	alsaOpenDevice();
	if (!alsa_pcm)
	{
		alsaClose();
		return 0;
	}

	SetOptions(44100, PLR_16BIT|PLR_STEREO);
	return 1;
}

static int alsaDetect(struct deviceinfo *card)
{
	int cards = detect_cards ();

	card->devtype=&plrAlsa;

	snprintf (card->path, sizeof(card->path), "%s", cfGetProfileString("devpALSA", "path", "default"));
	snprintf (alsaCardName, sizeof(alsaCardName), "%s", card->path);

	snprintf (card->mixer, sizeof(card->mixer), "%s", cfGetProfileString("devpALSA", "mixer", "default"));
	snprintf (alsaMixerName, sizeof(alsaMixerName), "%s", card->mixer);
/*
	card->irq     = -1;
	card->irq2    = -1;
	card->dma     = -1;
	card->dma2    = -1;
*/
	card->subtype = -1;
	card->mem     = 0;
	card->chan    = 2;

	return cards > 0;
}

static void alsaClose(void)
{
	plrPlay=0;

	if (alsasetupregistered)
	{
		plUnregisterInterface (&alsaPCMoutIntr);

		filesystem_setup_unregister_dir (&dir_alsa);
		dirdbUnref (dir_alsa.dirdb_ref, dirdb_use_dir);
		alsasetupregistered = 0;
	}
}

static int alsaMixerIntrSetDev (struct moduleinfostruct *info, struct ocpfilehandle_t *f, const struct interfaceparameters *ip)
{
	char *name;

	dirdbGetName_internalstr (f->dirdb_ref, &name);

#warning we must add custom.dev in the file-list!!
	if (!strcmp(name, "custom.dev"))
	{
		int mlTop=mlDrawBox();
		char str[DEVICE_NAME_MAX+1];
		unsigned int curpos;
		unsigned int cmdlen;
		int insmode=1;
		unsigned int scrolled=0;

		strcpy(str, alsaCardName);

		displaystr(mlTop+1, 5, 0x0b, "Give me something to crunch!!", 29);
		displaystr(mlTop+3, 5, 0x0b, "-- Finish with enter, abort with escape --", 32);

		curpos=strlen(str);
		cmdlen=strlen(str);
		setcurshape(1);

		while (1)
		{
			uint16_t key;
			displaystr(mlTop+2, 5, 0x8f, str+scrolled, plScrWidth-10);
			setcur(mlTop+2, 5+curpos-scrolled);

			while (!ekbhit())
				framelock();
			key=egetch();

			if ((key>=0x20)&&(key<=0x7f))
			{
				if (insmode)
				{
					if ((cmdlen+1)<sizeof(str))
					{
						memmove(str+curpos+1, str+curpos, cmdlen-curpos+1);
						str[curpos++]=key;
						cmdlen++;
					}
				} else if (curpos==cmdlen)
				{
					if ((cmdlen+1)<(sizeof(str)))
					{
						str[curpos++]=key;
						str[curpos]=0;
						cmdlen++;
					}
				} else
					str[curpos++]=key;
			} else switch (key)
			{
			case KEY_ESC:
				setcurshape(0);
				return 0;
			case KEY_LEFT:
				if (curpos)
					curpos--;
				break;
			case KEY_RIGHT:
				if (curpos<cmdlen)
					curpos++;
				break;
			case KEY_HOME:
				curpos=0;
				break;
			case KEY_END:
				curpos=cmdlen;
				break;
			case KEY_INSERT:
				{
					insmode=!insmode;
					setcurshape(insmode?1:2);
				}
				break;
			case KEY_DELETE:
				if (curpos!=cmdlen)
				{
					memmove(str+curpos, str+curpos+1, cmdlen-curpos);
					cmdlen--;
				}
				break;
			case KEY_BACKSPACE:
				if (curpos)
				{
					memmove(str+curpos-1, str+curpos, cmdlen-curpos+1);
					curpos--;
					cmdlen--;
				}
				break;
			case _KEY_ENTER:
				strcpy(alsaCardName, str);
				setcurshape(0);
				goto out;
			}
			while ((curpos-scrolled)>=(plScrWidth-10))
				scrolled+=8;
			while (((signed)curpos-(signed)scrolled)<0)
				scrolled-=8;
		}
	}
/* 1.0.14rc1 added support for snd_device_name_hint */
	else if (!strncmp(name, "alsa-", 4))
	{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation="
		snprintf (alsaCardName, sizeof (alsaCardName), "%s", info->title);
		snprintf (alsaMixerName, sizeof (alsaCardName), "%s", info->title);
#pragma GCC diagnostic pop
	}
out:
	fprintf(stderr, "ALSA: Selected PCM %s\n", alsaCardName);
	fprintf(stderr, "ALSA: Selected Mixer %s\n", alsaMixerName);

	if (custom_dsp_mdb_ref!=0xffffffff)
	{
		struct moduleinfostruct mi;
		mdbGetModuleInfo(&mi, custom_dsp_mdb_ref);
		snprintf(mi.title, sizeof(mi.title), "%s", alsaCardName);
		mdbWriteModuleInfo(custom_dsp_mdb_ref, &mi);
	}
	if (custom_mixer_mdb_ref!=0xffffffff)
	{
		struct moduleinfostruct mi;
		mdbGetModuleInfo(&mi, custom_mixer_mdb_ref);
		snprintf(mi.title, sizeof(mi.title), "%s", alsaMixerName);
		mdbWriteModuleInfo(custom_mixer_mdb_ref, &mi);
	}

	return 0;
}

static void __attribute__((constructor))init(void)
{
	int err;
	if ((err = snd_pcm_status_malloc(&alsa_pcm_status)))
	{
		fprintf(stderr, "snd_pcm_status_malloc() failed, %s\n", snd_strerror(-err));
		exit(0);
	}
	if ((err = snd_pcm_hw_params_malloc(&hwparams)))
	{
		fprintf(stderr, "snd_pcm_hw_params_malloc failed, %s\n", snd_strerror(-err));
		exit(0);
	}
	if ((err = snd_pcm_sw_params_malloc(&swparams)))
	{
		fprintf(stderr, "snd_pcm_sw_params_malloc failed, %s\n", snd_strerror(-err));
		exit(0);
	}
}

static void __attribute__((destructor))fini(void)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
	int err; /* err is only used if debug_printf() is not a dummy call */
#pragma GCC diagnostic pop

	debug_printf("ALSA_fini()\n");

	if (alsa_pcm)
	{
		err=snd_pcm_drain(alsa_pcm);
		debug_printf("      snd_pcm_drain(alsa_pcm) = %s\n", snd_strerror(-err));

		err=snd_pcm_close(alsa_pcm);
		debug_printf("      snd_pcm_close(alsa_pcm) = %s\n", snd_strerror(-err));

		alsa_pcm=NULL;
	}

	if (mixer)
	{
		err=snd_mixer_close(mixer);
		debug_printf("      snd_mixer_close(mixer) = %s\n", snd_strerror(-err));
		mixer=NULL;
	}

	if (alsa_pcm_status)
	{
		snd_pcm_status_free(alsa_pcm_status);
		alsa_pcm_status = NULL;
	}

	if (hwparams)
	{
		snd_pcm_hw_params_free(hwparams);
		hwparams=NULL;
	}

	if (swparams)
	{
		snd_pcm_sw_params_free(swparams);
		swparams=NULL;
	}

	snd_config_update_free_global ();

	alsa_mixers_n=0;
}

struct sounddevice plrAlsa={SS_PLAYER, 1, "ALSA device driver", alsaDetect, alsaInit, alsaClose, NULL};
static struct interfacestruct alsaPCMoutIntr = {alsaMixerIntrSetDev, 0, 0, "alsaPCMoutIntr" INTERFACESTRUCT_TAIL};
struct ocpvolregstruct volalsa={volalsaGetNumVolume, volalsaGetVolume, volalsaSetVolume};

char *dllinfo="driver plrAlsa; volregs volalsa";
struct linkinfostruct dllextinfo = {.name = "devpalsa", .desc = "OpenCP Player Device: ALSA (c) 2005-21 Stian Skjelstad", .ver = DLLVERSION, .size = 0};
