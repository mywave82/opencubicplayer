/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Wavetable devices system
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
 *
 * revision history: (please note changes here)
 *  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *    -first release
 *  -kb980717   Tammo Hinrichs <opencp@gmx.net>
 *    -changed INI reading of driver symbols to _dllinfo lookup
 */

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "types.h"
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "devigen.h"
#include "deviwave.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/filesystem-drive.h"
#include "filesel/filesystem-file-mem.h"
#include "filesel/filesystem-setup.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "imsdev.h"
#include "mcp.h"
#include "player.h"
#include "stuff/compat.h"
#include "stuff/err.h"

int (*mcpProcessKey)(uint16_t);

static struct devinfonode *plWaveTableDevices;
static struct devinfonode *curwavedev;
static struct devinfonode *defwavedev;
static struct interfacestruct mcpIntr;
static struct preprocregstruct mcpPreprocess;

static struct devinfonode *getdevstr(struct devinfonode *n, const char *hnd)
{
	while (n)
	{
		if (!strcasecmp(n->handle, hnd))
			return n;
		n=n->next;
	}
	return 0;
}

static void setdevice(struct devinfonode **curdev, struct devinfonode *dev)
{
	if (*curdev==dev)
		return;
	if (*curdev)
	{
		if ((*curdev)->devinfo.devtype->addprocs)
			if ((*curdev)->devinfo.devtype->addprocs->Close)
				(*curdev)->devinfo.devtype->addprocs->Close();
		mcpProcessKey=0;
		(*curdev)->devinfo.devtype->Close();
		if (!(*curdev)->keep)
		{
			lnkFree((*curdev)->linkhand);
			(*curdev)->linkhand=-1;
		}
	}
	(*curdev)=0;
	if (!dev)
		return;

	if (dev->linkhand<0)
	{
		char lname[22];
		snprintf (lname, sizeof (lname), "%s", cfGetProfileString(dev->handle, "link", ""));
		dev->linkhand=lnkLink(lname);
		if (dev->linkhand<0)
		{
			fprintf(stderr, "device load error\n");
			return;
		}
		dev->devinfo.devtype=(struct sounddevice *)_lnkGetSymbol(lnkReadInfoReg(dev->linkhand, "driver"));
		if (!dev->devinfo.devtype)
		{
			fprintf(stderr, "device symbol error\n");
			lnkFree(dev->linkhand);
			dev->linkhand=-1;
			return;
		}
	}

	fprintf(stderr, "%s selected...\n", dev->name);
	if (dev->devinfo.devtype->Init(&dev->devinfo))
	{
		if (dev->devinfo.devtype->addprocs)
			if (dev->devinfo.devtype->addprocs->Init)
				dev->devinfo.devtype->addprocs->Init(dev->handle);
		if (dev->devinfo.devtype->addprocs)
			if (dev->devinfo.devtype->addprocs->ProcessKey)
				mcpProcessKey=dev->devinfo.devtype->addprocs->ProcessKey;
		(*curdev)=dev;
		return;
	}
	if (*curdev)
		if (!(*curdev)->keep)
		{
			lnkFree((*curdev)->linkhand);
			(*curdev)->linkhand=-1;
		}
	fprintf(stderr, "device init error\n");
}

static void mcpSetDevice(const char *name, int def)
{
	setdevice(&curwavedev, getdevstr(plWaveTableDevices, name));
	if (def)
		defwavedev=curwavedev;
}

static void mcpResetDevice(void)
{
	setdevice(&curwavedev, defwavedev);
}

struct file_devw_t
{
	struct ocpfile_t head;
//	uint32_t filesize;
	struct devinfonode *dev;
};

static void file_devw_ref (struct ocpfile_t *_self)
{
	struct file_devw_t *self = (struct file_devw_t *)_self;
	self->head.refcount++;
}
static void file_devw_unref (struct ocpfile_t *_self)
{
	struct file_devw_t *self = (struct file_devw_t *)_self;
	self->head.refcount--;
	if (!self->head.refcount)
	{
		dirdbUnref (self->head.dirdb_ref, dirdb_use_file);
		free (self);
	}
}

static struct ocpfilehandle_t *file_devw_open (struct ocpfile_t *_self)
{
	struct file_devw_t *self = (struct file_devw_t *)_self;
	char *buffer = strdup (mcpIntr.name);
	struct ocpfilehandle_t *retval = mem_filehandle_open (/*self->head.parent,*/ self->head.dirdb_ref, buffer, strlen (mcpIntr.name));
	if (!retval)
	{
		free (buffer);
	}
	return retval;
}

static uint64_t file_devw_filesize (struct ocpfile_t *_self)
{
	return strlen (mcpIntr.name);
}

static int file_devw_filesize_ready (struct ocpfile_t *_self)
{
	return 1;
}

static struct ocpdir_t dir_devw;

static void dir_devw_ref (struct ocpdir_t *self)
{
}
static void dir_devw_unref (struct ocpdir_t *self)
{
}

struct dir_devw_handle_t
{
	void (*callback_file)(void *token, struct ocpfile_t *);
	void *token;
	struct ocpdir_t *owner;
	struct devinfonode *next;
};
static ocpdirhandle_pt dir_devw_readdir_start (struct ocpdir_t *self, void (*callback_file)(void *token, struct ocpfile_t *),
	                                                              void (*callback_dir )(void *token, struct ocpdir_t  *), void *token)
{
	struct dir_devw_handle_t *retval = malloc (sizeof (*retval));
	if (!retval)
	{
		return 0;
	}
	retval->callback_file = callback_file;
	retval->token = token;
	retval->owner = self;
	retval->next = plWaveTableDevices;
	self->ref(self);
	return retval;
};

static void dir_devw_readdir_cancel (ocpdirhandle_pt _handle)
{
	struct dir_devw_handle_t *handle = (struct dir_devw_handle_t *)_handle;
	handle->owner->unref (handle->owner);
	free (handle);
}

static int dir_devw_readdir_iterate (ocpdirhandle_pt _handle)
{
	struct devinfonode *iter;
	struct dir_devw_handle_t *handle = (struct dir_devw_handle_t *)_handle;

	for (iter = plWaveTableDevices; iter; iter = iter->next)
	{
		if (handle->next == iter)
		{
			struct file_devw_t *file = malloc (sizeof (*file));
			if (file)
			{
				char npath[64];
				uint32_t mdb_ref;

				snprintf (npath, sizeof(npath), "%s.DEV", iter->handle);

				ocpfile_t_fill (&file->head,
				                 file_devw_ref,
				                 file_devw_unref,
				                 handle->owner,
				                 file_devw_open,
				                 file_devw_filesize,
				                 file_devw_filesize_ready,
				                 0, /* filename_override */
				                 dirdbFindAndRef (handle->owner->dirdb_ref, npath, dirdb_use_file),
				                 1, /* refcount */
				                 1  /* is_nodetect */);
				/* file->filesize = iter->devinfo.mem; */
				file->dev = iter;

				mdb_ref = mdbGetModuleReference2 (file->head.dirdb_ref, strlen (mcpIntr.name));
				if (mdb_ref!=0xffffffff)
				{
					struct moduleinfostruct mi;
					mdbGetModuleInfo(&mi, mdb_ref);
					mi.flags = MDB_VIRTUAL;
					mi.channels = iter->devinfo.chan;
					snprintf (mi.title, sizeof(mi.title), "%s", iter->name);
					mi.modtype.integer.i = MODULETYPE("DEVv");
					mdbWriteModuleInfo (mdb_ref, &mi);
				}
				handle->callback_file (handle->token, &file->head);
				file->head.unref (&file->head);
			}
			handle->next = iter->next;
			return 1;
		}
	}
	return 0;
}

static struct ocpdir_t *dir_devw_readdir_dir (struct ocpdir_t *_self, uint32_t dirdb_ref)
{
	/* this can not succeed */
	return 0;
}

static struct ocpfile_t *dir_devw_readdir_file (struct ocpdir_t *_self, uint32_t dirdb_ref)
{
	struct devinfonode *iter;
	const char *searchpath = 0;

	uint32_t parent_dirdb_ref;

/* assertion begin */
	parent_dirdb_ref = dirdbGetParentAndRef (dirdb_ref, dirdb_use_file);
	dirdbUnref (parent_dirdb_ref, dirdb_use_file);
	if (parent_dirdb_ref != _self->dirdb_ref)
	{
		fprintf (stderr, "dir_devw_readdir_file: dirdb_ref->parent is not the expected value\n");
		return 0;
	}
/* assertion end */

	dirdbGetName_internalstr (dirdb_ref, &searchpath);
	if (!searchpath)
	{
		return 0;
	}

	for (iter = plWaveTableDevices; iter; iter = iter->next)
	{
		char npath[64];
		struct file_devw_t *file;
		uint32_t mdb_ref;

		snprintf (npath, sizeof(npath), "%s.DEV", iter->handle);

		if (strcmp (npath, searchpath))
		{
			continue;
		}

		file = malloc (sizeof (*file));
		if (!file)
		{
			fprintf (stderr, "dir_devw_readdir_file: out of memory\n");
			return 0;
		}

		ocpfile_t_fill (&file->head,
		                 file_devw_ref,
		                 file_devw_unref,
		                 _self,
		                 file_devw_open,
		                 file_devw_filesize,
		                 file_devw_filesize_ready,
		                 0, /* filename_override */
		                 dirdbRef (dirdb_ref, dirdb_use_file),
		                 1, /* refcount */
		                 1  /* is_nodetect */);

		/* file->filesize = iter->devinfo.mem; */
		file->dev = iter;

		mdb_ref = mdbGetModuleReference2 (file->head.dirdb_ref, strlen (mcpIntr.name));
		if (mdb_ref!=0xffffffff)
		{
			struct moduleinfostruct mi;
			mdbGetModuleInfo(&mi, mdb_ref);
			mi.flags = MDB_VIRTUAL;
			mi.channels = iter->devinfo.chan;
			snprintf (mi.title, sizeof(mi.title), "%s", iter->name);
			mi.modtype.integer.i = MODULETYPE("DEVv");
			mdbWriteModuleInfo (mdb_ref, &mi);
		}

		return &file->head;
	}
	return 0;
}


static int wavedevinited = 0;
static int wavedevinit(void)
{
	const char *def;
	int playrate;

#ifdef INITCLOSE_DEBUG
	fprintf(stderr, "wavedevinit... trying to find all wavetables    [sound]->wavetabledevices\n");
#endif

	wavedevinited = 1;

	plRegisterInterface (&mcpIntr);
	plRegisterPreprocess (&mcpPreprocess);

	ocpdir_t_fill (&dir_devw,
	                dir_devw_ref,
	                dir_devw_unref,
	                dmSetup->basedir,
	                dir_devw_readdir_start,
	                0,
	                dir_devw_readdir_cancel,
	                dir_devw_readdir_iterate,
	                dir_devw_readdir_dir,
	                dir_devw_readdir_file,
	                0,
	                dirdbFindAndRef (dmSetup->basedir->dirdb_ref, "devw", dirdb_use_dir),
	                0, /* refcount, not used */
	                0, /* is_archive */
	                0  /* is_playlist */);
	filesystem_setup_register_dir (&dir_devw);

	if (!strlen(cfGetProfileString2(cfSoundSec, "sound", "wavetabledevices", "")))
		return errOk;
	fprintf(stderr, "wavetabledevices:\n");
	if (!deviReadDevices(cfGetProfileString2(cfSoundSec, "sound", "wavetabledevices", ""), &plWaveTableDevices))
	{
		fprintf(stderr, "could not install wavetable devices!\n");
		return errGen;
	}

	curwavedev=0;
	defwavedev=0;

	def=cfGetProfileString("commandline_s", "w", cfGetProfileString2(cfSoundSec, "sound", "defwavetable", ""));

	if (strlen(def))
		mcpSetDevice(def, 1);
	else
		if (plWaveTableDevices)
			mcpSetDevice(plWaveTableDevices->handle, 1);

	fprintf(stderr, "\n");

	playrate=cfGetProfileInt("commandline_s", "r", cfGetProfileInt2(cfSoundSec, "sound", "mixrate", 44100, 10), 10);
	if (playrate<66)
	{
		if (playrate%11)
			playrate*=1000;
		else
			playrate=playrate*11025/11;
	}
	mcpMixMaxRate=playrate;
	mcpMixProcRate=cfGetProfileInt2(cfSoundSec, "sound", "mixprocrate", 1536000, 10);

	if (!curwavedev)
	{
		fprintf (stderr, "Wave device/mixer not set\n");
		return errGen;
	}

	return errOk;
}

static void wavedevclose(void)
{
#ifdef INITCLOSE_DEBUG
	fprintf(stderr, "wavedevclose....\n");
#endif

	if (wavedevinited)
	{
		filesystem_setup_unregister_dir (&dir_devw);
		dirdbUnref (dir_devw.dirdb_ref, dirdb_use_dir);

		plUnregisterInterface (&mcpIntr);
		plUnregisterPreprocess (&mcpPreprocess);

		wavedevinited = 0;
	}

	setdevice(&curwavedev, 0);

	while (plWaveTableDevices)
	{
		struct devinfonode *o=plWaveTableDevices;
		plWaveTableDevices=plWaveTableDevices->next;
		free(o);
	}
}

static int mcpSetDev(struct moduleinfostruct *mi, struct ocpfilehandle_t *fp, const struct cpifaceplayerstruct *cp)
{
	const char *path;
	char *name;

	if (mi->modtype.integer.i != MODULETYPE("DEVv"))
	{
		return 0;
	}

	dirdbGetName_internalstr (fp->dirdb_ref, &path);
	splitpath4_malloc (path, 0, 0, &name, 0);

	mcpSetDevice(name, 1);

	free (name);

	return 0;
}

static void mcpPrep(struct moduleinfostruct *info, struct ocpfilehandle_t **bp)
{
	mcpResetDevice();
/*
	if (info->gen.moduleflags&MDB_BIGMODULE)           TODO
		mcpSetDevice(cfGetProfileString2(cfSoundSec, "sound", "bigmodules", ""), 0);
*/
}

static struct interfacestruct mcpIntr = {mcpSetDev, 0, 0, "mcpIntr" INTERFACESTRUCT_TAIL};
static struct preprocregstruct mcpPreprocess = {mcpPrep PREPROCREGSTRUCT_TAIL};
DLLEXTINFO_CORE_PREFIX struct linkinfostruct dllextinfo = {.name = "mcpbase", .desc = "OpenCP Wavetable Devices System (c) 1994-'22 Niklas Beisert, Tammo Hinrichs, Stian Skjelstad", .ver = DLLVERSION, .Init = wavedevinit, .Close = wavedevclose, .sortindex = 30};
