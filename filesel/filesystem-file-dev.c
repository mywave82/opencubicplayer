/* OpenCP Module Player
 * copyright (c) 2020-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Code for gluing file-selector and .DEV files
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
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h" // interfaceReturnEnum
#include "filesel/filesystem-file-dev.h"

struct dev_ocpfile_t
{
	struct ocpfile_t  head;

	void *token;

	int  (*Init)       (void **token, struct moduleinfostruct *info, const struct DevInterfaceAPI_t *API); // Client can change token
	void (*Run)        (void **token,                                const struct DevInterfaceAPI_t *API); // Client can change token
	void (*Close)      (void **token,                                const struct DevInterfaceAPI_t *API); // Client can change token
	void (*Destructor) (void  *token);
};

struct dev_ocpfilehandle_t
{
	struct ocpfilehandle_t  head;
	struct dev_ocpfile_t   *owner;
	struct IOCTL_DevInterface devinterface;

	void *token;
};

static void dev_filehandle_ref (struct ocpfilehandle_t *_s)
{
	struct dev_ocpfilehandle_t *s = (struct dev_ocpfilehandle_t *)_s;
	s->head.refcount++;
}

static void dev_filehandle_unref (struct ocpfilehandle_t *_s)
{
	struct dev_ocpfilehandle_t *s = (struct dev_ocpfilehandle_t *)_s;
	s->head.refcount--;

	if (!s->head.refcount)
	{
		dirdbUnref (s->head.dirdb_ref, dirdb_use_filehandle);
		s->owner->head.unref (&s->owner->head);
		s->owner = 0;
		free (s);
	}
}

static int dev_filehandle_seek (struct ocpfilehandle_t *_s, int64_t pos)
{
	//struct dev_ocpfilehandle_t *s = (struct dev_ocpfilehandle_t *)_s;
	if (pos < 0) return -1;
	if (pos > 0) return -1;

	return 0;
}

static uint64_t dev_filehandle_getpos (struct ocpfilehandle_t *_s)
{
	//struct dev_ocpfilehandle_t *s = (struct dev_ocpfilehandle_t *)_s;

	return 0;
}

static int dev_filehandle_eof (struct ocpfilehandle_t *_s)
{
	//struct dev_ocpfilehandle_t *s = (struct dev_ocpfilehandle_t *)_s;

	return 1;
}

static int dev_filehandle_error (struct ocpfilehandle_t *_s)
{
	//struct dev_ocpfilehandle_t *s = (struct dev_ocpfilehandle_t *)_s;

	return 0;
}

static int dev_filehandle_read (struct ocpfilehandle_t *_s, void *dst, int len)
{
	//struct dev_ocpfilehandle_t *s = (struct dev_ocpfilehandle_t *)_s;

	return 0;
}

static uint64_t dev_filehandle_filesize (struct ocpfilehandle_t *_s)
{
	//struct dev_ocpfilehandle_t *s = (struct dev_ocpfilehandle_t *)_s;

	return 0;
}

static int dev_filehandle_filesize_ready (struct ocpfilehandle_t *_s)
{
	//struct dev_ocpfilehandle_t *s = (struct dev_ocpfilehandle_t *)_s;

	return 1;
}

static int DevInterface_Init (struct IOCTL_DevInterface *self, struct moduleinfostruct *info, const struct DevInterfaceAPI_t *API)
{
	struct dev_ocpfilehandle_t *s = (struct dev_ocpfilehandle_t *)((char *)self - offsetof(struct dev_ocpfilehandle_t, devinterface));
	if (s->owner->Init)
	{
		return s->owner->Init (&s->token, info, API);
	}
	return 1;
}

static interfaceReturnEnum DevInterface_Run (struct IOCTL_DevInterface *self, const struct DevInterfaceAPI_t *API)
{
	struct dev_ocpfilehandle_t *s = (struct dev_ocpfilehandle_t *)((char *)self - offsetof(struct dev_ocpfilehandle_t, devinterface));
	if (s->owner->Run)
	{
		s->owner->Run (&s->token, API);
	}
	return interfaceReturnNextAuto;
}

static void DevInterface_Close (struct IOCTL_DevInterface *self, const struct DevInterfaceAPI_t *API)
{
	struct dev_ocpfilehandle_t *s = (struct dev_ocpfilehandle_t *)((char *)self - offsetof(struct dev_ocpfilehandle_t, devinterface));
	if (s->owner->Close)
	{
		return s->owner->Close (&s->token, API);
	}
}

static int dev_filehandle_ioctl (struct ocpfilehandle_t *_handle, const char *cmd, void *ptr)
{
	struct dev_ocpfilehandle_t *s = (struct dev_ocpfilehandle_t *)_handle;

	if (!strcmp (cmd, IOCTL_DEVINTERFACE))
	{
		*(const struct IOCTL_DevInterface **)ptr = &s->devinterface;
		return 0;
	}
	return -1;
}

static struct ocpfilehandle_t *dev_file_open (struct ocpfile_t *_owner)
{
	struct dev_ocpfile_t *owner = (struct dev_ocpfile_t *)_owner;
	struct dev_ocpfilehandle_t *s = calloc (1, sizeof (*s));

	ocpfilehandle_t_fill
	(
		&s->head,
		dev_filehandle_ref,
		dev_filehandle_unref,
		_owner,
		dev_filehandle_seek,
		dev_filehandle_getpos,
		dev_filehandle_eof,
		dev_filehandle_error,
		dev_filehandle_read,
		dev_filehandle_ioctl, /* ioctl */
		dev_filehandle_filesize,
		dev_filehandle_filesize_ready,
		0, /* filename_override */
		dirdbRef (owner->head.dirdb_ref, dirdb_use_filehandle),
		1 /* refcount */
	);

	s->devinterface.Init  = DevInterface_Init;
	s->devinterface.Run   = DevInterface_Run;
	s->devinterface.Close = DevInterface_Close;

	s->owner = owner;
	_owner->ref (_owner);
	s->token = owner->token;

	return &s->head;
}

static void dev_file_ref (struct ocpfile_t *_s)
{
	struct dev_ocpfile_t *f = (struct dev_ocpfile_t *)_s;
	f->head.refcount++;
}

static void dev_file_unref (struct ocpfile_t *_s)
{
	struct dev_ocpfile_t *f = (struct dev_ocpfile_t *)_s;
	f->head.refcount--;

	if (!f->head.refcount)
	{
		dirdbUnref (f->head.dirdb_ref, dirdb_use_file);
		if (f->Destructor)
		{
			f->Destructor (f->token);
		}

		f->head.parent->unref (f->head.parent);
		free (f);
	}
}

static uint64_t dev_filesize (struct ocpfile_t *_s)
{
	//struct dev_ocpfile_t *f = (struct dev_ocpfile_t *)_s;

	return 0;
}

static int dev_filesize_ready (struct ocpfile_t *_s)
{
	//struct dev_ocpfile_t *f = (struct dev_ocpfile_t *)_s;

	return 1;
}

struct ocpfile_t *dev_file_create
(
	struct ocpdir_t *parent,
	const char *devname,
	const char *mdbtitle,
	const char *mdbcomposer,
	void *token,
	int  (*Init)       (void **token, struct moduleinfostruct *info, const struct DevInterfaceAPI_t *API),
	void (*Run)        (void **token,                                const struct DevInterfaceAPI_t *API),
	void (*Close)      (void **token,                                const struct DevInterfaceAPI_t *API),
	void (*Destructor) (void  *token)
)
{
	int dirdb_ref;
	struct dev_ocpfile_t *f;

	dirdb_ref = dirdbFindAndRef (parent->dirdb_ref, devname, dirdb_use_file);
	if (dirdb_ref == DIRDB_CLEAR)
	{
		fprintf (stderr, "dev_file_create: dirdbFindAndRef() failed\n");
		return 0;
	}

	f = calloc (1, sizeof (*f));

	if (!f)
	{
		fprintf (stderr, "dev_file_create: calloc() failed\n");
		return 0;
	}

	ocpfile_t_fill
	(
		&f->head,
		dev_file_ref,
		dev_file_unref,
		parent,
		dev_file_open,
		dev_filesize,
		dev_filesize_ready,
		0, /* filename_override */
		dirdb_ref, /* we already own this reference */
		1, /* refcount */
		1, /* is_nodetect */
	        COMPRESSION_NONE
	);

	parent->ref (parent);

	f->token = token;
	f->Init = Init;
	f->Run = Run;
	f->Close = Close;
	f->Destructor = Destructor;

	if (mdbtitle || mdbcomposer)
	{
		struct moduleinfostruct m;
		uint32_t mdbref;

		mdbref = mdbGetModuleReference2 (dirdb_ref, 0);

		mdbGetModuleInfo (&m, mdbref);

		m.flags = MDB_VIRTUAL;

		m.modtype.integer.i = MODULETYPE("DEVv");

		if (mdbtitle)
		{
			snprintf (m.title, sizeof (m.title), "%.*s", (int)(sizeof (m.title) - 1), mdbtitle);
		}
		if (mdbcomposer)
		{
			snprintf (m.composer, sizeof (m.composer), "%.*s", (int)(sizeof (m.composer) - 1), mdbcomposer);
		}

		mdbWriteModuleInfo (mdbref, &m);
	}

	return &f->head;
}
