#include "config.h"
#include <iconv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "types.h"

#include "audio.h"
#include "cdfs.h"
#include "cue.h"
#include "boot/plinkman.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "iso9660.h"
#include "main.h"
#include "stuff/err.h"
#include "toc.h"
#include "udf.h"

#ifdef CDFS_DEBUG
#define debug_printf(...) fprintf (stderr, __VA_ARGS__)
#else
#define debug_printf(format,args...) ((void)0)
#endif

iconv_t __attribute__ ((visibility ("internal"))) UTF16BE_cd = ((iconv_t) -1);

static struct ocpdir_t *cdfs_disc_to_dir (struct cdfs_disc_t *disc)
{
	int descriptor = 0;
	int descriptorend = 0;
	int ISO9660descriptorend = 0 ;
	uint8_t buffer[SECTORSIZE];

#ifdef CDFS_DEBUG
	{
		int i;
		for (i=0; i < disc->datasources_count; i++)
		{
			debug_printf ("DISC-SOURCE.%d first:%d last:%d (length=%d) zerofill=%d\n",
			              i,
			              disc->datasources_data[i].sectoroffset,
			              disc->datasources_data[i].sectoroffset + disc->datasources_data[i].sectorcount - 1,
			              disc->datasources_data[i].sectorcount,
			             !disc->datasources_data[i].file);
		}
	}
#endif

	while (!descriptorend)
	{
		uint32_t sector = 16 + descriptor;

		if (cdfs_fetch_absolute_sector_2048 (disc, sector, buffer))
		{
			goto check_audio;
		}

		descriptor++; /* descriptor are 1, not zero based... for the user */

		if ((buffer[1] == 'B') &&
		    (buffer[2] == 'E') &&
		    (buffer[3] == 'A') &&
		    (buffer[4] == '0') &&
		    (buffer[5] == '1'))
		{
			debug_printf ("descriptor[%d] Beginning Extended Area Descriptor (just a marker)\n", descriptor);
			continue;
		}

		if ((buffer[1] == 'T') &&
		    (buffer[2] == 'E') &&
		    (buffer[3] == 'A') &&
		    (buffer[4] == '0') &&
		    (buffer[5] == '1'))
		{
			debug_printf ("descriptor[%d] Terminating Extended Area Descriptor (just a marker)\n", descriptor);
			descriptorend = 1;
			break;
		}

		if ((buffer[1] == 'B') &&
		    (buffer[2] == 'O') &&
		    (buffer[3] == 'O') &&
		    (buffer[4] == 'T') &&
		    (buffer[5] == '2'))
		{
#warning TODO ECMA 168 BOOT
			debug_printf ("descriptor[%d] ECMA 167/168 Boot Descriptor\n", descriptor);
			continue;
		}

		if ((buffer[1] == 'C') &&
		    (buffer[2] == 'D') &&
		    (buffer[3] == 'W') &&
		    (buffer[4] == '0') &&
		    (buffer[5] == '2'))
		{
#warning TODO ECMA 168
			debug_printf ("descriptor[%d] ISO/IEC 13490 / ECMA 168 Descriptor\n", descriptor);
			continue;
		}

		if ((buffer[1] == 'N') &&
		    (buffer[2] == 'S') &&
		    (buffer[3] == 'R') &&
		    (buffer[4] == '0') &&
		    (buffer[5] == '2'))
		{
			debug_printf ("descriptor[%d] ISO/IEC 13346:1995 / ECMA 167 2nd edition / UDF Descriptor\n", descriptor);
			UDF_Descriptor (disc);
			continue;
		}

		if ((buffer[1] == 'N') &&
		    (buffer[2] == 'S') &&
		    (buffer[3] == 'R') &&
		    (buffer[4] == '0') &&
		    (buffer[5] == '3'))
		{
			debug_printf ("descriptor[%d] ECMA 167 3rd edition / UDF Descriptor\n", descriptor);
			UDF_Descriptor (disc);
			continue;
		}

		if ((buffer[1] =='C') ||
		    (buffer[2] =='D') ||
		    (buffer[3] =='0') ||
		    (buffer[4] =='0') ||
		    (buffer[5] =='1'))
		{
			debug_printf ("descriptor[%d] ISO 9660 / ECMA 119 Descriptor\n", descriptor);
			if (ISO9660descriptorend)
			{
				debug_printf ("WARNING - this is unepected, CD001 parsing should be complete\n");
			}
			ISO9660_Descriptor (disc, buffer, sector, descriptor, &ISO9660descriptorend);
			continue;
		}

		if (ISO9660descriptorend)
		{
			debug_printf ("descriptor[%d] has invalid Identifier (got '%c%c%c%c%c'), but ISO9660 has already terminated list, so should be OK\n", descriptor, buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);
			descriptorend = 1;
		} else {
			debug_printf ("descriptor[%d] has invalid Identifier (got '%c%c%c%c%c')\n", descriptor, buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);
		}
		break;
	}

	if (disc->iso9660_session)
	{
		if (disc->iso9660_session->Primary_Volume_Description)
		{
			debug_printf ("ISO9660 vanilla\n");
#ifdef CDFS_DEBUG
			DumpFS_dir_ISO9660 (disc->iso9660_session->Primary_Volume_Description, ".", disc->iso9660_session->Primary_Volume_Description->root_dirent.Absolute_Location);
#endif
			CDFS_Render_ISO9660 (disc, CDFS_Directory_add (disc, 0, "ISO9660"));
		}
		if (disc->iso9660_session->Primary_Volume_Description && disc->iso9660_session->Primary_Volume_Description->RockRidge)
		{
			debug_printf ("ISO9660 RockRidge\n");
#ifdef CDFS_DEBUG
			DumpFS_dir_RockRidge (disc->iso9660_session->Primary_Volume_Description, ".", disc->iso9660_session->Primary_Volume_Description->root_dirent.Absolute_Location);
#endif
			CDFS_Render_RockRidge (disc, CDFS_Directory_add (disc, 0, "RockRidge"));
		}
		if (disc->iso9660_session->Supplementary_Volume_Description && disc->iso9660_session->Supplementary_Volume_Description->UTF16)
		{
			debug_printf ("ISO9660 Joliet\n");
#ifdef CDFS_DEBUG
			DumpFS_dir_Joliet (disc->iso9660_session->Supplementary_Volume_Description, ".", disc->iso9660_session->Supplementary_Volume_Description->root_dirent.Absolute_Location);
#endif
			CDFS_Render_Joliet (disc, CDFS_Directory_add (disc, 0, "Joliet"));
		}
	}

	if (disc->udf_session)
	{
#ifdef CDFS_DEBUG
		DumpFS_UDF (disc);
#endif
		CDFS_Render_UDF (disc, CDFS_Directory_add (disc, 0, "UDF"));
	}

check_audio:
	Check_Audio (disc);

	if ((disc->dir_fill > 1) || (disc->file_fill > 0))
	{
		//cdfs_disc_unref (disc);
		return &disc->dirs[0]->head;
	}

//fail_out:
	cdfs_disc_unref (disc);
	return 0;
}


static struct ocpdir_t *test_cue (struct ocpfile_t *file)
{
	struct ocpfilehandle_t *fh;
	struct cdfs_disc_t     *disc;
	char                    buffer[65536];
	int                     result;
	struct cue_parser_t    *data;

	fh = file->open (file);
	if (!fh)
	{
		return 0;
	}

	result = fh->read (fh, buffer, sizeof (buffer) - 1);
	buffer[result] = 0;
	if (result < 5)
	{
		fh->unref (fh);
		return 0;
	}
	data = cue_parser_from_data (buffer);
	fh->unref (fh); fh = 0;

	if (!data)
	{
		return 0;
	}

	disc = cue_parser_to_cdfs_disc (file, data);
	cue_parser_free (data);

	if (!disc)
	{
		return 0;
	}

	return cdfs_disc_to_dir (disc);
}

static struct ocpdir_t *test_toc (struct ocpfile_t *file)
{
	struct ocpfilehandle_t *fh;
	struct cdfs_disc_t     *disc;
	char                    buffer[65536];
	int                     result;
	struct toc_parser_t    *data;

	fh = file->open (file);
	if (!fh)
	{
		return 0;
	}

	result = fh->read (fh, buffer, sizeof (buffer) - 1);
	buffer[result] = 0;
	if (result < 5)
	{
		fh->unref (fh);
		return 0;
	}
	data = toc_parser_from_data (buffer);
	fh->unref (fh); fh = 0;

	if (!data)
	{
		return 0;
	}

	disc = toc_parser_to_cdfs_disc (file, data);
	toc_parser_free (data);

	if (!disc)
	{
		return 0;
	}

	return cdfs_disc_to_dir (disc);
}


static struct ocpdir_t *test_iso (struct ocpfile_t *file)
{
	struct ocpfilehandle_t *fh;
	const char             *filename;
	enum cdfs_format_t      isofile_format;
	uint32_t                isofile_sectorcount = 0;
	struct cdfs_disc_t     *disc;

	fh = file->open (file);
	if (!fh)
	{
		return 0;
	}

	dirdbGetName_internalstr (file->dirdb_ref, &filename);

	if (detect_isofile_sectorformat (fh, filename, fh->filesize (fh), &isofile_format, &isofile_sectorcount))
	{
		debug_printf ("Unable to detect ISOFILE sector format\n");
		fh->unref (fh);
		return 0;
	}

	disc = cdfs_disc_new (file);
	if (!disc)
	{
		fprintf (stderr, "test_iso(): cdfs_disc_new() failed\n");
		return 0;
	}

	cdfs_disc_datasource_append (disc,
	                             0,                  /* sectoroffset */
	                             isofile_sectorcount,
	                             file, fh,
	                             isofile_format,
	                             0,                  /* offset */
	                             fh->filesize (fh)); /* length */

	/* track 00 */
	cdfs_disc_track_append (disc,
	                        0,  /* pregap */
	                        0,  /* offset */
	                        0,  /* sectorcount */
	                        0,  /* title */
	                        0,  /* performer */
	                        0,  /* songwriter */
	                        0,  /* composer */
	                        0,  /* arranger */
	                        0); /* message */

	/* track 01 */
	cdfs_disc_track_append (disc,
	                        0,  /* pregap */
	                        0,  /* offset */
	                        disc->datasources_data[0].sectorcount,
	                        0,  /* title */
	                        0,  /* performer */
	                        0,  /* songwriter */
	                        0,  /* composer */
	                        0,  /* arranger */
	                        0); /* message */

	return cdfs_disc_to_dir (disc);
}

static struct ocpdir_t *cdfs_check (const struct ocpdirdecompressor_t *self, struct ocpfile_t *file, const char *filetype)
{
#warning check cache here!
#if 0
	struct cdfs_disc_t *iter;

	/* Check the cache for an active instance */
	for (iter = cdfs_root; iter; iter = iter->next)
	{
		if (iter->dirs[0]->head.dirdb_ref == file->dirdb_ref)
		{
			DEBUG_PRINT ("[CDFS] found a cached entry for the given dirdb_ref => refcount the ROOT entry\n");
			iter->dirs[0]->head.ref (&iter->dirs[0]->head);
			return &iter->dirs[0]->head;
		}
	}
#endif

	if (!strcasecmp (filetype, ".iso"))
	{
		debug_printf ("[CDFS] filetype (%s) matches .iso\n", filetype);
		return test_iso (file);
	}

	if (!strcasecmp (filetype, ".cue"))
	{
		debug_printf ("[CDFS] filetype (%s) matches .cue\n", filetype);
		return test_cue (file);
	}

	if (!strcasecmp (filetype, ".toc"))
	{
		debug_printf ("[CDFS] filetype (%s) matches .toc\n", filetype);
		return test_toc (file);
	}

	return 0;
}


static struct ocpdirdecompressor_t cdfsdecompressor =
{
	"cdfs",
	"ISO9660, UDF and compact disc audio support",
	cdfs_check
};

static int cdfsint(void)
{
	UTF16BE_cd = iconv_open ("UTF-8", "UTF-16BE");
	if (UTF16BE_cd == ((iconv_t) -1))
	{
		perror ("iconv_open()");
		return 1;
	}

	register_dirdecompressor (&cdfsdecompressor);

	return errOk;
}

static void cdfsclose(void)
{
	if (UTF16BE_cd != ((iconv_t) -1))
	{
		iconv_close (UTF16BE_cd);
		UTF16BE_cd = (iconv_t) -1;
	}

	//unregister_dirdecompressor (&cdfsdecompressor);
}


#ifndef SUPPORT_STATIC_PLUGINS
char *dllinfo = "";
#endif

DLLEXTINFO_PREFIX struct linkinfostruct dllextinfo = {.name = "cdfs", .desc = "OpenCP UNIX CDFS filebrowser (c) 2022 Stian Skjelstad", .ver = DLLVERSION, .size = 0, .Init = cdfsint, .Close = cdfsclose};
