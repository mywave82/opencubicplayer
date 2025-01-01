/* OpenCP Module Player
 * copyright (c) 2024-'25 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * For a ocpfilehandle_t, implement readline wrapper-API
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "types.h"

#include "dirdb.h"
#include "filesystem.h"
#include "filesystem-textfile.h"

#ifndef TEXTFILE_BUFFERSIZE
# define TEXTFILE_BUFFERSIZE 1024
#endif

struct textfile_t
{
	struct ocpfilehandle_t *filehandle;
	char buffer[TEXTFILE_BUFFERSIZE];
	unsigned next;
	unsigned int fill;
	uint64_t remaining;
};

struct textfile_t *textfile_start (struct ocpfilehandle_t *filehandle)
{
	struct textfile_t *self;

	if (!filehandle)
	{
		return 0;
	}

	self = calloc (sizeof (*self), 1);
	if (!self)
	{
		return 0;
	}
	self->filehandle = filehandle;
	self->filehandle->ref (self->filehandle);
	self->remaining = filehandle->filesize (filehandle);

	return self;
}

void textfile_stop (struct textfile_t *self)
{
	if (!self)
	{
		return;
	}
	self->filehandle->unref (self->filehandle);
	self->filehandle = 0;
	free (self);
}

const char *textfile_fgets (struct textfile_t *self)
{
	unsigned int i;
	if (!self)
	{
		return 0;
	}

again:
	if (self->next == self->fill)
	{
		/* EOF? */
		if (self->remaining == 0)
		{
			return 0;
		}
		/* the next character perfectly aligned with end of buffer, also safe to hit if next==fill==0 */
		self->next = 0;
		self->fill = 0;
	}

	/* fill up the buffer if needed */
	if ((self->next == 0) &&
	    (self->remaining != 0) &&
	    (self->fill != TEXTFILE_BUFFERSIZE))
	{
		int request = TEXTFILE_BUFFERSIZE - self->fill;
		int result;
		if (request > self->remaining)
		{
			request = self->remaining;
		}
		result = self->filehandle->read (self->filehandle, self->buffer + self->fill, request);
		if (result != request)
		{ /* premature EOF */
			self->remaining = 0;
		} else {
			self->remaining -= result;
		}
		self->fill += result;
	}

	/* search for new-line */
	for (i = self->next; i < self->fill; i++)
	{
		/* if we are at the second to last character, and the line did not start at zero, retrim and restart... */
		if (i >= (TEXTFILE_BUFFERSIZE - 1))
		{
			if (self->next)
			{
				memmove (self->buffer, self->buffer + self->next, self->fill - self->next);
				self->fill -= self->next;
				self->next = 0;
				goto again;
			}
			/* line is too long, and we can't safely detect \r\n or \n\r */
			return 0;
		}

		if ((self->buffer[i] == '\n') || (self->buffer[i] == '\r'))
		{
			unsigned int j = self->next;
			if ((i != (self->fill - 1)) &&                                      /* there is more data available ? */
			    ((self->buffer[i+1] == '\n') || (self->buffer[i+1] == '\r')) && /* next character is also a new-line thing */
			    (self->buffer[i] != self->buffer[i+1]))                         /* but not the same */
			{ /* two character new-line, \r\n or \n\r */
				self->next = i + 2;
			} else {
				self->next = i + 1;
			}
			self->buffer[i] = 0;
			return self->buffer + j;
		}
	}

	/* remaining data does not contain a new-line */
	i = self->next;
	self->next = self->fill;
	self->buffer[self->fill] = 0;

	return self->buffer + i;
}
