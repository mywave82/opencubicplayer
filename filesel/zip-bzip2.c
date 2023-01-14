/* OpenCP Module Player
 * copyright (c) 2021-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Code to decompress ZIP bz2ip method using libbz2
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

#include <stdint.h>
#include <string.h>
#include <bzlib.h>

struct zip_bzip2_t
{
	char     out_buffer[65536];
	/* reset after each call to digest and feed */
	uint8_t *out_buffer_readnext;
	int      out_buffer_fill;

	int need_deinit;
	int eof_hit;
	bz_stream strm;
};

static int zip_bzip2_init (struct zip_bzip2_t *self)
{
	bzero (&self->strm, sizeof (self->strm));
	if (BZ2_bzDecompressInit (&self->strm, 0 /* no verbosity */, 0 /* do not use the small decompression routine */))
	{
		self->eof_hit = 1;
		self->need_deinit = 0;
		return -1;
	}
	self->need_deinit = 1;
	self->eof_hit = 0;
	self->out_buffer_fill = 0;

	return 0;
}

static void zip_bzip2_done (struct zip_bzip2_t *self)
{
	if (self->need_deinit)
	{
		BZ2_bzDecompressEnd (&self->strm);
		self->need_deinit = 0;
	}
}

/* call this when readnext is exhausted */
static int64_t zip_bzip2_digest(struct zip_bzip2_t *self)
{
	if (self->eof_hit)
	{
		return -1;
	}
	if (self->strm.avail_in)
	{
		int res;

		self->out_buffer_readnext = (uint8_t *)self->out_buffer;
		self->strm.next_out = self->out_buffer;
		self->strm.avail_out = sizeof (self->out_buffer);

		res = BZ2_bzDecompress (&self->strm);
		if (res == BZ_STREAM_END)
		{
			self->eof_hit = 1;
			self->out_buffer_fill = self->strm.next_out - self->out_buffer;
			return self->out_buffer_fill;
		}
		if (res == BZ_OK)
		{
			self->out_buffer_fill = self->strm.next_out - self->out_buffer;
			return self->out_buffer_fill;
		}
		self->eof_hit = 1; /* we treat all errors as EOF */
		self->out_buffer_fill = 0;
		return -1;
	}
	return 0;
}

/* call this when both readnext is exhausted, and digest above yielded no new data */
static int zip_bzip2_feed (struct zip_bzip2_t *self, uint8_t *src, uint32_t len)
{
	int res;

	if (self->eof_hit)
	{
		return -1;
	}

	self->out_buffer_readnext = (uint8_t *)self->out_buffer;
	self->strm.next_in = (char *)src;
	self->strm.avail_in = len;
	self->strm.next_out = self->out_buffer;
	self->strm.avail_out = sizeof (self->out_buffer);

	res = BZ2_bzDecompress (&self->strm);
	if (res == BZ_STREAM_END)
	{
		self->eof_hit = 1;
		self->out_buffer_fill = self->strm.next_out - self->out_buffer;
		return self->out_buffer_fill;
	}
	if (res == BZ_OK)
	{
		self->out_buffer_fill = self->strm.next_out - self->out_buffer;
		return self->out_buffer_fill;
	}
	self->eof_hit = 1; /* we treat all errors as EOF */
	self->out_buffer_fill = 0;
	return -1;
}
