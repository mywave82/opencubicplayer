/* OpenCP Module Player
 * copyright (c) 2020-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * unit test for dirdb.c
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

//#define DIRDB_DEBUG 1
#define CFDATAHOMEDIR_OVERRIDE "/tmp/"
#define CFHOMEDIR_OVERRIDE "/foo/home/ocp/"
#define MEASURESTR_UTF8_OVERRIDE

#include "dirdb.c"
#include "../stuff/compat.c"
#include "../stuff/file.c"

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

int (*_measurestr_utf8)(const char *src, int srclen) = 0;
uint32_t utf8_decode (const char *_src, size_t srclen, int *inc)
{
	*inc=1;
	return 1;
}

static void clear_dirdb()
{
	int i;
	for (i=0; i < dirdbNum; i++)
	{
		free(dirdbData[i].name);
		free(dirdbData[i].children);
	}
	free (dirdbData);
	dirdbData=0;
	dirdbNum=0;
	dirdbDirty=0;

	free (dirdbRootChildren);
	free (dirdbFreeChildren);

	dirdbRootChildren = 0;
	dirdbRootChildren_fill = 0;
	dirdbRootChildren_size = 0;
	dirdbFreeChildren = 0;
	dirdbFreeChildren_fill = 0;
	dirdbFreeChildren_size = 0;

	dirdbFreeChildren_size = FREE_MINSIZE;
	dirdbFreeChildren = malloc (sizeof (dirdbFreeChildren[0]) * dirdbFreeChildren_size);

	if (dirdbFile)
	{
		osfile_close(dirdbFile);
		dirdbFile = 0;
	}
	unlink (CFDATAHOMEDIR_OVERRIDE "CPDIRDB.DAT");
}

uint8_t mdbCleanSlate = 0;

static int dirdb_basic_test1(void)
{
	int i;
	int retval = 0;
	uint32_t node0;
	uint32_t node1;
	/*uint32_t node2;*/
	uint32_t node3;
	/*uint32_t node4;*/
	/*uint32_t node5;*/
	char test[65536+2+5];

	fprintf (stderr, ANSI_COLOR_CYAN "Corner cases with dirdbResolvePathAndRef()\n" ANSI_COLOR_RESET);

	/* trying to resolve empty string, should fail */
	node0 = dirdbResolvePathAndRef ("", dirdb_use_filehandle);
	if (node0 != DIRDB_NOPARENT)
	{
		fprintf (stderr, "dirdbResolvePathAndRef(" ANSI_COLOR_RED "\"\"" ANSI_COLOR_RESET ") gave a node, and not DIRDB_NOPARENT\n");
		retval++;
		dirdbUnref(node0, dirdb_use_filehandle);
	} else {
		fprintf (stderr, "dirdbResolvePathAndRef(" ANSI_COLOR_GREEN "\"\"" ANSI_COLOR_RESET ") did not give a node, but DIRDB_NOPARENT\n");
	}

	/* trying to resolve /, should fail */
	node0 = dirdbResolvePathAndRef ("/", dirdb_use_filehandle);
	if (node0 != DIRDB_NOPARENT)
	{
		fprintf (stderr, "dirdbResolvePathAndRef(" ANSI_COLOR_RED "\"/\"" ANSI_COLOR_RESET ") gave a node, and not DIRDB_NOPARENT\n");
		retval++;
		dirdbUnref(node0, dirdb_use_filehandle);
	} else {
		fprintf (stderr, "dirdbResolvePathAndRef(" ANSI_COLOR_GREEN "\"/\"" ANSI_COLOR_RESET ") did not give a node, but DIRDB_NOPARENT\n");
	}

	/* trying to resolve NULL, should fail */
	node0 = dirdbResolvePathAndRef (NULL, dirdb_use_filehandle);
	if (node0 != DIRDB_NOPARENT)
	{
		fprintf (stderr, "dirdbResolvePathAndRef(" ANSI_COLOR_RED "NULL" ANSI_COLOR_RESET ") gave a node, and not DIRDB_NOPARENT\n");
		retval++;
		dirdbUnref(node0, dirdb_use_filehandle);
	} else {
		fprintf (stderr, "dirdbResolvePathAndRef(" ANSI_COLOR_GREEN "NULL" ANSI_COLOR_RESET ") did not give a node, but DIRDB_NOPARENT\n");
	}

	/* trying to resolve empty drive, should be OK */
	node0 = dirdbResolvePathAndRef ("file:", dirdb_use_filehandle);
	if (node0 != DIRDB_NOPARENT)
	{
		fprintf (stderr, "dirdbResolvePathAndRef(" ANSI_COLOR_GREEN "file:" ANSI_COLOR_RESET ") gave a node, and not DIRDB_NOPARENT\n");
		dirdbUnref(node0, dirdb_use_filehandle);
	} else {
		fprintf (stderr, "dirdbResolvePathAndRef(" ANSI_COLOR_RED "file:" ANSI_COLOR_RESET ") did not give a node, but DIRDB_NOPARENT\n");
		retval++;
	}


	/* trying to resolve file:/a*65535 should work-out */
	test[0] = 'f';
	test[1] = 'i';
	test[2] = 'l';
	test[3] = 'e';
	test[4] = ':';
	test[5] = '/';
	for (i=0; i < 65535; i++)
	{
		test[i+6] = 'a';
	}
	test[6+65535] = 0;
	node0 = dirdbResolvePathAndRef (test, dirdb_use_filehandle);
	if (node0 == DIRDB_NOPARENT)
	{
		fprintf (stderr, "dirdbResolvePathAndRef(" ANSI_COLOR_RED "65535 character long name" ANSI_COLOR_RESET ") failed to resolve\n");
		retval++;
	} else {
		fprintf (stderr, "dirdbResolvePathAndRef(" ANSI_COLOR_GREEN "65535 character long name" ANSI_COLOR_RESET ") gave a node\n");
		dirdbUnref(node0, dirdb_use_filehandle);
	}

	/* trying to resolve /a*65536 should fail, we use uint16_t to store the length on disk */
	test[0] = 'f';
	test[1] = 'i';
	test[2] = 'l';
	test[3] = 'e';
	test[4] = ':';
	test[5] = '/';
	for (i=0; i < 65536; i++)
	{
		test[i+6] = 'a';
	}
	test[6+65536] = 0;

	node0 = dirdbResolvePathAndRef (test, dirdb_use_filehandle);
	if (node0 != DIRDB_NOPARENT)
	{
		fprintf (stderr, "dirdbResolvePathAndRef(" ANSI_COLOR_RED "65536 character long name" ANSI_COLOR_RESET ") gave a node, not DIRDB_NOPARENT\n");
		retval++;
	} else {
		fprintf (stderr, "dirdbResolvePathAndRef(" ANSI_COLOR_GREEN "65536 character long name" ANSI_COLOR_RESET ") gave a node\n");
		dirdbUnref(node0, dirdb_use_filehandle);
	}

	fprintf (stderr, ANSI_COLOR_CYAN "Creating nodes with dirdbResolvePathAndRef()\n" ANSI_COLOR_RESET);

	node1   =   dirdbResolvePathAndRef ("file:/tmp/foo/test.s3m", dirdb_use_filehandle);
	/*node2 =*/ dirdbResolvePathAndRef ("file:/tmp/foo/moo.s3m",  dirdb_use_filehandle);
	node3   =   dirdbResolvePathAndRef ("file:/tmp/foo/test.s3m", dirdb_use_filehandle);
	/*node4 =*/ dirdbResolvePathAndRef ("file:/tmp/moo.s3m",      dirdb_use_filehandle);
	/*node5 =*/ dirdbResolvePathAndRef ("file:/tmp/foo/",         dirdb_use_filehandle);

	if (node1 != node3)
	{
		fprintf (stderr, "Duplicated nodes \"file:/tmp/foo/test.s3m\" did not get matching ID's (" ANSI_COLOR_RED "%d" ANSI_COLOR_RESET " != " ANSI_COLOR_RED "%d" ANSI_COLOR_RESET ")\n", node1, node3);
		retval++;
	} else {
		fprintf (stderr, "Duplicate node \"file:/tmp/foo/test.s3m\" gave matching ID's (" ANSI_COLOR_GREEN "%d" ANSI_COLOR_RESET " == " ANSI_COLOR_GREEN "%d" ANSI_COLOR_RESET ")\n", node1, node3);
	}

	clear_dirdb();

	fprintf (stderr, "\n");

	return retval;
}

static int dirdb_basic_test2(void)
{
	int i;
	int retval = 0;
	uint32_t node1;
	uint32_t node2;
	uint32_t node3;
	uint32_t node4;
	uint32_t node5;
	uint32_t node6;
	uint32_t node7[16]; /* to trigger atleast one realloc() */

	fprintf (stderr, ANSI_COLOR_CYAN "Creating nodes with dirdbFindAndRef() + one duplicate\n" ANSI_COLOR_RESET);

	node1 = dirdbResolvePathAndRef ("file:/tmp/foo/test.s3m", dirdb_use_filehandle);
	node2 = dirdbResolvePathAndRef ("file:/tmp/foo/moo.s3m",  dirdb_use_filehandle);
	node3 = dirdbResolvePathAndRef ("file:/tmp/foo/test.s3m", dirdb_use_filehandle);
	node4 = dirdbResolvePathAndRef ("file:/tmp/moo.s3m",      dirdb_use_filehandle);
	node5 = dirdbResolvePathAndRef ("file:/tmp/foo/",         dirdb_use_filehandle);

	node6 = dirdbFindAndRef (node5, "test.s3m",               dirdb_use_filehandle);
	node7[0] = dirdbFindAndRef (node5, "test.mp0",            dirdb_use_filehandle);
	node7[1] = dirdbFindAndRef (node5, "test.mp1",            dirdb_use_filehandle);
	node7[2] = dirdbFindAndRef (node5, "test.mp2",            dirdb_use_filehandle);
	node7[3] = dirdbFindAndRef (node5, "test.mp3",            dirdb_use_filehandle);
	node7[4] = dirdbFindAndRef (node5, "test.mp4",            dirdb_use_filehandle);
	node7[5] = dirdbFindAndRef (node5, "test.mp5",            dirdb_use_filehandle);
	node7[6] = dirdbFindAndRef (node5, "test.mp6",            dirdb_use_filehandle);
	node7[7] = dirdbFindAndRef (node5, "test.mp7",            dirdb_use_filehandle);
	node7[8] = dirdbFindAndRef (node5, "test.mp8",            dirdb_use_filehandle);
	node7[9] = dirdbFindAndRef (node5, "test.mp9",            dirdb_use_filehandle);
	node7[10] = dirdbFindAndRef (node5, "test.mp10",          dirdb_use_filehandle);
	node7[11] = dirdbFindAndRef (node5, "test.mp11",          dirdb_use_filehandle);
	node7[12] = dirdbFindAndRef (node5, "test.mp12",          dirdb_use_filehandle);
	node7[13] = dirdbFindAndRef (node5, "test.mp13",          dirdb_use_filehandle);
	node7[14] = dirdbFindAndRef (node5, "test.mp14",          dirdb_use_filehandle);
	node7[15] = dirdbFindAndRef (node5, "test.mp15",          dirdb_use_filehandle);

	if (node1 != node6)
	{
		fprintf (stderr, "dirdbFindAndRef \"file:/tmp/foo\" + \"test.s3m\" did not get matching ID's (" ANSI_COLOR_RED "%d" ANSI_COLOR_RESET " != %d" ANSI_COLOR_RED ANSI_COLOR_RESET ")\n", node1, node6);
		retval++;
	} else {
		fprintf (stderr, "dirdbFindAndRef \"file:/tmp/foo\" + \"test.s3m\" gave matching ID's (" ANSI_COLOR_GREEN "%d" ANSI_COLOR_RESET " == " ANSI_COLOR_GREEN "%d" ANSI_COLOR_RESET ")\n", node1, node6);
	}

	for (i=0; i < 16; i++)
	{
		if (node7[i] == DIRDB_NOPARENT)
		{
			fprintf (stderr, "dirdbFindAndRef() failed for node index " ANSI_COLOR_RED "%d" ANSI_COLOR_RESET "\n", i);
			retval++;
		}
	}

	fprintf (stderr, ANSI_COLOR_CYAN "Testing dirdbRef()\n" ANSI_COLOR_RESET);

	if (node7[15] != DIRDB_NOPARENT) /* skip test, if earlier tests failed */
	{
		dirdbRef(node7[15], dirdb_use_filehandle);
		if (dirdbData[node7[15]].refcount != 2)
		{
			fprintf (stderr, "Refcount not 2 after dirdbRef() as expected, but " ANSI_COLOR_RED "%d" ANSI_COLOR_RESET "\n", dirdbData[node7[15]].refcount);
			retval++;
		}
		dirdbUnref(node7[15], dirdb_use_filehandle);
	}

	fprintf (stderr, "\n");

	fprintf (stderr, ANSI_COLOR_CYAN "Testing dirdbUnref()\n" ANSI_COLOR_RESET);
	dirdbUnref(node1, dirdb_use_filehandle);
	dirdbUnref(node2, dirdb_use_filehandle);
	dirdbUnref(node3, dirdb_use_filehandle);
	dirdbUnref(node4, dirdb_use_filehandle);
	dirdbUnref(node5, dirdb_use_filehandle);
	dirdbUnref(node6 ,dirdb_use_filehandle);
	for (i=0; i < 16; i++)
	{
		dirdbUnref(node7[i], dirdb_use_filehandle);
	}

	for (i=0; i < dirdbNum; i++)
	{
		if ((dirdbData[i].mdb_ref != DIRDB_NO_MDBREF) ||
		    (dirdbData[i].newmdb_ref != DIRDB_NO_MDBREF) ||
		    (dirdbData[i].name) ||
		    (dirdbData[i].parent != DIRDB_NOPARENT))
		{
			fprintf (stderr, "Entry " ANSI_COLOR_RED "%d" ANSI_COLOR_RESET " is not clean\n", i);
			retval++;
		}
	}

	clear_dirdb();

	fprintf (stderr, "\n");

	return retval;
}

static int dirdb_basic_test3(void)
{
	int i;
	int retval = 0;
	uint32_t node1, node1_, node1__;
	uint32_t node2;
	uint32_t node3;
	uint32_t node4;
	uint32_t node5, node5b, node5c;
	uint32_t node6;
	uint32_t node7;
	uint32_t node8;
	uint32_t node9;
	uint32_t node10;
	uint32_t node11;
	uint32_t node12;
	uint32_t node13, node13_, node13__, node13___, node13____;
	uint32_t node14;
	uint32_t node15;
	uint32_t node16;
	uint32_t node17;

	fprintf (stderr, ANSI_COLOR_CYAN "Going to test dirdbResolvePathWithBaseAndRef()\n" ANSI_COLOR_RESET);

#if 0
	node1  = dirdbResolvePathAndRef ("file:");
	node1_ = dirdbResolvePathAndRef ("file:/");
	node1__ = dirdbResolvePathAndRef ("file://");
	node2 = dirdbResolvePathAndRef ("file:/tmp");
	node3 = dirdbResolvePathAndRef ("file:/tmp/foo");
	node4 = dirdbResolvePathAndRef ("file:/tmp/foo/moo");
	node5 = dirdbResolvePathAndRef ("file:/tmp/foo/moo/super-power.mp3");
	node5b = dirdbResolvePathAndRef ("file:/foo/moo/super-power.mp3");
	node5c = dirdbResolvePathAndRef ("super:/tmp/foo/moo/super-power.mp3");

	node6 = dirdbResolvePathWithBaseAndRef (node2, "foo/moo/super-power.mp3");
	node7 = dirdbResolvePathWithBaseAndRef (node2, "/foo/moo/super-power.mp3");
	node8 = dirdbResolvePathWithBaseAndRef (node2, "foo///moo/super-power.mp3");
	node9 = dirdbResolvePathWithBaseAndRef (node2, NULL);
	node10 = dirdbResolvePathWithBaseAndRef (node2, "");
	node11 = dirdbResolvePathWithBaseAndRef (node2, ".");
	node12 = dirdbResolvePathWithBaseAndRef (node2, "./");

	node13     = dirdbResolvePathWithBaseAndRef (node3, "..");
	node13_    = dirdbResolvePathWithBaseAndRef (node3, "../");
	node13__   = dirdbResolvePathWithBaseAndRef (node3, "..//");
	node13___  = dirdbResolvePathWithBaseAndRef (node3, "..///");
	node13____ = dirdbResolvePathWithBaseAndRef (node3, "..//./");
	node14     = dirdbResolvePathWithBaseAndRef (node3, "../..");
	node15     = dirdbResolvePathWithBaseAndRef (node3, "../../..");
	node16     = dirdbResolvePathWithBaseAndRef (node3, "../../../../");

	node17 = dirdbResolvePathWithBaseAndRef (node3, "super:/tmp/foo/moo/super-power.mp3");
#endif

	fprintf (stderr, ANSI_COLOR_BLUE "1 - " ANSI_COLOR_RESET);
	node1  = dirdbResolvePathAndRef ("file:", dirdb_use_filehandle);
	node1_ = dirdbResolvePathAndRef ("file:/", dirdb_use_filehandle);
	if (node1 != node1_)
	{
		fprintf (stderr, ANSI_COLOR_RED "file:" ANSI_COLOR_RESET " and " ANSI_COLOR_RED "file:/" ANSI_COLOR_RESET " did not give us the same node\n");
		retval++;
	} else {
		fprintf (stderr, ANSI_COLOR_GREEN "file:" ANSI_COLOR_RESET " and " ANSI_COLOR_GREEN "file:/" ANSI_COLOR_RESET " give us the same node\n");
	}

	fprintf (stderr, ANSI_COLOR_BLUE "2 - " ANSI_COLOR_RESET);
	node1__ = dirdbResolvePathAndRef ("file://", dirdb_use_filehandle);
	if (node1 != node1__)
	{
		fprintf (stderr, ANSI_COLOR_RED "file:" ANSI_COLOR_RESET " and " ANSI_COLOR_RED "file://" ANSI_COLOR_RESET " did not give us the same node\n");
		retval++;
	} else {
		fprintf (stderr, ANSI_COLOR_GREEN "file:" ANSI_COLOR_RESET " and " ANSI_COLOR_GREEN "file://" ANSI_COLOR_RESET " give us the same node\n");
	}

	/* fill in some references */
	node2 = dirdbResolvePathAndRef ("file:/tmp",                           dirdb_use_filehandle);
	node3 = dirdbResolvePathAndRef ("file:/tmp/foo",                       dirdb_use_filehandle);
	node4 = dirdbResolvePathAndRef ("file:/tmp/foo/moo",                   dirdb_use_filehandle);
	node5 = dirdbResolvePathAndRef ("file:/tmp/foo/moo/super-power.mp3",   dirdb_use_filehandle);
	node5b = dirdbResolvePathAndRef ("file:/foo/moo/super-power.mp3",      dirdb_use_filehandle);
	node5c = dirdbResolvePathAndRef ("super:/tmp/foo/moo/super-power.mp3", dirdb_use_filehandle);

	fprintf (stderr, ANSI_COLOR_BLUE "3 - " ANSI_COLOR_RESET);
	node6 = dirdbResolvePathWithBaseAndRef (node2, "foo/moo/super-power.mp3", DIRDB_RESOLVE_DRIVE, dirdb_use_filehandle);
	if (node6 != node5)
	{
		fprintf (stderr, ANSI_COLOR_RED "file:/tmp  foo/moo/super-power.mp3" ANSI_COLOR_RESET " did not give file:/tmp/foo/moo/super-power.mp3\n");
		retval++;
	} else {
		fprintf (stderr, ANSI_COLOR_GREEN "file:/tmp  foo/moo/super-power.mp3" ANSI_COLOR_RESET " did give file:/tmp/foo/moo/super-power.mp3\n");
	}

	fprintf (stderr, ANSI_COLOR_BLUE "4 - " ANSI_COLOR_RESET);
	node7 = dirdbResolvePathWithBaseAndRef (node2, "/foo/moo/super-power.mp3", DIRDB_RESOLVE_DRIVE, dirdb_use_filehandle);
	if (node7 != node5b)
	{
		fprintf (stderr, ANSI_COLOR_RED "file:/tmp  /foo/moo/super-power.mp3" ANSI_COLOR_RESET " did not give file:/foo/moo/super-power.mp3\n");
		retval++;
	} else {
		fprintf (stderr, ANSI_COLOR_GREEN "file:/tmp  /foo/moo/super-power.mp3" ANSI_COLOR_RESET " did give file:/foo/moo/super-power.mp3\n");
	}

	fprintf (stderr, ANSI_COLOR_BLUE "5 - " ANSI_COLOR_RESET);
	node8 = dirdbResolvePathWithBaseAndRef (node2, "foo///moo/super-power.mp3", DIRDB_RESOLVE_DRIVE, dirdb_use_filehandle);
	if (node8 != node5)
	{
		fprintf (stderr, ANSI_COLOR_RED "file:/tmp  foo///moo/super-power.mp3" ANSI_COLOR_RESET " did not give file:/tmp/foo/moo/super-power.mp3\n");
		retval++;
	} else {
		fprintf (stderr, ANSI_COLOR_GREEN "file:/tmp  foo///moo/super-power.mp3" ANSI_COLOR_RESET " did give file:/tmp/foo/moo/super-power.mp3\n");
	}

	fprintf (stderr, ANSI_COLOR_BLUE "6 - " ANSI_COLOR_RESET);
	node9 = dirdbResolvePathWithBaseAndRef (node2, NULL, DIRDB_RESOLVE_DRIVE, dirdb_use_filehandle); /* no good defined result for this... */
	if (node9 != DIRDB_NOPARENT)
	{
		fprintf (stderr, ANSI_COLOR_RED "file:/tmp  NULL" ANSI_COLOR_RESET " did not give us DIRDB_NOPARENT\n");
		retval++;
		dirdbUnref(node9, dirdb_use_filehandle);
	} else {
		fprintf (stderr, ANSI_COLOR_GREEN "file:/tmp  NULL" ANSI_COLOR_RESET " gave us DIRDB_NOPARENT\n");
	}

	fprintf (stderr, ANSI_COLOR_BLUE "7 - " ANSI_COLOR_RESET);
	node10 = dirdbResolvePathWithBaseAndRef (node2, "", DIRDB_RESOLVE_DRIVE, dirdb_use_filehandle);
	if (node10 != node2)
	{
		fprintf (stderr, ANSI_COLOR_RED "\"file:/tmp  \"" ANSI_COLOR_RESET " did not give us parent\n");
		retval++;
	} else {
		fprintf (stderr, ANSI_COLOR_GREEN "\"file:/tmp  \"" ANSI_COLOR_RESET " gave us parent\n");
	}

	fprintf (stderr, ANSI_COLOR_BLUE "8 - " ANSI_COLOR_RESET);
	node11 = dirdbResolvePathWithBaseAndRef (node2, ".", DIRDB_RESOLVE_DRIVE, dirdb_use_filehandle);
	if (node11 != node2)
	{
		fprintf (stderr, ANSI_COLOR_RED "file:/tmp  .." ANSI_COLOR_RESET " did not maintain directory\n");
		retval++;
	} else {
		fprintf (stderr, ANSI_COLOR_GREEN "file:/tmp  ." ANSI_COLOR_RESET " did maintain directory\n");
	}

	fprintf (stderr, ANSI_COLOR_BLUE "9 - " ANSI_COLOR_RESET);
	node12 = dirdbResolvePathWithBaseAndRef (node2, "./", DIRDB_RESOLVE_DRIVE, dirdb_use_filehandle);
	if (node12 != node2)
	{
		fprintf (stderr, ANSI_COLOR_RED "file:/tmp  ./" ANSI_COLOR_RESET " did not maintain directory\n");
		retval++;
	} else {
		fprintf (stderr, ANSI_COLOR_GREEN "file:/tmp  ./" ANSI_COLOR_RESET " did maintain directory\n");
	}

	fprintf (stderr, ANSI_COLOR_BLUE "10 - " ANSI_COLOR_RESET);
	node13     = dirdbResolvePathWithBaseAndRef (node3, "..", DIRDB_RESOLVE_DRIVE, dirdb_use_filehandle);
	if (node13 != node2)
	{
		fprintf (stderr, ANSI_COLOR_RED ".." ANSI_COLOR_RESET " did not give us the parent\n");
		retval++;
	} else {
		fprintf (stderr, ANSI_COLOR_GREEN ".." ANSI_COLOR_RESET " gave us the parent\n");
	}

	fprintf (stderr, ANSI_COLOR_BLUE "11 - " ANSI_COLOR_RESET);
	node13_    = dirdbResolvePathWithBaseAndRef (node3, "../", DIRDB_RESOLVE_DRIVE, dirdb_use_filehandle);
	if (node13_ != node2)
	{
		fprintf (stderr, ANSI_COLOR_RED "../" ANSI_COLOR_RESET " did not give us the parent\n");
		retval++;
	} else {
		fprintf (stderr, ANSI_COLOR_GREEN "../" ANSI_COLOR_RESET " gave us the parent\n");
	}

	fprintf (stderr, ANSI_COLOR_BLUE "12 - " ANSI_COLOR_RESET);
	node13__   = dirdbResolvePathWithBaseAndRef (node3, "..//", DIRDB_RESOLVE_DRIVE, dirdb_use_filehandle);
	if (node13__ != node2)
	{
		fprintf (stderr, ANSI_COLOR_RED "..//" ANSI_COLOR_RESET " did not give us the parent\n");
		retval++;
	} else {
		fprintf (stderr, ANSI_COLOR_GREEN "..//" ANSI_COLOR_RESET " gave us the parent\n");
	}

	fprintf (stderr, ANSI_COLOR_BLUE "13 - " ANSI_COLOR_RESET);
	node13___  = dirdbResolvePathWithBaseAndRef (node3, "..///", DIRDB_RESOLVE_DRIVE, dirdb_use_filehandle);
	if (node13___ != node2)
	{
		fprintf (stderr, ANSI_COLOR_RED "..///" ANSI_COLOR_RESET " did not give us the parent\n");
		retval++;
	} else {
		fprintf (stderr, ANSI_COLOR_GREEN "..///" ANSI_COLOR_RESET " gave us the parent\n");
	}

	fprintf (stderr, ANSI_COLOR_BLUE "14 - " ANSI_COLOR_RESET);
	node13____ = dirdbResolvePathWithBaseAndRef (node3, "..//./", DIRDB_RESOLVE_DRIVE, dirdb_use_filehandle);
	if (node13____ != node2)
	{
		fprintf (stderr, ANSI_COLOR_RED "..//./" ANSI_COLOR_RESET " did not give us the parent\n");
		retval++;
	} else {
		fprintf (stderr, ANSI_COLOR_GREEN "..//./" ANSI_COLOR_RESET " gave us the parent\n");
	}

	fprintf (stderr, ANSI_COLOR_BLUE "15 - " ANSI_COLOR_RESET);
	node14     = dirdbResolvePathWithBaseAndRef (node3, "../..", DIRDB_RESOLVE_DRIVE, dirdb_use_filehandle);
	if (node14 != node1)
	{
		fprintf (stderr, ANSI_COLOR_RED "../.." ANSI_COLOR_RESET " did not give us the grand-parent (drive)\n");
		retval++;
	} else {
		fprintf (stderr, ANSI_COLOR_GREEN "../../" ANSI_COLOR_RESET " gave us the grand-parent (drive)\n");
	}

	fprintf (stderr, ANSI_COLOR_BLUE "16 - " ANSI_COLOR_RESET);
	node15     = dirdbResolvePathWithBaseAndRef (node3, "../../..", DIRDB_RESOLVE_DRIVE, dirdb_use_filehandle);
	if (node15 != node1)
	{
		fprintf (stderr, ANSI_COLOR_RED "../../.." ANSI_COLOR_RESET " (too many) did not give us the grand-parent (drive)\n");
		retval++;
	} else {
		fprintf (stderr, ANSI_COLOR_GREEN "../../.." ANSI_COLOR_RESET " (too many) gave us the grand-parent (drive)\n");
	}

	fprintf (stderr, ANSI_COLOR_BLUE "17 - " ANSI_COLOR_RESET);
	node16     = dirdbResolvePathWithBaseAndRef (node3, "../../../../", DIRDB_RESOLVE_DRIVE, dirdb_use_filehandle);
	if (node16 != node1)
	{
		fprintf (stderr, ANSI_COLOR_RED "../../../.." ANSI_COLOR_RESET " (too many) did not give us the grand-parent (drive)\n");
		retval++;
	} else {
		fprintf (stderr, ANSI_COLOR_GREEN "../../../.." ANSI_COLOR_RESET " (too many) gave us the grand-parent (drive)\n");
	}

	fprintf (stderr, ANSI_COLOR_BLUE "18 - " ANSI_COLOR_RESET);
	node17 = dirdbResolvePathWithBaseAndRef (node3, "super:/tmp/foo/moo/super-power.mp3", DIRDB_RESOLVE_DRIVE, dirdb_use_filehandle);
	if (node17 != node5c)
	{
		fprintf (stderr, ANSI_COLOR_RED "file:/tmp/foo  super:/tmp/foo/moo/super-power.mp3" ANSI_COLOR_RESET " did not change drive\n");
		retval++;
	} else {
		fprintf (stderr, ANSI_COLOR_GREEN "file:/tmp/foo  super:/tmp/foo/moo/super-power.mp3" ANSI_COLOR_RESET " did change drive\n");
	}

	dirdbUnref(node1, dirdb_use_filehandle);
	dirdbUnref(node1_, dirdb_use_filehandle);
	dirdbUnref(node1__, dirdb_use_filehandle);
	dirdbUnref(node2, dirdb_use_filehandle);
	dirdbUnref(node3, dirdb_use_filehandle);
	dirdbUnref(node4, dirdb_use_filehandle);
	dirdbUnref(node5, dirdb_use_filehandle);
	dirdbUnref(node5b, dirdb_use_filehandle);
	dirdbUnref(node5c, dirdb_use_filehandle);
	dirdbUnref(node6, dirdb_use_filehandle);
	dirdbUnref(node7, dirdb_use_filehandle);
	dirdbUnref(node8, dirdb_use_filehandle);

	dirdbUnref(node10, dirdb_use_filehandle);
	dirdbUnref(node11, dirdb_use_filehandle);
	dirdbUnref(node12, dirdb_use_filehandle);

	dirdbUnref(node13, dirdb_use_filehandle);
	dirdbUnref(node13_, dirdb_use_filehandle);
	dirdbUnref(node13__, dirdb_use_filehandle);
	dirdbUnref(node13___, dirdb_use_filehandle);
	dirdbUnref(node13____, dirdb_use_filehandle);
	dirdbUnref(node14, dirdb_use_filehandle);
	dirdbUnref(node15, dirdb_use_filehandle);
	dirdbUnref(node16, dirdb_use_filehandle);
	dirdbUnref(node17, dirdb_use_filehandle);


	node1  = dirdbResolvePathAndRef ("file:/",                              dirdb_use_filehandle);
	node2  = dirdbResolvePathAndRef ("file:/foo",                           dirdb_use_filehandle);
	node3  = dirdbResolvePathAndRef ("file:/foo/home",                      dirdb_use_filehandle);
	node4  = dirdbResolvePathAndRef ("file:/foo/home/ocp",                  dirdb_use_filehandle);
	node5  = dirdbResolvePathAndRef ("file:/foo/home/ocp/test",             dirdb_use_filehandle);
	node6  = dirdbResolvePathAndRef ("file:/foo/home/bar",                  dirdb_use_filehandle);

	fprintf (stderr, ANSI_COLOR_BLUE "19 - " ANSI_COLOR_RESET);
	node7 = dirdbResolvePathWithBaseAndRef (node1, "~/", DIRDB_RESOLVE_TILDE_HOME, dirdb_use_filehandle);
	if (node7 != node4)
	{
		fprintf (stderr, ANSI_COLOR_RED "file:/ ~/" ANSI_COLOR_RESET " did not give file:/foo/home/ocp\n");
		retval++;
	} else {
		fprintf (stderr, ANSI_COLOR_GREEN "file:/ ~/" ANSI_COLOR_RESET " did give file:/foo/home/ocp\n");
	}
	dirdbUnref (node7, dirdb_use_filehandle);

	fprintf (stderr, ANSI_COLOR_BLUE "20 - " ANSI_COLOR_RESET);
	node7 = dirdbResolvePathWithBaseAndRef (node5, "~/", DIRDB_RESOLVE_TILDE_HOME, dirdb_use_filehandle);
	if (node7 != node4)
	{
		fprintf (stderr, ANSI_COLOR_RED "file:/foo/home/ocp/test ~/" ANSI_COLOR_RESET " did not give file:/foo/home/ocp\n");
		retval++;
	} else {
		fprintf (stderr, ANSI_COLOR_GREEN "file:/foo/home/ocp/test ~/" ANSI_COLOR_RESET " did give file:/foo/home/ocp\n");
	}
	dirdbUnref (node7, dirdb_use_filehandle);

	fprintf (stderr, ANSI_COLOR_BLUE "21 - " ANSI_COLOR_RESET);
	node7 = dirdbResolvePathWithBaseAndRef (node6, "~/test", DIRDB_RESOLVE_TILDE_HOME, dirdb_use_filehandle);
	if (node7 != node5)
	{
		fprintf (stderr, ANSI_COLOR_RED "file:/foo/home/bar/ ~/test" ANSI_COLOR_RESET " did not give file:/foo/home/ocp/test\n");
		retval++;
	} else {
		fprintf (stderr, ANSI_COLOR_GREEN "file:/foo/home/bar/ ~/test" ANSI_COLOR_RESET " did give file:/foo/home/ocp/test\n");
	}
	dirdbUnref (node7, dirdb_use_filehandle);

	dirdbUnref (node1, dirdb_use_filehandle);
	dirdbUnref (node2, dirdb_use_filehandle);
	dirdbUnref (node3, dirdb_use_filehandle);
	dirdbUnref (node4, dirdb_use_filehandle);
	dirdbUnref (node5, dirdb_use_filehandle);
	dirdbUnref (node6, dirdb_use_filehandle);

	for (i=0; i < dirdbNum; i++)
	{
		if ((dirdbData[i].mdb_ref != DIRDB_NO_MDBREF) ||
		    (dirdbData[i].newmdb_ref != DIRDB_NO_MDBREF) ||
		    (dirdbData[i].name) ||
		    (dirdbData[i].parent != DIRDB_NOPARENT))
		{
			fprintf (stderr, "Entry " ANSI_COLOR_RED "%d" ANSI_COLOR_RESET " is not clean\n", i);
			retval++;
		}
	}

	clear_dirdb();

	fprintf (stderr, "\n");

	return retval;
}

static int dirdb_basic_test4(void)
{
	int retval = 0;
	uint32_t node0;
	uint32_t node5;

	fprintf (stderr, ANSI_COLOR_CYAN "Testing dirdbFindAndRef() corner-cases\n" ANSI_COLOR_RESET);

	node5 = dirdbResolvePathAndRef ("file:/tmp/foo/moo/foo", dirdb_use_filehandle);

	node0 = dirdbFindAndRef (node5, NULL, dirdb_use_filehandle);
	if (node0 != DIRDB_NOPARENT)
	{
		fprintf (stderr, "dirdbFindAndRef(validparent, " ANSI_COLOR_RED "NULL" ANSI_COLOR_RESET") did not fail as expected\n");
		retval++;
		dirdbUnref(node0, dirdb_use_filehandle);
	} else {
		fprintf (stderr, "dirdbFindAndRef(validparent, " ANSI_COLOR_GREEN "NULL" ANSI_COLOR_RESET") failed as expected\n");
	}

	node0 = dirdbFindAndRef (node5, "", dirdb_use_filehandle);
	if (node0 != DIRDB_NOPARENT)
	{
		fprintf (stderr, "dirdbFindAndRef(validparent, " ANSI_COLOR_RED "\"\"" ANSI_COLOR_RESET") did not fail as expected\n");
		retval++;
		dirdbUnref(node0, dirdb_use_filehandle);
	} else {
		fprintf (stderr, "dirdbFindAndRef(validparent, " ANSI_COLOR_GREEN "\"\"" ANSI_COLOR_RESET") failed as expected\n");
	}

	node0 = dirdbFindAndRef (node5, ".", dirdb_use_filehandle);
	if (node0 != DIRDB_NOPARENT)
	{
		fprintf (stderr, "dirdbFindAndRef(validparent, \"" ANSI_COLOR_RED "." ANSI_COLOR_RESET"\") did not fail as expected\n");
		retval++;
		dirdbUnref(node0, dirdb_use_filehandle);
	} else {
		fprintf (stderr, "dirdbFindAndRef(validparent, \"" ANSI_COLOR_GREEN "." ANSI_COLOR_RESET"\") failed as expected\n");
	}
	node0 = dirdbFindAndRef (node5, "..", dirdb_use_filehandle);
	if (node0 != DIRDB_NOPARENT)
	{
		fprintf (stderr, "dirdbFindAndRef(validparent, \"" ANSI_COLOR_RED ".." ANSI_COLOR_RESET"\") did not fail as expected\n");
		retval++;
		dirdbUnref(node0, dirdb_use_filehandle);
	} else {
		fprintf (stderr, "dirdbFindAndRef(validparent, \"" ANSI_COLOR_GREEN ".." ANSI_COLOR_RESET"\") failed as expected\n");
	}

	node0 = dirdbFindAndRef (node5, "tmp/", dirdb_use_filehandle);
	if (node0 != DIRDB_NOPARENT)
	{
		fprintf (stderr, "dirdbFindAndRef(validparent, \"" ANSI_COLOR_RED "tmp/" ANSI_COLOR_RESET"\") did not fail as expected\n");
		retval++;
		dirdbUnref(node0, dirdb_use_filehandle);
	} else {
		fprintf (stderr, "dirdbFindAndRef(validparent, \"" ANSI_COLOR_GREEN "tmp/" ANSI_COLOR_RESET"\") failed as expected\n");
	}

	node0 = dirdbFindAndRef (0xffff0000, "test", dirdb_use_filehandle);
	if (node0 != DIRDB_NOPARENT)
	{
		fprintf (stderr, "dirdbFindAndRef(" ANSI_COLOR_RED "invalidparent" ANSI_COLOR_RESET ", \"test\") did not fail as expected\n");
		retval++;
		dirdbUnref(node0, dirdb_use_filehandle);
	} else {
		fprintf (stderr, "dirdbFindAndRef(" ANSI_COLOR_GREEN "invalidparent" ANSI_COLOR_RESET ", \"test\") failed as expected\n");
	}

	clear_dirdb();
	fprintf (stderr, "\n");

	return retval;
}

static int dirdb_basic_test5(void)
{
	int retval = 0;
	/*uint32_t node2;*/
	/*uint32_t node3;*/
	uint32_t node4;
	/*uint32_t node5;*/
	char *tmp;

	fprintf (stderr, ANSI_COLOR_CYAN "Testing dirdbGetName_malloc()\n" ANSI_COLOR_RESET);

	/*node2 =*/ dirdbResolvePathAndRef ("file:/tmp", dirdb_use_filehandle);
	/*node3 =*/ dirdbResolvePathAndRef ("file:/tmp/foo", dirdb_use_filehandle);
	  node4 =   dirdbResolvePathAndRef ("file:/tmp/foo/moo", dirdb_use_filehandle);
	/*node5 =*/ dirdbResolvePathAndRef ("file:/tmp/foo/moo/super-power.mp3", dirdb_use_filehandle);

	tmp = "TEST";
	dirdbGetName_malloc (node4, &tmp);
	if (!tmp)
	{
		fprintf (stderr, "dirdbGetName_malloc(valid_node) => " ANSI_COLOR_RED "Failed, got NULL\n" ANSI_COLOR_RESET);
		retval++;
	} else {
		if (strcmp (tmp, "moo"))
		{
			fprintf (stderr, "dirdbGetName_malloc(valid_node) => \"" ANSI_COLOR_RED "%s" ANSI_COLOR_RESET "\" instead of moo\n", tmp);
			retval++;
		} else {
			fprintf (stderr, "dirdbGetName_malloc(valid_node) => \"" ANSI_COLOR_GREEN "moo" ANSI_COLOR_RESET "\"\n");
		}
		free (tmp);
	}

	dirdbGetName_malloc (DIRDB_NOPARENT, &tmp);
	if (tmp)
	{
		fprintf (stderr, "dirdbGetName_malloc(DIRDB_NOPARENT) => \"" ANSI_COLOR_RED "%s" ANSI_COLOR_RESET " instead of NULL\n", tmp);
		free (tmp);
		retval++;
	} else {
		fprintf (stderr, "dirdbGetName_malloc(DIRDB_NOPARENT) => " ANSI_COLOR_GREEN "NULL" ANSI_COLOR_RESET "\n");
	}

	dirdbGetName_malloc (0xffff0000, &tmp);
	if (tmp)
	{
		fprintf (stderr, "dirdbGetName_malloc(invalid_node) => \"" ANSI_COLOR_RED "%s" ANSI_COLOR_RESET " instead of NULL\n", tmp);
		free (tmp);
		retval++;
	} else {
		fprintf (stderr, "dirdbGetName_malloc(invalid_node) => " ANSI_COLOR_GREEN "NULL" ANSI_COLOR_RESET "\n");
	}

	clear_dirdb();

	fprintf (stderr, "\n");

	return retval;
}

static int dirdbGetFullname_malloc_subtest(const uint32_t node, const char *expected, const int flags)
{
	int retval = 0;
	const char *org = "TEST";
	char *tmp = (char *)org;

	dirdbGetFullname_malloc (node, &tmp, flags);
	if (!tmp)
	{
		fprintf (stderr, ANSI_COLOR_RED "Failed, got NULL\n" ANSI_COLOR_RESET);
	} else {
		if (strcmp (tmp, expected))
		{
			fprintf (stderr, "Got " ANSI_COLOR_RED "%s" ANSI_COLOR_RESET " instead of %s\n", tmp, expected);
			retval++;
		} else {
			fprintf (stderr, "Got " ANSI_COLOR_GREEN "%s" ANSI_COLOR_RESET "\n", tmp);
		}
		if (tmp != org)
		{
			free (tmp);
		}
	}

	return retval;
}

static int dirdb_basic_test6(void)
{
	int retval = 0;
	uint32_t node1, node4;

	  node1 =   dirdbResolvePathAndRef ("file:/", dirdb_use_filehandle);
	/*node2 =*/ dirdbResolvePathAndRef ("file:/tmp", dirdb_use_filehandle);
	/*node3 =*/ dirdbResolvePathAndRef ("file:/tmp/foo", dirdb_use_filehandle);
	  node4 =   dirdbResolvePathAndRef ("file:/tmp/foo/moo", dirdb_use_filehandle);
	/*node5 =*/ dirdbResolvePathAndRef ("file:/tmp/foo/moo/super-power.mp3", dirdb_use_filehandle);

	fprintf (stderr, ANSI_COLOR_CYAN "Testing dirdbGetFullname_malloc(no flags)\n" ANSI_COLOR_RESET);
	retval |= dirdbGetFullname_malloc_subtest (node4, "file:/tmp/foo/moo", 0);

	fprintf (stderr, ANSI_COLOR_CYAN "Testing dirdbGetFullname_malloc(DIRDB_FULLNAME_NODRIVE)\n" ANSI_COLOR_RESET);
	retval |= dirdbGetFullname_malloc_subtest (node4, "/tmp/foo/moo", DIRDB_FULLNAME_NODRIVE);

	fprintf (stderr, ANSI_COLOR_CYAN "Testing dirdbGetFullname_malloc(DIRDB_FULLNAME_NODRIVE | DIRDB_FULLNAME_ENDSLASH)\n" ANSI_COLOR_RESET);
	retval |= dirdbGetFullname_malloc_subtest (node4, "/tmp/foo/moo/", DIRDB_FULLNAME_NODRIVE | DIRDB_FULLNAME_ENDSLASH);

	fprintf (stderr, ANSI_COLOR_CYAN "Testing dirdbGetFullname_malloc(DIRDB_FULLNAME_ENDSLASH)\n" ANSI_COLOR_RESET);
	retval |= dirdbGetFullname_malloc_subtest (node4, "file:/tmp/foo/moo/", DIRDB_FULLNAME_ENDSLASH);

	fprintf (stderr, ANSI_COLOR_CYAN "Testing dirdbGetFullname_malloc(no flags)\n" ANSI_COLOR_RESET);
	retval |= dirdbGetFullname_malloc_subtest (node1, "file:", 0);

	fprintf (stderr, ANSI_COLOR_CYAN "Testing dirdbGetFullname_malloc(DIRDB_FULLNAME_NODRIVE)\n" ANSI_COLOR_RESET);
	retval |= dirdbGetFullname_malloc_subtest (node1, "", DIRDB_FULLNAME_NODRIVE);

	fprintf (stderr, ANSI_COLOR_CYAN "Testing dirdbGetFullname_malloc(DIRDB_FULLNAME_NODRIVE | DIRDB_FULLNAME_ENDSLASH)\n" ANSI_COLOR_RESET);
	retval |= dirdbGetFullname_malloc_subtest (node1, "/", DIRDB_FULLNAME_NODRIVE | DIRDB_FULLNAME_ENDSLASH);

	fprintf (stderr, ANSI_COLOR_CYAN "Testing dirdbGetFullname_malloc(DIRDB_FULLNAME_ENDSLASH)\n" ANSI_COLOR_RESET);
	retval |= dirdbGetFullname_malloc_subtest (node1, "file:/", DIRDB_FULLNAME_ENDSLASH);

	fprintf (stderr, ANSI_COLOR_CYAN "Testing dirdbGetFullname_malloc(DIRDB_FULLNAME_ENDSLASH | DIRDB_FULLNAME_BACKSLASH)\n" ANSI_COLOR_RESET);
	retval |= dirdbGetFullname_malloc_subtest (node4, "file:\\tmp\\foo\\moo\\", DIRDB_FULLNAME_ENDSLASH | DIRDB_FULLNAME_BACKSLASH);

	clear_dirdb();

	fprintf (stderr, "\n");

	return retval;
}

static int dirdb_basic_test7(void)
{
	int retval = 0;
 /* dirdbTagSetParent(), dirdbMakeMdbRef(), dirdbTagRemoveUntaggedAndSubmit(), dirdbGetMdb() */

	uint32_t node1 = dirdbResolvePathAndRef ("file:/tmp", dirdb_use_filehandle);
	uint32_t node2 = dirdbResolvePathAndRef ("file:/tmp/foo", dirdb_use_filehandle);
	uint32_t node3 = dirdbResolvePathAndRef ("file:/tmp/foo/test1.mp3", dirdb_use_filehandle);
	uint32_t node4 = dirdbResolvePathAndRef ("file:/tmp/foo/test2.mp3", dirdb_use_filehandle);
	uint32_t node5 = dirdbResolvePathAndRef ("file:/tmp/bar", dirdb_use_filehandle);
	uint32_t node6, node7, node8;
	int i;

	int first;
	uint32_t iter, mdb;
	int found_node3, found_node4, found_node6, found_node7, found_node8;

	fprintf (stderr, ANSI_COLOR_CYAN "Testing initial dirdbTagSetParent(), dirdbMakeMdbRef(), dirdbTagRemoveUntaggedAndSubmit() and dirdbGetMdb()\n" ANSI_COLOR_RESET);

	dirdbTagSetParent (node1);

	dirdbMakeMdbRef (node3, 1);
	dirdbMakeMdbRef (node4, 2);
	node6 = dirdbResolvePathAndRef ("file:/tmp/bar/test3.mp3", dirdb_use_filehandle);
	node7 = dirdbResolvePathAndRef ("file:/tmp/bar/test4.mp3", dirdb_use_filehandle);
	dirdbMakeMdbRef (node6, 3);
	dirdbMakeMdbRef (node7, 4);

	dirdbTagRemoveUntaggedAndSubmit ();

	dirdbUnref (node1, dirdb_use_filehandle);
	dirdbUnref (node2, dirdb_use_filehandle);
	dirdbUnref (node3, dirdb_use_filehandle);
	dirdbUnref (node4, dirdb_use_filehandle);
	dirdbUnref (node5, dirdb_use_filehandle);
	dirdbUnref (node6, dirdb_use_filehandle);
	dirdbUnref (node7, dirdb_use_filehandle);

	first=1;
	iter = 0xa1234;
	mdb = 0xa5678;
	found_node3=0;
	found_node4=0;
	found_node6=0;
	found_node7=0;
	while (!dirdbGetMdb(&iter, &mdb, &first))
	{
		     if ((iter==node3) && (mdb==1)) found_node3++;
		else if ((iter==node4) && (mdb==2)) found_node4++;
		else if ((iter==node6) && (mdb==3)) found_node6++;
		else if ((iter==node7) && (mdb==4)) found_node7++;
		else {
			fprintf (stderr, "dirdbGetMdb() gave an unknown node " ANSI_COLOR_RED "iter=0x%08"PRIx32" mdb=%d" ANSI_COLOR_RESET "\n", iter, mdb);
			retval++;
		}
	}
	if (found_node3 != 1) {
		fprintf (stderr, "dirdbGetMdb() did not reveal " ANSI_COLOR_RED "iter=%d mdb=1" ANSI_COLOR_RESET "\n", node3);
		retval++;
	} else {
		fprintf (stderr, "dirdbGetMdb() gave us " ANSI_COLOR_GREEN "iter=%d mdb=1" ANSI_COLOR_RESET "\n", node3);
	}
	if (found_node4 != 1) {
		fprintf (stderr, "dirdbGetMdb() did not reveal " ANSI_COLOR_RED "iter=%d mdb=2" ANSI_COLOR_RESET "\n", node4);
		retval++;
	} else {
		fprintf (stderr, "dirdbGetMdb() gave us " ANSI_COLOR_GREEN "iter=%d mdb=2" ANSI_COLOR_RESET "\n", node4);
	}
	if (found_node6 != 1) {
		fprintf (stderr, "dirdbGetMdb() did not reveal " ANSI_COLOR_RED "iter=%d mdb=3" ANSI_COLOR_RESET "\n", node6);
		retval++;
	} else {
		fprintf (stderr, "dirdbGetMdb() gave us " ANSI_COLOR_GREEN "iter=%d mdb=3" ANSI_COLOR_RESET "\n", node6);
	}
	if (found_node7 != 1) {
		fprintf (stderr, "dirdbGetMdb() did not reveal " ANSI_COLOR_RED "iter=%d mdb=4" ANSI_COLOR_RESET "\n", node7);
		retval++;
	} else {
		fprintf (stderr, "dirdbGetMdb() gave us " ANSI_COLOR_GREEN "iter=%d mdb=4" ANSI_COLOR_RESET "\n", node7);
	}



	fprintf (stderr, ANSI_COLOR_CYAN "Testing patching with dirdbTagSetParent(), dirdbMakeMdbRef(), dirdbTagRemoveUntaggedAndSubmit() and dirdbGetMdb()\n" ANSI_COLOR_RESET);

	dirdbTagSetParent (node1);

	dirdbMakeMdbRef (node3, 1);
	node8 = dirdbResolvePathAndRef ("file:/tmp/bar/test5.mp3", dirdb_use_filehandle);
	dirdbMakeMdbRef (node6, 3);
	dirdbMakeMdbRef (node8, 5);
	dirdbUnref (node8, dirdb_use_filehandle);

	dirdbTagRemoveUntaggedAndSubmit ();

	first=1;
	iter = 0xa1234;
	mdb = 0xa5678;
	found_node3=0;
	found_node6=0;
	found_node8=0;
	while (!dirdbGetMdb(&iter, &mdb, &first))
	{
		fprintf (stderr, "%d %d\n", iter, mdb);
		     if ((iter==node3) && (mdb==1)) found_node3++;
		else if ((iter==node6) && (mdb==3)) found_node6++;
		else if ((iter==node8) && (mdb==5)) found_node8++;
		else {
			fprintf (stderr, "dirdbGetMdb() gave an unknown node " ANSI_COLOR_RED "iter=%d mdb=%d" ANSI_COLOR_RESET "\n", iter, mdb);
			retval++;
		}
	}
	if (found_node3 != 1) {
		fprintf (stderr, "dirdbGetMdb() did not reveal " ANSI_COLOR_RED "iter=%d mdb=1" ANSI_COLOR_RESET "\n", node3);
		retval++;
	} else {
		fprintf (stderr, "dirdbGetMdb() gave us " ANSI_COLOR_GREEN "iter=%d mdb=1" ANSI_COLOR_RESET "\n", node3);
	}
	if (found_node6 != 1) {
		fprintf (stderr, "dirdbGetMdb() did not reveal " ANSI_COLOR_RED "iter=%d mdb=3" ANSI_COLOR_RESET "\n", node6);
		retval++;
	} else {
		fprintf (stderr, "dirdbGetMdb() gave us " ANSI_COLOR_GREEN "iter=%d mdb=3" ANSI_COLOR_RESET "\n", node6);
	}
	if (found_node8 != 1) {
		fprintf (stderr, "dirdbGetMdb() did not reveal " ANSI_COLOR_RED "iter=%d mdb=5" ANSI_COLOR_RESET "\n", node8);
		retval++;
	} else {
		fprintf (stderr, "dirdbGetMdb() gave us " ANSI_COLOR_GREEN "iter=%d mdb=5" ANSI_COLOR_RESET "\n", node8);
	}


	fprintf (stderr, ANSI_COLOR_CYAN "Testing clearing with dirdbTagSetParent(), dirdbTagRemoveUntaggedAndSubmit() and dirdbGetMdb()\n" ANSI_COLOR_RESET);

	dirdbTagSetParent (DIRDB_NOPARENT);
	dirdbTagRemoveUntaggedAndSubmit ();

	first=1;
	iter = 0xa1234;
	mdb = 0xa5678;
	while (!dirdbGetMdb(&iter, &mdb, &first))
	{
		fprintf (stderr, "dirdbGetMdb() gave a node " ANSI_COLOR_RED "iter=%d mdb=%d" ANSI_COLOR_RESET "\n", iter, mdb);
		retval++;
	}

	for (i=0; i < dirdbNum; i++)
	{
		if ((dirdbData[i].mdb_ref != DIRDB_NO_MDBREF) ||
		    (dirdbData[i].newmdb_ref != DIRDB_NO_MDBREF) ||
		    (dirdbData[i].name) ||
		    (dirdbData[i].parent != DIRDB_NOPARENT))
		{
			fprintf (stderr, "Entry " ANSI_COLOR_RED "%d" ANSI_COLOR_RESET " is not clean\n", i);
			retval++;
		}
	}

	clear_dirdb();

	return retval;
}

static int dirdb_basic_test8_subtest (uint32_t base, uint32_t node, char *expected_dir, int windows)
{
	char *base_str;
	char *node_str;
	char *diff;
	int retval = 0;

	if (base != DIRDB_NOPARENT)
	{
		dirdbGetFullname_malloc (base, &base_str, 0);
	} else {
		base_str = strdup("NULL");
	}

	if (node != DIRDB_NOPARENT)
	{
		dirdbGetFullname_malloc (node, &node_str, 0);
	} else {
		node_str = strdup("NULL");
	}

	diff = dirdbDiffPath (base, node, windows?DIRDB_DIFF_WINDOWS_SLASH:0);

	if ((!diff) != (!expected_dir))
	{
		retval |= 1;
	} else if (diff && strcmp (diff, expected_dir))
	{
		retval |= 2;
	}

	if (retval)
	{
		fprintf (stderr, "dirdbDiffPath("
		                  ANSI_COLOR_RED "%s"
		                  ANSI_COLOR_RESET ", "
		                  ANSI_COLOR_RED "%s"
		                  ANSI_COLOR_RESET ", "
		                  ANSI_COLOR_RED "%s"
		                  ANSI_COLOR_RESET"%s) FAILED, expected %s\n",
		                  base_str, node_str, diff?diff:"NULL", windows?" (windows)":"",expected_dir?expected_dir:"NULL");
	} else {
		fprintf (stderr, "dirdbDiffPath("
		                  ANSI_COLOR_GREEN "%s"
		                  ANSI_COLOR_RESET ", "
		                  ANSI_COLOR_GREEN "%s"
		                  ANSI_COLOR_RESET ", "
		                  ANSI_COLOR_GREEN "%s"
		                  ANSI_COLOR_RESET"%s) OK\n",
		                  base_str, node_str, diff?diff:"NULL", windows?" (windows)":"");
	}
	free (base_str);
	free (node_str);
	free (diff);
	return retval;
}

static int dirdb_basic_test8(void)
{
	int retval = 0;

	uint32_t node0a = dirdbResolvePathAndRef ("file:/", dirdb_use_filehandle);
	uint32_t node1a = dirdbResolvePathAndRef ("file:/tmp", dirdb_use_filehandle);
	uint32_t node2a = dirdbResolvePathAndRef ("file:/tmp/foo", dirdb_use_filehandle);
	uint32_t node3a = dirdbResolvePathAndRef ("file:/tmp/foo/test1.mp3", dirdb_use_filehandle);
	uint32_t node4a = dirdbResolvePathAndRef ("file:/tmp/foo/test2.mp3", dirdb_use_filehandle);
	uint32_t node5a = dirdbResolvePathAndRef ("file:/tmp/bar", dirdb_use_filehandle);
	uint32_t node6a = dirdbResolvePathAndRef ("file:/tmp/bar/test3.mp3", dirdb_use_filehandle);
	uint32_t node7a = dirdbResolvePathAndRef ("file:/nope", dirdb_use_filehandle);
	uint32_t node8a = dirdbResolvePathAndRef ("file:/nope/blab/", dirdb_use_filehandle);
	uint32_t node9a = dirdbResolvePathAndRef ("file:/nope/blab/test4.mp3", dirdb_use_filehandle);
	uint32_t node10 = dirdbResolvePathAndRef ("file:/nope/\\foo\\/test5.mp3", dirdb_use_filehandle);

	uint32_t node0b = dirdbResolvePathAndRef ("setup:/", dirdb_use_filehandle);
	uint32_t node1b = dirdbResolvePathAndRef ("setup:/tmp", dirdb_use_filehandle);
	uint32_t node2b = dirdbResolvePathAndRef ("setup:/tmp/foo", dirdb_use_filehandle);
	uint32_t node3b = dirdbResolvePathAndRef ("setup:/tmp/foo/test1.mp3", dirdb_use_filehandle);
	uint32_t node4b = dirdbResolvePathAndRef ("setup:/tmp/foo/test2.mp3", dirdb_use_filehandle);
	uint32_t node5b = dirdbResolvePathAndRef ("setup:/tmp/bar", dirdb_use_filehandle);
	uint32_t node6b = dirdbResolvePathAndRef ("setup:/tmp/bar/test3.mp3", dirdb_use_filehandle);
	uint32_t node7b = dirdbResolvePathAndRef ("setup:/nope", dirdb_use_filehandle);
	uint32_t node8b = dirdbResolvePathAndRef ("setup:/nope/blab/", dirdb_use_filehandle);
	uint32_t node9b = dirdbResolvePathAndRef ("setup:/nope/blab/test4.mp3", dirdb_use_filehandle);

	fprintf (stderr, ANSI_COLOR_CYAN "Testing initial dirdbDiffPath()\n" ANSI_COLOR_RESET);

	retval |= dirdb_basic_test8_subtest (node0a, node0a, "./", 0);
	retval |= dirdb_basic_test8_subtest (node1a, node1a, "./", 0);

	retval |= dirdb_basic_test8_subtest (node0a, node1a, "tmp", 0);
	retval |= dirdb_basic_test8_subtest (node0a, node2a, "tmp/foo", 0);
	retval |= dirdb_basic_test8_subtest (node0a, node3a, "tmp/foo/test1.mp3", 0);

	retval |= dirdb_basic_test8_subtest (node1a, node2a, "foo", 0);
	retval |= dirdb_basic_test8_subtest (node1a, node3a, "foo/test1.mp3", 0);

	retval |= dirdb_basic_test8_subtest (node2a, node3a, "test1.mp3", 0);

	retval |= dirdb_basic_test8_subtest (node3a, node2a, "../", 0);
	retval |= dirdb_basic_test8_subtest (node3a, node1a, "../../", 0);
	retval |= dirdb_basic_test8_subtest (node3a, node6a, "../../bar/test3.mp3", 0); /* in the test, we are using the base, a mp3 file as it was a directory name we are inside */

	retval |= dirdb_basic_test8_subtest (node3a, node0a, "/", 0);
	retval |= dirdb_basic_test8_subtest (node3a, node9a, "/nope/blab/test4.mp3", 0);

	retval |= dirdb_basic_test8_subtest (node0a, node0b, "setup:/", 0);
	retval |= dirdb_basic_test8_subtest (node0a, node1b, "setup:/tmp", 0);
	retval |= dirdb_basic_test8_subtest (node0a, node3b, "setup:/tmp/foo/test1.mp3", 0);
	retval |= dirdb_basic_test8_subtest (node3a, node3b, "setup:/tmp/foo/test1.mp3", 0);

	retval |= dirdb_basic_test8_subtest (node9a, node10, "../../\\foo\\/test5.mp3", 0); /* test that \\ works */
	retval |= dirdb_basic_test8_subtest (node9a, node10, "..\\..\\/foo/\\test5.mp3", 1); /* test that slashes reverses fully on windows */

	retval |= dirdb_basic_test8_subtest (node3a, DIRDB_NOPARENT, NULL, 0);

	retval |= dirdb_basic_test8_subtest (DIRDB_NOPARENT, node3a, "file:/tmp/foo/test1.mp3", 0); /* does the same as dirdbGetFullname_malloc() */


	fprintf (stderr, "\n");

	dirdbUnref (node0a, dirdb_use_filehandle);
	dirdbUnref (node1a, dirdb_use_filehandle);
	dirdbUnref (node2a, dirdb_use_filehandle);
	dirdbUnref (node3a, dirdb_use_filehandle);
	dirdbUnref (node4a, dirdb_use_filehandle);
	dirdbUnref (node5a, dirdb_use_filehandle);
	dirdbUnref (node6a, dirdb_use_filehandle);
	dirdbUnref (node7a, dirdb_use_filehandle);
	dirdbUnref (node8a, dirdb_use_filehandle);
	dirdbUnref (node9a, dirdb_use_filehandle);

	dirdbUnref (node0b, dirdb_use_filehandle);
	dirdbUnref (node1b, dirdb_use_filehandle);
	dirdbUnref (node2b, dirdb_use_filehandle);
	dirdbUnref (node3b, dirdb_use_filehandle);
	dirdbUnref (node4b, dirdb_use_filehandle);
	dirdbUnref (node5b, dirdb_use_filehandle);
	dirdbUnref (node6b, dirdb_use_filehandle);
	dirdbUnref (node7b, dirdb_use_filehandle);
	dirdbUnref (node8b, dirdb_use_filehandle);
	dirdbUnref (node9b, dirdb_use_filehandle);

	clear_dirdb();

	return retval;
}

static int dirdb_basic_test9(void)
{
	int retval1 = 0;
	int retval2 = 0;

	fprintf (stderr, ANSI_COLOR_CYAN "Testing that dirdbFindAndRef() keeps all nodes sorted\n" ANSI_COLOR_RESET);

	uint32_t node0_file = dirdbResolvePathAndRef ("file:/", dirdb_use_dir);
	uint32_t node0_c = dirdbResolvePathAndRef ("c:/", dirdb_use_dir);
	uint32_t node0_setup = dirdbResolvePathAndRef ("setup:/", dirdb_use_dir);

	uint32_t node0_file_aab = dirdbResolvePathAndRef ("file:/aab.mod", dirdb_use_file);
	uint32_t node0_file_baa = dirdbResolvePathAndRef ("file:/baa.mod", dirdb_use_file);
	uint32_t node0_file_aba = dirdbResolvePathAndRef ("file:/aba.mod", dirdb_use_file);
	uint32_t node0_file_aaa = dirdbResolvePathAndRef ("file:/aaa.mod", dirdb_use_file);
	uint32_t node0_file_abb = dirdbResolvePathAndRef ("file:/abb.mod", dirdb_use_file);
	uint32_t node0_file_cab = dirdbResolvePathAndRef ("file:/cab.mod", dirdb_use_file);
	uint32_t node0_file_bab = dirdbResolvePathAndRef ("file:/bab.mod", dirdb_use_file);
	uint32_t node0_file_caa = dirdbResolvePathAndRef ("file:/caa.mod", dirdb_use_file);

	if ((dirdbRootChildren_fill < 1) || (dirdbRootChildren[0] != node0_c    )) { fprintf (stderr, ANSI_COLOR_RED "c: not in position 0 in root\n"); retval1++; }
	if ((dirdbRootChildren_fill < 2) || (dirdbRootChildren[1] != node0_file )) { fprintf (stderr, ANSI_COLOR_RED "file: not in position 1 in root\n"); retval1++; }
	if ((dirdbRootChildren_fill < 3) || (dirdbRootChildren[2] != node0_setup)) { fprintf (stderr, ANSI_COLOR_RED "setup: not in position 2 in root\n"); retval1++; }

	if ((dirdbData[node0_file].children_fill < 1) || dirdbData[node0_file].children[0] != node0_file_aaa) { fprintf (stderr, ANSI_COLOR_RED "aaa.mod not in position 0 in c:\n"); retval1++; }
	if ((dirdbData[node0_file].children_fill < 2) || dirdbData[node0_file].children[1] != node0_file_aab) { fprintf (stderr, ANSI_COLOR_RED "aab.mod not in position 1 in c:\n"); retval1++; }
	if ((dirdbData[node0_file].children_fill < 3) || dirdbData[node0_file].children[2] != node0_file_aba) { fprintf (stderr, ANSI_COLOR_RED "aba.mod not in position 2 in c:\n"); retval1++; }
	if ((dirdbData[node0_file].children_fill < 4) || dirdbData[node0_file].children[3] != node0_file_abb) { fprintf (stderr, ANSI_COLOR_RED "abb.mod not in position 3 in c:\n"); retval1++; }
	if ((dirdbData[node0_file].children_fill < 5) || dirdbData[node0_file].children[4] != node0_file_baa) { fprintf (stderr, ANSI_COLOR_RED "baa.mod not in position 4 in c:\n"); retval1++; }
	if ((dirdbData[node0_file].children_fill < 6) || dirdbData[node0_file].children[5] != node0_file_bab) { fprintf (stderr, ANSI_COLOR_RED "bab.mod not in position 5 in c:\n"); retval1++; }
	if ((dirdbData[node0_file].children_fill < 7) || dirdbData[node0_file].children[6] != node0_file_caa) { fprintf (stderr, ANSI_COLOR_RED "caa.mod not in position 6 in c:\n"); retval1++; }
	if ((dirdbData[node0_file].children_fill < 8) || dirdbData[node0_file].children[7] != node0_file_cab) { fprintf (stderr, ANSI_COLOR_RED "cab.mod not in position 7 in c:\n"); retval1++; }

	if (!retval1)
	{
		fprintf (stderr, ANSI_COLOR_GREEN "All good\n");
	}
	fprintf (stderr, ANSI_COLOR_RESET "\n");

	fprintf (stderr, ANSI_COLOR_CYAN "Testing that dirdbUnref() removes the correct node from the sorted list\n");
	dirdbUnref (node0_file_aab, dirdb_use_file);
	if ((dirdbData[node0_file].children_fill < 1) || dirdbData[node0_file].children[0] != node0_file_aaa) { fprintf (stderr, ANSI_COLOR_RED "aaa.mod not in position 0 in c: after removing aab.mod\n"); retval2++; }
	if ((dirdbData[node0_file].children_fill < 2) || dirdbData[node0_file].children[1] != node0_file_aba) { fprintf (stderr, ANSI_COLOR_RED "aba.mod not in position 1 in c: after removing aab.mod\n"); retval2++; }
	if ((dirdbData[node0_file].children_fill < 3) || dirdbData[node0_file].children[2] != node0_file_abb) { fprintf (stderr, ANSI_COLOR_RED "abb.mod not in position 2 in c: after removing aab.mod\n"); retval2++; }
	if ((dirdbData[node0_file].children_fill < 4) || dirdbData[node0_file].children[3] != node0_file_baa) { fprintf (stderr, ANSI_COLOR_RED "baa.mod not in position 3 in c: after removing aab.mod\n"); retval2++; }
	if ((dirdbData[node0_file].children_fill < 5) || dirdbData[node0_file].children[4] != node0_file_bab) { fprintf (stderr, ANSI_COLOR_RED "bab.mod not in position 4 in c: after removing aab.mod\n"); retval2++; }
	if ((dirdbData[node0_file].children_fill < 6) || dirdbData[node0_file].children[5] != node0_file_caa) { fprintf (stderr, ANSI_COLOR_RED "caa.mod not in position 5 in c: after removing aab.mod\n"); retval2++; }
	if ((dirdbData[node0_file].children_fill < 7) || dirdbData[node0_file].children[6] != node0_file_cab) { fprintf (stderr, ANSI_COLOR_RED "cab.mod not in position 6 in c: after removing aab.mod\n"); retval2++; }

	dirdbUnref (node0_file_aaa, dirdb_use_file);
	if ((dirdbData[node0_file].children_fill < 1) || dirdbData[node0_file].children[0] != node0_file_aba) { fprintf (stderr, ANSI_COLOR_RED "aba.mod not in position 0 in c: after removing aaa.mod aab.mod\n"); retval2++; }
	if ((dirdbData[node0_file].children_fill < 2) || dirdbData[node0_file].children[1] != node0_file_abb) { fprintf (stderr, ANSI_COLOR_RED "abb.mod not in position 1 in c: after removing aaa.mod aab.mod\n"); retval2++; }
	if ((dirdbData[node0_file].children_fill < 3) || dirdbData[node0_file].children[2] != node0_file_baa) { fprintf (stderr, ANSI_COLOR_RED "baa.mod not in position 2 in c: after removing aaa.mod aab.mod\n"); retval2++; }
	if ((dirdbData[node0_file].children_fill < 4) || dirdbData[node0_file].children[3] != node0_file_bab) { fprintf (stderr, ANSI_COLOR_RED "bab.mod not in position 3 in c: after removing aaa.mod aab.mod\n"); retval2++; }
	if ((dirdbData[node0_file].children_fill < 5) || dirdbData[node0_file].children[4] != node0_file_caa) { fprintf (stderr, ANSI_COLOR_RED "caa.mod not in position 4 in c: after removing aaa.mod aab.mod\n"); retval2++; }
	if ((dirdbData[node0_file].children_fill < 6) || dirdbData[node0_file].children[5] != node0_file_cab) { fprintf (stderr, ANSI_COLOR_RED "cab.mod not in position 5 in c: after removing aaa.mod aab.mod\n"); retval2++; }

	dirdbUnref (node0_file_cab, dirdb_use_file);
	if ((dirdbData[node0_file].children_fill < 1) || dirdbData[node0_file].children[0] != node0_file_aba) { fprintf (stderr, ANSI_COLOR_RED "aba.mod not in position 0 in c: after removing aaa.mod aab.mod cab.mod\n"); retval2++; }
	if ((dirdbData[node0_file].children_fill < 2) || dirdbData[node0_file].children[1] != node0_file_abb) { fprintf (stderr, ANSI_COLOR_RED "abb.mod not in position 1 in c: after removing aaa.mod aab.mod cab.mod\n"); retval2++; }
	if ((dirdbData[node0_file].children_fill < 3) || dirdbData[node0_file].children[2] != node0_file_baa) { fprintf (stderr, ANSI_COLOR_RED "baa.mod not in position 2 in c: after removing aaa.mod aab.mod cab.mod\n"); retval2++; }
	if ((dirdbData[node0_file].children_fill < 4) || dirdbData[node0_file].children[3] != node0_file_bab) { fprintf (stderr, ANSI_COLOR_RED "bab.mod not in position 3 in c: after removing aaa.mod aab.mod cab.mod\n"); retval2++; }
	if ((dirdbData[node0_file].children_fill < 5) || dirdbData[node0_file].children[4] != node0_file_caa) { fprintf (stderr, ANSI_COLOR_RED "caa.mod not in position 4 in c: after removing aaa.mod aab.mod cab.mod\n"); retval2++; }

	dirdbUnref (node0_file_bab, dirdb_use_file);
	if ((dirdbData[node0_file].children_fill < 1) || dirdbData[node0_file].children[0] != node0_file_aba) { fprintf (stderr, ANSI_COLOR_RED "aba.mod not in position 0 in c: after removing aaa.mod aab.mod bab.mod cab.mod\n"); retval2++; }
	if ((dirdbData[node0_file].children_fill < 2) || dirdbData[node0_file].children[1] != node0_file_abb) { fprintf (stderr, ANSI_COLOR_RED "abb.mod not in position 1 in c: after removing aaa.mod aab.mod bab.mod cab.mod\n"); retval2++; }
	if ((dirdbData[node0_file].children_fill < 3) || dirdbData[node0_file].children[2] != node0_file_baa) { fprintf (stderr, ANSI_COLOR_RED "baa.mod not in position 2 in c: after removing aaa.mod aab.mod bab.mod cab.mod\n"); retval2++; }
	if ((dirdbData[node0_file].children_fill < 4) || dirdbData[node0_file].children[3] != node0_file_caa) { fprintf (stderr, ANSI_COLOR_RED "caa.mod not in position 3 in c: after removing aaa.mod aab.mod bab.mod cab.mod\n"); retval2++; }

	dirdbUnref (node0_file_aba, dirdb_use_file);
	dirdbUnref (node0_file_abb, dirdb_use_file);
	dirdbUnref (node0_file_baa, dirdb_use_file);
	dirdbUnref (node0_file_caa, dirdb_use_file);
	if (dirdbData[node0_file].children_fill) { fprintf (stderr, ANSI_COLOR_RED "c: not empty after removing all the children\n"); retval2++; }

	if (!retval2)
	{
		fprintf (stderr, ANSI_COLOR_GREEN "All good\n");
	}

	fprintf (stderr, ANSI_COLOR_RESET "\n");

	clear_dirdb();

	return retval1 + retval2;
}

static int dirdb_basic_test10(void)
{
	int retval1 = 0;
	int retval2 = 0;

	uint32_t node0_z = dirdbResolvePathAndRef ("z:/", dirdb_use_dir);
	/*uint32_t node0_z_z =*/ dirdbResolvePathAndRef ("z:/z.txt", dirdb_use_dir);
	uint32_t node = 0, iter;
	uint32_t orig_size = dirdbRootChildren_size;

	fprintf (stderr, ANSI_COLOR_CYAN "dirdbFindAndRef() Growing the root children list\n" ANSI_COLOR_RESET);
	for (node = 0; dirdbRootChildren_size == orig_size; node++)
	{
		char temp[20];
		snprintf (temp, sizeof(temp), "C_%08u:", (unsigned int)node);
		dirdbResolvePathAndRef (temp, dirdb_use_dir);
		if (dirdbRootChildren_fill != (node + 2)) { fprintf (stderr, ANSI_COLOR_RED "dirdbRootChildren_fill has unexpected value (%"PRIu32" vs %"PRIu32")\n", dirdbRootChildren_fill, node + 2); retval1++; }
		if (dirdbRootChildren_fill > dirdbRootChildren_size) { fprintf (stderr, ANSI_COLOR_RED "dirdbRootChildren_fill (%"PRIu32") > dirdbRootChildren_size %"PRIu32")\n", dirdbRootChildren_fill, dirdbRootChildren_size); retval1++; }

		if (node > 500) { fprintf (stderr, ANSI_COLOR_RED "Giving up waiting for grow\n"); retval1++; break; }
	}
	if (node != orig_size)
	{
		fprintf (stderr, ANSI_COLOR_RED "List grew at unexpected size %"PRIu32" instead of %"PRIu32"\n", node, orig_size);
		retval1++;
	} else {
		fprintf (stderr, ANSI_COLOR_RESET "List grew at " ANSI_COLOR_GREEN "%"PRIu32" " ANSI_COLOR_RESET "nodes - OK\n", node);
	}

	for (iter = 0; iter < node; iter++)
	{
		char temp[20];
		snprintf (temp, sizeof (temp), "C_%08u:", (unsigned int)iter);
		if (strcmp (dirdbData[dirdbRootChildren[iter]].name, temp))
		{
			fprintf (stderr, ANSI_COLOR_RED "Child at offset %"PRIu32" has unexpected name \"%s\" instead of \"%s\"\n", iter, dirdbData[dirdbRootChildren[iter]].name, temp);
			retval1++;
		}
	}
	if (strcmp (dirdbData[dirdbRootChildren[iter]].name, "z:"))
	{
		fprintf (stderr, ANSI_COLOR_RED "Child at offset %"PRIu32" has unexpected name \"%s\" instead of \"z:\"\n", iter, dirdbData[dirdbRootChildren[iter]].name);
		retval1++;
	}

	fprintf (stderr, ANSI_COLOR_RESET "\n");

	fprintf (stderr, ANSI_COLOR_CYAN "dirdbFindAndRef() Growing the non-root children list\n" ANSI_COLOR_RESET);
	node = 0;
	orig_size = dirdbData[node0_z].children_size;

	for (node = 0; dirdbData[node0_z].children_size == orig_size; node++)
	{
		char temp[20];
		snprintf (temp, sizeof (temp), "z:/C_%08u.txt", (unsigned int)node);
		dirdbResolvePathAndRef (temp, dirdb_use_file);
		if (dirdbData[node0_z].children_fill != (node + 2)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[node0_z].children_fill has unexpected value (%"PRIu32" vs %"PRIu32")\n", dirdbData[node0_z].children_fill, node + 2); retval2++; }
		if (dirdbData[node0_z].children_fill > dirdbData[node0_z].children_size) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[node0_z].children_fill (%"PRIu32") > dirdbData[node0_z].children_size %"PRIu32")\n", dirdbData[node0_z].children_fill, dirdbData[node0_z].children_size); retval2++; }
		if (node > 500) { fprintf (stderr, ANSI_COLOR_RED "Giving up waiting for grow\n"); retval2++; break; }
	}
	if (node != orig_size)
	{
		fprintf (stderr, ANSI_COLOR_RED "List grew at unexpected size %"PRIu32" instead of %"PRIu32"\n", node, orig_size);
		retval1++;
	} else {
		fprintf (stderr, ANSI_COLOR_RESET "List grew at " ANSI_COLOR_GREEN "%"PRIu32" " ANSI_COLOR_RESET "nodes - OK\n", node);
	}

	for (iter = 0; iter < node; iter++)
	{
		char temp[20];
		snprintf (temp, sizeof (temp), "C_%08u.txt", (unsigned int)iter);
		if (strcmp (dirdbData[dirdbData[node0_z].children[iter]].name, temp))
		{
			fprintf (stderr, ANSI_COLOR_RED "Child at offset %"PRIu32" has unexpected name \"%s\" instead of \"%s\"\n", iter, dirdbData[dirdbData[node0_z].children[iter]].name, temp);
			retval2++;
		}
	}
	if (strcmp (dirdbData[dirdbData[node0_z].children[iter]].name, "z.txt"))
	{
		fprintf (stderr, ANSI_COLOR_RED "Child at offset %"PRIu32" has unexpected name \"%s\" instead of \"z:\"\n", iter, dirdbData[dirdbData[node0_z].children[iter]].name);
		retval2++;
	}

	fprintf (stderr, ANSI_COLOR_RESET "\n");

	clear_dirdb();

	return retval1 + retval2;
}

static int dirdb_basic_test11(void)
{
	int retval = 0;
	uint32_t node = 0, iter;
	uint32_t orig_size = 0;

	fprintf (stderr, ANSI_COLOR_CYAN "dirdbFindAndRef() Growing the database\n" ANSI_COLOR_RESET);

	for (node = 0; (!orig_size) || (dirdbNum == orig_size); node++)
	{
		char temp[20];
		snprintf (temp, sizeof (temp), "c_%08u:", (unsigned int)node);
		dirdbResolvePathAndRef (temp, dirdb_use_dir);
		if (!orig_size)
		{
			orig_size = dirdbNum;
		}
		if (node > 500) { fprintf (stderr, ANSI_COLOR_RED "Giving up waiting for grow\n"); retval++; break; }
	}
	if ((node - 1) != orig_size)
	{
		fprintf (stderr, ANSI_COLOR_RED "Database grew at unexpected size %"PRIu32" instead of %"PRIu32"\n", node, orig_size);
		retval++;
	} else {
		fprintf (stderr, ANSI_COLOR_RESET "Database grew at " ANSI_COLOR_GREEN "%"PRIu32" " ANSI_COLOR_RESET "nodes - OK\n", node);
	}

	for (iter = 0; iter < node; iter++)
	{
		char temp[20];
		snprintf (temp, sizeof (temp), "c_%08u:", (unsigned int)iter);
		if (strcmp (dirdbData[dirdbRootChildren[iter]].name, temp))
		{
			fprintf (stderr, ANSI_COLOR_RED "Child at offset %"PRIu32" has unexpected name \"%s\" instead of \"%s\"\n", iter, dirdbData[dirdbRootChildren[iter]].name, temp);
			retval++;
		}
	}

	if (!retval)
	{
		fprintf (stderr, ANSI_COLOR_GREEN "All good\n");
	}

	fprintf (stderr, ANSI_COLOR_RESET "\n");

	clear_dirdb();

	return retval;
}

static int dirdb_basic_test12(void)
{
	int retval = 0;
	uint32_t orig_size = dirdbFreeChildren_size;
	uint32_t iter, node;

	fprintf (stderr, ANSI_COLOR_CYAN "dirdbUnref() Growing the freelist\n" ANSI_COLOR_RESET);

	for (iter = 0; iter < (orig_size * 2); iter++)
	{
		char temp[20];
		snprintf (temp, sizeof (temp), "c_%08u:", (unsigned int)iter);
		dirdbResolvePathAndRef (temp, dirdb_use_dir);
	}

	for (iter = 0; iter < (orig_size * 2); iter++)
	{
		char temp[20];
		snprintf (temp, sizeof (temp), "c_%08u:", (unsigned int)iter);
		node = dirdbResolvePathAndRef (temp, dirdb_use_dir);
		dirdbUnref (node, dirdb_use_dir);
		dirdbUnref (node, dirdb_use_dir);
	}

	if (dirdbFreeChildren_fill != dirdbNum)
	{
		fprintf (stderr, ANSI_COLOR_RED "dirdbFreeChildren_fill %"PRIu32" != dirdbNum %"PRIu32"\n", dirdbFreeChildren_fill, dirdbNum);
		retval++;
	}

	for (iter = 0; iter < dirdbNum; iter++)
	{
		int found = 0;
		uint32_t iter2;
		for (iter2 = 0; iter2 < dirdbFreeChildren_fill; iter2++)
		{
			if (dirdbFreeChildren[iter2] == iter)
			{
				found = 1;
				break;
			}
		}
		if (!found)
		{
			fprintf (stderr, ANSI_COLOR_RED "Unable to find node %"PRIu32" in dirdbFreeChildren list\n", iter);
			retval++;
		}
	}

	if (!retval)
	{
		fprintf (stderr, ANSI_COLOR_GREEN "All good\n");
	}

	fprintf (stderr, ANSI_COLOR_RESET "\n");

	clear_dirdb();

	return retval;
}

const char TestDatabaseHealthy[60+4 + 2*16 + 13*12 + 5+4+3+6*10] =
/* 60 */ "Cubic Player Directory Data Base\x1B\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00" /* dirdbsigv2 */
/*  4 */ "\x10\x00\x00\x00" /* 16 entries */
/*  2 */ "\x00\x00" /* entry 0, not in use */
/*  2 */ "\x00\x00" /* entry 1, not in use */
/*  2 */ "\x05\x00" /* entry 2, file: */
/*  4 */  "\xff\xff\xff\xff" /* parent: NO_PARENT */
/*  4 */  "\xff\xff\xff\xff" /* mdb:    NO_REFERENCE */
/*  4 */  "\xff\xff\xff\xff" /* adb:    defunctional NO_REFERENCE */
/*  5 */  "file:"
/*  2 */"\x04\x00" /* entry 3, file:/home */
/*  4 */  "\x02\x00\x00\x00" /* parent: 2, file: */
/*  4 */  "\xff\xff\xff\xff" /* mdb:    NO_REFERENCE */
/*  4 */  "\xff\xff\xff\xff" /* adb:    defunctional NO_REFERENCE */
/*  4 */  "home"
/*  2 */"\x03\x00" /* entry 4, file:/home/foo */
/*  4 */  "\x03\x00\x00\x00" /* parent: 3, file:/home */
/*  4 */  "\xff\xff\xff\xff" /* mdb:    NO_REFERENCE */
/*  4 */  "\xff\xff\xff\xff" /* adb:    defunctional NO_REFERENCE */
/*  3 */  "foo"
/*  2 */"\x06\x00" /* entry 5, file:/home/foo/01.mod */
/*  4 */  "\x04\x00\x00\x00" /* parent: 4, file:/home/foo/01.mod */
/*  4 */  "\x00\x01\x02\x03" /* mdb:    0x03020100 */
/*  4 */  "\xff\xff\xff\xff" /* adb:    defunctional NO_REFERENCE */
/*  6 */  "01.mod"
/*  2 */"\x06\x00" /* entry 6, file:/home/foo/02.mod */
/*  4 */  "\x04\x00\x00\x00" /* parent: 4, file:/home/foo/02.mod */
/*  4 */  "\x04\x05\x06\x07" /* mdb:    0x07060504 */
/*  4 */  "\xff\xff\xff\xff" /* adb:    defunctional NO_REFERENCE */
/*  6 */  "02.mod"
/*  2 */"\x06\x00" /* entry 7, file:/home/foo/03.mod */
/*  4 */  "\x04\x00\x00\x00" /* parent: 4, file:/home/foo/02.mod */
/*  4 */  "\x08\x09\x0a\x0b" /* mdb:    0x0b0a0908 */
/*  4 */  "\xff\xff\xff\xff" /* adb:    defunctional NO_REFERENCE */
/*  6 */  "03.mod"
/*  2 */"\x06\x00" /* entry 8, file:/home/foo/04.mod */
/*  4 */  "\x04\x00\x00\x00" /* parent: 4, file:/home/foo/02.mod */
/*  4 */  "\x0c\x0d\x0e\x0f" /* mdb:    0x0f0e0d0c */
/*  4 */  "\xff\xff\xff\xff" /* adb:    defunctional NO_REFERENCE */
/*  6 */  "04.mod"
/*  2 */"\x06\x00" /* entry 9, file:/home/foo/05.mod */
/*  4 */  "\x04\x00\x00\x00" /* parent: 4, file:/home/foo/02.mod */
/*  4 */  "\x10\x11\x12\x13" /* mdb:    0x13121110 */
/*  4 */  "\xff\xff\xff\xff" /* adb:    defunctional NO_REFERENCE */
/*  6 */  "05.mod"
/*  2 */"\x06\x00" /* entry 10, file:/home/foo/06.mod */
/*  4 */  "\x04\x00\x00\x00" /* parent: 4, file:/home/foo/02.mod */
/*  4 */  "\x14\x15\x16\x17" /* mdb:    0x17161514 */
/*  4 */  "\xff\xff\xff\xff" /* adb:    defunctional NO_REFERENCE */
/*  6 */  "06.mod"
/*  2 */"\x06\x00" /* entry 11, file:/home/foo/07.mod */
/*  4 */  "\x04\x00\x00\x00" /* parent: 4, file:/home/foo/02.mod */
/*  4 */  "\x18\x19\x1a\x1b" /* mdb:    0x1b1a1918 */
/*  4 */  "\xff\xff\xff\xff" /* adb:    defunctional NO_REFERENCE */
/*  6 */  "07.mod"
/*  2 */"\x06\x00" /* entry 12, file:/home/foo/08.mod */
/*  4 */  "\x04\x00\x00\x00" /* parent: 4, file:/home/foo/02.mod */
/*  4 */  "\x1c\x1d\x1e\x1f" /* mdb:    0x1f1e1d1c */
/*  4 */  "\xff\xff\xff\xff" /* adb:    defunctional NO_REFERENCE */
/*  6 */  "08.mod"
/*  2 */"\x06\x00" /* entry 13, file:/home/foo/09.mod */
/*  4 */  "\x04\x00\x00\x00" /* parent: 4, file:/home/foo/02.mod */
/*  4 */  "\x20\x21\x22\x23" /* mdb:    0x23222120 */
/*  4 */  "\xff\xff\xff\xff" /* adb:    defunctional NO_REFERENCE */
/*  6 */  "09.mod"
/*  2 */"\x06\x00" /* entry 14, file:/home/foo/10.mod */
/*  4 */  "\x04\x00\x00\x00" /* parent: 4, file:/home/foo/02.mod */
/*  4 */  "\x24\x25\x26\x27" /* mdb:    0x27262524 */
/*  4 */  "\xff\xff\xff\xff" /* adb:    defunctional NO_REFERENCE */
/*  6 */  "10.mod"
/*  2 */"\x00\x00" /* entry 15, not in use */
;

const char TestDatabasePartial[60+4 + 2*16 + 14*12 +5+4+3+6*9+4+3] =
/* 60 */ "Cubic Player Directory Data Base\x1B\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00" /* dirdbsigv2 */
/*  4 */ "\x20\x00\x00\x00" /* 32, last 16 entries missing*/
/*  2 */ "\x00\x00" /* entry 0, not in use */
/*  2 */ "\x00\x00" /* entry 1, not in use */
/*  2 */ "\x05\x00" /* entry 2, file: */
/*  4 */  "\xff\xff\xff\xff" /* parent: NO_PARENT */
/*  4 */  "\xff\xff\xff\xff" /* mdb:    NO_REFERENCE */
/*  4 */  "\xff\xff\xff\xff" /* adb:    defunctional NO_REFERENCE */
/*  5 */  "file:"
/*  2 */"\x04\x00" /* entry 3, file:/home */
/*  4 */  "\x02\x00\x00\x00" /* parent: 2, file: */
/*  4 */  "\xff\xff\xff\xff" /* mdb:    NO_REFERENCE */
/*  4 */  "\xff\xff\xff\xff" /* adb:    defunctional NO_REFERENCE */
/*  4 */  "home"
/*  2 */"\x03\x00" /* entry 4, file:/home/foo */
/*  4 */  "\x03\x00\x00\x00" /* parent: 3, file:/home */
/*  4 */  "\xff\xff\xff\xff" /* mdb:    NO_REFERENCE */
/*  4 */  "\xff\xff\xff\xff" /* adb:    defunctional NO_REFERENCE */
/*  3 */  "foo"
/*  2 */"\x06\x00" /* entry 5, file:/home/foo/01.mod */
/*  4 */  "\x04\x00\x00\x00" /* parent: 4, file:/home/foo/01.mod */
/*  4 */  "\x00\x01\x02\x03" /* mdb:    0x03020100 */
/*  4 */  "\xff\xff\xff\xff" /* adb:    defunctional NO_REFERENCE */
/*  6 */  "01.mod"
/*  2 */"\x06\x00" /* entry 6, file:/home/foo/02.mod */
/*  4 */  "\x04\x00\x00\x00" /* parent: 4, file:/home/foo/02.mod */
/*  4 */  "\x04\x05\x06\x07" /* mdb:    0x07060504 */
/*  4 */  "\xff\xff\xff\xff" /* adb:    defunctional NO_REFERENCE */
/*  6 */  "02.mod"
/*  2 */"\x06\x00" /* entry 7, file:/home/foo/03.mod */
/*  4 */  "\x04\x00\x00\x00" /* parent: 4, file:/home/foo/02.mod */
/*  4 */  "\x08\x09\x0a\x0b" /* mdb:    0x0b0a0908 */
/*  4 */  "\xff\xff\xff\xff" /* adb:    defunctional NO_REFERENCE */
/*  6 */  "03.mod"
/*  2 */"\x06\x00" /* entry 8, file:/home/foo/04.mod */
/*  4 */  "\x04\x00\x00\x00" /* parent: 4, file:/home/foo/02.mod */
/*  4 */  "\x0c\x0d\x0e\x0f" /* mdb:    0x0f0e0d0c */
/*  4 */  "\xff\xff\xff\xff" /* adb:    defunctional NO_REFERENCE */
/*  6 */  "04.mod"
/*  2 */"\x06\x00" /* entry 9, file:/home/foo/05.mod */
/*  4 */  "\x04\x00\x00\x00" /* parent: 4, file:/home/foo/02.mod */
/*  4 */  "\x10\x11\x12\x13" /* mdb:    0x13121110 */
/*  4 */  "\xff\xff\xff\xff" /* adb:    defunctional NO_REFERENCE */
/*  6 */  "05.mod"
/*  2 */"\x06\x00" /* entry 10, file:/home/foo/06.mod */
/*  4 */  "\x04\x00\x00\x00" /* parent: 4, file:/home/foo/02.mod */
/*  4 */  "\x14\x15\x16\x17" /* mdb:    0x17161514 */
/*  4 */  "\xff\xff\xff\xff" /* adb:    defunctional NO_REFERENCE */
/*  6 */  "06.mod"
/*  2 */"\x06\x00" /* entry 11, file:/home/foo/07.mod */
/*  4 */  "\x04\x00\x00\x00" /* parent: 4, file:/home/foo/02.mod */
/*  4 */  "\x18\x19\x1a\x1b" /* mdb:    0x1b1a1918 */
/*  4 */  "\xff\xff\xff\xff" /* adb:    defunctional NO_REFERENCE */
/*  6 */  "07.mod"
/*  2 */"\x04\x00" /* entry 12, ?/test */
/*  4 */  "\x10\x00\x00\x00" /* parent: 16, missing */
/*  4 */  "\xff\xff\xff\xff" /* mdb:    NO_REFERENCE */
/*  4 */  "\xff\xff\xff\xff" /* adb:    defunctional NO_REFERENCE */
/*  6 */  "test"
/*  2 */"\x03\x00" /* entry 13, ?/test/moo */
/*  4 */  "\x0c\x00\x00\x00" /* parent: 12, file:/home/foo/02.mod */
/*  4 */  "\xff\xff\xff\xff" /* mdb:    NO_REFERENCE */
/*  4 */  "\xff\xff\xff\xff" /* adb:    defunctional NO_REFERENCE */
/*  6 */  "moo"
/*  2 */"\x06\x00" /* entry 14, ?/test/moo/10.mod */
/*  4 */  "\x0d\x00\x00\x00" /* parent: 13 */
/*  4 */  "\x20\x21\x22\x23" /* mdb:    0x23222120 */
/*  4 */  "\xff\xff\xff\xff" /* adb:    defunctional NO_REFERENCE */
/*  6 */  "10.mod"
/*  2 */"\x06\x00" /* entry 15, ?/11.mod */
/*  4 */  "\x10\x00\x00\x00" /* parent: 16 missing */
/*  4 */  "\x24\x25\x26\x27" /* mdb:    0x27262524 */
/*  4 */  "\xff\xff\xff\xff" /* adb:    defunctional NO_REFERENCE */
/*  6 */  "11.mod"
;

void InitDummyFileIO (const char * const basedata, const int datalen)
{
	dirdbFile = osfile_open_readwrite (CFDATAHOMEDIR_OVERRIDE "CPDIRDB.DAT", 0, 0);
	if (!dirdbFile)
	{
		fprintf(stderr, "InitDummyFileIO() failed, expect failures\n");
		return;
	}
	osfile_write (dirdbFile, basedata, datalen);
	osfile_close (dirdbFile);
	dirdbFile = 0;
}

static int dirdb_basic_test13(void)
{
	int retval = 0;

	fprintf (stderr, ANSI_COLOR_CYAN "dirdbInit() Parse non-existing file\n" ANSI_COLOR_RESET);
	if (!dirdbInit (0))
	{
		fprintf (stderr, ANSI_COLOR_RED "Failed to run\n");
		retval++;
	} else {
		if (dirdbFreeChildren_size < FREE_MINSIZE) { fprintf (stderr, ANSI_COLOR_RED "dirdbFreeChildren_size < FREE_MINSIZE\n"); retval++; }
	}
	clear_dirdb ();

	fprintf (stderr, ANSI_COLOR_CYAN "dirdbInit() Parse empty file\n" ANSI_COLOR_RESET);
	InitDummyFileIO ("", 0);
	if (!dirdbInit (0))
	{
		fprintf (stderr, ANSI_COLOR_RED "Failed to run\n");
		retval++;
	} else {
		if (dirdbFreeChildren_size < FREE_MINSIZE) { fprintf (stderr, ANSI_COLOR_RED "dirdbFreeChildren_size < FREE_MINSIZE\n"); retval++; }
	}
	clear_dirdb ();

	fprintf (stderr, ANSI_COLOR_CYAN "dirdbInit() Parse healty binary file\n" ANSI_COLOR_RESET);
	InitDummyFileIO (TestDatabaseHealthy, sizeof (TestDatabaseHealthy));
	if (!dirdbInit (0))
	{
		fprintf (stderr, ANSI_COLOR_RED "Failed to run\n");
		retval++;
	} else {
		if (dirdbFreeChildren_size < FREE_MINSIZE) { fprintf (stderr, ANSI_COLOR_RED "dirdbFreeChildren_size < FREE_MINSIZE\n"); retval++; }
		if (dirdbNum < 16) { fprintf (stderr, ANSI_COLOR_RED "dirdbNum (%"PRIu32") < 16\n", dirdbNum); retval++; }

		if ((dirdbNum <  3) || strcmp (dirdbData[ 2].name, "file:")) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[2].name != \"file:\"\n"); retval++; }

		if ((dirdbNum <  4) || strcmp (dirdbData[ 3].name, "home")) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[3].name != \"home\"\n"); retval++; }
		if ((dirdbNum <  4) || (dirdbData[ 3].parent != 2)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[3].parent != 2\n"); retval++; }

		if ((dirdbNum <  5) || strcmp (dirdbData[ 4].name, "foo")) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[4].name != \"foo\"\n"); retval++; }
		if ((dirdbNum <  5) || (dirdbData[ 4].parent != 3)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[4].parent != 3\n"); retval++; }

		if ((dirdbNum <  6) || strcmp (dirdbData[ 5].name, "01.mod")) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[5].name != \"01.mod\"\n"); retval++; }
		if ((dirdbNum <  6) || (dirdbData[ 5].parent != 4)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[5].parent != 4\n"); retval++; }
		if ((dirdbNum <  6) || (dirdbData[ 5].mdb_ref != 0x03020100)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[5].mdb_ref 0x%08"PRIx32" != 0x03020100\n", dirdbData[5].mdb_ref); retval++; }

		if ((dirdbNum <  7) || strcmp (dirdbData[ 6].name, "02.mod")) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[6].name != \"02.mod\"\n"); retval++; }
		if ((dirdbNum <  7) || (dirdbData[ 6].parent != 4)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[6].parent != 4\n"); retval++; }
		if ((dirdbNum <  7) || (dirdbData[ 6].mdb_ref != 0x07060504)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[6].mdb_ref 0x%08"PRIx32" != 0x07060504\n", dirdbData[6].mdb_ref); retval++; }

		if ((dirdbNum <  8) || strcmp (dirdbData[ 7].name, "03.mod")) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[7].name != \"03.mod\"\n"); retval++; }
		if ((dirdbNum <  8) || (dirdbData[ 7].parent != 4)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[7].parent != 4\n"); retval++; }
		if ((dirdbNum <  8) || (dirdbData[ 7].mdb_ref != 0x0b0a0908)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[7].mdb_ref 0x%08"PRIx32" != 0x0b0a0906\n", dirdbData[7].mdb_ref); retval++; }

		if ((dirdbNum <  9) || strcmp (dirdbData[ 8].name, "04.mod")) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[8].name != \"04.mod\"\n"); retval++; }
		if ((dirdbNum <  9) || (dirdbData[ 8].parent != 4)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[8].parent != 4\n"); retval++; }
		if ((dirdbNum <  9) || (dirdbData[ 8].mdb_ref != 0x0f0e0d0c)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[8].mdb_ref 0x%08"PRIx32" != 0x0f0e0d0c\n", dirdbData[8].mdb_ref); retval++; }

		if ((dirdbNum < 10) || strcmp (dirdbData[ 9].name, "05.mod")) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[9].name != \"05.mod\"\n"); retval++; }
		if ((dirdbNum < 10) || (dirdbData[ 9].parent != 4)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[9].parent != 4\n"); retval++; }
		if ((dirdbNum < 10) || (dirdbData[ 9].mdb_ref != 0x13121110)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[9].mdb_ref 0x%08"PRIx32" != 0x13121110\n", dirdbData[9].mdb_ref); retval++; }

		if ((dirdbNum < 11) || strcmp (dirdbData[10].name, "06.mod")) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[10].name != \"06.mod\"\n"); retval++; }
		if ((dirdbNum < 11) || (dirdbData[10].parent != 4)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[10].parent != 4\n"); retval++; }
		if ((dirdbNum < 11) || (dirdbData[10].mdb_ref != 0x17161514)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[10].mdb_ref 0x%08"PRIx32" != 0x17161514\n", dirdbData[10].mdb_ref); retval++; }

		if ((dirdbNum < 12) || strcmp (dirdbData[11].name, "07.mod")) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[11].name != \"07.mod\"\n"); retval++; }
		if ((dirdbNum < 12) || (dirdbData[11].parent != 4)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[11].parent != 4\n"); retval++; }
		if ((dirdbNum < 12) || (dirdbData[11].mdb_ref != 0x1b1a1918)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[11].mdb_ref 0x%08"PRIx32" != 0x1b1a1918\n", dirdbData[11].mdb_ref); retval++; }

		if ((dirdbNum < 13) || strcmp (dirdbData[12].name, "08.mod")) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[12].name != \"08.mod\"\n"); retval++; }
		if ((dirdbNum < 13) || (dirdbData[12].parent != 4)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[12].parent != 4\n"); retval++; }
		if ((dirdbNum < 13) || (dirdbData[12].mdb_ref != 0x1f1e1d1c)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[12].mdb_ref 0x%08"PRIx32" != 0x1f1e1d1c\n", dirdbData[12].mdb_ref); retval++; }

		if ((dirdbNum < 14) || strcmp (dirdbData[13].name, "09.mod")) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[13].name != \"09.mod\"\n"); retval++; }
		if ((dirdbNum < 14) || (dirdbData[13].parent != 4)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[13].parent != 4\n"); retval++; }
		if ((dirdbNum < 14) || (dirdbData[13].mdb_ref != 0x23222120)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[13].mdb_ref 0x%08"PRIx32" != 0x23222120\n", dirdbData[13].mdb_ref); retval++; }

		if ((dirdbNum < 15) || strcmp (dirdbData[14].name, "10.mod")) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[14].name != \"10.mod\"\n"); retval++; }
		if ((dirdbNum < 15) || (dirdbData[14].parent != 4)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[14].parent != 4\n"); retval++; }
		if ((dirdbNum < 15) || (dirdbData[14].mdb_ref != 0x27262524)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[14].mdb_ref 0x%08"PRIx32" != 0x27262524\n", dirdbData[14].mdb_ref); retval++; }

		if ((dirdbNum <  5) || (dirdbData[4].children_fill <  1) || (dirdbData[4].children[0] !=  5)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[4].children[0] %"PRIu32" != 5\n", dirdbData[4].children[0]); retval++; }
		if ((dirdbNum <  5) || (dirdbData[4].children_fill <  2) || (dirdbData[4].children[1] !=  6)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[4].children[1] %"PRIu32" != 6\n", dirdbData[4].children[1]); retval++; }
		if ((dirdbNum <  5) || (dirdbData[4].children_fill <  3) || (dirdbData[4].children[2] !=  7)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[4].children[2] %"PRIu32" != 7\n", dirdbData[4].children[2]); retval++; }
		if ((dirdbNum <  5) || (dirdbData[4].children_fill <  4) || (dirdbData[4].children[3] !=  8)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[4].children[3] %"PRIu32" != 8\n", dirdbData[4].children[3]); retval++; }
		if ((dirdbNum <  5) || (dirdbData[4].children_fill <  5) || (dirdbData[4].children[4] !=  9)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[4].children[4] %"PRIu32" != 9\n", dirdbData[4].children[4]); retval++; }
		if ((dirdbNum <  5) || (dirdbData[4].children_fill <  6) || (dirdbData[4].children[5] != 10)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[4].children[5] %"PRIu32" != 10\n", dirdbData[4].children[5]); retval++; }
		if ((dirdbNum <  5) || (dirdbData[4].children_fill <  7) || (dirdbData[4].children[6] != 11)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[4].children[6] %"PRIu32" != 11\n", dirdbData[4].children[6]); retval++; }
		if ((dirdbNum <  5) || (dirdbData[4].children_fill <  8) || (dirdbData[4].children[7] != 12)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[4].children[7] %"PRIu32" != 12\n", dirdbData[4].children[7]); retval++; }
		if ((dirdbNum <  5) || (dirdbData[4].children_fill <  9) || (dirdbData[4].children[8] != 13)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[4].children[8] %"PRIu32" != 13\n", dirdbData[4].children[8]); retval++; }
		if ((dirdbNum <  5) || (dirdbData[4].children_fill < 10) || (dirdbData[4].children[9] != 14)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[4].children[9] %"PRIu32" != 14\n", dirdbData[4].children[9]); retval++; }
	}
	clear_dirdb ();

	fprintf (stderr, ANSI_COLOR_CYAN "dirdbInit() Parse partial file\n" ANSI_COLOR_RESET);
	InitDummyFileIO (TestDatabasePartial, sizeof (TestDatabasePartial));
	if (!dirdbInit (0))
	{
		fprintf (stderr, ANSI_COLOR_RED "Failed to run\n");
		retval++;
	} else {
		int i;
		if (dirdbFreeChildren_size < FREE_MINSIZE) { fprintf (stderr, ANSI_COLOR_RED "dirdbFreeChildren_size < FREE_MINSIZE\n"); retval++; }
		if (dirdbNum < 32) { fprintf (stderr, ANSI_COLOR_RED "dirdbNum (%"PRIu32") < 32\n", dirdbNum); retval++; }

		if ((dirdbNum <  3) || strcmp (dirdbData[ 2].name, "file:")) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[2].name != \"file:\"\n"); retval++; }

		if ((dirdbNum <  4) || strcmp (dirdbData[ 3].name, "home")) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[3].name != \"home\"\n"); retval++; }
		if ((dirdbNum <  4) || (dirdbData[ 3].parent != 2)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[3].parent != 2\n"); retval++; }

		if ((dirdbNum <  5) || strcmp (dirdbData[ 4].name, "foo")) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[4].name != \"foo\"\n"); retval++; }
		if ((dirdbNum <  5) || (dirdbData[ 4].parent != 3)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[4].parent != 3\n"); retval++; }

		if ((dirdbNum <  6) || strcmp (dirdbData[ 5].name, "01.mod")) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[5].name != \"01.mod\"\n"); retval++; }
		if ((dirdbNum <  6) || (dirdbData[ 5].parent != 4)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[5].parent != 4\n"); retval++; }
		if ((dirdbNum <  6) || (dirdbData[ 5].mdb_ref != 0x03020100)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[5].mdb_ref 0x%08"PRIx32" != 0x03020100\n", dirdbData[5].mdb_ref); retval++; }

		if ((dirdbNum <  7) || strcmp (dirdbData[ 6].name, "02.mod")) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[6].name != \"02.mod\"\n"); retval++; }
		if ((dirdbNum <  7) || (dirdbData[ 6].parent != 4)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[6].parent != 4\n"); retval++; }
		if ((dirdbNum <  7) || (dirdbData[ 6].mdb_ref != 0x07060504)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[6].mdb_ref 0x%08"PRIx32" != 0x07060504\n", dirdbData[6].mdb_ref); retval++; }

		if ((dirdbNum <  8) || strcmp (dirdbData[ 7].name, "03.mod")) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[7].name != \"03.mod\"\n"); retval++; }
		if ((dirdbNum <  8) || (dirdbData[ 7].parent != 4)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[7].parent != 4\n"); retval++; }
		if ((dirdbNum <  8) || (dirdbData[ 7].mdb_ref != 0x0b0a0908)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[7].mdb_ref 0x%08"PRIx32" != 0x0b0a0908\n", dirdbData[7].mdb_ref); retval++; }

		if ((dirdbNum <  9) || strcmp (dirdbData[ 8].name, "04.mod")) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[8].name != \"04.mod\"\n"); retval++; }
		if ((dirdbNum <  9) || (dirdbData[ 8].parent != 4)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[8].parent != 4\n"); retval++; }
		if ((dirdbNum <  9) || (dirdbData[ 8].mdb_ref != 0x0f0e0d0c)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[8].mdb_ref 0x%08"PRIx32" != 0x0f0e0d0c\n", dirdbData[8].mdb_ref); retval++; }

		if ((dirdbNum < 10) || strcmp (dirdbData[ 9].name, "05.mod")) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[9].name != \"05.mod\"\n"); retval++; }
		if ((dirdbNum < 10) || (dirdbData[ 9].parent != 4)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[9].parent != 4\n"); retval++; }
		if ((dirdbNum < 10) || (dirdbData[ 9].mdb_ref != 0x13121110)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[9].mdb_ref 0x%08"PRIx32" != 0x13121110\n", dirdbData[9].mdb_ref); retval++; }

		if ((dirdbNum < 11) || strcmp (dirdbData[10].name, "06.mod")) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[10].name != \"06.mod\"\n"); retval++; }
		if ((dirdbNum < 11) || (dirdbData[10].parent != 4)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[10].parent != 4\n"); retval++; }
		if ((dirdbNum < 11) || (dirdbData[10].mdb_ref != 0x17161514)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[10].mdb_ref 0x%08"PRIx32" != 0x17161514\n", dirdbData[10].mdb_ref); retval++; }

		if ((dirdbNum < 12) || strcmp (dirdbData[11].name, "07.mod")) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[11].name != \"07.mod\"\n"); retval++; }
		if ((dirdbNum < 12) || (dirdbData[11].parent != 4)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[11].parent != 4\n"); retval++; }
		if ((dirdbNum < 12) || (dirdbData[11].mdb_ref != 0x1b1a1918)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[11].mdb_ref 0x%08"PRIx32" != 0x1b1a1918\n", dirdbData[11].mdb_ref); retval++; }

		if ((dirdbNum <  5) || (dirdbData[4].children_fill <  1) || (dirdbData[4].children[0] !=  5)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[4].children[0] %"PRIu32" != 5\n", dirdbData[4].children[0]); retval++; }
		if ((dirdbNum <  5) || (dirdbData[4].children_fill <  2) || (dirdbData[4].children[1] !=  6)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[4].children[1] %"PRIu32" != 6\n", dirdbData[4].children[1]); retval++; }
		if ((dirdbNum <  5) || (dirdbData[4].children_fill <  3) || (dirdbData[4].children[2] !=  7)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[4].children[2] %"PRIu32" != 7\n", dirdbData[4].children[2]); retval++; }
		if ((dirdbNum <  5) || (dirdbData[4].children_fill <  4) || (dirdbData[4].children[3] !=  8)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[4].children[3] %"PRIu32" != 8\n", dirdbData[4].children[3]); retval++; }
		if ((dirdbNum <  5) || (dirdbData[4].children_fill <  5) || (dirdbData[4].children[4] !=  9)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[4].children[4] %"PRIu32" != 9\n", dirdbData[4].children[4]); retval++; }
		if ((dirdbNum <  5) || (dirdbData[4].children_fill <  6) || (dirdbData[4].children[5] != 10)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[4].children[5] %"PRIu32" != 10\n", dirdbData[4].children[5]); retval++; }
		if ((dirdbNum <  5) || (dirdbData[4].children_fill <  7) || (dirdbData[4].children[6] != 11)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[4].children[6] %"PRIu32" != 11\n", dirdbData[4].children[6]); retval++; }

		for (i = 12; i < dirdbNum; i++)
		{
			if (dirdbData[i].name)
			{
				fprintf(stderr, "dirdbData[%d] should have been marked as not-in-use\n", i);
				retval++;
			}
		}
	}
	clear_dirdb ();

	fprintf (stderr, ANSI_COLOR_RESET "\n");

	return retval;
}

static int dirdb_basic_test14(void)
{
	int retval = 0;

	fprintf (stderr, ANSI_COLOR_CYAN "dirdbFlush()\n" ANSI_COLOR_RESET);
	if (!dirdbInit (0))
	{
		fprintf (stderr, ANSI_COLOR_RED "dirdbInit() failed, expect errors\n");
	}

	if (dirdbNum < 16)
	{
		dirdbData = realloc (dirdbData, sizeof (dirdbData[0]) * 16);
		memset (dirdbData + dirdbNum, 0, sizeof (dirdbData[0]) * (16 - dirdbNum));
		dirdbNum = 16;
	}

	dirdbData[0].parent = DIRDB_NOPARENT;
	dirdbData[0].name = strdup ("file:");
	dirdbData[0].mdb_ref = DIRDB_NO_MDBREF;
	dirdbData[0].children_size = 16;
	dirdbData[0].children = calloc (sizeof (dirdbData[0].children[0]), dirdbData[0].children_size);
	dirdbData[0].children_fill = 1;
	dirdbData[0].children[0] = 1;
	dirdbData[0].refcount = 1;
#ifdef DIRDB_DEBUG
	dirdbData[0].refcount_children = 1;
#endif

	dirdbData[1].parent = 0;
	dirdbData[1].name = strdup ("tmp");
	dirdbData[1].mdb_ref = DIRDB_NO_MDBREF;
	dirdbData[1].children_size = 16;
	dirdbData[1].children = calloc (sizeof (dirdbData[1].children[0]), dirdbData[1].children_size);
	dirdbData[1].children_fill = 4;
	dirdbData[1].children[0] = 2;
	dirdbData[1].children[1] = 3;
	dirdbData[1].children[2] = 4;
	dirdbData[1].children[3] = 5;
	dirdbData[1].refcount = 4;
#ifdef DIRDB_DEBUG
	dirdbData[1].refcount_children = 4;
#endif

	dirdbData[2].parent = 1;
	dirdbData[2].name = strdup ("01.mod");
	dirdbData[2].mdb_ref = 0x00000001;
	dirdbData[2].refcount = 1;
#ifdef DIRDB_DEBUG
	dirdbData[2].refcount_mdb_medialib = 1;
#endif

	dirdbData[3].parent = 1;
	dirdbData[3].name = strdup ("02.mod");
	dirdbData[3].mdb_ref = 0x00000020;
	dirdbData[3].refcount = 1;
#ifdef DIRDB_DEBUG
	dirdbData[3].refcount_mdb_medialib = 1;
#endif

	dirdbData[4].parent = 1;
	dirdbData[4].name = strdup ("03.mod");
	dirdbData[4].mdb_ref = 0x00000300;
	dirdbData[4].refcount = 1;
#ifdef DIRDB_DEBUG
	dirdbData[4].refcount_mdb_medialib = 1;
#endif

	dirdbData[5].parent = 1;
	dirdbData[5].name = strdup ("04.mod");
	dirdbData[5].mdb_ref = 0x00004000;
	dirdbData[5].refcount = 1;
#ifdef DIRDB_DEBUG
	dirdbData[5].refcount_mdb_medialib = 1;
#endif

	dirdbDirty = 1;

	dirdbFlush();
	dirdbClose();
	if (!dirdbInit (0))
	{
		fprintf (stderr, ANSI_COLOR_RED "dirdbInit() failed to parse dirdbClose binary block\n");
		retval++;
	}

	if ((dirdbNum < 1) || (!dirdbData[0].name) || (strcmp (dirdbData[0].name, "file:" )))
	{
		fprintf (stderr, ANSI_COLOR_RED "dirdbData[0].name %s%s%s != \"file:\"\n",
			((dirdbNum < 1) || (!dirdbData[0].name)) ? "" : "\"",
			 (dirdbNum < 1) ? "void" : dirdbData[0].name ? dirdbData[0].name : "NULL",
			((dirdbNum < 1) || (!dirdbData[0].name)) ? "" : "\"");
		retval++;
	}
	if ((dirdbNum < 2) || (!dirdbData[1].name) || (strcmp (dirdbData[1].name, "tmp"   )))
	{
		fprintf (stderr, ANSI_COLOR_RED "dirdbData[1].name %s%s%s != \"tmp\"\n",
			((dirdbNum < 2) || (!dirdbData[1].name)) ? "" : "\"",
			 (dirdbNum < 2) ? "void" : dirdbData[1].name ? dirdbData[1].name : "NULL",
			((dirdbNum < 2) || (!dirdbData[1].name)) ? "" : "\"");
		retval++;
	}
	if ((dirdbNum < 3) || (!dirdbData[2].name) || (strcmp (dirdbData[2].name, "01.mod")))
	{
		fprintf (stderr, ANSI_COLOR_RED "dirdbData[2].name %s%s%s != \"01.mod\"\n",
			((dirdbNum < 3) || (!dirdbData[2].name)) ? "" : "\"",
			 (dirdbNum < 3) ? "void" : dirdbData[2].name ? dirdbData[2].name : "NULL",
			((dirdbNum < 3) || (!dirdbData[2].name)) ? "" : "\"");
		retval++;
	}
	if ((dirdbNum < 4) || (!dirdbData[3].name) || (strcmp (dirdbData[3].name, "02.mod")))
	{
		fprintf (stderr, ANSI_COLOR_RED "dirdbData[3].name %s%s%s != \"02.mod\"\n",
			((dirdbNum < 4) || (!dirdbData[3].name)) ? "" : "\"",
			 (dirdbNum < 4) ? "void" : dirdbData[3].name ? dirdbData[3].name : "NULL",
			((dirdbNum < 4) || (!dirdbData[3].name)) ? "" : "\"");
		retval++;
	}
	if ((dirdbNum < 5) || (!dirdbData[4].name) || (strcmp (dirdbData[4].name, "03.mod")))
	{
		fprintf (stderr, ANSI_COLOR_RED "dirdbData[4].name %s%s%s != \"03.mod\"\n",
			((dirdbNum < 5) || (!dirdbData[4].name)) ? "" : "\"",
			 (dirdbNum < 5) ? "void" : dirdbData[4].name ? dirdbData[4].name : "NULL",
			((dirdbNum < 5) || (!dirdbData[4].name)) ? "" : "\""); 
		retval++;
	}
	if ((dirdbNum < 6) || (!dirdbData[5].name) || (strcmp (dirdbData[5].name, "04.mod")))
	{
		fprintf (stderr, ANSI_COLOR_RED "dirdbData[5].name %s%s%s != \"04.mod\"\n",
			((dirdbNum < 6) || (!dirdbData[5].name)) ? "" : "\"",
			 (dirdbNum < 6) ? "void" : dirdbData[5].name ? dirdbData[5].name : "NULL",
			((dirdbNum < 6) || (!dirdbData[5].name)) ? "" : "\"");
		retval++;
	}

	if ((dirdbNum < 1) || (dirdbData[0].parent != DIRDB_NOPARENT)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[0].parent != DIRDB_NOPARENT\n"); retval++; }
	if ((dirdbNum < 2) || (dirdbData[1].parent != 0             )) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[0].parent != 0\n");              retval++; }
	if ((dirdbNum < 3) || (dirdbData[2].parent != 1             )) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[0].parent != 1\n");              retval++; }
	if ((dirdbNum < 4) || (dirdbData[3].parent != 1             )) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[0].parent != 1\n");              retval++; }
	if ((dirdbNum < 5) || (dirdbData[4].parent != 1             )) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[0].parent != 1\n");              retval++; }
	if ((dirdbNum < 6) || (dirdbData[5].parent != 1             )) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[0].parent != 1\n");              retval++; }

	if ((dirdbNum < 1) || (dirdbData[0].mdb_ref != DIRDB_NO_MDBREF)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[0].mdb_ref != DIRDB_NO_MDBREF\n"); retval++; }
	if ((dirdbNum < 2) || (dirdbData[1].mdb_ref != DIRDB_NO_MDBREF)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[0].mdb_ref != DIRDB_NO_MDBREF\n"); retval++; }
	if ((dirdbNum < 3) || (dirdbData[2].mdb_ref != 0x00000001     )) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[0].mdb_ref != 0x00000001\n");      retval++; }
	if ((dirdbNum < 4) || (dirdbData[3].mdb_ref != 0x00000020     )) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[0].mdb_ref != 0x00000020\n");      retval++; }
	if ((dirdbNum < 5) || (dirdbData[4].mdb_ref != 0x00000300     )) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[0].mdb_ref != 0x00000300\n");      retval++; }
	if ((dirdbNum < 6) || (dirdbData[5].mdb_ref != 0x00004000     )) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[0].mdb_ref != 0x00004000\n");      retval++; }

	if ((dirdbNum < 1) || (dirdbData[0].children_fill < 1) || (dirdbData[0].children[0] != 1)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[0].children[0] != 1\n"); retval++; }
	if ((dirdbNum < 2) || (dirdbData[1].children_fill < 1) || (dirdbData[1].children[0] != 2)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[1].children[0] != 2\n"); retval++; }
	if ((dirdbNum < 2) || (dirdbData[1].children_fill < 2) || (dirdbData[1].children[1] != 3)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[1].children[1] != 3\n"); retval++; }
	if ((dirdbNum < 2) || (dirdbData[1].children_fill < 3) || (dirdbData[1].children[2] != 4)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[1].children[2] != 4\n"); retval++; }
	if ((dirdbNum < 2) || (dirdbData[1].children_fill < 4) || (dirdbData[1].children[3] != 5)) { fprintf (stderr, ANSI_COLOR_RED "dirdbData[1].children[3] != 5\n"); retval++; }

	clear_dirdb ();

	return retval;
}

int main(int argc, char *argv[])
{
	int retval = 0;

	clear_dirdb ();

	retval |= dirdb_basic_test1(); /* dirdbResolvePathAndRef() */

	retval |= dirdb_basic_test2(); /* dirdbFindAndRef() + dirdbUnref() */

	retval |= dirdb_basic_test3(); /* dirdbResolvePathWithBaseAndRef() */

	retval |= dirdb_basic_test4(); /* dirdbFindAndRef() */

	retval |= dirdb_basic_test5(); /* dirdbGetName_malloc() */

	retval |= dirdb_basic_test6(); /* dirdbGetFullname_malloc() */

	retval |= dirdb_basic_test8(); /* dirdbDiffPath() */

	retval |= dirdb_basic_test7(); /* dirdbTagSetParent(), dirdbMakeMdbRef(), dirdbTagRemoveUntaggedAndSubmit(), dirdbGetMdb() */

	retval |= dirdb_basic_test8(); /* dirdbDiffPath() */

	retval |= dirdb_basic_test9(); /* dirdbFindAndRef(), sorting, dirdbUnref(), sorting */

	retval |= dirdb_basic_test10(); /* dirdbFindAndRef(), growing the children list */

	retval |= dirdb_basic_test11(); /* dirdbFindAndRef(), growing the database */

	retval |= dirdb_basic_test12(); /* dirdbUnref(), growing the free list */

	retval |= dirdb_basic_test13(); /* dirdbInit(), parsing database */

	retval |= dirdb_basic_test14(); /* dirdbFlush(), writing database */

	return retval;
}
