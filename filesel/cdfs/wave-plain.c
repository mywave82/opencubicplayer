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

struct cdfs_datasource_handle_plain_t
{
	struct cdfs_datasource_handle_t h;
	int refcount;
	struct ocpfilehandle_t *fh;
	uint64_t offset;
};

static void audiofile_handle_plain_ref (struct cdfs_datasource_handle_t *_ah)
{
	struct cdfs_datasource_handle_plain_t *ah = (struct cdfs_datasource_handle_plain_t *)_ah;
	ah->refcount++;
}

static void audiofile_handle_plain_unref (struct cdfs_datasource_handle_t *_ah)
{
	struct cdfs_datasource_handle_plain_t *ah = (struct cdfs_datasource_handle_plain_t *)_ah;
	ah->refcount--;
	if (ah->refcount)
	{
		return;
	}
	ah->fh->unref (ah->fh);
	free (ah);
}

static unsigned int audiofile_handle_plain_read (struct cdfs_datasource_handle_t *_ah, uint64_t offset, uint8_t *target, unsigned int len)
{
	struct cdfs_datasource_handle_plain_t *ah = (struct cdfs_datasource_handle_plain_t *)_ah;
	uint64_t retval;
	ah->fh->seek_set (ah->fh, offset + ah->offset);
	retval = ah->fh->read (ah->fh, target, len);
	if (retval < len)
	{
		memset (target + retval, 0, len - retval);
	}
	return retval;
}

OCP_INTERNAL struct cdfs_datasource_handle_t *spawn_audiofile_handle_plain (struct ocpfilehandle_t *fh, uint64_t offset, uint64_t length)
{
	struct cdfs_datasource_handle_plain_t *retval = malloc (sizeof (*retval));
	if (!retval)
	{
		fprintf (stderr, "spawn_audiofile_handle_plain: malloc() failed\n");
		return 0;
	}
	retval->h.dirdb_ref = fh->dirdb_ref; // we ref the fh itself, so no need to ref this
	retval->h.ref = audiofile_handle_plain_ref;
	retval->h.unref = audiofile_handle_plain_unref;
	retval->h.length = length;
	retval->h.read = audiofile_handle_plain_read;
	retval->refcount = 1;
	fh->ref (fh);
	retval->fh = fh;
	retval->offset = offset;
	return &retval->h;
}
