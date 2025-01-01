/* OpenCP Module Player
 * copyright (c) 2021-'25 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * unit test for filesystem-filehandle-cache.c
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

int do_debug_print = 0;

#define CACHE_LINE_SIZE 8

struct cache_ocpfilehandle_t;

#include "filesystem-filehandle-cache.c"

#include <stdlib.h>
#include <unistd.h>

#include "dirdb.h"

uint32_t dirdbFindAndRef (uint32_t parent, const char *name, enum dirdb_use use)
{
	return 0;
}

const char *ocpfilehandle_t_fill_default_filename_override (struct ocpfilehandle_t *fh)
{
	return 0;
}

static uint32_t dirdbCounters[3] = {0};
static int dirdbError;

uint32_t dirdbRef (uint32_t ref, enum dirdb_use use)
{
	if ((ref < 1) || (ref > 2))
	{
		fprintf (stderr, "dirdbRef(ref=0x%08" PRIx32 ") invalid\n", ref);
		dirdbError++;
		return UINT32_MAX;
	}
	if (!dirdbCounters[ref])
	{
		fprintf (stderr, "dirdbRef(ref=0x%08" PRIx32 ") counter is zero\n", ref);
		dirdbError++;
		return UINT32_MAX;
	}
	dirdbCounters[ref]++;
	return ref;
}

void dirdbUnref (uint32_t ref, enum dirdb_use use)
{
	if ((ref < 1) || (ref > 2))
	{
		fprintf (stderr, "dirdbUnref(ref=0x%08" PRIx32 ") invalid\n", ref);
		dirdbError++;
		return;
	}
	if (!dirdbCounters[ref])
	{
		fprintf (stderr, "dirdbUnref(ref=0x%08" PRIx32 ") counter is zero\n", ref);
		dirdbError++;
		return;
	}
	dirdbCounters[ref]--;
}

static void dirdbPrepare (void)
{
	dirdbError = 0;
	dirdbCounters[1] = 1;
	dirdbCounters[2] = 1;
}

static void dirdbValidate (void)
{
	dirdbError += dirdbCounters[1] != 1;
	dirdbError += dirdbCounters[2] != 1;
}

struct ocpdir_t *ocpdir_t_fill_default_readdir_dir  (struct ocpdir_t *_self, uint32_t dirdb_ref)
{
	fprintf (stderr, "Dummy symbol ocpdir_t_fill_default_readdir_dir called?\n");
	_exit(1);
}

struct ocpfile_t *ocpdir_t_fill_default_readdir_file (struct ocpdir_t *_self, uint32_t dirdb_ref)
{
	fprintf (stderr, "Dummy symbol ocpdir_t_fill_default_readdir_file called?\n");
	_exit(1);
}

const char *ocpfile_t_fill_default_filename_override (struct ocpfile_t *file)
{
	return 0;
}

int ocpfilehandle_t_fill_default_ioctl (struct ocpfilehandle_t *s, const char *cmd, void *ptr)
{
	return -1;
}

struct cache_ocpfile_test_t
{
	struct ocpfile_t head;
	int filesize_ready;
	uint64_t filesize;
	const uint8_t *data;

	int errors;
};

struct cache_ocpfilehandle_test_t
{
	struct ocpfilehandle_t head;
	uint64_t pos;
	uint32_t *data_accesses;
	uint32_t reads;
	uint32_t seeks;
	uint32_t errors;
};

static void file_test_ref (struct ocpfile_t *_f)
{
	struct cache_ocpfile_test_t *f = (struct cache_ocpfile_test_t *)_f;
	f->head.refcount++;
}

static void file_test_unref (struct ocpfile_t *_f)
{
	struct cache_ocpfile_test_t *f = (struct cache_ocpfile_test_t *)_f;
	if (!f->head.refcount)
	{
		fprintf (stderr, "file->unref() called too many times\n");
		f->errors++;
	}
	f->head.refcount--;
}

static uint64_t file_test_filesize (struct ocpfile_t *_f)
{
	struct cache_ocpfile_test_t *f = (struct cache_ocpfile_test_t *)_f;
	if (f->filesize_ready > 1)
	{
		fprintf (stderr, "file->filesize(): Asking for filesize() when filesize_ready is not given\n");
		f->errors++;
	}
	f->filesize_ready = 1;
	return f->filesize;
}

static int file_test_filesize_ready (struct ocpfile_t *_f)
{
	struct cache_ocpfile_test_t *f = (struct cache_ocpfile_test_t *)_f;
	return f->filesize_ready == 1;
}

static void filehandle_test_ref (struct ocpfilehandle_t *_f)
{
	struct cache_ocpfilehandle_test_t *f = (struct cache_ocpfilehandle_test_t *)_f;
	f->head.refcount++;
}

static void filehandle_test_unref (struct ocpfilehandle_t *_f)
{
	struct cache_ocpfilehandle_test_t *f = (struct cache_ocpfilehandle_test_t *)_f;
	if (!f->head.refcount)
	{
		fprintf (stderr, "filehandle->unref() called too many times\n");
		f->errors++;
	}
	f->head.refcount--;
}

static int filehandle_test_seek_set (struct ocpfilehandle_t *_f, int64_t pos)
{
	struct cache_ocpfilehandle_test_t *f = (struct cache_ocpfilehandle_test_t *)_f;

	if (pos < 0)
	{
		fprintf (stderr, "filehandle->seek_set() negative position\n");
		return -1;
	}
	if (pos > ((struct cache_ocpfile_test_t *)f->head.origin)->filesize)
	{
		fprintf (stderr, "filehandle->seek_set() position too large\n");
		return -1;

	}
	f->pos = pos;
	f->seeks++;
	return 0;
}

static uint64_t filehandle_test_getpos (struct ocpfilehandle_t *_f)
{
	struct cache_ocpfilehandle_test_t *f = (struct cache_ocpfilehandle_test_t *)_f;
	return f->pos;
}

static int filehandle_test_eof (struct ocpfilehandle_t *_f)
{
	struct cache_ocpfilehandle_test_t *f = (struct cache_ocpfilehandle_test_t *)_f;
	return f->pos >= f->head.origin->filesize (f->head.origin);
}

static int filehandle_test_error (struct ocpfilehandle_t *_f)
{
	/* struct cache_ocpfilehandle_test_t *f = (struct cache_ocpfilehandle_test_t *)_f; */
	return 0;
}

static int filehandle_test_read (struct ocpfilehandle_t *_f, void *dst, int len)
{
	struct cache_ocpfilehandle_test_t *f = (struct cache_ocpfilehandle_test_t *)_f;
	uint64_t maxpos = ((struct cache_ocpfile_test_t *)f->head.origin)->filesize;
	uint32_t u;

	if (len < 0)
	{
		fprintf (stderr, "filehandle_read() negative length\n");
		f->errors++;
		return 0;
	}

	f->reads++;
	if (f->pos >= maxpos)
	{
		((struct cache_ocpfile_test_t *)f->head.origin)->filesize_ready = 1;
		return 0;
	}
	if ((f->pos + len) > maxpos)
	{
		((struct cache_ocpfile_test_t *)f->head.origin)->filesize_ready = 1;
		len = maxpos - f->pos;
	}

	for (u=0; u < len; u++)
	{
		f->data_accesses[u + f->pos]++;
	}

	memcpy (dst, ((struct cache_ocpfile_test_t *)(f->head.origin))->data + f->pos, len);

	f->pos += len;

	return len;
}

static uint64_t filehandle_test_filesize (struct ocpfilehandle_t *_f)
{
	struct cache_ocpfilehandle_test_t *f = (struct cache_ocpfilehandle_test_t *)_f;
	return f->head.origin->filesize (f->head.origin);
}

static int filehandle_test_filesize_ready (struct ocpfilehandle_t *_f)
{
	struct cache_ocpfilehandle_test_t *f = (struct cache_ocpfilehandle_test_t *)_f;
	return f->head.origin->filesize_ready (f->head.origin);
}

static int dotest (const uint64_t filesize,
                   const int filesize_ready,
                   const uint8_t *filedata,
                   int (*exercise)(const uint64_t filesize, const uint8_t *filedata, struct ocpfilehandle_t *h),
                   int (*validate)(const uint64_t filesize, const uint8_t *filedata, const uint32_t *dataaccessed, const struct cache_ocpfilehandle_test_t *h))
{
	int retval = 0;
	struct cache_ocpfile_test_t       file_test = {{0}};
	struct cache_ocpfilehandle_test_t filehandle_test = {{0}};
	struct ocpfilehandle_t            *cachehandle;

	dirdbPrepare();

	ocpfile_t_fill (
		&file_test.head,
		file_test_ref,
		file_test_unref,
		0, /* parent */
		0, /* open() */
		file_test_filesize,
		file_test_filesize_ready,
		0, /* filename_override() */
		1, /* dirdb_ref */
		1, /* refcount */
		0,  /* is_nodetect */
		COMPRESSION_NONE
	);
	file_test.filesize = filesize;
	file_test.filesize_ready = filesize_ready;
	file_test.data = filedata;

	ocpfilehandle_t_fill (
		&filehandle_test.head,
		filehandle_test_ref,
		filehandle_test_unref,
		&file_test.head,
		filehandle_test_seek_set,
		filehandle_test_getpos,
		filehandle_test_eof,
		filehandle_test_error,
		filehandle_test_read,
		0, /* ioctl() */
		filehandle_test_filesize,
		filehandle_test_filesize_ready,
		0, /* filename_override() */
		2, /* dirdb_ref */
		1 /* refcount */
	);
	filehandle_test.pos = 0;
	filehandle_test.data_accesses = filesize ? calloc (filesize, sizeof (uint32_t)) : 0;
	filehandle_test.reads = 0;
	filehandle_test.seeks = 0;

	cachehandle = cache_filehandle_open (&filehandle_test.head);

	retval += exercise (filesize, filedata, cachehandle);
	retval += validate (filesize, filedata, filehandle_test.data_accesses, &filehandle_test);
	cachehandle->unref (cachehandle);
	cachehandle = 0;

	filehandle_test.head.unref (&filehandle_test.head);
	file_test.head.unref (&file_test.head);

	if (filehandle_test.head.refcount) { fprintf (stderr, "filehandle refcount non-zero (%d)\n", filehandle_test.head.refcount); retval++; }
	if (file_test.head.refcount) { fprintf (stderr, "file refcount non-zero (%d)\n", file_test.head.refcount); retval++; }
	dirdbValidate();
	retval += dirdbError;
	retval += file_test.errors;
	retval += filehandle_test.errors;

	free (filehandle_test.data_accesses);

	return retval;
}

static const uint8_t buf256[256] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
                                    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
                                    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
                                    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
                                    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
                                    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
                                    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
                                    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
                                    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x89, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
                                    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
                                    0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xad, 0xad, 0xae, 0xaf,
                                    0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
                                    0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
                                    0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xdf, 0xdf,
                                    0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
                                    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff};

static int exercise_zerosize (const uint64_t filesize, const uint8_t *filedata, struct ocpfilehandle_t *h)
{
	int retval = 0, result;
	uint8_t buffer[16];
	if (h->seek_set (h, 0))
	{
		fprintf (stderr, "exercise_zerosize: seek_set(0) failed\n");
		retval++;
	}
	memset (buffer, 0, sizeof (buffer));
	result = h->read (h, buffer, 16);
	if (result != 0)
	{
		fprintf (stderr, "exercise_zerosize: read(16) offset=0 failed, got %d\n", result);
		retval++;
	}
	if (!h->seek_set (h, 1))
	{
		fprintf (stderr, "exercise_zerosize: seek_set(1) did not fail\n");
		retval++;
	}
	return retval;
}

static int validate_zerosize (const uint64_t filesize, const uint8_t *filedata, const uint32_t *dataaccessed, const struct cache_ocpfilehandle_test_t *h)
{
	return 0;
}

static int test1_zerosize_file (void)
{
	int retval = 0;
	fprintf (stderr, "test1: zero-size file ");
	retval += dotest (0, 1, buf256, exercise_zerosize, validate_zerosize);
	fprintf (stderr, "\n\n");
	return retval;
}

static int exercise_entirefile_random (const uint64_t filesize, const uint8_t *filedata, struct ocpfilehandle_t *h)
{
	int retval = 0;
	int sizetoread, offsetfrom, result;
	uint8_t buffer[filesize];

	int reads = 0;

	for (sizetoread = 1; sizetoread <= filesize; sizetoread++)
	{
		for (offsetfrom = 0; offsetfrom + sizetoread <= filesize; offsetfrom++)
		{
			if (h->seek_set (h, offsetfrom))
			{
				fprintf (stderr, "exercise_entirefile_random: seek_set(%d) failed\n", offsetfrom);
				retval++;
				continue;
			}
			memset (buffer, 0, sizeof (buffer));
			result = h->read (h, buffer, sizetoread);
			reads++;
			if (result != sizetoread)
			{
				fprintf (stderr, "exercise_entirefile_random: read(%d) offset=%d failed, got %d\n", sizetoread, offsetfrom, result);
				retval++;
			}
			if (memcmp (buffer, filedata + offsetfrom, sizetoread))
			{
				fprintf (stderr, "exercise_entirefile_random: data read back does not match\n");
				retval++;
			}
		}
	}
	fprintf (stderr, "(reads=%d)", reads);
	return retval;
}

static int validate_entirefile_random_fitsinbuffer (const uint64_t filesize, const uint8_t *filedata, const uint32_t *dataaccessed, const struct cache_ocpfilehandle_test_t *h)
{
	int i;
	int retval = 0;
	for (i=0; i < filesize; i++)
	{
		if (dataaccessed[i] != 1)
		{
			fprintf (stderr, "validate_entirefile_random_fitsinbuffer: offset %d is %"PRId32" instead of 1\n", i, dataaccessed[i]);
			retval++;
		}
	}
	return retval;
}

static int test2_fits_in_cache_normal (void)
{
	int i;
	int retval = 0;
	fprintf (stderr, "test2: file can fit in cache (mode NORMAL)");
	for (i=1; i < (CACHE_LINE_SIZE * CACHE_LINES); i++)
	{
		fprintf (stderr, " [size=%d]", i);
		retval += dotest (i, 1, buf256, exercise_entirefile_random, validate_entirefile_random_fitsinbuffer);
	}
	fprintf (stderr, "\n\n");
	return retval;
}

static int test3_fits_in_cache_stream (void)
{
	int i;
	int retval = 0;
	fprintf (stderr, "test3: file can fit in cache (mode STREAM)");
	for (i=1; i < (CACHE_LINE_SIZE * CACHE_LINES); i++)
	{
		fprintf (stderr, " [size=%d]", i);
		retval += dotest (i, 2, buf256, exercise_entirefile_random, validate_entirefile_random_fitsinbuffer);
	}
	fprintf (stderr, "\n\n");
	return retval;
}

static int exercise_big_random_start_end (const uint64_t filesize, const uint8_t *filedata, struct ocpfilehandle_t *h)
{
	int retval = 0;
	int sizetoread, offsetfrom, result;
	uint8_t buffer[filesize];
	for (sizetoread = 1; sizetoread <= CACHE_LINE_SIZE; sizetoread++)
	{
		for (offsetfrom = 0; offsetfrom + sizetoread <= CACHE_LINE_SIZE; offsetfrom++)
		{
			if (h->seek_set (h, offsetfrom))
			{
				fprintf (stderr, "exercise_entirefile_random: seek_set(%d) failed\n", offsetfrom);
				retval++;
				continue;
			}
			memset (buffer, 0, sizeof (buffer));
			result = h->read (h, buffer, sizetoread);
			if (result != sizetoread)
			{
				fprintf (stderr, "exercise_entirefile_random: read(%d) offset=%d failed, got %d\n", sizetoread, offsetfrom, result);
				retval++;
			}
			if (memcmp (buffer, filedata + offsetfrom, sizetoread))
			{
				fprintf (stderr, "exercise_entirefile_random: data read back does not match\n");
				retval++;
			}
		}
	}

	for (sizetoread = 1; sizetoread <= CACHE_LINE_SIZE; sizetoread++)
	{
		for (offsetfrom = 256 - CACHE_LINE_SIZE; offsetfrom + sizetoread <= 256; offsetfrom++)
		{
			if (h->seek_set (h, offsetfrom))
			{
				fprintf (stderr, "exercise_entirefile_random: seek_set(%d) failed\n", offsetfrom);
				retval++;
				continue;
			}
			memset (buffer, 0, sizeof (buffer));
			result = h->read (h, buffer, sizetoread);
			if (result != sizetoread)
			{
				fprintf (stderr, "exercise_entirefile_random: read(%d) offset=%d failed, got %d\n", sizetoread, offsetfrom, result);
				retval++;
			}
			if (memcmp (buffer, filedata + offsetfrom, sizetoread))
			{
				fprintf (stderr, "exercise_entirefile_random: data read back does not match\n");
				retval++;
			}
		}
	}

	return retval;
}

static int validate_big_normal_random_start_end (const uint64_t filesize, const uint8_t *filedata, const uint32_t *dataaccessed, const struct cache_ocpfilehandle_test_t *h)
{
	int i;
	int retval = 0;
	for (i=0; i < filesize; i++)
	{
		int expect = (i <        CACHE_LINE_SIZE) ? 1 :
		             (i >= 256 - CACHE_LINE_SIZE) ? 1 : 0;
		if (dataaccessed[i] != expect)
		{
			fprintf (stderr, "validate_big_normal_random_start_end: offset %d is %"PRId32" instead of %d\n", i, dataaccessed[i], expect);
			retval++;
		}
	}
	return retval;
}

static int test4_big_normal_start_stop (void)
{
	int retval = 0;
	fprintf (stderr, "test4: big file, cache start and end (mode NORMAL)\n");
	retval += dotest (256, 1, buf256, exercise_big_random_start_end, validate_big_normal_random_start_end);
	fprintf (stderr, "\n");
	return retval;
}

static int validate_big_stream_random_start_end (const uint64_t filesize, const uint8_t *filedata, const uint32_t *dataaccessed, const struct cache_ocpfilehandle_test_t *h)
{
	int i;
	int retval = 0;
	for (i=0; i < filesize; i++)
	{
		int expect = 1;
		if (dataaccessed[i] != expect)
		{
			fprintf (stderr, "validate_big_stream_random_start_end: offset %d is %"PRId32" instead of %d\n", i, dataaccessed[i], expect);
			retval++;
		}
	}
	return retval;
}

static int test5_big_stream_start_stop (void)
{
	int retval = 0;
	fprintf (stderr, "test5: big file, cache start and end (mode STREAM)\n");
	retval += dotest (256, 2, buf256, exercise_big_random_start_end, validate_big_stream_random_start_end);
	fprintf (stderr, "\n");
	return retval;
}

static int exercise_entirefile_random_max8 (const uint64_t filesize, const uint8_t *filedata, struct ocpfilehandle_t *h)
{
	int retval = 0;
	int sizetoread, offsetfrom, result;
	uint8_t buffer[filesize];

	int reads = 0;

	for (sizetoread = 1; (sizetoread <= filesize) && (sizetoread <= 8); sizetoread++)
	{
		for (offsetfrom = 0; offsetfrom + sizetoread <= filesize; offsetfrom++)
		{
			if (h->seek_set (h, offsetfrom))
			{
				fprintf (stderr, "exercise_entirefile_random: seek_set(%d) failed\n", offsetfrom);
				retval++;
				continue;
			}
			memset (buffer, 0, sizeof (buffer));
			result = h->read (h, buffer, sizetoread);
			reads++;
			if (result != sizetoread)
			{
				fprintf (stderr, "exercise_entirefile_random: read(%d) offset=%d failed, got %d\n", sizetoread, offsetfrom, result);
				retval++;
			}
			if (memcmp (buffer, filedata + offsetfrom, sizetoread))
			{
				fprintf (stderr, "exercise_entirefile_random: data read back does not match\n");
				retval++;
			}
		}
	}
	fprintf (stderr, "(reads=%d)", reads);
	return retval;
}

static int validate_big_random (const uint64_t filesize, const uint8_t *filedata, const uint32_t *dataaccessed, const struct cache_ocpfilehandle_test_t *h)
{
	int i;
	int retval = 0;
	fprintf (stderr, "(physical_reads=%d)", h->reads);
	for (i=0; i < filesize; i++)
	{
		if ((i < CACHE_LINE_SIZE) || (i >= 256 - CACHE_LINE_SIZE))
		{
			if (dataaccessed[i] != 1)
			{
				fprintf (stderr, "validate_big_random: offset %d is %"PRId32" instead of 1\n", i, dataaccessed[i]);
				retval++;
			}
		} else {
			if (dataaccessed[i] > (h->reads / 2))
			{
				fprintf (stderr, "validate_big_random: excessive amount of reads offset %d, %d PRId32 > threshold %"PRId32"\n", i, dataaccessed[i], (h->reads / 2));
				retval++;
			}
		}
	}
	return retval;
}

static int test6_big_random (void)
{
	int retval = 0;
	fprintf (stderr, "test6: big file, cache start and end random (mode NORMAL)\n");
	retval += dotest (256, 1, buf256, exercise_entirefile_random_max8, validate_big_random);
	fprintf (stderr, "\n");
	return retval;
}

static int exercise_eof_twice (const uint64_t filesize, const uint8_t *filedata, struct ocpfilehandle_t *h)
{
	int retval = 0;
	int result;
	uint8_t buffer[CACHE_LINE_SIZE * (CACHE_LINES + 1) + 1];

	result = h->read (h, buffer, CACHE_LINE_SIZE * (CACHE_LINES + 1) + 1);
	if (result != filesize)
	{
		fprintf (stderr, "exercise_eof_twice() iteration 1: read(oversize), returned %d instead of %"PRIu64"\n", result, filesize);
	}
	if (memcmp (buffer, filedata, filesize))
	{
		fprintf (stderr, "exercise_eof_twice() iteration 1: data read back does not match\n");
		retval++;
	}

	memset (buffer, 0, filesize);
	if (h->seek_set (h, 0))
	{
		fprintf (stderr, "exercise_eof_twice(): seek_set() failed\n");
	}

	result = h->read (h, buffer, CACHE_LINE_SIZE * (CACHE_LINES + 1) + 1);
	if (result != filesize)
	{
		fprintf (stderr, "exercise_eof_twice() iteration 2: read(oversize), returned %d instead of %"PRIu64"\n", result, filesize);
	}
	if (memcmp (buffer, filedata, filesize))
	{
		fprintf (stderr, "exercise_eof_twice() iteration 2: data read back does not match\n");
		retval++;
	}

	return retval;
}

static int validate_eof_twice_normal (const uint64_t filesize, const uint8_t *filedata, const uint32_t *dataaccessed, const struct cache_ocpfilehandle_test_t *h)
{
	int i;
	int retval = 0;

	for (i=0; i < filesize; i++)
	{
		int expected = 1;
		if (filesize > (CACHE_LINE_SIZE * CACHE_LINES))
		{
			if ((i >= CACHE_LINE_SIZE) && (i < (filesize & ~(CACHE_LINE_SIZE - 1))))
			{
				expected = 2;
			}
		}
		if (dataaccessed[i] != expected)
		{
			fprintf (stderr, "validate_eof_twice_normal: offset %d is %"PRId32" instead of %d\n", i, dataaccessed[i], expected);
			retval++;
		}
	}
	return retval;
}

static int validate_eof_twice_stream (const uint64_t filesize, const uint8_t *filedata, const uint32_t *dataaccessed, const struct cache_ocpfilehandle_test_t *h)
{
	int i;
	int retval = 0;

	for (i=0; i < filesize; i++)
	{
		int expected = 1;
		if (filesize == (CACHE_LINE_SIZE * CACHE_LINES))
		{
			if ((i >= CACHE_LINE_SIZE) && (i < (CACHE_LINE_SIZE * 2)))
			{
				expected = 2; /* page two is re-used when probing EOF, it is expected to have the least amount of points */
			}
		} else if (filesize > (CACHE_LINE_SIZE * CACHE_LINES))
		{
			if ((i >= CACHE_LINE_SIZE) && (i < (filesize & ~(CACHE_LINE_SIZE - 1))))
			{
				expected = 2;
			}
		}
		if (dataaccessed[i] != expected)
		{
			fprintf (stderr, "validate_eof_twice_stream: offset %d is %"PRId32" instead of %d\n", i, dataaccessed[i], expected);
			retval++;
		}
	}
	return retval;
}

static int test7_eof_normal (void)
{
	int retval = 0;
	int i;
	fprintf (stderr, "test7: eof (mode NORMAL) ");
	for (i=1; i < (CACHE_LINE_SIZE * (CACHE_LINES + 1)); i++)
	{
		fprintf (stderr, " [size=%d]", i);
		retval += dotest (i, 1, buf256, exercise_eof_twice, validate_eof_twice_normal);
	}
	fprintf (stderr, "\n\n");
	return retval;
}

static int test8_eof_stream (void)
{
	int retval = 0;
	int i;
	fprintf (stderr, "test8: eof (mode STREAM) ");
	for (i=1; i < (CACHE_LINE_SIZE * (CACHE_LINES + 1)); i++)
	{
		fprintf (stderr, " [size=%d]", i);
		retval += dotest (i, 2, buf256, exercise_eof_twice, validate_eof_twice_stream);
	}
	fprintf (stderr, "\n\n");
	return retval;
}

int main (int argc, char *argv[])
{
	int retval = 0;
	retval += test1_zerosize_file ();
	retval += test2_fits_in_cache_normal ();
	retval += test3_fits_in_cache_stream ();
	retval += test4_big_normal_start_stop ();
	retval += test5_big_stream_start_stop ();
	retval += test6_big_random ();
	retval += test7_eof_normal ();
	retval += test8_eof_stream ();

	return retval;
}
