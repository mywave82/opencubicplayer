/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'23 Stian Sebastian Skjelstad <stian.skjelstad@gmail.com>
 *
 * UNIX cdrom filebrowser
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
 *  -ss040907   Stian Skjelstad <stian@nixia.no>
 *    -first release
 *  -ss040920   Stian Skjelstad <stian@nixia.no>
 *    -Duplicate filedescriptor, so we can try to avoid the kernel re-reading
 *     the toc when we access the cdrom from the fileselector while in use
 */

#include <linux/cdrom.h>
#include "config.h"
#include <discid/discid.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "types.h"
#include "boot/plinkman.h"
#include "cdrom.h"
#include "dirdb.h"
#include "filesystem.h"
#include "filesystem-drive.h"
#include "filesystem-file-mem.h"
#include "mdb.h"
#include "musicbrainz.h"
#include "pfilesel.h"
#include "stuff/err.h"

static struct cdrom_t
{
	char dev[32];
	char vdev[12];
	int caps;
	int fd;

	pthread_mutex_t mutex;
	pthread_cond_t cond;
	pthread_t thread;
	struct ioctl_cdrom_readaudio_request_t *request;
	int request_complete;
	int request_returnvalue;
	int shutdown;

	struct ioctl_cdrom_readtoc_request_t lasttoc;
	char *lastdiscid;
	char *lasttocstr;
	void *musicbrainzhandle;
	struct musicbrainz_database_h *musicbrainzdata;
} *cdroms = 0;
static int cdromn = 0;

static struct dmDrive *dmCDROM=0;
static struct ocpdir_t cdrom_root;

struct cdrom_drive_ocpdir_t
{
	struct ocpdir_t head;
	struct cdrom_t *cdrom;
};

struct cdrom_track_ocpfile_t
{
	struct ocpfile_t head;
	struct cdrom_t *cdrom;
	int trackno;
	char trackfilename[13];
};

static void cdrom_root_ref (struct ocpdir_t *);
static void cdrom_root_unref (struct ocpdir_t *);
static ocpdirhandle_pt cdrom_root_readdir_start (struct ocpdir_t *, void(*callback_file)(void *token, struct ocpfile_t *),
                                                                    void(*callback_dir )(void *token, struct ocpdir_t *), void *token);
static void cdrom_root_readdir_cancel (ocpdirhandle_pt);
static int cdrom_root_readdir_iterate (ocpdirhandle_pt);
static struct ocpdir_t *cdrom_root_readdir_dir (struct ocpdir_t *_self, uint32_t dirdb_ref);
static struct ocpfile_t *cdrom_root_readdir_file (struct ocpdir_t *_self, uint32_t dirdb_ref);

static void cdrom_drive_ref (struct ocpdir_t *);
static void cdrom_drive_unref (struct ocpdir_t *);
static ocpdirhandle_pt cdrom_drive_readdir_start (struct ocpdir_t *, void(*callback_file)(void *token, struct ocpfile_t *),
                                                                     void(*callback_dir )(void *token, struct ocpdir_t *), void *token);
static void cdrom_drive_readdir_cancel (ocpdirhandle_pt);
static int cdrom_drive_readdir_iterate (ocpdirhandle_pt);

static void cdrom_track_ref (struct ocpfile_t *);
static void cdrom_track_unref (struct ocpfile_t *);
static struct ocpfilehandle_t *cdrom_track_open (struct ocpfile_t *);
static uint64_t cdrom_track_filesize (struct ocpfile_t *);
static int cdrom_track_filesize_ready (struct ocpfile_t *);

static void try(const char *dev, const char *vdev)
{
	char temppath[256];
	struct cdrom_t *temp;
	int fd;
#ifdef CDROM_VERBOSE
	fprintf(stderr, "Testing %s\n", dev);
#endif
	bzero (temppath, sizeof (temppath));
	if (readlink (dev, temppath, sizeof (temppath) - 1) > 0)
	{
		if (strncmp (temppath, "/dev/sr", 7)) return;
		if (strncmp (temppath, "/dev/hd", 7)) return;
		if (strncmp (temppath, "/dev/scd", 8)) return;
	}
	if ((fd=open(dev, O_RDONLY|O_NONBLOCK))>=0)
	{
		int caps;

		if ((caps=ioctl(fd, CDROM_GET_CAPABILITY, 0))>=0)
		{
			temp = realloc (cdroms, sizeof (struct cdrom_t) * (cdromn+1));
			if (!temp)
			{
				fprintf (stderr, "cdrom.c: realloc() failed in try()\n");
				close (fd);
				return;
			}
			cdroms = temp;

			strcpy(cdroms[cdromn].dev, dev);
			strcpy(cdroms[cdromn].vdev, vdev);
			//cdroms[cdromn].dirdbnode=dirdbFindAndRef(dmCDROM->basepath, vdev);
			cdroms[cdromn].caps=caps;
			cdroms[cdromn].fd=fd;
			cdroms[cdromn].request=0;
			cdroms[cdromn].request_complete=0;
			cdroms[cdromn].request_returnvalue=0;
			cdroms[cdromn].shutdown=0;
			cdroms[cdromn].lastdiscid=0;
			cdroms[cdromn].lasttocstr=0;
			cdroms[cdromn].musicbrainzhandle=0;
			cdroms[cdromn].musicbrainzdata=0;

			fcntl(fd, F_SETFD, 1);
			cdromn++;
#ifdef CDROM_VERBOSE
			fprintf(stderr, "%s is a cdrom\n", dev);
			if (caps&CDC_CLOSE_TRAY)
				fprintf(stderr, "CDC_CLOSE_TRAY\n");
			if (caps&CDC_OPEN_TRAY)
				fprintf(stderr, "CDC_OPEN_TRAY\n");
			if (caps&CDC_LOCK)
				fprintf(stderr, "CDC_LOCK\n");
			if (caps&CDC_SELECT_SPEED)
				fprintf(stderr, "CDC_SELECT_SPEED\n");
			if (caps&CDC_SELECT_DISC)
				fprintf(stderr, "CDC_SELECT_DISC\n");
			if (caps&CDC_MULTI_SESSION)
				fprintf(stderr, "CDC_MULTI_SESSION\n");
			if (caps&CDC_MCN)
				fprintf(stderr, "CDC_MCN\n");
			if (caps&CDC_MEDIA_CHANGED)
				fprintf(stderr, "CDC_MEDIA_CHANGED\n");
			if (caps&CDC_PLAY_AUDIO)
				fprintf(stderr, "CDC_PLAY_AUDIO\n");
			if (caps&CDC_RESET)
				fprintf(stderr, "CDC_RESET\n");
#ifdef CDC_IOCTLS
			if (caps&CDC_IOCTLS)
				fprintf(stderr, "CDC_IOCTLS\n");
#endif
			if (caps&CDC_DRIVE_STATUS)
				fprintf(stderr, "CDC_DRIVE_STATUS\n");
			if (caps&CDC_GENERIC_PACKET)
				fprintf(stderr, "CDC_GENERIC_PACKET\n");
			if (caps&CDC_CD_R)
				fprintf(stderr, "CDC_CD_R\n");
			if (caps&CDC_CD_RW)
				fprintf(stderr, "CDC_CD_RW\n");
			if (caps&CDC_DVD)
				fprintf(stderr, "CDC_DVD\n");
			if (caps&CDC_DVD_R)
				fprintf(stderr, "CDC_DVD_R\n");
			if (caps&CDC_DVD_RAM)
				fprintf(stderr, "CDC_DVD_RAM\n");
#ifdef CDC_MO_DRIVE
			if (caps&CDC_MO_DRIVE)
				fprintf(stderr, "CDC_MO_DRIVE\n");
#endif
#ifdef CDC_MRW
			if (caps&CDC_MRW)
				fprintf (stderr, "CDC_MRW\n");
#endif
#ifdef CDC_MRW_W
			if (caps&CDC_MRW_W)
				fprintf (stderr, "CDC_MRW_W\n");
#endif
#ifdef CDC_RAM
			if (caps&CDC_RAM)
				fprintf (stderr, "CDC_RAM\n");
#endif

			switch (ioctl(fd, CDROM_DISC_STATUS))
			{
				case CDS_NO_INFO: fprintf(stderr, "CDROM doesn't support CDROM_DISC_STATUS\n"); break;
				case CDS_NO_DISC: fprintf(stderr, "No disc\n"); break;
#ifdef CDS_TRAY_OPEN
				case CDS_TRAY_OPEN: fprintf (stderr, "Tray open\n"); break;
#endif
#ifdef CDS_DRIVE_NOT_READY
				case CDS_DRIVE_NOT_READY: fprintf (stderr, "Drive not ready\n"); break;
#endif
#ifdef CDS_DISC_OK
				case CDS_DISC_OK: fprintf (stderr, "Disc OK (\?\?\?)\n"); break;
#endif
				case CDS_AUDIO: fprintf(stderr, "Audio CD\n"); break;
				case CDS_DATA_1: fprintf(stderr, "Data CD, mode 1, form 1\n"); break;
				case CDS_DATA_2: fprintf(stderr, "Data CD, mode 1, form 2\n"); break;
				case CDS_XA_2_1: fprintf(stderr, "Data CD, mode 2, form 1\n"); break;
				case CDS_XA_2_2: fprintf(stderr, "Data CD, mode 2, form 2\n"); break;
				case CDS_MIXED: fprintf(stderr, "Mixed mode CD\n"); break;
				case -1: perror("ioctl()"); break;
				default:
					 fprintf(stderr, "Unknown cd type\n");
			}
			{
				struct cdrom_mcn mcn;
				if (!ioctl(fd, CDROM_GET_MCN, &mcn))
					fprintf(stderr, "MCN: %13s\n", mcn.medium_catalog_number);
			}
#endif
		} else {
			close(fd);
		}
	}
}

static void *cdrom_thread (void *_self)
{
	struct cdrom_t *self = _self;
	pthread_mutex_lock (&self->mutex);
	while (1)
	{
		if (self->shutdown)
		{
			pthread_mutex_unlock (&self->mutex);
			return 0;
		}
		if (self->request)
		{
			struct cdrom_read_audio rip_ioctl;

			pthread_mutex_unlock (&self->mutex);

			rip_ioctl.addr.lba = self->request->lba_addr;
			rip_ioctl.addr_format = CDROM_LBA;
			rip_ioctl.nframes = self->request->lba_count;
			rip_ioctl.buf = self->request->ptr;
			self->request->retval = ioctl (self->fd, CDROMREADAUDIO, &rip_ioctl);
			self->request->lba_count = self->request->retval ? 0 : rip_ioctl.nframes;

			pthread_mutex_lock (&self->mutex);
			self->request_complete = 1;
		}
		pthread_cond_wait (&self->cond, &self->mutex);
	}
}

static int cdint(void)
{
	char dev[32], vdev[12];
	char a;

	int i;

	ocpdir_t_fill (
		&cdrom_root,
		cdrom_root_ref,
		cdrom_root_unref,
		0,
		cdrom_root_readdir_start,
		0,
		cdrom_root_readdir_cancel,
		cdrom_root_readdir_iterate,
		cdrom_root_readdir_dir,
		cdrom_root_readdir_file,
		0,
		dirdbFindAndRef (DIRDB_NOPARENT, "cdrom:", dirdb_use_dir),
		0, /* we ignore refcounting */
		0, /* not an archive */
		0  /* not a playlist */);

	dmCDROM=RegisterDrive("cdrom:", &cdrom_root, &cdrom_root);

	fprintf(stderr, "Locating cdroms [     ]\010\010\010\010\010\010");

	sprintf(dev, "/dev/cdrom");
	sprintf(vdev, "cdrom");
	try(dev, vdev);
	for (a=0;a<=32;a++)
	{
		sprintf(dev, "/dev/cdrom%d", a);
		sprintf(vdev, "cdrom%d", a);
		try(dev, vdev);
	}
	fprintf(stderr, ".");

	for (a=0;a<=32;a++)
	{
		sprintf(dev, "/dev/cdroms/cdrom%d", a);
		sprintf(vdev, "cdrom%d", a);
		try(dev, vdev);
	}
	fprintf(stderr, ".");

	for (a=0;a<=32;a++)
	{
		sprintf(dev, "/dev/scd%d", a);
		sprintf(vdev, "scd%d", a);
		try(dev, vdev);
	}
	fprintf(stderr, ".");

	for (a='a';a<='z';a++)
	{
		sprintf(dev, "/dev/hd%c", a);
		sprintf(vdev, "hd%c", a);
		try(dev, vdev);
	}
	fprintf(stderr, ".");

	for (a='0';a<='9';a++)
	{
		sprintf(dev, "/dev/sr%c", a);
		sprintf(vdev, "sr%c", a);
		try(dev, vdev);
	}

	fprintf(stderr, ".]\n");

	/* We wait with thread initialization until here, since realloc might move all the cdroms structures */
	for (i=0; i < cdromn; i++)
	{
		pthread_mutex_init (&cdroms[i].mutex, NULL);
		pthread_cond_init (&cdroms[i].cond, NULL);
		pthread_create (&cdroms[i].thread, NULL, cdrom_thread, &cdroms[i]);
	}

	return errOk;
}

static void cdclose(void)
{
	int i;

	for (i=0; i < cdromn; i++)
	{
		void *retval;

		pthread_mutex_lock (&cdroms[i].mutex);
		cdroms[i].shutdown = 1;
		pthread_mutex_unlock (&cdroms[i].mutex);
		pthread_cond_signal (&cdroms[i].cond);
		pthread_join (cdroms[i].thread, &retval);

		close (cdroms[i].fd);
		cdroms[i].fd = -1;
		free (cdroms[i].lastdiscid);
		free (cdroms[i].lasttocstr);
		//dirdbUnref(cdroms[i].dirdbnode);
	}
	free (cdroms);
	cdroms = 0;
	cdromn = 0;
}

static void cdrom_root_ref (struct ocpdir_t *self)
{
	self->refcount++;
}

static void cdrom_root_unref (struct ocpdir_t *self)
{
	self->refcount--;
	if (!self->refcount)
	{
		if (self->dirdb_ref != DIRDB_NOPARENT)
		{
			dirdbUnref (self->dirdb_ref, dirdb_use_dir);
			self->dirdb_ref = DIRDB_NOPARENT;
		}
	}
	return;
}

struct cdrom_root_dirhandle_t
{
	void (*callback_dir )(void *token, struct ocpdir_t *);
	void *token;
	struct ocpdir_t *owner;
	int  n;
};

static ocpdirhandle_pt cdrom_root_readdir_start (struct ocpdir_t *self, void(*callback_file)(void *token, struct ocpfile_t *),
                                                                        void(*callback_dir )(void *token, struct ocpdir_t *), void *token)
{
	struct cdrom_root_dirhandle_t *dh = calloc (1, sizeof (*dh));
	dh->callback_dir = callback_dir;
	dh->token = token;
	dh->owner = self;

	return dh;
}

static void cdrom_root_readdir_cancel (ocpdirhandle_pt dh)
{
	free (dh);
}

static int cdrom_root_readdir_iterate (ocpdirhandle_pt _dh)
{
	struct cdrom_root_dirhandle_t *dh = _dh;
	struct cdrom_drive_ocpdir_t *dir;

	if (dh->n >= cdromn)
	{
		return 0;
	}

	dir = calloc (1, sizeof (*dir));
	if (!dir)
	{
		return 0;
	}

	ocpdir_t_fill (
		&dir->head,
		cdrom_drive_ref,
		cdrom_drive_unref,
		dh->owner,
		cdrom_drive_readdir_start,
		0,
		cdrom_drive_readdir_cancel,
		cdrom_drive_readdir_iterate,
		0, // readdir_dir
		0, // readdir_file
		0,
		dirdbFindAndRef (dh->owner->dirdb_ref, cdroms[dh->n].vdev, dirdb_use_dir),
		1,
		0, /* not an archive */
		0  /* not a playlist */);

	dir->cdrom = cdroms + dh->n;
	dh->owner->ref (dh->owner);

	dh->callback_dir (dh->token, &dir->head);

	dir->head.unref (&dir->head);
	dh->n++;

	return 1;
}

static struct ocpdir_t *cdrom_root_readdir_dir (struct ocpdir_t *_self, uint32_t dirdb_ref)
{
	const char *searchpath = 0;
	uint32_t parent_dirdb_ref;
	int n;

/* assertion begin */
	parent_dirdb_ref = dirdbGetParentAndRef (dirdb_ref, dirdb_use_dir);
	dirdbUnref (parent_dirdb_ref, dirdb_use_dir);
	if (parent_dirdb_ref != _self->dirdb_ref)
	{
		fprintf (stderr, "cdrom_root_readdir_dir: dirdb_ref->parent is not the expected value\n");
		return 0;
	}
/* assertion end */

	dirdbGetName_internalstr (dirdb_ref, &searchpath);
	if (!searchpath)
	{
		return 0;
	}

	for (n=0; n < cdromn; n++)
	{
		struct cdrom_drive_ocpdir_t *dir;

		if (strcmp (cdroms[n].vdev, searchpath))
		{
			continue;
		}

		dir = calloc (1, sizeof (*dir));
		if (!dir)
		{
			return 0;
		}

		ocpdir_t_fill (
			&dir->head,
			cdrom_drive_ref,
			cdrom_drive_unref,
			_self,
			cdrom_drive_readdir_start,
			0,
			cdrom_drive_readdir_cancel,
			cdrom_drive_readdir_iterate,
			0, // readdir_dir
			0, // readdir_file
			0,
			dirdbRef (dirdb_ref, dirdb_use_dir),
			1,
			0, /* not an archive */
			0  /* not a playlist */);

		return &dir->head;
	}
	return 0;
}

static struct ocpfile_t *cdrom_root_readdir_file (struct ocpdir_t *_self, uint32_t dirdb_ref)
{
	/* this can not succeed */
	return 0;
}

static void cdrom_drive_ref (struct ocpdir_t *_self)
{
	struct cdrom_drive_ocpdir_t *self = (struct cdrom_drive_ocpdir_t *)_self;
	self->head.refcount++;
}

static void cdrom_drive_unref (struct ocpdir_t *_self)
{
	struct cdrom_drive_ocpdir_t *self = (struct cdrom_drive_ocpdir_t *)_self;
	self->head.refcount--;
	if (!self->head.refcount)
	{
		if (self->head.parent)
		{
			self->head.parent->unref (self->head.parent);
			self->head.parent = 0;
		}
		dirdbUnref(self->head.dirdb_ref, dirdb_use_dir);
		free (self);
	}
}

struct cdrom_drive_dirhandle_t
{
	void (*callback_file)(void *token, struct ocpfile_t *);
	void *token;
	struct cdrom_drive_ocpdir_t *owner;

	struct cdrom_tochdr tochdr;
	int i;
	int initlba;
	int lastlba;
};

static ocpdirhandle_pt cdrom_drive_readdir_start (struct ocpdir_t *_self, void(*callback_file)(void *token, struct ocpfile_t *),
                                                                          void(*callback_dir )(void *token, struct ocpdir_t *), void *token)
{
	struct cdrom_drive_ocpdir_t *self = (struct cdrom_drive_ocpdir_t *)_self;
	struct cdrom_drive_dirhandle_t *dh = calloc (1, sizeof (*dh));

	int ti;

	dh->callback_file = callback_file;
	dh->token = token;
	dh->owner = self;
	dh->initlba = -1;

	bzero (&self->cdrom->lasttoc, sizeof (self->cdrom->lasttoc));
	free (dh->owner->cdrom->lastdiscid);
	free (dh->owner->cdrom->lasttocstr);
	dh->owner->cdrom->lastdiscid = 0;
	dh->owner->cdrom->lasttocstr = 0;

	if (ioctl(self->cdrom->fd, CDROMREADTOCHDR, &dh->tochdr))
	{ /* should not happen, but sometime it does for unknown reasons? */
		struct cdrom_tocentry cdte;
		dh->i = 1;
		self->cdrom->lasttoc.starttrack = 1;
		self->cdrom->lasttoc.lasttrack  = 99;

		for (ti = self->cdrom->lasttoc.starttrack; ti <= 99; ti++)
		{
			cdte.cdte_track = ti;
			cdte.cdte_format = CDROM_LBA; /* CDROM_MSF */
			if (!ioctl (dh->owner->cdrom->fd, CDROMREADTOCENTRY, &cdte))
			{
				if (cdte.cdte_format==CDROM_MSF)
				{
					dh->owner->cdrom->lasttoc.track[ti].lba_addr = 150 - cdte.cdte_addr.msf.minute * 75 * 60 +
					                                                     cdte.cdte_addr.msf.second * 75 +
					                                                     cdte.cdte_addr.msf.frame;
				} else {
					dh->owner->cdrom->lasttoc.track[ti].lba_addr = cdte.cdte_addr.lba;
				}
				dh->owner->cdrom->lasttoc.track[ti].is_data = cdte.cdte_datamode;
				if ((dh->initlba < 0) && (!cdte.cdte_datamode))
				{
					dh->initlba = dh->owner->cdrom->lasttoc.track[ti].lba_addr;
				}
			} else {
				self->cdrom->lasttoc.lasttrack = ti - 1;
				goto leadout;
			}
		}
leadout:
		cdte.cdte_track = CDROM_LEADOUT;
		cdte.cdte_format = CDROM_LBA; /* CDROM_MSF */
		if (!ioctl (dh->owner->cdrom->fd, CDROMREADTOCENTRY, &cdte))
		{
			if (cdte.cdte_format==CDROM_MSF)
			{
				dh->owner->cdrom->lasttoc.track[ti].lba_addr = 150 - cdte.cdte_addr.msf.minute * 75 * 60 +
				                                                     cdte.cdte_addr.msf.second * 75 +
				                                                     cdte.cdte_addr.msf.frame;
			} else {
				dh->owner->cdrom->lasttoc.track[ti].lba_addr = cdte.cdte_addr.lba;
			}
			dh->owner->cdrom->lasttoc.track[ti].is_data = cdte.cdte_datamode;
		}
	} else {
		dh->i = dh->tochdr.cdth_trk0;
		self->cdrom->lasttoc.starttrack = dh->tochdr.cdth_trk0;
		self->cdrom->lasttoc.lasttrack  = (dh->tochdr.cdth_trk1 < 100) ? dh->tochdr.cdth_trk1 : 99;

		for (ti = self->cdrom->lasttoc.starttrack; ti <= (self->cdrom->lasttoc.lasttrack + 1); ti++)
		{
			struct cdrom_tocentry cdte;
			cdte.cdte_track= (ti != (self->cdrom->lasttoc.lasttrack + 1)) ? ti : CDROM_LEADOUT;
			cdte.cdte_format=CDROM_LBA; /* CDROM_MSF */
			if (!ioctl (dh->owner->cdrom->fd, CDROMREADTOCENTRY, &cdte))
			{
				if (cdte.cdte_format==CDROM_MSF)
				{
					dh->owner->cdrom->lasttoc.track[ti].lba_addr = 150 - cdte.cdte_addr.msf.minute * 75 * 60 +
					                                                     cdte.cdte_addr.msf.second * 75 +
					                                                     cdte.cdte_addr.msf.frame;
				} else {
					dh->owner->cdrom->lasttoc.track[ti].lba_addr = cdte.cdte_addr.lba;
				}
				dh->owner->cdrom->lasttoc.track[ti].is_data = cdte.cdte_datamode;
				if ((dh->initlba < 0) && (!cdte.cdte_datamode))
				{
					dh->initlba = dh->owner->cdrom->lasttoc.track[ti].lba_addr;
				}
			}
		}
	}

	{
		DiscId *did = discid_new();
		int offsets[100];
		int first = self->cdrom->lasttoc.starttrack;
		int last = self->cdrom->lasttoc.lasttrack;
		int i;
		char *discid;
		char *toc;

		if (!did)
		{
			goto failout;
		}

		bzero (offsets, sizeof (offsets));
		for (i=first; i <= last; i++)
		{
			offsets[i] = dh->owner->cdrom->lasttoc.track[i].lba_addr + 150;
			offsets[0] = dh->owner->cdrom->lasttoc.track[i+1].lba_addr + 150;
			if (self->cdrom->lasttoc.track[i].is_data)
			{
				first = i + 1;
			}
		}

		if (first > last)
		{
			goto failout;
		}

		if (!discid_put (did, first, last, offsets))
		{
			goto failout;
		}

		discid = discid_get_id (did);
		toc = discid_get_toc_string (did);
		if (discid && toc)
		{
			dh->owner->cdrom->lastdiscid = strdup(discid);
			dh->owner->cdrom->lasttocstr = strdup(toc);
			if (dh->owner->cdrom->musicbrainzhandle)
			{
				musicbrainz_lookup_discid_cancel (dh->owner->cdrom->musicbrainzhandle);
				dh->owner->cdrom->musicbrainzhandle = 0;
			}
			if (dh->owner->cdrom->musicbrainzdata)
			{
				musicbrainz_database_h_free (dh->owner->cdrom->musicbrainzdata);
				dh->owner->cdrom->musicbrainzdata = 0;
			}
			dh->owner->cdrom->musicbrainzhandle = musicbrainz_lookup_discid_init (dh->owner->cdrom->lastdiscid, dh->owner->cdrom->lasttocstr, &dh->owner->cdrom->musicbrainzdata);
		}
failout:
		if (did)
		{
			discid_free (did);
		}
	}

	return dh;
}

static void cdrom_drive_readdir_cancel (ocpdirhandle_pt dh)
{
	free (dh);
}

static const char *cdrom_track_filename_override_disc (struct ocpfile_t *file)
{
	return "DISC.CDA";
}

static const char *cdrom_track_filename_override_track (struct ocpfile_t *_file)
{
	struct cdrom_track_ocpfile_t *file = (struct cdrom_track_ocpfile_t *)_file;
	return file->trackfilename;
}

static int cdrom_drive_readdir_iterate (ocpdirhandle_pt _dh)
{
	struct cdrom_drive_dirhandle_t *dh = _dh;
	struct cdrom_track_ocpfile_t *file;

	uint32_t mdb_ref;
	struct moduleinfostruct mi;
	char filename[64];

	if (dh->owner->cdrom->musicbrainzhandle)
	{
		if (musicbrainz_lookup_discid_iterate (dh->owner->cdrom->musicbrainzhandle, &dh->owner->cdrom->musicbrainzdata))
		{
			usleep (1000); /* anything is better than nothing... */
			return 1;      /* this will throttle this CPU core */
		}
		dh->owner->cdrom->musicbrainzhandle = 0;
	}

	if ((dh->i > dh->tochdr.cdth_trk1) || (dh->i >= 100)) /* last check is not actually needed */
	{
		if (dh->initlba >= 0) /* did we initialize? Create the DISC.CDA that covers the entire disc */
		{
			file = calloc (1, sizeof (*file));
			if (!file)
			{
				return 0;
			}

			snprintf(filename, sizeof (filename), "%sDISC.CDA", dh->owner->cdrom->lastdiscid ? dh->owner->cdrom->lastdiscid : "");

			ocpfile_t_fill (&file->head,
			                 cdrom_track_ref,
			                 cdrom_track_unref,
			                &dh->owner->head,
			                 cdrom_track_open,
			                 cdrom_track_filesize,
			                 cdrom_track_filesize_ready,
			                 cdrom_track_filename_override_disc,
			                 dirdbFindAndRef (dh->owner->head.dirdb_ref, filename, dirdb_use_file),
			                 1, /* refcount */
			                 1  /* is_nodetect */);

			dh->owner->head.ref (&dh->owner->head);
			file->cdrom = dh->owner->cdrom;

			mdb_ref = mdbGetModuleReference2 (file->head.dirdb_ref, (dh->owner->cdrom->lasttoc.track[dh->owner->cdrom->lasttoc.lasttrack + 1].lba_addr - dh->owner->cdrom->lasttoc.track[dh->owner->cdrom->lasttoc.starttrack].lba_addr) * 2352);
			if (mdb_ref != UINT32_MAX)
			{
				if (mdbGetModuleInfo(&mi, mdb_ref))
				{
					mi.modtype.integer.i=MODULETYPE("CDA");
					mi.channels=2;
					mi.playtime=(dh->lastlba - dh->initlba) / CD_FRAMES;
					if (dh->owner->cdrom->musicbrainzdata)
					{
						strcpy(mi.comment, "Looked up via Musicbrainz");
					} else {
						strcpy(mi.comment, dh->owner->cdrom->vdev);
					}
					if (dh->owner->cdrom->musicbrainzdata && dh->owner->cdrom->musicbrainzdata->album[0])
					{
						snprintf (mi.title, sizeof (mi.title), "%s", dh->owner->cdrom->musicbrainzdata->album);
						snprintf (mi.album, sizeof (mi.album), "%s", dh->owner->cdrom->musicbrainzdata->album);
					} else if (!mi.title[0])
					{
						strcpy(mi.title, "CDROM audio disc");
					}
					if (dh->owner->cdrom->musicbrainzdata && dh->owner->cdrom->musicbrainzdata->artist[0][0])
					{
						snprintf (mi.artist, sizeof (mi.artist), "%s", dh->owner->cdrom->musicbrainzdata->artist[0]);
					}
					if (dh->owner->cdrom->musicbrainzdata && dh->owner->cdrom->musicbrainzdata->date[0])
					{
						mi.date = dh->owner->cdrom->musicbrainzdata->date[0];
					}
					mdbWriteModuleInfo (mdb_ref, &mi);
				}
			}
			file->trackno = 0;
			dh->callback_file (dh->token, &file->head);
			file->head.unref (&file->head);
		}
		return 0;
	}

	if (!dh->owner->cdrom->lasttoc.track[dh->i].is_data)
	{
		snprintf(filename, sizeof (filename), "%sTRACK%02u.CDA", dh->owner->cdrom->lastdiscid ? dh->owner->cdrom->lastdiscid : "", dh->i);
		int len = (dh->owner->cdrom->lasttoc.track[dh->i + 1].lba_addr - dh->owner->cdrom->lasttoc.track[dh->i].lba_addr);

		if (dh->initlba < 0)
		{
			dh->initlba = dh->owner->cdrom->lasttoc.track[dh->i + 1].lba_addr;
		}
		dh->lastlba = dh->owner->cdrom->lasttoc.track[dh->i].lba_addr;

		file = calloc (1, sizeof (*file));
		if (!file)
		{
			goto next;
		}

		snprintf(file->trackfilename, sizeof (file->trackfilename), "TRACK%02u.CDA", dh->i);

		ocpfile_t_fill (&file->head,
		                 cdrom_track_ref,
		                 cdrom_track_unref,
		                &dh->owner->head,
		                 cdrom_track_open,
		                 cdrom_track_filesize,
		                 cdrom_track_filesize_ready,
		                 cdrom_track_filename_override_track,
		                 dirdbFindAndRef (dh->owner->head.dirdb_ref, filename, dirdb_use_file),
		                 1, /* refcount */
		                 1  /* is_nodetect */);

		dh->owner->head.ref (&dh->owner->head);
		file->cdrom = dh->owner->cdrom;
		mdb_ref = mdbGetModuleReference2 (file->head.dirdb_ref, len * 2352);
		if (mdb_ref != UINT32_MAX)
		{
			if (mdbGetModuleInfo(&mi, mdb_ref))
			{
				mi.modtype.integer.i=MODULETYPE("CDA");
				mi.channels=2;
				mi.playtime=len / CD_FRAMES;
				if (dh->owner->cdrom->musicbrainzdata)
				{
					strcpy(mi.comment, "Looked up via Musicbrainz");
				} else {
					strcpy(mi.comment, dh->owner->cdrom->vdev);
				}
				if (dh->owner->cdrom->musicbrainzdata && dh->owner->cdrom->musicbrainzdata->album[0])
				{
					snprintf (mi.album, sizeof (mi.album), "%s", dh->owner->cdrom->musicbrainzdata->album);
				}
				if (dh->owner->cdrom->musicbrainzdata && dh->owner->cdrom->musicbrainzdata->title[dh->i][0])
				{
					snprintf (mi.title, sizeof (mi.title), "%s", dh->owner->cdrom->musicbrainzdata->title[dh->i]);
				} else if (!mi.title[0])
				{
					strcpy(mi.title, "CDROM audio track");
				}
				if (dh->owner->cdrom->musicbrainzdata && dh->owner->cdrom->musicbrainzdata->artist[dh->i][0])
				{
					snprintf (mi.artist, sizeof (mi.artist), "%s", dh->owner->cdrom->musicbrainzdata->artist[dh->i]);
				}
				if (dh->owner->cdrom->musicbrainzdata && dh->owner->cdrom->musicbrainzdata->date[dh->i])
				{
					mi.date = dh->owner->cdrom->musicbrainzdata->date[dh->i];
				}

				mdbWriteModuleInfo (mdb_ref, &mi);
			}
		}
		file->trackno = dh->i;
		dh->callback_file (dh->token, &file->head);
		file->head.unref (&file->head);
	}

next:
	dh->i++;

	return 1;
}

static void cdrom_track_ref (struct ocpfile_t *_self)
{
	struct cdrom_track_ocpfile_t *self = (struct cdrom_track_ocpfile_t *)_self;
	self->head.refcount++;
}

static void cdrom_track_unref (struct ocpfile_t *_self)
{
	struct cdrom_track_ocpfile_t *self = (struct cdrom_track_ocpfile_t *)_self;
	self->head.refcount--;
	if (!self->head.refcount)
	{
		dirdbUnref (self->head.dirdb_ref, dirdb_use_file);

		self->head.parent->unref (self->head.parent);
		self->head.parent = 0;

		free (self);
	}
}

struct ocpfilehandle_cdrom_track_t
{
	struct ocpfilehandle_t head;
	struct cdrom_track_ocpfile_t *owner;
	int refcount;
};

static void ocpfilehandle_cdrom_track_ref (struct ocpfilehandle_t *_handle)
{
	struct ocpfilehandle_cdrom_track_t *handle = (struct ocpfilehandle_cdrom_track_t *)_handle;
	handle->refcount++;
}

static void ocpfilehandle_cdrom_track_unref (struct ocpfilehandle_t *_handle)
{
	struct ocpfilehandle_cdrom_track_t *handle = (struct ocpfilehandle_cdrom_track_t *)_handle;
	handle->refcount--;
	if (!handle->refcount)
	{
		handle->owner->head.unref (&handle->owner->head);
		dirdbUnref (handle->head.dirdb_ref, dirdb_use_filehandle);
		free (handle);
	}
}

static int ocpfilehandle_cdrom_track_seek (struct ocpfilehandle_t *_handle, int64_t pos)
{
	return pos ? -1 : 0;
}

static uint64_t ocpfilehandle_cdrom_track_getpos (struct ocpfilehandle_t *_handle)
{
	return 0;
}

static int ocpfilehandle_cdrom_track_eof (struct ocpfilehandle_t *_handle)
{
	return 1;
}

static int ocpfilehandle_cdrom_track_error (struct ocpfilehandle_t *_handle)
{
	return 0;
}

static int ocpfilehandle_cdrom_track_read (struct ocpfilehandle_t *_handle, void *dst, int len)
{
	return len ? -1 : 0;
}

static int ocpfilehandle_cdrom_track_ioctl (struct ocpfilehandle_t *_handle, const char *cmd, void *ptr)
{
	struct ocpfilehandle_cdrom_track_t *handle = (struct ocpfilehandle_cdrom_track_t *)_handle;
	struct cdrom_t *self = handle->owner->cdrom;

	if (!strcmp (cmd, IOCTL_CDROM_READTOC))
	{
		memcpy (ptr, &handle->owner->cdrom->lasttoc, sizeof (handle->owner->cdrom->lasttoc));
		return 0;
	}
	if (!strcmp (cmd, IOCTL_CDROM_READAUDIO_ASYNC_REQUEST))
	{
		pthread_mutex_lock (&self->mutex);

		if (self->request)
		{
			pthread_mutex_unlock (&self->mutex);
			return -1;
		}

		self->request = ptr;
		self->request_returnvalue = 0;
		self->request_complete = 0;

		pthread_mutex_unlock (&self->mutex);
		pthread_cond_signal (&self->cond);
		return 1; /* not ready */
	}
	if (!strcmp (cmd, IOCTL_CDROM_READAUDIO_ASYNC_PULL))
	{
		pthread_mutex_lock (&self->mutex);
		if (!self->request)
		{
			pthread_mutex_unlock (&self->mutex);
			return -1;
		}
		if (self->request != ptr)
		{
			pthread_mutex_unlock (&self->mutex);
			return -1;
		}
		if (!self->request_complete)
		{
			pthread_mutex_unlock (&self->mutex);
			return 1; /* not ready */
		}
		self->request = 0;
		self->request_complete = 0;
		pthread_mutex_unlock (&self->mutex);
		return 0;
	}
	return -1;
}

static uint64_t ocpfilehandle_cdrom_track_filesize (struct ocpfilehandle_t *_handle)
{
	return 0;
}

static int ocpfilehandle_cdrom_track_filesize_ready (struct ocpfilehandle_t *_handle)
{
	return 1;
}

static const char *ocpfilehandle_cdrom_track_filename_override_disc (struct ocpfilehandle_t *_handle)
{
	struct ocpfilehandle_cdrom_track_t *handle = (struct ocpfilehandle_cdrom_track_t *)_handle;

	return handle->owner->head.filename_override (&handle->owner->head);
}

static struct ocpfilehandle_t *cdrom_track_open (struct ocpfile_t *_self)
{
	struct cdrom_track_ocpfile_t *self = (struct cdrom_track_ocpfile_t *)_self;
	struct ocpfilehandle_cdrom_track_t *retval;

	retval = calloc (sizeof (struct ocpfilehandle_cdrom_track_t), 1);
	if (!retval)
	{
		return 0;
	}

	ocpfilehandle_t_fill (&retval->head,
	                      ocpfilehandle_cdrom_track_ref,
	                      ocpfilehandle_cdrom_track_unref,
	                      &self->head,
	                      ocpfilehandle_cdrom_track_seek,
	                      ocpfilehandle_cdrom_track_seek,
	                      ocpfilehandle_cdrom_track_seek,
	                      ocpfilehandle_cdrom_track_getpos,
	                      ocpfilehandle_cdrom_track_eof,
	                      ocpfilehandle_cdrom_track_error,
	                      ocpfilehandle_cdrom_track_read,
	                      ocpfilehandle_cdrom_track_ioctl,
	                      ocpfilehandle_cdrom_track_filesize,
	                      ocpfilehandle_cdrom_track_filesize_ready,
	                      ocpfilehandle_cdrom_track_filename_override_disc,
	                      _self->dirdb_ref);
	dirdbRef (_self->dirdb_ref, dirdb_use_filehandle);

	retval->owner = self;
	retval->owner->head.ref (&retval->owner->head);

	retval->refcount = 1;

	return &retval->head;
}

static uint64_t cdrom_track_filesize (struct ocpfile_t *_self)
{
	struct cdrom_track_ocpfile_t *self = (struct cdrom_track_ocpfile_t *)_self;
	if (self->trackno)
	{
		return (self->cdrom->lasttoc.track[self->trackno                  + 1].lba_addr - self->cdrom->lasttoc.track[self->trackno                  ].lba_addr) * 2352;
	} else {
		return (self->cdrom->lasttoc.track[self->cdrom->lasttoc.lasttrack + 1].lba_addr - self->cdrom->lasttoc.track[self->cdrom->lasttoc.starttrack].lba_addr) * 2352;
	}
}

static int cdrom_track_filesize_ready (struct ocpfile_t *_self)
{
	return 1;
}

DLLEXTINFO_CORE_PREFIX struct linkinfostruct dllextinfo = {.name = "cdrom", .desc = "OpenCP UNIX audio-cdrom filebrowser (c) 2004-'23 Stian Skjelstad", .ver = DLLVERSION, .Init = cdint, .Close = cdclose, .sortindex = 30};
