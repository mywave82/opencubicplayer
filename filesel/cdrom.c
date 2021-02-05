/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) '04-'21 Stian Sebastian Skjelstad <stian.skjelstad@gmail.com>
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
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "types.h"
#include "boot/plinkman.h"
#include "dirdb.h"
#include "filesystem.h"
#include "filesystem-drive.h"
#include "filesystem-file-mem.h"
#include "mdb.h"
#include "pfilesel.h"
#include "stuff/err.h"

struct cdrom_t;
static struct cdrom_t
{
	char dev[32];
	char vdev[12];
	int caps;
	int fd;
	//uint32_t dirdbnode;
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
	char buffer[128];
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
static struct ocpdir_t *cdrom_drive_readdir_dir (struct ocpdir_t *_self, uint32_t dirdb_ref);
static struct ocpfile_t *cdrom_drive_readdir_file (struct ocpdir_t *_self, uint32_t dirdb_ref);

static void cdrom_track_ref (struct ocpfile_t *);
static void cdrom_track_unref (struct ocpfile_t *);
static struct ocpfilehandle_t *cdrom_track_open (struct ocpfile_t *);
static uint64_t cdrom_track_filesize (struct ocpfile_t *);
static int cdrom_track_filesize_ready (struct ocpfile_t *);

static void try(const char *dev, const char *vdev)
{
	struct cdrom_t *temp;
	int fd;
#ifdef CDROM_VERBOSE
	fprintf(stderr, "Testing %s\n", dev);
#endif
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
			{
				struct cdrom_tochdr tochdr;
				if (!ioctl(fd, CDROMREADTOCHDR, &tochdr))
				{
#if 0
					int i;
					struct cdrom_tocentry tocentry;
#endif
					fprintf(stderr, "Start track: %d\nStop track: %d\n", tochdr.cdth_trk0, tochdr.cdth_trk1);
#if 0
					for (i=tochdr.cdth_trk0;i<=(tochdr.cdth_trk1+1);i++)
					{
						if (i>tochdr.cdth_trk1)
							i=CDROM_LEADOUT;
						tocentry.cdte_track=i;
						tocentry.cdte_format=CDROM_MSF; /* CDROM_LBA */
						if (!ioctl(fd, CDROMREADTOCENTRY, &tocentry))
						{
							fprintf(stderr, "cdte_track:    %d%s\n", tocentry.cdte_track, (i==CDROM_LEADOUT)?" LEADOUT":"");
							fprintf(stderr, "cdte_adr:      %d\n", tocentry.cdte_adr);
							fprintf(stderr, "cdte_ctrl:     %d %s\n", tocentry.cdte_ctrl, (tocentry.cdte_ctrl&CDROM_DATA_TRACK)?"(DATA)":"AUDIO");
							fprintf(stderr, "cdte_format:   %d\n", tocentry.cdte_format);
							if (tocentry.cdte_format==CDROM_MSF){
								fprintf(stderr, "cdte_addr.msf.minute: %d\n", tocentry.cdte_addr.msf.minute);
								fprintf(stderr, "cdte_addr.msf.second: %d\n", tocentry.cdte_addr.msf.second);
								fprintf(stderr, "cdte_addr.msf.frame:  %d\n", tocentry.cdte_addr.msf.frame);
							} else {
								fprintf(stderr, "cdte_addr.lba:        %d\n", tocentry.cdte_addr.lba);
							}
							fprintf(stderr, "cdte_datamode: %d\n", tocentry.cdte_datamode);
							fprintf(stderr, "\n");
						}
					}
#endif
				}
			}
#endif
		} else {
			close(fd);
		}
	}
}

static int cdint(void)
{
	char dev[32], vdev[12];
	char a;

	fsRegisterExt("CDA");

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
	fprintf(stderr, ".]\n");
	return errOk;
}

static void cdclose(void)
{
	int i;

	for (i=0; i < cdromn; i++)
	{
		close (cdroms[i].fd);
		cdroms[i].fd = -1;
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
		cdrom_drive_readdir_dir,
		cdrom_drive_readdir_file,
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
	char *searchpath = 0;
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
			cdrom_drive_readdir_dir,
			cdrom_drive_readdir_file,
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
	dh->callback_file = callback_file;
	dh->token = token;
	dh->owner = self;
	dh->initlba = -1;

	if (ioctl(self->cdrom->fd, CDROMREADTOCHDR, &dh->tochdr))
	{
		dh->i = -1;
	} else {
		dh->i = dh->tochdr.cdth_trk0;
	}

	return dh;
}

static void cdrom_drive_readdir_cancel (ocpdirhandle_pt dh)
{
	free (dh);
}

static int cdrom_drive_readdir_iterate (ocpdirhandle_pt _dh)
{
	struct cdrom_drive_dirhandle_t *dh = _dh;
	struct cdrom_track_ocpfile_t *file;

	struct cdrom_tocentry tocentry;
	struct cdrom_tocentry tocentryN;

	uint32_t mdb_ref;
	struct moduleinfostruct mi;

	if ((dh->i > dh->tochdr.cdth_trk1) || (dh->i >= 100)) /* last check is not actually needed */
	{
		if (dh->initlba >= 0)
		{
			file = calloc (1, sizeof (*file));
			if (!file)
			{
				return 0;
			}

			ocpfile_t_fill (&file->head,
			                 cdrom_track_ref,
			                 cdrom_track_unref,
			                &dh->owner->head,
			                 cdrom_track_open,
			                 cdrom_track_filesize,
			                 cdrom_track_filesize_ready,
			                 dirdbFindAndRef (dh->owner->head.dirdb_ref, "DISK.CDA", dirdb_use_file),
			                 1, /* refcount */
			                 1  /* is_nodetect */);

			dh->owner->head.ref (&dh->owner->head);
			file->cdrom = dh->owner->cdrom;
			snprintf (file->buffer, sizeof (file->buffer), "fd=%d,disk=%d:%d", dh->owner->cdrom->fd, dh->initlba, dh->lastlba);

			mdb_ref = mdbGetModuleReference2 (file->head.dirdb_ref, strlen (file->buffer));
			if (mdb_ref != UINT32_MAX)
			{
				if (mdbGetModuleInfo(&mi, mdb_ref))
				{
					mi.modtype=mtCDA;
					mi.channels=2;
					mi.playtime=(dh->lastlba - dh->initlba) / CD_FRAMES;
					strcpy(mi.comment, dh->owner->cdrom->vdev);
					strcpy(mi.modname, "CDROM audio disc");
					mdbWriteModuleInfo (mdb_ref, &mi);
				}
			}
			dh->callback_file (dh->token, &file->head);
			file->head.unref (&file->head);
		}
		return 0;
	}

	tocentry.cdte_track=dh->i;
	tocentry.cdte_format=CDROM_LBA; /* CDROM_MSF */

	if (!ioctl (dh->owner->cdrom->fd, CDROMREADTOCENTRY, &tocentry))
	{
		tocentryN.cdte_track = ( dh->i == dh->tochdr.cdth_trk1 ) ? CDROM_LEADOUT : dh->i+1;
		tocentryN.cdte_format = CDROM_LBA;
		ioctl (dh->owner->cdrom->fd, CDROMREADTOCENTRY, &tocentryN);
/*
		fprintf(stderr, "[cdte_track:   %d%s]\n", tocentry.cdte_track, (i==CDROM_LEADOUT)?" LEADOUT":"");
		fprintf(stderr, "cdte_adr:      %d\n", tocentry.cdte_adr);
		fprintf(stderr, "cdte_ctrl:     %d %s\n", tocentry.cdte_ctrl, (tocentry.cdte_ctrl&CDROM_DATA_TRACK)?"(DATA)":"AUDIO");
		fprintf(stderr, "cdte_format:   %d\n", tocentry.cdte_format);
		if (tocentry.cdte_format==CDROM_MSF)
		{
			fprintf(stderr, "cdte_addr.msf.minute: %d\n", tocentry.cdte_addr.msf.minute);
			fprintf(stderr, "cdte_addr.msf.second: %d\n", tocentry.cdte_addr.msf.second);
			fprintf(stderr, "cdte_addr.msf.frame:  %d\n", tocentry.cdte_addr.msf.frame);
		} else {
			fprintf(stderr, "cdte_addr.lba:        %d\n", tocentry.cdte_addr.lba);
		}
		fprintf(stderr, "cdte_datamode: %d\n", tocentry.cdte_datamode);
*/
		if (!(tocentry.cdte_ctrl&CDROM_DATA_TRACK))
		{
			char filename[12];
			snprintf(filename, sizeof (filename), "TRACK%02u.CDA", dh->i);

			if (dh->initlba < 0)
			{
				dh->initlba = tocentry.cdte_addr.lba;
			}
			dh->lastlba = tocentryN.cdte_addr.lba;

			file = calloc (1, sizeof (*file));
			if (!file)
			{
				goto next;
			}

			ocpfile_t_fill (&file->head,
			                 cdrom_track_ref,
			                 cdrom_track_unref,
			                &dh->owner->head,
			                 cdrom_track_open,
			                 cdrom_track_filesize,
			                 cdrom_track_filesize_ready,
			                 dirdbFindAndRef (dh->owner->head.dirdb_ref, filename, dirdb_use_file),
			                 1, /* refcount */
			                 1  /* is_nodetect */);

			dh->owner->head.ref (&dh->owner->head);
			file->cdrom = dh->owner->cdrom;
			snprintf (file->buffer, sizeof (file->buffer), "fd=%d,track=%d", dh->owner->cdrom->fd, dh->i);
			mdb_ref = mdbGetModuleReference2 (file->head.dirdb_ref, strlen (file->buffer));
			if (mdb_ref != UINT32_MAX)
			{
				if (mdbGetModuleInfo(&mi, mdb_ref))
				{
					mi.modtype=mtCDA;
					mi.channels=2;
					mi.playtime=(tocentryN.cdte_addr.lba - tocentry.cdte_addr.lba)/CD_FRAMES;
					strcpy(mi.comment, dh->owner->cdrom->vdev);
					strcpy(mi.modname, "CDROM audio track");
					mdbWriteModuleInfo (mdb_ref, &mi);
				}
			}
			dh->callback_file (dh->token, &file->head);
			file->head.unref (&file->head);
		}
	}

next:
	dh->i++;

	return 1;
}


static struct ocpdir_t *cdrom_drive_readdir_dir (struct ocpdir_t *_self, uint32_t dirdb_ref)
{
	/* this can not succeed */
	return 0;
}


struct cdrom_drive_readdir_file_t // helper struct for cdrom_drive_readdir_file
{
	uint32_t dirdb_ref;
	struct ocpfile_t *retval;
};

static void cdrom_drive_readdir_file_file (void *_token, struct ocpfile_t *file) // helper function for cdrom_drive_readdir_file
{
	struct cdrom_drive_readdir_file_t *token = _token;
	if (token->dirdb_ref == file->dirdb_ref)
	{
		if (token->retval)
		{
			token->retval->unref (token->retval);
		}
		file->ref (file);
		token->retval = file;
	}
}

static void cdrom_drive_readdir_file_dir (void *_token, struct ocpdir_t *dir) // helper function for cdrom_drive_readdir_file
{
}

static struct ocpfile_t *cdrom_drive_readdir_file (struct ocpdir_t *_self, uint32_t dirdb_ref)
{
	/* we use cdrom_drive_readdir_start() and friends, since we perform a lot of IOCTL calls....*/
	ocpdirhandle_pt handle;
	struct cdrom_drive_readdir_file_t token;

	token.dirdb_ref = dirdb_ref;
	token.retval = 0;

	handle = _self->readdir_start (_self, cdrom_drive_readdir_file_file, cdrom_drive_readdir_file_dir, &token);
	if (!handle)
	{
		return 0;
	}

	while (_self->readdir_iterate (handle))
	{
	};
	_self->readdir_cancel (handle);

	return token.retval;
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

static struct ocpfilehandle_t *cdrom_track_open (struct ocpfile_t *_self)
{
	struct cdrom_track_ocpfile_t *self = (struct cdrom_track_ocpfile_t *)_self;
	char *temp = strdup (self->buffer);
	if (!temp)
	{
		return 0;
	}
	return mem_filehandle_open (self->head.dirdb_ref, temp, strlen (temp));
}

static uint64_t cdrom_track_filesize (struct ocpfile_t *_self)
{
	struct cdrom_track_ocpfile_t *self = (struct cdrom_track_ocpfile_t *)_self;
	return strlen (self->buffer);
}
static int cdrom_track_filesize_ready (struct ocpfile_t *_self)
{
	return 1;
}

#ifndef SUPPORT_STATIC_PLUGINS
char *dllinfo = "";
#endif

DLLEXTINFO_PREFIX struct linkinfostruct dllextinfo = {.name = "cdrom", .desc = "OpenCP UNIX audio-cdrom filebrowser (c) 2004-21 Stian Skjelstad", .ver = DLLVERSION, .size = 0, .Init = cdint, .Close = cdclose};
