/* OpenCP Module Player
 * copyright (c) 2022-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * WAVE file support for CDFS images
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

#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "wave.h"

OCP_INTERNAL int wave_filename(const char *filename)
{
	int len = strlen (filename);
	if (len < 4)
	{
		return 0;
	}

	if (!strcasecmp (filename + len - 4, ".wav"))
	{
		return 1;
	}

	if (len < 5)
	{
		return 0;
	}

	if (!strcasecmp (filename + len - 5, ".wave"))
	{
		return 1;
	}

	return 0;
}

static int wave_openfile2 (struct ocpfilehandle_t *handle, uint64_t *offset, uint64_t *length)
{
	uint8_t b16[16];
	// RIFFoffset = 0
	uint32_t  RIFFlen;
	uint32_t _RIFFlen;

	/* RIFF + len
	 * {
	 *   WAVE + len
	 *   {
	 *     fmt  + len
	 *     {
	 *        compression, alignement, bitrate, ....
	 *     }
	 *     ....
	 *     data + len
	 *     {
	 *       pcm data....
	 *     }
	 *   }
	 * }
	 */

	if (handle->read (handle, b16, 8) != 8)
	{
		fprintf (stderr, "wave_openfile() failed to read RIFF header\n");
		return -1;
	}
	if ((b16[0] != 'R') ||
	    (b16[1] != 'I') ||
	    (b16[2] != 'F') ||
	    (b16[3] != 'F'))
	{
		fprintf (stderr, "wave_openfile() failed to verify RIFF header\n");
		return -1;
	}
	_RIFFlen = RIFFlen = b16[4] | (b16[5] << 8) | (b16[6] << 16) | (b16[7] << 24);
	if (RIFFlen < (4 + 8 + 16 + 8 + 1))
	{
		fprintf (stderr, "wave_openfile() RIFF length is smaller than absolute minimum size\n");
		return -1;
	}

	if (handle->read (handle, b16, 4) != 4)
	{
		fprintf (stderr, "wave_openfile() failed to read WAVE subheader\n");
		return -1;
	}
	_RIFFlen -= 4;
	if ((b16[0] != 'W') ||
	    (b16[1] != 'A') ||
	    (b16[2] != 'V') ||
	    (b16[3] != 'E'))
	{
		fprintf (stderr, "wave_openfile() failed to verify WAVE subheader\n");
		return -1;
	}

	/* locate "fmt ", it always appears before data */
	while (1)
	{
		uint32_t sublen;
		if (_RIFFlen < 8)
		{
			fprintf (stderr, "wave_openfile() ran out of space inside RIFF header when searching for fmt subheader #1\n");
			return -1;
		}

		if (handle->read (handle, b16, 8) != 8)
		{
			fprintf (stderr, "wave_openfile() failed to read WAVE subheader\n");
			return -1;
		}
		_RIFFlen -= 8;
		sublen = b16[4] | (b16[5] << 8) | (b16[6] << 16) | (b16[7] << 24);
		if (_RIFFlen < sublen)
		{
			fprintf (stderr, "wave_openfile() ran out of space inside RIFF header when searching for fmt subheader #2\n");
			return -1;
		}
		if ((b16[0] != 'f') ||
		    (b16[1] != 'm') ||
		    (b16[2] != 't') ||
		    (b16[3] != ' '))
		{
			if (handle->seek_set (handle, handle->getpos (handle) + sublen))
			{
				fprintf (stderr, "wave_openfile() lseek caused EOF when skipping chunk while searching for fmt subheader\n");
				return -1;
			}
			_RIFFlen -= sublen;
			continue;
		}
		if (sublen < 16)
		{
			fprintf (stderr, "wave_openfile() fmt subheader is way too small\n");
			return -1;
		}
		if (handle->read (handle, b16, 16) != 16)
		{
			fprintf (stderr, "wave_openfile() failed to read fmt data\n");
			return -1;
		}
		if (sublen > 16)
		{
			if (handle->seek_set (handle, handle->getpos (handle) + sublen - 16))
			{
				fprintf (stderr, "wave_openfile() lseek caused EOF when skipping end of fmt chunk\n");
				return -1;
			}
		}
		_RIFFlen -= sublen;
		if ((b16[ 0] != 0x01) || // PCM-16bit
		    (b16[ 1] != 0x00) || // ^

		    (b16[ 2] != 0x02) || // 2-channels
		    (b16[ 3] != 0x00) || // ^

		    (b16[ 4] != 0x44) || // 44100Hz sample rate
		    (b16[ 5] != 0xac) || // ^^^
		    (b16[ 6] != 0x00) || // ^^
		    (b16[ 7] != 0x00) || // ^

		    (b16[ 8] != 0x10) || // 176400 bytes per second
		    (b16[ 9] != 0xb1) || // ^^^
		    (b16[10] != 0x02) || // ^^
		    (b16[11] != 0x00) || // ^

		    (b16[12] != 0x04) || // 4 bytes block align (bytes per sample)
		    (b16[13] != 0x00) || // ^

		    (b16[14] != 0x10) || // 16 bits per sample
		    (b16[15] != 0x00))   // ^
		{
			fprintf (stderr, "wave_openfile() WAV file is not 16bit stereo 44100Hz PCM formatted\n");
			return -1;
		}
		break; 
	}

	/* locate "data" */
	while (1)
	{
		uint32_t sublen;
		if (_RIFFlen < 8)
		{
			fprintf (stderr, "wave_openfile() ran out of space inside RIFF header when searching for data subheader #1\n");
			return -1;
		}

		if (handle->read (handle, b16, 8) != 8)
		{
			fprintf (stderr, "wave_openfile() failed to read WAVE subheader\n");
			return -1;
		}
		_RIFFlen -= 8;
		sublen = b16[4] | (b16[5] << 8) | (b16[6] << 16) | (b16[7] << 24);
		if (_RIFFlen < sublen)
		{
			fprintf (stderr, "wave_openfile() ran out of space inside RIFF header when searching for data subheader #2\n");
			return -1;
		}
		if ((b16[0] != 'd') ||
		    (b16[1] != 'a') ||
		    (b16[2] != 't') ||
		    (b16[3] != 'a'))
		{
			if (handle->seek_set (handle, handle->getpos (handle) + sublen))
			{
				fprintf (stderr, "wave_openfile() lseek caused EOF when skipping chunk while searching for fmt subheader\n");
				return -1;
			}
			_RIFFlen -= sublen;
			continue;
		}
		*offset = RIFFlen - _RIFFlen + 8;
		*length = sublen;
		return 0;
	}
}

struct wave_openfile_result_t
{
	struct ocpfile_t *exact;
	struct ocpfile_t *inexact;
	const char *filename;
};

static void wave_openfile_file (void *token, struct ocpfile_t *file)
{
	struct wave_openfile_result_t *result = token;
	const char *filename = 0;
	dirdbGetName_internalstr (file->dirdb_ref, &filename);
	if (!strcmp (filename, result->filename))
	{
		result->exact = file;
		file->ref (file);
		return;
	}
	if (result->inexact)
	{
		return;
	}
	if (!strcasecmp (filename, result->filename))
	{
		result->inexact = file;
		file->ref (file);
		return;
	}
}

static void wave_openfile_dir (void *token, struct ocpdir_t *dir)
{
}

OCP_INTERNAL int wave_openfile (struct ocpdir_t *dir, const char *filename, struct ocpfile_t **file, struct ocpfilehandle_t **handle, uint64_t *offset, uint64_t *length)
{
	struct wave_openfile_result_t result;
	ocpdirhandle_pt dh;
	if (!dir) return -1;
	*file = 0;
	*handle = 0;
	*offset = 0;
	*length = 0;

	result.exact = 0;
	result.inexact = 0;
	result.filename = filename;
	dh = dir->readdir_start (dir, wave_openfile_file, wave_openfile_dir, &result);
	if (!dh) return -1;
	while (dir->readdir_iterate (dh))
	{
		if (result.exact)
		{
			break;
		}
	}
	dir->readdir_cancel (dh);

	if (result.exact)
	{
		*file = result.exact;
		result.exact = 0;
		if (result.inexact)
		{
			result.inexact->unref (result.inexact);
			result.inexact = 0;
		}
	} else if (result.exact)
	{
		*file = result.inexact;
		result.inexact = 0;
	} else {
		return -1;
	}

	*handle = (*file)->open (*file);
	if (!*handle)
	{
		(*file)->unref (*file);
		*file = 0;
		return -1;
	}

	if (wave_openfile2(*handle, offset, length))
	{
		(*handle)->unref (*handle);
		(*file)->unref (*file);
		*file = 0;
		*handle = 0;
		*offset = 0;
		*length = 0;
		return -1;
	}
	return 0;
}

OCP_INTERNAL int data_openfile (struct ocpdir_t *dir, const char *filename, struct ocpfile_t **file, struct ocpfilehandle_t **handle, uint64_t *length)
{
	struct wave_openfile_result_t result;
	ocpdirhandle_pt dh;
	if (!dir) return -1;
	*file = 0;
	*handle = 0;
	*length = 0;

	result.exact = 0;
	result.inexact = 0;
	result.filename = filename;
	dh = dir->readdir_start (dir, wave_openfile_file, wave_openfile_dir, &result);
	if (!dh) return -1;
	while (dir->readdir_iterate (dh))
	{
		if (result.exact)
		{
			break;
		}
	}
	dir->readdir_cancel (dh);

	if (result.exact)
	{
		*file = result.exact;
		result.exact = 0;
		if (result.inexact)
		{
			result.inexact->unref (result.inexact);
			result.inexact = 0;
		}
	} else if (result.exact)
	{
		*file = result.inexact;
		result.inexact = 0;
	} else {
		return -1;
	}

	*handle = (*file)->open (*file);
	if (!*handle)
	{
		(*file)->unref (*file);
		*file = 0;
		return -1;
	}
	*length = (*handle)->filesize(*handle);

	return 0;
}

