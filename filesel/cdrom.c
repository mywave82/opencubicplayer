/*
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
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

#include "config.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/cdrom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "types.h"
#include "dirdb.h"
#include "boot/plinkman.h"
#include "modlist.h"
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
	uint32_t dirdbnode;
} *cdroms = 0;
static int cdromn = 0;

static struct dmDrive *dmCDROM=0;
static struct mdbreaddirregstruct cdReadDirReg;

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
			cdroms[cdromn].dirdbnode=dirdbFindAndRef(dmCDROM->basepath, vdev);
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
		} else
			close(fd);
	}
}

static int cdint(void)
{
	char dev[32], vdev[12];
	char a;

	mdbRegisterReadDir(&cdReadDirReg);

	dmCDROM=RegisterDrive("cdrom:");
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
		dirdbUnref(cdroms[i].dirdbnode);
	}
	free (cdroms);
	cdroms = 0;
	cdromn = 0;
	mdbUnregisterReadDir(&cdReadDirReg);
}

static FILE *cdrom_ReadHandle(struct modlistentry *entry)
{
	int fd=dup(cdroms[entry->adb_ref].fd);
	if (fd>=0)
		return fdopen(fd, "r");
	return NULL;
}

static int cdReadDir(struct modlist *ml, const struct dmDrive *drive, const uint32_t path, const char *mask, unsigned long opt)
{
	struct modlistentry entry;

	if (strcmp(drive->drivename, "cdrom:"))
		return 1;
	if (path==drive->basepath) /* / */
	{
		int i;
		for (i=0; i<cdromn;i++)
		{
			strcpy(entry.shortname, cdroms[i].vdev);
			strcpy(entry.name, cdroms[i].dev);
			entry.drive=drive;
			entry.dirdbfullpath=cdroms[i].dirdbnode;
			dirdbRef(entry.dirdbfullpath); /* overkill */
			entry.flags=MODLIST_FLAG_DIR;
			entry.fileref=0xffffffff;
			entry.adb_ref=0xffffffff;
			entry.Read=0; entry.ReadHeader=0; entry.ReadHandle=0;
			modlist_append(ml, &entry);
			dirdbUnref(entry.dirdbfullpath); /* overkill */
		}
	} else {
		int j;
		for (j=0; j<cdromn;j++)
		{
			if (cdroms[j].dirdbnode==path)
			{
				struct cdrom_tochdr tochdr;
				struct cdrom_tocentry tocentry;
				struct cdrom_tocentry tocentryN;
				int initlba=-1;
				int lastlba=lastlba; /* remove a warning */

				if (!ioctl(cdroms[j].fd, CDROMREADTOCHDR, &tochdr))
				{
					unsigned int i;
					for (i=tochdr.cdth_trk0;i<=(tochdr.cdth_trk1);i++)
					{
/*
						if (i>tochdr.cdth_trk1)
							i=CDROM_LEADOUT;
*/
						tocentry.cdte_track=i;
						tocentry.cdte_format=CDROM_LBA; /* CDROM_MSF */
						if (!ioctl(cdroms[j].fd, CDROMREADTOCENTRY, &tocentry))
						{
							tocentryN.cdte_track=(i==tochdr.cdth_trk1)?CDROM_LEADOUT:i+1;
							tocentryN.cdte_format=CDROM_LBA;
							ioctl(cdroms[j].fd, CDROMREADTOCENTRY, &tocentryN);
/*
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
*/
							if (!(tocentry.cdte_ctrl&CDROM_DATA_TRACK))
							{
								char buffer[12];
								struct moduleinfostruct mi;

								snprintf(buffer, 12, "TRACK%02d.CDA", i);

								if (initlba<0)
									initlba=tocentry.cdte_addr.lba;
								lastlba=tocentryN.cdte_addr.lba;
								fs12name(entry.shortname,buffer);
								strcpy(entry.name, buffer);
								entry.drive=drive;
								entry.dirdbfullpath=dirdbFindAndRef(path, buffer);
								entry.flags=MODLIST_FLAG_FILE|MODLIST_FLAG_VIRTUAL;
								if ((entry.fileref=mdbGetModuleReference(entry.shortname, 0))==0xffffffff)
								{
									dirdbUnref(entry.dirdbfullpath);
									return 0;
								}
								if (mdbGetModuleInfo(&mi, entry.fileref))
								{
									mi.modtype=mtCDA;
									mi.channels=2;
									mi.playtime=(tocentryN.cdte_addr.lba - tocentry.cdte_addr.lba)/CD_FRAMES;
									strcpy(mi.comment, cdroms[j].vdev);
									strcpy(mi.modname, "CDROM audio track");
									mdbWriteModuleInfo(entry.fileref, &mi);
								}
								entry.adb_ref=j; /* nasty hack, but hey.. it is free of use */
#warning ADB_REF hack used here
								entry.Read=0; entry.ReadHeader=0; entry.ReadHandle=cdrom_ReadHandle;
								modlist_append(ml, &entry);
								dirdbUnref(entry.dirdbfullpath);
							}
						}
					}
					if (initlba>=0)
					{
						const char *buffer="DISK.CDA";
						struct moduleinfostruct mi;

						fs12name(entry.shortname,buffer);
						strcpy(entry.name, buffer);
						entry.drive=drive;
						entry.dirdbfullpath=dirdbFindAndRef(path, buffer);
						entry.flags=MODLIST_FLAG_FILE|MODLIST_FLAG_VIRTUAL;
						if ((entry.fileref=mdbGetModuleReference(entry.shortname, 0))==0xffffffff)
						{
							dirdbUnref(entry.dirdbfullpath);
							return 0;
						}
						if (mdbGetModuleInfo(&mi, entry.fileref))
						{
							mi.modtype=mtCDA;
							mi.channels=2;
							mi.playtime=(lastlba - initlba)/CD_FRAMES;
							strcpy(mi.comment, cdroms[j].vdev);
							strcpy(mi.modname, "CDROM audio disc");
							mdbWriteModuleInfo(entry.fileref, &mi);
						}
						entry.adb_ref=j; /* nasty hack, but hey.. it is free of use */
#warning ADB_REF hack used here
						entry.Read=0; entry.ReadHeader=0; entry.ReadHandle=cdrom_ReadHandle;
						modlist_append(ml, &entry);
						dirdbUnref(entry.dirdbfullpath);
					}
				}
				break;
			}
		}
	}
	return 1;
}

static struct mdbreaddirregstruct cdReadDirReg = {cdReadDir MDBREADDIRREGSTRUCT_TAIL};
#ifndef SUPPORT_STATIC_PLUGINS
char *dllinfo = "";
#endif

DLLEXTINFO_PREFIX struct linkinfostruct dllextinfo = {.name = "cdrom", .desc = "OpenCP UNIX audio-cdrom filebrowser (c) 2004-09 Stian Skjelstad", .ver = DLLVERSION, .size = 0, .Init = cdint, .Close = cdclose};
