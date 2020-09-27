#include "gendir.c"

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

static int genreldir_malloc_subtest(const char *org, const char *target, const char *expectedpatch)
{
	int result;
	char *patch = 0;

	result = genreldir_malloc(org, target, &patch);

	printf ("genreldir_malloc(\"%s%s%s\", \"%s%s%s\", expect=%s%s%s%s%s) - ",
	         ANSI_COLOR_CYAN, org, ANSI_COLOR_RESET,
	         ANSI_COLOR_CYAN, target, ANSI_COLOR_RESET,
	         expectedpatch?"\"":"", ANSI_COLOR_CYAN, expectedpatch?expectedpatch:"(null)", ANSI_COLOR_RESET, expectedpatch?"\"":"");

	if (expectedpatch)
	{
		if (result)
		{
			printf ("%sFailed - expected result, got error%s\n", ANSI_COLOR_RED, ANSI_COLOR_RESET);
			return -1;
		} else {
			int retval = strcmp(patch, expectedpatch);
			if (retval)
			{
				printf ("%sFailed, got \"%s\"%s\n", ANSI_COLOR_RED, patch, ANSI_COLOR_RESET);
			} else {
				printf ("%s OK, match%s\n", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);
			}
			free (patch);
			return retval;
		}
	} else {
		if (!result)
		{
			printf ("%sFailed - expected failure, got a result%s\n", ANSI_COLOR_RED, ANSI_COLOR_RESET);
			free (patch);
			return -1;
		} else {
			printf ("%sfunction failed as expected, OK%s\n", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);
			return 0;
		}
	}
}

static int genreldir_malloc_test (void)
{
	int retval = 0;

	printf (ANSI_COLOR_MAGENTA "Going to test genreldir_malloc()" ANSI_COLOR_RESET "\n");

	retval |= genreldir_malloc_subtest ("foo", "bar", 0);
	retval |= genreldir_malloc_subtest ("foo/", "bar/", 0);
	retval |= genreldir_malloc_subtest ("foo/bar", "bar/tmp", 0);
	retval |= genreldir_malloc_subtest ("foo/bar/", "bar/tmp/", 0);

	retval |= genreldir_malloc_subtest ("/tmp/foo/bar",   "/tmp/foo/bar",  ".");
	retval |= genreldir_malloc_subtest ("/tmp/foo/bar/",  "/tmp/foo/bar",  ".");
	retval |= genreldir_malloc_subtest ("/tmp/foo/bar/",  "/tmp/foo/bar/", ".");
	retval |= genreldir_malloc_subtest ("/tmp/foo/bar",   "/tmp/foo/bar/", ".");

	retval |= genreldir_malloc_subtest ("/",  "/",  ".");
	retval |= genreldir_malloc_subtest ("//", "/",  ".");
	retval |= genreldir_malloc_subtest ("//", "//", ".");
	retval |= genreldir_malloc_subtest ("/",  "//", ".");


	retval |= genreldir_malloc_subtest ("/foo/bar", "/bar/tmp", "/bar/tmp");
	retval |= genreldir_malloc_subtest ("/foo/bar/", "/bar/tmp", "/bar/tmp");
	retval |= genreldir_malloc_subtest ("/foo/bar/", "/bar/tmp/", "/bar/tmp");
	retval |= genreldir_malloc_subtest ("/foo/bar", "/bar/tmp/", "/bar/tmp");
	retval |= genreldir_malloc_subtest ("/foo//bar", "/bar/tmp/", "/bar/tmp");
	retval |= genreldir_malloc_subtest ("/foo/bar//", "/bar/tmp/", "/bar/tmp");
	retval |= genreldir_malloc_subtest ("//foo/bar/", "/bar/tmp/", "/bar/tmp");
	retval |= genreldir_malloc_subtest ("/foo/bar/", "/bar//tmp/", "/bar/tmp");

	retval |= genreldir_malloc_subtest ("/tmp/foo/bar",  "/tmp",  "../..");
	retval |= genreldir_malloc_subtest ("/tmp/foo/bar/", "/tmp",  "../..");
	retval |= genreldir_malloc_subtest ("/tmp/foo/bar",  "/tmp/", "../..");
	retval |= genreldir_malloc_subtest ("/tmp/foo/bar/", "/tmp/", "../..");

	retval |= genreldir_malloc_subtest ("/tmp/foo/bar/test/moo",  "/tmp",  "../../../..");

	retval |= genreldir_malloc_subtest ("/tmp/foo/bar", "/tmp/home", "../../home");
	retval |= genreldir_malloc_subtest ("/tmp/foo/bar", "/tmp/home/test", "../../home/test");

#define _10 "abcdefghij"
#define _50 _10 _10 _10 _10 _10
#define _100 _50 _50

	retval |= genreldir_malloc_subtest ("/tmp/foo/bar/test/moo", "/tmp/home/" _100 _100 "/test/" _100 _100, "../../../../home/"  _100 _100 "/test/" _100 _100);
	retval |= genreldir_malloc_subtest ("/tmp/foo/bar/test/moo/" _100 _100 "/cow/" _100 _100, "/tmp/home/", "../../../../../../../home");

	return retval;
}

static int gendir_malloc_subtest(const char *org, const char *target, const char *expectedpatch)
{
	int result;
	char *patch = 0;

	result = gendir_malloc(org, target, &patch);

	printf ("gendir_malloc(\"%s%s%s\", \"%s%s%s\", expect=%s%s%s%s%s) - ",
	         ANSI_COLOR_CYAN, org, ANSI_COLOR_RESET,
	         ANSI_COLOR_CYAN, target, ANSI_COLOR_RESET,
	         expectedpatch?"\"":"", ANSI_COLOR_CYAN, expectedpatch?expectedpatch:"(null)", ANSI_COLOR_RESET, expectedpatch?"\"":"");

	if (expectedpatch)
	{
		if (result)
		{
			printf ("%sFailed - expected result, got error%s\n", ANSI_COLOR_RED, ANSI_COLOR_RESET);
			return -1;
		} else {
			int retval = strcmp(patch, expectedpatch);
			if (retval)
			{
				printf ("%sFailed, got \"%s\"%s\n", ANSI_COLOR_RED, patch, ANSI_COLOR_RESET);
			} else {
				printf ("%s OK, match%s\n", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);
			}
			free (patch);
			return retval;
		}
	} else {
		if (!result)
		{
			printf ("%sFailed - expected failure, got a result%s\n", ANSI_COLOR_RED, ANSI_COLOR_RESET);
			free (patch);
			return -1;
		} else {
			printf ("%sfunction failed as expected, OK%s\n", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);
			return 0;
		}
	}
}

static int gendir_malloc_test (void)
{
	int retval = 0;

	printf (ANSI_COLOR_MAGENTA "Going to test gendir_malloc()" ANSI_COLOR_RESET "\n");

	retval |= gendir_malloc_subtest ("foo", "/tmp", 0);
	retval |= gendir_malloc_subtest ("foo/", "/tmp", 0);
	retval |= gendir_malloc_subtest ("foo/bar", "/tmp", 0);
	retval |= gendir_malloc_subtest ("foo/bar/", "/tmp", 0);
	retval |= gendir_malloc_subtest ("f/oo/bar/", "/tmp", 0);

	retval |= gendir_malloc_subtest ("/tmp/foo/bar", "/home/superuser", "/home/superuser");
	retval |= gendir_malloc_subtest ("/tmp/foo/bar", "./home/superuser", "/tmp/foo/bar/home/superuser");
	retval |= gendir_malloc_subtest ("/tmp/foo/bar", "./home/superuser/", "/tmp/foo/bar/home/superuser");
	retval |= gendir_malloc_subtest ("/tmp/foo/bar", "./home//superuser/", "/tmp/foo/bar/home/superuser");
	retval |= gendir_malloc_subtest ("/tmp/foo/bar", "./home///superuser/", "/tmp/foo/bar/home/superuser");
	retval |= gendir_malloc_subtest ("/tmp/foo/bar", "./home/superuser//", "/tmp/foo/bar/home/superuser");
	retval |= gendir_malloc_subtest ("/tmp/foo/bar", "./home/superuser///", "/tmp/foo/bar/home/superuser");

	retval |= gendir_malloc_subtest ("/tmp/foo/bar", "./../home/superuser", "/tmp/foo/home/superuser");
	retval |= gendir_malloc_subtest ("/tmp/foo/bar",   "../home/superuser", "/tmp/foo/home/superuser");

	retval |= gendir_malloc_subtest ("/tmp/foo/bar", "./../../home/superuser", "/tmp/home/superuser");
	retval |= gendir_malloc_subtest ("/tmp/foo/bar",   "../../home/superuser", "/tmp/home/superuser");
	retval |= gendir_malloc_subtest ("/tmp/foo/bar", "../.././home/superuser", "/tmp/home/superuser");


	retval |= gendir_malloc_subtest ("/tmp/foo/bar",   "./../../../home/superuser", "/home/superuser");
	retval |= gendir_malloc_subtest ("/tmp/foo/bar",     "../../../home/superuser", "/home/superuser");
	retval |= gendir_malloc_subtest ("/tmp/foo/bar",   ".././../../home/superuser", "/home/superuser");

	retval |= gendir_malloc_subtest ("/tmp/foo/bar",   "../../../../home/superuser", 0); /* should this fail???? */

	retval |= gendir_malloc_subtest ("/tmp/foo/bar",   "../../home/test/../superuser", "/tmp/home/superuser"); /* should this fail???? */

	return retval;
}

int main(int argc, char *argv[])
{
	int retval = 0;

	retval |= genreldir_malloc_test ();

	retval |= gendir_malloc_test ();

	return retval;
}
