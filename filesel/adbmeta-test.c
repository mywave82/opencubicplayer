/* unit test for adbmeta.c */

int adbmeta_silene_open_errors = 0;
#define ADBMETA_SILENCE_OPEN_ERRORS adbmeta_silene_open_errors
#define CFDATAHOMEDIR_OVERRIDE "/tmp/"

#include "adbmeta.c"
#include "../stuff/file.c"

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

const char test_blob[] = {
	/* header */
	'O', 'C', 'P', 'A', 'r', 'c', 'h', 'i', 'v', 'e', 'M', 'e', 't', 'a', '\x1b', '\x00',
	/* entries #4 */
	0, 0, 0, 4,

	/* entry 1 */
	'f', 'o', 'o', '1', 0,
	's', 'z', 0,
	0, 0, 0, 0, 0, 0, 0, 10,
	0, 0, 0, 4,
	't', 'e', 's', 't',

	/* entry 2 */
	'b', 'a', 'r', '2', 0,
	't', 'u', 0,
	0, 0, 0, 0, 0, 0, 0, 14,
	0, 0, 0, 6,
	'm', 'o', 'o', 'm', 'o', 'o',

	/* entry 3 */
	'o', 'p', 'e', 'n', 0,
	'S', 'U', 0,
	0, 0, 0, 0, 0, 0, 20, 12,
	0, 0, 0, 3,
	'T', 'X', 'T',

	/* entry 4 */
	's', 'o', 'u', 'r', 'c', 'e', 0,
	'L', 'A', 'B', 0,
	0, 0, 0, 0, 0, 0, 20, 14,
	0, 0, 0, 4,
	'T', 'E', 'X', 'T'
};

const struct adbMetaEntry_t test_expect[4] = {
	{"foo1", 10, "sz", 4, (unsigned char *)"test"},
	{"bar2", 14, "tu", 6, (unsigned char *)"moomoo"},
	{"open", 5132, "SU", 3, (unsigned char *)"TXT"},
	{"source", 5134, "LAB", 4, (unsigned char *)"TEXT"}
};

static int adbmeta_basic_test1 (void)
{
	int f;
	int retval = 0;
	int i;

	f = open ("/tmp/CPARCMETA.DAT", O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

	if (!f)
	{
		fprintf (stderr, "adbmeta_basic_test1: " ANSI_COLOR_RED " open(\"/tmp/CPARCMETA.DAT\", O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR): %s" ANSI_COLOR_RESET "\n", strerror (errno));
		return -1;
	}

	if (write (f, test_blob, sizeof (test_blob)) != sizeof (test_blob))
	{
		fprintf (stderr, "adbmeta_basic_test1: write(\"/tmp/CPARCMETA.DAT\", block, sizeof (blob)): " ANSI_COLOR_RED "%s" ANSI_COLOR_RESET "\n", strerror (errno));
		close (f);
		unlink ("/tmp/CPARCMETA.DAT");
		return -1;
	}
	close (f);

	adbmeta_silene_open_errors = 0;

	if (adbMetaInit (0))
	{
		retval |= 1;
		fprintf (stderr, "adbmeta_basic_test1: " ANSI_COLOR_RED "adbMetaInit() failed " ANSI_COLOR_RESET "\n");
	}

	if (adbMetaCount != 4)
	{
		retval |= 1;
		fprintf (stderr, "adbmeta_basic_test1: " ANSI_COLOR_RED "adbMetaCount should be 4, but is %ld" ANSI_COLOR_RESET "\n", (long)adbMetaCount);
	}

	for (i=0; i < 4; i++)
	{
		if (adbMetaCount >= (i+1))
		{
			if (strcmp (adbMetaEntries[i]->filename, test_expect[i].filename))
			{
				retval |= 2;
				fprintf (stderr, "adbmeta_basic_test1: " ANSI_COLOR_RED "adbMetaEntries[%d]->filename should be \"%s\", but is \"%s\"" ANSI_COLOR_RESET "\n", i, test_expect[i].filename, adbMetaEntries[i]->filename);
			}
			if (adbMetaEntries[i]->filesize != test_expect[i].filesize)
			{
				retval |= 2;
				fprintf (stderr, "adbmeta_basic_test1: " ANSI_COLOR_RED "adbMetaEntries[%d]->filesize should be %" PRIu64 ", but is %" PRIu64 ANSI_COLOR_RESET "\n", i, test_expect[i].filesize, adbMetaEntries[i]->filesize);
			}
			if (strcmp (adbMetaEntries[i]->SIG, test_expect[i].SIG))
			{
				retval |= 2;
				fprintf (stderr, "adbmeta_basic_test1: " ANSI_COLOR_RED "adbMetaEntries[%d]->SIG should be \"%s\", but is \"%s\"" ANSI_COLOR_RESET "\n", i, test_expect[i].SIG, adbMetaEntries[i]->SIG);
			}
			if (adbMetaEntries[i]->datasize != test_expect[i].datasize)
			{
				retval |= 2;
				fprintf (stderr, "adbmeta_basic_test1: " ANSI_COLOR_RED "adbMetaEntries[%d]->datasize should be %" PRIu32 ", but is %" PRIu32 ANSI_COLOR_RESET "\n", i, test_expect[i].datasize, adbMetaEntries[i]->datasize);
			} else if (memcmp (adbMetaEntries[i]->data, test_expect[i].data, test_expect[i].datasize))
			{
				int j;
				retval |= 2;
				fprintf (stderr, "adbmeta_basic_test1: " ANSI_COLOR_RED "adbMetaEntries[%d]->data should be", i);
				for (j = 0; j < test_expect[i].datasize; j++)
				{
					fprintf (stderr, " %02x", (uint8_t)(test_expect[i].data[j]));
				}
				fprintf (stderr, ", but is");

				for (j = 0; j < test_expect[i].datasize; j++)
				{
					fprintf (stderr, " %02x", (uint8_t)(adbMetaEntries[i]->data[j]));
				}
				fprintf (stderr, ANSI_COLOR_RESET "\n");
			}
		}
	}

	adbMetaClose ();

	unlink ("/tmp/CPARCMETA.DAT");
	return retval;
}

static int adbmeta_basic_test2 (void)
{
	int f;
	int retval = 0;
	int i;
	char buffer[65536];
	int fill;

	f = open ("/tmp/CPARCMETA.DAT", O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

	if (!f)
	{
		fprintf (stderr, "adbmeta_basic_test2: " ANSI_COLOR_RED " open(\"/tmp/CPARCMETA.DAT\", O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR): %s" ANSI_COLOR_RESET "\n", strerror (errno));
		return -1;
	}

	if (write (f, test_blob, sizeof (test_blob)) != sizeof (test_blob))
	{
		fprintf (stderr, "adbmeta_basic_test2: write(\"/tmp/CPARCMETA.DAT\", block, sizeof (blob)): " ANSI_COLOR_RED "%s" ANSI_COLOR_RESET "\n", strerror (errno));
		close (f);
		unlink ("/tmp/CPARCMETA.DAT");
		return -1;
	}

	adbmeta_silene_open_errors = 0;

	if (adbMetaInit (0))
	{
		retval |= 1;
		fprintf (stderr, "adbmeta_basic_test2: " ANSI_COLOR_RED "adbMetaInit() failed " ANSI_COLOR_RESET "\n");
	}

	ftruncate (f, 0);
	close (f);

	adbMetaDirty = 1;

	adbMetaClose ();

	f = open ("/tmp/CPARCMETA.DAT", O_RDONLY);

	if (!f)
	{
		fprintf (stderr, "adbmeta_basic_test2: " ANSI_COLOR_RED " open(\"/tmp/CPARCMETA.DAT\", O_RDONLY): %s" ANSI_COLOR_RESET "\n", strerror (errno));
		return -1;
	}

	if ((fill = read (f, buffer, sizeof (buffer))) < 0)
	{
		fprintf (stderr, "adbmeta_basic_test2: read(\"/tmp/CPARCMETA.DAT\"): " ANSI_COLOR_RED "%s" ANSI_COLOR_RESET "\n", strerror (errno));
		close (f);
		unlink ("/tmp/CPARCMETA.DAT");
		return -1;
	}
	close (f);

	unlink ("/tmp/CPARCMETA.DAT");

	if (fill != sizeof (test_blob))
	{
		retval |= 1;
		fprintf (stderr, "adbmeta_basic_test2: " ANSI_COLOR_RED "new file size %d != %d" ANSI_COLOR_RESET "\n", fill, (int)sizeof (test_blob));
	}
	if (memcmp (buffer, test_blob, (fill < sizeof (test_blob)) ? fill : sizeof (test_blob)))
	{
		fprintf (stderr, "adbmeta_basic_test2: " ANSI_COLOR_RED "new file content missmatch" ANSI_COLOR_RESET "\n");
	}

	if (retval)
	{
		fprintf (stderr, "expected data:\n");
		for (i=0; i < sizeof (test_blob); i++)
		{
			fprintf (stderr, " %02x", (uint8_t)(test_blob[i]));
		}
		fprintf (stderr, "\nactual data:\n");
		for (i=0; i < fill; i++)
		{
			fprintf (stderr, " %02x", (uint8_t)(buffer[i]));
		}
		fprintf (stderr, "\n");
	}

	return retval;
}

static int adbmeta_basic_test3 (void)
{
	int retval = 0;
	int i;

	unlink ("/tmp/CPARCMETA.DAT");

	adbmeta_silene_open_errors = 1;

	adbMetaInit (0);

	adbMetaAdd (test_expect[1].filename, test_expect[1].filesize, test_expect[1].SIG, test_expect[1].data, test_expect[1].datasize);

	adbMetaAdd (test_expect[3].filename, test_expect[3].filesize, test_expect[3].SIG, test_expect[3].data, test_expect[3].datasize);

	adbMetaAdd (test_expect[2].filename, test_expect[2].filesize, test_expect[2].SIG, test_expect[2].data, test_expect[2].datasize);

	adbMetaAdd (test_expect[0].filename, test_expect[0].filesize, test_expect[0].SIG, test_expect[0].data, test_expect[0].datasize);

	if (adbMetaCount != 4)
	{
		retval |= 1;
		fprintf (stderr, "adbmeta_basic_test3: " ANSI_COLOR_RED "adbMetaCount != 4" ANSI_COLOR_RESET "\n");
	}

	for (i=0; i < adbMetaCount; i++)
	{
		if (strcmp (adbMetaEntries[i]->filename, test_expect[i].filename))
		{
			retval |= 2;
		}
	}

	if (retval & 2)
	{
		for (i=0; i < adbMetaCount; i++)
		{
			if (strcmp (adbMetaEntries[i]->filename, test_expect[i].filename))
			{
				fprintf (stderr, "adbmeta_basic_test3: " ANSI_COLOR_RED "expected file \"%s\" (filesize=%d), but got \"%s\" (filesize=%d)" ANSI_COLOR_RESET "\n", test_expect[i].filename, (int)(test_expect[i].filesize), adbMetaEntries[i]->filename, (int)(adbMetaEntries[i]->filesize));
			} else {
				fprintf (stderr, "adbmeta_basic_test3:      got file \"%s\" (filesize=%d)\n", adbMetaEntries[i]->filename, (int)(adbMetaEntries[i]->filesize));
			}
		}
	}

	adbMetaDirty = 0;

	adbMetaClose ();

	unlink ("/tmp/CPARCMETA.DAT");

	return retval;
}

static int adbmeta_basic_test4 (void)
{
const struct adbMetaEntry_t test_many_expect[32] = {
	{"foo 10", 10, "test", 6, (unsigned char *)"moomoo"},
	{"foo 14", 14, "test", 6, (unsigned char *)"moomoo"},
	{"foo 16", 16, "test", 6, (unsigned char *)"moomoo"},
	{"foo 18", 18, "test", 6, (unsigned char *)"moomoo"},
	{"foo 20", 20, "test", 6, (unsigned char *)"moomoo"},
	{"foo 24", 24, "test", 6, (unsigned char *)"moomoo"},
	{"foo 26", 26, "test", 6, (unsigned char *)"moomoo"},
	{"foo 28", 28, "test", 6, (unsigned char *)"moomoo"},
	{"foo 30", 30, "test", 6, (unsigned char *)"moomoo"},
	{"foo 34", 34, "test", 6, (unsigned char *)"moomoo"},
	{"foo 36", 36, "test", 6, (unsigned char *)"moomoo"},
	{"foo 38", 38, "test", 6, (unsigned char *)"moomoo"},
	{"foo 40", 40, "test", 6, (unsigned char *)"moomoo"},
	{"foo 44", 44, "test", 6, (unsigned char *)"moomoo"},
	{"foo 46", 46, "test", 6, (unsigned char *)"moomoo"},
	{"foo 48", 48, "test", 6, (unsigned char *)"moomoo"},
	{"foo 50", 50, "test", 6, (unsigned char *)"moomoo"},
	{"foo 54", 54, "test", 6, (unsigned char *)"moomoo"},
	{"foo 56", 56, "test", 6, (unsigned char *)"moomoo"},
	{"foo 58", 58, "test", 6, (unsigned char *)"moomoo"},
	{"foo 60", 60, "test", 6, (unsigned char *)"moomoo"},
	{"foo 64", 64, "test", 6, (unsigned char *)"moomoo"},
	{"foo 66", 66, "test", 6, (unsigned char *)"moomoo"},
	{"foo 68", 68, "test", 6, (unsigned char *)"moomoo"},
	{"foo 70", 70, "test", 6, (unsigned char *)"moomoo"},
	{"foo 74", 74, "test", 6, (unsigned char *)"moomoo"},
	{"foo 76", 76, "test", 6, (unsigned char *)"moomoo"},
	{"foo 78", 78, "test", 6, (unsigned char *)"moomoo"},
	{"foo 80", 80, "test", 6, (unsigned char *)"moomoo"},
	{"foo 84", 84, "test", 6, (unsigned char *)"moomoo"},
	{"foo 86", 86, "test", 6, (unsigned char *)"moomoo"},
	{"foo 88", 88, "test", 6, (unsigned char *)"moomoo"}
};


	const int order[32] = {
3,
25,
11,
0,
1,
2,
12,
26,
27,
28,
16,
17,
13,
14,
9,
10,
20,
21,
22,
15,
18,
19,
4,
23,
24,
29,
30,
5,
8,
6,
7,
31
	};
	int retval = 0;
	int i;

	unlink ("/tmp/CPARCMETA.DAT");

	adbmeta_silene_open_errors = 1;

	adbMetaInit (0);

	for (i=0; i < 32; i++)
	{
		adbMetaAdd (test_many_expect[order[i]].filename,
		            test_many_expect[order[i]].filesize,
		            test_many_expect[order[i]].SIG,
		            test_many_expect[order[i]].data,
		            test_many_expect[order[i]].datasize);
	}

	if (adbMetaCount != 32)
	{
		retval |= 1;
		fprintf (stderr, "adbmeta_basic_test4: " ANSI_COLOR_RED "adbMetaCount != 32" ANSI_COLOR_RESET "\n");
	}

	for (i=0; i < adbMetaCount; i++)
	{
		if (strcmp (adbMetaEntries[i]->filename, test_many_expect[i].filename))
		{
			retval |= 2;
		}
	}

	if (retval & 2)
	{
		for (i=0; i < adbMetaCount; i++)
		{
			if (strcmp (adbMetaEntries[i]->filename, test_many_expect[i].filename))
			{
				fprintf (stderr, "adbmeta_basic_test4: " ANSI_COLOR_RED "expected file \"%s\" (filesize=%d), but got \"%s\" (filesize=%d)" ANSI_COLOR_RESET "\n", test_many_expect[i].filename, (int)(test_many_expect[i].filesize), adbMetaEntries[i]->filename, (int)(adbMetaEntries[i]->filesize));
			} else {
				fprintf (stderr, "adbmeta_basic_test4:      got file \"%s\" (filesize=%d)\n", adbMetaEntries[i]->filename, (int)(adbMetaEntries[i]->filesize));
			}
		}
	}

	adbMetaDirty = 0;

	adbMetaClose ();

	unlink ("/tmp/CPARCMETA.DAT");

	return retval;
}

static int adbmeta_basic_test5 (void)
{
const struct adbMetaEntry_t test_many_expect[32] = {
	{"foo 10", 10, "test01", 6, (unsigned char *)"moomoo"},
	{"foo 10", 10, "test02", 6, (unsigned char *)"moomoo"},
	{"foo 10", 10, "test03", 6, (unsigned char *)"moomoo"},
	{"foo 10", 10, "test04", 6, (unsigned char *)"moomoo"},
	{"foo 12", 12, "test05", 6, (unsigned char *)"moomoo"},
	{"foo 12", 12, "test06", 6, (unsigned char *)"moomoo"},
	{"foo 12", 12, "test07", 6, (unsigned char *)"moomoo"},
	{"foo 12", 12, "test08", 6, (unsigned char *)"moomoo"},
	{"foo 14", 14, "test09", 6, (unsigned char *)"moomoo"},
	{"foo 14", 14, "test10", 6, (unsigned char *)"moomoo"},
	{"foo 14", 14, "test11", 6, (unsigned char *)"moomoo"},
	{"foo 14", 14, "test12", 6, (unsigned char *)"moomoo"},
	{"foo 16", 16, "test13", 6, (unsigned char *)"moomoo"},
	{"foo 16", 16, "test14", 6, (unsigned char *)"moomoo"},
	{"foo 16", 16, "test15", 6, (unsigned char *)"moomoo"},
	{"foo 16", 16, "test16", 6, (unsigned char *)"moomoo"},
	{"foo 18", 18, "test17", 6, (unsigned char *)"moomoo"},
	{"foo 18", 18, "test18", 6, (unsigned char *)"moomoo"},
	{"foo 18", 18, "test19", 6, (unsigned char *)"moomoo"},
	{"foo 18", 18, "test20", 6, (unsigned char *)"moomoo"},
	{"foo 20", 20, "test21", 6, (unsigned char *)"moomoo"},
	{"foo 20", 20, "test22", 6, (unsigned char *)"moomoo"},
	{"foo 20", 20, "test23", 6, (unsigned char *)"moomoo"},
	{"foo 20", 20, "test24", 6, (unsigned char *)"moomoo"},
	{"foo 22", 22, "test25", 6, (unsigned char *)"moomoo"},
	{"foo 22", 22, "test26", 6, (unsigned char *)"moomoo"},
	{"foo 22", 22, "test27", 6, (unsigned char *)"moomoo"},
	{"foo 22", 22, "test28", 6, (unsigned char *)"moomoo"},
	{"foo 24", 24, "test29", 6, (unsigned char *)"moomoo"},
	{"foo 24", 24, "test30", 6, (unsigned char *)"moomoo"},
	{"foo 24", 24, "test31", 6, (unsigned char *)"moomoo"},
	{"foo 24", 24, "test32", 6, (unsigned char *)"moomoo"}
};


	const int order[32] = {
3,
25,
11,
0,
1,
2,
12,
26,
27,
28,
16,
17,
13,
14,
9,
10,
20,
21,
22,
15,
18,
19,
4,
23,
24,
29,
30,
5,
8,
6,
7,
31
	};
	int retval = 0;
	int i;

	unlink ("/tmp/CPARCMETA.DAT");

	adbmeta_silene_open_errors = 1;

	adbMetaInit (0);

	for (i=0; i < 32; i++)
	{
		adbMetaAdd (test_many_expect[order[i]].filename,
		            test_many_expect[order[i]].filesize,
		            test_many_expect[order[i]].SIG,
		            test_many_expect[order[i]].data,
		            test_many_expect[order[i]].datasize);
	}

	if (adbMetaCount != 32)
	{
		retval |= 1;
		fprintf (stderr, "adbmeta_basic_test5: " ANSI_COLOR_RED "adbMetaCount != 32" ANSI_COLOR_RESET "\n");
	}

	for (i=0; i < adbMetaCount; i++)
	{
		if (strcmp (adbMetaEntries[i]->filename, test_many_expect[i].filename))
		{
			retval |= 2;
		}
	}

	if (retval & 2)
	{
		for (i=0; i < adbMetaCount; i++)
		{
			if (strcmp (adbMetaEntries[i]->filename, test_many_expect[i].filename))
			{
				fprintf (stderr, "adbmeta_basic_test5: " ANSI_COLOR_RED "expected file \"%s\" (filesize=%d), but got \"%s\" (filesize=%d)" ANSI_COLOR_RESET "\n", test_many_expect[i].filename, (int)(test_many_expect[i].filesize), adbMetaEntries[i]->filename, (int)(adbMetaEntries[i]->filesize));
			} else {
				fprintf (stderr, "adbmeta_basic_test5:      got file \"%s\" (filesize=%d)\n", adbMetaEntries[i]->filename, (int)(adbMetaEntries[i]->filesize));
			}
		}
	}

	adbMetaDirty = 0;

	adbMetaClose ();

	unlink ("/tmp/CPARCMETA.DAT");

	return retval;
}

static int adbmeta_basic_test6 (void)
{
const struct adbMetaEntry_t test_many_expect[7] = {
	{"foo 11", 11, "test", 6, (unsigned char *)"moomo1"},
	{"foo 12", 12, "test", 6, (unsigned char *)"moomo2"},
	{"foo 13", 13, "test", 6, (unsigned char *)"moomo3"},
	{"foo 14", 14, "test", 6, (unsigned char *)"moomo4"},
	{"foo 15", 15, "test", 6, (unsigned char *)"moomo5"},
	{"foo 16", 16, "test", 6, (unsigned char *)"moomo6"},
	{"foo 17", 17, "test", 6, (unsigned char *)"moomo7"},
};
const struct adbMetaEntry_t test_many_insert[14] = {
	{"foo 17", 17, "test", 6, (unsigned char *)"moomoo"},
	{"foo 12", 12, "test", 6, (unsigned char *)"moomoo"},
	{"foo 11", 11, "test", 6, (unsigned char *)"moomoo"},
	{"foo 14", 14, "test", 6, (unsigned char *)"moomoo"},
	{"foo 13", 13, "test", 6, (unsigned char *)"moomoo"},
	{"foo 15", 15, "test", 6, (unsigned char *)"moomoo"},
	{"foo 16", 16, "test", 6, (unsigned char *)"moomoo"},
	{"foo 11", 11, "test", 6, (unsigned char *)"moomo1"},
	{"foo 12", 12, "test", 6, (unsigned char *)"moomo2"},
	{"foo 13", 13, "test", 6, (unsigned char *)"moomo3"},
	{"foo 14", 14, "test", 6, (unsigned char *)"moomo4"},
	{"foo 15", 15, "test", 6, (unsigned char *)"moomo5"},
	{"foo 16", 16, "test", 6, (unsigned char *)"moomo6"},
	{"foo 17", 17, "test", 6, (unsigned char *)"moomo7"},
};
	int retval = 0;
	int i;

	unlink ("/tmp/CPARCMETA.DAT");

	adbmeta_silene_open_errors = 1;

	adbMetaInit (0);

	for (i=0; i < 14; i++)
	{
		adbMetaAdd (test_many_insert[i].filename,
		            test_many_insert[i].filesize,
		            test_many_insert[i].SIG,
		            test_many_insert[i].data,
		            test_many_insert[i].datasize);
	}

	if (adbMetaCount != 7)
	{
		retval |= 1;
		fprintf (stderr, "adbmeta_basic_test6: " ANSI_COLOR_RED "adbMetaCount != 7" ANSI_COLOR_RESET "\n");
	}

	for (i=0; i < ((adbMetaCount <= 7) ? adbMetaCount : 7); i++)
	{
		if (strcmp (adbMetaEntries[i]->filename, test_many_expect[i].filename))
		{
			retval |= 2;
		}
	}

	if (retval & 2)
	{
		for (i=0; i < ((adbMetaCount <= 7) ? adbMetaCount : 7); i++)
		{
			if (strcmp (adbMetaEntries[i]->filename, test_many_expect[i].filename))
			{
				fprintf (stderr, "adbmeta_basic_test6: " ANSI_COLOR_RED "expected file \"%s\" (filesize=%d), but got \"%s\" (filesize=%d)" ANSI_COLOR_RESET "\n", test_many_expect[i].filename, (int)(test_many_expect[i].filesize), adbMetaEntries[i]->filename, (int)(adbMetaEntries[i]->filesize));
			} else {
				fprintf (stderr, "adbmeta_basic_test6:      got file \"%s\" (filesize=%d)\n", adbMetaEntries[i]->filename, (int)(adbMetaEntries[i]->filesize));
			}
		}
	}

	adbMetaDirty = 0;

	adbMetaClose ();

	unlink ("/tmp/CPARCMETA.DAT");

	return retval;
}

static int adbmeta_basic_test7 (void)
{
const struct adbMetaEntry_t test_many_expect[4] = {
	{"foo 11", 11, "test", 6, (unsigned char *)"moomo1"},
	{"foo 13", 13, "test", 6, (unsigned char *)"moomo3"},
	{"foo 15", 15, "test", 6, (unsigned char *)"moomo5"},
	{"foo 17", 17, "test", 6, (unsigned char *)"moomo7"},
};
const struct adbMetaEntry_t test_many_insert[7] = {
	{"foo 11", 11, "test", 6, (unsigned char *)"moomo1"},
	{"foo 12", 12, "test", 6, (unsigned char *)"moomo2"},
	{"foo 13", 13, "test", 6, (unsigned char *)"moomo3"},
	{"foo 14", 14, "test", 6, (unsigned char *)"moomo4"},
	{"foo 15", 15, "test", 6, (unsigned char *)"moomo5"},
	{"foo 16", 16, "test", 6, (unsigned char *)"moomo6"},
	{"foo 17", 17, "test", 6, (unsigned char *)"moomo7"},
};
const struct adbMetaEntry_t test_many_remove[3] = {
	{"foo 12", 12, "test", 6, (unsigned char *)"moomo2"},
	{"foo 14", 14, "test", 6, (unsigned char *)"moomo4"},
	{"foo 16", 16, "test", 6, (unsigned char *)"moomo6"},
};

	int retval = 0;
	int i;

	unlink ("/tmp/CPARCMETA.DAT");

	adbmeta_silene_open_errors = 1;

	adbMetaInit (0);

	for (i=0; i < 7; i++)
	{
		adbMetaAdd (test_many_insert[i].filename,
		            test_many_insert[i].filesize,
		            test_many_insert[i].SIG,
		            test_many_insert[i].data,
		            test_many_insert[i].datasize);
	}

	for (i=0; i < 3; i++)
	{
		adbMetaRemove (test_many_remove[i].filename,
		               test_many_remove[i].filesize,
		               test_many_remove[i].SIG);
	}

	if (adbMetaCount != 4)
	{
		retval |= 1;
		fprintf (stderr, "adbmeta_basic_test7: " ANSI_COLOR_RED "adbMetaCount != 4" ANSI_COLOR_RESET "\n");
	}

	for (i=0; i < ((adbMetaCount <= 4) ? adbMetaCount : 4); i++)
	{
		if (strcmp (adbMetaEntries[i]->filename, test_many_expect[i].filename))
		{
			retval |= 2;
		}
	}

	if (retval & 2)
	{
		for (i=0; i < ((adbMetaCount <= 4) ? adbMetaCount : 4); i++)
		{
			if (strcmp (adbMetaEntries[i]->filename, test_many_expect[i].filename))
			{
				fprintf (stderr, "adbmeta_basic_test7: " ANSI_COLOR_RED "expected file \"%s\" (filesize=%d), but got \"%s\" (filesize=%d)" ANSI_COLOR_RESET "\n", test_many_expect[i].filename, (int)(test_many_expect[i].filesize), adbMetaEntries[i]->filename, (int)(adbMetaEntries[i]->filesize));
			} else {
				fprintf (stderr, "adbmeta_basic_test7:      got file \"%s\" (filesize=%d)\n", adbMetaEntries[i]->filename, (int)(adbMetaEntries[i]->filesize));
			}
		}
	}

	adbMetaDirty = 0;

	adbMetaClose ();

	unlink ("/tmp/CPARCMETA.DAT");

	return retval;
}

static int adbmeta_basic_test8 (void)
{
const struct adbMetaEntry_t test_many_insert[7] = {
	{"foo 12", 11, "test", 6, (unsigned char *)"moomo1"},
	{"foo 12", 13, "test", 6, (unsigned char *)"moomo2"},
	{"foo 12", 15, "test", 6, (unsigned char *)"moomo3"},
	{"foo 12", 17, "test", 6, (unsigned char *)"moomo4"},
	{"foo 12", 19, "test", 6, (unsigned char *)"moomo5"},
	{"foo 12", 21, "test", 6, (unsigned char *)"moomo6"},
	{"foo 12", 23, "test", 6, (unsigned char *)"moomo7"},
};

	int retval = 0;
	int i;

	unlink ("/tmp/CPARCMETA.DAT");

	adbmeta_silene_open_errors = 1;

	adbMetaInit (0);

	for (i=0; i < 7; i++)
	{
		adbMetaAdd (test_many_insert[i].filename,
		            test_many_insert[i].filesize,
		            test_many_insert[i].SIG,
		            test_many_insert[i].data,
		            test_many_insert[i].datasize);
	}

	for (i=0; i < 7; i++)
	{
		unsigned char *data = (unsigned char *)"b";
		size_t datasize = 1;
		adbMetaGet (test_many_insert[i].filename,
		            test_many_insert[i].filesize,
		            test_many_insert[i].SIG,
		            &data,
		            &datasize);
		if (!data)
		{
			retval |= 1;
			fprintf (stderr, "adbmeta_basic_test8: unable to retrieve index %d\n", i);
		} else if (datasize != test_many_insert[i].datasize)
		{
			retval |= 2;
			fprintf (stderr, "adbmeta_basic_test8: index %d returned length %d instead of %d\n", i, (int)datasize, (int)test_many_insert[i].datasize);
		} else if (memcmp (data, test_many_insert[i].data, test_many_insert[i].datasize))
		{
			retval |= 4;
			fprintf (stderr, "adbmeta_basic_test8: index %d returned the wrong data content\n", i);
		}
		free (data);
	}

	{
		unsigned char *data = (unsigned char *)"a";
		size_t datasize = 1;
		adbMetaGet (test_many_insert[0].filename,
		            14,
		            test_many_insert[0].SIG,
		            &data,
		            &datasize);
		if (data)
		{
			retval |= 1;
			fprintf (stderr, "adbmeta_basic_test8: returned data for an invalid entry ID\n");
		}
		free (data);
	}

	adbMetaDirty = 0;

	adbMetaClose ();

	unlink ("/tmp/CPARCMETA.DAT");

	return retval;
}

int main(int argc, char *argv[])
{
	int retval = 0;

	fprintf (stderr, ANSI_COLOR_CYAN "Testing adbMetaInit()" ANSI_COLOR_RESET "\n");
	retval |= adbmeta_basic_test1();

	fprintf (stderr, "\n" ANSI_COLOR_CYAN "Testing adbMetaCommit()" ANSI_COLOR_RESET "\n");
	retval |= adbmeta_basic_test2();

	fprintf (stderr, "\n" ANSI_COLOR_CYAN "Testing adbMetaAdd() // simple insertion, all unique data" ANSI_COLOR_RESET "\n");
	retval |= adbmeta_basic_test3();

	fprintf (stderr, "\n" ANSI_COLOR_CYAN "Testing adbMetaAdd() // many simple insertion, all unique data" ANSI_COLOR_RESET "\n");
	retval |= adbmeta_basic_test4();

	fprintf (stderr, "\n" ANSI_COLOR_CYAN "Testing adbMetaAdd() // many simple insertion, duplicate names + sizes" ANSI_COLOR_RESET "\n");
	retval |= adbmeta_basic_test5();

	fprintf (stderr, "\n" ANSI_COLOR_CYAN "Testing adbMetaAdd() // replacing" ANSI_COLOR_RESET "\n");
	retval |= adbmeta_basic_test6();

	fprintf (stderr, "\n" ANSI_COLOR_CYAN "Testing adbMetaRemove() // removing" ANSI_COLOR_RESET "\n");
	retval |= adbmeta_basic_test7();

	fprintf (stderr, "\n" ANSI_COLOR_CYAN "Testing adbMetaGet() // fetching back" ANSI_COLOR_RESET "\n");
	retval |= adbmeta_basic_test8();

	return retval;
}
