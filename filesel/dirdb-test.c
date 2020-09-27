//#define DIRDB_DEBUG 1

#include "dirdb.c"

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

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
}

char *cfConfigDir = "/foo/home/ocp/.ocp/";

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
	node0 = dirdbResolvePathAndRef ("");
	if (node0 != DIRDB_NOPARENT)
	{
		fprintf (stderr, "dirdbResolvePathAndRef(" ANSI_COLOR_RED "\"\"" ANSI_COLOR_RESET ") gave a node, and not DIRDB_NOPARENT\n");
		retval++;
		dirdbUnref(node0);
	} else {
		fprintf (stderr, "dirdbResolvePathAndRef(" ANSI_COLOR_GREEN "\"\"" ANSI_COLOR_RESET ") did not give a node, but DIRDB_NOPARENT\n");
	}

	/* trying to resolve /, should fail */
	node0 = dirdbResolvePathAndRef ("/");
	if (node0 != DIRDB_NOPARENT)
	{
		fprintf (stderr, "dirdbResolvePathAndRef(" ANSI_COLOR_RED "\"/\"" ANSI_COLOR_RESET ") gave a node, and not DIRDB_NOPARENT\n");
		retval++;
		dirdbUnref(node0);
	} else {
		fprintf (stderr, "dirdbResolvePathAndRef(" ANSI_COLOR_GREEN "\"/\"" ANSI_COLOR_RESET ") did not give a node, but DIRDB_NOPARENT\n");
	}

	/* trying to resolve NULL, should fail */
	node0 = dirdbResolvePathAndRef (NULL);
	if (node0 != DIRDB_NOPARENT)
	{
		fprintf (stderr, "dirdbResolvePathAndRef(" ANSI_COLOR_RED "NULL" ANSI_COLOR_RESET ") gave a node, and not DIRDB_NOPARENT\n");
		retval++;
		dirdbUnref(node0);
	} else {
		fprintf (stderr, "dirdbResolvePathAndRef(" ANSI_COLOR_GREEN "NULL" ANSI_COLOR_RESET ") did not give a node, but DIRDB_NOPARENT\n");
	}

	/* trying to resolve empty drive, should be OK */
	node0 = dirdbResolvePathAndRef ("file:");
	if (node0 != DIRDB_NOPARENT)
	{
		fprintf (stderr, "dirdbResolvePathAndRef(" ANSI_COLOR_GREEN "file:" ANSI_COLOR_RESET ") gave a node, and not DIRDB_NOPARENT\n");
		dirdbUnref(node0);
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
	node0 = dirdbResolvePathAndRef (test);
	if (node0 == DIRDB_NOPARENT)
	{
		fprintf (stderr, "dirdbResolvePathAndRef(" ANSI_COLOR_RED "65535 character long name" ANSI_COLOR_RESET ") failed to resolve\n");
		retval++;
	} else {
		fprintf (stderr, "dirdbResolvePathAndRef(" ANSI_COLOR_GREEN "65535 character long name" ANSI_COLOR_RESET ") gave a node\n");
		dirdbUnref(node0);
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

	node0 = dirdbResolvePathAndRef (test);
	if (node0 != DIRDB_NOPARENT)
	{
		fprintf (stderr, "dirdbResolvePathAndRef(" ANSI_COLOR_RED "65536 character long name" ANSI_COLOR_RESET ") gave a node, not DIRDB_NOPARENT\n");
		retval++;
	} else {
		fprintf (stderr, "dirdbResolvePathAndRef(" ANSI_COLOR_GREEN "65536 character long name" ANSI_COLOR_RESET ") gave a node\n");
		dirdbUnref(node0);
	}

	fprintf (stderr, ANSI_COLOR_CYAN "Creating nodes with dirdbResolvePathAndRef()\n" ANSI_COLOR_RESET);

	node1 = dirdbResolvePathAndRef ("file:/tmp/foo/test.s3m");
	/*node2 =*/ dirdbResolvePathAndRef ("file:/tmp/foo/moo.s3m");
	node3 = dirdbResolvePathAndRef ("file:/tmp/foo/test.s3m");
	/*node4 =*/ dirdbResolvePathAndRef ("file:/tmp/moo.s3m");
	/*node5 =*/ dirdbResolvePathAndRef ("file:/tmp/foo/");

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

	node1 = dirdbResolvePathAndRef ("file:/tmp/foo/test.s3m");
	node2 = dirdbResolvePathAndRef ("file:/tmp/foo/moo.s3m");
	node3 = dirdbResolvePathAndRef ("file:/tmp/foo/test.s3m");
	node4 = dirdbResolvePathAndRef ("file:/tmp/moo.s3m");
	node5 = dirdbResolvePathAndRef ("file:/tmp/foo/");

	node6 = dirdbFindAndRef (node5, "test.s3m");
	node7[0] = dirdbFindAndRef (node5, "test.mp0");
	node7[1] = dirdbFindAndRef (node5, "test.mp1");
	node7[2] = dirdbFindAndRef (node5, "test.mp2");
	node7[3] = dirdbFindAndRef (node5, "test.mp3");
	node7[4] = dirdbFindAndRef (node5, "test.mp4");
	node7[5] = dirdbFindAndRef (node5, "test.mp5");
	node7[6] = dirdbFindAndRef (node5, "test.mp6");
	node7[7] = dirdbFindAndRef (node5, "test.mp7");
	node7[8] = dirdbFindAndRef (node5, "test.mp8");
	node7[9] = dirdbFindAndRef (node5, "test.mp9");
	node7[10] = dirdbFindAndRef (node5, "test.mp10");
	node7[11] = dirdbFindAndRef (node5, "test.mp11");
	node7[12] = dirdbFindAndRef (node5, "test.mp12");
	node7[13] = dirdbFindAndRef (node5, "test.mp13");
	node7[14] = dirdbFindAndRef (node5, "test.mp14");
	node7[15] = dirdbFindAndRef (node5, "test.mp15");

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
		dirdbRef(node7[15]);
		if (dirdbData[node7[15]].refcount != 2)
		{
			fprintf (stderr, "Refcount not 2 after dirdbRef() as expected, but " ANSI_COLOR_RED "%d" ANSI_COLOR_RESET "\n", dirdbData[node7[15]].refcount);
			retval++;
		}
		dirdbUnref(node7[15]);
	}

	fprintf (stderr, "\n");

	fprintf (stderr, ANSI_COLOR_CYAN "Testing dirdbUnref()\n" ANSI_COLOR_RESET);
	dirdbUnref(node1);
	dirdbUnref(node2);
	dirdbUnref(node3);
	dirdbUnref(node4);
	dirdbUnref(node5);
	dirdbUnref(node6);
	for (i=0; i < 16; i++)
	{
		dirdbUnref(node7[i]);
	}

	for (i=0; i < dirdbNum; i++)
	{
		if ((dirdbData[i].adb_ref != DIRDB_NO_ADBREF) ||
		    (dirdbData[i].newadb_ref != DIRDB_NO_ADBREF) ||
		    (dirdbData[i].mdb_ref != DIRDB_NO_MDBREF) ||
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
	uint32_t node1, node1_;
	uint32_t node2;
	uint32_t node3;
	uint32_t node4;
	uint32_t node5;
	uint32_t node6;
	uint32_t node7;
	uint32_t node8;
	uint32_t node9;
	uint32_t node10;
	uint32_t node11;
	uint32_t node12;

	fprintf (stderr, ANSI_COLOR_CYAN "Going to test dirdbResolvePathWithBaseAndRef()\n" ANSI_COLOR_RESET);

	node1  = dirdbResolvePathAndRef ("file:");
	node1_ = dirdbResolvePathAndRef ("file:/");
	node2 = dirdbResolvePathAndRef ("file:/tmp");
	node3 = dirdbResolvePathAndRef ("file:/tmp/foo");
	node4 = dirdbResolvePathAndRef ("file:/tmp/foo/moo");
	node5 = dirdbResolvePathAndRef ("file:/tmp/foo/moo/super-power.mp3");

	node6 = dirdbResolvePathWithBaseAndRef (node2, "foo/moo/super-power.mp3");
	node7 = dirdbResolvePathWithBaseAndRef (node2, "/foo/moo/super-power.mp3");
	node8 = dirdbResolvePathWithBaseAndRef (node2, "foo///moo/super-power.mp3");
	node9 = dirdbResolvePathWithBaseAndRef (node2, NULL);
	node10 = dirdbResolvePathWithBaseAndRef (node2, "");
	node11 = dirdbResolvePathWithBaseAndRef (node2, ".");
	node12 = dirdbResolvePathWithBaseAndRef (node2, "./");


	if (node1 != node1_)
	{
		fprintf (stderr, ANSI_COLOR_RED "file:" ANSI_COLOR_RESET " and " ANSI_COLOR_RED "file:/" ANSI_COLOR_RESET " did not give us the same node\n");
		retval++;
	} else {
		fprintf (stderr, ANSI_COLOR_GREEN "file:" ANSI_COLOR_RESET " and " ANSI_COLOR_GREEN "file:/" ANSI_COLOR_RESET " give us the same node\n");

	}

	if (node6 != node7)
	{
		fprintf (stderr, ANSI_COLOR_RED "foo/moo/super-power.mp3" ANSI_COLOR_RESET " and " ANSI_COLOR_RED "/foo/moo/super-power.mp3" ANSI_COLOR_RESET " gave different results\n");
		retval++;
	} else {
		fprintf (stderr, ANSI_COLOR_GREEN "foo/moo/super-power.mp3" ANSI_COLOR_RESET " and " ANSI_COLOR_GREEN "foo///moo/super-power.mp3" ANSI_COLOR_RESET " gave same results\n");
	}

	if (node6 != node8)
	{
		fprintf (stderr, ANSI_COLOR_RED "foo/moo/super-power.mp3" ANSI_COLOR_RESET " and " ANSI_COLOR_RED "/foo/moo/super-power.mp3" ANSI_COLOR_RESET " gave different results\n");
		retval++;
	} else {
		fprintf (stderr, ANSI_COLOR_GREEN "foo/moo/super-power.mp3" ANSI_COLOR_RESET " and " ANSI_COLOR_GREEN "foo///moo/super-power.mp3" ANSI_COLOR_RESET " gave same results\n");
	}

	if (node9 != DIRDB_NOPARENT)
	{
		fprintf (stderr, ANSI_COLOR_RED "NULL" ANSI_COLOR_RESET " did not give us DIRDB_NOPARENT\n");
		retval++;
		dirdbUnref(node9);
	} else {
		fprintf (stderr, ANSI_COLOR_GREEN "NULL" ANSI_COLOR_RESET " gave us DIRDB_NOPARENT\n");
	}
	if (node10 != node2)
	{
		fprintf (stderr, ANSI_COLOR_RED "\"\"" ANSI_COLOR_RESET " did not give us parent\n");
		retval++;
		dirdbUnref(node10);
	} else {
		fprintf (stderr, ANSI_COLOR_GREEN "\"\"" ANSI_COLOR_RESET " gave us parent\n");
		dirdbUnref(node10);
	}
	if (node11 != DIRDB_NOPARENT)
	{
		fprintf (stderr, ANSI_COLOR_RED "." ANSI_COLOR_RESET " did not give us DIRDB_NOPARENT\n");
		retval++;
		dirdbUnref(node11);
	} else {
		fprintf (stderr, ANSI_COLOR_GREEN "." ANSI_COLOR_RESET " gave us DIRDB_NOPARENT\n");
	}
	if (node12 != DIRDB_NOPARENT)
	{
		fprintf (stderr, ANSI_COLOR_RED "./" ANSI_COLOR_RESET " did not give us DIRDB_NOPARENT\n");
		retval++;
		dirdbUnref(node12);
	} else {
		fprintf (stderr, ANSI_COLOR_GREEN "./" ANSI_COLOR_RESET " gave us DIRDB_NOPARENT\n");
	}

	dirdbUnref(node1);
	dirdbUnref(node1_);
	dirdbUnref(node2);
	dirdbUnref(node3);
	dirdbUnref(node4);
	dirdbUnref(node5);
	dirdbUnref(node6);
	dirdbUnref(node7);
	dirdbUnref(node8);

	for (i=0; i < dirdbNum; i++)
	{
		if ((dirdbData[i].adb_ref != DIRDB_NO_ADBREF) ||
		    (dirdbData[i].newadb_ref != DIRDB_NO_ADBREF) ||
		    (dirdbData[i].mdb_ref != DIRDB_NO_MDBREF) ||
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

	node5 = dirdbResolvePathAndRef ("file:/tmp/foo/moo/foo");

	node0 = dirdbFindAndRef (node5, NULL);
	if (node0 != DIRDB_NOPARENT)
	{
		fprintf (stderr, "dirdbFindAndRef(validparent, " ANSI_COLOR_RED "NULL" ANSI_COLOR_RESET") did not fail as expected\n");
		retval++;
		dirdbUnref(node0);
	} else {
		fprintf (stderr, "dirdbFindAndRef(validparent, " ANSI_COLOR_GREEN "NULL" ANSI_COLOR_RESET") failed as expected\n");
	}

	node0 = dirdbFindAndRef (node5, "");
	if (node0 != DIRDB_NOPARENT)
	{
		fprintf (stderr, "dirdbFindAndRef(validparent, " ANSI_COLOR_RED "\"\"" ANSI_COLOR_RESET") did not fail as expected\n");
		retval++;
		dirdbUnref(node0);
	} else {
		fprintf (stderr, "dirdbFindAndRef(validparent, " ANSI_COLOR_GREEN "\"\"" ANSI_COLOR_RESET") failed as expected\n");
	}

	node0 = dirdbFindAndRef (node5, ".");
	if (node0 != DIRDB_NOPARENT)
	{
		fprintf (stderr, "dirdbFindAndRef(validparent, \"" ANSI_COLOR_RED "." ANSI_COLOR_RESET"\") did not fail as expected\n");
		retval++;
		dirdbUnref(node0);
	} else {
		fprintf (stderr, "dirdbFindAndRef(validparent, \"" ANSI_COLOR_GREEN "." ANSI_COLOR_RESET"\") failed as expected\n");
	}
	node0 = dirdbFindAndRef (node5, "..");
	if (node0 != DIRDB_NOPARENT)
	{
		fprintf (stderr, "dirdbFindAndRef(validparent, \"" ANSI_COLOR_RED ".." ANSI_COLOR_RESET"\") did not fail as expected\n");
		retval++;
		dirdbUnref(node0);
	} else {
		fprintf (stderr, "dirdbFindAndRef(validparent, \"" ANSI_COLOR_GREEN ".." ANSI_COLOR_RESET"\") failed as expected\n");
	}

	node0 = dirdbFindAndRef (node5, "tmp/");
	if (node0 != DIRDB_NOPARENT)
	{
		fprintf (stderr, "dirdbFindAndRef(validparent, \"" ANSI_COLOR_RED "tmp/" ANSI_COLOR_RESET"\") did not fail as expected\n");
		retval++;
		dirdbUnref(node0);
	} else {
		fprintf (stderr, "dirdbFindAndRef(validparent, \"" ANSI_COLOR_GREEN "tmp/" ANSI_COLOR_RESET"\") failed as expected\n");
	}

	node0 = dirdbFindAndRef (0xffff0000, "test");
	if (node0 != DIRDB_NOPARENT)
	{
		fprintf (stderr, "dirdbFindAndRef(" ANSI_COLOR_RED "invalidparent" ANSI_COLOR_RESET ", \"test\") did not fail as expected\n");
		retval++;
		dirdbUnref(node0);
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

	/*node2 =*/ dirdbResolvePathAndRef ("file:/tmp");
	/*node3 =*/ dirdbResolvePathAndRef ("file:/tmp/foo");
	node4 = dirdbResolvePathAndRef ("file:/tmp/foo/moo");
	/*node5 =*/ dirdbResolvePathAndRef ("file:/tmp/foo/moo/super-power.mp3");

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

	node1 = dirdbResolvePathAndRef ("file:/");
	/*node2 =*/ dirdbResolvePathAndRef ("file:/tmp");
	/*node3 =*/ dirdbResolvePathAndRef ("file:/tmp/foo");
	node4 = dirdbResolvePathAndRef ("file:/tmp/foo/moo");
	/*node5 =*/ dirdbResolvePathAndRef ("file:/tmp/foo/moo/super-power.mp3");

	fprintf (stderr, ANSI_COLOR_CYAN "Testing dirdbGetFullname_malloc(no flags)\n" ANSI_COLOR_RESET);
	retval |= dirdbGetFullname_malloc_subtest (node4, "file:/tmp/foo/moo", 0);

	fprintf (stderr, ANSI_COLOR_CYAN "Testing dirdbGetFullname_malloc(DIRDB_FULLNAME_NOBASE)\n" ANSI_COLOR_RESET);
	retval |= dirdbGetFullname_malloc_subtest (node4, "/tmp/foo/moo", DIRDB_FULLNAME_NOBASE);

	fprintf (stderr, ANSI_COLOR_CYAN "Testing dirdbGetFullname_malloc(DIRDB_FULLNAME_NOBASE | DIRDB_FULLNAME_ENDSLASH)\n" ANSI_COLOR_RESET);
	retval |= dirdbGetFullname_malloc_subtest (node4, "/tmp/foo/moo/", DIRDB_FULLNAME_NOBASE | DIRDB_FULLNAME_ENDSLASH);

	fprintf (stderr, ANSI_COLOR_CYAN "Testing dirdbGetFullname_malloc(DIRDB_FULLNAME_ENDSLASH)\n" ANSI_COLOR_RESET);
	retval |= dirdbGetFullname_malloc_subtest (node4, "file:/tmp/foo/moo/", DIRDB_FULLNAME_ENDSLASH);

	fprintf (stderr, ANSI_COLOR_CYAN "Testing dirdbGetFullname_malloc(no flags)\n" ANSI_COLOR_RESET);
	retval |= dirdbGetFullname_malloc_subtest (node1, "file:", 0);

	fprintf (stderr, ANSI_COLOR_CYAN "Testing dirdbGetFullname_malloc(DIRDB_FULLNAME_NOBASE)\n" ANSI_COLOR_RESET);
	retval |= dirdbGetFullname_malloc_subtest (node1, "", DIRDB_FULLNAME_NOBASE);

	fprintf (stderr, ANSI_COLOR_CYAN "Testing dirdbGetFullname_malloc(DIRDB_FULLNAME_NOBASE | DIRDB_FULLNAME_ENDSLASH)\n" ANSI_COLOR_RESET);
	retval |= dirdbGetFullname_malloc_subtest (node1, "/", DIRDB_FULLNAME_NOBASE | DIRDB_FULLNAME_ENDSLASH);

	fprintf (stderr, ANSI_COLOR_CYAN "Testing dirdbGetFullname_malloc(DIRDB_FULLNAME_ENDSLASH)\n" ANSI_COLOR_RESET);
	retval |= dirdbGetFullname_malloc_subtest (node1, "file:/", DIRDB_FULLNAME_ENDSLASH);

	clear_dirdb();

	fprintf (stderr, "\n");

	return retval;
}

static int dirdb_basic_test7(void)
{
	int retval = 0;
 /* dirdbTagSetParent(), dirdbMakeMdbAdbRef(), dirdbTagRemoveUntaggedAndSubmit(), dirdbGetMdbAdb() */

	uint32_t node1 = dirdbResolvePathAndRef ("file:/tmp");
	uint32_t node2 = dirdbResolvePathAndRef ("file:/tmp/foo");
	uint32_t node3 = dirdbResolvePathAndRef ("file:/tmp/foo/test1.mp3");
	uint32_t node4 = dirdbResolvePathAndRef ("file:/tmp/foo/test2.mp3");
	uint32_t node5 = dirdbResolvePathAndRef ("file:/tmp/bar");
	uint32_t node6, node7, node8;
	int i;

	int first;
	uint32_t iter, mdb, adb;
	int found_node3, found_node4, found_node6, found_node7, found_node8;

	fprintf (stderr, ANSI_COLOR_CYAN "Testing initial dirdbTagSetParent(), dirdbMakeMdbAdbRef(), dirdbTagRemoveUntaggedAndSubmit() and dirdbGetMdbAdb()\n" ANSI_COLOR_RESET);

	dirdbTagSetParent (node1);

	dirdbMakeMdbAdbRef (node3, 1, 0xffffffff);
	dirdbMakeMdbAdbRef (node4, 2, 0xffffffff);
	node6 = dirdbResolvePathAndRef ("file:/tmp/bar/test3.mp3");
	node7 = dirdbResolvePathAndRef ("file:/tmp/bar/test4.mp3");
	dirdbMakeMdbAdbRef (node6, 3, 0xffffffff);
	dirdbMakeMdbAdbRef (node7, 4, 0xffffffff);

	dirdbTagRemoveUntaggedAndSubmit ();

	dirdbUnref (node1);
	dirdbUnref (node2);
	dirdbUnref (node3);
	dirdbUnref (node4);
	dirdbUnref (node5);
	dirdbUnref (node6);
	dirdbUnref (node7);

	first=1;
	iter = 0xa1234;
	mdb = 0xa5678;
	adb = 0xffeeff;
	found_node3=0;
	found_node4=0;
	found_node6=0;
	found_node7=0;
	while (!dirdbGetMdbAdb(&iter, &mdb, &adb, &first))
	{
		     if ((iter==node3) && (mdb==1)) found_node3++;
		else if ((iter==node4) && (mdb==2)) found_node4++;
		else if ((iter==node6) && (mdb==3)) found_node6++;
		else if ((iter==node7) && (mdb==4)) found_node7++;
		else {
			fprintf (stderr, "dirdbGetMdbAdb() gave an unknown node " ANSI_COLOR_RED "iter=%d mdb=%d adb=%d" ANSI_COLOR_RESET "\n", iter, mdb, adb);
			retval++;
		}
	}
	if (found_node3 != 1) {
		fprintf (stderr, "dirdbGetMdbAdb() did not reveal " ANSI_COLOR_RED "iter=%d mdb=1" ANSI_COLOR_RESET "\n", node3);
		retval++;
	} else {
		fprintf (stderr, "dirdbGetMdbAdb() gave us " ANSI_COLOR_GREEN "iter=%d mdb=1" ANSI_COLOR_RESET "\n", node3);
	}
	if (found_node4 != 1) {
		fprintf (stderr, "dirdbGetMdbAdb() did not reveal " ANSI_COLOR_RED "iter=%d mdb=2" ANSI_COLOR_RESET "\n", node4);
		retval++;
	} else {
		fprintf (stderr, "dirdbGetMdbAdb() gave us " ANSI_COLOR_GREEN "iter=%d mdb=2" ANSI_COLOR_RESET "\n", node4);
	}
	if (found_node6 != 1) {
		fprintf (stderr, "dirdbGetMdbAdb() did not reveal " ANSI_COLOR_RED "iter=%d mdb=3" ANSI_COLOR_RESET "\n", node6);
		retval++;
	} else {
		fprintf (stderr, "dirdbGetMdbAdb() gave us " ANSI_COLOR_GREEN "iter=%d mdb=3" ANSI_COLOR_RESET "\n", node6);
	}
	if (found_node7 != 1) {
		fprintf (stderr, "dirdbGetMdbAdb() did not reveal " ANSI_COLOR_RED "iter=%d mdb=4" ANSI_COLOR_RESET "\n", node7);
		retval++;
	} else {
		fprintf (stderr, "dirdbGetMdbAdb() gave us " ANSI_COLOR_GREEN "iter=%d mdb=4" ANSI_COLOR_RESET "\n", node7);
	}



	fprintf (stderr, ANSI_COLOR_CYAN "Testing patching with dirdbTagSetParent(), dirdbMakeMdbAdbRef(), dirdbTagRemoveUntaggedAndSubmit() and dirdbGetMdbAdb()\n" ANSI_COLOR_RESET);

	dirdbTagSetParent (node1);

	dirdbMakeMdbAdbRef (node3, 1, 0xffffffff);
	node8 = dirdbResolvePathAndRef ("file:/tmp/bar/test5.mp3");
	dirdbMakeMdbAdbRef (node6, 3, 0xffffffff);
	dirdbMakeMdbAdbRef (node8, 5, 0xffffffff);
	dirdbUnref (node8);

	dirdbTagRemoveUntaggedAndSubmit ();

	first=1;
	iter = 0xa1234;
	mdb = 0xa5678;
	adb = 0xffeeff;
	found_node3=0;
	found_node6=0;
	found_node8=0;
	while (!dirdbGetMdbAdb(&iter, &mdb, &adb, &first))
	{
		fprintf (stderr, "%d %d %d\n", iter, mdb, adb);
		     if ((iter==node3) && (mdb==1)) found_node3++;
		else if ((iter==node6) && (mdb==3)) found_node6++;
		else if ((iter==node8) && (mdb==5)) found_node8++;
		else {
			fprintf (stderr, "dirdbGetMdbAdb() gave an unknown node " ANSI_COLOR_RED "iter=%d mdb=%d adb=%d" ANSI_COLOR_RESET "\n", iter, mdb, adb);
			retval++;
		}
	}
	if (found_node3 != 1) {
		fprintf (stderr, "dirdbGetMdbAdb() did not reveal " ANSI_COLOR_RED "iter=%d mdb=1" ANSI_COLOR_RESET "\n", node3);
		retval++;
	} else {
		fprintf (stderr, "dirdbGetMdbAdb() gave us " ANSI_COLOR_GREEN "iter=%d mdb=1" ANSI_COLOR_RESET "\n", node3);
	}
	if (found_node6 != 1) {
		fprintf (stderr, "dirdbGetMdbAdb() did not reveal " ANSI_COLOR_RED "iter=%d mdb=3" ANSI_COLOR_RESET "\n", node6);
		retval++;
	} else {
		fprintf (stderr, "dirdbGetMdbAdb() gave us " ANSI_COLOR_GREEN "iter=%d mdb=3" ANSI_COLOR_RESET "\n", node6);
	}
	if (found_node8 != 1) {
		fprintf (stderr, "dirdbGetMdbAdb() did not reveal " ANSI_COLOR_RED "iter=%d mdb=5" ANSI_COLOR_RESET "\n", node8);
		retval++;
	} else {
		fprintf (stderr, "dirdbGetMdbAdb() gave us " ANSI_COLOR_GREEN "iter=%d mdb=5" ANSI_COLOR_RESET "\n", node8);
	}


	fprintf (stderr, ANSI_COLOR_CYAN "Testing clearing with dirdbTagSetParent(), dirdbTagRemoveUntaggedAndSubmit() and dirdbGetMdbAdb()\n" ANSI_COLOR_RESET);

	dirdbTagSetParent (DIRDB_NOPARENT);
	dirdbTagRemoveUntaggedAndSubmit ();

	first=1;
	iter = 0xa1234;
	mdb = 0xa5678;
	adb = 0xffeeff;
	while (!dirdbGetMdbAdb(&iter, &mdb, &adb, &first))
	{
		fprintf (stderr, "dirdbGetMdbAdb() gave a node " ANSI_COLOR_RED "iter=%d mdb=%d adb=%d" ANSI_COLOR_RESET "\n", iter, mdb, adb);
		retval++;
	}

	for (i=0; i < dirdbNum; i++)
	{
		if ((dirdbData[i].adb_ref != DIRDB_NO_ADBREF) ||
		    (dirdbData[i].newadb_ref != DIRDB_NO_ADBREF) ||
		    (dirdbData[i].mdb_ref != DIRDB_NO_MDBREF) ||
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

int main(int argc, char *argv[])
{
	int retval = 0;

	retval |= dirdb_basic_test1(); /* dirdbResolvePathAndRef() */

	retval |= dirdb_basic_test2(); /* dirdbFindAndRef() + dirdbUnref() */

	retval |= dirdb_basic_test3(); /* dirdbResolvePathWithBaseAndRef() */

	retval |= dirdb_basic_test4(); /* dirdbFindAndRef() */

	retval |= dirdb_basic_test5(); /* dirdbGetName_malloc() */

	retval |= dirdb_basic_test6(); /* dirdbGetFullname_malloc() */

	retval |= dirdb_basic_test7(); /* dirdbTagSetParent(), dirdbMakeMdbAdbRef(), dirdbTagRemoveUntaggedAndSubmit(), dirdbGetMdbAdb() */

	return retval;
}
