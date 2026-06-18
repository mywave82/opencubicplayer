/* OpenCP Module Player
 * copyright (c) 2026 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * BIN/RAW file support for CDFS images
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



#ifndef HAVE_WAVPACK
static struct cdfs_datasource_handle_t *spawn_audiofile_handle_wavpack (struct ocpfilehandle_t *fh)
{
	fprintf (stderr, "[CUE/TOC]: Warning: WavPack support not enabled, adding audio file source failed\n");
	return 0;
}
#else

#include <math.h>
#include <wavpack/wavpack.h>

struct cdfs_datasource_handle_wavpack_t
{
	struct cdfs_datasource_handle_t h;
	int refcount;
	uint64_t currentsamplepos;

	int BytesPerSample;
	int Mode; // MODE_FLOAT is important here

	WavpackContext *wpc;
	//WavpackStreamReader64 wpfile;// = {0};
	struct ocpfilehandle_t *mainfile, *controlfile;

	int32_t buffer[588*2];
};

static WavpackStreamReader64 wpfile = {0};

static int32_t wpfile_read_bytes (void *id, void *data, int32_t bcount)
{
	struct ocpfilehandle_t *f = (struct ocpfilehandle_t *)id;
	return f->read (f, data, bcount);
}

static int32_t wpfile_write_bytes (void *id, void *data, int32_t bcount)
{
	return 0;
}

static int64_t wpfile_get_pos (void *id)
{
	struct ocpfilehandle_t *f = (struct ocpfilehandle_t *)id;
	return f->getpos (f);
}

static int wpfile_set_pos_abs (void *id, int64_t pos)
{
	struct ocpfilehandle_t *f = (struct ocpfilehandle_t *)id;
	return f->seek_set (f, pos);
}

static int wpfile_set_pos_rel (void *id, int64_t newpos, int mode)
{
	struct ocpfilehandle_t *f = (struct ocpfilehandle_t *)id;

	switch (mode)
	{
		case SEEK_SET: return f->seek_set (f, newpos);
		case SEEK_CUR:
		{
			uint64_t len = f->filesize (f);
			uint64_t curpos = f->getpos (f);
			if (newpos >= 0)
			{
				if ((curpos + newpos) > len) return -1;
				return f->seek_set (f, curpos + newpos);
			}
			if (-newpos > curpos)
			{
				return -1;
			}
			return f->seek_set (f, curpos + newpos);
		}
		case SEEK_END:
		{
			uint64_t len = f->filesize (f);
			if (newpos > 0)
			{
				return -1;
			}
			if (-newpos > len)
			{
				return -1;
			}
			return f->seek_set (f, len + newpos);
		}
	}
	return -1;
}

static int wpfile_push_back_byte (void *id, int c)
{
	return wpfile_set_pos_rel (id, -1, SEEK_CUR);
}

static int64_t wpfile_get_length (void *id)
{
	struct ocpfilehandle_t *f = (struct ocpfilehandle_t *)id;
	return f->filesize (f);
}

static int wpfile_can_seek (void *id)
{
	return 1;
}

static int wpfile_truncate_here (void *id)
{
	return -1;
}

static int wpfile_close (void *id)
{
	return 0;
}

static void audiofile_handle_wavpack_ref (struct cdfs_datasource_handle_t *_ah)
{
	struct cdfs_datasource_handle_wavpack_t *ah = (struct cdfs_datasource_handle_wavpack_t *)_ah;
	ah->refcount++;
}

static void audiofile_handle_wavpack_unref (struct cdfs_datasource_handle_t *_ah)
{
	struct cdfs_datasource_handle_wavpack_t *ah = (struct cdfs_datasource_handle_wavpack_t *)_ah;
	ah->refcount--;
	if (ah->refcount)
	{
		return;
	}
	WavpackCloseFile (ah->wpc);
	ah->mainfile->unref (ah->mainfile);
	if (ah->controlfile) ah->controlfile->unref (ah->controlfile);
	free (ah);
}

static unsigned int audiofile_handle_wavpack_read (struct cdfs_datasource_handle_t *_ah, uint64_t offset, uint8_t *target, unsigned int len)
{
	struct cdfs_datasource_handle_wavpack_t *ah = (struct cdfs_datasource_handle_wavpack_t *)_ah;
	uint64_t retval = 0;

	if ((len & 3) || (offset & 3))
	{
		memset (target, 0, len);
		return 0;
	}
	offset >>= 2; /* stereo, 16-bit */
	if ((ah->currentsamplepos) != offset)
	{
		WavpackSeekSample64 (ah->wpc, offset);
	}

	while (len)
	{
		int job = len >> 2;
		if (job > 588)
		{
			job = 588;
		}

		uint32_t result = WavpackUnpackSamples (ah->wpc, ah->buffer, job);
		if (result == 0)
		{
			break;
		}

		int i;
		if (ah->Mode & MODE_FLOAT)
		{
			float *f = (float *)ah->buffer;
			int16_t *t = (int16_t *)target;
			int r = result;

			r *= 2; // stereo, two actual samples, per sample row
			for (i=0; i < r; i++)
			{
				if (*f >= 1.0)
				{
					*t++ = 32767;
				} else if (*f <= -1.0)
				{
					*t++ = -32768;
				} else {
					*t++ = floor ((*f) * 32768.0);
				}
				f++;
			}
		} else {
			int32_t *d = ah->buffer;
			int16_t *t = (int16_t *)target;
			int r = result;

			r *= 2; // stereo, two actual samples, per sample row
			switch (ah->BytesPerSample)
			{
				case 4:
					for (i=0; i < r; i++)
					{
						*t++ = (*d++) >> 16;
					}
					break;

				case 3:
					for (i=0; i < r; i++)
					{
						*t++ = (*d++) >> 8;
					}
					break;

				case 2:
					for (i=0; i < r; i++)
					{
						*t++ = *d++;
					}
					break;

				default:
				case 1:
					for (i=0; i < r; i++)
					{
						uint16_t v = (uint8_t)*d++;
						v |= (v<<8);
						*t++ = (int16_t)v;

					}
					break;
			}
		}

		ah->currentsamplepos += result;
		len -= result<<2;
		target += result<<2;
		retval += result<<2;
	}

	if (retval < len)
	{
		memset (target, 0, len);
	}
	return retval;
}

static struct cdfs_datasource_handle_t *spawn_audiofile_handle_wavpack2 (struct ocpfilehandle_t *fh, struct ocpfilehandle_t *fhc)
{
	struct cdfs_datasource_handle_wavpack_t *retval;
	WavpackContext *wpc;

	wpfile.read_bytes     = wpfile_read_bytes;
	wpfile.write_bytes    = wpfile_write_bytes;
	wpfile.get_pos        = wpfile_get_pos;
	wpfile.set_pos_abs    = wpfile_set_pos_abs;
	wpfile.set_pos_rel    = wpfile_set_pos_rel;
	wpfile.push_back_byte = wpfile_push_back_byte;
	wpfile.get_length     = wpfile_get_length;
	wpfile.can_seek       = wpfile_can_seek;
	wpfile.truncate_here  = wpfile_truncate_here;
	wpfile.close          = wpfile_close;

	char error[128];
	wpc = WavpackOpenFileInputEx64 (&wpfile, fh, fhc, error, (fhc ? OPEN_WVC : 0) | OPEN_2CH_MAX | OPEN_NORMALIZE, 0);
	if (!wpc)
	{
		fprintf (stderr, "spawn_audiofile_handle_wavpack2(): WavpackOpenFileInputEx64() failed: %s\n", error);
		return 0;
	}
	if (WavpackGetSampleRate (wpc) != 44100)
	{
		fprintf (stderr, "spawn_audiofile_handle_wavpack2(): samplerate not 44100: %u\n", (unsigned int)WavpackGetSampleRate (wpc));
		WavpackCloseFile (wpc);
		wpc = 0;
		return 0;
	}
	if (WavpackGetReducedChannels (wpc) != 2)
	{
		fprintf (stderr, "spawn_audiofile_handle_wavpack2(): not a stereo file: %u channels\n", (unsigned int) WavpackGetNumChannels (wpc));
		WavpackCloseFile (wpc);
		wpc = 0;
		return 0;
	}

	retval = calloc (1, sizeof (*retval));
	if (!retval)
	{
		fprintf (stderr, "spawn_audiofile_handle_wavpack2(): calloc() failed\n");
		WavpackCloseFile (wpc);
		wpc = 0;
		return 0;
	}

	retval->h.ref = audiofile_handle_wavpack_ref;
	retval->h.unref = audiofile_handle_wavpack_unref;
	retval->h.dirdb_ref = fh->dirdb_ref;
	retval->h.length = WavpackGetNumSamples64 (wpc) << (1 /* stereo */ + 1 /* 16-bit */);
	retval->h.read = audiofile_handle_wavpack_read;

	retval->refcount = 1;
	retval->currentsamplepos = 0;
	retval->wpc = wpc;
	retval->BytesPerSample = WavpackGetBytesPerSample (wpc);
	retval->Mode = WavpackGetMode (wpc);
	retval->mainfile = fh;
	retval->controlfile = fhc;
	fh->ref (fh);
	if (fhc) fhc->ref(fhc);

	return &retval->h;
}

struct wavpackSearchC
{
	const char *filename;
	struct ocpfile_t *indirect_hit;
	struct ocpfile_t *direct_hit;
};

static void file_hit (void *token, struct ocpfile_t *f)
{
	const char *filename;
	struct wavpackSearchC *t = (struct wavpackSearchC *)token;
	if (t->direct_hit)
	{
		return;
	}
	dirdbGetName_internalstr (f->dirdb_ref, &filename);
	if (!strcmp (filename, t->filename))
	{
		t->direct_hit = f;
		f->ref (f);
		return;
	}
	if (!t->indirect_hit)
	{
		if (!strcasecmp (filename, t->filename))
		{
			t->indirect_hit = f;
			f->ref (f);
			return;
		}
	}
}

static void dir_hit (void *token, struct ocpdir_t *f)
{
	return;
}

static struct cdfs_datasource_handle_t *spawn_audiofile_handle_wavpack (struct ocpfilehandle_t *fh)
{
	struct cdfs_datasource_handle_t *retval = 0;
	struct ocpfile_t *file_wvc = 0;
	struct ocpfilehandle_t *filehandle_wvc = 0;

	const char *filename;
	dirdbGetName_internalstr (fh->dirdb_ref, &filename);

	do {
		size_t filename_len;
		char *filename_wvc_ideal;

		filename_len = strlen (filename);
		filename_wvc_ideal = malloc (filename_len+2);
		if (filename_wvc_ideal)
		{
			strcpy (filename_wvc_ideal, filename);
			filename_wvc_ideal[filename_len] = 'c';
			filename_wvc_ideal[filename_len+1] = 0;
			if ((filename_len >= 2) &&
			     ((filename[filename_len-2] == 'W') || (filename[filename_len-1] == 'V')) )
			{
				filename_wvc_ideal[filename_len] = 'C';
			}

			struct wavpackSearchC token;
			token.filename = filename_wvc_ideal;
			token.indirect_hit = 0;
			token.direct_hit = 0;
			ocpdirhandle_pt readdir = fh->origin->parent->readdir_start(fh->origin->parent, file_hit, dir_hit, &token);
			if (readdir)
			{
				while (fh->origin->parent->readdir_iterate(readdir) && (!token.direct_hit))
				{
				}
				fh->origin->parent->readdir_cancel (readdir);
			}
			if (token.direct_hit)
			{
				file_wvc = token.direct_hit;
				if (token.indirect_hit)
				{
					token.indirect_hit->unref (token.indirect_hit);
				}
			} else if (token.indirect_hit)
			{
				file_wvc = token.indirect_hit;
			}
		}

		free (filename_wvc_ideal);
	} while (0);

	if (file_wvc)
	{
		filehandle_wvc = file_wvc->open (file_wvc);
		file_wvc->unref (file_wvc);
		file_wvc = 0;
	}

	retval = spawn_audiofile_handle_wavpack2 (fh, filehandle_wvc);
	if (filehandle_wvc)
	{
		filehandle_wvc->unref (filehandle_wvc);
	}
	return retval;
}

#endif
