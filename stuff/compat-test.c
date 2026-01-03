/* OpenCP Module Player
 * copyright (c) 2020-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Unit testing of some of the functions in stuff/compat.c
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
#include "types.h"

#undef HAVE_MEMRCHR
#define memrchr ocp_memrchr

#include "compat.h"

#include "compat.c"

#undef memrchr

#include <string.h>
#include <unistd.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

static int strcmp_safe(const char *a, const char *b)
{
	if (!a)
	{
		if (!b)
		{
			return 0;
		}
		return 1;
	} else {
		if (!b)
		{
			return 1;
		}
	}

	return strcmp (a, b);
}

static int test_splitpath41_sub (const char *src, int expecterror, const char *expectdrive, const char *expectpath, const char *expectfile, const char *expectext)
{
	char *NEWAPI_drive = 0;
	char *NEWAPI_path = 0;
	char *NEWAPI_file = 0;
	char *NEWAPI_ext = 0;

	int failed = 0;

	printf ("Testing ->%s%s%s<- (expect_error=%d)\n", ANSI_COLOR_CYAN, src, ANSI_COLOR_RESET, expecterror);

	if (splitpath4_malloc (src, &NEWAPI_drive, &NEWAPI_path, &NEWAPI_file, &NEWAPI_ext))
	{
		if (NEWAPI_drive)
		{
			printf ("%ssplitpath4_malloc gave non-null drive on error%s\n", ANSI_COLOR_RED, ANSI_COLOR_RESET);
			failed = 1;
		}
		if (NEWAPI_path)
		{
			printf ("%ssplitpath4_malloc gave non-null path on error%s\n", ANSI_COLOR_RED, ANSI_COLOR_RESET);
			failed = 1;
		}
		if (NEWAPI_file)
		{
			printf ("%ssplitpath4_malloc gave non-null file on error%s\n", ANSI_COLOR_RED, ANSI_COLOR_RESET);
			failed = 1;
		}
		if (NEWAPI_ext)
		{
			printf ("%ssplitpath4_malloc gave non-null ext on error%s\n", ANSI_COLOR_RED, ANSI_COLOR_RESET);
			failed = 1;
		}
		free (NEWAPI_drive);
		free (NEWAPI_path);
		free (NEWAPI_file);
		free (NEWAPI_ext);

		if (expecterror)
		{
			printf ("Reported error as expected\n\n");
			return failed;
		} else {
			printf ("%sfailed: unepected error%s\n\n", ANSI_COLOR_RED, ANSI_COLOR_RESET);
			return 1;
		}
	} else {
		if (expecterror)
		{
			printf ("%sfailed: did not report error%s\n\n", ANSI_COLOR_RED, ANSI_COLOR_RESET);
			free (NEWAPI_drive);
			free (NEWAPI_path);
			free (NEWAPI_file);
			free (NEWAPI_ext);
			return 1;
		}
	}

	if (strcmp_safe (expectdrive, NEWAPI_drive))
	{
		printf ("%sDRIVE failed: ->%s<- vs ->%s<-%s\n", ANSI_COLOR_RED, expectdrive, NEWAPI_drive?NEWAPI_drive:"(null)", ANSI_COLOR_RESET);
		failed = 1;
	} else {
		printf ("DRIVE ->%s%s%s<-\n", ANSI_COLOR_GREEN, NEWAPI_drive, ANSI_COLOR_RESET);
	}

	if (strcmp_safe (expectpath, NEWAPI_path))
	{
		printf ("%sPATH failed: ->%s<- vs ->%s<-%s\n", ANSI_COLOR_RED, expectpath, NEWAPI_path?NEWAPI_path:"(null)", ANSI_COLOR_RESET);
		failed = 1;
	} else {
		printf ("PATH  ->%s%s%s<-\n", ANSI_COLOR_GREEN, NEWAPI_path, ANSI_COLOR_RESET);
	}

	if (strcmp_safe (expectfile, NEWAPI_file))
	{
		printf ("%sFILE failed: ->%s<- vs ->%s<-%s\n", ANSI_COLOR_RED, expectfile, NEWAPI_file?NEWAPI_file:"(null)", ANSI_COLOR_RESET);
		failed = 1;
	} else {
		printf ("FILE  ->%s%s%s<-\n", ANSI_COLOR_GREEN, NEWAPI_file, ANSI_COLOR_RESET);
	}

	if (strcmp_safe (expectext, NEWAPI_ext))
	{
		printf ("%sEXT failed: ->%s<- vs ->%s<-%s\n", ANSI_COLOR_RED, expectext, NEWAPI_ext?NEWAPI_ext:"(null)", ANSI_COLOR_RESET);
		failed = 1;
	} else {
		printf ("EXT   ->%s%s%s<-\n", ANSI_COLOR_GREEN, NEWAPI_ext, ANSI_COLOR_RESET);
	}

	printf ("\n");

	free (NEWAPI_drive);
	free (NEWAPI_path);
	free (NEWAPI_file);
	free (NEWAPI_ext);

	return failed;
}

static int test_splitpath41 (void)
{
	int failed = 0;

	fprintf (stderr, ANSI_COLOR_MAGENTA "Going to test splitpath4_malloc() fails when it should, and that it can handle longer strings than typical _PATH_MAX and _NAME_MAX" ANSI_COLOR_RESET "\n");

	failed |= test_splitpath41_sub ("file:/", 0, "file:", "/", "", "");

	failed |= test_splitpath41_sub ("file:/test:", 0, "file:", "/", "test:", "");

	failed |= test_splitpath41_sub ("file:test:", 1, 0, 0, 0, 0); /* missing / after file: */

	failed |= test_splitpath41_sub ("file:test/foo/bar/test.mod", 1, 0, 0, 0, 0); /* missing / after file: */

	failed |= test_splitpath41_sub ("file:/test/foo:/bar/test:.mod", 0, "file:", "/test/foo:/bar/", "test:", ".mod");

	failed |= test_splitpath41_sub ("file:/test/foo:/bar/test.mod.gz", 0, "file:", "/test/foo:/bar/", "test.mod", ".gz");

#define _10 "0123456789"
#define _50 _10 _10 _10 _10 _10
#define _100 _50 _50
#define _1000 _100 _100 _100 _100 _100 _100 _100 _100 _100 _100

	failed |= test_splitpath41_sub ("_NAME_MAX_" _100 _100 _50 _10 ":/_PATH_MAX_" _1000 _1000 _1000 _1000 _100 _100 "/_NAME_MAX_" _100 _100 _50 _10 "._NAME_MAX_" _100 _100 _50 _10, 0,
	                              "_NAME_MAX_" _100 _100 _50 _10 ":",
	                              "/_PATH_MAX_" _1000 _1000 _1000 _1000 _100 _100 "/",
	                              "_NAME_MAX_" _100 _100 _50 _10,
	                              "._NAME_MAX_" _100 _100 _50 _10);

	return failed;
}

static int test_splitpath42_sub(const char *src, const char *drive, const char *path, const char *file, const char *ext)
{
	char *NEWAPI_drive = 0;
	char *NEWAPI_path = 0;
	char *NEWAPI_file = 0;
	char *NEWAPI_ext = 0;

	int failed = 0;

	printf ("Testing ->%s%s%s<- drive:%s path:%s file:%s ext:%s\n", ANSI_COLOR_CYAN, src, ANSI_COLOR_RESET, drive?drive:"(null)", path?path:"(null)", file?file:"(null)", ext?ext:"(null)");

	if (splitpath4_malloc (src, drive?&NEWAPI_drive:0, path?&NEWAPI_path:0, file?&NEWAPI_file:0, ext?&NEWAPI_ext:0))
	{
		if (NEWAPI_drive)
		{
			printf ("%ssplitpath4_malloc gave non-null drive on error%s\n", ANSI_COLOR_RED, ANSI_COLOR_RESET);
			failed = 1;
		}
		if (NEWAPI_path)
		{
			printf ("%ssplitpath4_malloc gave non-null path on error%s\n", ANSI_COLOR_RED, ANSI_COLOR_RESET);
			failed = 1;
		}
		if (NEWAPI_file)
		{
			printf ("%ssplitpath4_malloc gave non-null file on error%s\n", ANSI_COLOR_RED, ANSI_COLOR_RESET);
			failed = 1;
		}
		if (NEWAPI_ext)
		{
			printf ("%ssplitpath4_malloc gave non-null ext on error%s\n", ANSI_COLOR_RED, ANSI_COLOR_RESET);
			failed = 1;
		}
		free (NEWAPI_drive);
		free (NEWAPI_path);
		free (NEWAPI_file);
		free (NEWAPI_ext);

		printf ("%sfailed: unepected error%s\n\n", ANSI_COLOR_RED, ANSI_COLOR_RESET);
		return 1;
	}

	if (strcmp_safe (drive, NEWAPI_drive))
	{
		printf ("%sDRIVE failed: ->%s<- vs ->%s<-%s\n", ANSI_COLOR_RED, drive, NEWAPI_drive?NEWAPI_drive:"(null)", ANSI_COLOR_RESET);
		failed = 1;
	} else {
		printf ("DRIVE ->%s%s%s<-\n", ANSI_COLOR_GREEN, NEWAPI_drive?NEWAPI_drive:"(null)", ANSI_COLOR_RESET);
	}

	if (strcmp_safe (path, NEWAPI_path))
	{
		printf ("%sPATH failed: ->%s<- vs ->%s<-%s\n", ANSI_COLOR_RED, path, NEWAPI_path?NEWAPI_path:"(null)", ANSI_COLOR_RESET);
		failed = 1;
	} else {
		printf ("PATH  ->%s%s%s<-\n", ANSI_COLOR_GREEN, NEWAPI_path?NEWAPI_path:"(null)", ANSI_COLOR_RESET);
	}

	if (strcmp_safe (file, NEWAPI_file))
	{
		printf ("%sFILE failed: ->%s<- vs ->%s<-%s\n", ANSI_COLOR_RED, file, NEWAPI_file?NEWAPI_file:"(null)", ANSI_COLOR_RESET);
		failed = 1;
	} else {
		printf ("FILE  ->%s%s%s<-\n", ANSI_COLOR_GREEN, NEWAPI_file?NEWAPI_file:"(null)", ANSI_COLOR_RESET);
	}

	if (strcmp_safe (ext, NEWAPI_ext))
	{
		printf ("%sEXT failed: ->%s<- vs ->%s<-%s\n", ANSI_COLOR_RED, ext, NEWAPI_ext?NEWAPI_ext:"(null)", ANSI_COLOR_RESET);
		failed = 1;
	} else {
		printf ("EXT   ->%s%s%s<-\n", ANSI_COLOR_GREEN, NEWAPI_ext?NEWAPI_ext:"(null)", ANSI_COLOR_RESET);
	}

	printf ("\n");

	free (NEWAPI_drive);
	free (NEWAPI_path);
	free (NEWAPI_file);
	free (NEWAPI_ext);

	return failed;
}

static int test_splitpath42 (void)
{
	int failed = 0;

	int i, j, k, l;

	fprintf (stderr, ANSI_COLOR_MAGENTA "Going to test splitpath4_malloc() can combine any combination of optional parameters" ANSI_COLOR_RESET "\n");

	for (i = 0; i < 2; i++)
	{
		for (j = 0; j < 2; j++)
		{
			for (k = 0; k < 2; k++)
			{
				for (l = 0; l < 2; l++)
				{
					failed |= test_splitpath42_sub ("file:/tmp/file.s3m", i?"file:":0, j?"/tmp/":0, k?"file":0, l?".s3m":0);
				}
			}
		}
	}

	return failed;
}

int test_memrchr (const char *s, const char c, size_t n, const char *e, const char *d)
{
	char *r = ocp_memrchr (s, c, n);
	printf("%s: %s\n", d, (r != e) ? "failed" : "ok");
	return r != e;
}

int do_test_memchr (void)
{
	const char *ooo = "ooo";
	const char *ofo = "ofo";

	const char *oooo = "oooo";
	const char *foobarfoo = "foobarfoo";

	int retval = 0;
	retval |= test_memrchr ("foo", 'a', 3, 0, "memrchr(\"foo\", 'a', 3) => NULL");
	retval |= test_memrchr (ooo+1, 'o', 0, 0, "memrchr(\"ooo\" + 1, 'o', 0) => NULL");
	retval |= test_memrchr (ooo+1, 'o', 1, ooo+1, "memrchr(\"ooo\" + 1, 'o') => \"ooo\" + 1");
	retval |= test_memrchr (ofo+1, 'o', 1, 0, "memrchr(\"ofo\" + 1, 'o') => NULL");
	retval |= test_memrchr (oooo+1, 'o', 2, oooo+2, "memrchr(\"oooo\" + 1, 'o') => \"oooo\" + 2");
	retval |= test_memrchr (foobarfoo, 'r', 9, foobarfoo + 5, "memrchr(\"foobarfoo\", 'r') => \"rfoo\"");

	return retval;
}

int main(int argc, char *argv[])
{
	int retval = 0;

	retval |= test_splitpath41 ();

	retval |= test_splitpath42 ();

	retval |= do_test_memchr ();

	if (retval)
	{
		printf ("%sSomething failed%s\n", ANSI_COLOR_RED, ANSI_COLOR_RESET);
	} else {
		printf ("%sAll OK%s\n", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);
	}
	return retval;
}
