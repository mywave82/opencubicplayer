/* OpenCP Module Player
 * copyright (c) 2022-'25 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Parsing of ISO9660 filesystems
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "types.h"

#include "cdfs.h"
#include "iso9660.h"
#include "main.h"

#ifdef CDFS_DEBUG
#define debug_printf(...) fprintf (stderr, __VA_ARGS__)
#else
#define debug_printf(format,args...) ((void)0)
#endif

#ifdef CDFS_DEBUG
static const char *get_month (uint8_t i)
{
	switch (i)
	{
		case  1: return "jan";
		case  2: return "feb";
		case  3: return "mar";
		case  4: return "apr";
		case  5: return "may";
		case  6: return "jun";
		case  7: return "jul";
		case  8: return "aug";
		case  9: return "sep";
		case 10: return "oct";
		case 11: return "nov";
		case 12: return "dec";
		default: return "\?\?\?";
	}
}

static void DumpFS_dir_permissions_ISO9660 (struct iso_dirent_t *de) /* includes XA parsing */
{
	if (de->Flags & ISO9660_DIRENT_FLAGS_DIR)
	{
		debug_printf ("d");
	} else {
		debug_printf ("-");
	}
	if (de->XA)
	{
		debug_printf ("%c-%c%c-%c%c-%c",
			de->XA_attr & XA_ATTR__OWNER_READ ? 'r':'-',
			de->XA_attr & XA_ATTR__OWNER_EXEC ? 'x':'-',
			de->XA_attr & XA_ATTR__GROUP_READ ? 'r':'-',
			de->XA_attr & XA_ATTR__GROUP_EXEC ? 'x':'-',
			de->XA_attr & XA_ATTR__OTHER_READ ? 'r':'-',
			de->XA_attr & XA_ATTR__OTHER_EXEC ? 'x':'-');
	} else {
		debug_printf ("?-\?\?-\?\?-?");
	}
	debug_printf ("-");
}

static void DumpFS_dir_permissions_RockRidge (struct iso_dirent_t *de) /* Uses ISO9660/XA information if data is missing */
{
	if (de->RockRidge_PX_Present)
	{
		switch (de->RockRidge_PX_st_mode & 0170000)
		{
			case 0140000: debug_printf ("s"); break; /* socket */
			case 0120000: debug_printf ("l"); break; /* symlink */
			case 0100000: debug_printf ("-"); break; /* file */
			case 0060000: debug_printf ("b"); break; /* block special */
			case 0020000: debug_printf ("c"); break; /* character special */
			case 0040000: debug_printf ("d"); break; /* directory */
			case 0010000: debug_printf ("p"); break; /* pipe or FIFO */
			default:      debug_printf ("?"); break;
		}
	} else if (de->RockRidge_Symlink_Components_Length)
	{
		debug_printf ("s");
	} else if (de->Flags & ISO9660_DIRENT_FLAGS_DIR)
	{
		debug_printf ("d");
	} else if (de->RockRidge_IsAugmentedDirectory) /* file is a placeholder for a directory redirect */
	{
		debug_printf ("d");
	} else {
		debug_printf ("-");
	}

	if (de->RockRidge_PX_Present)
	{
		debug_printf ("%c%c%c%c%c%c%c%c%c%c",
			(de->RockRidge_PX_st_mode & 0000400) ? 'r' : '-', /* S_IRUSR */
			(de->RockRidge_PX_st_mode & 0000200) ? 'w' : '-', /* S_IWUSR */
			(de->RockRidge_PX_st_mode & 0004000) ?
				((de->RockRidge_PX_st_mode & 0000100) ? 's' : 'S'): /* S_IXUSR + SUID */
				((de->RockRidge_PX_st_mode & 0000100) ? 'x' : '-'), /* S_IXUSR */
			(de->RockRidge_PX_st_mode & 0000040) ? 'r' : '-', /* S_IRGRP */
			(de->RockRidge_PX_st_mode & 0000020) ? 'w' : '-', /* S_IWGRP */
			(de->RockRidge_PX_st_mode & 0002000) ?
				((de->RockRidge_PX_st_mode & 0000010) ? 's' : 'S'): /* S_IXGRP + GUID*/
				((de->RockRidge_PX_st_mode & 0000010) ? 'x' : '-'), /* S_IXGRP */
			(de->RockRidge_PX_st_mode & 0000004) ? 'r' : '-', /* S_IROTH */
			(de->RockRidge_PX_st_mode & 0000002) ? 'w' : '-', /* S_IWOTH */
			(de->RockRidge_PX_st_mode & 0000001) ? 'x' : '-', /* S_IXOTH */
			(de->RockRidge_PX_st_mode & 0001000) ? 't' : '-'); /* S_ISVTX (sticky) */
	} else if (de->XA)
	{
		debug_printf ("%c-%c%c-%c%c-%c-",
			de->XA_attr & XA_ATTR__OWNER_READ ? 'r':'-',
			de->XA_attr & XA_ATTR__OWNER_EXEC ? 'x':'-',
			de->XA_attr & XA_ATTR__GROUP_READ ? 'r':'-',
			de->XA_attr & XA_ATTR__GROUP_EXEC ? 'x':'-',
			de->XA_attr & XA_ATTR__OTHER_READ ? 'r':'-',
			de->XA_attr & XA_ATTR__OTHER_EXEC ? 'x':'-');
	} else {
		debug_printf ("\?\?\?\?\?\?\?\?\?\?");
	}
}

static void DumpFS_dir_owner_ISO9660 (struct iso_dirent_t *de) /* XA parsing */
{
	if (de->XA)
	{
		debug_printf (" %5d %5d", de->XA_UID, de->XA_GID);
	} else {
		debug_printf ("     ?     ?");
	}
}

static void DumpFS_dir_owner_RockRidge (struct iso_dirent_t *de) /* Uses ISO9660/XA information if data is missing */
{
	if (de->RockRidge_PX_Present)
	{
		debug_printf (" %5d %5d", de->RockRidge_PX_st_uid, de->RockRidge_PX_st_gid);
	} else {
		DumpFS_dir_owner_ISO9660 (de);
	}
}

static void DumpFS_dir_filesize_ISO9660 (struct iso_dirent_t *de)
{
	uint64_t len = 0;
	struct iso_dirent_t *iter;

	for (iter = de; iter; iter = iter->next_extent) /* files can be split into extents */
	{
		len += iter->Length;
	}
	debug_printf (" %10" PRIu64, len);
}

static void DumpFS_dir_filesize_RockRidge (struct iso_dirent_t *de) /* Falls back to ISO9660 */
{

	if (  (de->RockRidge_PX_Present) &&
	      (de->RockRidge_PN_Present) &&
	    (((de->RockRidge_PX_st_mode & 0170000) == 0060000) || /* block device */
	     ((de->RockRidge_PX_st_mode & 0170000) == 0020000)))  /* character special */
	{
		debug_printf ("  %4" PRIu32 ",%4" PRIu32, de->RockRidge_PN_major, de->RockRidge_PN_minor);
	} else {
		DumpFS_dir_filesize_ISO9660 (de);
	}
}

static void DumpFS_dir_cdate_ISO9660 (struct iso_dirent_t *de)
{
	debug_printf (" %02d %3s %4d %02u:%02u:%02u%+05d ",
		de->Created.day,              /* day */
		get_month(de->Created.month), /* month */
		de->Created.year,             /* year */
		de->Created.hour,             /* hour */
		de->Created.minute,           /* minute */
		de->Created.second,           /* second */
		de->Created.tz);              /* timezone */
}

static void DumpFS_dir_cdate_RockRidge (struct iso_dirent_t *de) /* Falls back to ISO9660 */
{
	if (de->RockRidge_TF_Created_Present)
	{
		debug_printf (" %02d %3s %4d %02u:%02u:%02u%+05d ",
			          de->RockRidge_TF_Created.day,       /* day */
			get_month(de->RockRidge_TF_Created.month),    /* month */
			          de->RockRidge_TF_Created.year,      /* year */
			          de->RockRidge_TF_Created.hour,      /* hour */
			          de->RockRidge_TF_Created.minute,    /* minute */
			          de->RockRidge_TF_Created.second,    /* second */
			          de->RockRidge_TF_Created.tz);       /* timezone */
	} else {
		DumpFS_dir_cdate_ISO9660 (de);
	}
}

/* assumes ASCII */
static void _DumpFS_dir_ISO9660 (struct Volume_Description_t *vd, const char *name, struct iso_dir_t *directory)
{
	int i, j;
	char *temp;

	debug_printf ("%s :\n", name);

	for (i=2; i < directory->dirents_count; i++) /* skip . and .. */
	{
		DumpFS_dir_permissions_ISO9660 (directory->dirents_data[i]);

		DumpFS_dir_owner_ISO9660 (directory->dirents_data[i]);

		DumpFS_dir_filesize_ISO9660 (directory->dirents_data[i]);

		DumpFS_dir_cdate_ISO9660 (directory->dirents_data[i]);

		for (j=0; j<directory->dirents_data[i]->Name_ISO9660_Length; j++)
		{
			if (directory->dirents_data[i]->Name_ISO9660[j]<32)
			{
				debug_printf ("\\x%08" PRIx8, directory->dirents_data[i]->Name_ISO9660[j]);
			} else {
				debug_printf ("%c", directory->dirents_data[i]->Name_ISO9660[j]);
			}
		}

		debug_printf ("\n");
	}

	for (i=2; i < directory->dirents_count; i++) /* skip . and .. */
	{
		int l;
		if (!(directory->dirents_data[i]->Flags & ISO9660_DIRENT_FLAGS_DIR))
		{
			continue;
		}
		l = strlen (name) + 1 + directory->dirents_data[i]->Name_ISO9660_Length + 1;
		temp = malloc (l);
		if (temp)
		{
			snprintf (temp, l, "%s/%s", name, directory->dirents_data[i]->Name_ISO9660);
			DumpFS_dir_ISO9660 (vd, temp, directory->dirents_data[i]->Absolute_Location);
			free (temp);
		}
	}
}

OCP_INTERNAL void DumpFS_dir_ISO9660 (struct Volume_Description_t *vd, const char *name, uint32_t Location)
{
	int j;
	for (j=0; j < vd->directories_count; j++)
	{
		if (vd->directories_data[j].Location == Location)
		{
			_DumpFS_dir_ISO9660 (vd, name, &vd->directories_data[j]);
			break;
		}
	}
}

/* assumes UCS-2 / UTF16BE */
void DumpFS_dir_Joliet (struct Volume_Description_t *vd, const char *name, uint32_t Location);
static void _DumpFS_dir_Joliet (struct Volume_Description_t *vd, const char *name, struct iso_dir_t *directory)
{
	int i;

	debug_printf ("%s :\n", name);

	for (i=2; i < directory->dirents_count; i++) /* skip . and .. */
	{
		DumpFS_dir_permissions_ISO9660 (directory->dirents_data[i]);

		DumpFS_dir_owner_ISO9660 (directory->dirents_data[i]);

		DumpFS_dir_filesize_ISO9660 (directory->dirents_data[i]);

		DumpFS_dir_cdate_ISO9660 (directory->dirents_data[i]);

		{
			uint8_t namebuffer[128*4]; /* maximum 128 UTF16BE codepoints, 4 is the maxlength of a codepoint in UTF-8 */
			char *inbuf = (char *)directory->dirents_data[i]->Name_ISO9660;
			size_t inbytesleft = directory->dirents_data[i]->Name_ISO9660_Length;
			char *outbuf = (char *)namebuffer;
			size_t outbytesleft = sizeof (namebuffer);
			//size_t res;

			/* res = */ iconv (UTF16BE_cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);

			debug_printf ("%.*s\n", (int)((uint8_t *)outbuf - namebuffer), namebuffer);
		}
	}

	for (i=2; i < directory->dirents_count; i++) /* skip . and .. */
	{
		char namebuffer[128*4+1]; /* maximum 128 UTF16BE codepoints, 4 is the maxlength of a codepoint in UTF-8 */
		char *inbuf = (char *)directory->dirents_data[i]->Name_ISO9660;
		size_t inbytesleft = directory->dirents_data[i]->Name_ISO9660_Length;
		char *outbuf = namebuffer;
		size_t outbytesleft = sizeof (namebuffer);
		char *temp;
		//size_t res;

		if (!(directory->dirents_data[i]->Flags & ISO9660_DIRENT_FLAGS_DIR))
		{
			continue;
		}
		/* res = */ iconv (UTF16BE_cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
		*outbuf = 0;
		temp = malloc (strlen (name) + 1 + strlen (namebuffer) + 1);
		if (temp)
		{
			sprintf (temp, "%s/%s", name, namebuffer);
			DumpFS_dir_Joliet (vd, temp, directory->dirents_data[i]->Absolute_Location);
			free (temp);
		}
	}
}

OCP_INTERNAL void DumpFS_dir_Joliet (struct Volume_Description_t *vd, const char *name, uint32_t Location)
{
	int j;
	for (j=0; j < vd->directories_count; j++)
	{
		if (vd->directories_data[j].Location == Location)
		{
			_DumpFS_dir_Joliet (vd, name, &vd->directories_data[j]);
			break;
		}
	}

}

/* assumes UTF-8 */
void DumpFS_dir_RockRidge (struct Volume_Description_t *vd, const char *name, uint32_t Location);
static void _DumpFS_dir_RockRidge (struct Volume_Description_t *vd, const char *name, struct iso_dir_t *directory)
{
	int i;

	debug_printf ("%s :\n", name);

	for (i=2; i < directory->dirents_count; i++) /* skip . and .. */
	{
		if (directory->dirents_data[i]->RockRidge_DirectoryIsRedirected)
		{
			continue;
		}

		DumpFS_dir_permissions_RockRidge (directory->dirents_data[i]);

		DumpFS_dir_owner_RockRidge (directory->dirents_data[i]);

		DumpFS_dir_filesize_RockRidge (directory->dirents_data[i]);

		DumpFS_dir_cdate_RockRidge (directory->dirents_data[i]);

		if (directory->dirents_data[i]->Name_RockRidge_Length)
		{
			debug_printf ("%.*s", directory->dirents_data[i]->Name_RockRidge_Length, directory->dirents_data[i]->Name_RockRidge);
		} else {
			debug_printf ("%.*s", directory->dirents_data[i]->Name_ISO9660_Length, directory->dirents_data[i]->Name_ISO9660);
		}

		if (directory->dirents_data[i]->RockRidge_Symlink_Components_Length)
		{
			uint32_t left = directory->dirents_data[i]->RockRidge_Symlink_Components_Length;
			uint8_t *next = directory->dirents_data[i]->RockRidge_Symlink_Components;
			uint8_t incontinue = 0;
			uint8_t first = 1;
			debug_printf (" -> ");
			while (left)
			{
				uint8_t nextcontinue = next[0] & 0x01;
				uint8_t length;

				if ((!first) && (!incontinue))
				{
					debug_printf ("/");
				}

				if (next[0] & 0x02)
				{
					debug_printf (".");
				} else if (next[0] & 0x04)
				{
					debug_printf ("..");
				} else if (next[0] & 0x08)
				{
					debug_printf ("/");
				} else if (next[0] & 0x10)
				{
					debug_printf ("currentdrive:");
				} else if (next[0] & 0x20)
				{
					debug_printf ("localhost:/");
				}
				left--;
				next++;

				if (!left) /* protect buffer */
				{
					break;
				}

				length = next[0];
				left--;
				next++;

				if (left < length) /* protect buffer */
				{
					break;
				}
				if (left)
				{
					debug_printf ("%.*s", length, next);
				}

				next += length;
				left -= length;

				incontinue = nextcontinue;
				first = 0;
			}
		}

		debug_printf ("\n");
	}

	for (i=2; i < directory->dirents_count; i++) /* skip . and .. */
	{
		int l;
		char *temp;
		uint32_t Location;

		if (directory->dirents_data[i]->RockRidge_DirectoryIsRedirected)
		{
			continue;
		} else if (directory->dirents_data[i]->RockRidge_PX_Present)
		{
			if ((directory->dirents_data[i]->RockRidge_PX_st_mode & 0170000) != 0040000) /* directory */ continue;
		} if (directory->dirents_data[i]->RockRidge_IsAugmentedDirectory)
		{
			Location = directory->dirents_data[i]->RockRidge_AugmentedDirectoryFrom;
			/* always with*/
		} else if (!(directory->dirents_data[i]->Flags & ISO9660_DIRENT_FLAGS_DIR))
		{
			continue;
		} else {
			Location = directory->dirents_data[i]->Absolute_Location; /* the normal way */
		}

		if (directory->dirents_data[i]->Name_RockRidge_Length)
		{
			l = strlen (name) + 1 + directory->dirents_data[i]->Name_RockRidge_Length + 1;
		} else {
			l = strlen (name) + 1 + directory->dirents_data[i]->Name_ISO9660_Length + 1;
		}
		temp = malloc (l);
		if (temp)
		{
			if (directory->dirents_data[i]->Name_RockRidge_Length)
			{
				snprintf (temp, l, "%s/%s", name, directory->dirents_data[i]->Name_RockRidge);
			} else {
				snprintf (temp, l, "%s/%s", name, directory->dirents_data[i]->Name_ISO9660);
			}
			DumpFS_dir_RockRidge (vd, temp, Location);
			free (temp);
		}
	}
}

OCP_INTERNAL void DumpFS_dir_RockRidge (struct Volume_Description_t *vd, const char *name, uint32_t Location)
{
	int j;
	for (j=0; j < vd->directories_count; j++)
	{
		if (vd->directories_data[j].Location == Location)
		{
			_DumpFS_dir_RockRidge (vd, name, &vd->directories_data[j]);
			break;
		}
	}
}
#endif


static void iso_dirent_free (struct iso_dirent_t *iso_dirent);

static void iso_dirent_clear (struct iso_dirent_t *iso_dirent)
{
	if (!iso_dirent)
	{
		return;
	}

	if (iso_dirent->next_extent)
	{
		iso_dirent_free (iso_dirent->next_extent);
	}

	free (iso_dirent->Name_RockRidge);

	free (iso_dirent->RockRidge_Symlink_Components);
}

static void iso_dirent_free (struct iso_dirent_t *iso_dirent)
{
	if (!iso_dirent)
	{
		return;
	}

	iso_dirent_clear (iso_dirent);
	free (iso_dirent);
}

static void iso_dir_clear (struct iso_dir_t *iso_dir)
{
	int i;

	if (!iso_dir)
	{
		return;
	}

	for (i = 0; i < iso_dir->dirents_count; i++)
	{
		iso_dirent_free (iso_dir->dirents_data[i]);
	}
	iso_dir->dirents_count = 0;
	if (iso_dir->dirents_size)
	{
		free (iso_dir->dirents_data);
	}
	iso_dir->dirents_size = 0;
	iso_dir->dirents_data = 0;
}

OCP_INTERNAL void Volume_Description_Free (struct Volume_Description_t *volume_desc)
{
	int i;

	if (!volume_desc)
	{
		return;
	}

	iso_dirent_clear (&volume_desc->root_dirent);

	for (i=0; i < volume_desc->directories_count; i++)
	{
		iso_dir_clear (&volume_desc->directories_data[i]);
	}
	free (volume_desc->directories_data);

	free (volume_desc->directory_scan_queue_data);

	free (volume_desc);
}

#ifdef CDFS_DEBUG
static const char *PartitionType (uint8_t type)
{
	switch (type)
	{
		case 0x00: return "Empty partition entry";
		case 0x01: return "FAT12 < 32M";
		case 0x02: return "XENIX root";
		case 0x03: return "XENIX usr";
		case 0x04: return "FAT16 < 32M";
		case 0x05: return "Extended partition (CHS addressing)"; /* new set of 4 partitions */
		case 0x06: return "FAT16B >= 32M (FAT12 / FAT16 outside first 32M of the disk)";
		case 0x07: return "NTFS/HPFS/exFAT";
		case 0x08: return "FAT12/FAT16 logical sectored"; /* Commodore MS-DOS 3.x */
		case 0x09: return "AIX/QNX/Coherent file system/OS-9 RBF";
		case 0x0a: return "OS/2 Boot Manager / 	Coherent swap partition";
		case 0x0b: return "FAT32 (CHS addressing)";
		case 0x0c: return "FAT32 (LBA addressing)";
		//case 0x0d:
		case 0x0e: return "FAT16B (LBA addressing)";
		case 0x0f: return "Extended partition (LBA addressing)"; /* new set of 4 partitions */
		//case 0x10:
		case 0x11: return "FAT12/FAT16 Logical sectored"; /* Leading Edge MS-DOS 3.x */
		case 0x12: return "Service/Recovery";
		//case 0x13:
		case 0x14: return "FAT16 (hidden)";
		case 0x15: return "SWAP";
		case 0x16: return "FAT16B (hidden)";
		case 0x17: return "NTFS/HPFS/exFAT (hidden)";
		case 0x18: return "AST Zero Volt Suspend or SmartSleep partition";
		case 0x19: return "Willowtech Photon coS";
		//case 0x1a:
		case 0x1b: return "FAT32 (hidden)";
		case 0x1c: return "FAT32 (hidden, LBA addressing)";
		//case 0x1d:
		case 0x1e: return "FAT16 (hidden, LBA addressing)";
		case 0x1f: return "Extended partition (hidden, LBA addressing)"; /* new set of 4 partitions */
		case 0x20: return "Windows Mobile update XIP";
		case 0x21: return "FSo2 (Oxygen File System)";
		case 0x22: return "Oxygen Extended Partition";
		case 0x23: return "Windows Mobile boot XIP";
		case 0x24: return "FAT12/FAT16 Logical sectored"; /* NEC MS-DOS 3.30 */
		//case 0x25:
		//case 0x26:
		case 0x27: return "Windows Recovery Environment (NTFS hidden)";
		//case 0x28:
		//case 0x29:
		case 0x2a:
		case 0x31:
		case 0x32:
		case 0x33:
		case 0x34:
		case 0x36:
		default: return "Reserved";
		case 0x2b: return "SyllableSecure (SylStor)";
		//case 0x2c:
		//case 0x2d:
		//case 0x2e:
		//case 0x2f:
		//case 0x30:
		case 0x71:
		case 0x73:
		case 0x35: return "JFS";
		//case 0x37:
		case 0x38: return "THEOS version 3.2, 2 GB";
		case 0x39: return "Plan 9 edition 3 partition/THEOS version 4 spanned partition";
		case 0x3a: return "THEOS version 4, 4 GB partition";
		case 0x3b: return "THEOS version 4 extended partition";
		case 0x3c: return "PqRP (PartitionMagic or DriveImage in progress)";
		case 0x3d: return "NetWare (hidden)";
		//case 0x3e:
		//case 0x3f:
		case 0x40: return "PICK R83/Venix 80286";
		case 0x41: return "Personal RISC Boot / Old Linux/Minix / PPC PReP (Power PC Reference Platform) Boot";
		case 0x42: return "Old Linux swap / Dynamic extended partition marker";
		case 0x43: return "Old Linux native";
		case 0x44: return "Norton GoBack, WildFile GoBack, Adaptec GoBack, Roxio GoBack";
		case 0x45: return "Priam/Boot-US boot manager (1 cylinder)/EUMEL/ELAN (L2)";
		case 0x46: return "EUMEL/ELAN (L2)";
		case 0x47: return "EUMEL/ELAN (L2)";
		case 0x48: return "EUMEL/ELAN (L2), ERGOS L3";
		//case 0x49:
		case 0x4a: return "Aquila/ALFS/THIN advanced lightweight file system for DOS";
		//case 0x4b:
		case 0x4c: return "Aos (A2) file system";
		case 0x4d: return "Primary QNX POSIX volume on disk";
		case 0x4e: return "Secondary QNX POSIX volume on disk";
		case 0x4f: return "Tertiary QNX POSIX volume on disk / ETH Oberon Boot/native file system";
		case 0x50: return "ETH Oberon Alternative native file system / OnTrack Disk Manager 4 Read-only partition / Lynx RTOS";
		case 0x51: return "OnTrack Disk Manager 4-6 Read-write partition (Aux 1)";
		case 0x52: return "CP/M-80";
		case 0x53: return "OnTrack Disk Manager 6 Auxiliary 3 (WO)";
		case 0x54: return "OnTrack Disk Manager 6 Dynamic Drive Overlay (DDO)";
		case 0x55: return "EZ-Drive, Maxtor, MaxBlast, or DriveGuide INT 13h redirector volume";
		case 0x56: return "FAT12/FAT16 Logical sectored"; /* AT&T */
		case 0x57: return "VNDI partition";
		//case 0x58:
		case 0x59: return "yocFS";
		case 0x60: return "Priam EDisk Partitioned Volume";
		case 0x61: return "FAT12 (hidden)"; /* SpeedStor */
		//case 0x62:
		case 0x63: return "FAT16 (hidden, read-only)"; /* SpeedStor */
		case 0x64: return "FAT16 (hidden) / NetWare File System 286/2 / PC-ARMOUR";
		case 0x65: return "NetWare File System 386";
		case 0x66: return "NetWare File System 386 / Storage Management Services (SMS) / FAT16 (hidden, read-only)";
		case 0x67: return "Wolf Mountain";
		//case 0x68:
		case 0x69: return "Novell Storage Services (NSS)";
		//case 0x6a:
		//case 0x6b:
		case 0x6c: return "BSD slice (DragonFly BSD)";
		case 0x70: return "DiskSecure multiboot";
		case 0x72: return "APTI alternative FAT12 (CHS, SFN) / V7/x86";
		case 0x74: return "FAT16B (hidden)"; /* SpeedStor */
		//case 0x75:
		case 0x76: return "FAT16B (hidden, read-only)"; /* SpeedStor */
		case 0x77: return "VNDI, M2FS, M2CS";
		case 0x78: return "XOSL bootloader file system";
		case 0x79: return "APTI alternative FAT16 (CHS, SFN)";
		case 0x7a: return "APTI alternative FAT16 (LBA, SFN)";
		case 0x7b: return "APTI alternative FAT16B (CHS, SFN)";
		case 0x7c: return "APTI alternative FAT32 (LBA, SFN)";
		case 0x7d: return "APTI alternative FAT32 (CHS, SFN)";
		case 0x7e: return "PrimoCache Level 2 cache";
		case 0x7f: return "Reserved for individual or local use and temporary or experimental projects";
		case 0x80: return "MINIX file system (old)";
		case 0x81: return "MINIX file system";
		case 0x82: return "Linux swap space / Solaris x86 (for Sun disklabels up to 2005)";
		case 0x83: return "Any native Linux file system";
		case 0x84: return "APM hibernation (suspend to disk, S2D) / FAT16 (hidden C:\\) / Rapid Start hibernation data";
		case 0x85: return "Linux extended"; /* new set of 4 partitions */
		case 0x86: return "Fault-tolerant FAT16B mirrored volume set / Linux RAID superblock with auto-detect (old)";
		case 0x87: return "Fault-tolerant HPFS/NTFS mirrored volume set";
		case 0x88: return "Linux plaintext partition table";
		//case 0x89:
		case 0x8a: return "Linux kernel image"; /* AiR-BOOT */
		case 0x8b: return "Legacy fault-tolerant FAT32 mirrored volume set";
		case 0x8c: return "Legacy fault-tolerant FAT32 mirrored volume set";
		case 0x8d: return "FAT12 (hidden)"; /* FreeDOS Free FDISK */
		case 0x8e: return "Linux LVM since 1999";
		// case 0x8f:
		case 0x90: return "FAT16 (hidden)"; /* FreeDOS Free FDISK */
		case 0x91: return "extended partition (hidden, CHS addressing)"; /* FreeDOS Free FDISK */
		case 0x92: return "FAT16B (hidden)"; /* FreeDOS Free FDISK */
		case 0x93: return "Hidden Linux file system / Amoeba native file system";
		case 0x94: return "Amoeba bad block table";
		case 0x95: return "EXOPC native";
		case 0x96: return "ISO-9660";
		case 0x97: return "FAT32 (hidden)"; /* FreeDOS Free FDISK */
		case 0x98: return "FAT32 (hidden) / Service partition (bootable FAT) / ROM-DOS SuperBoot";
		case 0x99: return "Early Unix";
		case 0x9a: return "FAT16 (hidden)"; /* FreeDOS Free FDISK */
		case 0x9b: return "Extended partition (hidden, LBA addressing)";
		//case 0x9c:
		//case 0x9d:
		case 0x9e: return "ForthOS";
		//case 0x9f:
		case 0xa0: return "Diagnostic partition for HP laptops / Hibernate partition";
		case 0xa1: return "Hibernate partition"; /* Phoenix, NEC */
		case 0xa2: return "Hard Processor System (HPS) ARM preloader";
		//case 0xa3:
		//case 0xa4:
		case 0xa5: return "BSD slice (BSD/386, 386BSD, NetBSD (before 1998-02-19), FreeBSD)";
		case 0xa6: return "OpenBSD slice";
		case 0xa7: return "NeXTSTEP ?";
		case 0xa8: return "Apple Darwin, Mac OS X UFS";
		case 0xa9: return "NetBSD slice";
		case 0xaa: return "Olivetti MS-DOS FAT12 (1.44 MB)"; /* Olivetti */
		case 0xab: return "Apple Darwin, Mac OS X boot / GO!";
		case 0xac: return "Apple RAID, Mac OS X RAID";
		case 0xad: return "RISC OS ADFS / FileCore format";
		case 0xae: return "ShagOS file system";
		case 0xaf: return "HFS and HFS+ / swap";
		case 0xb0: return "Boot-Star dummy partition";
		case 0xb1: return "QNX Neutrino power-safe file system";
		case 0xb2: return "QNX Neutrino power-safe file system";
		case 0xb3: return "QNX Neutrino power-safe file system";
		//case 0xb4:
		//case 0xb5:
		case 0xb6: return "Corrupted fault-tolerant FAT16B mirrored master volume";
		case 0xb7: return "BSDI native file system / swap / Corrupted fault-tolerant HPFS/NTFS mirrored master volume";
		case 0xb8: return "BSDI swap / native file system";
		//case 0xb9:
		//case 0xba:
		case 0xbb: return "OEM Secure Zone / Corrupted fault-tolerant FAT32 mirrored master volume";
		case 0xbc: return "Corrupted fault-tolerant FAT32 mirrored master volume";
		//case 0xbd:
		case 0xbe: return "Solaris 8 boot";
		case 0xbf: return "Solaris x86 (for Sun disklabels, since 2005)";
		case 0xc0: return "Secured FAT partition <32 MB";
		case 0xc1: return "Secured FAT12";
		case 0xc2: return "Hidden Linux native file system";
		case 0xc3: return "Hidden Linux swap";
		case 0xc4: return "Secured FAT16";
		case 0xc5: return "Secured extended partition (CHS addressing)";
		case 0xc6: return "Secured FAT16B / Corrupted fault-tolerant FAT16B mirrored slave volume";
		case 0xc7: return "Syrinx boot / Corrupted fault-tolerant HPFS/NTFS mirrored slave volume";
		//case 0xc8: return "Reserved for DR-DOS since 1997";
		//case 0xc9: return "Reserved for DR-DOS since 1997";
		//case 0xca: return "Reserved for DR-DOS since 1997";
		case 0xcb: return "Secured FAT32 / Corrupted fault-tolerant FAT32 mirrored slave volume";
		case 0xcc: return "Secured FAT32 / Corrupted fault-tolerant FAT32 mirrored slave volume";
		case 0xcd: return "Memory dump";
		case 0xce: return "Secured FAT16B";
		case 0xcf: return "Secured extended partition (LBA addressing)";
		case 0xd0: return "Secured FAT partition >32MB";
		case 0xd1: return "Secured FAT12";
		//case 0xd2:
		//case 0xd3:
		case 0xd4: return "Secured FAT16";
		case 0xd5: return "Secured extended partition (CHS addressing)";
		case 0xd6: return "Secured FAT16B";
		//case 0xd7:
		case 0xd8: return "CP/M-86";
		//case 0xd9:
		case 0xda: return "Non-file system data / Shielded disk";
		case 0xdb: return "FAT32 system restore partition (DSR)";
		//case 0xdc:
		case 0xdd: return "Hidden memory dump";
		case 0xde: return "FAT16 utility/diagnostic partition";
		case 0xdf: return "DG/UX virtual disk manager / EMBRM";
		case 0xe0: return "ST AVFS";
		case 0xe1: return "FAT12 <16MB";
		//case 0xe2:
		case 0xe3: return "FAT12 (read-only)";
		case 0xe4: return "FAT16 <32MB";
		case 0xe5: return "Logical sectored FAT12/FAT16";
		case 0xe6: return "FAT16 (read-only)";
		//case 0xe7:
		case 0xe8: return "Linux Unified Key Setup";
		//case 0xe9:
		//case 0xea:
		case 0xeb: return "BFS";
		case 0xec: return "SkyFS";
		case 0xed: return "Sprytix EDC loader / Was proposed for GPT hybrid MBR";
		case 0xee: return "GPT protective MBR";
		case 0xef: return "EFI system partition. Can be a FAT12, FAT16, FAT32 (or other) file system";
		case 0xf0: return "PA-RISC Linux boot loader";
		//case 0xf1:
		case 0xf2: return "Logical sectored FAT12 or FAT16 secondary partition";
		//case 0xf3:
		case 0xf4: return "FAT16B / Single volume partition for NGF or TwinFS";
		case 0xf5: return "MD0-MD9 multi volume partition for NGF or TwinFS";
		case 0xf6: return "FAT16B (read-only)";
		case 0xf7: return "EFAT / Solid State file system";
		case 0xf8: return "Protective partition for the area containing system firmware";
		case 0xf9: return "pCache ext2/ext3 persistent cache";
		//case 0xfa:
		case 0xfb: return "VMware VMFS file system partition";
		case 0xfc: return "VMware swap / VMKCORE kernel dump partition";
		case 0xfd: return "Linux RAID superblock with auto-detect";
		case 0xfe: return "PS/2 IML partition / PS/2 recovery partition (FAT12 reference disk floppy image) / Old Linux LVM";
		case 0xff: return "XENIX bad block table";
	}
}
#endif

#include "ElTorito.c"

static uint32_t decode_uint32_both (uint8_t *buffer, const char *name)
{
#ifdef CDFS_DEBUG
	uint32_t l = buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);
#endif
	uint32_t b = buffer[7] | (buffer[6] << 8) | (buffer[5] << 16) | (buffer[4] << 24);

	debug_printf ("%s: %"PRId32"%s\n", name, b, (l != b) ? " WARNING LSB and MSB version does not match":"");

	return b;
}

static uint32_t decode_uint32_lsb (uint8_t *buffer, const char *name)
{
	uint32_t l = buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);

	debug_printf ("%s: %"PRId32"\n", name, l);

	return l;
}

static uint32_t decode_uint32_msb (uint8_t *buffer, const char *name)
{
	uint32_t b = buffer[3] | (buffer[2] << 8) | (buffer[1] << 16) | (buffer[0] << 24);

	debug_printf ("%s: %"PRId32"\n", name, b);

	return b;
}

static uint16_t decode_uint16_both (uint8_t *buffer, const char *name)
{
#ifdef CDFS_DEBUG
	uint16_t l = buffer[0] | (buffer[1] << 8);
#endif
	uint16_t b = buffer[3] | (buffer[2] << 8);

	debug_printf ("%s: %"PRId16"%s\n", name, b, (l != b) ? " WARNING LSB and MSB version does not match":"");

	return b;
}

static uint16_t decode_uint16_lsb (uint8_t *buffer, const char *name)
{
	uint16_t l = buffer[0] | (buffer[1] << 8);

	debug_printf ("%s: %"PRId16"\n", name, l);

	return l;
}

static uint16_t decode_uint16_msb (uint8_t *buffer, const char *name)
{
	uint16_t b = buffer[1] | (buffer[0] << 8);

	debug_printf ("%s: %"PRId16"\n", name, b);

	return b;
}

#ifdef CDFS_DEBUG
static void decode_datetime_17 (uint8_t *buffer, const char *name, struct iso9660_datetime_t *target)
{
	int i = ((int)(int8_t)buffer[6])-40;
	int tz = ((i/4)*100) + ((i % 4)*15);

	debug_printf ("%s: ", name);

	if (target)
	{
		target->year = (buffer[0] - '0') * 1000 +
		               (buffer[1] - '0') * 100  +
		               (buffer[2] - '0') * 10   +
		               (buffer[3] - '0');

		target->month = (buffer[4] - '0') * 10 +
		                (buffer[5] - '0');

		target->day = (buffer[6] - '0') * 10 +
		              (buffer[7] - '0');

		target->hour = (buffer[8] - '0') * 10 +
		               (buffer[9] - '0');

		target->minute = (buffer[10] - '0') * 10 +
		                 (buffer[11] - '0');

		target->second = (buffer[12] - '0') * 10 +
		                 (buffer[13] - '0');
/*
		target->hundreth = (buffer[14] - '0') * 10 +
		                   (buffer[15] - '0');
*/
		target->tz = tz;
	}

	for (i=0; i < 16; i++)
	{
		if ((i==4) ||
		    (i==6) ||
		    (i==8))
		{
			debug_printf ("-");
		}
		if ((i==10) ||
		    (i==12))
		{
			debug_printf (":");
		}
		if (i==14) /* the hundreths of a second is usually 00 */
		{
			debug_printf (".");
		}


		debug_printf ("%c", buffer[i]);
	}

	debug_printf ("%+05d\n", tz);
}

static void decode_datetime_7 (uint8_t *buffer, const char *name, struct iso9660_datetime_t *target)
{
	int i = (int8_t)buffer[6];
	int tz = ((i/4)*100) + ((i % 4)*15);

	debug_printf ("%s: ", name);

	if (target)
	{
		target->year   = buffer[0] + 1900;
		target->month  = buffer[1];
		target->day    = buffer[2];
		target->hour   = buffer[3];
		target->minute = buffer[4];
		target->second = buffer[5];
		target->tz = tz;
	}


	debug_printf ("%04d-%02d-%02d-%02d:%02d:%02d%+05d\n",
		1900 + buffer[0],
		buffer[1],
		buffer[2],
		buffer[3],
		buffer[4],
		buffer[5],
		tz);
}
#endif

static void CDFS_Render_ISO9660_directory (struct cdfs_disc_t *disc, struct Volume_Description_t *vd, uint32_t parent_directory, struct iso_dir_t *directory)
{
	int i;

	for (i=2; i < directory->dirents_count; i++) /* skip . and .. */
	{
		char *temp;
		if (directory->dirents_data[i]->Flags & ISO9660_DIRENT_FLAGS_HIDDEN) continue;
		temp = malloc (directory->dirents_data[i]->Name_ISO9660_Length + 1);
		sprintf (temp, "%.*s", directory->dirents_data[i]->Name_ISO9660_Length, directory->dirents_data[i]->Name_ISO9660);
		if (directory->dirents_data[i]->Flags & ISO9660_DIRENT_FLAGS_DIR)
		{
			uint32_t Location = directory->dirents_data[i]->Absolute_Location;
			int j;
			uint32_t this_directory_handle = CDFS_Directory_add (disc, parent_directory, temp);
			for (j=0; j < vd->directories_count; j++)
			{
				if (vd->directories_data[j].Location == Location)
				{
					CDFS_Render_ISO9660_directory (disc, vd, this_directory_handle, &vd->directories_data[j]);
					break;
				}
			}
		} else {
			int handle = CDFS_File_add (disc, parent_directory, temp);
			struct iso_dirent_t *iter;
			uint32_t Length = directory->dirents_data[i]->Length;
			for (iter = directory->dirents_data[i]; iter; iter = iter->next_extent) /* files can be split into extents */
			{
				uint32_t RunLength = Length;
				if (RunLength > (iter->Length * 2048))
				{
					RunLength = iter->Length * 2048;
				}
				CDFS_File_extent (disc, handle, iter->Absolute_Location, RunLength, 0);
			}
		}
		free (temp);
	}
}

OCP_INTERNAL void CDFS_Render_ISO9660 (struct cdfs_disc_t *disc, uint32_t parent_directory) /* parent_directory should point to "ISO9660" */
{
	struct Volume_Description_t *vd = disc->iso9660_session->Primary_Volume_Description;
	uint32_t Location = disc->iso9660_session->Primary_Volume_Description->root_dirent.Absolute_Location;
	int j;
	for (j=0; j < vd->directories_count; j++)
	{
		if (vd->directories_data[j].Location == Location)
		{
			CDFS_Render_ISO9660_directory (disc, vd, parent_directory, &vd->directories_data[j]);
			break;
		}
	}
}

static void CDFS_Render_Joliet_directory (struct cdfs_disc_t *disc, struct Volume_Description_t *vd, uint32_t parent_directory, struct iso_dir_t *directory)
{
	int i;

	for (i=2; i < directory->dirents_count; i++) /* skip . and .. */
	{
		char namebuffer[128*4+1]; /* maximum 128 UTF16BE codepoints, 4 is the maxlength of a codepoint in UTF-8 */
		char *inbuf = (char *)directory->dirents_data[i]->Name_ISO9660;
		size_t inbytesleft = directory->dirents_data[i]->Name_ISO9660_Length;
		char *outbuf = namebuffer;
		size_t outbytesleft = sizeof (namebuffer);

		if (directory->dirents_data[i]->Flags & ISO9660_DIRENT_FLAGS_HIDDEN) continue;

		/* res = */ iconv (UTF16BE_cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
		*outbuf = 0;

		if (directory->dirents_data[i]->Flags & ISO9660_DIRENT_FLAGS_DIR)
		{
			uint32_t Location = directory->dirents_data[i]->Absolute_Location;
			int j;
			uint32_t this_directory_handle = CDFS_Directory_add (disc, parent_directory, namebuffer);
			for (j=0; j < vd->directories_count; j++)
			{
				if (vd->directories_data[j].Location == Location)
				{
					CDFS_Render_Joliet_directory (disc, vd, this_directory_handle, &vd->directories_data[j]);
					break;
				}
			}
		} else {
			int handle = CDFS_File_add (disc, parent_directory, namebuffer);
			struct iso_dirent_t *iter;
			uint32_t Length = directory->dirents_data[i]->Length;
			for (iter = directory->dirents_data[i]; iter; iter = iter->next_extent) /* files can be split into extents */
			{
				uint32_t RunLength = Length;
				if (RunLength > (iter->Length * 2048))
				{
					RunLength = iter->Length * 2048;
				}
				CDFS_File_extent (disc, handle, iter->Absolute_Location, RunLength, 0);
			}
		}
	}
}

OCP_INTERNAL void CDFS_Render_Joliet (struct cdfs_disc_t *disc, uint32_t parent_directory) /* parent_directory should point to "Joliet" */
{
	struct Volume_Description_t *vd = disc->iso9660_session->Supplementary_Volume_Description;
	uint32_t Location = disc->iso9660_session->Supplementary_Volume_Description->root_dirent.Absolute_Location;
	int j;
	for (j=0; j < vd->directories_count; j++)
	{
		if (vd->directories_data[j].Location == Location)
		{
			CDFS_Render_Joliet_directory (disc, vd, parent_directory, &vd->directories_data[j]);
			break;
		}
	}
}

static void CDFS_Render_RockRidge_directory (struct cdfs_disc_t *disc, struct Volume_Description_t *vd, uint32_t parent_directory, struct iso_dir_t *directory)
{
	int i, j;

	for (i=2; i < directory->dirents_count; i++) /* skip . and .. */
	{
		char *temp = 0;
		if (directory->dirents_data[i]->RockRidge_DirectoryIsRedirected) continue;

		if (directory->dirents_data[i]->Name_RockRidge_Length)
		{
			temp = malloc (directory->dirents_data[i]->Name_RockRidge_Length + 1);
			if (!temp) continue;
			sprintf (temp, "%.*s", directory->dirents_data[i]->Name_RockRidge_Length, directory->dirents_data[i]->Name_RockRidge);
		} else {
			temp = malloc (directory->dirents_data[i]->Name_ISO9660_Length + 1);
			if (!temp) continue;
			sprintf (temp, "%.*s", directory->dirents_data[i]->Name_ISO9660_Length, directory->dirents_data[i]->Name_ISO9660);
		}

		if (directory->dirents_data[i]->RockRidge_PX_Present)
		{
			switch (directory->dirents_data[i]->RockRidge_PX_st_mode & 0170000)
			{
				case 0140000: goto next; /* socket */
				case 0120000: goto next; /* symlink */
				case 0100000: goto file; /* file */
				case 0060000: goto next; /* block special */
				case 0020000: goto next; /* character special */
				case 0040000: goto dir;  /* directory */
				case 0010000: goto next; /* pipe or FIFO */
				default:      goto next; break;
			}
		}

		if ((directory->dirents_data[i]->Flags & ISO9660_DIRENT_FLAGS_DIR) || directory->dirents_data[i]->RockRidge_IsAugmentedDirectory)
		{dir:;{ /* clang is stricter than gcc, no labels before variables */
			uint32_t Location = directory->dirents_data[i]->Absolute_Location;
			uint32_t this_directory_handle = CDFS_Directory_add (disc, parent_directory, temp);
			if (directory->dirents_data[i]->RockRidge_IsAugmentedDirectory)
			{
				Location = directory->dirents_data[i]->RockRidge_AugmentedDirectoryFrom;
			}
			for (j=0; j < vd->directories_count; j++)
			{
				if (vd->directories_data[j].Location == Location)
				{
					CDFS_Render_RockRidge_directory (disc, vd, this_directory_handle, &vd->directories_data[j]);
					break;
				}
			}
		}} else {file:;{ /* clang is stricter than gcc, no labels before variables */
			int handle = CDFS_File_add (disc, parent_directory, temp);
			struct iso_dirent_t *iter;
			uint32_t Length = directory->dirents_data[i]->Length;
			for (iter = directory->dirents_data[i]; iter; iter = iter->next_extent) /* files can be split into extents */
			{
				uint32_t RunLength = Length;
				if (RunLength > (iter->Length * 2048))
				{
					RunLength = iter->Length * 2048;
				}
				CDFS_File_extent (disc, handle, iter->Absolute_Location, RunLength, 0);
			}
		}}
next:
		free (temp);
	}
}

OCP_INTERNAL void CDFS_Render_RockRidge (struct cdfs_disc_t *disc, uint32_t parent_directory) /* parent_directory should point to "RockRidge" */
{
	struct Volume_Description_t *vd = disc->iso9660_session->Primary_Volume_Description;
	uint32_t Location = disc->iso9660_session->Primary_Volume_Description->root_dirent.Absolute_Location;
	int j;
	for (j=0; j < vd->directories_count; j++)
	{
		if (vd->directories_data[j].Location == Location)
		{
			CDFS_Render_RockRidge_directory (disc, vd, parent_directory, &vd->directories_data[j]);
			break;
		}
	}
}

#include "susp.c"

static int decode_record (struct cdfs_disc_t *disc, struct Volume_Description_t *volumedesc, uint8_t *buffer, int len, struct iso_dirent_t *de, int isrootnode)
{
//	uint8_t ExtendedAttributeLength;
//	uint8_t Padding1;
//	uint8_t Padding2;
	int i;

	if (len < 1+8+8+7+1+1+1+4+1)
	{
		debug_printf ("     WARNING - not enough data to hold a full record\n");
		return -1;
	}

	//ExtendedAttributeLength = buffer[0];
	/* These would in theory be placed infront of the file-data */
	debug_printf ("     Extended Attribute Length: %d\n", buffer[0]);

	de->Absolute_Location = decode_uint32_both (buffer + 1, "     Location");

	de->Length = decode_uint32_both (buffer + 9, "     Length");

#ifdef CDFS_DEBUG
	decode_datetime_7 (buffer + 17, "     DateTime", &de->Created);
#endif

	de->Flags = buffer[24];
	debug_printf ("     Flags: 0x%02" PRIx8 "\n", buffer[24]);
	if (de->Flags & ISO9660_DIRENT_FLAGS_HIDDEN)
	{
		debug_printf ("       Hidden\n");
	}
	debug_printf ("       Type: %s\n", de->Flags & ISO9660_DIRENT_FLAGS_DIR ? "directory" : "file");
	if (de->Flags & ISO9660_DIRENT_FLAGS_ASSOCIATED_FILE)
	{
		debug_printf ("       Associated file?\n");
	}
	if (de->Flags & ISO9660_DIRENT_FLAGS_EXTENDED_ATTRIBUTES_PRESENT)
	{
		debug_printf ("       Extended attributes present\n");
	}
	if (de->Flags & ISO9660_DIRENT_FLAGS_PERMISSIONS_PRESENT)
	{
		debug_printf ("       Owner/Group permissions present\n");
	}
	if (de->Flags & ISO9660_DIRENT_FLAGS_FILE_NOT_LAST_EXTENT)
	{
		debug_printf ("       Expect more file-extents for this file\n");
	}

	debug_printf ("     Interleave Unit Size: %d\n", buffer[25]);
	debug_printf ("     Interleave Gap Size: %d\n", buffer[26]);

	decode_uint16_both (buffer + 27, "     Volume Sequence");

	de->Name_ISO9660_Length = buffer[31];
	memcpy (de->Name_ISO9660, buffer + 32, de->Name_ISO9660_Length);
	de->Name_ISO9660[de->Name_ISO9660_Length] = 0;
	debug_printf ("     Name Length: %d\n", buffer[31]);
	if (31+buffer[31] > len)
	{
		debug_printf ("     WARNING - not enough data to hold the full name\n");
		return -1;
	}

	if ((de->Name_ISO9660_Length == 1) && (buffer[32] == 0))
	{
		debug_printf ("     Name: (root)\n");
	} else {
		debug_printf ("     Name: \"");
		for (i=0; i < de->Name_ISO9660_Length; i++)
		{
//			if (buffer[32+i] == ';') break;
			debug_printf ("%c", buffer[32+i]);
		}
		debug_printf ("\"\n");
	}
	if (len - 32 - de->Name_ISO9660_Length + ((de->Name_ISO9660_Length + 1) & 1))
	{
		int loopcount = 0;
		int o = 32 + de->Name_ISO9660_Length + /* padding */ ((de->Name_ISO9660_Length + 1) & 1);
		debug_printf ("     System Use: (%d - %d => ) %d (padding = %d)\n", len, o, len - o, (de->Name_ISO9660_Length + 1) & 1);
		decode_susp (disc, volumedesc, de, buffer + o, len - o, isrootnode, 0, &loopcount);
	}

	if ((de->Name_ISO9660_Length >= 2) &&
	    (!volumedesc->UTF16) && /* Joliet */
	    (!(de->Flags & ISO9660_DIRENT_FLAGS_DIR)) &&
	    (de->Name_ISO9660[de->Name_ISO9660_Length-2] == ';') &&
	    (de->Name_ISO9660[de->Name_ISO9660_Length-1] == '1') )
	{
		de->Name_ISO9660[de->Name_ISO9660_Length-2] = 0;
		de->Name_ISO9660_Length -= 2;
	} else if ((de->Name_ISO9660_Length >= 4) &&
	    (volumedesc->UTF16) && /* Joliet */
	    (!(de->Flags & ISO9660_DIRENT_FLAGS_DIR)) &&
	    (de->Name_ISO9660[de->Name_ISO9660_Length-4] == '\0') &&
	    (de->Name_ISO9660[de->Name_ISO9660_Length-3] == ';') &&
	    (de->Name_ISO9660[de->Name_ISO9660_Length-2] == '\0') &&
	    (de->Name_ISO9660[de->Name_ISO9660_Length-1] == '1') )
	{
		de->Name_ISO9660[de->Name_ISO9660_Length-4] = 0;
		de->Name_ISO9660[de->Name_ISO9660_Length-3] = 0;
		de->Name_ISO9660_Length -= 4;
	}	


	return 0;
}

static void path_table_decode (uint8_t *buffer, int_fast32_t len, uint16_t (*decode_uint16)(uint8_t *buffer, const char *name), uint32_t (*decode_uint32)(uint8_t *buffer, const char *name))
{
	int i;

	if (len & 1)
	{
		debug_printf ("    WARNING, len is a odd number\n");
		len++;
	}
	while (len >= 8)
	{
		uint8_t LengthOfDirectoryIdentifier = buffer[0];
		uint8_t ExtendedAttributeRecordLength = buffer[1];
		/*uint32_t LocationOfExtent = */ (*decode_uint32) (buffer + 2, "     Location");
		/*uint16_t ParentDirectory  = */ (*decode_uint16) (buffer + 6, "     Parent");

		if ((LengthOfDirectoryIdentifier + 8 > len))
		{
			debug_printf ("     WARNING, no space for Directory Identifier\n");
		} else {
			if ((LengthOfDirectoryIdentifier == 1) && (buffer[8] == 0x00))
			{
				debug_printf ("     Directory Identifier: (1) (root)\n");
			} else {
				debug_printf ("     Directory Identifier: (%d) \"", (int)LengthOfDirectoryIdentifier);
				for (i = 0; i < LengthOfDirectoryIdentifier; i++)
				{
					debug_printf ("%c", buffer[8 + i]);
				}
				debug_printf ("\"\n");

				if (ExtendedAttributeRecordLength)
				{
					if (LengthOfDirectoryIdentifier + ExtendedAttributeRecordLength + 8 > len)
					{
						debug_printf ("     WARNING, no space for Extended Attribute Record\n");
					} else {
						debug_printf ("     Extended Attribute Record: (%d)", (int)ExtendedAttributeRecordLength);
						for (i = 0; i < ExtendedAttributeRecordLength; i++)
						{
							debug_printf (" 0x%02" PRIx8, buffer[8 + LengthOfDirectoryIdentifier + i]);
						}
						debug_printf ("\n");
					}
				}
			}
		}
		debug_printf ("\n");
		buffer += (8 + LengthOfDirectoryIdentifier + ExtendedAttributeRecordLength + 1) & ~ 1;
		len    -= (8 + LengthOfDirectoryIdentifier + ExtendedAttributeRecordLength + 1) & ~ 1;
	}
}

static int Volume_Description_Queue_Directory (struct Volume_Description_t *self, uint32_t Location, uint32_t Length, int isrootnode)
{
	int i;

	for (i=0; i < self->directories_count; i++)
	{
		if (self->directories_data[i].Location == Location)
		{
			debug_printf ("WARNING - Volume_Description_Queue_Directory() tried to add an entry already present\n");
			return 0;
		}
	}

	if (self->directory_scan_queue_count >= self->directory_scan_queue_size)
	{
		struct iso_dir_queue *temp = realloc (self->directory_scan_queue_data, sizeof (self->directory_scan_queue_data[0]) * (self->directory_scan_queue_size + 64));
		if (!temp)
		{
			debug_printf ("WARNING - Volume_Description_Queue_Directory() realloc() failed\n");
			return -1;
		}
		self->directory_scan_queue_data = temp;
		self->directory_scan_queue_size += 64;
	}

	for (i=0; i < self->directory_scan_queue_count; i++)
	{
		if (self->directory_scan_queue_data[0].Location == Location)
		{
			debug_printf ("WARNING - Volume_Description_Queue_Directory() tried to add an entry already present\n");
			return 0;
		}
		if (self->directory_scan_queue_data[0].Location > Location)
		{
			break;
		}
	}

	if (i != self->directory_scan_queue_count)
	{
		memmove (self->directory_scan_queue_data + i + 1, self->directory_scan_queue_data + i, sizeof (self->directory_scan_queue_data[0]) * (self->directory_scan_queue_count - i));
	}
	self->directory_scan_queue_data[i].Location = Location;
	self->directory_scan_queue_data[i].Length = Length;
	self->directory_scan_queue_data[i].isrootnode = isrootnode;
	self->directory_scan_queue_count += 1;

	return 0;
}

static int Volume_Description_DeQueue (struct cdfs_disc_t *disc, struct Volume_Description_t *self)
{
	uint32_t Length;
	int i;
	int j, o;

	int isrootnode;

	struct iso_dir_t *targetdir;

	if (!self->directory_scan_queue_count)
	{ /* this should never happen */
		return 0;
	}

	if (self->directories_count >= self->directories_size)
	{
		struct iso_dir_t *temp = realloc (self->directories_data, sizeof (self->directories_data[0]) * (self->directories_size + 32));
		if (!temp)
		{
			debug_printf ("WARNING - Volume_Description_DeQueue() realloc() failed\n");
			return -1;
		}
		self->directories_data = temp;
		self->directories_size += 32;
	}

	for (i=0; i < self->directories_count; i++)
	{
		if (self->directories_data[i].Location == self->directory_scan_queue_data[0].Location)
		{
			debug_printf ("WARNING - Volume_Description_DeQueue() tried to add an entry already present\n");
			return 0;
		}
		if (self->directories_data[i].Location > self->directory_scan_queue_data[0].Location)
		{
			break;
		}
	}

	if (i != self->directories_count)
	{
		memmove (self->directories_data + i + 1, self->directories_data + i, sizeof (self->directories_data[0]) * (self->directories_count - i));
	}
	targetdir = &self->directories_data[i];
	targetdir->Location   = self->directory_scan_queue_data[0].Location;
	           Length     = self->directory_scan_queue_data[0].Length;
	           isrootnode = self->directory_scan_queue_data[0].isrootnode;
	targetdir->dirents_count = 0;
	targetdir->dirents_size = 0;
	targetdir->dirents_data = 0;
	self->directories_count += 1;

	memmove (self->directory_scan_queue_data, self->directory_scan_queue_data + 1, sizeof (self->directory_scan_queue_data[0]) * (self->directory_scan_queue_count - 1));
	self->directory_scan_queue_count--;

	debug_printf ("\n[dir Location:0x%08" PRIx32 "]\n", targetdir->Location);

	j = 0; /* record counter */
	o = 0; /* sector counter */
	while (Length)
	{
		int len;
		uint8_t *b;
		uint8_t buffer[2048];

		if (cdfs_fetch_absolute_sector_2048 (disc, targetdir->Location + o, buffer))
		{
			break;
		}

		/* a record never crosses a sector-boundary */

		o++;
		len = Length < 2048 ? Length : 2048;
		Length -= len;
		b = buffer;

		while (len)
		{
			struct iso_dirent_t *dirent;
			int used;

			if (!b[0])
			{
				b++;
				len--;
				continue;
			}

			used = b[0];

			if (used > len)
			{
				return -1;
			}
			debug_printf ("\n");

			dirent = calloc (sizeof (*dirent), 1);
			if (decode_record (disc, self, b + 1, used - 1, dirent, isrootnode))
			{
				iso_dirent_free (dirent);
				return -1;
			}
			isrootnode = 0;

			b += used;
			len -= used;

			if ((used & 1) && len)
			{
				b += 1;
				len -= 1;
			}

			if ((targetdir->dirents_count) &&
			    (targetdir->dirents_data[targetdir->dirents_count-1]->Name_ISO9660_Length == dirent->Name_ISO9660_Length) &&
			    (!memcmp(targetdir->dirents_data[targetdir->dirents_count-1]->Name_ISO9660, dirent->Name_ISO9660, dirent->Name_ISO9660_Length)))
			{ /* append extent */
				struct iso_dirent_t *last = targetdir->dirents_data[targetdir->dirents_count-1];
				while (last->next_extent)
				{
					last = last->next_extent;
				}
				last->next_extent = dirent;
			} else {
				if (targetdir->dirents_count >= targetdir->dirents_size)
				{
					struct iso_dirent_t **temp = realloc (targetdir->dirents_data, sizeof (*temp) * (targetdir->dirents_size + 16));
					if (!temp)
					{
						fprintf (stderr, "Volume_Description_DeQueue realloc failed\n");
						iso_dirent_free (dirent);
						return -1;
					}
					targetdir->dirents_data = temp;
					targetdir->dirents_size += 16;
				}
				targetdir->dirents_data[targetdir->dirents_count] = dirent;
				targetdir->dirents_count += 1;
			}

			if (j == 0)
			{ /* self - expect \0 or .*/
//#warning verify self, but ignore it in general, except augmented directories in Rock Ridge
			} else if (j == 1)
			{ /* parent - expect .. */
//#warning verify parent, but ignore it in general
			} else {
				/* queue if a dir, recursive scan please */
				if (dirent->Flags & ISO9660_DIRENT_FLAGS_DIR)
				{
					if (Volume_Description_Queue_Directory(self, dirent->Absolute_Location, dirent->Length, 0))
					{
						return -1;
					}
				}
			}
			j++;
		}
	}
	return 0;
}

static struct Volume_Description_t *Primary_Volume_Descriptor (struct cdfs_disc_t *disc, uint8_t *buffer, uint32_t sector, int IsPrimary)
{
	int i;
	uint32_t offset;
	uint32_t path_table_size;
	uint8_t *path_table_buffer = 0;
	uint32_t path_table_l_loc;
	uint32_t path_table_m_loc;
	int retval = 0;

	struct Volume_Description_t *volumedesc = calloc (sizeof (struct Volume_Description_t), 1);

	if (!volumedesc)
	{
		fprintf (stderr, "Primary_Volume_Descriptor() calloc() failed\n");
		return 0;
	}

	/* buffer[0x07] */
	debug_printf ("  system_identifier: \"");
	for (i=0; i < 32; i++)
	{
		if (buffer[0x08 + i] == 0x00) break;
		debug_printf ("%c", buffer[0x08 + i]);
	}
	debug_printf ("\"\n");

	debug_printf ("  volume_identifier: \"");
	for (i=0; i < 32; i++)
	{
		if (buffer[0x28 + i] == 0x00) break;
		debug_printf ("%c", buffer[0x08 + i]);
	}
	debug_printf ("\"\n");

	/* buffer[0x48-0x4f] */

	decode_uint32_both (buffer + 0x50, "  volume_space_size (sectors)"); /* i */

	debug_printf ("  escape_sequences:");
	for (i=0; i < 32; i++)
	{
		debug_printf (" 0x%02" PRIx8, buffer[0x58 + i]);
	}

	/* ISO-10646 UCS-2 == UNICODE UTF-16
	 * ISO-10646 UCS-4 == UNICODE UTF-32
	 */

	// TODO, parse all G0 etc commands that overrides the 8-bit encoding in order to support japanese etc.
	// \x25\x40  is to escape back to ASCII ?
	       if (!memcmp (buffer + 0x58, "\x25\x2f\x40", 4)) { volumedesc->UTF16=1; debug_printf (" UCS-2_LEVEL_1"); } /* Unicode 1.1? - Level 1 requires no composite character support */
	//else if (!memcmp (buffer + 0x58, "\x25\x2f\x41", 4)) { volumedesc->UTF32=1; debug_printf (" UCS-4-LEVEL_1"); }
	  else if (!memcmp (buffer + 0x58, "\x25\x2f\x43", 4)) { volumedesc->UTF16=1; debug_printf (" UCS-2_LEVEL_2"); } /* Unicode 1.1? - Level 2 requires support for specific scripts (including most of the Unicode scripts such as Arabic and Thai) */
	//else if (!memcmp (buffer + 0x58, "\x25\x2f\x44", 4)) { volumedesc->UTFxx=1; debug_printf (" UCS-4-LEVEL_2");
	  else if (!memcmp (buffer + 0x58, "\x25\x2f\x45", 4)) { volumedesc->UTF16=1; debug_printf (" UCS-2_LEVEL_3"); } /* Unicode 1.1? - Level 3 requires unrestricted support for composite characters in all languages. */
	//else if (!memcmp (buffer + 0x58, "\x25\x2f\x46", 4)) { volumedesc->UTF32=1; debug_printf (" UCS-4-LEVEL_3/UTF-32BE"); }
	  else if (!memcmp (buffer + 0x58, "\x25\x2f\x47", 4)) { volumedesc->UTF8=1;  debug_printf (" UTF-8_LEVEL_1"); }
	  else if (!memcmp (buffer + 0x58, "\x25\x2f\x48", 4)) { volumedesc->UTF8=1;  debug_printf (" UTF-8_LEVEL_2"); }
	  else if (!memcmp (buffer + 0x58, "\x25\x2f\x49", 4)) { volumedesc->UTF8=1;  debug_printf (" UTF-8_LEVEL_3"); } /* Level 3 part of name is phased out */
	  else if (!memcmp (buffer + 0x58, "\x25\x2f\x4a", 4)) { volumedesc->UTF16=1; debug_printf (" UCS-2_LEVEL_1"); }
	  else if (!memcmp (buffer + 0x58, "\x25\x2f\x4b", 4)) { volumedesc->UTF16=1; debug_printf (" UCS-2_LEVEL_2"); }
	  else if (!memcmp (buffer + 0x58, "\x25\x2f\x4c", 4)) { volumedesc->UTF16=1; debug_printf (" USC-2_LEVEL_3/UTF-16BE"); } /* Level 3 part of name is phased out */

	  else if (!memcmp (buffer + 0x58, "\x25\x47", 3))     { volumedesc->UTF8=1;  debug_printf (" UTF-8");         }
	debug_printf ("\n");

	if (IsPrimary && volumedesc->UTF16)
	{
		debug_printf ("   WARNING: Primary Volume Descriptor does not allow UCS-2 / UTF16 - This disc has malformed header, ignoring\n");
		volumedesc->UTF16 = 0;
	}

	/* buffer[0x58-0x77] */

	decode_uint16_both (buffer + 0x78, "  volume_set_size");      /* h */
	decode_uint16_both (buffer + 0x7c, "  volume_seq_num");       /* h */
	decode_uint16_both (buffer + 0x80, "  logical_block_size");   /* h */
	path_table_size =
	decode_uint32_both (buffer + 0x84, "  path_table_size");      /* i */
	path_table_l_loc =
	decode_uint32_lsb  (buffer + 0x8c, "  path_table_l_loc");     /* <i */
	decode_uint32_lsb  (buffer + 0x90, "  path_table_opt_l_loc"); /* <i */
	path_table_m_loc =
	decode_uint32_msb  (buffer + 0x94, "  path_table_m_loc");     /* >i */
	decode_uint32_msb  (buffer + 0x98, "  path_table_opt_m_loc"); /* >i */

	/* The PATH_TABLE_L vs M can in theory be used as a DRM scheme. Different OS'es will use different once, or skip them intererly and just rely on the directory entries */
	if (path_table_size)
	{
		path_table_buffer = malloc ((path_table_size + SECTORSIZE - 1) & ~ (SECTORSIZE - 1));
	}
	if (path_table_buffer)
	{
		uint_fast32_t sectors = ((path_table_size + SECTORSIZE - 1) & ~ (SECTORSIZE - 1)) / SECTORSIZE;
		uint_fast32_t ui;

		for (ui = 0; ui < sectors; ui++)
		{
			if (cdfs_fetch_absolute_sector_2048 (disc, path_table_l_loc + ui, path_table_buffer + ui * SECTORSIZE))
			{
				debug_printf ("  WARNING - Unable to fetch path_table_l\n");
				break;
			}
		}
		if (ui == sectors)
		{
			debug_printf ("   [PATH_TABLE_L]\n");
			path_table_decode (path_table_buffer, path_table_size, decode_uint16_lsb, decode_uint32_lsb);
		}

		for (ui = 0; ui < sectors; ui++)
		{
			if (cdfs_fetch_absolute_sector_2048 (disc, path_table_m_loc + ui, path_table_buffer + ui * SECTORSIZE))
			{
				debug_printf ("  WARNING - Unable to fetch path_table_m\n");
				break;
			}
		}
		if (ui == sectors)
		{
			debug_printf ("   [PATH_TABLE_M]\n");
			path_table_decode (path_table_buffer, path_table_size, decode_uint16_msb, decode_uint32_msb);
		}

		free (path_table_buffer);
		path_table_buffer = 0;
	}

	{ /* root_record */
		uint8_t record_len = buffer[0x9c];
		if (record_len)
		{
			offset = (int)record_len;
			debug_printf ("   [root record]\n");
			retval |= decode_record (disc, volumedesc, buffer + 0x9d, record_len - 1, &volumedesc->root_dirent, 0);
		} else {
			offset = 0;
		}
	}

	debug_printf ("  volume_set_identifier: \"");
	for (i=0; i < 128; i++)
	{
		if (buffer[0x9c + offset + i] == 0x00) break;
		debug_printf ("%c", buffer[0x9c + offset + i]);
	}
	debug_printf ("\"\n");

	debug_printf ("  publisher_identifier: \"");
	for (i=0; i < 128; i++)
	{
		if (buffer[0x11c + offset + i] == 0x00) break;
		debug_printf ("%c", buffer[0x11c + offset + i]);
	}
	debug_printf ("\"\n");

	debug_printf ("  data_preparer_identifier: \"");
	for (i=0; i < 128; i++)
	{
		if (buffer[0x19c + offset + i] == 0x00) break;
		debug_printf ("%c", buffer[0x19c + offset + i]);
	}
	debug_printf ("\"\n");

	debug_printf ("  application_identifier: \"");
	for (i=0; i < 128; i++)
	{
		if (buffer[0x21c + offset + i] == 0x00) break;
		debug_printf ("%c", buffer[0x21c + offset + i]);
	}
	debug_printf ("\"\n");

	debug_printf ("  copyright_file_identifier: \"");
	for (i=0; i < 38; i++)
	{
		if (buffer[0x29c + offset + i] == 0x00) break;
		debug_printf ("%c", buffer[0x29c + offset + i]);
	}
	debug_printf ("\"\n");

	debug_printf ("  abstract_file_identifier: \"");
	for (i=0; i < 36; i++)
	{
		if (buffer[0x2c2 + offset + i] == 0x00) break;
		debug_printf ("%c", buffer[0x2c2 + offset + i]);
	}
	debug_printf ("\"\n");

	debug_printf ("  bibliographic_file_identifier: \"");
	for (i=0; i < 37; i++)
	{
		if (buffer[0x2e6 + offset + i] == 0x00) break;
		debug_printf ("%c", buffer[0x2e6 + offset + i]);
	}
	debug_printf ("\"\n");

#ifdef CDFS_DEBUG
	decode_datetime_17 (buffer + offset + 0x30b, "  volume_datetime_created", 0);
	decode_datetime_17 (buffer + offset + 0x31c, "  volume_datetime_modified", 0);
	decode_datetime_17 (buffer + offset + 0x32d, "  volume_datetime_expires", 0);
	decode_datetime_17 (buffer + offset + 0x33e, "  volume_datetime_effective", 0);
#endif

	debug_printf ("  file_structure_version: %d\n", (int)buffer[offset + 0x34f]);

	if ((buffer[1024 + 0] == 'C') &&
	    (buffer[1024 + 1] == 'D') &&
	    (buffer[1024 + 2] == '-') &&
	    (buffer[1024 + 3] == 'X') &&
	    (buffer[1024 + 4] == 'A') &&
	    (buffer[1024 + 5] == '0') &&
	    (buffer[1024 + 6] == '0') &&
	    (buffer[1024 + 7] == '1'))
	{
		volumedesc->XA1 = 1;
		volumedesc->SystemUse_Skip = 14;
		debug_printf ("  XA1 header found\n"); /* Disc should be a CDROM-XA */
	}

	if (!retval)
	{
		Volume_Description_Queue_Directory (volumedesc, volumedesc->root_dirent.Absolute_Location, volumedesc->root_dirent.Length, 1);
	}

	while (volumedesc->directory_scan_queue_count)
	{
		retval |= Volume_Description_DeQueue(disc, volumedesc);
	}

	if (retval)
	{
		Volume_Description_Free (volumedesc);
		return 0;
	} else {
		return volumedesc;
	}
}

OCP_INTERNAL void ISO9660_Descriptor (struct cdfs_disc_t *disc, uint8_t buffer[SECTORSIZE], const int sector, const int descriptor, int *descriptorend)
{
	if (buffer[0] != 2)
	{
		if ((buffer[6] != 1))
		{
			debug_printf ("descriptor[%d] has invalid version (expected 1 got %u)\n", descriptor, buffer[6]);
		}
	} else {
		if ((buffer[6] != 1) && (buffer[6] != 2))
		{
			debug_printf ("descriptor[%d] has invalid version (expected 1 got %u)\n", descriptor, buffer[6]);
		}
	}

	switch (buffer[0])
	{
		case 0: /* Boot record volume descriptor */
			debug_printf ("descriptor[%d]: Boot record volume descriptor\n", descriptor);
			if (descriptor != 2)
			{
				debug_printf ("WARNING - boot record volume must always be descriptor 2, located at sector 17 (in the last session)\n");
			}
			if (!memcmp (buffer + 0x07, "EL TORITO SPECIFICATION\0\0\0\0\0\0\0\0\0", 0x26 - 0x07 + 1))
			{
#ifdef CDFS_DEBUG
				uint32_t elsector;
#endif

				debug_printf (" El Torito    format identifier found\n");
				if (memcmp (buffer + 0x27, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 0x46 - 0x27 + 1))
				{
					debug_printf (" WARNING - reserved area is not padded with \\0\n");
				}
#ifdef CDFS_DEBUG
				elsector =  buffer[0x47]        |
					   (buffer[0x48] << 8)  |
					   (buffer[0x49] << 16) |
					   (buffer[0x4a] << 24);
				debug_printf (" Absolute sector of Boot Catalog %"PRId32"\n", elsector);
				ElTorito_abs_sector (disc, elsector);
#endif
				/* byte 0x4b-0x7ff should be zero.... */
			} else {
				debug_printf (" WARNING - Unknown boot record identifier: %s\n", buffer + 0x07);
			}
			break;
		case 1: /* Primary volume descriptor */
			{
				struct Volume_Description_t *temp;
				debug_printf ("descriptor[%d]: Primary volume descriptor\n", descriptor);
				temp = Primary_Volume_Descriptor (disc, buffer, sector, 1);
				if (temp)
				{
					if (!disc->iso9660_session)
					{
						disc->iso9660_session = calloc (sizeof (*disc->iso9660_session), 1);
						if (!disc->iso9660_session)
						{
							fprintf (stderr, "ISO9660_Descriptor() calloc() failed\n");
							Volume_Description_Free (temp); /* we do not store the redundant descriptions */
							return;
						}
					}		
					if (disc->iso9660_session->Primary_Volume_Description)
					{
						Volume_Description_Free (temp); /* we do not store the redundant descriptions */
					} else {
						disc->iso9660_session->Primary_Volume_Description = temp;
					}
				}
				break;
			}
		case 2: /* Supplementary volume descriptor, or enhanced volume descriptor */
			{
				struct Volume_Description_t *temp;
				debug_printf ("descriptor[%d]: Supplementary volume descriptor, or enhanced volume descriptor\n", descriptor);
				temp = Primary_Volume_Descriptor (disc, buffer, sector, 0);
				if (temp)
				{
					if (!disc->iso9660_session)
					{
						disc->iso9660_session = calloc (sizeof (*disc->iso9660_session), 1);
						if (!disc->iso9660_session)
						{
							fprintf (stderr, "ISO9660_Descriptor() calloc() failed\n");
							Volume_Description_Free (temp); /* we do not store the redundant descriptions */
							return;
						}
					}		

					if (disc->iso9660_session->Supplementary_Volume_Description)
					{
						Volume_Description_Free (temp); /* we do not store the redundant descriptions */
					} else {
						disc->iso9660_session->Supplementary_Volume_Description = temp;
					}
				}
				break;

			}
			break;
		case 3: /* Volume partition descriptor */
			debug_printf ("descriptor[%d]: Volume partition descriptor\n", descriptor);

			break;
		default:
			debug_printf ("descriptor[%d]: Descriptor type %u is unknown\n", descriptor, buffer[0]);
			break;
		case 255: /* Volume descriptor set terminator */
			debug_printf ("descriptor[%d]: Volume partition set descriptor\n", descriptor);

			*descriptorend = 1;
			break;
	}
}

OCP_INTERNAL void ISO9660_Session_Free (struct ISO9660_session_t **s)
{
	if (!s) return;
	if (!(*s)) return;
	if ((*s)->Primary_Volume_Description)       Volume_Description_Free ((*s)->Primary_Volume_Description);
	if ((*s)->Supplementary_Volume_Description) Volume_Description_Free ((*s)->Supplementary_Volume_Description);
	free (*s);
	(*s) = 0;
}
