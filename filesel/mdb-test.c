/* OpenCP Module Player
 * copyright (c) 2016-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * unit test for mdb.c
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

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/file.h>
#include <unistd.h>

#ifdef read
# undef read
#endif
#ifdef write
# undef write
#endif
#ifdef open
# undef open
#endif
#ifdef close
# undef close
#endif
#ifdef lseek
# undef lseek
#endif
#define osfile int
#define osfile_read mdb_test_read
#define osfile_write mdb_test_write
#define osfile_open_readwrite mdb_test_open
#define osfile_close mdb_test_close
#define osfile_setpos mdb_test_lseek
#define osfile_purge_readahead_cache(x)

static ssize_t mdb_test_read (int *fd, void *buf, size_t size);
static ssize_t mdb_test_write (int *fd, const void *buf, size_t size);
static int *mdb_test_open(const char *pathname, int dolock, int mustcreate);
static off_t mdb_test_lseek (int *fd, off_t offset);
static int mdb_test_close (int *fd);

int fd_3 = 3;

#define CFDATAHOMEDIR_OVERRIDE "/foo/home/ocp/.ocp/"
#include "mdb.c"
#include "../stuff/compat.c"

int fsWriteModInfo = 1;

static ssize_t (*mdb_test_read_hook) (int *fd, void *buf, size_t size) = 0;
static ssize_t (*mdb_test_write_hook) (int *fd, const void *buf, size_t size) = 0;
static int *(*mdb_test_open_hook) (const char *pathname, int dolock, int mustcreate) = 0;
static off_t (*mdb_test_lseek_hook) (int *fd, off_t offset) = 0;
static int (*mdb_test_close_hook) (int *fd) = 0;

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

struct ocpfilehandle_t *ancient_filehandle (char *compressionmethod, int compressionmethod_len, struct ocpfilehandle_t *s)
{
	return 0;
}

void latin1_f_to_utf8_z (const char *src, size_t srclen, char *dst, size_t dstlen)
{
	snprintf (dst, dstlen, "%*s", (int)(MIN(dstlen - 1, srclen-1)), src);
}

void cp437_f_to_utf8_z(const char *src, size_t srclen, char *dst, size_t dstlen)
{
	snprintf (dst, dstlen, "%*s", (int)(MIN(dstlen - 1, srclen-1)), src);
}

const struct dirdbAPI_t dirdbAPI;

static ssize_t mdb_test_read (int *fd, void *buf, size_t size)
{
	if (mdb_test_read_hook) return mdb_test_read_hook (fd, buf, size);
	fprintf (stderr, ANSI_COLOR_RED "Unexepected read() call\n" ANSI_COLOR_RESET);
	_exit(1);
}

static ssize_t mdb_test_write (int *fd, const void *buf, size_t size)
{
	if (mdb_test_write_hook) return mdb_test_write_hook (fd, buf, size);
	fprintf (stderr, ANSI_COLOR_RED "Unexepected write() call\n" ANSI_COLOR_RESET);
	_exit(1);
}

static int *mdb_test_open (const char *pathname, int dolock, int mustcreate)
{
	if (mdb_test_open_hook)
	{
		return mdb_test_open_hook (pathname, dolock, mustcreate);
	}
	fprintf (stderr, ANSI_COLOR_RED "Unexepected open() call\n" ANSI_COLOR_RESET);
	_exit(1);
}

static off_t mdb_test_lseek (int *fd, off_t offset)
{
	if (mdb_test_lseek_hook) return mdb_test_lseek_hook (fd, offset);
	fprintf (stderr, ANSI_COLOR_RED "Unexepected lseek() call\n" ANSI_COLOR_RESET);
	_exit(1);
}

static int mdb_test_close (int *fd)
{
	if (mdb_test_close_hook) return mdb_test_close_hook (fd);
	fprintf (stderr, ANSI_COLOR_RED "Unexepected close() call\n" ANSI_COLOR_RESET);
	_exit(1);
}

void dirdbGetName_internalstr(uint32_t ref, const char **name)
{
	switch (ref)
	{
		default: *name = 0;           return;
		case 1:  *name = "test1.mod"; return;
	}
}

int mdb_basic_sizeof (void)
{
	int retval = 0;

	fprintf (stderr, ANSI_COLOR_CYAN "MDB sizeof and indexof tests\n" ANSI_COLOR_RESET);

	retval |= (sizeof (struct modinfoentry) != 64);
	fprintf (stderr, "sizeof (struct modinfoentry) == 64: %ld %s\n" ANSI_COLOR_RESET,
		sizeof (struct modinfoentry),
		(sizeof (struct modinfoentry) == 64) ? ANSI_COLOR_GREEN "OK" : ANSI_COLOR_RED "FAILED");

	retval |= (sizeof (mdbData[0].mie) != 64);
	fprintf (stderr, "sizeof (struct modinfoentry.mie) == 64: %ld %s\n" ANSI_COLOR_RESET,
		sizeof (mdbData[0].mie),
		(sizeof (mdbData[0].mie) == 64) ? ANSI_COLOR_GREEN "OK" : ANSI_COLOR_RED "FAILED");

	retval |= (sizeof (mdbData[0].mie.general) != 64);
	fprintf (stderr, "sizeof (struct modinfoentry.mie.general) == 64: %ld %s\n" ANSI_COLOR_RESET,
		sizeof (mdbData[0].mie.general),
		(sizeof (mdbData[0].mie.general) == 64) ? ANSI_COLOR_GREEN "OK" : ANSI_COLOR_RED "FAILED");

	retval |= (sizeof (mdbData[0].mie.string) != 64);
	fprintf (stderr, "sizeof (struct modinfoentry.mie.string) == 64: %ld %s\n" ANSI_COLOR_RESET,
		sizeof (mdbData[0].mie.string),
		(sizeof (mdbData[0].mie.string) == 64) ? ANSI_COLOR_GREEN "OK" : ANSI_COLOR_RED "FAILED");

	retval |= (offsetof (struct modinfoentry, mie.general.filename_hash) != 1);
	fprintf (stderr, "offsetof(struct modinfoentry.mie.general.filename_hash) == 1: %ld %s\n" ANSI_COLOR_RESET,
		offsetof (struct modinfoentry, mie.general.filename_hash),
		(offsetof (struct modinfoentry, mie.general.filename_hash) == 1) ? ANSI_COLOR_GREEN "OK" : ANSI_COLOR_RED "FAILED");

	retval |= (offsetof (struct modinfoentry, mie.general.size) != 8);
	fprintf (stderr, "offsetof(struct modinfoentry.mie.general.size) == 8: %ld %s\n" ANSI_COLOR_RESET,
		offsetof (struct modinfoentry, mie.general.size),
		(offsetof (struct modinfoentry, mie.general.size) == 8) ? ANSI_COLOR_GREEN "OK" : ANSI_COLOR_RED "FAILED");

	retval |= (offsetof (struct modinfoentry, mie.general.reserved) != 55);
	fprintf (stderr, "offsetof(struct modinfoentry.mie.general.reserved) == 55: %ld %s\n" ANSI_COLOR_RESET,
		offsetof (struct modinfoentry, mie.general.reserved),
		(offsetof (struct modinfoentry, mie.general.reserved) == 55) ? ANSI_COLOR_GREEN "OK" : ANSI_COLOR_RED "FAILED");

	return retval;
}

void mdb_basic_mdbNew_prepare (void)
{
	mdbDataSize = 19;
	mdbData = calloc (64, mdbDataSize);
	mdbDataNextFree = 1;

	mdbData[1].mie.general.record_flags = MDB_USED;
	mdbData[1].mie.general.filename_hash[0] = 0x01;
	/* 1 */
	mdbData[3].mie.general.record_flags = MDB_USED;
	mdbData[3].mie.general.filename_hash[0] = 0x02;
	/* 2 */
	mdbData[6].mie.general.record_flags = MDB_USED;
	mdbData[6].mie.general.filename_hash[0] = 0x03;
	/* 2 */
	mdbData[9].mie.general.record_flags = MDB_USED;
	mdbData[9].mie.general.filename_hash[0] = 0x04;
	/* 3 */
	mdbData[13].mie.general.record_flags = MDB_USED;
	mdbData[13].mie.general.filename_hash[0] = 0x05;

	mdbDirty = 1;
	mdbDirtyMapSize = 64;
	mdbDirtyMap = calloc (1, mdbDirtyMapSize / 8);

	mdbSearchIndexData = 0;
	mdbSearchIndexCount = 0;
	mdbSearchIndexSize = 0;
}

void mdb_basic_mdbNew_finalize (void)
{
	free (mdbData);
	free (mdbDirtyMap);
}

int mdb_basic_mdbNew (void)
{
	int retval = 0;
	uint32_t size1;
	uint32_t size2_1;
	uint32_t size2_2;
	uint32_t size3;
	uint32_t size4;

	fprintf (stderr, ANSI_COLOR_CYAN "MDB mdbNew allocator (no heap grow)\n" ANSI_COLOR_RESET);

	mdb_basic_mdbNew_prepare ();

	size4 = mdbNew (4);
	size3 = mdbNew (3);
	size2_1 = mdbNew (2);
	size2_2 = mdbNew (2);
	size1 = mdbNew (1);

	fprintf (stderr, "Initial map [HU.U..U. .U...U.. ...]\n");

	retval |= (size4 != 14);
	fprintf (stderr, "size4 ==> 14: %"PRIu32" %s\n" ANSI_COLOR_RESET,
		size4,
		(size4 == 14) ? ANSI_COLOR_GREEN "OK" : ANSI_COLOR_RED "FAILED");

	retval |= (size3 != 10);
	fprintf (stderr, "size3 ==> 10: %"PRIu32" %s\n" ANSI_COLOR_RESET,
		size3,
		(size3 == 10) ? ANSI_COLOR_GREEN "OK" : ANSI_COLOR_RED "FAILED");

	retval |= (size2_1 != 4);
	fprintf (stderr, "size2 ==> 4: %"PRIu32" %s\n" ANSI_COLOR_RESET,
		size2_1,
		(size2_1 == 4) ? ANSI_COLOR_GREEN "OK" : ANSI_COLOR_RED "FAILED");

	retval |= (size2_2 != 7);
	fprintf (stderr, "size2 ==> 7: %"PRIu32" %s\n" ANSI_COLOR_RESET,
		size2_2,
		(size2_2 == 7) ? ANSI_COLOR_GREEN "OK" : ANSI_COLOR_RED "FAILED");

	retval |= (size1 != 2);
	fprintf (stderr, "size1 ==> 2: %"PRIu32" %s\n" ANSI_COLOR_RESET,
		size1,
		(size1 == 2) ? ANSI_COLOR_GREEN "OK" : ANSI_COLOR_RED "FAILED");

	retval |= mdbData[2].mie.general.record_flags != MDB_USED;
	retval |= mdbData[4].mie.general.record_flags != MDB_USED;
	retval |= mdbData[5].mie.general.record_flags != MDB_USED;
	retval |= mdbData[7].mie.general.record_flags != MDB_USED;
	retval |= mdbData[8].mie.general.record_flags != MDB_USED;
	retval |= mdbData[10].mie.general.record_flags != MDB_USED;
	retval |= mdbData[11].mie.general.record_flags != MDB_USED;
	retval |= mdbData[12].mie.general.record_flags != MDB_USED;
	retval |= mdbData[14].mie.general.record_flags != MDB_USED;
	retval |= mdbData[15].mie.general.record_flags != MDB_USED;
	retval |= mdbData[16].mie.general.record_flags != MDB_USED;
	retval |= mdbData[17].mie.general.record_flags != MDB_USED;
	retval |= mdbData[18].mie.general.record_flags == MDB_USED;
	fprintf (stderr, "New map     [HU%sU%s%sU%s %sU%s%s%sU%s%s %s%s%s] %s\n" ANSI_COLOR_RESET,
		mdbData[2].mie.general.record_flags == MDB_USED ? ANSI_COLOR_GREEN "+" ANSI_COLOR_RESET : ANSI_COLOR_RED "." ANSI_COLOR_RESET,
		mdbData[4].mie.general.record_flags == MDB_USED ? ANSI_COLOR_GREEN "+" ANSI_COLOR_RESET : ANSI_COLOR_RED "." ANSI_COLOR_RESET,
		mdbData[5].mie.general.record_flags == MDB_USED ? ANSI_COLOR_GREEN "+" ANSI_COLOR_RESET : ANSI_COLOR_RED "." ANSI_COLOR_RESET,
		mdbData[7].mie.general.record_flags == MDB_USED ? ANSI_COLOR_GREEN "+" ANSI_COLOR_RESET : ANSI_COLOR_RED "." ANSI_COLOR_RESET,
		mdbData[8].mie.general.record_flags == MDB_USED ? ANSI_COLOR_GREEN "+" ANSI_COLOR_RESET : ANSI_COLOR_RED "." ANSI_COLOR_RESET,
		mdbData[10].mie.general.record_flags == MDB_USED ? ANSI_COLOR_GREEN "+" ANSI_COLOR_RESET : ANSI_COLOR_RED "." ANSI_COLOR_RESET,
		mdbData[11].mie.general.record_flags == MDB_USED ? ANSI_COLOR_GREEN "+" ANSI_COLOR_RESET : ANSI_COLOR_RED "." ANSI_COLOR_RESET,
		mdbData[12].mie.general.record_flags == MDB_USED ? ANSI_COLOR_GREEN "+" ANSI_COLOR_RESET : ANSI_COLOR_RED "." ANSI_COLOR_RESET,
		mdbData[14].mie.general.record_flags == MDB_USED ? ANSI_COLOR_GREEN "+" ANSI_COLOR_RESET : ANSI_COLOR_RED "." ANSI_COLOR_RESET,
		mdbData[15].mie.general.record_flags == MDB_USED ? ANSI_COLOR_GREEN "+" ANSI_COLOR_RESET : ANSI_COLOR_RED "." ANSI_COLOR_RESET,
		mdbData[16].mie.general.record_flags == MDB_USED ? ANSI_COLOR_GREEN "+" ANSI_COLOR_RESET : ANSI_COLOR_RED "." ANSI_COLOR_RESET,
		mdbData[17].mie.general.record_flags == MDB_USED ? ANSI_COLOR_GREEN "+" ANSI_COLOR_RESET : ANSI_COLOR_RED "." ANSI_COLOR_RESET,
		mdbData[18].mie.general.record_flags == MDB_USED ? ANSI_COLOR_RED "+" ANSI_COLOR_RESET : ANSI_COLOR_GREEN "." ANSI_COLOR_RESET,
		((mdbData[2].mie.general.record_flags != MDB_USED) ||
		 (mdbData[4].mie.general.record_flags != MDB_USED) ||
		 (mdbData[5].mie.general.record_flags != MDB_USED) ||
		 (mdbData[7].mie.general.record_flags != MDB_USED) ||
		 (mdbData[8].mie.general.record_flags != MDB_USED) ||
		 (mdbData[10].mie.general.record_flags != MDB_USED) ||
		 (mdbData[11].mie.general.record_flags != MDB_USED) ||
		 (mdbData[12].mie.general.record_flags != MDB_USED) ||
		 (mdbData[14].mie.general.record_flags != MDB_USED) ||
		 (mdbData[15].mie.general.record_flags != MDB_USED) ||
		 (mdbData[16].mie.general.record_flags != MDB_USED) ||
		 (mdbData[17].mie.general.record_flags != MDB_USED) ||
		 (mdbData[18].mie.general.record_flags == MDB_USED)) ? ANSI_COLOR_RED "FAILED" : ANSI_COLOR_GREEN "OK");

	retval |= (mdbDirtyMap[0] != (0x04 | 0x10 | 0x20 | 0xb4));
	retval |= (mdbDirtyMap[1] != (0x01 | 0x04 | 0x08 | 0x10 | 0x40 | 0x80));
	retval |= (mdbDirtyMap[2] != (0x01 | 0x02));
	fprintf (stderr, "DirtyMap    [%s%s%s%s%s%s%s%s %s%s%s%s%s%s%s%s %s%s%s%s%s%s%s%s" ANSI_COLOR_RESET "] %s\n" ANSI_COLOR_RESET,
		(!!(mdbDirtyMap[0] & 0x01)) ? ANSI_COLOR_RED "X" : ANSI_COLOR_GREEN "_", //  0
		(!!(mdbDirtyMap[0] & 0x02)) ? ANSI_COLOR_RED "X" : ANSI_COLOR_GREEN "_", //  1
		(!!(mdbDirtyMap[0] & 0x04)) ? ANSI_COLOR_GREEN "X" : ANSI_COLOR_RED "_", //  2**
		(!!(mdbDirtyMap[0] & 0x08)) ? ANSI_COLOR_RED "X" : ANSI_COLOR_GREEN "_", //  3
		(!!(mdbDirtyMap[0] & 0x10)) ? ANSI_COLOR_GREEN "X" : ANSI_COLOR_RED "_", //  4**
		(!!(mdbDirtyMap[0] & 0x20)) ? ANSI_COLOR_GREEN "X" : ANSI_COLOR_RED "_", //  5**
		(!!(mdbDirtyMap[0] & 0x40)) ? ANSI_COLOR_RED "X" : ANSI_COLOR_GREEN "_", //  6
		(!!(mdbDirtyMap[0] & 0x80)) ? ANSI_COLOR_GREEN "X" : ANSI_COLOR_RED "_", //  7**
		(!!(mdbDirtyMap[1] & 0x01)) ? ANSI_COLOR_GREEN "X" : ANSI_COLOR_RED "_", //  8**
		(!!(mdbDirtyMap[1] & 0x02)) ? ANSI_COLOR_RED "X" : ANSI_COLOR_GREEN "_", //  9
		(!!(mdbDirtyMap[1] & 0x04)) ? ANSI_COLOR_GREEN "X" : ANSI_COLOR_RED "_", // 10**
		(!!(mdbDirtyMap[1] & 0x08)) ? ANSI_COLOR_GREEN "X" : ANSI_COLOR_RED "_", // 11**
		(!!(mdbDirtyMap[1] & 0x10)) ? ANSI_COLOR_GREEN "X" : ANSI_COLOR_RED "_", // 12**
		(!!(mdbDirtyMap[1] & 0x20)) ? ANSI_COLOR_RED "X" : ANSI_COLOR_GREEN "_", // 13
		(!!(mdbDirtyMap[1] & 0x40)) ? ANSI_COLOR_GREEN "X" : ANSI_COLOR_RED "_", // 14**
		(!!(mdbDirtyMap[1] & 0x80)) ? ANSI_COLOR_GREEN "X" : ANSI_COLOR_RED "_", // 15**
		(!!(mdbDirtyMap[2] & 0x01)) ? ANSI_COLOR_GREEN "X" : ANSI_COLOR_RED "_", // 16**
		(!!(mdbDirtyMap[2] & 0x02)) ? ANSI_COLOR_GREEN "X" : ANSI_COLOR_RED "_", // 17**
		(!!(mdbDirtyMap[2] & 0x04)) ? ANSI_COLOR_RED "X" : ANSI_COLOR_GREEN "_", // 18
		(!!(mdbDirtyMap[2] & 0x08)) ? ANSI_COLOR_RED "X" : ANSI_COLOR_GREEN "_", // 19
		(!!(mdbDirtyMap[2] & 0x10)) ? ANSI_COLOR_RED "X" : ANSI_COLOR_GREEN "_", // 20
		(!!(mdbDirtyMap[2] & 0x20)) ? ANSI_COLOR_RED "X" : ANSI_COLOR_GREEN "_", // 21
		(!!(mdbDirtyMap[2] & 0x40)) ? ANSI_COLOR_RED "X" : ANSI_COLOR_GREEN "_", // 22
		(!!(mdbDirtyMap[2] & 0x80)) ? ANSI_COLOR_RED "X" : ANSI_COLOR_GREEN "_", // 23
		((mdbDirtyMap[0] != (0x04 | 0x10 | 0x20 | 0xb4)) ||
		 (mdbDirtyMap[1] != (0x01 | 0x04 | 0x08 | 0x10 | 0x40 | 0x80)) ||
		 (mdbDirtyMap[2] != (0x01 | 0x02))) ? ANSI_COLOR_RED "Wrong" : ANSI_COLOR_GREEN "OK");

	retval |= (mdbDataNextFree != 3);
	fprintf (stderr, "mdbDataNextFree == 3 (only mdbNew(1) moves it unless Next == returnvalue): %"PRIu32" %s\n" ANSI_COLOR_RESET,
		mdbDataNextFree,
		(mdbDataNextFree == 3) ? ANSI_COLOR_GREEN "OK" : ANSI_COLOR_RED "FAILED");

	mdb_basic_mdbNew_finalize ();

	return retval;
}

void mdb_heap1_mdbNew_prepare (int test, int freeN)
{
	int i;

	mdbDataSize = 24;
	mdbData = calloc (64, mdbDataSize);
	mdbDataNextFree = 1;

	for (i = 1; i < mdbDataSize - freeN; i++)
	{
		mdbData[i].mie.general.record_flags = MDB_USED;
		mdbData[i].mie.general.filename_hash[0] = i;
	}

	mdbDirty = 0;
	mdbDirtyMapSize = (test == 1) ? 256 : 24;
	mdbDirtyMap = calloc (1, mdbDirtyMapSize / 8);

	mdbSearchIndexData = 0;
	mdbSearchIndexCount = 0;
	mdbSearchIndexSize = 0;
}

void mdb_heap1_mdbNew_finalize (void)
{
	free (mdbData);
	free (mdbDirtyMap);
}

int mdb_heap1_mdbNew (int test)
{
	int retval = 0;

	if (test == 1)
	{
		fprintf (stderr, ANSI_COLOR_CYAN "MDB mdbNew allocator (mdbData only heap grow)\n" ANSI_COLOR_RESET);
	} else {
		fprintf (stderr, ANSI_COLOR_CYAN "MDB mdbNew allocator (mdbData and mdbDirtyMap only heap grow)\n" ANSI_COLOR_RESET);
	}

	mdb_heap1_mdbNew_prepare (test, 2);
	mdbNew (1);
	retval |= (mdbDataSize     !=                24);
	retval |= (mdbDirtyMapSize != ((test==1)?256:24));
	fprintf (stderr, "2 free, allocate 1 => mdbData(%d):%s mdbDirtyMap(%d):%s\n",
		(int)mdbDataSize,
		(mdbDataSize     !=                24) ? ANSI_COLOR_RED "Failed, heap resized" ANSI_COLOR_RESET : ANSI_COLOR_GREEN "OK" ANSI_COLOR_RESET,
		(int)mdbDirtyMapSize,
		(mdbDirtyMapSize != ((test==1)?256:24)) ? ANSI_COLOR_RED "Failed, heap resized" ANSI_COLOR_RESET : ANSI_COLOR_GREEN "OK" ANSI_COLOR_RESET);
	mdb_heap1_mdbNew_finalize ();

	mdb_heap1_mdbNew_prepare (test, 2);
	mdbNew (2);
	retval |= (mdbDataSize     !=                24);
	retval |= (mdbDirtyMapSize != ((test==1)?256:24));
	fprintf (stderr, "2 free, allocate 2 => mdbData(%d):%s mdbDirtyMap:(%d)%s\n",
		(int)mdbDataSize,
		(mdbDataSize     !=                24) ? ANSI_COLOR_RED "Failed, heap resized" ANSI_COLOR_RESET : ANSI_COLOR_GREEN "OK" ANSI_COLOR_RESET,
		(int)mdbDirtyMapSize,
		(mdbDirtyMapSize != ((test==1)?256:24)) ? ANSI_COLOR_RED "Failed, heap resized" ANSI_COLOR_RESET : ANSI_COLOR_GREEN "OK" ANSI_COLOR_RESET);
	mdb_heap1_mdbNew_finalize ();

	mdb_heap1_mdbNew_prepare (test, 2);
	mdbNew (3);
	retval |= (mdbDataSize     ==  24);
	retval |= (test==1)?(mdbDirtyMapSize != 256) : (mdbDirtyMapSize == 24);
	fprintf (stderr, "2 free, allocate 3 => mdbData(%d):%s mdbDirtyMap(%d):%s\n",
		(int)mdbDataSize,
		(mdbDataSize     !=  24) ? ANSI_COLOR_GREEN "OK" ANSI_COLOR_RESET : ANSI_COLOR_RED "Failed, heap stuck" ANSI_COLOR_RESET,
		(int)mdbDirtyMapSize,
		(test==1)?
		(mdbDirtyMapSize != 256) ? ANSI_COLOR_RED "Failed, heap resized" ANSI_COLOR_RESET : ANSI_COLOR_GREEN "OK" ANSI_COLOR_RESET :
		(mdbDirtyMapSize != 24) ? ANSI_COLOR_GREEN "OK" ANSI_COLOR_RESET : ANSI_COLOR_RED "Failed, heap stuck" ANSI_COLOR_RESET );
	mdb_heap1_mdbNew_finalize ();

	mdb_heap1_mdbNew_prepare (test, 1);
	mdbNew (1);
	retval |= (mdbDataSize     !=                24);
	retval |= (mdbDirtyMapSize != ((test==1)?256:24));
	fprintf (stderr, "1 free, allocate 1 => mdbData(%d):%s mdbDirtyMap(%d):%s\n",
		(int)mdbDataSize,
		(mdbDataSize     !=                24) ? ANSI_COLOR_RED "Failed, heap resized" ANSI_COLOR_RESET : ANSI_COLOR_GREEN "OK" ANSI_COLOR_RESET,
		(int)mdbDirtyMapSize,
		(mdbDirtyMapSize != ((test==1)?256:24)) ? ANSI_COLOR_RED "Failed, heap resized" ANSI_COLOR_RESET : ANSI_COLOR_GREEN "OK" ANSI_COLOR_RESET);
	mdb_heap1_mdbNew_finalize ();

	mdb_heap1_mdbNew_prepare (test, 0);
	mdbNew (1);
	retval |= (mdbDataSize     ==  24);
	retval |= (test==1)?(mdbDirtyMapSize != 256) : (mdbDirtyMapSize == 24);
	fprintf (stderr, "0 free, allocate 1 => mdbData(%d):%s mdbDirtyMap(%d):%s\n",
		(int)mdbDataSize,
		(mdbDataSize     ==  24) ? ANSI_COLOR_RED "Failed, heap resized" ANSI_COLOR_RESET : ANSI_COLOR_GREEN "OK" ANSI_COLOR_RESET,
		(int)mdbDirtyMapSize,
		(test==1)?
		(mdbDirtyMapSize != 256) ? ANSI_COLOR_RED "Failed, heap resized" ANSI_COLOR_RESET : ANSI_COLOR_GREEN "OK" ANSI_COLOR_RESET :
		(mdbDirtyMapSize != 24) ? ANSI_COLOR_GREEN "OK" ANSI_COLOR_RESET : ANSI_COLOR_RED "Failed, heap stuck" ANSI_COLOR_RESET );
	mdb_heap1_mdbNew_finalize ();

	return retval;
}

void mdb_basic_mdbFree_prepare (void)
{
	mdbDataSize = 16;
	mdbData = calloc (64, mdbDataSize);
	mdbDataNextFree = 1;

	mdbDirty = 1;
	mdbDirtyMapSize = 64;
	mdbDirtyMap = calloc (1, mdbDirtyMapSize / 8);

	mdbSearchIndexData = 0;
	mdbSearchIndexCount = 0;
	mdbSearchIndexSize = 0;
}

void mdb_basic_mdbFree_finalize (void)
{
	free (mdbData);
	free (mdbDirtyMap);
}

int mdb_basic_mdbFree (void)
{
	int retval = 0;
	uint32_t r, e;

	fprintf (stderr, ANSI_COLOR_CYAN "MDB mdbFree allocator\n" ANSI_COLOR_RESET);

	mdb_basic_mdbFree_prepare ();
	r = mdbNew (1);
	memset (mdbDirtyMap, 0, mdbDirtyMapSize / 8);
	mdbFree (r, 1);
	e = 0,
	fprintf (stderr, "mdbFree(1):");
	if (mdbData[r].mie.general.record_flags)
	{
		e++;
		fprintf (stderr, ANSI_COLOR_RED " [data not free]");
	}
	if (mdbDirtyMap[0] != 0x02)
	{
		e++;
		fprintf (stderr, ANSI_COLOR_RED " [dirtyMap incorrect]");
	}
	fprintf (stderr, "%s\n" ANSI_COLOR_RESET, e?"" : ANSI_COLOR_GREEN " OK");
	mdb_basic_mdbFree_finalize ();

	mdb_basic_mdbFree_prepare ();
	r = mdbNew (2);
	memset (mdbDirtyMap, 0, mdbDirtyMapSize / 8);
	mdbFree (r, 2);
	e = 0,
	fprintf (stderr, "mdbFree(2):");
	if (mdbData[r].mie.general.record_flags || mdbData[r+1].mie.general.record_flags)
	{
		e++;
		fprintf (stderr, ANSI_COLOR_RED " [data not free]");
	}
	if (mdbDirtyMap[0] != 0x06)
	{
		e++;
		fprintf (stderr, ANSI_COLOR_RED " [dirtyMap incorrect]");
	}
	fprintf (stderr, "%s\n" ANSI_COLOR_RESET, e?"" : ANSI_COLOR_GREEN " OK");
	mdb_basic_mdbFree_finalize ();

	mdb_basic_mdbFree_prepare ();
	mdbNew (1);
	r = mdbNew (1);
	mdbFree (r, 1);
	retval |= (mdbDataNextFree != r);
	fprintf (stderr, "mdbFree(1) moves back Next if after: %s\n" ANSI_COLOR_RESET, (mdbDataNextFree == r) ? ANSI_COLOR_GREEN "OK" : ANSI_COLOR_RED "Failed");
	mdb_basic_mdbFree_finalize ();

	mdb_basic_mdbFree_prepare ();
	mdbNew(4);
	r = mdbNew (1);
	mdbDataNextFree = 2;
	mdbFree (r, 1);
	retval |= (mdbDataNextFree != 2);
	fprintf (stderr, "mdbFree(1) does not touch Next if before: %s\n" ANSI_COLOR_RESET, (mdbDataNextFree == 2) ? ANSI_COLOR_GREEN "OK" : ANSI_COLOR_RED "Failed");
	mdb_basic_mdbFree_finalize ();

	mdb_basic_mdbFree_prepare ();
	mdbNew(1);
	r = mdbNew (2);
	mdbFree (r, 2);
	retval |= (mdbDataNextFree != r);
	fprintf (stderr, "mdbFree(2) moves back Next if after: %s\n" ANSI_COLOR_RESET, (mdbDataNextFree == r) ? ANSI_COLOR_GREEN "OK" : ANSI_COLOR_RED "Failed");
	mdb_basic_mdbFree_finalize ();

	return retval;
}

void mdb_basic_mdbGetModuleReference_prepare (void)
{
	mdbDataSize = 256;
	mdbData = calloc (64, mdbDataSize);
	mdbDataNextFree = 1;

	mdbDirty = 0;
	mdbDirtyMapSize = 256;
	mdbDirtyMap = calloc (1, mdbDirtyMapSize / 8);

	mdbSearchIndexData = 0;
	mdbSearchIndexCount = 0;
	mdbSearchIndexSize = 0;
}

void mdb_basic_mdbGetModuleReference_finalize (void)
{
	free (mdbData);
	free (mdbDirtyMap);
	free (mdbSearchIndexData);
}

int mdb_basic_mdbGetModuleReference (void)
{
	int retval = 0;
	uint32_t ref[10];
	uint32_t r;
	int i, j;

	fprintf (stderr, ANSI_COLOR_CYAN "MDB mdbGetModuleReference (hash, mdbSearchIndex, deduplication)\n" ANSI_COLOR_RESET);

	mdb_basic_mdbGetModuleReference_prepare();

	ref[0] = mdbGetModuleReference ("test0.mod", 128000);
	ref[1] = mdbGetModuleReference ("A very long filename - even longer.mp3", 511423);
	ref[2] = mdbGetModuleReference ("11111112222222.mod", 4444555);
	ref[3] = mdbGetModuleReference ("22222221111111.mod", 4444555);
	ref[4] = mdbGetModuleReference ("foobar.s3m", 128000);
	ref[5] = mdbGetModuleReference ("barfoo.s3m", 128000);
	ref[6] = mdbGetModuleReference ("foofoo.s3m", 128000);
	ref[7] = mdbGetModuleReference ("barbar.s3m", 128000);
	ref[8] = mdbGetModuleReference ("supertest.s3m", 128000);
	ref[9] = mdbGetModuleReference ("open cubic player.s3m", 128000);

	r = mdbGetModuleReference ("test0.mod", 128000);
	retval |= (r != ref[0]);
	fprintf (stderr, "Duplicate calls reveals the same ID: %"PRIu32" %"PRIu32" %s\n" ANSI_COLOR_RESET, ref[0], r, (ref[0] == r) ? ANSI_COLOR_GREEN "OK" : ANSI_COLOR_RED "Failed");

	fprintf (stderr, "Hashes are unique: ");
	r = 0;
	for (i = 0; i < 10; i++)
	{
		fprintf (stderr, " %" PRIu32, ref[i]);
		for (j = i + 1; j < 10; j++)
		{
			if (ref[i] == ref[j])
			{
				r++;
			}
		}
	}
	retval |= r;
	fprintf (stderr, " => %d duplicates - %s\n" ANSI_COLOR_RESET, r, r ? ANSI_COLOR_RED "Failed" : ANSI_COLOR_GREEN "OK");

	r = 0;
	fprintf (stderr, "mdbSearchIndexData is in incrementing order:\n");
	for (i = 0; i < mdbSearchIndexCount; i++)
	{
		fprintf (stderr, "  %8" PRIu64 " 0x%02x%02x%02x%02x%02x%02x%02x\n",
			mdbData[mdbSearchIndexData[i]].mie.general.size,
			mdbData[mdbSearchIndexData[i]].mie.general.filename_hash[0],
			mdbData[mdbSearchIndexData[i]].mie.general.filename_hash[1],
			mdbData[mdbSearchIndexData[i]].mie.general.filename_hash[2],
			mdbData[mdbSearchIndexData[i]].mie.general.filename_hash[3],
			mdbData[mdbSearchIndexData[i]].mie.general.filename_hash[4],
			mdbData[mdbSearchIndexData[i]].mie.general.filename_hash[5],
			mdbData[mdbSearchIndexData[i]].mie.general.filename_hash[6]);
		if (i)
		{
			if (mdbData[mdbSearchIndexData[i-1]].mie.general.size > mdbData[mdbSearchIndexData[i]].mie.general.size)
			{
				r++;
			} else if (mdbData[mdbSearchIndexData[i-1]].mie.general.size == mdbData[mdbSearchIndexData[i]].mie.general.size)
			{
				int temp = memcmp (mdbData[mdbSearchIndexData[i-1]].mie.general.filename_hash, mdbData[mdbSearchIndexData[i]].mie.general.filename_hash, 7);
				if (temp >= 0)
				{
					r++;
				}
			 }
		}
	}
	retval |= r;
	fprintf (stderr, "  ... %s\n" ANSI_COLOR_RESET, r ? ANSI_COLOR_RED "Failed" : ANSI_COLOR_GREEN "OK");

	mdb_basic_mdbGetModuleReference_finalize ();

	return retval;
}

void mdb_basic_mdbWriteString_prepare (void)
{
	mdbDataSize = 256;
	mdbData = calloc (64, mdbDataSize);
	mdbDataNextFree = 1;

	mdbData[0].mie.string.flags = MDB_USED;
	memset (mdbData[0].mie.string.data, '_', 63);

	memset (mdbData[1].mie.string.data, '_', 63);

	mdbData[2].mie.string.flags = MDB_USED;
	memset (mdbData[2].mie.string.data, '_', 63);

	memset (mdbData[3].mie.string.data, '_', 63);
	memset (mdbData[4].mie.string.data, '_', 63);

	mdbData[5].mie.string.flags = MDB_USED;
	memset (mdbData[5].mie.string.data, '_', 63);

	memset (mdbData[6].mie.string.data, '_', 63);
	memset (mdbData[7].mie.string.data, '_', 63);
	memset (mdbData[8].mie.string.data, '_', 63);

	mdbData[9].mie.string.flags = MDB_USED;
	memset (mdbData[9].mie.string.data, '_', 63);

	mdbDirty = 0;
	mdbDirtyMapSize = 256;
	mdbDirtyMap = calloc (1, mdbDirtyMapSize / 8);

	mdbSearchIndexData = 0;
	mdbSearchIndexCount = 0;
	mdbSearchIndexSize = 0;
}

void mdb_basic_mdbWriteString_finalize (void)
{
	free (mdbData);
	free (mdbDirtyMap);
}

int mdb_basic_mdbWriteString (void)
{
	int retval = 0;
	uint8_t d[128];
	int i;
	uint32_t r;

	fprintf (stderr, ANSI_COLOR_CYAN "MDB mdbWriteString\n" ANSI_COLOR_RESET);

	for (i=1; i <= 127; i++)
	{
		uint32_t e = 0;

		if ((i > 3) && (i < 60)) continue;
		if ((i > 65) && (i < 125)) continue;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation="
		snprintf ((char *)d, i, "%s",
"abcdefghijklmnopqrstuvwxyz" // 26
"ABCDEFGHIJKLMNOPQRSTUVWXYZ" // 26 =>  52
"0123456789!?+-.,:;=()[]{|}" // 26 =>  78
"abcdefghijklmnopqrstuvwxyz" // 26 => 104
"ABCDEFGHIJKLMNOPQRSTUVWXYZ" // 26 => 130
"0123456789!?+-.,:;=()[]{|}" // 26 => 156
		);
#pragma GCC diagnostic pop
		fprintf (stderr, "strlen() => %d, NULL source:", (int)strlen((char *)d));
		mdb_basic_mdbWriteString_prepare ();
		r = UINT32_MAX;
		mdbWriteString ((char *)d, &r);
		fprintf (stderr, " (r=%"PRIu32")", r);
		if ((i - 1) == 0)
		{
			if (r != UINT32_MAX)
			{
				fprintf (stderr, ANSI_COLOR_RED " [allocated more than zero buffers]");
				e++;
			}
		} else if ((i-1) <= 63)
		{
			if (r != 1)
			{
				fprintf (stderr, ANSI_COLOR_RED " [allocated more (or less) than one buffer]");
				e++;
			}
		} else if ((i-1) < (63+63))
		{
			if (r != 3)
			{
				fprintf (stderr, ANSI_COLOR_RED " [allocated more (or less) than two buffers]");
				e++;
			}
		}
		if ((mdbData[0].mie.string.flags != MDB_USED) ||
		    (mdbData[0].mie.string.data[0] != '_') ||
		    (mdbData[0].mie.string.data[62] != '_') ||
		    (mdbData[2].mie.string.flags != MDB_USED) ||
		    (mdbData[2].mie.string.data[0] != '_') ||
		    (mdbData[2].mie.string.data[62] != '_') ||
		    (mdbData[5].mie.string.flags != MDB_USED) ||
		    (mdbData[5].mie.string.data[0] != '_') ||
		    (mdbData[5].mie.string.data[62] != '_') ||
		    (mdbData[9].mie.string.flags != MDB_USED) ||
		    (mdbData[9].mie.string.data[0] != '_') ||
		    (mdbData[9].mie.string.data[62] != '_'))
		{
			fprintf (stderr, ANSI_COLOR_RED " [canaries touched]");
			e++;
		}
		retval |= e;
		fprintf (stderr, "%s\n" ANSI_COLOR_RESET, e ? "" : ANSI_COLOR_GREEN " OK");
		mdb_basic_mdbWriteString_finalize ();
	}

	for (i=1; i <= 127; i++)
	{
		uint32_t e = 0;

		if ((i > 3) && (i < 60)) continue;
		if ((i > 65) && (i < 125)) continue;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation="
		snprintf ((char *)d, i, "%s",
"abcdefghijklmnopqrstuvwxyz" // 26
"ABCDEFGHIJKLMNOPQRSTUVWXYZ" // 26 =>  52
"0123456789!?+-.,:;=()[]{|}" // 26 =>  78
"abcdefghijklmnopqrstuvwxyz" // 26 => 104
"ABCDEFGHIJKLMNOPQRSTUVWXYZ" // 26 => 130
"0123456789!?+-.,:;=()[]{|}" // 26 => 156
		);
#pragma GCC diagnostic pop
		fprintf (stderr, "NULL, strlen() => %d source:", (int)strlen((char *)d));
		mdb_basic_mdbWriteString_prepare ();
		r = UINT32_MAX;
		mdbWriteString ((char *)d, &r);
		mdbWriteString ("", &r);
		fprintf (stderr, " (r=%"PRIu32")", r);
		if (r != UINT32_MAX)
		{
			fprintf (stderr, ANSI_COLOR_RED " [allocated more than zero buffers]");
			e++;
		}
		if ((mdbData[0].mie.string.flags != MDB_USED) ||
		    (mdbData[0].mie.string.data[0] != '_') ||
		    (mdbData[0].mie.string.data[62] != '_') ||
		    (mdbData[2].mie.string.flags != MDB_USED) ||
		    (mdbData[2].mie.string.data[0] != '_') ||
		    (mdbData[2].mie.string.data[62] != '_') ||
		    (mdbData[5].mie.string.flags != MDB_USED) ||
		    (mdbData[5].mie.string.data[0] != '_') ||
		    (mdbData[5].mie.string.data[62] != '_') ||
		    (mdbData[9].mie.string.flags != MDB_USED) ||
		    (mdbData[9].mie.string.data[0] != '_') ||
		    (mdbData[9].mie.string.data[62] != '_'))
		{
			fprintf (stderr, ANSI_COLOR_RED " [canaries touched]");
			e++;
		}
		if ((mdbData[1].mie.string.flags) ||
		    (mdbData[3].mie.string.flags) ||
		    (mdbData[4].mie.string.flags) ||
		    (mdbData[6].mie.string.flags) ||
		    (mdbData[7].mie.string.flags) ||
		    (mdbData[8].mie.string.flags) ||
		    (mdbData[10].mie.string.flags))
		{
			fprintf (stderr, ANSI_COLOR_RED " [data not released]");
			e++;
		}

		retval |= e;
		fprintf (stderr, "%s\n" ANSI_COLOR_RESET, e ? "" : ANSI_COLOR_GREEN " OK");
		mdb_basic_mdbWriteString_finalize ();
	}

	for (i=1; i <= 127; i++)
	{
		uint32_t e = 0;

		if ((i > 3) && (i < 60)) continue;
		if ((i > 65) && (i < 125)) continue;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation="
		snprintf ((char *)d, i, "%s",
"abcdefghijklmnopqrstuvwxyz" // 26
"ABCDEFGHIJKLMNOPQRSTUVWXYZ" // 26 =>  52
"0123456789!?+-.,:;=()[]{|}" // 26 =>  78
"abcdefghijklmnopqrstuvwxyz" // 26 => 104
"ABCDEFGHIJKLMNOPQRSTUVWXYZ" // 26 => 130
"0123456789!?+-.,:;=()[]{|}" // 26 => 156
		);
#pragma GCC diagnostic pop
		fprintf (stderr, "one page, strlen() => %d source:", (int)strlen((char *)d));
		mdb_basic_mdbWriteString_prepare ();
		r = UINT32_MAX;
		mdbWriteString ((char *)d, &r);
		mdbWriteString ("foo", &r);
		fprintf (stderr, " (r=%"PRIu32")", r);
		if (r != 1)
		{
			fprintf (stderr, ANSI_COLOR_RED " [allocated other size buffer that expected]");
			e++;
		}
		if ((mdbData[0].mie.string.flags != MDB_USED) ||
		    (mdbData[0].mie.string.data[0] != '_') ||
		    (mdbData[0].mie.string.data[62] != '_') ||
		    (mdbData[2].mie.string.flags != MDB_USED) ||
		    (mdbData[2].mie.string.data[0] != '_') ||
		    (mdbData[2].mie.string.data[62] != '_') ||
		    (mdbData[5].mie.string.flags != MDB_USED) ||
		    (mdbData[5].mie.string.data[0] != '_') ||
		    (mdbData[5].mie.string.data[62] != '_') ||
		    (mdbData[9].mie.string.flags != MDB_USED) ||
		    (mdbData[9].mie.string.data[0] != '_') ||
		    (mdbData[9].mie.string.data[62] != '_'))
		{
			fprintf (stderr, ANSI_COLOR_RED " [canaries touched]");
			e++;
		}
		if ((mdbData[3].mie.string.flags) ||
		    (mdbData[4].mie.string.flags) ||
		    (mdbData[6].mie.string.flags) ||
		    (mdbData[7].mie.string.flags) ||
		    (mdbData[8].mie.string.flags) ||
		    (mdbData[10].mie.string.flags))
		{
			fprintf (stderr, ANSI_COLOR_RED " [data not released]");
			e++;
		}

		retval |= e;
		fprintf (stderr, "%s\n" ANSI_COLOR_RESET, e ? "" : ANSI_COLOR_GREEN " OK");
		mdb_basic_mdbWriteString_finalize ();
	}
	return retval;
}

void mdb_basic_mdbGetString_prepare (void)
{
	mdbDataSize = 256;
	mdbData = calloc (64, mdbDataSize);
	mdbDataNextFree = 1;

	mdbSearchIndexData = 0;
	mdbSearchIndexCount = 0;
	mdbSearchIndexSize = 0;
}

void mdb_basic_mdbGetString_finalize (void)
{
	free (mdbData);
	free (mdbDirtyMap);
}

int mdb_basic_mdbGetString (void)
{
	int retval = 0;
	uint8_t d[128];
	uint8_t s[256];
	int i;
	uint32_t r = UINT32_MAX;

	fprintf (stderr, ANSI_COLOR_CYAN "MDB mdbGetString\n" ANSI_COLOR_RESET);

	mdb_basic_mdbWriteString_prepare ();

	for (i=1; i <= 127; i++)
	{
		uint32_t e = 0;

		if ((i > 3) && (i < 60)) continue;
		if ((i > 65) && (i < 125)) continue;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation="
		snprintf ((char *)d, i, "%s",
"abcdefghijklmnopqrstuvwxyz" // 26
"ABCDEFGHIJKLMNOPQRSTUVWXYZ" // 26 =>  52
"0123456789!?+-.,:;=()[]{|}" // 26 =>  78
"abcdefghijklmnopqrstuvwxyz" // 26 => 104
"ABCDEFGHIJKLMNOPQRSTUVWXYZ" // 26 => 130
"0123456789!?+-.,:;=()[]{|}" // 26 => 156
		);
#pragma GCC diagnostic pop
		fprintf (stderr, "strlen() => %d, full store and fetch:", (int)strlen((char *)d));
		mdbWriteString ((char *)d, &r);
		memset (s, 65, 128);
		memset (s + 128, 0, 128);
		mdbGetString ((char *)s, 127, r);
		if (strcmp ((char *)d, (char *)s))
		{
			fprintf (stderr, ANSI_COLOR_RED " [missmatch]");
			e++;
		}
		if (s[127] != 65)
		{
			fprintf (stderr, ANSI_COLOR_RED " [byte after buffer touched]");
			e++;
		}
		retval |= e;
		fprintf (stderr, "%s\n" ANSI_COLOR_RESET, e ? "" : ANSI_COLOR_GREEN " OK");
	}

	for (i=1; i <= 127; i++)
	{
		uint32_t e = 0;

		if ((i > 3) && (i < 60)) continue;
		if ((i > 65) && (i < 125)) continue;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation="
		snprintf ((char *)d, 63+63+1, "%s",
"abcdefghijklmnopqrstuvwxyz" // 26
"ABCDEFGHIJKLMNOPQRSTUVWXYZ" // 26 =>  52
"0123456789!?+-.,:;=()[]{|}" // 26 =>  78
"abcdefghijklmnopqrstuvwxyz" // 26 => 104
"ABCDEFGHIJKLMNOPQRSTUVWXYZ" // 26 => 130
"0123456789!?+-.,:;=()[]{|}" // 26 => 156
		);
#pragma GCC diagnostic pop
		fprintf (stderr, "store and fetch into %d sized buffer:", i);
		mdbWriteString ((char *)d, &r);
		memset (s, 65, 128);
		memset (s + 128, 0, 128);
		mdbGetString ((char *)s, i, r);
		if (strncmp ((char *)d, (char *)s, i - 1))
		{
			fprintf (stderr, ANSI_COLOR_RED " [missmatch]");
			e++;
		}
		if (s[i] != 65)
		{
			fprintf (stderr, ANSI_COLOR_RED " [byte after buffer touched]");
			e++;
		}
		if (s[i-1] != 0)
		{
			fprintf (stderr, ANSI_COLOR_RED " [string not terminated as expected]");
			e++;
		}
		retval |= e;
		fprintf (stderr, "%s\n" ANSI_COLOR_RESET, e ? "" : ANSI_COLOR_GREEN " OK");
	}

	// need to test size restrictions too

	mdb_basic_mdbWriteString_finalize ();
	return retval;
}

void mdb_basic_mdbWriteModuleInfo_mdbGetModuleInfo_prepare (void)
{
	mdbDataSize = 64;
	mdbData = calloc (64, mdbDataSize);
	mdbDataNextFree = 1;

	mdbDirty = 1;
	mdbDirtyMapSize = 64;
	mdbDirtyMap = calloc (1, mdbDirtyMapSize / 8);

	mdbSearchIndexData = 0;
	mdbSearchIndexCount = 0;
	mdbSearchIndexSize = 0;
}

void mdb_basic_mdbWriteModuleInfo_mdbGetModuleInfo_finalize (void)
{
	free (mdbData);
	free (mdbDirtyMap);
	free (mdbSearchIndexData);
}

int mdb_basic_mdbWriteModuleInfo_mdbGetModuleInfo (void)
{
	int retval = 0, e;

	uint32_t r;
	struct moduleinfostruct src;
	struct moduleinfostruct dst;

	memset (&src, 0, sizeof (src));
	memset (&dst, 0, sizeof (dst));

	fprintf (stderr, ANSI_COLOR_CYAN "MDB mdbWriteModuleInfo mdbGetModuleInfo\n" ANSI_COLOR_RESET);

	mdb_basic_mdbWriteModuleInfo_mdbGetModuleInfo_prepare();

	r = mdbGetModuleReference ("module.xm", 123456);
	if (r != 1)
	{
		fprintf (stderr, ANSI_COLOR_RED "initial mdbGetModuleReference failed (ref expected to be 1, got %" PRIu32 "\n" ANSI_COLOR_RESET, r);
		return -1;
	}

	src.size = 123456;
	src.modtype.string.c[0]='X';
	src.modtype.string.c[1]='M';
	src.modtype.string.c[2]=0;
	src.modtype.string.c[3]=0;
	src.flags = 0x5a;
	src.channels = 2;
	src.playtime = 0x0450;
	src.date = 0x12345678;
	memset (src.title, 0, sizeof (src.title));
	memset (src.composer, 0, sizeof (src.composer));
	memset (src.artist, 0, sizeof (src.artist));
	memset (src.style, 0, sizeof (src.style));
	memset (src.comment, 0, sizeof (src.comment));
	memset (src.album, 0, sizeof (src.album));

	e=0;
	fprintf (stderr, "NULL-strings mdbWriteModuleInfo() mdbGetModuleInfo() sequence:");
	if (!mdbWriteModuleInfo (r, &src))
	{
		fprintf (stderr, ANSI_COLOR_RED " [mdbWriteModuleInfo() failed]");
		e++;
	}
	if (mdbDataNextFree != 2)
	{
		fprintf (stderr, ANSI_COLOR_RED " [mdbDataNextFree => %"PRIu32" != 2]", mdbDataNextFree);
		e++;
	}
	if (!mdbGetModuleInfo (&dst, r))
	{
		fprintf (stderr, ANSI_COLOR_RED " [mdbGetModuleInfo() failed]");
		e++;
	}
	if (memcmp (&src, &dst, sizeof (src)))
	{
		fprintf (stderr, ANSI_COLOR_RED " [src and dst data missmatches]");
		e++;
	}
	retval |= e;
	fprintf (stderr, "%s\n" ANSI_COLOR_RESET, e ? "" : ANSI_COLOR_GREEN " OK");

	snprintf (src.title, sizeof(src.title), "%s", "a short standard title");
	snprintf (src.composer, sizeof (src.composer), "%s", "a short composer name");
	snprintf (src.artist, sizeof (src.artist), "%s", "a short artist name");
	snprintf (src.style, sizeof (src.style), "%s", "short style");
	snprintf (src.comment, sizeof (src.comment), "%s", "a very short comment");
	snprintf (src.album, sizeof (src.album), "%s", "a very short album");
	e=0;
	fprintf (stderr, "short strings mdbWriteModuleInfo() mdbGetModuleInfo() sequence:");
	if (!mdbWriteModuleInfo (r, &src))
	{
		fprintf (stderr, ANSI_COLOR_RED " [mdbWriteModuleInfo() failed]");
		e++;
	}
	if (mdbDataNextFree != 8)
	{
		fprintf (stderr, ANSI_COLOR_RED " [mdbDataNextFree => %"PRIu32" != 8]", mdbDataNextFree);
		e++;
	}
	if (!mdbGetModuleInfo (&dst, r))
	{
		fprintf (stderr, ANSI_COLOR_RED " [mdbGetModuleInfo() failed]");
		e++;
	}
	if (memcmp (&src, &dst, sizeof (src)))
	{
		fprintf (stderr, ANSI_COLOR_RED " [src and dst data missmatches]");
		e++;
	}
	retval |= e;
	fprintf (stderr, "%s\n" ANSI_COLOR_RESET, e ? "" : ANSI_COLOR_GREEN " OK");

	snprintf (src.title, sizeof(src.title), "%s", "a very long title can be from the earth, to the moon and back again, tour-retur");
	snprintf (src.composer, sizeof (src.composer), "%s", "a very long composer name can be a name of a developer, Stian, or somebody else");
	snprintf (src.artist, sizeof (src.artist), "%s", "a very long artist name can be your self every time you are in the shower.......");
	snprintf (src.style, sizeof (src.style), "%s", "a very long style can be boombasta-fantastic world wide teddy funky hair top bottom");
	snprintf (src.comment, sizeof (src.comment), "%s", "a very long comment can be very describing, or just plain non-sense, or anything");
	snprintf (src.album, sizeof (src.album), "%s", "a very long album title can be very verbose and sometimes just very strang and fun");
	e=0;
	fprintf (stderr, "long strings mdbWriteModuleInfo() mdbGetModuleInfo() sequence:");
	if (!mdbWriteModuleInfo (r, &src))
	{
		fprintf (stderr, ANSI_COLOR_RED " [mdbWriteModuleInfo() failed]");
		e++;
	}
#if 0
	Memory will have a hole at position 6, since there are an odd-number of strings, so this test can not be used
	if (mdbDataNextFree != 12)
	{
		fprintf (stderr, ANSI_COLOR_RED " [mdbDataNextFree => %"PRIu32" != 12]", mdbDataNextFree);
		e++;
	}
#endif
	if (!mdbGetModuleInfo (&dst, r))
	{
		fprintf (stderr, ANSI_COLOR_RED " [mdbGetModuleInfo() failed]");
		e++;
	}
	if (memcmp (&src, &dst, sizeof (src)))
	{
		fprintf (stderr, ANSI_COLOR_RED " [src and dst data missmatches]");
		e++;
	}
	retval |= e;
	fprintf (stderr, "%s\n" ANSI_COLOR_RESET, e ? "" : ANSI_COLOR_GREEN " OK");

	/* remove garbage, after zero-terminations */
	memset (src.title, 0, sizeof (src.title));
	memset (src.composer, 0, sizeof (src.composer));
	memset (src.artist, 0, sizeof (src.artist));
	memset (src.style, 0, sizeof (src.style));
	memset (src.comment, 0, sizeof (src.comment));
	memset (src.album, 0, sizeof (src.album));
	snprintf (src.title, sizeof(src.title), "%s", "a short standard title");
	snprintf (src.composer, sizeof (src.composer), "%s", "a short composer name");
	snprintf (src.artist, sizeof (src.artist), "%s", "a short artist name");
	snprintf (src.style, sizeof (src.style), "%s", "short style");
	snprintf (src.comment, sizeof (src.comment), "%s", "a very short comment");
	snprintf (src.album, sizeof (src.album), "%s", "a very short album");
	e=0;
	fprintf (stderr, "shrink back strings mdbWriteModuleInfo() mdbGetModuleInfo() sequence:");
	if (!mdbWriteModuleInfo (r, &src))
	{
		fprintf (stderr, ANSI_COLOR_RED " [mdbWriteModuleInfo() failed]");
		e++;
	}
#if 0
	Memory will likely be somewhat fragmented
	if (mdbDataNextFree != 7)
	{
		fprintf (stderr, ANSI_COLOR_RED " [mdbDataNextFree => %"PRIu32" != 7]", mdbDataNextFree);
		e++;
	}
#endif
	if (!mdbGetModuleInfo (&dst, r))
	{
		fprintf (stderr, ANSI_COLOR_RED " [mdbGetModuleInfo() failed]");
		e++;
	}
	if (memcmp (&src, &dst, sizeof (src)))
	{
		fprintf (stderr, ANSI_COLOR_RED " [src and dst data missmatches]");
		e++;
	}
	retval |= e;
	fprintf (stderr, "%s\n" ANSI_COLOR_RESET, e ? "" : ANSI_COLOR_GREEN " OK");

	mdb_basic_mdbWriteModuleInfo_mdbGetModuleInfo_finalize ();

	return retval;
}

#ifdef WORDS_BIGENDIAN
#define BIGlittle_1or1(BIG,little) BIG
#define BIGlittle_2(MSB,LSB) MSB,LSB
#define BIGlittle_4(MSB,B1,B2,LSB) MSB,B1,B2,LSB
#define BIGlittle_8(MSB,B1,B2,B3,B4,B5,B6,LSB) MSB,B1,B2,B3,B4,B5,B6,LSB
#define BIG
#else
#define BIGlittle_1or1(BIG,little) little
#define BIGlittle_2(MSB,LSB) LSB,MSB
#define BIGlittle_4(MSB,B1,B2,LSB) LSB,B2,B1,MSB
#define BIGlittle_8(MSB,B1,B2,B3,B4,B5,B6,LSB) LSB,B6,B5,B4,B3,B2,B1,MSB
#endif

const uint8_t mdb_basic_mdbInit_src[] =
{
/* header */
'C','u','b','i','c',' ','P','l','a','y','e','r',' ','M','o','d','u','l','e',' ','I','n','f','o','r','m','a','t','i','o','n',' ','D','a','t','a',' ','B','a','s','e',' ','I','I',0x1b,0,0,0,0,0,0,0,0,0,0,0,0,0,0,BIGlittle_1or1(0,1),BIGlittle_4(0,0,0,29),
/* file records
.........RECORD_FLAGS........... ....HASH...... ............SIZE............. ...MOD-TYPE.. FLG CHN ....PLAYTIME.... .........DATE........ .....TITLE_REF....... ....COMPOSER_REF..... .....ARTIST_REF...... .....STYLE_REF....... .....COMMENT_REF..... */
MDB_USED,                       1,2,3,4,5,6, 7,BIGlittle_8(0,0,0,0,0,10,0,0),'X','M', 0, 0, 1,  8, BIGlittle_2(1,2),BIGlittle_4( 4,3,2,1),BIGlittle_4(0,0,0, 2),BIGlittle_4(0,0,0, 3),BIGlittle_4(0,0,0, 4),BIGlittle_4(0,0,0, 5),BIGlittle_4(0,0,0, 6),BIGlittle_4(0,0,0, 7),0,0,0,0,0,0,0,0,0,0,0,0,
MDB_USED|MDB_STRING_TERMINATION,'T','i','t','l','e',' ','1',  0,  0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
MDB_USED|MDB_STRING_TERMINATION,'C','o','m','p','o','s','e','r',' ','1',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
MDB_USED|MDB_STRING_TERMINATION,'A','r','t','i','s','t',' ','1',  0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
MDB_USED|MDB_STRING_TERMINATION,'S','t','y','l','e',' ','1',  0,  0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
MDB_USED|MDB_STRING_TERMINATION,'C','o','m','m','e','n','t',' ','1',  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
MDB_USED|MDB_STRING_TERMINATION,'A','l','b','u','m',' ','1',  0,  0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
MDB_USED,                       4,5,6,7,8,9,10,BIGlittle_8(0,0,0,0,0, 9,0,0),'S','3','M',0, 2,  8, BIGlittle_2(3,4),BIGlittle_4( 6,5,4,3),BIGlittle_4(0,0,0, 9),BIGlittle_4(0,0,0,10),BIGlittle_4(0,0,0,11),BIGlittle_4(0,0,0,12),BIGlittle_4(0,0,0,13),BIGlittle_4(0,0,0,14),0,0,0,0,0,0,0,0,0,0,0,0,
MDB_USED|MDB_STRING_TERMINATION,'T','i','t','l','e',' ','2',  0,  0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
MDB_USED|MDB_STRING_TERMINATION,'C','o','m','p','o','s','e','r',' ','2',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
MDB_USED|MDB_STRING_TERMINATION,'A','r','t','i','s','t',' ','2',  0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
MDB_USED|MDB_STRING_TERMINATION,'S','t','y','l','e',' ','2',  0,  0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
MDB_USED|MDB_STRING_TERMINATION,'C','o','m','m','e','n','t',' ','2',  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
MDB_USED|MDB_STRING_TERMINATION,'A','l','b','u','m',' ','2',  0,  0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
MDB_USED,                       3,4,5,6,7,8, 9,BIGlittle_8(0,0,0,0,0, 9,0,0),'M','O','D',0, 3,  4, BIGlittle_2(5,6),BIGlittle_4( 8,7,6,5),BIGlittle_4(0,0,0,16),BIGlittle_4(0,0,0,17),BIGlittle_4(0,0,0,18),BIGlittle_4(0,0,0,19),BIGlittle_4(0,0,0,20),BIGlittle_4(0,0,0,21),0,0,0,0,0,0,0,0,0,0,0,0,
MDB_USED|MDB_STRING_TERMINATION,'T','i','t','l','e',' ','3',  0,  0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
MDB_USED|MDB_STRING_TERMINATION,'C','o','m','p','o','s','e','r',' ','3',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
MDB_USED|MDB_STRING_TERMINATION,'A','r','t','i','s','t',' ','3',  0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
MDB_USED|MDB_STRING_TERMINATION,'S','t','y','l','e',' ','3',  0,  0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
MDB_USED|MDB_STRING_TERMINATION,'C','o','m','m','e','n','t',' ','3',  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
MDB_USED|MDB_STRING_TERMINATION,'A','l','b','u','m',' ','3',  0,  0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
MDB_USED,                       2,3,4,5,6,7, 8,BIGlittle_8(0,0,0,0,0, 9,0,0),'I','T', 0, 0, 4,  6, BIGlittle_2(7,8),BIGlittle_4(10,9,8,7),BIGlittle_4(0,0,0,23),BIGlittle_4(0,0,0,24),BIGlittle_4(0,0,0,25),BIGlittle_4(0,0,0,26),BIGlittle_4(0,0,0,27),BIGlittle_4(0,0,0,28),0,0,0,0,0,0,0,0,0,0,0,0,
MDB_USED|MDB_STRING_TERMINATION,'T','i','t','l','e',' ','4',  0,  0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
MDB_USED|MDB_STRING_TERMINATION,'C','o','m','p','o','s','e','r',' ','4',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
MDB_USED|MDB_STRING_TERMINATION,'A','r','t','i','s','t',' ','4',  0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
MDB_USED|MDB_STRING_TERMINATION,'S','t','y','l','e',' ','4',  0,  0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
MDB_USED|MDB_STRING_TERMINATION,'C','o','m','m','e','n','t',' ','4',  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
MDB_USED|MDB_STRING_TERMINATION,'A','l','b','u','m',' ','4',  0,  0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};
int mdb_basic_mdbInit_src_pos;
int mdb_basic_mdbInit_src_isopen;

ssize_t mdb_basic_mdbInit_read (int *fd, void *buf, size_t size)
{
	ssize_t res;
	if (!mdb_basic_mdbInit_src_isopen || (fd != &fd_3))
	{
		errno = EBADF;
		return -1;
	}

	res = sizeof (mdb_basic_mdbInit_src) - mdb_basic_mdbInit_src_pos;
	if (res > size)
	{
		res = size;
	}
	if (res < 0)
	{
		return 0;
	}

	memcpy (buf, mdb_basic_mdbInit_src + mdb_basic_mdbInit_src_pos, res);
	mdb_basic_mdbInit_src_pos += res;
	return res;
}

int *mdb_basic_mdbInit_open (const char *pathname, int dolock, int mustcreate)
{
	/* TODO, verify flags and mode.. */

	mdb_basic_mdbInit_src_isopen++;

	return &fd_3;
}


int mdb_basic_mdbInit_close (int *fd)
{
	if (!mdb_basic_mdbInit_src_isopen || (fd != &fd_3))
	{
		errno = EBADF;
		return -1;
	}
	mdb_basic_mdbInit_src_isopen--;
	return 0;
}

off_t mdb_basic_mdbInit_lseek (int *fd, off_t offset)
{
	if (!mdb_basic_mdbInit_src_isopen || (fd != &fd_3))
	{
		errno = EBADF;
		return -1;
	}

	if (offset < 0)
	{
		errno = EINVAL;
		return (off_t) -1;
	}
	mdb_basic_mdbInit_src_pos = offset;
	return mdb_basic_mdbInit_src_pos;
}

void mdb_basic_mdbInit_prepare (void)
{
	mdbDataSize = 0;
	mdbData = 0;
	mdbDataNextFree = 0;

	mdbDirty = 0;
	mdbDirtyMapSize = 0;
	mdbDirtyMap = 0;

	mdbSearchIndexData = 0;
	mdbSearchIndexCount = 0;
	mdbSearchIndexSize = 0;

	mdb_basic_mdbInit_src_pos = 0;
	mdb_basic_mdbInit_src_isopen = 0;

	mdb_test_read_hook = mdb_basic_mdbInit_read;
	mdb_test_open_hook = mdb_basic_mdbInit_open;
	mdb_test_lseek_hook = mdb_basic_mdbInit_lseek;
	mdb_test_close_hook = mdb_basic_mdbInit_close;
}

void mdb_basic_mdbInit_finalize (void)
{
	free (mdbData);
	free (mdbDirtyMap);
	free (mdbSearchIndexData);

	mdb_test_read_hook = 0;
	mdb_test_open_hook = 0;
	mdb_test_lseek_hook = 0;
	mdb_test_close_hook = 0;
}

int mdb_basic_mdbInit (void)
{
	int retval = 0;
	struct moduleinfostruct m;
	int e;

	fprintf (stderr, ANSI_COLOR_CYAN "MDB mdbInit\n" ANSI_COLOR_RESET);

	mdb_basic_mdbInit_prepare ();

	fprintf (stderr, "mdbInit(): %s\n" ANSI_COLOR_RESET, mdbInit (0) ? ANSI_COLOR_GREEN "OK" : ANSI_COLOR_RED "Failed");

	fprintf (stderr, "mdbDataSize: ");
	if (mdbDataSize < 25) /* we do allow possible future pre-allocation of free units */
	{
		fprintf (stderr, ANSI_COLOR_RED " => %"PRIu32" < 25)\n" ANSI_COLOR_RESET, (int)mdbDataSize);
		retval |= 1;
	} else {
		fprintf (stderr, ANSI_COLOR_GREEN "OK\n" ANSI_COLOR_RESET);
	}

	fprintf (stderr, "mdbDataNextFree: ");
	if (mdbDataNextFree < 25)
	{
		fprintf (stderr, ANSI_COLOR_RED " => %"PRIu32" < 25)\n" ANSI_COLOR_RESET, mdbDataNextFree);
		retval |= 1;
	} else {
		fprintf (stderr, ANSI_COLOR_GREEN "OK\n" ANSI_COLOR_RESET);
	}

	fprintf (stderr, "mdbDirty: %s\n" ANSI_COLOR_RESET, mdbDirty ? ANSI_COLOR_RED "Failed" : ANSI_COLOR_GREEN "OK");

	fprintf (stderr, "mdbDirtyMapSize: ");
	if (mdbDirtyMapSize < 25)
	{
		fprintf (stderr, ANSI_COLOR_RED " => %"PRIu32" < 25)\n" ANSI_COLOR_RESET, mdbDirtyMapSize);
		retval |= 1;
	} else {
		fprintf (stderr, ANSI_COLOR_GREEN "OK\n" ANSI_COLOR_RESET);
	}

	fprintf (stderr, "mdbSearchIndex: ");
	if (mdbSearchIndexCount != 4)
	{
		fprintf (stderr, ANSI_COLOR_RED "mdbSearchIndexCount => %d >= 4\n" ANSI_COLOR_RESET, (int)mdbSearchIndexCount);
		retval |= 1;
	} else if ((mdbSearchIndexData[0] != 22) || (mdbSearchIndexData[1] != 15) || (mdbSearchIndexData[2] != 8) || (mdbSearchIndexData[3] != 1))
	{
		fprintf (stderr, ANSI_COLOR_RED "mdbSearchIndexData[] => 22, 15, 8, 1 != %d %d %d %d\n" ANSI_COLOR_RESET,
			(int)mdbSearchIndexData[0],
			(int)mdbSearchIndexData[1],
			(int)mdbSearchIndexData[2],
			(int)mdbSearchIndexData[3]);
		retval |= 1;
	} else {
		fprintf (stderr, ANSI_COLOR_GREEN "OK\n" ANSI_COLOR_RESET);
	}

	fprintf (stderr, "mdbInfoIsAvailable(1): %s\n" ANSI_COLOR_RESET, mdbInfoIsAvailable (0x00000001) ? ANSI_COLOR_GREEN "OK" : ANSI_COLOR_RED "Failed");
	e = 0;
	fprintf (stderr, "mdbGetModuleInfo(m,1):");
	if (!mdbGetModuleInfo(&m,1))
	{
		fprintf (stderr, ANSI_COLOR_RED " [call failed]");
		e++;
	} else {
		if (m.size != 0x0a0000)                { fprintf (stderr, ANSI_COLOR_RED " [size failed]");     e++; }
		if (strcmp (m.modtype.string.c, "XM")) { fprintf (stderr, ANSI_COLOR_RED " [modtype failed]");  e++; }
		if (m.flags != 1)                      { fprintf (stderr, ANSI_COLOR_RED " [flags failed]");    e++; }
		if (m.channels != 8)                   { fprintf (stderr, ANSI_COLOR_RED " [channels failed]"); e++; }
		if (m.playtime != 0x0102)              { fprintf (stderr, ANSI_COLOR_RED " [playtime failed]"); e++; }
		if (m.date != 0x04030201)              { fprintf (stderr, ANSI_COLOR_RED " [date failed]");     e++; }
		if (strcmp (m.title, "Title 1"))       { fprintf (stderr, ANSI_COLOR_RED " [title failed]");    e++; }
		if (strcmp (m.composer, "Composer 1")) { fprintf (stderr, ANSI_COLOR_RED " [composer failed]"); e++; }
		if (strcmp (m.artist, "Artist 1"))     { fprintf (stderr, ANSI_COLOR_RED " [artist failed]");   e++; }
		if (strcmp (m.style, "Style 1"))       { fprintf (stderr, ANSI_COLOR_RED " [style failed]");    e++; }
		if (strcmp (m.comment, "Comment 1"))   { fprintf (stderr, ANSI_COLOR_RED " [comment failed]");  e++; }
		if (strcmp (m.album, "Album 1"))       { fprintf (stderr, ANSI_COLOR_RED " [album failed]");    e++; }
	}
	fprintf (stderr, "%s\n" ANSI_COLOR_RESET, e ? "" : ANSI_COLOR_GREEN " OK");
	retval |= e;

	fprintf (stderr, "mdbClose():"); mdbClose(); fprintf (stderr, ANSI_COLOR_GREEN " OK\n" ANSI_COLOR_RESET);

	mdb_basic_mdbInit_finalize ();

	return retval;
}

uint8_t mdb_basic_mdbUpdate_data[65536];
int mdb_basic_mdbUpdate_pos;
int mdb_basic_mdbUpdate_isopen;
int mdb_basic_mdbUpdate_size;

int mdb_basic_mdbUpdate_pos;
int mdb_basic_mdbUpdate_isopen;
int mdb_basic_mdbUpdate_writeready;
int mdb_basic_mdbUpdate_writeerrors;

ssize_t mdb_basic_mdbUpdate_read (int *fd, void *buf, size_t size)
{
	ssize_t res;
	if (!mdb_basic_mdbUpdate_isopen || (fd != &fd_3))
	{
		errno = EBADF;
		return -1;
	}

	res = mdb_basic_mdbUpdate_size - mdb_basic_mdbUpdate_pos;
	if (res > size)
	{
		res = size;
	}
	if (res < 0)
	{
		return 0;
	}

	memcpy (buf, mdb_basic_mdbUpdate_data + mdb_basic_mdbUpdate_pos, res);
	mdb_basic_mdbUpdate_pos += res;
	return res;
}

ssize_t mdb_basic_mdbUpdate_write (int *fd, const void *buf, size_t size)
{
	if (!mdb_basic_mdbUpdate_isopen || (fd != &fd_3))
	{
		errno = EBADF;
		return -1;
	}

	if (    /* starts before, but crosses into the "do not update zone" */
		((mdb_basic_mdbUpdate_pos < (8*64)) && (mdb_basic_mdbUpdate_pos+size > (8*64))) ||
		/* starts inside the "do not update zone" */
		((mdb_basic_mdbUpdate_pos > (8*64) && (mdb_basic_mdbUpdate_pos < (24*64)) && size))
	)
	{
		fprintf (stderr, ANSI_COLOR_RED " [writing inside area that should not need rewrite]" ANSI_COLOR_RESET);
		mdb_basic_mdbUpdate_writeerrors++;
	}

	if (!mdb_basic_mdbUpdate_writeready)
	{
		fprintf (stderr, ANSI_COLOR_RED " [premature write: pos=%d size=%d]" ANSI_COLOR_RESET, (int)mdb_basic_mdbUpdate_pos, (int)size);
		mdb_basic_mdbUpdate_writeerrors++;
	}

	if ((mdb_basic_mdbUpdate_pos + size) > sizeof (mdb_basic_mdbUpdate_data))
	{
		fprintf (stderr, ANSI_COLOR_RED " [attempt to write beyond end of virtual device]" ANSI_COLOR_RESET);
		mdb_basic_mdbUpdate_writeerrors++;
		size = sizeof (mdb_basic_mdbUpdate_data) - mdb_basic_mdbUpdate_pos;
	}

	memcpy (mdb_basic_mdbUpdate_data + mdb_basic_mdbUpdate_pos, buf, size);
	mdb_basic_mdbUpdate_pos += size;
	return size;
}

int *mdb_basic_mdbUpdate_open (const char *pathname, int dolock, int mustcreate)
{
	/* TODO, verify flags and mode.. */

	mdb_basic_mdbUpdate_isopen++;

	return &fd_3;
}


int mdb_basic_mdbUpdate_close (int *fd)
{
	if (!mdb_basic_mdbUpdate_isopen || (fd != &fd_3))
	{
		errno = EBADF;
		return -1;
	}
	mdb_basic_mdbUpdate_isopen--;
	return 0;
}

off_t mdb_basic_mdbUpdate_lseek (int *fd, off_t offset)
{
	if (!mdb_basic_mdbUpdate_isopen || (fd != &fd_3))
	{
		errno = EBADF;
		return -1;
	}

	if (offset < 0)
	{
		errno = EINVAL;
		return (off_t) -1;
	}
	mdb_basic_mdbUpdate_pos = offset;
	return mdb_basic_mdbUpdate_pos;
}

void mdb_basic_mdbUpdate_prepare (void)
{
	mdbDataSize = 0;
	mdbData = 0;
	mdbDataNextFree = 0;

	mdbDirty = 0;
	mdbDirtyMapSize = 0;
	mdbDirtyMap = 0;

	mdbSearchIndexData = 0;
	mdbSearchIndexCount = 0;
	mdbSearchIndexSize = 0;

	memcpy (mdb_basic_mdbUpdate_data, mdb_basic_mdbInit_src, sizeof (mdb_basic_mdbInit_src));
	mdb_basic_mdbUpdate_size = sizeof (mdb_basic_mdbInit_src);
	mdb_basic_mdbUpdate_pos = 0;
	mdb_basic_mdbUpdate_isopen = 0;
	mdb_basic_mdbUpdate_writeready = 0;
	mdb_basic_mdbUpdate_writeerrors = 0;

	mdb_test_read_hook = mdb_basic_mdbUpdate_read;
	mdb_test_write_hook = mdb_basic_mdbUpdate_write;
	mdb_test_open_hook = mdb_basic_mdbUpdate_open;
	mdb_test_lseek_hook = mdb_basic_mdbUpdate_lseek;
	mdb_test_close_hook = mdb_basic_mdbUpdate_close;
}

void mdb_basic_mdbUpdate_finalize (void)
{
	free (mdbData);
	free (mdbDirtyMap);
	free (mdbSearchIndexData);

	mdb_test_read_hook = 0;
	mdb_test_write_hook = 0;
	mdb_test_open_hook = 0;
	mdb_test_lseek_hook = 0;
	mdb_test_close_hook = 0;
}

const uint8_t mdb_basic_mdbUpdate_added[] =
{
/* header */
/* file records
.........RECORD_FLAGS........... .........HASH........... ............SIZE............. ...MOD-TYPE... FLG CHN .....PLAYTIME..... ..........DATE......... .....TITLE_REF....... ....COMPOSER_REF..... .....ARTIST_REF...... .....STYLE_REF....... .....COMMENT_REF..... */
MDB_USED,                       136,26,188,231,165,95,155,BIGlittle_8(0,0,0,0,0,0,48,57),'M','O','D', 0, 0,  4, BIGlittle_2(84,50),BIGlittle_4(0,8,118,84),BIGlittle_4(0,0,0,30),BIGlittle_4(0,0,0,31),BIGlittle_4(0,0,0,32),BIGlittle_4(0,0,0,33),BIGlittle_4(0,0,0,34),BIGlittle_4(0,0,0,35),0,0,0,0,0,0,0,0,0,0,0,0,
MDB_USED|MDB_STRING_TERMINATION,'T','h','e',' ','t','i','t','l','e',  0,  0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
MDB_USED|MDB_STRING_TERMINATION,'T','h','e',' ','c','o','m','p','o','s','e','r',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
MDB_USED|MDB_STRING_TERMINATION,'T','h','e',' ','a','r','t','i','s','t',  0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
MDB_USED|MDB_STRING_TERMINATION,'T','h','e',' ','s','t','y','l','e',  0,  0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
MDB_USED|MDB_STRING_TERMINATION,'T','h','e',' ','c','o','m','m','e','n','t',  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
MDB_USED|MDB_STRING_TERMINATION,'T','h','e',' ','a','l','b','u','m',  0,  0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

int mdb_basic_mdbUpdate (void)
{
	int retval = 0;
	struct moduleinfostruct m;
	int e, i;
	uint32_t ref;

	fprintf (stderr, ANSI_COLOR_CYAN "MDB mdbUpdate (commit to disk)\n" ANSI_COLOR_RESET);

	mdb_basic_mdbUpdate_prepare ();

	if (!mdbInit (0))
	{
		fprintf (stderr, ANSI_COLOR_RED "mdbInit() failed...\n" ANSI_COLOR_RESET);
		retval |= 1;
	}

	ref = mdbGetModuleReference2 (1, 12345);
	if (ref != 29)
	{
		fprintf (stderr, ANSI_COLOR_RED "mdbGetModuleReference2() did not return 25, but %"PRIu32"\n" ANSI_COLOR_RESET, ref);
		retval |= 1;
	}
	mdbGetModuleInfo (&m, ref);
	m.modtype.string.c[0] = 'M';
	m.modtype.string.c[1] = 'O';
	m.modtype.string.c[2] = 'D';
	m.modtype.string.c[3] = 0;
	m.flags = 0;
	m.channels = 4;
	m.playtime = 0x5432;
	m.date = 0x00087654;
	strcpy (m.title, "The title");
	strcpy (m.composer, "The composer");
	strcpy (m.artist, "The artist");
	strcpy (m.style, "The style");
	strcpy (m.comment, "The comment");
	strcpy (m.album, "The album");
	if (!mdbWriteModuleInfo (ref, &m))
	{
		fprintf (stderr, ANSI_COLOR_RED "mdbWriteModuleInfo() failed\n" ANSI_COLOR_RESET);
		retval |= 1;
	}

	mdb_basic_mdbUpdate_writeready = 1;
	fprintf (stderr, "mdbUpdate: ");
	e = 0;
	mdbUpdate();
	if (mdbDirty)
	{
		fprintf (stderr, ANSI_COLOR_RED " [mdbDirty still set]");
		e++;
	}
	for (i=0; i < ((mdbDataSize + 7) / 8); i++)
	{
		if (mdbDirtyMap[i])
		{
			fprintf (stderr, ANSI_COLOR_RED " [mdbDirtyMap still set]");
			e++;
		}
	}

	if (memcmp (mdb_basic_mdbUpdate_data + 29 * 64, mdb_basic_mdbUpdate_added, sizeof (mdb_basic_mdbUpdate_added)))
	{
		int i;
		fprintf (stderr, ANSI_COLOR_RED " [data flushed to disk does not match expected data]");
		fprintf (stderr, ANSI_COLOR_RESET "\nexpected:");
		for (i=0; i<sizeof (mdb_basic_mdbUpdate_added); i++)
			fprintf (stderr, " %02x", ((uint8_t *)mdb_basic_mdbUpdate_added)[i]);
		fprintf (stderr, ANSI_COLOR_RESET "\nactual  :");
		for (i=0; i<sizeof (mdb_basic_mdbUpdate_added); i++)
			fprintf (stderr, " %02x", ((uint8_t *)(mdb_basic_mdbUpdate_data + 29 * 64))[i]);
		e++;
	}
	if (*(uint32_t *)(mdb_basic_mdbUpdate_data + 60) < 31)
	{
		fprintf (stderr, ANSI_COLOR_RED " [number of records in header < 31]");
		e++;
	}

	retval |= e;
	retval |= mdb_basic_mdbUpdate_writeerrors;
	fprintf (stderr, "%s\n" ANSI_COLOR_RESET, (mdb_basic_mdbUpdate_writeerrors || e) ? "" : ANSI_COLOR_GREEN "OK");

	fprintf (stderr, "mdbClose():"); mdbClose(); fprintf (stderr, ANSI_COLOR_GREEN " OK\n" ANSI_COLOR_RESET);

	mdb_basic_mdbUpdate_finalize ();

	return retval;
}

int main (int argc, char *argv[])
{
	int retval = 0;

	printf ("%ld\n", sizeof (mdb_basic_mdbInit_src));

	retval |= mdb_basic_sizeof();

	retval |= mdb_basic_mdbNew();

	retval |= mdb_heap1_mdbNew(1);

	retval |= mdb_heap1_mdbNew(2);

	retval |= mdb_basic_mdbFree();

	retval |= mdb_basic_mdbGetModuleReference ();

	retval |= mdb_basic_mdbWriteString ();

	retval |= mdb_basic_mdbGetString ();

	retval |= mdb_basic_mdbWriteModuleInfo_mdbGetModuleInfo ();

	retval |= mdb_basic_mdbInit();

	retval |= mdb_basic_mdbUpdate();

	return retval;
}
