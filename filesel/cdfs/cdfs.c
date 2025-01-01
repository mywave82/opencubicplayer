/* OpenCP Module Player
 * copyright (c) 2022-'25 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * FILE-IO handling for CDFS
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
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "types.h"

#include "cdfs.h"
#include "filesel/cdrom.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/mdb.h"
#include "filesel/musicbrainz.h"
#include "iso9660.h"
#include "udf.h"

#ifdef CDFS_DEBUG
#define debug_printf(...) fprintf (stderr, __VA_ARGS__)
#else
#define debug_printf(format,args...) ((void)0)
#endif

void cdfs_disc_ref (struct cdfs_disc_t *self);
void cdfs_disc_unref (struct cdfs_disc_t *self);

static void cdfs_dir_ref (struct ocpdir_t *);
static void cdfs_dir_unref (struct ocpdir_t *);

static ocpdirhandle_pt cdfs_dir_readdir_start (struct ocpdir_t *, void(*callback_file)(void *token, struct ocpfile_t *),
                                                                 void(*callback_dir )(void *token, struct ocpdir_t *), void *token);
static ocpdirhandle_pt cdfs_dir_readflatdir_start (struct ocpdir_t *, void(*callback_file)(void *token, struct ocpfile_t *), void *token);
static void cdfs_dir_readdir_cancel (ocpdirhandle_pt);
static int cdfs_dir_readdir_iterate (ocpdirhandle_pt);
static struct ocpdir_t *cdfs_dir_readdir_dir (struct ocpdir_t *_self, uint32_t dirdb_ref);
static struct ocpfile_t *cdfs_dir_readdir_file (struct ocpdir_t *_self, uint32_t dirdb_ref);

static void cdfs_file_ref (struct ocpfile_t *);
static void cdfs_file_unref (struct ocpfile_t *);
static struct ocpfilehandle_t *cdfs_file_open (struct ocpfile_t *);
static uint64_t cdfs_file_filesize (struct ocpfile_t *);
static int cdfs_file_filesize_ready (struct ocpfile_t *);
static const char *cdfs_file_filename_override (struct ocpfile_t *);
static struct ocpfilehandle_t *cdfs_audio_open (struct ocpfile_t *);

static void cdfs_filehandle_ref (struct ocpfilehandle_t *);
static void cdfs_filehandle_unref (struct ocpfilehandle_t *);
static int cdfs_filehandle_seek_set (struct ocpfilehandle_t *, int64_t pos);
static uint64_t cdfs_filehandle_getpos (struct ocpfilehandle_t *);
static int cdfs_filehandle_eof (struct ocpfilehandle_t *);
static int cdfs_filehandle_error (struct ocpfilehandle_t *);
static int cdfs_filehandle_read (struct ocpfilehandle_t *, void *dst, int len);
static uint64_t cdfs_filehandle_filesize (struct ocpfilehandle_t *);
static int cdfs_filehandle_filesize_ready (struct ocpfilehandle_t *);
static const char *cdfs_filehandle_filename_override (struct ocpfilehandle_t *);
static int cdfs_filehandle_audio_read (struct ocpfilehandle_t *_handle, void *dst, int len);
static int cdfs_filehandle_audio_ioctl (struct ocpfilehandle_t *_self, const char *cmd, void *ptr);

struct cdfs_instance_filehandle_t
{
	struct ocpfilehandle_t       head;
	struct cdfs_instance_file_t *file;
	int                          error;
	uint64_t                     filepos;

	unsigned char buffer[2048];
	int curextent;       /* which extent is current in the buffer */
	uint32_t cursector;  /* which sector-offset into the buffer */
	uint64_t curoffset;  /* absolute filepos of the buffer */
	int cursectorskip;   /* should we skip bytes at the start of the buffer? (UDF-inline) */
	int cursectorsize;   /* the data-size of the current sector (sectorskip is taken into account) */
};

OCP_INTERNAL int
detect_isofile_sectorformat (struct ocpfilehandle_t *isofile_fh,
                             const char *filename,
                             off_t st_size,
                             enum cdfs_format_t *isofile_format,
                             uint32_t *isofile_sectorcount)
{
	uint8_t buffer[6+8+4+12];

	/* Detect MODE1 / XA_MODE2_FORM1 - 2048 bytes of data */
	if (isofile_fh->seek_set (isofile_fh, SECTORSIZE * 16) < 0)
	{
		debug_printf ("Failed to seek to SECTORSIZE * 16\n");
		return 1;
	}
	if (isofile_fh->read (isofile_fh, buffer, 6) != 6)
	{
		debug_printf ("Failed to read first 6 bytes at offset SECTORSIZE * 16\n");
		return 1;
	}
	if (((buffer[1] == 'C') &&
	     (buffer[2] == 'D') &&
	     (buffer[3] == '0') &&
	     (buffer[4] == '0') &&
	     (buffer[5] == '1')) ||
	    ((buffer[1] == 'B') &&
	     (buffer[2] == 'E') &&
	     (buffer[3] == 'A') &&
	     (buffer[4] == '0') &&
	     (buffer[5] == '1')))
	{
		debug_printf ("%s: detected as ISO file format, containing only data as is\n", filename);
		*isofile_format = FORMAT_MODE_1__XA_MODE2_FORM1___NONE;
		*isofile_sectorcount = st_size / 2048;
		return 0;
	}

	/* Detect FORMAT_XA1_MODE2_FORM1___NONE */
	if (isofile_fh->seek_set (isofile_fh, (SECTORSIZE + 8) * 16) < 0)
	{
		debug_printf ("Failed to seek to (SECTORSIZE + 8) * 16 // FORMAT_XA1_DATA_2048\n");
		return 1;
	}

	if (isofile_fh->read (isofile_fh, buffer, (6+8)) != (6+8))
	{
		debug_printf ("Failed to read first 6 + 8 bytes at offset (SECTORSIZE + 8) * 16\n");
		return 1;
	}
	if ((!(buffer[2] & 0x20)) && // XA_SUBH_DATA - copy1
	    (!(buffer[6] & 0x20)) && // XA_SUBH_DATA - copy2
	    (((buffer[8+1] == 'C') &&
	      (buffer[8+2] == 'D') &&
	      (buffer[8+3] == '0') &&
	      (buffer[8+4] == '0') &&
	      (buffer[8+5] == '1')) ||
	     ((buffer[8+1] == 'B') &&
	      (buffer[8+2] == 'E') &&
	      (buffer[8+3] == 'A') &&
	      (buffer[8+4] == '0') &&
	      (buffer[8+5] == '1'))))
	{
		debug_printf ("%s: detected as ISO file format, each sector prefixed with XA1 header (8 bytes)\n", filename);
		*isofile_format = FORMAT_XA1_MODE2_FORM1___NONE;
		*isofile_sectorcount = st_size / (2048 + 8);
		return 0;
	}

	/* Detect FORMAT_RAW___NONE MODE-1 / MODE-2 FORM-1 */
	if (isofile_fh->seek_set (isofile_fh, SECTORSIZE_XA2 * 16) < 0)
	{
		debug_printf ("Failed to seek to SECTORSIZE_XA2 * 16\n");
		return 1;
	}

	if (isofile_fh->read (isofile_fh, buffer, (6+12+4+8)) != (6+12+4+8))
	{
		debug_printf ("Failed to read first 6 + 12 + 4 + 8 bytes at offset SECTORSIZE_XA2 * 16\n");
		return 1;
	}
	if ((buffer[ 0] == 0x00) && // SYNC
	    (buffer[ 1] == 0xff) && // SYNC
	    (buffer[ 2] == 0xff) && // SYNC
	    (buffer[ 3] == 0xff) && // SYNC
	    (buffer[ 4] == 0xff) && // SYNC
	    (buffer[ 5] == 0xff) && // SYNC
	    (buffer[ 6] == 0xff) && // SYNC
	    (buffer[ 7] == 0xff) && // SYNC
	    (buffer[ 8] == 0xff) && // SYNC
	    (buffer[ 9] == 0xff) && // SYNC
	    (buffer[10] == 0xff) && // SYNC
	    (buffer[11] == 0x00)) // SYNC
	{
		if (buffer[12+3] == 1) // MODE 1
		{
			if (((buffer[12+4+1] =='C') &&
			     (buffer[12+4+2] =='D') &&
			     (buffer[12+4+3] =='0') &&
			     (buffer[12+4+4] =='0') &&
			     (buffer[12+4+5] =='1')) ||
			    ((buffer[12+4+1] =='B') &&
			     (buffer[12+4+2] =='E') &&
			     (buffer[12+4+3] =='A') &&
			     (buffer[12+4+4] =='0') &&
			     (buffer[12+4+5] =='1')))
			{
				debug_printf ("%s: detected as ISO file format, each sector prefixed with SYNC and MODE 1 header\n", filename);
				*isofile_format = FORMAT_MODE1_RAW___NONE;
				*isofile_sectorcount = st_size / (SECTORSIZE_XA2);
				return 0;
			}
		} else if (buffer[12+3] == 2)
		{
			// MODE 2
			 if (((buffer[12+4+1] == 'C') &&
			      (buffer[12+4+2] == 'D') &&
			      (buffer[12+4+3] == '0') &&
			      (buffer[12+4+4] == '0') &&
			      (buffer[12+4+5] == '1')) ||
			     ((buffer[12+4+1] == 'B') &&
			      (buffer[12+4+2] == 'E') &&
			      (buffer[12+4+3] == 'A') &&
			      (buffer[12+4+4] == '0') &&
			      (buffer[12+4+5] == '1')))
			{
				debug_printf ("%s: detected as ISO file format, each sector prefixed with SYNC and MODE 2\n", filename);
				*isofile_format = FORMAT_MODE2_RAW___NONE;
				*isofile_sectorcount = st_size / (SECTORSIZE_XA2);
				return 0;
			}
			// MODE 2 XA FORM 1
			if ((!(buffer[12+4+2] & 0x20)) && // XA_SUBH_DATA - copy1
			    (!(buffer[12+4+6] & 0x20)) && // XA_SUBH_DATA - copy2
			    (((buffer[12+4+8+1] == 'C') &&
			      (buffer[12+4+8+2] == 'D') &&
			      (buffer[12+4+8+3] == '0') &&
			      (buffer[12+4+8+4] == '0') &&
			      (buffer[12+4+8+5] == '1')) ||
			     ((buffer[12+4+8+1] == 'B') &&
			      (buffer[12+4+8+2] == 'E') &&
			      (buffer[12+4+8+3] == 'A') &&
			      (buffer[12+4+8+4] == '0') &&
			      (buffer[12+4+8+5] == '1'))))
			{
				debug_printf ("%s: detected as ISO file format, each sector prefixed with SYNC and MODE 2 FORM 1 header\n", filename);
				*isofile_format = FORMAT_XA_MODE2_RAW;
				*isofile_sectorcount = st_size / (SECTORSIZE_XA2);
				return 0;
			}
		}
	}

	/* Detect FORMAT_RAW___RAW_RW / FORMAT_RAW___RW */
	if (isofile_fh->seek_set (isofile_fh, (SECTORSIZE_XA2 + 96) * 16) < 0)
	{
		debug_printf ("Failed to seek to (SECTORSIZE_XA2 + 96) * 16\n");
		return 1;
	}
	if (isofile_fh->read (isofile_fh, buffer, (4 + 12 + 4 + 8)) != (6 + 12 + 4 + 8))
	{
		debug_printf ("Failed to read first 6 + 12 + 4 + 8 bytes at offset (SECTORSIZE_XA2 + 96) * 16\n");
		return 1;
	}
	if ((buffer[ 0] == 0x00) && // SYNC
	    (buffer[ 1] == 0xff) && // SYNC
	    (buffer[ 2] == 0xff) && // SYNC
	    (buffer[ 3] == 0xff) && // SYNC
	    (buffer[ 4] == 0xff) && // SYNC
	    (buffer[ 5] == 0xff) && // SYNC
	    (buffer[ 6] == 0xff) && // SYNC
	    (buffer[ 7] == 0xff) && // SYNC
	    (buffer[ 8] == 0xff) && // SYNC
	    (buffer[ 9] == 0xff) && // SYNC
	    (buffer[10] == 0xff) && // SYNC
	    (buffer[11] == 0x00)) // SYNC
	{
		if (buffer[12+3] == 1) // MODE 1
		{
			if (((buffer[12+4+1] == 'C') &&
			     (buffer[12+4+2] == 'D') &&
			     (buffer[12+4+3] == '0') &&
			     (buffer[12+4+4] == '0') &&
			     (buffer[12+4+5] == '1')) ||
			    ((buffer[12+4+1] == 'B') &&
			     (buffer[12+4+2] == 'E') &&
			     (buffer[12+4+3] == 'A') &&
			     (buffer[12+4+4] == '0') &&
			     (buffer[12+4+5] == '1')))
			{
				debug_printf ("%s: detected as ISO file format, each sector prefixed with SYNC and MODE 1 header, and suffixed with SUBCHANNEL R-W\n", filename);
				*isofile_format = FORMAT_MODE1_RAW___RAW_RW;
				*isofile_sectorcount = st_size / (SECTORSIZE_XA2 + 96);
				return 0;
			}
		} else if (buffer[12+3] == 2) // MODE 2
		{
			// MODE 2
			 if (((buffer[12+4+1] == 'C') &&
			      (buffer[12+4+2] == 'D') &&
			      (buffer[12+4+3] == '0') &&
			      (buffer[12+4+4] == '0') &&
			      (buffer[12+4+5] == '1')) ||
			     ((buffer[12+4+1] == 'B') &&
			      (buffer[12+4+2] == 'E') &&
			      (buffer[12+4+3] == 'A') &&
			      (buffer[12+4+4] == '0') &&
			      (buffer[12+4+5] == '1')))
			{
				debug_printf ("%s: detected as ISO file format, each sector prefixed with SYNC and MODE 2, and suffixed with SUBCHANNEL R-W\n", filename);
				*isofile_format = FORMAT_MODE2_RAW___RAW_RW;
				*isofile_sectorcount = st_size / (SECTORSIZE_XA2);
				return 0;
			}
			// MODE 2 XA FORM 1
			if ((!(buffer[12+4+2] & 0x20)) && // XA_SUBH_DATA - copy1
			    (!(buffer[12+4+6] & 0x20)) && // XA_SUBH_DATA - copy2
			    (((buffer[12+4+8+1] == 'C') &&
			      (buffer[12+4+8+2] == 'D') &&
			      (buffer[12+4+8+3] == '0') &&
			      (buffer[12+4+8+4] == '0') &&
			      (buffer[12+4+8+5] == '1')) ||
			     ((buffer[12+4+8+1] == 'B') &&
			      (buffer[12+4+8+2] == 'E') &&
			      (buffer[12+4+8+3] == 'A') &&
			      (buffer[12+4+8+4] == '0') &&
			      (buffer[12+4+8+5] == '1'))))
			{
				debug_printf ("%s: detected as ISO file format, each sector prefixed with SYNC and MODE 2 FORM 1 header, and suffixed with SUBCHANNEL R-W\n", filename);
				*isofile_format = FORMAT_XA_MODE2_RAW___RAW_RW;
				*isofile_sectorcount = st_size / (SECTORSIZE_XA2 + 96);
				return 0;
			}
		}
	}

	return 1;
}

OCP_INTERNAL int cdfs_fetch_absolute_sector_2048 (struct cdfs_disc_t *disc, uint32_t sector, uint8_t *buffer) /* 2048 byte modes */
{
	int i;
	uint8_t xbuffer[16];
	int subchannel = 0;

	for (i=0; i < disc->datasources_count; i++)
	{
		if (  (disc->datasources_data[i].sectoroffset <= sector) &&
		     ((disc->datasources_data[i].sectoroffset + disc->datasources_data[i].sectorcount) > sector))
		{
			uint32_t relsector = sector - disc->datasources_data[i].sectoroffset;
			if (!disc->datasources_data[i].fh)
			{
				memset (buffer, 0, 2048);
				return 0;
			}

			switch (disc->datasources_data[i].format)
			{
				case FORMAT_AUDIO_SWAP___RAW_RW:
				case FORMAT_AUDIO_SWAP___RW:
				case FORMAT_AUDIO___RAW_RW: /* we do not swap endian on 2048 byte fetches */
				case FORMAT_AUDIO___RW: /* we do not swap endian on 2048 byte fetches */
				case FORMAT_MODE1_RAW___RAW_RW:
				case FORMAT_MODE1_RAW___RW:
				case FORMAT_MODE2_RAW___RAW_RW:
				case FORMAT_MODE2_RAW___RW:
				case FORMAT_XA_MODE2_RAW___RAW_RW:
				case FORMAT_XA_MODE2_RAW___RW:
				case FORMAT_RAW___RAW_RW:
				case FORMAT_RAW___RW:
					subchannel = 96;
					/* fall-through */
				case FORMAT_RAW___NONE:
				case FORMAT_AUDIO___NONE:
				case FORMAT_AUDIO_SWAP___NONE: /* we do not swap endian on 2048 byte fetches */
				case FORMAT_MODE1_RAW___NONE:
				case FORMAT_MODE2_RAW___NONE:
				case FORMAT_XA_MODE2_RAW:
					if (disc->datasources_data[i].fh->seek_set (disc->datasources_data[i].fh, disc->datasources_data[i].offset + ((uint64_t)relsector)*(SECTORSIZE_XA2 + subchannel)) < 0)
					{
						debug_printf ("seek((%"PRId32")*(SECTORSIZE_XA2+%d), SEEK_SET) failed\n", relsector, subchannel);
						return -1;
					}

					if (disc->datasources_data[i].fh->read(disc->datasources_data[i].fh, xbuffer, 16) != 16)
					{
						debug_printf ("read(xbuffer, 16) failed\n");
						return -1;
					}
					if (memcmp (xbuffer, "\x00\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00", 12))
					{
						debug_printf ("Invalid sync in sector %"PRId32"\n", relsector);
						return -1;
					}
					// xbuffer[12, 13 and 14] should be the current sector adress
					switch (xbuffer[15])
					{
						case 0x00: /* CLEAR */
							debug_printf ("Sector %"PRId32" is CLEAR\n", sector);
							return -1;
						case 0x01: /* MODE 1: DATA */
							if (disc->datasources_data[i].fh->read(disc->datasources_data[i].fh, buffer, SECTORSIZE) != SECTORSIZE)
							{
								debug_printf ("read(buffer, SECTORSIZE) failed\n");
								return -1;
							}
							return 0;
						case 0xe2: /* Seems to be second to last sector on CD-R, mode-2 */
						case 0x02: /* MODE 2 */
							/* assuming XA-FORM-1, that is the only mode2 that can provide 2048 bytes of data */
#warning ignoring sub-header in FORMAT_XA_MODE2_RAW for now..
							if (disc->datasources_data[i].fh->read(disc->datasources_data[i].fh, xbuffer, 8) != 8)
							{
								debug_printf ("read(xbuffer_subheader, 8) failed\n");
								return -1;
							}
							if (disc->datasources_data[i].fh->read(disc->datasources_data[i].fh, buffer, SECTORSIZE) != SECTORSIZE)
							{
								debug_printf ("read(buffer, SECTORSIZE) failed\n");
								return -1;
							}
							return 0;
						default:
							debug_printf ("Sector %"PRId32" is of unknown type (0x%02x)\n", sector, xbuffer[15]);
							return -1;
					}
					return -1; /* not reachable */

				case FORMAT_XA_MODE2_FORM_MIX___RAW_RW: /* not tested */
				case FORMAT_XA_MODE2_FORM_MIX___RW: /* not tested */
					subchannel = 96;
					/* fall-through */
				case FORMAT_XA_MODE2_FORM_MIX___NONE: /* not tested */
					if (disc->datasources_data[i].fh->seek_set(disc->datasources_data[i].fh, disc->datasources_data[i].offset + ((uint64_t)relsector)*(2324 + 8 + subchannel)) < 0)
					{
						debug_printf ("seek((%"PRId32")*(2324 + 8 + %d)), SEEK_SET) failed\n", relsector, subchannel);
						return -1;
					}

					if (disc->datasources_data[i].fh->read(disc->datasources_data[i].fh, xbuffer, 8) != 8)
					{
						debug_printf ("read(xbuffer, 8) failed\n");
						return -1;
					}
#warning ignoring sub-header in FORMAT_XA_MODE2_FORM_MIX for now..
					if (disc->datasources_data[i].fh->read(disc->datasources_data[i].fh, xbuffer, 8) != 8)
					{
						debug_printf ("read(xbuffer_subheader, 8) failed\n");
						return -1;
					}
					if (disc->datasources_data[i].fh->read(disc->datasources_data[i].fh, buffer, SECTORSIZE) != SECTORSIZE)
					{
						debug_printf ("read(buffer, SECTORSIZE) failed\n");
						return -1;
					}
					return 0;

				case FORMAT_MODE1___RAW_RW:
				case FORMAT_MODE1___RW:
				case FORMAT_XA_MODE2_FORM1___RAW_RW:
				case FORMAT_XA_MODE2_FORM1___RW:
				case FORMAT_MODE_1__XA_MODE2_FORM1___RAW_RW:
				case FORMAT_MODE_1__XA_MODE2_FORM1___RW:
					subchannel = 96;
					/* fall-through */
				case FORMAT_MODE1___NONE:
				case FORMAT_XA_MODE2_FORM1___NONE:
				case FORMAT_MODE_1__XA_MODE2_FORM1___NONE:
					if (disc->datasources_data[i].fh->seek_set(disc->datasources_data[i].fh, disc->datasources_data[i].offset + ((uint64_t)relsector)*(SECTORSIZE + subchannel)) < 0)
					{
						debug_printf ("seek((%"PRId32")*(SECTORSIZE + %d)), SEEK_SET) failed\n", relsector, subchannel);
						return -1;
					}
					if (disc->datasources_data[i].fh->read(disc->datasources_data[i].fh, buffer, SECTORSIZE) != SECTORSIZE)
					{
						debug_printf ("read(buffer, SECTORSIZE) failed\n");
						return -1;
					}
					return 0;

				case FORMAT_XA1_MODE2_FORM1___RW:
				case FORMAT_XA1_MODE2_FORM1___RW_RAW:
					subchannel = 96;
					/* fall-through */
				case FORMAT_XA1_MODE2_FORM1___NONE:

					if (disc->datasources_data[i].fh->seek_set(disc->datasources_data[i].fh, disc->datasources_data[i].offset + ((uint64_t)relsector)*(SECTORSIZE + 8 + subchannel) + 8) < 0)
					{
						debug_printf ("seek((%"PRId32")*(SECTORSIZE + 8 + %d) + 8), SEEK_SET) failed\n", relsector, subchannel);
						return -1;
					}
					if (disc->datasources_data[i].fh->read(disc->datasources_data[i].fh, buffer, SECTORSIZE) != SECTORSIZE)
					{
						debug_printf ("read(buffer, SECTORSIZE) failed\n");
						return -1;
					}
					return 0;

				case FORMAT_MODE2___NONE:
				case FORMAT_MODE2___RAW_RW:
				case FORMAT_MODE2___RW:
					debug_printf ("Sector %"PRIu32" does not contain 2048 bytes of data, but 2336\n", sector);
					return 1;

				case FORMAT_XA_MODE2_FORM2___NONE:
				case FORMAT_XA_MODE2_FORM2___RAW_RW:
				case FORMAT_XA_MODE2_FORM2___RW:
					debug_printf ("Sector %"PRIu32" does not contain 2048 bytes of data, but 2324\n", sector);
					return 1;

				default:
					debug_printf ("Unable to fetch absolute sector %" PRId32 "\n", sector);
					return 1;
			}
		}
	}
	debug_printf ("Unable to locate absolute sector %" PRId32 "\n", sector);
	return 1;
}

OCP_INTERNAL int cdfs_fetch_absolute_sector_2352 (struct cdfs_disc_t *disc, uint32_t sector, uint8_t *buffer)
{
	int i;
	int subchannel = 0;

	for (i=0; i < disc->datasources_count; i++)
	{
		if (  (disc->datasources_data[i].sectoroffset <= sector) &&
		     ((disc->datasources_data[i].sectoroffset + disc->datasources_data[i].sectorcount) > sector))
		{
			uint32_t relsector = sector - disc->datasources_data[i].sectoroffset;

			if (!disc->datasources_data[i].fh)
			{
				memset (buffer, 0, 2352);
				return 0;
			}

			switch (disc->datasources_data[i].format)
			{
				case FORMAT_AUDIO_SWAP___RAW_RW:
				case FORMAT_AUDIO_SWAP___RW:
					subchannel = 96;
					/* fall-through */
				case FORMAT_AUDIO_SWAP___NONE: /* we do not swap endian on 2048 byte fetches */
					if (disc->datasources_data[i].fh->seek_set (disc->datasources_data[i].fh, disc->datasources_data[i].offset + ((uint64_t)relsector)*(SECTORSIZE_XA2 + subchannel)) < 0)
					{
						debug_printf ("seek((%"PRId32")*(SECTORSIZE_XA2+%d), SEEK_SET) failed\n", relsector, subchannel);
						return -1;
					}

					if (disc->datasources_data[i].fh->read(disc->datasources_data[i].fh, buffer, SECTORSIZE_XA2) != SECTORSIZE_XA2)
					{
						debug_printf ("read(buffer, SECTORSIZE) failed\n");
						return -1;
					}
					return 0;

				case FORMAT_AUDIO___RAW_RW: /* we do not swap endian on 2048 byte fetches */
				case FORMAT_AUDIO___RW: /* we do not swap endian on 2048 byte fetches */
				case FORMAT_MODE1_RAW___RAW_RW:
				case FORMAT_MODE1_RAW___RW:
				case FORMAT_MODE2_RAW___RAW_RW:
				case FORMAT_MODE2_RAW___RW:
				case FORMAT_XA_MODE2_RAW___RAW_RW:
				case FORMAT_XA_MODE2_RAW___RW:
				case FORMAT_RAW___RAW_RW:
				case FORMAT_RAW___RW:
					subchannel = 96;
					/* fall-through */
				case FORMAT_RAW___NONE:
				case FORMAT_AUDIO___NONE:
				case FORMAT_MODE1_RAW___NONE:
				case FORMAT_MODE2_RAW___NONE:
				case FORMAT_XA_MODE2_RAW:
					if (disc->datasources_data[i].fh->seek_set (disc->datasources_data[i].fh, disc->datasources_data[i].offset + ((uint64_t)relsector)*(SECTORSIZE_XA2 + subchannel)) < 0)
					{
						debug_printf ("seek((%"PRId32")*(SECTORSIZE_XA2+%d), SEEK_SET) failed\n", relsector, subchannel);
						return -1;
					}

					if (disc->datasources_data[i].fh->read(disc->datasources_data[i].fh, buffer, SECTORSIZE_XA2) != SECTORSIZE_XA2)
					{
						debug_printf ("read(buffer, SECTORSIZE) failed\n");
						return -1;
					}

					{ /* CDA is big-endian */
						int i;
						uint16_t *a = (uint16_t *)buffer;
						for (i=0; i < 1176; i++)
						{
							*a = uint16_big (*a);
							a++;
						}
					}

					return 0;

				case FORMAT_XA_MODE2_FORM_MIX___RAW_RW: /* not tested */
				case FORMAT_XA_MODE2_FORM_MIX___RW: /* not tested */
					subchannel = 96;
					/* fall-through */
				case FORMAT_XA_MODE2_FORM_MIX___NONE: /* not tested */
					return -1;

				case FORMAT_MODE1___RAW_RW:
				case FORMAT_MODE1___RW:
				case FORMAT_XA_MODE2_FORM1___RAW_RW:
				case FORMAT_XA_MODE2_FORM1___RW:
				case FORMAT_MODE_1__XA_MODE2_FORM1___RAW_RW:
				case FORMAT_MODE_1__XA_MODE2_FORM1___RW:
					subchannel = 96;
					/* fall-through */
				case FORMAT_MODE1___NONE:
				case FORMAT_XA_MODE2_FORM1___NONE:
				case FORMAT_MODE_1__XA_MODE2_FORM1___NONE:
					return -1;

				case FORMAT_XA1_MODE2_FORM1___RW:
				case FORMAT_XA1_MODE2_FORM1___RW_RAW:
					subchannel = 96;
					/* fall-through */
				case FORMAT_XA1_MODE2_FORM1___NONE:
					return -1;

				case FORMAT_MODE2___NONE:
				case FORMAT_MODE2___RAW_RW:
				case FORMAT_MODE2___RW:
					return 1;

				case FORMAT_XA_MODE2_FORM2___NONE:
				case FORMAT_XA_MODE2_FORM2___RAW_RW:
				case FORMAT_XA_MODE2_FORM2___RW:
					return 1;

				default:
					debug_printf ("Unable to fetch absolute sector %" PRId32 "\n", sector);
					return 1;
			}
		}
	}
	debug_printf ("Unable to locate absolute sector %" PRId32 "\n", sector);
	return 1;

}

OCP_INTERNAL void
cdfs_disc_datasource_append (struct cdfs_disc_t     *disc,
                             uint32_t                sectoroffset,
                             uint32_t                sectorcount,
                             struct ocpfile_t       *file,
                             struct ocpfilehandle_t *fh,
                             enum cdfs_format_t      format,
                             uint64_t                offset,
                             uint64_t                length)
{
	struct cdfs_datasource_t *temp;

	// append to previous datasource if possible
	if ( disc->datasources_count && // there is an entry there already
	     ((disc->datasources_data[disc->datasources_count-1].sectoroffset + disc->datasources_data[disc->datasources_count-1].sectorcount) == sectoroffset) &&
	     ((!!disc->datasources_data[disc->datasources_count-1].fh) == (!!fh)) && // both entries are either both files or zero-fills
	     ((!fh) || (disc->datasources_data[disc->datasources_count-1].fh->dirdb_ref == fh->dirdb_ref)) && // if entries are files, filenames matches
	     (disc->datasources_data[disc->datasources_count-1].format == format) &&
	     ((disc->datasources_data[disc->datasources_count-1].offset + disc->datasources_data[disc->datasources_count-1].length) == offset) )
	{
		disc->datasources_data[disc->datasources_count-1].sectorcount += sectorcount;
		disc->datasources_data[disc->datasources_count-1].length += length;
		return;
	}

	temp = realloc (disc->datasources_data, sizeof (disc->datasources_data[0]) * (disc->datasources_count + 1));
	if (!temp)
	{
		fprintf (stderr, "cdfs_disc_datasource_append() realloc failed\n");
		return;
	}
	disc->datasources_data = temp;
	disc->datasources_data[disc->datasources_count].sectoroffset = sectoroffset;
	disc->datasources_data[disc->datasources_count].sectorcount = sectorcount;
	disc->datasources_data[disc->datasources_count].file = file;
	if (file)
	{
		file->ref (file);
	}
	disc->datasources_data[disc->datasources_count].fh = fh;
	if (fh)
	{
		fh->ref (fh);
	}
	disc->datasources_data[disc->datasources_count].format = format;
	disc->datasources_data[disc->datasources_count].offset = offset;
	disc->datasources_data[disc->datasources_count].length = length;
	disc->datasources_count++;
}

OCP_INTERNAL void
cdfs_disc_track_append (struct cdfs_disc_t *disc,
                        uint32_t            pregap,
                        uint32_t            start,
                        uint32_t            length,
                        const char         *title,
                        const char         *performer,
                        const char         *songwriter,
                        const char         *composer,
                        const char         *arranger,
                        const char         *message)
{
	if (disc->tracks_count >= 100)
	{
		fprintf (stderr, "cdfs_disc_track_append() too many tracks\n");
		return;
	}

	disc->tracks_data[disc->tracks_count].pregap = pregap;
	disc->tracks_data[disc->tracks_count].start  = start;
	disc->tracks_data[disc->tracks_count].length = length;
	disc->tracks_data[disc->tracks_count].title      = title      ? strdup (title)      : 0;
	disc->tracks_data[disc->tracks_count].performer  = performer  ? strdup (performer)  : 0;
	disc->tracks_data[disc->tracks_count].songwriter = songwriter ? strdup (songwriter) : 0;
	disc->tracks_data[disc->tracks_count].composer   = composer   ? strdup (composer)   : 0;
	disc->tracks_data[disc->tracks_count].arranger   = arranger   ? strdup (arranger)   : 0;
	disc->tracks_data[disc->tracks_count].message    = message    ? strdup (message)    : 0;

	disc->tracks_count++;
}

OCP_INTERNAL enum cdfs_format_t cdfs_get_sector_format (struct cdfs_disc_t *disc, uint32_t sector)
{
	int i;
	for (i=0; i < disc->datasources_count; i++)
	{
		if ( (disc->datasources_data[i].sectoroffset <= sector) &&
		    ((disc->datasources_data[i].sectoroffset + disc->datasources_data[i].sectorcount) > sector))
		{
			return disc->datasources_data[i].format;
		}
	}
	return FORMAT_ERROR;
}

OCP_INTERNAL struct cdfs_disc_t *cdfs_disc_new (struct ocpfile_t *file)
{
	struct cdfs_disc_t *disc;

	disc = calloc (sizeof (*disc), 1);
	if (!disc)
	{
		fprintf (stderr, "cdfs_disc_new() calloc() failed\n");
		return 0;
	}

	disc->dir_size = 16;
	disc->dirs = malloc (disc->dir_size * sizeof (disc->dirs[0]));

	debug_printf ("[CDFS] creating a DIR using the same parent dirdb_ref\n");

	dirdbRef (file->dirdb_ref, dirdb_use_dir);
	disc->dirs[0] = &disc->dir0;
	ocpdir_t_fill (&disc->dirs[0]->head,
	                cdfs_dir_ref,
	                cdfs_dir_unref,
	                file->parent,
	                cdfs_dir_readdir_start,
	                cdfs_dir_readflatdir_start,
	                cdfs_dir_readdir_cancel,
	                cdfs_dir_readdir_iterate,
	                cdfs_dir_readdir_dir,
	                cdfs_dir_readdir_file,
	                0, /* charset_API */
	                file->dirdb_ref,
	                0, /* refcount */
	                1, /* is_archive */
	                0, /* is_playlist */
	                file->compression);

	file->parent->ref (file->parent);
	disc->dirs[0]->owner = disc;
	disc->dirs[0]->dir_parent = UINT32_MAX;
	disc->dirs[0]->dir_next = UINT32_MAX;
	disc->dirs[0]->dir_child = UINT32_MAX;
	disc->dirs[0]->file_child = UINT32_MAX;
	//disc->dirs[0]->orig_full_dirpath = 0;
	disc->dir_fill = 1;
	disc->refcount = 0;
	//file->ref (file);
	//disc->archive_file = file;

#warning insert into CACHE here
	//disc->next = tar_root;
	//cdfs_root = disc;

	disc->dirs[0]->head.ref (&disc->dirs[0]->head);

	return disc;
}

static void cdfs_disc_free (struct cdfs_disc_t *disc)
{
	int i;

	debug_printf ("  cdfs_disc_free()\n");

	if (disc->musicbrainzhandle)
	{
		musicbrainz_lookup_discid_cancel (disc->musicbrainzhandle);
		disc->musicbrainzhandle = 0;
	}
	if (disc->musicbrainzdata)
	{
		musicbrainz_database_h_free (disc->musicbrainzdata);
		disc->musicbrainzdata = 0;
	}
	free (disc->discid);
	free (disc->toc);

	if (disc->iso9660_session)
	{
		ISO9660_Session_Free (&disc->iso9660_session);
	}
	if (disc->udf_session)
	{
		UDF_Session_Free (disc);
	}

	for (i=0; i < disc->dir_fill; i++)
	{
		dirdbUnref(disc->dirs[i]->head.dirdb_ref, dirdb_use_dir);
	}
	for (i=1; i < disc->dir_fill; i++)
	{
		free (disc->dirs[i]);
	}
	for (i=0; i < disc->file_fill; i++)
	{
		dirdbUnref(disc->files[i]->head.dirdb_ref, dirdb_use_file);
		if (disc->files[i]->extents)
		{
			free (disc->files[i]->extent);
		}
		free (disc->files[i]->filenameshort);
		free (disc->files[i]);
	}
	free (disc->files);
	free (disc->dirs);

	for (i=0; i < disc->datasources_count; i++)
	{
		if (disc->datasources_data[i].file)
		{
			disc->datasources_data[i].file->unref (disc->datasources_data[i].file);
		}
		if (disc->datasources_data[i].fh)
		{
			disc->datasources_data[i].fh->unref (disc->datasources_data[i].fh);
		}
	}
	free (disc->datasources_data);

	for (i=0; i < 100; i++)
	{
		free (disc->tracks_data[i].title);
		free (disc->tracks_data[i].performer);
		free (disc->tracks_data[i].songwriter);
		free (disc->tracks_data[i].composer);
		free (disc->tracks_data[i].arranger);
		free (disc->tracks_data[i].message);
	}
	free (disc);
}

/* returns 0 on errors */
OCP_INTERNAL uint32_t
CDFS_Directory_add (struct cdfs_disc_t *self, const uint32_t dir_parent_handle, const char *Dirname)
{
	uint32_t *prev, iter;
	uint32_t dirdb_ref;

	debug_printf ("[CDFS] create_dir: %s\n", Dirname);

	dirdb_ref = dirdbFindAndRef (self->dirs[dir_parent_handle]->head.dirdb_ref, Dirname, dirdb_use_dir);

	if (self->dir_fill == self->dir_size)
	{
		int size = self->dir_size + 16;
		struct cdfs_instance_dir_t **dirs = realloc (self->dirs, size * sizeof (self->dirs[0]));

		if (!dirs)
		{ /* out of memory */
			dirdbUnref (dirdb_ref, dirdb_use_dir);
			return 0;
		}

		self->dirs = dirs;
		self->dir_size = size;
	}

	self->dirs[self->dir_fill] = malloc (sizeof(self->dirs[self->dir_fill][0]));
	if (!self->dirs[self->dir_fill])
	{ /* out of memory */
		dirdbUnref (dirdb_ref, dirdb_use_dir);
		return 0;
	}

	ocpdir_t_fill (&self->dirs[self->dir_fill]->head,
	                cdfs_dir_ref,
	                cdfs_dir_unref,
	               &self->dirs[dir_parent_handle]->head,
	                cdfs_dir_readdir_start,
	                cdfs_dir_readflatdir_start,
	                cdfs_dir_readdir_cancel,
	                cdfs_dir_readdir_iterate,
	                cdfs_dir_readdir_dir,
	                cdfs_dir_readdir_file,
	                0, /* charset_API */
	                dirdb_ref,
	                0, /* refcount */
	                1, /* is_archive */
	                0, /* is_playlist */
	                self->dirs[0]->head.compression);

	self->dirs[self->dir_fill]->owner = self;
	self->dirs[self->dir_fill]->dir_parent = dir_parent_handle;
	self->dirs[self->dir_fill]->dir_next = UINT32_MAX;
	self->dirs[self->dir_fill]->dir_child = UINT32_MAX;
	self->dirs[self->dir_fill]->file_child = UINT32_MAX;
	//self->dirs[self->dir_fill]->orig_full_dirpath = strdup (Dirpath);

	prev = &self->dirs[dir_parent_handle]->dir_child;
	for (iter = *prev; iter != UINT32_MAX; iter = self->dirs[iter]->dir_next)
	{
		prev = &self->dirs[iter]->dir_next;
	};
	*prev = self->dir_fill;

	self->dir_fill++;

	return *prev;
}

/* returns UINT32_MAX on errors */
OCP_INTERNAL uint32_t
CDFS_File_add (struct cdfs_disc_t *self,
               const uint32_t      dir_parent_handle,
               /*char             *Filepath,*/
               char               *Filename)
{
	uint32_t *prev, iter;
	uint32_t dirdb_ref;

	debug_printf ("[CDFS] add_file: %s\n", Filename);

	if (self->file_fill == self->file_size)
	{
		int size = self->file_size + 64;
		struct cdfs_instance_file_t **files = realloc (self->files, size * sizeof (self->files[0]));

		if (!files)
		{ /* out of memory */
			return UINT32_MAX;
		}

		self->files = files;
		self->file_size = size;
	}

	dirdb_ref = dirdbFindAndRef (self->dirs[dir_parent_handle]->head.dirdb_ref, Filename, dirdb_use_file);

	self->files[self->file_fill] = malloc (sizeof (*self->files[self->file_fill]));
	if (!self->files[self->file_fill])
	{ /* out of memory */
		dirdbUnref (dirdb_ref, dirdb_use_file);
		return UINT32_MAX;
	}

	ocpfile_t_fill (&self->files[self->file_fill]->head,
	                 cdfs_file_ref,
	                 cdfs_file_unref,
	                &self->dirs[dir_parent_handle]->head,
	                 cdfs_file_open,
	                 cdfs_file_filesize,
	                 cdfs_file_filesize_ready,
	                 0, /* filename_override */
	                 dirdb_ref,
	                 0, /* refcount */
	                 0, /* is_nodetect */
	                 COMPRESSION_ADD_STORE(self->dirs[0]->head.compression));

	self->files[self->file_fill]->owner      = self;
	self->files[self->file_fill]->dir_parent = dir_parent_handle;
	self->files[self->file_fill]->file_next  = UINT32_MAX;
	self->files[self->file_fill]->filesize   = 0;
	self->files[self->file_fill]->extents    = 0;
	self->files[self->file_fill]->extent     = 0;
	//self->files[self->file_fill]->orig_full_filepath = strdup (Filepath);
	self->files[self->file_fill]->filenameshort = 0;
	self->files[self->file_fill]->audiotrack    = 0;

	prev = &self->dirs[dir_parent_handle]->file_child;
	for (iter = *prev; iter != UINT32_MAX; iter = self->files[iter]->file_next)
	{
		prev = &self->files[iter]->file_next;
	};
	*prev = self->file_fill;

	self->file_fill++;

	return *prev;
}

OCP_INTERNAL void
CDFS_File_zeroextent (struct cdfs_disc_t *disc, uint32_t handle, uint64_t length)
{
	void *temp;
	struct cdfs_instance_file_t *f;
	if (handle >= disc->file_fill) return;

	debug_printf ("[CDFS] add_zeroextent: %"PRIu64"\n", length);

	f = disc->files[handle];

	f->filesize += length;

	if (!f->extents) goto append;
	if (f->extent[f->extents-1].location == UINT32_MAX)
	{
		f->extent[f->extents-1].count += (length + 2047) / 2048;
		return;
	}
append:
	temp = realloc (f->extent, (f->extents + 1) * sizeof (f->extent[0]));
	if (!temp)
	{
		fprintf (stderr, "CDFS_File_zeroextent: realloc() failed\n");
		return;
	}
	f->extent = temp;
	f->extent[f->extents].location = UINT32_MAX;
	f->extent[f->extents].count = length / 2048;
	f->extent[f->extents].skip_start = 0;
	f->extents++;
}

OCP_INTERNAL void
CDFS_File_extent (struct cdfs_disc_t *disc, uint32_t handle, uint32_t location, uint64_t length, int sector_skip)
{
	void *temp;
	struct cdfs_instance_file_t *f;

	debug_printf ("[CDFS] add_extent: %"PRIu64"\n", length);

	if (handle >= disc->file_fill) return;

	f = disc->files[handle];

	f->filesize += length;

	if (!f->extents) goto append;
	if (f->extent[f->extents-1].location != UINT32_MAX) goto append;
	if (sector_skip) goto append;

	if ((f->extent[f->extents-1].location + f->extent[f->extents-1].count) == location)
	{
		f->extent[f->extents-1].count += (length + 2047) / 2048;
		return;
	}
append:
	temp = realloc (f->extent, (f->extents + 1) * sizeof (f->extent[0]));
	if (!temp)
	{
		fprintf (stderr, "CDFS_File_extent: realloc() failed\n");
		return;
	}
	f->extent = temp;
	f->extent[f->extents].location = location;
	f->extent[f->extents].count = (length + 2047) / 2048;
	f->extent[f->extents].skip_start = sector_skip;
	f->extents++;
}

OCP_INTERNAL uint32_t
CDFS_File_add_audio (struct cdfs_disc_t *self, const uint32_t dir_parent_handle, char *FilenameShort, char *FilenameLong, uint_fast32_t filesize, int audiotrack)
{
	uint32_t *prev, iter;

	uint32_t dirdb_ref;

	debug_printf ("[CDFS] add_audio: %s\n", FilenameShort);

	if (self->file_fill == self->file_size)
	{
		int size = self->file_size + 64;
		struct cdfs_instance_file_t **files = realloc (self->files, size * sizeof (self->files[0]));

		if (!files)
		{ /* out of memory */
			return UINT32_MAX;
		}

		self->files = files;
		self->file_size = size;
	}

	dirdb_ref = dirdbFindAndRef (self->dirs[dir_parent_handle]->head.dirdb_ref, FilenameLong, dirdb_use_file);

	self->files[self->file_fill] = malloc (sizeof (*self->files[self->file_fill]));
	if (!self->files[self->file_fill])
	{ /* out of memory */
		dirdbUnref (dirdb_ref, dirdb_use_file);
		return UINT32_MAX;
	}

	ocpfile_t_fill (&self->files[self->file_fill]->head,
	                 cdfs_file_ref,
	                 cdfs_file_unref,
	                &self->dirs[dir_parent_handle]->head,
	                 cdfs_audio_open,
	                 cdfs_file_filesize,
	                 cdfs_file_filesize_ready,
	                 cdfs_file_filename_override,
	                 dirdb_ref,
	                 0, /* refcount */
	                 0, /* is_nodetect */
	                 COMPRESSION_ADD_STORE(self->dirs[0]->head.compression));

	self->files[self->file_fill]->owner      = self;
	self->files[self->file_fill]->dir_parent = dir_parent_handle;
	self->files[self->file_fill]->file_next  = UINT32_MAX;
	self->files[self->file_fill]->filesize   = filesize;
	self->files[self->file_fill]->extents    = 0;
	self->files[self->file_fill]->extent     = 0;
	//self->files[self->file_fill]->orig_full_filepath = strdup (Filepath);
	self->files[self->file_fill]->filenameshort = strdup (FilenameShort);
	self->files[self->file_fill]->audiotrack = audiotrack;

	prev = &self->dirs[dir_parent_handle]->file_child;
	for (iter = *prev; iter != UINT32_MAX; iter = self->files[iter]->file_next)
	{
		prev = &self->files[iter]->file_next;
	};
	*prev = self->file_fill;

	self->file_fill++;

	return *prev;
}

static void cdfs_dir_ref (struct ocpdir_t *_self)
{
	struct cdfs_instance_dir_t *self = (struct cdfs_instance_dir_t *)_self;
	debug_printf ("CDFS_DIR_REF (old count = %d)\n", self->head.refcount);
	if (!self->head.refcount)
	{
		cdfs_disc_ref (self->owner);
	}
	self->head.refcount++;
}

static void cdfs_dir_unref (struct ocpdir_t *_self)
{
	struct cdfs_instance_dir_t *self = (struct cdfs_instance_dir_t *)_self;
	debug_printf ("CDFS_DIR_UNREF (old count = %d)\n", self->head.refcount);
	assert (self->head.refcount);
	self->head.refcount--;
	if (!self->head.refcount)
	{
		cdfs_disc_unref (self->owner);
	}
}

struct cdfs_instance_ocpdirhandle_t
{
	struct cdfs_instance_dir_t *dir;

	void(*callback_file)(void *token, struct ocpfile_t *);
        void(*callback_dir )(void *token, struct ocpdir_t *);
	void *token;
	int flatdir;
	uint32_t nextdir;
	uint32_t nextfile;
};


static ocpdirhandle_pt cdfs_dir_readdir_start (struct ocpdir_t *_self, void(*callback_file)(void *token, struct ocpfile_t *),
                                                                       void(*callback_dir )(void *token, struct ocpdir_t *), void *token)
{
	struct cdfs_instance_dir_t *self = (struct cdfs_instance_dir_t *)_self;
	struct cdfs_instance_ocpdirhandle_t *retval = malloc (sizeof (*retval));

	debug_printf ("cdfs_dir_readdir_start, we need to REF\n");
	_self->ref (_self);
	retval->dir = self;

	retval->callback_file = callback_file;
	retval->callback_dir = callback_dir;
	retval->token = token;

	retval->flatdir = 0;

	retval->nextfile = self->file_child;
	retval->nextdir = self->dir_child;

	return retval;
}

static ocpdirhandle_pt cdfs_dir_readflatdir_start (struct ocpdir_t *_self, void(*callback_file)(void *token, struct ocpfile_t *), void *token)
{
	struct cdfs_instance_dir_t *self = (struct cdfs_instance_dir_t *)_self;
	struct cdfs_instance_ocpdirhandle_t *retval = malloc (sizeof (*retval));

	debug_printf ("cdfs_dir_readflatdir_start, we need to REF\n");
	_self->ref (_self);
	retval->dir = self;

	retval->callback_file = callback_file;
	retval->callback_dir = 0;
	retval->token = token;

	retval->flatdir = 1;

	retval->nextfile = 0;

	return retval;
}

static void cdfs_dir_readdir_cancel (ocpdirhandle_pt _self)
{
	struct cdfs_instance_ocpdirhandle_t *self = (struct cdfs_instance_ocpdirhandle_t *)_self;

	debug_printf ("cdfs_dir_readdir_cancel, we need to UNREF\n");

	self->dir->head.unref (&self->dir->head);

	free (self);
}

static void check_audio_track (struct cdfs_disc_t *disc, struct cdfs_instance_file_t *file)
{
	uint32_t mdb_ref;
	struct moduleinfostruct mi;

	if (!file->audiotrack) return;

	mdb_ref = mdbGetModuleReference2 (file->head.dirdb_ref, file->filesize);

	if (mdb_ref == UINT32_MAX) return;

	if (mdbGetModuleInfo (&mi, mdb_ref))
	{
		if (strlen (mi.comment)) return;
		if (strlen (mi.album)) return;
		if (strlen (mi.artist)) return;

		mi.modtype.integer.i = MODULETYPE("CDA");
		mi.channels = 2;
		mi.playtime = file->filesize / (2352 * 75);

		if ((file->audiotrack < 0) || (file->audiotrack > 99))
		{
			strcpy(mi.title, "CDROM audio disc");
			if (disc->tracks_data[0].title)     snprintf (mi.title,    sizeof (mi.title),    "%s", disc->tracks_data[0].title);
			if (disc->tracks_data[0].performer) snprintf (mi.artist,   sizeof (mi.artist),   "%s", disc->tracks_data[0].performer);
			if (disc->tracks_data[0].composer)  snprintf (mi.composer, sizeof (mi.composer), "%s", disc->tracks_data[0].composer);
			if (disc->tracks_data[0].message)   snprintf (mi.comment,  sizeof (mi.comment),  "%s", disc->tracks_data[0].message);
		} else if (file->audiotrack < disc->tracks_count)
		{
			strcpy(mi.title, "CDROM audio track");
			if (disc->tracks_data[file->audiotrack].title)     snprintf (mi.title,    sizeof (mi.title),    "%s", disc->tracks_data[file->audiotrack].title);
			if (disc->tracks_data[file->audiotrack].performer) snprintf (mi.artist,   sizeof (mi.artist),   "%s", disc->tracks_data[file->audiotrack].performer);
			if (disc->tracks_data[file->audiotrack].composer)  snprintf (mi.composer, sizeof (mi.composer), "%s", disc->tracks_data[file->audiotrack].composer);
			if (disc->tracks_data[file->audiotrack].message)   snprintf (mi.comment,  sizeof (mi.comment),  "%s", disc->tracks_data[file->audiotrack].message);
		}

		if (disc->musicbrainzdata)
		{
			strcpy(mi.comment, "Looked up via Musicbrainz");
			snprintf (mi.album, sizeof (mi.album), "%s", disc->musicbrainzdata->album);
			if ((file->audiotrack < 0) || (file->audiotrack > 99))
			{
				snprintf (mi.title, sizeof (mi.title), "%s", disc->musicbrainzdata->album);
				if (disc->musicbrainzdata->artist[0][0])
				{
					snprintf (mi.artist, sizeof (mi.artist), "%s", disc->musicbrainzdata->artist[0]);
				}
				if (disc->musicbrainzdata->date[0])
				{
					mi.date = disc->musicbrainzdata->date[0];
				}
			} else {
				if (disc->musicbrainzdata->title[file->audiotrack][0])
				{
					snprintf (mi.title, sizeof (mi.title), "%s", disc->musicbrainzdata->title[file->audiotrack]);
				}
				if (disc->musicbrainzdata->artist[file->audiotrack][0])
				{
					snprintf (mi.artist, sizeof (mi.artist), "%s", disc->musicbrainzdata->artist[file->audiotrack]);
				}
				if (disc->musicbrainzdata->date[file->audiotrack])
				{
					mi.date = disc->musicbrainzdata->date[file->audiotrack];
				}
			}
		}
		mdbWriteModuleInfo (mdb_ref, &mi);
	}
}

static int cdfs_dir_readdir_iterate (ocpdirhandle_pt _self)
{
	struct cdfs_instance_ocpdirhandle_t *self = (struct cdfs_instance_ocpdirhandle_t *)_self;

	if (self->dir->owner->musicbrainzhandle)
	{
		if (musicbrainz_lookup_discid_iterate (self->dir->owner->musicbrainzhandle, &self->dir->owner->musicbrainzdata))
		{
			usleep (1000); /* anything is better than nothing... */
			return 1;      /* this will throttle this CPU core */
		}
		self->dir->owner->musicbrainzhandle = 0;
	}

	if (self->flatdir)
	{
		if (self->nextfile >= self->dir->owner->file_fill)
		{
			return 0;
		}
		check_audio_track (self->dir->owner, self->dir->owner->files[self->nextfile]);
		self->callback_file (self->token, &self->dir->owner->files[self->nextfile++]->head);
		return 1;
	} else {
		if (self->nextdir != UINT32_MAX)
		{
			self->callback_dir (self->token, &self->dir->owner->dirs[self->nextdir]->head);
			self->nextdir = self->dir->owner->dirs[self->nextdir]->dir_next;
			return 1;
		}
		if (self->nextfile != UINT32_MAX)
		{
			check_audio_track (self->dir->owner, self->dir->owner->files[self->nextfile]);
			self->callback_file (self->token, &self->dir->owner->files[self->nextfile]->head);
			self->nextfile = self->dir->owner->files[self->nextfile]->file_next;
			return 1;
		}
		return 0;
	}
}

static struct ocpdir_t *cdfs_dir_readdir_dir (struct ocpdir_t *_self, uint32_t dirdb_ref)
{
	struct cdfs_instance_dir_t *self = (struct cdfs_instance_dir_t *)_self;
	int i;

	for (i=0; i < self->owner->dir_fill; i++)
	{
		if (self->owner->dirs[i]->head.dirdb_ref == dirdb_ref)
		{
			self->owner->dirs[i]->head.ref (&self->owner->dirs[i]->head);
			return &self->owner->dirs[i]->head;
		}
	}

	return 0;
}

static struct ocpfile_t *cdfs_dir_readdir_file (struct ocpdir_t *_self, uint32_t dirdb_ref)
{
	struct cdfs_instance_dir_t *self = (struct cdfs_instance_dir_t *)_self;
	int i;

	for (i=0; i < self->owner->file_fill; i++)
	{
		if (self->owner->files[i]->head.dirdb_ref == dirdb_ref)
		{
			self->owner->files[i]->head.ref (&self->owner->files[i]->head);
			return &self->owner->files[i]->head;
		}
	}

	return 0;
}

static void cdfs_file_ref (struct ocpfile_t *_self)
{
	struct cdfs_instance_file_t *self = (struct cdfs_instance_file_t *)_self;
	debug_printf ("cdfs_file_ref (old value=%d)\n", self->head.refcount);
	if (!self->head.refcount)
	{
		cdfs_disc_ref (self->owner);
	}
	self->head.refcount++;
}

static void cdfs_file_unref (struct ocpfile_t *_self)
{
	struct cdfs_instance_file_t *self = (struct cdfs_instance_file_t *)_self;
	debug_printf ("cdfs_file_unref (old value=%d)\n", self->head.refcount);
	assert (self->head.refcount);
	self->head.refcount--;
	if (!self->head.refcount)
	{
		cdfs_disc_unref (self->owner);
	}
}

static struct ocpfilehandle_t *cdfs_file_open (struct ocpfile_t *_self)
{
	struct cdfs_instance_file_t *self = (struct cdfs_instance_file_t *)_self;
	struct cdfs_instance_filehandle_t *retval;

	retval = calloc (sizeof (*retval), 1);
	ocpfilehandle_t_fill (&retval->head,
	                       cdfs_filehandle_ref,
	                       cdfs_filehandle_unref,
	                       _self,
	                       cdfs_filehandle_seek_set,
	                       cdfs_filehandle_getpos,
	                       cdfs_filehandle_eof,
	                       cdfs_filehandle_error,
	                       cdfs_filehandle_read,
	                       0, /* ioctl */
	                       cdfs_filehandle_filesize,
	                       cdfs_filehandle_filesize_ready,
	                       0, /* filename_override */
	                       dirdbRef (self->head.dirdb_ref, dirdb_use_filehandle),
	                       1 /* refcount */
	);

	cdfs_disc_ref (self->owner);

	retval->file = self;
	retval->curextent = 0;
	retval->cursector = 0;
	retval->curoffset = UINT64_MAX;

	debug_printf ("We just created a CDFS handle\n");

	return &retval->head;
}

static struct ocpfilehandle_t *cdfs_audio_open (struct ocpfile_t *_self)
{
	struct cdfs_instance_file_t *self = (struct cdfs_instance_file_t *)_self;
	struct cdfs_instance_filehandle_t *retval;

	retval = calloc (sizeof (*retval), 1);
	ocpfilehandle_t_fill (&retval->head,
	                       cdfs_filehandle_ref,
	                       cdfs_filehandle_unref,
	                       _self,
	                       cdfs_filehandle_seek_set,
	                       cdfs_filehandle_getpos,
	                       cdfs_filehandle_eof,
	                       cdfs_filehandle_error,
	                       cdfs_filehandle_audio_read,
	                       cdfs_filehandle_audio_ioctl,
	                       cdfs_filehandle_filesize,
	                       cdfs_filehandle_filesize_ready,
	                       cdfs_filehandle_filename_override,
	                       dirdbRef (self->head.dirdb_ref, dirdb_use_filehandle),
	                       1 /* refcount */
	);

	cdfs_disc_ref (self->owner);

	retval->file = self;
	retval->curextent = 0;
	retval->cursector = 0;
	retval->curoffset = UINT64_MAX;

	debug_printf ("We just created a CDFS audio handle\n");

	return &retval->head;
}

static uint64_t cdfs_file_filesize (struct ocpfile_t *_self)
{
	struct cdfs_instance_file_t *self = (struct cdfs_instance_file_t *)_self;
	return self->filesize;
}

static int cdfs_file_filesize_ready (struct ocpfile_t *_self)
{
	return 1;
}

static const char *cdfs_file_filename_override (struct ocpfile_t *_self)
{
	struct cdfs_instance_file_t *self = (struct cdfs_instance_file_t *)_self;
	return self->filenameshort;
}

static int cdfs_filehandle_read (struct ocpfilehandle_t *_self, void *dst, int len)
{
#warning cdfs_filehandle_read() At the moment we are hardcoded to 2048 sectorsize only
	struct cdfs_instance_filehandle_t *self = (struct cdfs_instance_filehandle_t *)_self;
	int retval = 0;

	if (self->error)
	{
		return 0;
	}

	if (self->filepos >= self->file->filesize)
	{
		return 0;
	}

	if (self->curoffset > self->filepos)
	{ /* we need to rewind */
		self->curextent = 0;
		self->curoffset = 0;
		self->cursector=UINT32_MAX;
	} else if ((self->filepos - self->curoffset) < self->cursectorsize)
	{ /* we can use the current buffer */
		goto usebuffer;
	}

	/* skip stage */
	while (1)
	{
		uint64_t esize;
		if (self->curextent >= self->file->extents)
		{ /* EOF */
			return retval;
		}

		if (self->cursector != UINT32_MAX)
		{
#warning is this block correct?
			self->curoffset += 2048 - (self->cursector ? self->file->extent[self->curextent].skip_start : 0);
			self->cursector++;
			esize = (uint64_t)(self->file->extent[self->curextent].count - self->cursector) * 2048;
	
			if ((esize + self->curoffset) > self->filepos)
			{ /* this extent contains our next data, so calculate sector and break out ot the fetch stage */
				uint32_t skip = (self->filepos - self->curoffset) / 2048;
				self->cursector += skip;
				self->curoffset += (skip * 2048);
				break;
			}
			self->curoffset += esize;
		} else {
			/* how large is the current extent */
			esize = (uint64_t)(self->file->extent[self->curextent].count) * 2048 - self->file->extent[self->curextent].skip_start /* - self->file->extent[curextent].skip_skip_end */;
			if ((esize + self->curoffset) > self->filepos)
			{ /* this extent contains our next data, so calculate sector and break out ot the fetch stage */
				self->cursector = (self->filepos - self->curoffset - self->file->extent[self->curextent].skip_start) / 2048;
				self->curoffset += (self->cursector * 2048) - (self->cursector ? self->file->extent[self->curextent].skip_start : 0);
				break;
			}
			self->curoffset += esize;
		}

again:
		self->curextent++;
		self->cursector=UINT32_MAX;
	}

	/* fetch stage */
	while (1)
	{
		int go;
		int sectorskip;

		/* fill the buffer */
		if (self->file->extent[self->curextent].location == UINT32_MAX)
		{
			memset (self->buffer, 0, sizeof (self->buffer));
		} else {
			debug_printf ("CDFS FileHandle about to request physical sector %"PRIu32"\n", self->file->extent[self->curextent].location + self->cursector);
			if (cdfs_fetch_absolute_sector_2048 (self->file->owner, self->file->extent[self->curextent].location + self->cursector, self->buffer))
			{
				self->cursector = UINT32_MAX;
				if (len && (self->filepos > self->file->filesize))
				{
					self->error = 1;
				}
				return retval;
			}
		}
		if (self->cursector == 0)
		{
			self->cursectorskip = self->file->extent[self->curextent].skip_start;
			self->cursectorsize = 2048 - self->cursectorskip;
		} else {
			self->cursectorskip = 0;
			self->cursectorsize = 2048;
		}
		if (!len)
		{ /* did we need to read more data?, if so exit */
			return retval;
		}
usebuffer:
		//debug_printf ("self->cursectorskip=%d self->filepos=%lu self->curoffset=%lu => %lu\n", self->cursectorskip, self->filepos, self->curoffset, (self->filepos - self->curoffset));
		sectorskip = self->cursectorskip + (self->filepos - self->curoffset);

		go = self->cursectorsize - (self->filepos - self->curoffset);
		if (go > len)
		{
			go = len;
		}
		//debug_printf ("dst: %p, sectorskip=%d go=%d\n", dst, sectorskip, go);
		memcpy (dst, self->buffer + sectorskip, go);

		self->filepos += go;
		dst += go;	
		len -= go;
		retval += go;

		if ((go + sectorskip) == self->cursectorsize)
		{
			self->cursector++;
			self->curoffset += self->cursectorsize;
			if (self->cursector == self->file->extent[self->curextent].count)
			{ /* did we use the last of the buffer? if so, prefetch the next one even if our read-operation is actually complete, we need buffer to match cursector etc to be correct */
				goto again;
			}
		}

		if (!len)
		{
			return retval;
		}
	}

	return retval;
}

static void cdfs_filehandle_ref (struct ocpfilehandle_t *_self)
{
	struct cdfs_instance_filehandle_t *self = (struct cdfs_instance_filehandle_t *)_self;

	debug_printf ("cdfs_filehandle_ref (old count = %d)\n", self->head.refcount);
	self->head.refcount++;
}

static void cdfs_filehandle_unref (struct ocpfilehandle_t *_self)
{
	struct cdfs_instance_filehandle_t *self = (struct cdfs_instance_filehandle_t *)_self;

	debug_printf ("cdfs_filehandle_unref (old count = %d)\n", self->head.refcount);
	assert (self->head.refcount);
	self->head.refcount--;
	if (!self->head.refcount)
	{
		dirdbUnref (self->head.dirdb_ref, dirdb_use_filehandle);
		//cdfs_io_unref (self->file->owner);
		cdfs_disc_unref (self->file->owner);
		free (self);
	}
}

static int cdfs_filehandle_seek_set (struct ocpfilehandle_t *_self, int64_t pos)
{
	struct cdfs_instance_filehandle_t *self = (struct cdfs_instance_filehandle_t *)_self;

	if (pos < 0) return -1;

	if (pos > self->file->filesize) return -1;

	self->filepos = pos;
	self->error = 0;

	return 0;
}

static uint64_t cdfs_filehandle_getpos (struct ocpfilehandle_t *_self)
{
	struct cdfs_instance_filehandle_t *self = (struct cdfs_instance_filehandle_t *)_self;
	return self->filepos;
}

static int cdfs_filehandle_eof (struct ocpfilehandle_t *_self)
{
	struct cdfs_instance_filehandle_t *self = (struct cdfs_instance_filehandle_t *)_self;
	return self->filepos >= self->file->filesize;
}

static int cdfs_filehandle_error (struct ocpfilehandle_t *_self)
{
	struct cdfs_instance_filehandle_t *self = (struct cdfs_instance_filehandle_t *)_self;
	return self->error;
}

static uint64_t cdfs_filehandle_filesize (struct ocpfilehandle_t *_self)
{
	struct cdfs_instance_filehandle_t *self = (struct cdfs_instance_filehandle_t *)_self;
	return self->file->filesize;
}

static int cdfs_filehandle_filesize_ready (struct ocpfilehandle_t *_self)
{
	return 1;
}

static const char *cdfs_filehandle_filename_override (struct ocpfilehandle_t *_self)
{
	struct cdfs_instance_filehandle_t *self = (struct cdfs_instance_filehandle_t *)_self;
	return self->file->filenameshort;
}

static int cdfs_filehandle_audio_read (struct ocpfilehandle_t *_handle, void *dst, int len)
{
	return len ? -1 : 0;
}

static int cdfs_filehandle_audio_ioctl (struct ocpfilehandle_t *_self, const char *cmd, void *ptr)
{
	struct cdfs_instance_filehandle_t *self = (struct cdfs_instance_filehandle_t *)_self;
	struct cdfs_disc_t *disc = self->file->owner;

	if (!strcmp (cmd, IOCTL_CDROM_READTOC))
	{
		struct ioctl_cdrom_readtoc_request_t *request = ptr;
		int i;
		for (i=0; i < disc->tracks_count; i++)
		{
			request->track[i].lba_addr = disc->tracks_data[i].start + (i?150:0) + disc->tracks_data[i].pregap;
			switch (cdfs_get_sector_format (disc, disc->tracks_data[i].start + disc->tracks_data[i].pregap))
			{
				case FORMAT_AUDIO___NONE:
				case FORMAT_AUDIO___RW:
				case FORMAT_AUDIO___RAW_RW:
				case FORMAT_AUDIO_SWAP___NONE:
				case FORMAT_AUDIO_SWAP___RW:
				case FORMAT_AUDIO_SWAP___RAW_RW:
					request->track[i].is_data = 0;
					break;
				default:
				case FORMAT_RAW___NONE:
				case FORMAT_RAW___RW:
				case FORMAT_RAW___RAW_RW:
				case FORMAT_MODE1_RAW___NONE:
				case FORMAT_MODE1_RAW___RW:
				case FORMAT_MODE1_RAW___RAW_RW:
				case FORMAT_MODE2_RAW___NONE:
				case FORMAT_MODE2_RAW___RW:
				case FORMAT_MODE2_RAW___RAW_RW:
				case FORMAT_XA_MODE2_RAW:
				case FORMAT_XA_MODE2_RAW___RW:
				case FORMAT_XA_MODE2_RAW___RAW_RW:
				case FORMAT_MODE1___NONE:
				case FORMAT_MODE1___RW:
				case FORMAT_MODE1___RAW_RW:
				case FORMAT_XA_MODE2_FORM1___NONE:
				case FORMAT_XA_MODE2_FORM1___RW:
				case FORMAT_XA_MODE2_FORM1___RAW_RW:
				case FORMAT_MODE_1__XA_MODE2_FORM1___NONE:
				case FORMAT_MODE_1__XA_MODE2_FORM1___RW:
				case FORMAT_MODE_1__XA_MODE2_FORM1___RAW_RW:
				case FORMAT_MODE2___NONE:
				case FORMAT_MODE2___RW:
				case FORMAT_MODE2___RAW_RW:
				case FORMAT_XA_MODE2_FORM2___NONE:
				case FORMAT_XA_MODE2_FORM2___RW:
				case FORMAT_XA_MODE2_FORM2___RAW_RW:
				case FORMAT_XA_MODE2_FORM_MIX___NONE:
				case FORMAT_XA_MODE2_FORM_MIX___RW:
				case FORMAT_XA_MODE2_FORM_MIX___RAW_RW:
				case FORMAT_XA1_MODE2_FORM1___NONE:
				case FORMAT_XA1_MODE2_FORM1___RW:
				case FORMAT_XA1_MODE2_FORM1___RW_RAW:
				case FORMAT_ERROR:
					request->track[i].is_data = 1;
					break;
			}
		}
		request->track[i].lba_addr = disc->tracks_data[i-1].start + disc->tracks_data[i-1].length;
		request->starttrack = 1;
		request->lasttrack = disc->tracks_count-1;
		return 0;
	}
	if (!strcmp (cmd, IOCTL_CDROM_READAUDIO_ASYNC_REQUEST))
	{
		struct ioctl_cdrom_readaudio_request_t *request = ptr;
		uint8_t *dst = request->ptr;
		uint32_t src = request->lba_addr - 150;
		uint32_t count = request->lba_count;

		request->retval = 0;
		
		while (count)
		{
			request->retval |= cdfs_fetch_absolute_sector_2352 (disc, src, dst);
			dst += 2352;
			src++;
			count--;
		}

		return 0;
	}
	return -1;
}

OCP_INTERNAL void cdfs_disc_ref (struct cdfs_disc_t *self)
{

	debug_printf ( " cdfs_disc_ref (old count = %d)\n", self->refcount);
	if (!self->refcount)
	{
		//self->archive_filehandle = self->archive_file->open (self->archive_file);
	}
	self->refcount++;

}

OCP_INTERNAL void cdfs_disc_unref (struct cdfs_disc_t *self)
{
	debug_printf ( " cdfs_disc_unref (old count = %d)\n", self->refcount);

	self->refcount--;
	if (self->refcount)
	{
		return;
	}

	cdfs_disc_free (self);
}

