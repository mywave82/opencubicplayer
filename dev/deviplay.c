/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Player devices system
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
#include "deviplay.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/filesystem-drive.h"
#include "filesel/filesystem-file-dev.h"
#include "filesel/filesystem-setup.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "imsdev.h"
#include "player.h"
#include "stuff/compat.h"
#include "stuff/err.h"

static struct devinfonode *plPlayerDevices;
static struct devinfonode *curplaydev;
static struct devinfonode *defplaydev;
static struct preprocregstruct plrPreprocess;

int plrBufSize;

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
		snprintf (lname, sizeof(lname), "%s", cfGetProfileString(dev->handle, "link", ""));
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
	if (dev->devinfo.devtype->Init(&dev->devinfo, dev->handle))
	{
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

static void plrSetDevice(const char *name, int def)
{
	setdevice(&curplaydev, getdevstr(plPlayerDevices, name));
	if (def)
		defplaydev=curplaydev;
}

static void plrResetDevice(void)
{
	setdevice(&curplaydev, defplaydev);
}

static int dir_devp_file_Init (void **token, struct moduleinfostruct *info, struct ocpfilehandle_t *f, const struct DevInterfaceAPI_t *API)
{
	const char **device = (const char **)token;
	if (*device)
	{
		plrSetDevice(*device, 1);
	}
	return 1;
}

static struct ocpdir_t dir_devp;

static void dir_devp_ref (struct ocpdir_t *self)
{
}
static void dir_devp_unref (struct ocpdir_t *self)
{
}

struct dir_devp_handle_t
{
	void (*callback_file)(void *token, struct ocpfile_t *);
	void *token;
	struct ocpdir_t *owner;
	struct devinfonode *next;
};

static ocpdirhandle_pt dir_devp_readdir_start (struct ocpdir_t *self, void (*callback_file)(void *token, struct ocpfile_t *),
	                                                              void (*callback_dir )(void *token, struct ocpdir_t  *), void *token)
{
	struct dir_devp_handle_t *retval = malloc (sizeof (*retval));
	if (!retval)
	{
		return 0;
	}
	retval->callback_file = callback_file;
	retval->token = token;
	retval->owner = self;
	retval->next = plPlayerDevices;
	self->ref(self);
	return retval;
};

static void dir_devp_readdir_cancel (ocpdirhandle_pt _handle)
{
	struct dir_devp_handle_t *handle = (struct dir_devp_handle_t *)_handle;
	handle->owner->unref (handle->owner);
	free (handle);
}

static int dir_devp_readdir_iterate (ocpdirhandle_pt _handle)
{
	struct devinfonode *iter;
	struct dir_devp_handle_t *handle = (struct dir_devp_handle_t *)_handle;

	for (iter = plPlayerDevices; iter; iter = iter->next)
	{
		if (handle->next == iter)
		{
			char npath[64];
			struct ocpfile_t *file;

			snprintf (npath, sizeof(npath), "%s.DEV", iter->handle);

			file = dev_file_create
			(
				handle->owner,
				npath,
				iter->name, /* mdb title */
				"", /* mdb composer */
				strdup (iter->handle),
				dir_devp_file_Init,
				0, /* Run */
				0, /* Close */
				free /* Destructor */
			);

			if (file)
			{
				uint32_t mdb_ref = mdbGetModuleReference2 (file->dirdb_ref, 0);
				if (mdb_ref != 0xffffffff)
				{
					struct moduleinfostruct mi;
					mdbGetModuleInfo(&mi, mdb_ref);
					mi.channels = 2;
					mdbWriteModuleInfo (mdb_ref, &mi);
				}

				handle->callback_file (handle->token, file);
				file->unref (file);
			}

			handle->next = iter->next;
			return 1;
		}
	}
	return 0;
}

static struct ocpdir_t *dir_devp_readdir_dir (struct ocpdir_t *_self, uint32_t dirdb_ref)
{
	/* this can not succeed */
	return 0;
}

static struct ocpfile_t *dir_devp_readdir_file (struct ocpdir_t *_self, uint32_t dirdb_ref)
{
	struct devinfonode *iter;
	const char *searchpath = 0;

	uint32_t parent_dirdb_ref;

/* assertion begin */
	parent_dirdb_ref = dirdbGetParentAndRef (dirdb_ref, dirdb_use_file);
	dirdbUnref (parent_dirdb_ref, dirdb_use_file);
	if (parent_dirdb_ref != _self->dirdb_ref)
	{
		fprintf (stderr, "dir_devp_readdir_file: dirdb_ref->parent is not the expected value\n");
		return 0;
	}
/* assertion end */

	dirdbGetName_internalstr (dirdb_ref, &searchpath);
	if (!searchpath)
	{
		return 0;
	}

	for (iter = plPlayerDevices; iter; iter = iter->next)
	{
		char npath[64];
		struct ocpfile_t *file;
		uint32_t mdb_ref;

		snprintf (npath, sizeof(npath), "%s.DEV", iter->handle);

		if (strcmp (npath, searchpath))
		{
			continue;
		}

		file = dev_file_create
		(
			_self,
			npath,
			iter->name, /* mdb title */
			"", /* mdb composer */
			strdup (iter->handle),
			dir_devp_file_Init,
			0, /* Run */
			0, /* Close */
			free /* Destructor */
		);

		if (file)
		{
			mdb_ref = mdbGetModuleReference2 (file->dirdb_ref, 0);
			if (mdb_ref != 0xffffffff)
			{
				struct moduleinfostruct mi;
				mdbGetModuleInfo(&mi, mdb_ref);
				mi.channels = 2;
				mdbWriteModuleInfo (mdb_ref, &mi);
			}

			return file;
		}
	}
	return 0;
}

static int playdevinited = 0;
static int playdevinit(void)
{
	const char *def;

#ifdef INITCLOSE_DEBUG
	fprintf(stderr, "playdevinit... trying to init all sound devices [sound]->playerdevices\n");
#endif

	playdevinited = 1;

	plRegisterPreprocess (&plrPreprocess);

	ocpdir_t_fill (&dir_devp,
	                dir_devp_ref,
	                dir_devp_unref,
	                dmSetup->basedir,
	                dir_devp_readdir_start,
	                0,
	                dir_devp_readdir_cancel,
	                dir_devp_readdir_iterate,
	                dir_devp_readdir_dir,
	                dir_devp_readdir_file,
	                0,
	                dirdbFindAndRef (dmSetup->basedir->dirdb_ref, "devp", dirdb_use_dir),
	                0, /* refcount, not used */
	                0, /* is_archive */
	                0  /* is_playlist */);
	filesystem_setup_register_dir (&dir_devp);

	if (!strlen(cfGetProfileString2(cfSoundSec, "sound", "playerdevices", "")))
		return errOk;
	fprintf(stderr, "playerdevices:\n");
	if (!deviReadDevices(cfGetProfileString2(cfSoundSec, "sound", "playerdevices", ""), &plPlayerDevices))
	{
		fprintf(stderr, "could not install player devices!\n");
		return errGen;
	}

	curplaydev=0;
	defplaydev=0;

	def=cfGetProfileString("commandline_s", "p", cfGetProfileString2(cfSoundSec, "sound", "defplayer", ""));

	if (strlen(def))
		plrSetDevice(def, 1);
	else
		if (plPlayerDevices)
			plrSetDevice(plPlayerDevices->handle, 1);

	fprintf(stderr, "\n");

	plrBufSize=cfGetProfileInt2(cfSoundSec, "sound", "plrbufsize", 100, 10);
	if (plrBufSize <= 0)
	{
		plrBufSize = 1;
	}
	if (plrBufSize >= 5000)
	{
		plrBufSize = 5000;
	}

	if (!curplaydev)
	{
		fprintf (stderr, "Output device not set\n");
		return errGen;
	}

	return errOk;
}

static void playdevclose(void)
{
#ifdef INITCLOSE_DEBUG
	fprintf(stderr, "playdevclose...\n");
#endif

	if (playdevinited)
	{
		filesystem_setup_unregister_dir (&dir_devp);
		dirdbUnref (dir_devp.dirdb_ref, dirdb_use_dir);

		plUnregisterPreprocess (&plrPreprocess);

		playdevinited = 0;
	}

	setdevice(&curplaydev, 0);

	while (plPlayerDevices)
	{
		struct devinfonode *o=plPlayerDevices;
		plPlayerDevices=plPlayerDevices->next;
		free(o);
	}
}

static void plrPrep(struct moduleinfostruct *m, struct ocpfilehandle_t **bp)
{
	plrResetDevice();
}

static struct preprocregstruct plrPreprocess = {plrPrep PREPROCREGSTRUCT_TAIL};
DLLEXTINFO_CORE_PREFIX struct linkinfostruct dllextinfo = {.name = "plrbase", .desc = "OpenCP Player Devices System (c) 1994-'22 Niklas Beisert, Tammo Hinrichs, Stian Skjelstad", .ver = DLLVERSION, .Init = playdevinit, .Close = playdevclose, .sortindex = 30};
