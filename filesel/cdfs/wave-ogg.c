/* OpenCP Module Player
 * copyright (c) 2026 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * OGG file support for CDFS images
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

/* OGG is always enabled, configure fails if not present */

#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

struct cdfs_datasource_handle_ogg_t
{
	struct cdfs_datasource_handle_t h;
	int refcount;
	int current_section;
	//uint64_t currentsamplepos;

	struct ocpfilehandle_t *fh;
	OggVorbis_File ov;
};

static size_t OV_read_func (void *ptr, size_t size, size_t nmemb, void *token)
{
	struct ocpfilehandle_t *oggfile = token;
	uint64_t retval;
	retval = oggfile->read (oggfile, ptr, size * nmemb);
	return retval / size;
}

static int OV_seek_func (void *token, ogg_int64_t offset, int whence)
{
	struct ocpfilehandle_t *oggfile = token;

	switch (whence)
	{
		case SEEK_SET:
			if (oggfile->seek_set (oggfile, offset) < 0)
			{
				return -1;
			}
			break;
		case SEEK_END:
			if (oggfile->seek_set (oggfile, oggfile->filesize(oggfile) + offset) < 0)
			{
				return -1;
			}
			break;
		case SEEK_CUR:
			if (oggfile->seek_set (oggfile, oggfile->getpos(oggfile) + offset) < 0)
			{
				return -1;
			}
			break;
		default:
			return -1;
	}
	return oggfile->getpos (oggfile);
}

static int OV_close_func(void *token)
{
	return 0;
}

static long OV_tell_func (void *token)
{
	struct ocpfilehandle_t *oggfile = token;

	return oggfile->getpos (oggfile);
}

static ov_callbacks OV_callbacks =
{
	OV_read_func,
	OV_seek_func,
	OV_close_func,
	OV_tell_func
};

static void audiofile_handle_ogg_ref (struct cdfs_datasource_handle_t *_ah)
{
	struct cdfs_datasource_handle_ogg_t *ah = (struct cdfs_datasource_handle_ogg_t *)_ah;
	ah->refcount++;
}

static void audiofile_handle_ogg_unref (struct cdfs_datasource_handle_t *_ah)
{
	struct cdfs_datasource_handle_ogg_t *ah = (struct cdfs_datasource_handle_ogg_t *)_ah;
	ah->refcount--;
	if (ah->refcount)
	{
		return;
	}

	ov_clear(&ah->ov);

	ah->fh->unref (ah->fh);
	free (ah);
}

static unsigned int audiofile_handle_ogg_read (struct cdfs_datasource_handle_t *_ah, uint64_t offset, uint8_t *target, unsigned int len)
{
	struct cdfs_datasource_handle_ogg_t *ah = (struct cdfs_datasource_handle_ogg_t *)_ah;
	unsigned int retval = 0;
	long result;

	debug_printf("audiofile_handle_ogg_read(offset %"PRIu64", len %u)\n", offset, len);

	if ((len & 3) || (offset & 3))
	{
		memset (target, 0, len);
		return 0;
	}
	offset >>= 2; /* stereo, 16-bit */

	if (ov_pcm_tell (&ah->ov) != offset)
	{
		ov_pcm_seek (&ah->ov, offset);
	}

	while (len)
	{
#ifndef WORDS_BIGENDIAN
		result = ov_read (&ah->ov, (char *)target, len, 0, 2, 1, &ah->current_section);
#else
		result = ov_read (&ah->ov, (char *)target, len, 1, 2, 1, &ah->current_section);
#endif
		debug_printf (" ov_read(len=%u) result=%ld\n", len, result);
		if (result <= 0)
		{
			memset (target, 0, len);
			return retval;
		}
		retval += result;
		target += result;
		len -= result;
	}

	return retval;
}

static struct cdfs_datasource_handle_t *spawn_audiofile_handle_ogg (struct ocpfilehandle_t *fh)
{
	struct cdfs_datasource_handle_ogg_t *retval;
	int result;

	debug_printf("spawn_audiofile_handle_ogg\n");

	fh->seek_set (fh, 0);

	retval = calloc (1, sizeof (*retval));
	if (!retval)
	{
		fprintf (stderr, "spawn_audiofile_handle_ogg(): calloc() failed\n");
		return 0;
	}
	retval->fh = fh;
	fh->ref (fh);

	if ((result = ov_open_callbacks(fh, &retval->ov, NULL, 0, OV_callbacks)))
	{
		switch (result)
		{
			case OV_EREAD:      fprintf (stderr, "spawn_audiofile_handle_ogg(): ov_open_callbacks(): A read from media returned an error.\n"); break;
			case OV_ENOTVORBIS: fprintf (stderr, "spawn_audiofile_handle_ogg(): ov_open_callbacks(): Bitstream does not contain any Vorbis data.\n"); break;
			case OV_EVERSION:   fprintf (stderr, "spawn_audiofile_handle_ogg(): ov_open_callbacks(): Vorbis version mismatch.\n"); break;
			case OV_EBADHEADER: fprintf (stderr, "spawn_audiofile_handle_ogg(): ov_open_callbacks(): Invalid Vorbis bitstream header.\n"); break;
			case OV_EFAULT:     fprintf (stderr, "spawn_audiofile_handle_ogg(): ov_open_callbacks(): Internal logic fault; indicates a bug or heap/stack corruption.\n"); break;
			default:            fprintf (stderr, "spawn_audiofile_handle_ogg(): ov_open_callbacks(): Unknown error %d\n", result); break;
		}
		goto error_out_ov;
	}

	struct vorbis_info *vi = ov_info (&retval->ov, -1);
	if (!vi)
	{
		fprintf (stderr, "spawn_audiofile_handle_ogg(): vi_info() failed\n");
		goto error_out_ov;
	}
	if (vi->channels != 2)
	{
		fprintf (stderr, "spawn_audiofile_handle_ogg(): channels != 2\n");
		goto error_out_ov;
	}
	if (vi->rate != 44100)
	{
		fprintf (stderr, "spawn_audiofile_handle_ogg(): rate != 44100\n");
		goto error_out_ov;
	}

	retval->h.ref = audiofile_handle_ogg_ref;
	retval->h.unref = audiofile_handle_ogg_unref;
	retval->h.dirdb_ref = fh->dirdb_ref;
	retval->h.length = ov_pcm_total(&retval->ov, -1) << (1 /* stereo */ + 1 /* 16-bit */);
	retval->h.read = audiofile_handle_ogg_read;

	retval->refcount = 1;

	return &retval->h;

error_out_ov:
	ov_clear(&retval->ov);

	retval->fh->unref (retval->fh);
	free (retval);

	return 0;
}
