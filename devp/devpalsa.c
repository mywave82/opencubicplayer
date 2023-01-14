/** OpenCP Module Player
 * copyright (c) 2005-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
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
#include "stuff/err.h"
#include "stuff/imsrtns.h"
#include "stuff/poutput.h"
#include "stuff/utf-8.h"

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

/****************************** setup/alsaconfig.dev ******************************/

enum alsaConfigDraw_Mode_t
{
	ACDM_AUDIO_DEVICE_SELECTED,
	ACDM_AUDIO_DEVICE_OPEN,
	ACDM_AUDIO_DEVICE_CUSTOM_SELECTED,
	ACDM_AUDIO_DEVICE_CUSTOM_EDIT,
	ACDM_MIXER_DEVICE_SELECTED,
	ACDM_MIXER_DEVICE_OPEN,
	ACDM_MIXER_DEVICE_CUSTOM_SELECTED,
	ACDM_MIXER_DEVICE_CUSTOM_EDIT
};

struct AlsaConfigDeviceEntry_t
{
	char *name;
	char *desc1;
	char *desc2;
};

struct AlsaConfigDeviceList_t
{
	struct AlsaConfigDeviceEntry_t *entries;
	int                             size;
	int                             fill;
	int                             preselected;
	int                             selected;
	char                            custom[DEVICE_NAME_MAX+1];
};

static struct ocpfile_t *alsasetup;

static void alsaSetupRun  (void **token, const struct DevInterfaceAPI_t *API);

static void alsaSetupAppendList (struct AlsaConfigDeviceList_t *list, char *name, char *desc)
{
	if (!name)
	{
		free (desc);
		return;
	}
	if (list->fill >= list->size)
	{
		void *temp = realloc (list->entries, (list->size + 10) * sizeof (list->entries[0]));
		if (!temp)
		{
			free (name);
			free (desc);
			return;
		}
		list->entries = temp;
		list->size += 10;
	}
	if (!strcmp (name, list->custom))
	{
		list->selected = list->fill;
	}
	list->entries[list->fill].name = name;
	list->entries[list->fill].desc1 = desc;
	if (desc) /* desc can contain one \n, which we try to split up */
	{
		list->entries[list->fill].desc2 = strchr (desc, '\n');
		if (list->entries[list->fill].desc2)
		{
			list->entries[list->fill].desc2[0] = 0;
			list->entries[list->fill].desc2++;
		}
	} else {
		list->entries[list->fill].desc2 = 0;
	}
	list->fill++;
}

static void alsaSetupClearList (struct AlsaConfigDeviceList_t *list)
{
	int i;
	for (i=0; i < list->fill; i++)
	{
		free (list->entries[i].name);
		free (list->entries[i].desc1); /* desc2 is from the same buffer, so should not be freed */
	}
	list->fill = 0;
	free (list->entries);
	list->size = 0;
}

static int alsaPluginInit (struct PluginInitAPI_t *API)
{
	alsasetup = dev_file_create (
		dmSetup->basedir,
		"alsaconfig.dev",
		"ALSA configuration",
		"",
		0, /* token */
		0, /* Init */
		alsaSetupRun,
		0, /* Close */
		0  /* Destructor */
	);

	filesystem_setup_register_file (alsasetup);

	return errOk;
}

static void alsaPluginClose (struct PluginCloseAPI_t *API)
{
	if (alsasetup)
	{
		filesystem_setup_unregister_file (alsasetup);
		alsasetup->unref (alsasetup);
		alsasetup = 0;
	}
}

static void alsaSetupDrawList (const int mlLeft, const int mlTop, const int mlWidth, const int mlHeight, const char *title, struct AlsaConfigDeviceList_t *list, const struct DevInterfaceAPI_t *API)
{
	int i;
	int scroll = 0;
	int overflow = list->fill - 12;

	if (overflow > 0)
	{
		if (list->preselected <= 6)
		{
		} else if (list->preselected > (list->fill - 6))
		{
			scroll = list->fill - 12;
		} else {
			scroll = list->preselected - 6;
		}
	}

	API->console->DisplayPrintf (mlTop + 1, mlLeft, 0x09, mlWidth, "\xb3    \xda\xc4 %s %*C\xc4\xbf    \xb3", title, 63 - strlen (title));
	for (i=0; i < 12; i++)
	{
		API->console->DisplayPrintf (mlTop + 2 + i, mlLeft, 0x09, mlWidth, "\xb3    \xb3%*.*o% 66s%0.9o\xb3    \xb3",
				(i + scroll == list->preselected) ?  8 : 0,
				(i + scroll == 0) ? 10 : 15,
				((i + scroll) >= list->fill) ? "" : list->entries[i + scroll].name);
	}
	API->console->DisplayPrintf (mlTop + 14, mlLeft, 0x09, mlWidth, "\xb3    \xc0%8C\xc4 Use arrow keys and select with enter or cancel with ESC \xc4\xd9    \xb3");
	API->console->DisplayPrintf (mlTop + 15, mlLeft, 0x09, mlWidth, "\xb3    %0.7o% 72s%0.9o\xb3", list->entries[list->preselected].desc1 ? list->entries[list->preselected].desc1 : "(no description)");
	API->console->DisplayPrintf (mlTop + 16, mlLeft, 0x09, mlWidth, "\xb3    %0.7o% 72s%0.9o\xb3", list->entries[list->preselected].desc2 ? list->entries[list->preselected].desc2 : "");
}

static void alsaSetupDraw (const int mlLeft, const int mlTop, const int mlWidth, const int mlHeight, enum alsaConfigDraw_Mode_t *alsaConfigDraw_Mode, struct AlsaConfigDeviceList_t *pcmlist, struct AlsaConfigDeviceList_t *mixerlist, const struct DevInterfaceAPI_t *API)
{
	API->console->DisplayPrintf (mlTop +  0, mlLeft, 0x09, mlWidth, "\xda%28C\xc4 ALSA configuration %28C\xc4\xbf");

	if (*alsaConfigDraw_Mode == ACDM_AUDIO_DEVICE_OPEN)
	{
		alsaSetupDrawList (mlLeft, mlTop, mlWidth, mlHeight, "Select audio device", pcmlist, API);

	} else if (*alsaConfigDraw_Mode == ACDM_MIXER_DEVICE_OPEN)
	{
		alsaSetupDrawList (mlLeft, mlTop, mlWidth, mlHeight, "Select mixer device", mixerlist, API);
	} else {
		API->console->DisplayPrintf (mlTop + 1, mlLeft, 0x09, mlWidth, "\xb3%.7o%.15o  Navigate with %.15o<\x18>%.7o,%.15o<\x19>%.7o and %.15o<ENTER>%.7o; hit %.15o<ESC>%.7o to save and exit.            %.9o\xb3");
		API->console->DisplayPrintf (mlTop + 2, mlLeft, 0x09, mlWidth, "\xc3%*C\xc4\xb4", mlWidth - 2);
		//API->console->DisplayPrintf (mlTop +  2, mlLeft, 0x09, mlWidth, "\xb3%76C \xb3");
		API->console->DisplayPrintf (mlTop +  3, mlLeft, 0x09, mlWidth, "\xb3%0.7o Audio device:%62C %0.9o\xb3");
		/* TODO draw selected item */
		API->console->DisplayPrintf (mlTop +  4, mlLeft, 0x09, mlWidth, "\xb3%0.7o    %*.15o[% 66s]%0.9o    \xb3",
			*alsaConfigDraw_Mode==ACDM_AUDIO_DEVICE_SELECTED ?  8 : 0,
			pcmlist->entries[pcmlist->selected].name);
		if (!pcmlist->selected) /* custom */
		{
			if (*alsaConfigDraw_Mode==ACDM_AUDIO_DEVICE_CUSTOM_EDIT)
			{
				API->console->DisplayPrintf (mlTop +  5, mlLeft,               0x09, 5, "\xb3    ");
				API->console->DisplayPrintf (mlTop +  5, mlLeft + mlWidth - 5, 0x09, 5, "    \xb3");
				/* we delay the EditStringUTF8z(), since it will trigger keyboard input + framelock */
			} else {
				API->console->DisplayPrintf (mlTop +  5, mlLeft, 0x09, mlWidth, "\xb3%0.7o    %*.15o%.68S%0.9o    \xb3",
					*alsaConfigDraw_Mode==ACDM_AUDIO_DEVICE_CUSTOM_SELECTED ?  8 : 0,
					pcmlist->custom);
			}
		} else {
			API->console->DisplayPrintf (mlTop +  5, mlLeft, 0x09, mlWidth, "\xb3    -%71C \xb3");
		}
		API->console->DisplayPrintf (mlTop +  6, mlLeft, 0x09, mlWidth, "\xb3%0.7o Description:%63C %0.9o\xb3");
		API->console->DisplayPrintf (mlTop +  7, mlLeft, 0x09, mlWidth, "\xb3%0.7o    %0.7o% 72s%0.9o\xb3", pcmlist->entries[pcmlist->selected].desc1 ? pcmlist->entries[pcmlist->selected].desc1 : "(no description)");
		API->console->DisplayPrintf (mlTop +  8, mlLeft, 0x09, mlWidth, "\xb3%0.7o    %0.7o% 72s%0.9o\xb3", pcmlist->entries[pcmlist->selected].desc2 ? pcmlist->entries[pcmlist->selected].desc2 : "");

		API->console->DisplayPrintf (mlTop +  9, mlLeft, 0x09, mlWidth, "\xb3%76C \xb3");
		API->console->DisplayPrintf (mlTop + 10, mlLeft, 0x09, mlWidth, "\xb3%76C \xb3");
		API->console->DisplayPrintf (mlTop + 11, mlLeft, 0x09, mlWidth, "\xb3%0.7o Mixer device (volume control):%45C %0.9o\xb3");
		API->console->DisplayPrintf (mlTop + 12, mlLeft, 0x09, mlWidth, "\xb3%0.7o    %*.15o[% 66s]%0.9o    \xb3",
			*alsaConfigDraw_Mode==ACDM_MIXER_DEVICE_SELECTED ?  8 : 0,
			mixerlist->entries[mixerlist->selected].name);
		if (!mixerlist->selected) /* custom */
		{
			if (*alsaConfigDraw_Mode==ACDM_MIXER_DEVICE_CUSTOM_EDIT)
			{
				API->console->DisplayPrintf (mlTop + 13, mlLeft,               0x09, 5, "\xb3    ");
				API->console->DisplayPrintf (mlTop + 13, mlLeft + mlWidth - 5, 0x09, 5, "    \xb3");
				/* we delay the EditStringUTF8z(), since it will trigger keyboard input + framelock */
			} else {
				API->console->DisplayPrintf (mlTop + 13, mlLeft, 0x09, mlWidth, "\xb3%0.7o    %*.15o%.68S%0.9o    \xb3",
					*alsaConfigDraw_Mode==ACDM_MIXER_DEVICE_CUSTOM_SELECTED ?  8 : 0,
					mixerlist->custom);
			}
		} else {
			API->console->DisplayPrintf (mlTop + 13, mlLeft, 0x09, mlWidth, "\xb3    -%71C \xb3");
		}
		API->console->DisplayPrintf (mlTop + 14, mlLeft, 0x09, mlWidth, "\xb3%0.7o Description:%63C %0.9o\xb3");
		API->console->DisplayPrintf (mlTop + 15, mlLeft, 0x09, mlWidth, "\xb3%0.7o    %0.7o% 72s%0.9o\xb3", mixerlist->entries[mixerlist->selected].desc1 ? mixerlist->entries[mixerlist->selected].desc1 : "(no description)");
		API->console->DisplayPrintf (mlTop + 16, mlLeft, 0x09, mlWidth, "\xb3%0.7o    %0.7o% 72s%0.9o\xb3", mixerlist->entries[mixerlist->selected].desc2 ? mixerlist->entries[mixerlist->selected].desc2 : "");
		//API->console->DisplayPrintf (mlTop + 17, mlLeft, 0x09, mlWidth, "\xb3%76C \xb3");
	}
	API->console->DisplayPrintf (mlTop + 17, mlLeft, 0x09, mlWidth, "\xc0%76C\xc4\xd9");

	if (*alsaConfigDraw_Mode==ACDM_AUDIO_DEVICE_CUSTOM_EDIT)
	{
#warning UTF-8 the way to go?
		if (EditStringUTF8z (mlTop + 5, mlLeft + 5, 68, sizeof (pcmlist->custom), pcmlist->custom) <= 0)
		{
			*alsaConfigDraw_Mode = ACDM_AUDIO_DEVICE_CUSTOM_SELECTED;
		}
	}
	if (*alsaConfigDraw_Mode==ACDM_MIXER_DEVICE_CUSTOM_EDIT)
	{
#warning UTF-8 the way to go?
		if (EditStringUTF8z (mlTop + 13, mlLeft + 5, 68, sizeof (mixerlist->custom), mixerlist->custom) <= 0)
		{
			*alsaConfigDraw_Mode = ACDM_MIXER_DEVICE_CUSTOM_SELECTED;
		}
	}
}

static void alsaSetupScan (struct AlsaConfigDeviceList_t *pcm, struct AlsaConfigDeviceList_t *mix)
{
	int result, i;
	void **hints;

	alsaSetupAppendList (pcm, strdup ("custom"), strdup ("User supplied value"));
	alsaSetupAppendList (mix, strdup ("custom"), strdup ("User supplied value"));

	result=snd_device_name_hint(-1, "pcm", &hints);
	if (!result)
	{
		for (i=0; hints[i]; i++)
		{
			char *name = snd_device_name_get_hint (hints[i], "NAME");
			char *desc = snd_device_name_get_hint (hints[i], "DESC");
			char *io   = snd_device_name_get_hint (hints[i], "IOID");

			if (name && ((!io) || strcmp (io, "Input")))
			{
				alsaSetupAppendList (pcm, name, desc);
				free (io);
			} else {
				free (name);
				free (desc);
				free (io);
			}
		}
		snd_device_name_free_hint (hints);
	}

	result=snd_device_name_hint(-1, "ctl", &hints);
	if (!result)
	{
		for (i=0; hints[i]; i++)
		{
			char *name = snd_device_name_get_hint (hints[i], "NAME");
			char *desc = snd_device_name_get_hint (hints[i], "DESC");

			alsaSetupAppendList (mix, name, desc);
		}

		snd_device_name_free_hint (hints);
	}
}

static void alsaSetupRun (void **token, const struct DevInterfaceAPI_t *API)
{
	enum alsaConfigDraw_Mode_t alsaConfigDraw_Mode = ACDM_AUDIO_DEVICE_SELECTED;
	struct AlsaConfigDeviceList_t pcmlist = {0, 0, 0};
	struct AlsaConfigDeviceList_t mixerlist = {0, 0, 0};
	int doexit = 0;

	snprintf (  pcmlist.custom, sizeof (  pcmlist.custom), "%s", alsaCardName);
	snprintf (mixerlist.custom, sizeof (mixerlist.custom), "%s", alsaMixerName);

	alsaSetupScan (&pcmlist, &mixerlist);

	while (!doexit)
	{
		const int mlWidth = 78;
		const int mlHeight = 18;
		int mlTop = (plScrHeight - mlHeight) / 2;
		int mlLeft = (plScrWidth - mlWidth) / 2;

		API->fsDraw();
		alsaSetupDraw (mlLeft, mlTop, mlWidth, mlHeight, &alsaConfigDraw_Mode, &pcmlist, &mixerlist, API);
		if ((alsaConfigDraw_Mode == ACDM_AUDIO_DEVICE_CUSTOM_EDIT) ||
		    (alsaConfigDraw_Mode == ACDM_MIXER_DEVICE_CUSTOM_EDIT))
		{
			continue;
		}
		while (API->console->KeyboardHit() && !doexit && (alsaConfigDraw_Mode != ACDM_AUDIO_DEVICE_CUSTOM_EDIT) && (alsaConfigDraw_Mode != ACDM_MIXER_DEVICE_CUSTOM_EDIT))
		{
			int key = API->console->KeyboardGetChar();
			switch (key)
			{
				case KEY_DOWN:
					switch (alsaConfigDraw_Mode)
					{
						case ACDM_AUDIO_DEVICE_SELECTED:
							if (pcmlist.selected) /* !custom */
							{
								alsaConfigDraw_Mode = ACDM_MIXER_DEVICE_SELECTED;
							} else {
								alsaConfigDraw_Mode = ACDM_AUDIO_DEVICE_CUSTOM_SELECTED;
							}
							break;
						case ACDM_AUDIO_DEVICE_OPEN:
							if (pcmlist.preselected + 1 < pcmlist.fill)
							{
								pcmlist.preselected++;
							}
							break;
						case ACDM_AUDIO_DEVICE_CUSTOM_SELECTED:
							alsaConfigDraw_Mode = ACDM_MIXER_DEVICE_SELECTED;
							break;
						case ACDM_AUDIO_DEVICE_CUSTOM_EDIT:
							break;
						case ACDM_MIXER_DEVICE_SELECTED:
							if (!mixerlist.selected) /* !custom */
							{
								alsaConfigDraw_Mode = ACDM_MIXER_DEVICE_CUSTOM_SELECTED;
							}
							break;
						case ACDM_MIXER_DEVICE_OPEN:
							if (mixerlist.preselected + 1 < mixerlist.fill)
							{
								mixerlist.preselected++;
							}
							break;
						case ACDM_MIXER_DEVICE_CUSTOM_SELECTED:
							break;
						case ACDM_MIXER_DEVICE_CUSTOM_EDIT:
							break;
					}
					break;
				case KEY_UP:
					switch (alsaConfigDraw_Mode)
					{
						case ACDM_AUDIO_DEVICE_SELECTED:
							break;
						case ACDM_AUDIO_DEVICE_OPEN:
							if (pcmlist.preselected > 0)
							{
								pcmlist.preselected--;
							}
							break;
						case ACDM_AUDIO_DEVICE_CUSTOM_SELECTED:
							alsaConfigDraw_Mode = ACDM_AUDIO_DEVICE_SELECTED;
							break;
						case ACDM_MIXER_DEVICE_SELECTED:
							if (pcmlist.selected) /* !custom */
							{
									alsaConfigDraw_Mode = ACDM_AUDIO_DEVICE_SELECTED;
							} else {
									alsaConfigDraw_Mode = ACDM_AUDIO_DEVICE_CUSTOM_SELECTED;
							}
							break;
						case ACDM_AUDIO_DEVICE_CUSTOM_EDIT:
							break;
						case ACDM_MIXER_DEVICE_OPEN:
							if (mixerlist.preselected > 0)
							{
								mixerlist.preselected--;
							}
							break;
						case ACDM_MIXER_DEVICE_CUSTOM_SELECTED:
							alsaConfigDraw_Mode = ACDM_MIXER_DEVICE_SELECTED;
							break;
						case ACDM_MIXER_DEVICE_CUSTOM_EDIT:
							break;
					}
					break;
				case KEY_EXIT:
						doexit = 1;
						break;
				case KEY_ESC:
					switch (alsaConfigDraw_Mode)
					{
						case ACDM_AUDIO_DEVICE_SELECTED:
							doexit = 1;
							break;
						case ACDM_AUDIO_DEVICE_OPEN:
							alsaConfigDraw_Mode = ACDM_AUDIO_DEVICE_SELECTED;
							break;
						case ACDM_AUDIO_DEVICE_CUSTOM_SELECTED:
							doexit = 1;
							break;
						case ACDM_AUDIO_DEVICE_CUSTOM_EDIT:
							alsaConfigDraw_Mode = ACDM_AUDIO_DEVICE_CUSTOM_SELECTED;
							API->console->Driver->SetCursorShape (0);
							break;
						case ACDM_MIXER_DEVICE_SELECTED:
							doexit = 1;
							break;
						case ACDM_MIXER_DEVICE_OPEN:
							alsaConfigDraw_Mode = ACDM_MIXER_DEVICE_SELECTED;
							break;
						case ACDM_MIXER_DEVICE_CUSTOM_SELECTED:
							doexit = 1;
							break;
						case ACDM_MIXER_DEVICE_CUSTOM_EDIT:
							alsaConfigDraw_Mode = ACDM_MIXER_DEVICE_CUSTOM_SELECTED;
							API->console->Driver->SetCursorShape (0);
							break;
					}
					break;
				case _KEY_ENTER:
					switch (alsaConfigDraw_Mode)
					{
						case ACDM_AUDIO_DEVICE_SELECTED:
							alsaConfigDraw_Mode = ACDM_AUDIO_DEVICE_OPEN;
							pcmlist.preselected = pcmlist.selected;
							break;
						case ACDM_AUDIO_DEVICE_OPEN:
							alsaConfigDraw_Mode = ACDM_AUDIO_DEVICE_SELECTED;
							pcmlist.selected = pcmlist.preselected;
							break;
						case ACDM_AUDIO_DEVICE_CUSTOM_SELECTED:
							alsaConfigDraw_Mode = ACDM_AUDIO_DEVICE_CUSTOM_EDIT;
							break;
						case ACDM_AUDIO_DEVICE_CUSTOM_EDIT:
							break;
						case ACDM_MIXER_DEVICE_SELECTED:
							alsaConfigDraw_Mode = ACDM_MIXER_DEVICE_OPEN;
							mixerlist.preselected = mixerlist.selected;
							break;
						case ACDM_MIXER_DEVICE_OPEN:
							alsaConfigDraw_Mode = ACDM_MIXER_DEVICE_SELECTED;
							mixerlist.selected = mixerlist.preselected;
							break;
						case ACDM_MIXER_DEVICE_CUSTOM_SELECTED:
							alsaConfigDraw_Mode = ACDM_MIXER_DEVICE_CUSTOM_EDIT;
							break;
						case ACDM_MIXER_DEVICE_CUSTOM_EDIT:
							break;
					}
					break;
			}
		}
		API->console->FrameLock();
	}

	if (pcmlist.selected)
	{
		snprintf (alsaCardName, sizeof (alsaCardName), "%.*s", (unsigned int)sizeof (alsaCardName) - 1, pcmlist.entries[pcmlist.selected].name);
	} else {
		snprintf (alsaCardName, sizeof (alsaCardName), "%.*s", (unsigned int)sizeof (alsaCardName) - 1, pcmlist.custom);
	}
	if (mixerlist.selected)
	{
		snprintf (alsaMixerName, sizeof (alsaMixerName), "%.*s", (unsigned int)sizeof (alsaMixerName) - 1, mixerlist.entries[mixerlist.selected].name);
	} else {
		snprintf (alsaMixerName, sizeof (alsaMixerName), "%.*s", (unsigned int)sizeof (alsaMixerName) - 1, mixerlist.custom);
	}

	debug_printf ("ALSA: Selected PCM %s\n", alsaCardName);
	debug_printf ("ALSA: Selected Mixer %s\n", alsaMixerName);

	alsaSetupClearList (&pcmlist);
	alsaSetupClearList (&mixerlist);

	API->configAPI->SetProfileString ("devpALSA", "path", alsaCardName);
	API->configAPI->SetProfileString ("devpALSA", "mixer", alsaMixerName);
	API->configAPI->StoreConfig ();
}

/****************************** devpALSA ******************************/

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
	cpifaceSession->plrActive = 1;

	return 1;
}

static void devpALSAStop (struct cpifaceSessionAPI_t *cpifaceSession)
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

	cpifaceSession->plrActive = 0;
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

static void alsaClose(void);
static int alsaInit(const struct deviceinfo *c, const char *handle)
{
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

	card->subtype = -1;

	return cards > 0;
}

static void alsaClose(void)
{
	if (plrDevAPI  == &devpALSA)
	{
		plrDevAPI = 0;
	}
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
	&volalsa,
	0 /* ProcessKey */
};

struct sounddevice plrAlsa =
{
	SS_PLAYER,
	1,
	"ALSA device driver",
	alsaDetect,
	alsaInit,
	alsaClose,
	0 /* GetOpt */
};

const char *dllinfo="driver plrAlsa";
DLLEXTINFO_DRIVER_PREFIX struct linkinfostruct dllextinfo = {.name = "devpalsa", .desc = "OpenCP Player Device: ALSA (c) 2005-'23 Stian Skjelstad", .ver = DLLVERSION, .sortindex = 99, .PluginInit = alsaPluginInit, .PluginClose = alsaPluginClose};
