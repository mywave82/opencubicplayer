#include "config.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef _WIN32
# include <windows.h>
# include <fileapi.h>
#endif
#include "types.h"

#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "filesel/dirdb.h"
#include "filesel/download.h"
#include "filesel/filesystem.h"
#include "filesel/filesystem-drive.h"
#include "filesel/filesystem-file-dev.h"
#include "filesel/filesystem-textfile.h"
#include "stuff/err.h"
#include "stuff/file.h"
#include "stuff/framelock.h"
#include "stuff/poutput.h"

struct modland_com_fileentry_t
{
	char *name;
	uint32_t size;
	int dirindex;
};

struct modland_com_database_t
{
	char *namestrings;
	unsigned int namestrings_c;
	unsigned int namestrings_n;
	unsigned int namestrings_size;

	uint16_t year;
	uint8_t  month;
	uint8_t  day;

	unsigned int direntries_n;
	unsigned int direntries_size;
	char **direntries;

	unsigned int fileentries_n;
	unsigned int fileentries_size;
	struct modland_com_fileentry_t *fileentries;
};

struct modland_com_initialize_t
{
	int invalid_entries;
};

struct modland_com_t
{
	char *cachepath;
	char *cachepathcustom;
	char *cacheconfig;
	char *cacheconfigcustom;
	char *mirror;
	char *mirrorcustom;
	struct dmDrive *drive;
#if 0
	struct ocpdir_mem_t *root;
#else
	struct ocpdir_t *root;
#endif
	struct ocpfile_t *initialize;
	struct ocpfile_t *setup;

	struct modland_com_database_t database;

	int showrelevantdirectoriesonly;
};
struct modland_com_t modland_com;

#define MODLAND_COM_MAXDIRLENGTH 256 /* 146 is the actual need per 1st of march 2024 */

static char *modland_filename_strdup (const char *src)
{
	size_t srclen = strlen (src);
	char *retval;

	if (srclen >= 4096)
	{
		return 0;
	}

	if ((modland_com.database.namestrings_n + srclen + 1) >= modland_com.database.namestrings_size)
	{
		unsigned int i;
		char *temp = realloc (modland_com.database.namestrings, modland_com.database.namestrings_size + 65536);
		if (!temp)
		{
			return 0;
		}
		modland_com.database.namestrings_size += 65536;
		for (i = 0; i < modland_com.database.fileentries_n; i++)
		{
			modland_com.database.fileentries[i].name = temp + (modland_com.database.fileentries[i].name - modland_com.database.namestrings);
		}
		for (i = 0; i < modland_com.database.direntries_n; i++)
		{
			modland_com.database.direntries[i] = temp + (modland_com.database.direntries[i] - modland_com.database.namestrings);
		}
		modland_com.database.namestrings = temp;
	}
	retval = modland_com.database.namestrings + modland_com.database.namestrings_n;
	modland_com.database.namestrings_n += srclen + 1;
	modland_com.database.namestrings_c++;
	strcpy (retval, src);
	return retval;
}

static void modland_com_database_clear (void)
{
	free (modland_com.database.namestrings);
	modland_com.database.namestrings = 0;
	modland_com.database.namestrings_c = 0;
	modland_com.database.namestrings_n = 0;
	modland_com.database.namestrings_size = 0;

	free (modland_com.database.fileentries);
	modland_com.database.fileentries = 0;

	free (modland_com.database.direntries);
	modland_com.database.direntries = 0;

	memset (&modland_com.database, 0, sizeof (modland_com.database));
}

static int modland_com_dir_grow (void)
{
	char **tmp = realloc (modland_com.database.direntries, (modland_com.database.direntries_size + 1024) * sizeof (char *));
	if (!tmp)
	{
		return -1;
	}
	modland_com.database.direntries_size += 1024;
	modland_com.database.direntries = tmp;
	return 0;
}

#if 0
static int modland_com_find_or_add_dir (const char *dir)
{
	int i;
	for (i=0; i < modland_com.database.direntries_n; i++)
	{
		if (!strcmp (modland_com.database.direntries[i], dir))
		{
			return i;
		}
	}

	if ((modland_com.database.direntries_n >= modland_com.database.direntries_size) &&
	    modland_com_dir_grow ())
	{
		return -1;
	}

	modland_com.database.direntries [ modland_com.database.direntries_n ] = strdup (dir);
	if (!modland_com.database.direntries [ modland_com.database.direntries_n ])
	{
		return -1;
	}

	return modland_com.database.direntries_n++;
}
#endif

/* optimization, assume list is appended somewhat-sorted */
static int modland_com_last_or_new_dir (const char *dir)
{
	if (modland_com.database.direntries_n)
	{
		if (!strcmp (modland_com.database.direntries[modland_com.database.direntries_n - 1], dir))
		{
			return modland_com.database.direntries_n - 1;
		}
	}

	if ((modland_com.database.direntries_n >= modland_com.database.direntries_size) &&
	    modland_com_dir_grow ())
	{
		return -1;
	}

	modland_com.database.direntries [ modland_com.database.direntries_n ] = modland_filename_strdup (dir);
	if (!modland_com.database.direntries [ modland_com.database.direntries_n ])
	{
		return -1;
	}

	return modland_com.database.direntries_n++;
}

static int modland_com_sort_dir_helper(const void *__a, const void *__b)
{
	const unsigned int *_a = __a;
	const unsigned int *_b = __b;

	const char *a = modland_com.database.direntries[*_a];
	const char *b = modland_com.database.direntries[*_b];

	while (1)
	{
		if (*a == *b)
		{
			if (*a == 0) return 0;
			a++;
			b++;
			continue;
		}
		if (*a == '/') return 1;
		if (*b == '/') return -1;

		if (!*a) return 1;
		if (!*b) return -1;

		if (*a > *b) return 1;

		return -1;
	}
}

static int modland_com_sort_dir (void)
{
	unsigned int *sortindex;
	unsigned int *reverseindex;
	char **sortholder;

	unsigned int i;
	if (modland_com.database.direntries_n <= 1)
	{
		return 0;
	}
	sortindex = malloc (modland_com.database.direntries_n * sizeof (unsigned int));
	reverseindex = malloc (modland_com.database.direntries_n * sizeof (unsigned int));
	sortholder = malloc (modland_com.database.direntries_size * sizeof (char *));
	if ((!sortindex) || (!reverseindex) || (!sortholder))
	{
		free (sortindex);
		free (reverseindex);
		free (sortholder);
		return -1;
	}

	for (i=0; i < modland_com.database.direntries_n; i++)
	{
		sortindex[i] = i;
	}
	qsort (sortindex, modland_com.database.direntries_n, sizeof (unsigned int), modland_com_sort_dir_helper);

	for (i=0; i < modland_com.database.direntries_n; i++)
	{
		sortholder[i] = modland_com.database.direntries[sortindex[i]];
	}
	free (modland_com.database.direntries);
	modland_com.database.direntries = sortholder;
	sortholder = 0;

	for (i=0; i < modland_com.database.direntries_n; i++)
	{
		reverseindex[sortindex[i]] = i;
	}

	free (sortindex);
	sortindex = 0;

	for (i=0; i < modland_com.database.fileentries_n; i++)
	{
		modland_com.database.fileentries[i].dirindex = reverseindex[modland_com.database.fileentries[i].dirindex];
	}

	free (reverseindex);

	return 0;
}

static int modland_com_sort_file_helper(const void *_a, const void *_b)
{
	const struct modland_com_fileentry_t *a = _a;
	const struct modland_com_fileentry_t *b = _b;

	if (a->dirindex > b->dirindex) return 1;
	if (a->dirindex < b->dirindex) return -1;

	return 0;
}

static int modland_com_sort_file (void)
{
	if (modland_com.database.fileentries_n <= 1)
	{
		return 0;
	}
	qsort (modland_com.database.fileentries, modland_com.database.fileentries_n, sizeof (modland_com.database.fileentries[0]), modland_com_sort_file_helper);

	return 0;
}

static int modland_com_addparent (unsigned int offset, const int length)
{
	char *str = malloc (length + 1);
	unsigned int i;

	if (!str)
	{
		return -1;
	}
	strncpy (str, modland_com.database.direntries[offset], length);
	str[length] = 0;

	if ((modland_com.database.direntries_n >= modland_com.database.direntries_size) &&
	    modland_com_dir_grow ())
	{
		free (str);
		return -1;
	}

	memmove (&modland_com.database.direntries[offset+1], &modland_com.database.direntries[offset], (modland_com.database.direntries_n - offset) * sizeof (modland_com.database.direntries[0]));
	modland_com.database.direntries[offset] = modland_filename_strdup (str);
	free (str);
	modland_com.database.direntries_n++;

	for (i=modland_com.database.fileentries_n; i; i--)
	{
		if (modland_com.database.fileentries[i-1].dirindex < offset)
		{
			break;
		}
		modland_com.database.fileentries[i-1].dirindex++;
	}

	return 0;
}

/* ensure that all directories have a parent */
static int modland_com_check_dir_parents (void)
{
	unsigned int curr;

	if (!modland_com.database.direntries_n) /* empty database */
	{
		return 0;
	}

	for (curr=modland_com.database.direntries_n - 1; curr;)
	{
		char *last = strrchr (modland_com.database.direntries[curr], '/');
		int pos;
		if (!last) /* parent should be root, special case */
		{
			curr--;
			continue;
		}
		pos = last - modland_com.database.direntries[curr];

		if (strncmp (modland_com.database.direntries[curr], modland_com.database.direntries[curr-1], pos) ||
		    ((modland_com.database.direntries[curr-1][pos] != 0) && (modland_com.database.direntries[curr-1][pos] != '/'))) /* directory infront of this one should have same prefix, or be our parent */
		{
			if (modland_com_addparent (curr, pos)) return -1;
		} else {
			curr--;
		}
	}

	curr = 0;
	/* we are at curr==0, expect root */
	while (strlen (modland_com.database.direntries[curr]))
	{
		char *last = strrchr (modland_com.database.direntries[curr], '/');
		if (last)
		{
			if (modland_com_addparent (0, last - modland_com.database.direntries[0])) return -1;
		} else {
			if (modland_com_addparent (0, 0)) return -1;
		}
	}

	return 0;
}

/* this is faster, than checking each directory when using modland_com_last_or_new_dir() */
static void modland_com_deduplicate_dir (void)
{
	unsigned int curr;

	if (!modland_com.database.direntries_n) /* empty database */
	{
		return;
	}

	for (curr=0; curr < (modland_com.database.direntries_n - 1);)
	{
		if (!strcmp (modland_com.database.direntries[curr], modland_com.database.direntries[curr+1]))
		{
			unsigned int i;

			memmove (&modland_com.database.direntries[curr], &modland_com.database.direntries[curr+1], (modland_com.database.direntries_n - curr) * sizeof (modland_com.database.direntries[0]));
			modland_com.database.direntries_n--;
			for (i=modland_com.database.fileentries_n; i; i--)
			{
				if (modland_com.database.fileentries[i-1].dirindex < curr)
				{
					break;
				}
				modland_com.database.fileentries[i-1].dirindex--;
			}
		} else {
			curr++;
		}
	}
}

static int modland_com_sort (void)
{
	if (modland_com_sort_dir())
	{
		return -1;
	}
	if (modland_com_sort_file())
	{
		return -1;
	}

	modland_com_deduplicate_dir();

	if (modland_com_check_dir_parents ())
	{
		return -1;
	}

	return 0;
}

static int modland_com_add_data_fileentry (struct modland_com_initialize_t *s, const char *dir, const char *filename, long filesize)
{
	int dirindex;

	dirindex = modland_com_last_or_new_dir (dir);
	if (dirindex < 0)
	{
		return -1;
	}


	if (modland_com.database.fileentries_n <= modland_com.database.fileentries_size)
	{
		struct modland_com_fileentry_t *tmp = realloc (modland_com.database.fileentries, (modland_com.database.fileentries_size + 4096) * sizeof (struct modland_com_fileentry_t));
		if (!tmp)
		{
			return -1;
		}
		modland_com.database.fileentries_size += 4096;
		modland_com.database.fileentries = tmp;
	}

	modland_com.database.fileentries[modland_com.database.fileentries_n].name = modland_filename_strdup (filename);
	if (!modland_com.database.fileentries[modland_com.database.fileentries_n].name)
	{
		return -1;
	}
	modland_com.database.fileentries[modland_com.database.fileentries_n].size = filesize;
	modland_com.database.fileentries[modland_com.database.fileentries_n].dirindex = dirindex;
	modland_com.database.fileentries_n++;

	return 0;
}

static int modland_com_add_data_line (struct modland_com_initialize_t *s, const char *path, long filesize)
{
	const char *last = strrchr (path, '/');
	char dir[MODLAND_COM_MAXDIRLENGTH];

	if ((filesize <= 0)  ||
	    (path[0] == '/') || /* path starts with / */
	    (!last)          || /* no / in path, modland.com does not have files in the root-directory */
	    (!last[1])       || /* no more data after */
	    (((last - path) + 1) >= MODLAND_COM_MAXDIRLENGTH))
	{
		s->invalid_entries++;
		return 0;
	}

	strncpy (dir, path, last - path);
	dir[(last-path)] = 0;

	return modland_com_add_data_fileentry (s, dir, last + 1, filesize);
}

static char *modland_com_strdup_slash(const char *src)
{
	char *retval;
	char *e;
	size_t len;

	if (!src)
	{
		fprintf (stderr, "modland_com_strdup_slash(src): src is NULL\n");
		return 0;
	}
	e = strrchr (src, '/');
	if (e && e[1])
	{
		e = 0;
	}
	len = strlen(src) + !e + 1;
	retval = malloc (len);
	if (!retval)
	{
		fprintf (stderr, "modland_com_strdup_slash(): malloc() failed\n");
	}
	snprintf (retval, len, "%s%s", src, !e ? "/" : "");
	return retval;
}
#include "modland-com-cachedir.c"
#include "modland-com-filehandle.c"
#include "modland-com-file.c"
#include "modland-com-filedb.c"
#include "modland-com-dir.c"
#include "modland-com-initialize.c"
#include "modland-com-mirrors.c"
#include "modland-com-removecache.c"
#include "modland-com-setup.c"


static int modland_com_init (const struct configAPI_t *configAPI)
{
	modland_com.cacheconfig = strdup (configAPI->GetProfileString ("modland.com", "cachedir", "$OCPHOMEDATA/modland.com/"));
	if (!modland_com.cacheconfig)
	{
		return errAllocMem;
	}

	modland_com.cachepath = modland_com_resolve_cachedir (configAPI, modland_com.cacheconfig);
	if (!modland_com.cachepath)
	{
		return errAllocMem;
	}

	modland_com.cacheconfigcustom = strdup (configAPI->GetProfileString ("modland.com", "cachedircustom", modland_com.cacheconfig));
	if (!modland_com.cacheconfigcustom)
	{
		return errAllocMem;
	}

	modland_com.cachepathcustom = modland_com_resolve_cachedir (configAPI, modland_com.cacheconfigcustom);
	if (!modland_com.cachepathcustom)
	{
		return errAllocMem;
	}

	modland_com.showrelevantdirectoriesonly = configAPI->GetProfileBool ("modland.com", "showrelevantdirectoriesonly", 1, 1);

	modland_com.root = modland_com_init_root ();
	modland_com.drive = RegisterDrive("modland.com:", modland_com.root, modland_com.root);

	if (!modland_com.drive)
	{
		return errAllocMem;
	}

	modland_com_filedb_load (configAPI);
	fprintf (stderr, "Sort CPMDLAND.DAT data ..");
	modland_com_sort ();
	fprintf (stderr, "Done\n");

#warning if modland_com_filedb_load() fails, fails create this, and remove if download is successfull
	modland_com.initialize = dev_file_create (
		modland_com.root, /* parent-dir */
		"initialize.dev",
		"Download metadatabase from modland.com",
		"",
		0, /* token */
		0, /* Init */
		modland_com_initialize_Run,
		0, /* Close */
		0  /* Destructor */
	);

#warning make a copy in setup: too
	modland_com.setup = dev_file_create (
		modland_com.root, /* parent-dir */
		"setup.dev",
		"setup modland.com: drive",
		"",
		0, /* token */
		0, /* Init */
		modland_com_setup_Run,
		0, /* Close */
		0  /* Destructor */
	);

	{
		const char *temp = configAPI->GetProfileString ("modland.com", "mirror", "https://modland.com/");
		modland_com.mirror = modland_com_strdup_slash (temp);
		if (!modland_com.mirror)
		{
			return errAllocMem;
		}

		temp = configAPI->GetProfileString ("modland.com", "mirrorcustom", modland_com.mirror);
		modland_com.mirrorcustom = modland_com_strdup_slash (temp);
		if (!modland_com.mirrorcustom)
		{
			return errAllocMem;
		}
	}

	return errOk;
}

static void modland_com_done (void)
{
	modland_com_filedb_close ();

	modland_com_database_clear();

	if (modland_com.initialize)
	{
		modland_com.initialize->unref (modland_com.initialize);
		modland_com.initialize = 0;
	}

	if (modland_com.setup)
	{
		modland_com.setup->unref (modland_com.setup);
		modland_com.setup = 0;
	}

	if (modland_com.root)
	{
		modland_com.root->unref (modland_com.root);
		modland_com.root = 0;
	}

	if (modland_com.drive)
	{
		UnregisterDrive (modland_com.drive);
		modland_com.drive = 0;
	}

	free (modland_com.cacheconfig);
	modland_com.cacheconfig = 0;

	free (modland_com.cachepath);
	modland_com.cachepath = 0;

	free (modland_com.cacheconfigcustom);
	modland_com.cacheconfigcustom = 0;

	free (modland_com.cachepathcustom);
	modland_com.cachepathcustom = 0;

	free (modland_com.mirror);
	modland_com.mirror = 0;

	free (modland_com.mirrorcustom);
	modland_com.mirrorcustom = 0;
}

DLLEXTINFO_CORE_PREFIX struct linkinfostruct dllextinfo = {.name = "modland-com", .desc = "OpenCP virtual modland.com filebrowser (c) 2024 Stian Skjelstad", .ver = DLLVERSION, .sortindex = 60, .Init = modland_com_init, .Close = modland_com_done};
