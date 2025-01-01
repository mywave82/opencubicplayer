/* OpenCP Module Player
 * copyright (c) 2024-'25 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * unit test for filesystem-testfile-test.c
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

#define FILEHANDLE_CACHE_DISABLE
#define TEXTFILE_BUFFERSIZE 6

#include "filesystem-textfile.c"

#include "filesystem-dir-mem.h"
#include "filesystem-file-mem.h"

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

uint32_t dirdbRef (uint32_t ref, enum dirdb_use use)
{
	return ref;
}

void dirdbUnref (uint32_t ref, enum dirdb_use use)
{
}

void dirdbGetName_internalstr (uint32_t ref, const char **retval)
{
	*retval = 0;
	if (ref == 0) *retval = "test:";
	if (ref == 1) *retval = "test.txt";
}

void dirdbGetName_malloc (uint32_t ref, char **retval)
{
	const char *temp = 0;
	dirdbGetName_internalstr (ref, &temp);
	if (!temp)
	{
		*retval = 0;
		return;
	}
	*retval = strdup (temp);
}

uint32_t dirdbFindAndRef (uint32_t parent, const char *name, enum dirdb_use use)
{
	return 0;
}

const char *ocpfile_t_fill_default_filename_override (struct ocpfile_t *file)
{
	return 0;
}

int ocpfilehandle_t_fill_default_ioctl (struct ocpfilehandle_t *s, const char *cmd, void *ptr)
{
	return -1;
}

const char *ocpfilehandle_t_fill_default_filename_override (struct ocpfilehandle_t *fh)
{
	return 0;
}

static int textfile_test (const char *title, char *src, const long srclen, const char **dst, const long dstlen)
{
	struct ocpdir_t *root_dir;
	struct ocpfile_t *src_file;
	struct ocpfilehandle_t *src_filehandle; // the handle for bzip2'd decompressed file
	struct textfile_t *textfile;

	const char *line;
	int lines = 0;
	int retval = 0;

	printf ("%s: ", title);

	root_dir = ocpdir_mem_getdir_t(ocpdir_mem_alloc (0, "test:"));
	src_file = mem_file_open (root_dir, /* dirdb_ref */ 1, src, srclen);
	root_dir->unref (root_dir); root_dir = 0;

	src_filehandle = src_file->open (src_file);
	src_file->unref (src_file); src_file = 0;

	textfile = textfile_start (src_filehandle);
	src_filehandle->unref (src_filehandle); src_filehandle = 0;

	while ((line = textfile_fgets (textfile)))
	{
		lines++;
		if (lines > dstlen)
		{
			retval++;
			printf ("\n" ANSI_COLOR_RED " extra line of data detected");
			continue;
		}
		if (strcmp (line, dst[lines-1]))
		{
			retval++;
			printf ("\n" ANSI_COLOR_RED " line missmatch at line %d ->%s<- ->%s<-", lines, dst[lines-1], line);
		}
	}

	if (lines < dstlen)
	{
		retval++;
		printf ("\n" ANSI_COLOR_RED " too few lines (%d)", lines);
	}

	if (retval)
	{
		printf (ANSI_COLOR_RESET "\n");
	} else {
		printf (ANSI_COLOR_GREEN "OK" ANSI_COLOR_RESET "\n");
	}
	return retval;
}

static int textfile_test1 (void)
{
	char src[] = /* can not be const!!! */
	{
		'o', 'n', 'e', '\n',
		't', 'w', 'o', '\n',
		'f', 'o', 'u', 'r', '\n',
		'f', 'i', 'v', 'e', '\n',
		'\n',
		't', 'e', 's', 't', '\n'
	};
	const char *lines[] = {
		"one",
		"two",
		"four",
		"five",
		"",
		"test"
	};
	return textfile_test ("\\n new-lines", src, sizeof (src), lines, sizeof(lines) / sizeof(lines[0]));
}

static int textfile_test2 (void)
{
	char src[] = /* can not be const!!! */
	{
		'o', 'n', 'e', '\r',
		't', 'w', 'o', '\r',
		'f', 'o', 'u', 'r', '\r',
		'f', 'i', 'v', 'e', '\r',
		'\r',
		't', 'e', 's', 't', '\r'
	};
	const char *lines[] = {
		"one",
		"two",
		"four",
		"five",
		"",
		"test"
	};
	return textfile_test ("\\r new-lines", src, sizeof (src), lines, sizeof(lines) / sizeof(lines[0]));
}

static int textfile_test3 (void)
{
	char src[] = /* can not be const!!! */
	{
		'o', 'n', 'e', '\r', '\n',
		't', 'w', 'o', '\r', '\n',
		'f', 'o', 'u', 'r', '\r', '\n',
		'f', 'i', 'v', 'e', '\r', '\n',
		'\r', '\n',
		't', 'e', 's', 't', '\r', '\n'
	};
	const char *lines[] = {
		"one",
		"two",
		"four",
		"five",
		"",
		"test"
	};
	return textfile_test ("\\r\\n new-lines", src, sizeof (src), lines, sizeof(lines) / sizeof(lines[0]));
}

static int textfile_test4 (void)
{
	char src[] = /* can not be const!!! */
	{
		'o', 'n', 'e', '\n', '\r',
		't', 'w', 'o', '\n', '\r',
		'f', 'o', 'u', 'r', '\n', '\r',
		'f', 'i', 'v', 'e', '\n', '\r',
		'\n', '\r',
		't', 'e', 's', 't', '\n', '\r',
	};
	const char *lines[] = {
		"one",
		"two",
		"four",
		"five",
		"",
		"test"
	};
	return textfile_test ("\\n\\r new-lines", src, sizeof (src), lines, sizeof(lines) / sizeof(lines[0]));
}

static int textfile_test5 (void)
{
	char src[] = /* can not be const!!! */
	{
		'o', 'n', 'e', '\n',
		't', 'w', 'o', '\n',
		't', 'h', 'r', 'e', 'e', '\n', // max line-length is TEXTFILE_BUFFERSIZE-2, so 5 characters is one too big
		'f', 'i', 'v', 'e', '\n',
	};
	const char *lines[] = {
		"one",
		"two"
	};
	return textfile_test ("too long line stops parsing", src, sizeof (src), lines, sizeof(lines) / sizeof(lines[0]));
}


static int textfile_test6 (void)
{
	char src[] = /* can not be const!!! */
	{
		'o', 'n', 'e', '\n',
		't', 'w', 'o', '\n',
		'f', 'o', 'u', 'r', '\n',
		'f', 'i', 'v', 'e', '\n',
		'\n',
		't', 'e', 's', 't'
	};
	const char *lines[] = {
		"one",
		"two",
		"four",
		"five",
		"",
		"test"
	};
	return textfile_test ("missing \\n on the last line", src, sizeof (src), lines, sizeof(lines) / sizeof(lines[0]));
}

static int textfile_test7 (void)
{
	char src[] = /* can not be const!!! */
	{
		'o', 'n', 'e', '\n',
		't', 'w', 'o', '\r',
		'f', 'o', 'u', 'r', '\n', '\r',
		'f', 'i', 'v', 'e', '\r', '\n',
		'\n',
		't', 'e', 's', 't', '\n'
	};
	const char *lines[] = {
		"one",
		"two",
		"four",
		"five",
		"",
		"test"
	};
	return textfile_test ("mixed newlines", src, sizeof (src), lines, sizeof(lines) / sizeof(lines[0]));
}

static int textfile_test8 (void)
{
	char src[] = /* can not be const!!! */
	{
		'o', 'n', 'e', '\n', 'T', '\n',
		't', 'w', 'o', '\n'
	};
	const char *lines[] = {
		"one",
		"T",
		"two"
	};
	return textfile_test ("perfect buffer-fill + more data", src, sizeof (src), lines, sizeof(lines) / sizeof(lines[0]));
}

static int textfile_test9 (void)
{
	char src[] = /* can not be const!!! */
	{
		'o', 'n', 'e', '\n', 'T', '\n',
	};
	const char *lines[] = {
		"one",
		"T",
	};
	return textfile_test ("perfect buffer-fill", src, sizeof (src), lines, sizeof(lines) / sizeof(lines[0]));
}

static int textfile_test10 (void)
{
	char src[] = /* can not be const!!! */
	{
		'o', 'n', '\r', '\n', 'T', '\r',
		'\n', 'f', 'o', 'o', '\r', '\n',
	};
	const char *lines[] = {
		"on",
		"T",
		"foo",
	};
	return textfile_test ("\\r\\n unaligned buffer", src, sizeof (src), lines, sizeof(lines) / sizeof(lines[0]));
}

int main(int argc, char *argv[])
{
	int retval = 0;

	printf ( ANSI_COLOR_CYAN "Testing textfile" ANSI_COLOR_RESET "\n");
	retval |= textfile_test1 ();
	retval |= textfile_test2 ();
	retval |= textfile_test3 ();
	retval |= textfile_test4 ();
	retval |= textfile_test5 ();
	retval |= textfile_test6 ();
	retval |= textfile_test7 ();
	retval |= textfile_test8 ();
	retval |= textfile_test9 ();
	retval |= textfile_test10 ();

	return retval;
}
