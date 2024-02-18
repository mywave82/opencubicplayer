/* OpenCP Module Player
 * copyright (c) 2020-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Code to decompress ZIP deflate method using zLib
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
#include <zlib.h>

struct zip_inflate_t
{
	Bytef out_buffer[65536];
	/* reset after each call to digest and feed */
	uint8_t *out_buffer_readnext;
	int      out_buffer_fill;

	int need_deinit;
	int eof_hit;
	z_stream strm;
};

static int zip_inflate_init (struct zip_inflate_t *self)
{
	memset (&self->strm, 0, sizeof (self->strm));
	if (inflateInit2 (&self->strm, -15))
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

static void zip_inflate_done (struct zip_inflate_t *self)
{
	if (self->need_deinit)
	{
		inflateEnd (&self->strm);
		self->need_deinit = 0;
	}
}

/* call this when readnext is exhausted */
static int64_t zip_inflate_digest(struct zip_inflate_t *self)
{
	if (self->eof_hit)
	{
		return -1;
	}
	if (self->strm.avail_in)
	{
		int res;

		self->out_buffer_readnext = self->out_buffer;
		self->strm.next_out = self->out_buffer;
		self->strm.avail_out = sizeof (self->out_buffer);

		res = inflate (&self->strm, Z_SYNC_FLUSH);
		if (res == Z_STREAM_END)
		{
			self->eof_hit = 1;
			self->out_buffer_fill = self->strm.next_out - self->out_buffer;
			return self->out_buffer_fill;
		}
		if (res == Z_OK)
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
static int zip_inflate_feed (struct zip_inflate_t *self, uint8_t *src, uint32_t len)
{
	int res;

	if (self->eof_hit)
	{
		return -1;
	}

	self->out_buffer_readnext = self->out_buffer;
	self->strm.next_in = src;
	self->strm.avail_in = len;
	self->strm.next_out = self->out_buffer;
	self->strm.avail_out = sizeof (self->out_buffer);

	res = inflate (&self->strm, Z_SYNC_FLUSH);
	if (res == Z_STREAM_END)
	{
		self->eof_hit = 1;
		self->out_buffer_fill = self->strm.next_out - self->out_buffer;
		return self->out_buffer_fill;
	}
	if (res == Z_OK)
	{
		self->out_buffer_fill = self->strm.next_out - self->out_buffer;
		return self->out_buffer_fill;
	}
	self->eof_hit = 1; /* we treat all errors as EOF */
	self->out_buffer_fill = 0;
	return -1;
}
