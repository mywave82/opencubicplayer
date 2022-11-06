/** OpenCP Module Player
 * copyright (c) 2005-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
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
#include <assert.h>
#include <string.h>
#include "types.h"
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "cpiface/cpiface.h"
#include "cpiface/vol.h"
#include "dev/imsdev.h"
#include "dev/player.h"
#include "dev/plrasm.h"
#include "dev/ringbuffer.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/filesystem-drive.h"
#include "filesel/filesystem-file-dev.h"
#include "filesel/filesystem-setup.h"
#include "filesel/mdb.h"
#include "filesel/modlist.h"
#include "filesel/pfilesel.h"
#include "stuff/imsrtns.h"
#include "stuff/poutput.h"

//#define ALSA_DEBUG_OUTPUT 1
#ifdef ALSA_DEBUG_OUTPUT
static int debug_output = -1;
#endif
#ifdef ALSA_DEBUG
#define debug_printf(...) fprintf (stderr, __VA_ARGS__)
#else
#define debug_printf(format, args...) ((void)0)
#endif

static void *devpALSABuffer;
static char *devpALSAShadowBuffer;
static struct ringbuffer_t *devpALSARingBuffer;
static int devpALSAPauseSamples;
static int devpALSAInPause;
static uint32_t devpALSARate;
static const struct plrDevAPI_t devpALSA;

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
static uint32_t dir_alsa_custom = 0xffffffff;
static uint32_t custom_mdb_ref = 0xffffffff;

static void alsaOpenDevice(void);

static char alsaCardName[DEVICE_NAME_MAX+1];
static char alsaMixerName[DEVICE_NAME_MAX+1];

/* stolen from devposs */
#define MAX_ALSA_MIXER 256
static struct ocpvolstruct mixer_entries[MAX_ALSA_MIXER];
static int alsa_mixers_n=0;

static int stereo;
static int bit16;
static int bitsigned;

static volatile int busy=0;

static void alsaSetCustomRun (void **token, const struct DevInterfaceAPI_t *API);
static int alsaSetPCMInit (void **token, struct moduleinfostruct *info, struct ocpfilehandle_t *f, const struct DevInterfaceAPI_t *API);
static int alsaSetMixerInit (void **token, struct moduleinfostruct *info, struct ocpfilehandle_t *f, const struct DevInterfaceAPI_t *API);

static void update_custom_dev (void)
{
	if (custom_mdb_ref != 0xffffffff)
	{
		struct moduleinfostruct mi;
		mdbGetModuleInfo(&mi, custom_mdb_ref);
		snprintf(mi.title, sizeof(mi.title), "%s", alsaCardName);
		snprintf(mi.composer, sizeof(mi.composer), "%s", alsaMixerName);
		mdbWriteModuleInfo(custom_mdb_ref, &mi);
	}
}

static unsigned int devpALSAIdle(void)
{
	int pos1, length1, pos2, length2;
	int result, odelay, tmp;
	int err;
	int kernlen;
	unsigned int RetVal;

	if (busy++)
	{
		busy--;
		return 0;
	}

	debug_printf("devpALSAIdle()\n");

/* Update the ringbuffer-tail START */
	err=snd_pcm_status(alsa_pcm, alsa_pcm_status);
	debug_printf("      snd_pcm_status(alsa_pcm, alsa_pcm_status) = %s\n", snd_strerror(-err));
	if (err<0)
	{
		fprintf(stderr, "ALSA: snd_pcm_status() failed: %s\n", snd_strerror(-err));
		busy--;
		return 0;
	}

#ifdef ALSA_DEBUG
	switch (snd_pcm_status_get_state (alsa_pcm_status))
	{
		case SND_PCM_STATE_OPEN:         fprintf (stderr, "  SND_PCM_STATE_OPEN: Open\n"); break;
		case SND_PCM_STATE_SETUP:        fprintf (stderr, "  SND_PCM_STATE_SETUP: Setup installed\n"); break;
		case SND_PCM_STATE_PREPARED:     fprintf (stderr, "  SND_PCM_STATE_PREPARED: Ready to start\n"); break;
		case SND_PCM_STATE_RUNNING:      fprintf (stderr, "  SND_PCM_STATE_RUNNING: Running\n"); break;
		case SND_PCM_STATE_XRUN:         fprintf (stderr, "  SND_PCM_STATE_XRUN: Stopped: underrun (playback) or overrun (capture) detected\n"); break;
		case SND_PCM_STATE_DRAINING:     fprintf (stderr, "  SND_PCM_STATE_DRAINING: Draining: running (playback) or stopped (capture)\n"); break;
		case SND_PCM_STATE_PAUSED:       fprintf (stderr, "  SND_PCM_STATE_PAUSED: Paused\n"); break;
		case SND_PCM_STATE_SUSPENDED:    fprintf (stderr, "  SND_PCM_STATE_SUSPENDED: Hardware is suspended\n"); break;
		case SND_PCM_STATE_DISCONNECTED: fprintf (stderr, "  SND_PCM_STATE_DISCONNECTED: Hardware is disconnected\n"); break;
		case SND_PCM_STATE_PRIVATE1:     fprintf (stderr, "  SND_PCM_STATE_PRIVATE1: Private - used internally in the library - do not use\n"); break;
	}
#endif
	if (snd_pcm_status_get_state (alsa_pcm_status) == SND_PCM_STATE_XRUN)
	{
		fprintf (stderr, "ALSA: Buffer underrun detected, restarting PCM stream\n");
		snd_pcm_prepare (alsa_pcm);
		goto error_out;
	} else {
		odelay=snd_pcm_status_get_delay(alsa_pcm_status);
		debug_printf("      snd_pcm_status_get_delay(alsa_pcm_status) = %d\n", odelay);
	}

	if (odelay<0) /* buffer under-run on old buggy drivers? */
	{
		fprintf (stderr, "ALSA: snd_pcm_status_get_delay() negative values? %d\n", odelay);
		odelay=0;
	} else if (odelay==0)
	{ /* ALSA sometimes in the distant past gives odelay==0 always */
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

	kernlen = ringbuffer_get_tail_available_samples (devpALSARingBuffer);

	if (odelay>kernlen)
	{
		odelay=kernlen;
	} else if (odelay<kernlen)
	{
		ringbuffer_tail_consume_samples (devpALSARingBuffer, kernlen - odelay);
		if (devpALSAPauseSamples)
		{
			if ((kernlen - odelay) > devpALSAPauseSamples)
			{
				devpALSAPauseSamples = 0;
			} else {
				devpALSAPauseSamples -= (kernlen - odelay);
			}
		}
	}
/* Update the ringbuffer-tail DONE */

/* do we need to insert pause-samples? START */
	if (devpALSAInPause)
	{
		ringbuffer_get_head_bytes (devpALSARingBuffer, &pos1, &length1, &pos2, &length2);
		bzero ((char *)devpALSABuffer+pos1, length1);
		if (length2)
		{
			bzero ((char *)devpALSABuffer+pos2, length2);
		}
		ringbuffer_head_add_bytes (devpALSARingBuffer, length1 + length2);
		devpALSAPauseSamples += (length1 + length2) >> 2; /* stereo + 16bit */
	}
/* do we need to insert pause-samples? DONE */

/* Move data from ringbuffer-head into processing/kernel START */
	tmp = snd_pcm_status_get_avail(alsa_pcm_status);
	debug_printf ("      snd_pcm_status_get_avail() = %d\n", tmp);

	ringbuffer_get_processing_samples (devpALSARingBuffer, &pos1, &length1, &pos2, &length2);
	if (tmp < length1)
	{
		length1 = tmp;
		length2 = 0;
	} else if (tmp < (length1 + length2))
	{
		length2 = tmp - length1;
	}

	result=0; // remove warning in the if further down
	if (length1)
	{
		if (devpALSAShadowBuffer)
		{
			plrConvertBufferFromStereo16BitSigned (devpALSAShadowBuffer, (int16_t *)devpALSABuffer + (pos1<<1), length1, bit16 /* 16bit */, bit16 /* signed follows 16bit */, stereo, 0 /* revstereo */);
			result=snd_pcm_writei(alsa_pcm, devpALSAShadowBuffer, length1);
		} else {
			result=snd_pcm_writei(alsa_pcm, (uint8_t *)devpALSABuffer + (pos1<<2), length1);
		}
		debug_printf ("      snd_pcm_writei (%d) = %d\n", length1, result);
		if (result > 0)
		{
			ringbuffer_processing_consume_samples (devpALSARingBuffer, result);
		}
	}

	if (length2 && (result > 0))
	{
		if (devpALSAShadowBuffer)
		{
			plrConvertBufferFromStereo16BitSigned (devpALSAShadowBuffer, (int16_t *)devpALSABuffer + (pos2<<1), length2, bit16 /* 16bit */, bit16 /* signed follows 16bit */, stereo, 0 /* revstereo */);
			result=snd_pcm_writei(alsa_pcm, devpALSAShadowBuffer, length2);
		} else {
			result=snd_pcm_writei(alsa_pcm, (uint8_t *)devpALSABuffer + (pos2<<2), length2);
		}
		debug_printf ("      snd_pcm_writei (%d) = %d\n", length2, result);
		if (result > 0)
		{
			ringbuffer_processing_consume_samples (devpALSARingBuffer, result);
		}
	}

	if (result<0)
	{
		if (result==-EPIPE)
		{
			fprintf (stderr, "ALSA: Machine is too slow, calling snd_pcm_prepare()\n");
			snd_pcm_prepare(alsa_pcm); /* TODO, can this fail? */
			debug_printf ("      snd_pcm_prepare()\n");
		} else {
			fprintf (stderr, "ALSA: snd_pcm_writei() %d\n", result);
		}
		busy--;
		return 0;
	}
/* Move data from ringbuffer-head into processing/kernel STOP */

error_out:
	ringbuffer_get_tailandprocessing_samples (devpALSARingBuffer, &pos1, &length1, &pos2, &length2);

	busy--;

	RetVal = length1 + length2;
	if (devpALSAPauseSamples >= RetVal)
	{
		return 0;
	}
	return RetVal - devpALSAPauseSamples;
}

static void devpALSAPeekBuffer (void **buf1, unsigned int *buf1length, void **buf2, unsigned int *buf2length)
{
	int pos1, length1, pos2, length2;

	ringbuffer_get_tailandprocessing_samples (devpALSARingBuffer, &pos1, &length1, &pos2, &length2);

	if (length1)
	{
		*buf1 = (uint8_t *)devpALSABuffer + (pos1 << 2); /* stereo + 16bit */
		*buf1length = length1;
		if (length2)
		{
			*buf2 = (uint8_t *)devpALSABuffer + (pos2 << 2); /* stereo + 16bit */
			*buf2length = length2;
		} else {
			*buf2 = 0;
			*buf2length = 0;
		}
	} else {
		*buf1 = 0;
		*buf1length = 0;
		*buf2 = 0;
		*buf2length = 0;
	}
}

static void devpALSAGetBuffer (void **buf, unsigned int *samples)
{
	int pos1, length1;
	assert (devpALSARingBuffer);

	debug_printf("%s()\n", __FUNCTION__);

	ringbuffer_get_head_samples (devpALSARingBuffer, &pos1, &length1, 0, 0);

	*samples = length1;
	*buf = (uint8_t *)devpALSABuffer + (pos1<<2); /* stereo + bit16 */
}

static uint32_t devpALSAGetRate (void)
{
	return devpALSARate;
}

static void devpALSAOnBufferCallback (int samplesuntil, void (*callback)(void *arg, int samples_ago), void *arg)
{
	assert (devpALSARingBuffer);
	ringbuffer_add_tail_callback_samples (devpALSARingBuffer, samplesuntil, callback, arg);
}

static void devpALSACommitBuffer (unsigned int samples)
{
	debug_printf ("%s(%u)\n", __FUNCTION__, samples);

	ringbuffer_head_add_samples (devpALSARingBuffer, samples);
}

static void devpALSAPause (int pause)
{
	assert (devpALSABuffer);
	devpALSAInPause = pause;
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
	void **hints_pcm;
	void **hints_ctl;
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

	result=snd_device_name_hint(-1, "pcm", &retval->hints_pcm);
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


	{
		struct ocpfile_t *child;

		child = dev_file_create (
			self,
			"custom.dev",
			alsaCardName,
			alsaMixerName,
			0, /* token */
			0, /* Init */
			alsaSetCustomRun,
			0, /* Close */
			0  /* Destructor */
		);

		if (child)
		{
			callback_file (token, child);
			child->unref (child);
		}
	}

	return retval;
}

static void dir_alsa_readdir_cancel (ocpdirhandle_pt _handle)
{
	struct dirhandle_alsa_t *handle = (struct dirhandle_alsa_t *)_handle;

	debug_printf ("dir_alsa_readdir_cancel()\n");

	if (handle->hints_pcm)
	{
		snd_device_name_free_hint (handle->hints_pcm);
		handle->hints_pcm = 0;
	}

	if (handle->hints_ctl)
	{
		snd_device_name_free_hint (handle->hints_ctl);
		handle->hints_ctl = 0;
	}

	free (handle);
}

static void dir_alsa_update_mdb (uint32_t dirdb_ref, const char *descr)
{
	uint32_t mdb_ref;
	struct moduleinfostruct mi;

	mdb_ref = mdbGetModuleReference2 (dirdb_ref, 0);
	debug_printf (" mdbGetModuleReference2 (0x%08"PRIx32") => 0x%08"PRIx32"\n", dirdb_ref, mdb_ref);
	if (mdb_ref == 0xffffffff)
	{
		return;
	}

	mdbGetModuleInfo (&mi, mdb_ref);
	mi.channels=2;
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
					snprintf(mi.composer, n-descr+1, "%.*s", (int)sizeof (mi.composer) - 1, descr);
					snprintf(mi.comment, sizeof (mi.comment), "%s", n + 1);
					break;
				}
			} else {
				snprintf(mi.composer, sizeof(mi.composer), "%.*s", (int)sizeof (mi.composer) - 1, descr);
			}
		}
	}
	mdbWriteModuleInfo(mdb_ref, &mi);
}


static int dir_alsa_readdir_iterate (ocpdirhandle_pt _handle)
{
	struct dirhandle_alsa_t *handle = (struct dirhandle_alsa_t *)_handle;

	char *name, *descr, *io;
	char namebuffer[128];
	uint32_t dirdb_ref;
	struct ocpfile_t *child;
	int result;

	debug_printf ("dir_alsa_readdir_iterate() i=%d handle->hints_pcm=%p handle->hints_ctl=%p\n", handle->i, handle->hints_pcm, handle->hints_ctl);

	if (handle->hints_pcm && handle->hints_pcm[handle->i])
	{
		name = snd_device_name_get_hint(handle->hints_pcm[handle->i], "NAME");
		descr = snd_device_name_get_hint(handle->hints_pcm[handle->i], "DESC");
		io = snd_device_name_get_hint(handle->hints_pcm[handle->i], "IOID");

		debug_printf ("snd_device_name_get_hint()\n");
		debug_printf ("       [%d] name=%s\n", handle->i, name?name:"(NULL)");
		debug_printf ("            desc=%s\n", descr?descr:"(NULL)");
		debug_printf ("            io=%s\n", io?io:"(NULL)");

		if (!name) /* should never happen */
		{
			goto pcm_end;
		}
		if (io && (!strcmp(io, "Input"))) /* ignore input only cards */
		{
			goto pcm_end;
		}

//		snprintf (namebuffer, sizeof (namebuffer), "ALSA-PCM-%s.dev", name);
		snprintf (namebuffer, sizeof (namebuffer), "pcm-%03d.dev", handle->count);

		dirdb_ref = dirdbFindAndRef (handle->owner->dirdb_ref,  namebuffer, dirdb_use_file);
		dir_alsa_update_mdb (dirdb_ref, descr);
		dirdbUnref (dirdb_ref, dirdb_use_file);

		child = dev_file_create
		(
			handle->owner,
			namebuffer,
			name, /* mdb title */
			0,  /* mdb composer */
			0, /* token */
			alsaSetPCMInit,
			0, /* Run */
			0, /* Close */
			0  /* Destructor */
		);

		if (child)
		{
			handle->callback_file (handle->token, child);
			child->unref (child);
		}
		handle->count++;
pcm_end:
		free (name);
		free (descr);
		free (io);

		handle->i++;

		if (!handle->hints_pcm[handle->i])
		{
			debug_printf ("PCM complete, MIX next\n");

			snd_device_name_free_hint (handle->hints_pcm);
			handle->hints_pcm = 0;
			handle->i = 0;
			handle->count = 1;

			result=snd_device_name_hint(-1, "ctl", &handle->hints_ctl);
			debug_printf ("      snd_device_name_hint() = %s\n", snd_strerror(-result));
			if (result)
			{
				return 0;
			}
			return 1;
		}
		return 1;
	} else if (handle->hints_ctl && handle->hints_ctl[handle->i])
	{
		name = snd_device_name_get_hint(handle->hints_ctl[handle->i], "NAME");
		descr = snd_device_name_get_hint(handle->hints_ctl[handle->i], "DESC");
		io = snd_device_name_get_hint(handle->hints_ctl[handle->i], "IOID");

		debug_printf ("snd_device_name_get_hint()\n");
		debug_printf ("       [%d] name=%s\n", handle->i, name?name:"(NULL)");
		debug_printf ("            desc=%s\n", descr?descr:"(NULL)");
		debug_printf ("            io=%s\n", io?io:"(NULL)");

		if (!name) /* should never happen */
		{
			goto mix_end;
		}

//		snprintf (namebuffer, sizeof (namebuffer), "ALSA-MIX-%s.dev", name);
		snprintf (namebuffer, sizeof (namebuffer), "mix-%03d.dev", handle->count);

		dirdb_ref = dirdbFindAndRef (handle->owner->dirdb_ref,  namebuffer, dirdb_use_file);
		dir_alsa_update_mdb (dirdb_ref, descr);
		dirdbUnref (dirdb_ref, dirdb_use_file);

		child = dev_file_create
		(
			handle->owner,
			namebuffer,
			name, /* mdb title */
			0, /* mdb composer */
			0, /* token */
			alsaSetMixerInit,
			0, /* Run */
			0, /* Close */
			0  /* Destructor */
		);

		if (child)
		{
			handle->callback_file (handle->token, child);
			child->unref (child);
		}
		handle->count++;
mix_end:
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

static int detect_cards (void)
{
	int result;
	int retval = 0;
	int i;
	void **hints_pcm = 0;

	debug_printf ("ALSA_detect_card\n");

	result=snd_device_name_hint(-1, "pcm", &hints_pcm);
	debug_printf ("      snd_device_name_hint() = %s\n", snd_strerror(-result));

	if (result)
	{
		return retval; /* zero cards found */
	}

	for (i=0; hints_pcm[i]; i++)
	{
		char *name, *descr, *io;
		name = snd_device_name_get_hint(hints_pcm[i], "NAME");
		descr = snd_device_name_get_hint(hints_pcm[i], "DESC");
		io = snd_device_name_get_hint(hints_pcm[i], "IOID");

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

	snd_device_name_free_hint (hints_pcm);

	debug_printf (" => %d\n", retval);

	return retval;
}

static int devpALSAPlay (uint32_t *rate, enum plrRequestFormat *format, struct ocpfilehandle_t *source_file, struct cpifaceSessionAPI_t *cpifaceSession)
{
	int err;
	unsigned int uval, realdelay;
	int plrbufsize, buflength;
	/* start with setting default values, if we bail out */

	alsaOpenDevice();
	if (!alsa_pcm)
		return 0;

	devpALSAInPause = 0;
	devpALSAPauseSamples = 0;

	*format = PLR_STEREO_16BIT_SIGNED;

	debug_printf ("devpALSAPlay (rate=%d)\n", *rate);

	err=snd_pcm_hw_params_any(alsa_pcm, hwparams);
	debug_printf("      snd_pcm_hw_params_any(alsa_pcm, hwparams) = %s\n", snd_strerror(err<0?-err:0));
	if (err<0)
	{
		fprintf(stderr, "ALSA: snd_pcm_hw_params_any() failed: %s\n", snd_strerror(-err));
		return 0;
	}

	err=snd_pcm_hw_params_set_access(alsa_pcm, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED);
	debug_printf("      snd_pcm_hw_params_set_access(alsa_pcm, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED) = %s\n", snd_strerror(-err));
	if (err)
	{
		fprintf(stderr, "ALSA: snd_pcm_hw_params_set_access() failed: %s\n", snd_strerror(-err));
		return 0;
	}

	err=snd_pcm_hw_params_set_format(alsa_pcm, hwparams, SND_PCM_FORMAT_S16);
	debug_printf("      snd_pcm_hw_params_set_format(alsa_pcm, hwparams, SND_PCM_FORMAT_S16) = %s\n", snd_strerror(-err));
	if (err==0)
	{
		bit16=1;
		bitsigned=1;
	} else {
		err=snd_pcm_hw_params_set_format(alsa_pcm, hwparams, SND_PCM_FORMAT_U16);
		debug_printf("      snd_pcm_hw_params_set_format(alsa_pcm, hwparams, SND_PCM_FORMAT_U16) = %s\n", snd_strerror(-err));
		if (err==0)
		{
			bit16=1;
			bitsigned=0;
		} else {
			err=snd_pcm_hw_params_set_format(alsa_pcm, hwparams, SND_PCM_FORMAT_S8);
			debug_printf("      snd_pcm_hw_params_set_format(alsa_pcm, hwparams, SND_PCM_FORMAT_S8) = %s\n", snd_strerror(-err));
			if (err==0)
			{
				bit16=0;
				bitsigned=1;
			} else
			{
				err=snd_pcm_hw_params_set_format(alsa_pcm, hwparams, SND_PCM_FORMAT_U8);
				debug_printf("      snd_pcm_hw_params_set_format(alsa_pcm, hwparams, SND_PCM_FORMAT_U8) = %s\n", snd_strerror(-err));
				if (err==0)
				{
					bit16=0;
					bitsigned=0;
				} else {
					fprintf(stderr, "ALSA: snd_pcm_hw_params_set_format() failed: %s\n", snd_strerror(-err));
					bit16=1;
					bitsigned=1;
					return 0;
				}
			}
		}
	}

	uval=2;
	err=snd_pcm_hw_params_set_channels_near(alsa_pcm, hwparams, &uval);
	debug_printf("      snd_pcm_hw_params_set_channels_near(alsa_pcm, hwparams, &channels=%i) = %s\n", uval, snd_strerror(-err));
	if (err==0)
	{
		stereo=1;
	} else {
		uval=1;
		err=snd_pcm_hw_params_set_channels_near(alsa_pcm, hwparams, &uval);
		debug_printf("      snd_pcm_hw_params_set_channels_near(alsa_pcm, hwparams, &channels=%i) = %s\n", uval, snd_strerror(-err));
		if (err==0)
		{
			stereo=0;
		} else {
			fprintf(stderr, "ALSA: snd_pcm_hw_params_set_channels_near() failed: %s\n", snd_strerror(-err));
			stereo=1;
			return 0;
		}
	}

	if (!*rate)
	{
#if 0
		/* this gives values that are insane */
		uval=0;
		err=snd_pcm_hw_params_get_rate_max (hwparams, &uval, 0);
		debug_printf("      snd_pcm_hw_params_get_rate_max (hwparams, &rate = %u, 0) = %s\n", uval, snd_strerror(-err));
		*rate = uval;
#else
		*rate = 48000;
#endif
	}
	uval = *rate;
	err=snd_pcm_hw_params_set_rate_near(alsa_pcm, hwparams, &uval, 0);
	debug_printf("      snd_pcm_hw_params_set_rate_near(alsa_pcm, hwparams, &rate = %u, 0) = %s\n", uval, snd_strerror(-err));
	if (err<0)
	{
		fprintf(stderr, "ALSA: snd_pcm_hw_params_set_rate_near() failed: %s\n", snd_strerror(-err));
		return 0;
	}
	if (uval==0)
	{
		fprintf(stderr, "ALSA: No usable samplerate available.\n");
		return 0;
	}
	*rate = uval;
	devpALSARate = *rate;

	realdelay = 125000;
	err=snd_pcm_hw_params_set_buffer_time_near(alsa_pcm, hwparams, &realdelay, 0);
	debug_printf("      snd_pcm_hw_params_set_buffer_time_near(alsa_pcm, hwparams, 125000 uS => %u, 0) = %s\n", realdelay, snd_strerror(-err));
	if (err)
	{
		fprintf(stderr, "ALSA: snd_pcm_hw_params_set_buffer_time_near() failed: %s\n", snd_strerror(-err));
		return 0;
	}

	err=snd_pcm_hw_params(alsa_pcm, hwparams);
	debug_printf("      snd_pcm_hw_params(alsa_pcm, hwparams) = %s\n", snd_strerror(-err));
	if (err<0)
	{
		fprintf(stderr, "ALSA: snd_pcm_hw_params() failed: %s\n", snd_strerror(-err));
		return 0;
	}

	err=snd_pcm_sw_params_current(alsa_pcm, swparams);
	debug_printf("       snd_pcm_sw_params_current(alsa_pcm, swparams) = %s\n", snd_strerror(-err));
	if (err<0)
	{
		fprintf(stderr, "ALSA: snd_pcm_sw_params_any() failed: %s\n", snd_strerror(-err));
		return 0;
	}

	err=snd_pcm_sw_params(alsa_pcm, swparams);
	debug_printf("      snd_pcm_sw_params(alsa_pcm, swparams) = %s\n", snd_strerror(-err));
	if (err<0)
	{
		fprintf(stderr, "ALSA: snd_pcm_sw_params() failed: %s\n", snd_strerror(-err));
		return 0;
	}

	plrbufsize = cfGetProfileInt2(cfSoundSec, "sound", "plrbufsize", 200, 10);
	/* clamp the plrbufsize to be atleast 150ms and below 1000 ms */
	if (plrbufsize < 150)
	{
		plrbufsize = 150;
	}
	if (plrbufsize > 1000)
	{
		plrbufsize = 1000;
	}
	buflength = *rate * plrbufsize / 1000;

	realdelay = (uint64_t)*rate * (uint64_t)realdelay / 1000000;

	if (buflength < realdelay * 2)
	{
		buflength = realdelay * 2;
	}
	if (!(devpALSABuffer=calloc (buflength, 4)))
	{
		fprintf (stderr, "alsaPlay(): calloc() failed\n");
		return 0;
	}

	if ((!bit16) || (!stereo) || (!bitsigned))
	{
		devpALSAShadowBuffer = malloc ( buflength << ((!!bit16) + (!!stereo)));
		if (!devpALSAShadowBuffer)
		{
			fprintf (stderr, "alsaPlay(): malloc() failed #2\n");
			free (devpALSABuffer);
			devpALSABuffer = 0;
			return 0;
		}
	}

	if (!(devpALSARingBuffer = ringbuffer_new_samples (RINGBUFFER_FLAGS_STEREO | RINGBUFFER_FLAGS_16BIT | RINGBUFFER_FLAGS_SIGNED | RINGBUFFER_FLAGS_PROCESS, buflength)))
	{
		free (devpALSABuffer);
		devpALSABuffer = 0;
		free (devpALSAShadowBuffer);
		devpALSAShadowBuffer=0;
		return 0;
	}

#ifdef ALSA_DEBUG_OUTPUT
	debug_output = open ("test-alsa.raw", O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
#endif

	cpifaceSession->GetMasterSample = plrGetMasterSample;
	cpifaceSession->GetRealMasterVolume = plrGetRealMasterVolume;

	return 1;
}

static void devpALSAStop(void)
{
	free(devpALSABuffer); devpALSABuffer=0;
	free(devpALSAShadowBuffer); devpALSAShadowBuffer=0;
	if (devpALSARingBuffer)
	{
		ringbuffer_reset (devpALSARingBuffer);
		ringbuffer_free (devpALSARingBuffer);
		devpALSARingBuffer = 0;
	}

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
	                0, /* no speed optimization in doing direct file, due to iterator being the same code logic as dir_alsa_readdir_iterate() */
	                0,
			dirdbFindAndRef (dmSetup->basedir->dirdb_ref, "alsa", dirdb_use_dir),
	                0, /* refcount, ALSA ignores this */
	                0, /* is_archive */
	                0  /* is_playlist */);
	filesystem_setup_register_dir (&dir_alsa);

	dir_alsa_custom = dirdbFindAndRef (dir_alsa.dirdb_ref, "custom.dev", dirdb_use_file);

	custom_mdb_ref = mdbGetModuleReference2 (dir_alsa_custom, 0);

	update_custom_dev ();

	plrDevAPI = &devpALSA;

	alsaOpenDevice();
	if (!alsa_pcm)
	{
		alsaClose();
		return 0;
	}

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
	if (alsasetupregistered)
	{
		filesystem_setup_unregister_dir (&dir_alsa);
		dirdbUnref (dir_alsa.dirdb_ref, dirdb_use_dir);

		dirdbUnref (dir_alsa_custom, dirdb_use_file);
		dir_alsa_custom = 0xffffffff;

		alsasetupregistered = 0;
	}

	if (plrDevAPI  == &devpALSA)
	{
		plrDevAPI = 0;
	}
}

#define MIN(a,b) (((a)<(b))?a:b)

static int mlDrawBox (int esel, int first, const char texts[2][DEVICE_NAME_MAX+1], unsigned int *curpos, const struct DevInterfaceAPI_t *API)
{
	const int mlTop      = API->console->TextHeight / 2 - 3;
	const int maxwidth   = API->console->TextWidth - 10;
	const int fixedwidth = 1 + 7 + 1 + 1;
	int textwidth = MIN (DEVICE_NAME_MAX + 1, maxwidth - fixedwidth);
	int mlWidth = textwidth + fixedwidth;
	int i;

	const int mlLeft = (API->console->TextWidth - mlWidth) / 2;

	static int scrolled[2];

	for (i=0; i < 2; i++)
	{
		if (first)
		{
			scrolled[i] = 0;
		}

		while ((curpos[i]-scrolled[i]) >= textwidth)
			scrolled[i] += 8;
		while (scrolled[i] && (((int)curpos[i] - (int)scrolled[i]) < (textwidth - 8)))
			scrolled[i] -= 8;
	}

	API->console->DisplayPrintf (mlTop,     mlLeft, 0x04, mlWidth, "\xda" "%*C\xc4" "\xbf", mlWidth - 2);
	API->console->DisplayPrintf (mlTop + 1, mlLeft, 0x04, mlWidth, "\xb3" "%.11o" "%*C " "Please provide PCM and mixer strings" "%*C " "%.4o" "\xb3", (mlWidth - 36 - 2) / 2, (mlWidth - 36 - 2 + 1) / 2);

	API->console->DisplayPrintf (mlTop + 2, mlLeft, 0x04, mlWidth, "\xb3" "%.7o" "PCM:   " "%*.*o" "%.*s" "%0.7o" "%*C " "%.4o" "\xb3",
		(esel==0)?7:0, (esel==0)?0:7, textwidth, texts[0] + scrolled[0], mlWidth - 2 - 7 - textwidth);

	API->console->DisplayPrintf (mlTop + 3, mlLeft, 0x04, mlWidth, "\xb3" "%.7o" "Mixer: " "%*.*o" "%.*s" "%0.7o" "%*C " "%.4o" "\xb3",
		(esel==1)?7:0, (esel==1)?0:7, textwidth, texts[1] + scrolled[1], mlWidth - 2 - 7 - textwidth);

	API->console->DisplayPrintf (mlTop + 4, mlLeft, 0x04, mlWidth, "\xb3" "%.11o" "%*C " "-- Finish with enter, abort with escape --" "%*C " "%.4o" "\xb3", (mlWidth - 42 - 2) / 2, (mlWidth - 42 - 2 + 1) / 2);
	API->console->DisplayPrintf (mlTop + 5, mlLeft, 0x04, mlWidth, "\xc0" "%*C\xc4" "\xd9", mlWidth - 2);

	API->console->Driver->SetCursorPosition (mlTop + 2 + esel, mlLeft + 1 + 7 + curpos[esel] - scrolled[esel]);

	return mlTop;
}

static void alsaSetCustomRun (void **token, const struct DevInterfaceAPI_t *API)
{
	char str[2][DEVICE_NAME_MAX+1];
	unsigned int curpos[2];
	unsigned int cmdlen[2];
	int insmode[2]={1,1};
	int esel = 0;
	int first = 1;

	strcpy(str[0], alsaCardName);
	strcpy(str[1], alsaMixerName);

	curpos[0]=strlen(str[0]);
	curpos[1]=strlen(str[1]);

	cmdlen[0]=strlen(str[0]);
	cmdlen[1]=strlen(str[1]);

	API->console->Driver->SetCursorShape (1);

	while (1)
	{
		uint16_t key;

		fsDraw ();

		mlDrawBox (esel, first, str, curpos, API);
		first = 0;

		while (!API->console->KeyboardHit())
		{
			API->console->FrameLock();
		}
		key = API->console->KeyboardGetChar();

		if ((key>=0x20)&&(key<=0x7f))
		{
			if (insmode[esel])
			{
				if ((cmdlen[esel] + 1) < sizeof(str[esel]))
				{
					memmove (str[esel] + curpos[esel] + 1, str[esel] + curpos[esel], cmdlen[esel] - curpos[esel] + 1);
					str[esel][curpos[esel]++] = key;
					cmdlen[esel]++;
				}
			} else if (curpos[esel] == cmdlen[esel])
			{
				if ((cmdlen[esel] + 1) < sizeof(str[esel]))
				{
					str[esel][curpos[esel]++] = key;
					str[esel][curpos[esel]]= 0 ;
					cmdlen[esel]++;
				}
			} else {
				str[esel][curpos[esel]++] = key;
			}
		} else switch (key)
		{
			case KEY_EXIT:
			case KEY_ESC:
				API->console->Driver->SetCursorShape (0);
				return;
			case KEY_LEFT:
				if (curpos[esel])
				{
					curpos[esel]--;
				}
				break;
			case KEY_RIGHT:
				if (curpos[esel] < cmdlen[esel])
				{
					curpos[esel]++;
				}
				break;
			case KEY_UP:
				esel = 0;
				API->console->Driver->SetCursorShape (insmode[esel] ? 1 : 2);
				break;
			case KEY_DOWN:
				esel = 1;
				API->console->Driver->SetCursorShape  (insmode[esel] ? 1 : 2);
				break;
			case KEY_HOME:
				curpos[esel] = 0;
				break;
			case KEY_END:
				curpos[esel] = cmdlen[esel];
				break;
			case KEY_INSERT:
				{
					insmode[esel] = !insmode[esel];
					API->console->Driver->SetCursorShape (insmode[esel]?1:2);
				}
				break;
			case KEY_DELETE:
				if (curpos[esel] != cmdlen[esel])
				{
					memmove (str[esel] + curpos[esel], str[esel] + curpos[esel] + 1, cmdlen[esel] - curpos[esel]);
					cmdlen[esel]--;
				}
				break;
			case KEY_BACKSPACE:
				if (curpos[esel])
				{
					memmove (str[esel] + curpos[esel] - 1, str[esel] + curpos[esel], cmdlen[esel] - curpos[esel] + 1);
					curpos[esel]--;
					cmdlen[esel]--;
				}
				break;
			case _KEY_ENTER:
				strcpy (alsaCardName, str[0]);
				strcpy (alsaMixerName, str[1]);
				API->console->Driver->SetCursorShape (0);
				goto out;
		}
	}
out:
	debug_printf ("ALSA: Selected PCM %s\n", alsaCardName);
	debug_printf ("ALSA: Selected Mixer %s\n", alsaMixerName);

	update_custom_dev ();
}

static int alsaSetPCMInit (void **token, struct moduleinfostruct *info, struct ocpfilehandle_t *f, const struct DevInterfaceAPI_t *API)
{
	snprintf (alsaCardName, sizeof (alsaCardName), "%.*s", (unsigned int)sizeof (alsaCardName) - 1, info->title);

	debug_printf ("ALSA: Selected PCM %s\n", alsaCardName);
	debug_printf ("ALSA: Selected Mixer %s\n", alsaMixerName);

	update_custom_dev ();

	return 1;
}

static int alsaSetMixerInit (void **token, struct moduleinfostruct *info, struct ocpfilehandle_t *f, const struct DevInterfaceAPI_t *API)
{
	snprintf (alsaMixerName, sizeof (alsaMixerName), "%.*s", (unsigned int)sizeof (alsaMixerName) - 1, info->title);

	debug_printf ("ALSA: Selected PCM %s\n", alsaCardName);
	debug_printf ("ALSA: Selected Mixer %s\n", alsaMixerName);

	update_custom_dev ();

	return 1;
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

static struct ocpvolregstruct volalsa={volalsaGetNumVolume, volalsaGetVolume, volalsaSetVolume};
static const struct plrDevAPI_t devpALSA = {
	devpALSAIdle,
	devpALSAPeekBuffer,
	devpALSAPlay,
	devpALSAGetBuffer,
	devpALSAGetRate,
	devpALSAOnBufferCallback,
	devpALSACommitBuffer,
	devpALSAPause,
	devpALSAStop,
	&volalsa
};

struct sounddevice plrAlsa={SS_PLAYER, 1, "ALSA device driver", alsaDetect, alsaInit, alsaClose, NULL};

const char *dllinfo="driver plrAlsa";
DLLEXTINFO_DRIVER_PREFIX struct linkinfostruct dllextinfo = {.name = "devpalsa", .desc = "OpenCP Player Device: ALSA (c) 2005-'22 Stian Skjelstad", .ver = DLLVERSION, .sortindex = 99};
