/* OpenCP Module Player
 * copyright (c) 2020-'21 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Base code to handle filesystems
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
#include "types.h"
#include "filesystem.h"

#define MAX_FILEDECOMPRESSORS 16
#define MAX_DIRDECOMPRESSORS 16

const struct ocpdirdecompressor_t *ocpdirdecompressor[MAX_DIRDECOMPRESSORS];
int ocpdirdecompressors;

void register_dirdecompressor(const struct ocpdirdecompressor_t *e)
{
	int i;

	if (ocpdirdecompressors >= MAX_DIRDECOMPRESSORS)
	{
		fprintf (stderr, "[filesystem] Too many dirdecompressors, unable to add %s\n", e->name);
		return;
	}

	for (i=0; i < ocpdirdecompressors; i++)
	{
		if (ocpdirdecompressor[i] == e)
		{
			return;
		}
	}
	ocpdirdecompressor[ocpdirdecompressors++] = e;
}

struct ocpdir_t *ocpdirdecompressor_check (struct ocpfile_t *f, const char *filetype)
{
	int i;
	for (i=0; i < ocpdirdecompressors; i++)
	{
		struct ocpdir_t *r = ocpdirdecompressor[i]->check(ocpdirdecompressor[i], f, filetype);
		if (r)
		{
			return r;
		}
	}
	return 0;
}

/* returns 0 or the number of bytes read */
int ocpfilehandle_read_uint8 (struct ocpfilehandle_t *s, uint8_t *dst)
{
	if (s->read (s, dst, 1) != 1) return -1;
	return 0;
}

/* returns 0 for OK, and -1 on error */
int ocpfilehandle_read_uint16_be (struct ocpfilehandle_t *s, uint16_t *dst)
{
	if (s->read (s, dst, 2) != 2) return -1;
	*dst = uint16_big (*dst);
	return 0;
}

/* returns 0 for OK, and -1 on error */
int ocpfilehandle_read_uint32_be (struct ocpfilehandle_t *s, uint32_t *dst)
{
	if (s->read (s, dst, 4) != 4) return -1;
	*dst = uint32_big (*dst);
	return 0;
}

/* returns 0 for OK, and -1 on error */
int ocpfilehandle_read_uint64_be (struct ocpfilehandle_t *s, uint64_t *dst)
{
	if (s->read (s, dst, 8) != 8) return -1;
	*dst = uint64_big (*dst);
	return 0;
}

/* returns 0 for OK, and -1 on error */
int ocpfilehandle_read_uint16_le (struct ocpfilehandle_t *s, uint16_t *dst)
{
	if (s->read (s, dst, 2) != 2) return -1;
	*dst = uint16_little (*dst);
	return 0;
}

/* returns 0 for OK, and -1 on error */
int ocpfilehandle_read_uint32_le (struct ocpfilehandle_t *s, uint32_t *dst)
{
	if (s->read (s, dst, 4) != 4) return -1;
	*dst = uint32_little (*dst);
	return 0;
}

/* returns 0 for OK, and -1 on error */
int ocpfilehandle_read_uint64_le (struct ocpfilehandle_t *s, uint64_t *dst)
{
	if (s->read (s, dst, 8) != 8) return -1;
	*dst = uint64_little (*dst);
	return 0;
}

/* returns 0 for OK, and -1 on error */
int ocpfilehandle_read_uint24_be (struct ocpfilehandle_t *s, uint32_t *dst)
{
	uint8_t t[3];
	if (s->read (s, t, 3) != 3) return -1;
	*dst = (t[0] << 16) | (t[1]<<8) | t[2];
	return 0;
}

/* returns 0 for OK, and -1 on error */
int ocpfilehandle_read_uint24_le (struct ocpfilehandle_t *s, uint32_t *dst)
{
	uint8_t t[3];
	if (s->read (s, t, 3) != 3) return -1;
	*dst = (t[2] << 16) | (t[1]<<8) | t[0];
	return 0;
}

struct ocpdir_t_fill_default_readdir_dir_t // help struct for ocpdir_t_fill_default_readdir_dir
{
	uint32_t dirdb_ref;
	struct ocpdir_t *retval;
};

static void ocpdir_t_fill_default_readdir_dir_file (void *_token, struct ocpfile_t *file) // helper function for ocpdir_t_fill_default_readdir_dir
{
}

static void ocpdir_t_fill_default_readdir_dir_dir (void *_token, struct ocpdir_t *dir) // helper function for ocpdir_t_fill_default_readdir_dir
{
	struct ocpdir_t_fill_default_readdir_dir_t *token = _token;
	if (token->dirdb_ref == dir->dirdb_ref)
	{
		if (token->retval)
		{
			token->retval->unref (token->retval);
		}
		dir->ref (dir);
		token->retval = dir;
	}
}

struct ocpdir_t *ocpdir_t_fill_default_readdir_dir  (struct ocpdir_t *_self, uint32_t dirdb_ref)
{
	ocpdirhandle_pt handle;
	struct ocpdir_t_fill_default_readdir_dir_t token;

	token.dirdb_ref = dirdb_ref;
	token.retval = 0;

	handle = _self->readdir_start (_self, ocpdir_t_fill_default_readdir_dir_file, ocpdir_t_fill_default_readdir_dir_dir, &token);
	if (!handle)
	{
		return 0;
	}

	while (_self->readdir_iterate (handle))
	{
	};
	_self->readdir_cancel (handle);

	return token.retval;
}

struct ocpdir_t_fill_default_readdir_file_t // helper struct for ocpdir_t_fill_default_readdir_file
{
	uint32_t dirdb_ref;
	struct ocpfile_t *retval;
};

static void ocpdir_t_fill_default_readdir_file_file (void *_token, struct ocpfile_t *file) // helper function for ocpdir_t_fill_default_readdir_file
{
	struct ocpdir_t_fill_default_readdir_file_t *token = _token;
	if (token->dirdb_ref == file->dirdb_ref)
	{
		if (token->retval)
		{
			token->retval->unref (token->retval);
		}
		file->ref (file);
		token->retval = file;
	}
}

static void ocpdir_t_fill_default_readdir_file_dir (void *_token, struct ocpdir_t *dir) // helper function for ocpdir_t_fill_default_readdir_file
{
}

struct ocpfile_t *ocpdir_t_fill_default_readdir_file (struct ocpdir_t *_self, uint32_t dirdb_ref)
{
	ocpdirhandle_pt handle;
	struct ocpdir_t_fill_default_readdir_file_t token;

	token.dirdb_ref = dirdb_ref;
	token.retval = 0;

	handle = _self->readdir_start (_self, ocpdir_t_fill_default_readdir_file_file, ocpdir_t_fill_default_readdir_file_dir, &token);
	if (!handle)
	{
		return 0;
	}

	while (_self->readdir_iterate (handle))
	{
	};
	_self->readdir_cancel (handle);

	return token.retval;
}

int ocpfilehandle_t_fill_default_ioctl (struct ocpfilehandle_t *s, const char *cmd, void *ptr)
{
	return -1;
}
