/* OpenCP Module Player
 * copyright (c) 2020-'25 Stian Skjelstad <stian.skjelstad@gmail.com>
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
#define CFDATAHOMEDIR_OVERRIDE "/foo/home/ocp/.ocp/"
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
	}
	free (dirdbData);
	dirdbData=0;
	dirdbNum=0;
	dirdbDirty=0;
	dirdbRootChild = DIRDB_NOPARENT;
	dirdbFreeChild = DIRDB_NOPARENT;
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

int main(int argc, char *argv[])
{
	int retval = 0;

	retval |= dirdb_basic_test1(); /* dirdbResolvePathAndRef() */

	retval |= dirdb_basic_test2(); /* dirdbFindAndRef() + dirdbUnref() */

	retval |= dirdb_basic_test3(); /* dirdbResolvePathWithBaseAndRef() */

	retval |= dirdb_basic_test4(); /* dirdbFindAndRef() */

	retval |= dirdb_basic_test5(); /* dirdbGetName_malloc() */

	retval |= dirdb_basic_test6(); /* dirdbGetFullname_malloc() */

	retval |= dirdb_basic_test8(); /* dirdbDiffPath() */

	retval |= dirdb_basic_test7(); /* dirdbTagSetParent(), dirdbMakeMdbRef(), dirdbTagRemoveUntaggedAndSubmit(), dirdbGetMdb() */

	return retval;
}
