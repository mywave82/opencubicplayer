/* OpenCP Module Player
 * copyright (c) 2022-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * Glue logic for fetching and caching data from MusicBrainz
 * online music database (CDROM).
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
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <cJSON.h>
#ifdef _WIN32
# include <windows.h>
# include <fileapi.h>
# include <winbase.h>
# include <shellapi.h>
#else
# include <sys/wait.h>
#endif
#include "types.h"
#include "boot/psetting.h"
#include "dirdb.h"
#include "filesystem.h"
#include "filesystem-drive.h"
#include "filesystem-file-dev.h"
#include "filesystem-setup.h"
#include "musicbrainz.h"
#include "pfilesel.h"
#include "stuff/compat.h"
#include "stuff/piperun.h"
#include "stuff/poutput.h"
#include "stuff/utf-8.h"

const char musicbrainzsigv1[64] = "Cubic Player MusicBrainz Data Base\x1B\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";

struct musicbrainz_queue_t;
struct musicbrainz_queue_t
{
	char discid[28+1];
	char toc[705+1];
	struct musicbrainz_queue_t *next;
};

struct musicbrainz_cacheline_t
{
	char discid[28+1];
	uint64_t lastscan;
	uint32_t size;
	char *data;
};

struct musicbrainz_t
{
	void *pipehandle;
#ifdef _WIN32
	HANDLE fdb;
#else
	int fddb;
#endif
	struct timespec lastactive;
	struct musicbrainz_cacheline_t *cache;
	int                             cachecount;
	int                             cachesize;
	int                             cachedirty;
	int                             cachedirtyfrom;
	struct musicbrainz_queue_t *active;
	struct musicbrainz_queue_t *head, *tail;
	char out [256*1024];
	char outo[      16];
	char err [  2*1024];
	char erro[      16];
	int outs;
	int errs;
};

struct musicbrainz_cacheline_sort_t
{
	int pointsat;
	char artist[MDB_COMPOSER_LEN];
	char album[MDB_COMPOSER_LEN];
}; // used by musicbrainzSetup

struct musicbrainz_t musicbrainz = {
	0,      /* pipehandle */
#ifdef _WIN32
	INVALID_HANDLE_VALUE, /* fdb */
#else
	-1,     /* fddb */
#endif
	{0, 0}, /* lastactive */
	0,      /* cache */
	0,      /* cachecount */
	0,      /* cachesize */
	0,      /* cachedirty */
	0,      /* cachedirtyfrom */
	NULL,   /* active */
	NULL,   /* head */
	NULL,   /* tail */
	{0},    /* out  - stdout buffer */
	{0},    /* outo - stdout-overflow buffer */
	{0},    /* err  - stderr buffer */
	{0},    /* erro - stderr-overflow buffer */
	0,      /* outs - stdout buffer fill */
	0       /* errs - stderr buffer fill */
};

static void musicbrainz_parse_release (cJSON *release, struct musicbrainz_database_h **result);
static const uint32_t SIZE_PRIVATE      = 0x80000000UL;
static const uint32_t SIZE_VALID        = 0x40000000UL;
static const uint32_t SIZE_FORCEREFRESH = 0x20000000UL;
static const uint32_t SIZE_MASK         = 0x000FFFFFUL;


static void musicbrainz_setup_init (void);
static void musicbrainz_setup_done (void);

static int musicbrainz_spawn (struct musicbrainz_queue_t *t)
{
	char temp[4096];

	snprintf (temp, sizeof (temp), "https://musicbrainz.org/ws/2/discid/%s?inc=recordings+artist-credits&cdstubs=no", t->discid);

	do
	{
		const char *command_line[] =
		{
#ifdef _WIN32
			"curl.exe",
#else
			"curl"
#endif
			"--max-redirs", "10",
			"--user-agent", "opencubicplayer/" PACKAGE_VERSION " ( stian.skjelstad@gmail.com )",
			"--header", "Accept: application/json",
			"--max-time", "10",
			"-L", temp,
			0
		};
		musicbrainz.outs = 0;
		musicbrainz.errs = 0;
		musicbrainz.pipehandle = ocpPipeProcess_create (command_line);
		return 0;
	} while (0);
}

void *musicbrainz_lookup_discid_init (const char *discid, const char *toc, struct musicbrainz_database_h **result)
{
	struct musicbrainz_queue_t *t;
	int i;
	struct timespec now;

	*result = 0;

	if (strlen (discid) > 28)
	{
		fprintf (stderr, "INVALID DISCID\n");
		return 0;
	}

	if (strlen (toc) > 705)
	{
		fprintf (stderr, "INVALID TOC\n");
		return 0;
	}

	/* check cache first */
	for (i=0; i < musicbrainz.cachecount; i++)
	{
		if (!strcmp (musicbrainz.cache[i].discid, discid))
		{
			int old = (musicbrainz.cache[i].lastscan + 60 * 60 * 24 * 7 * 26) < time(0); /* is record over 26 weeks old */
			int private   = !!(musicbrainz.cache[i].size & SIZE_PRIVATE);
			int valid     = !!(musicbrainz.cache[i].size & SIZE_VALID);
			uint32_t size = musicbrainz.cache[i].size & SIZE_MASK;

			if (private)
			{
				return 0;
			}

			if (musicbrainz.cache[i].size & SIZE_FORCEREFRESH)
			{
				break;
			}

			if (old || (!valid))
			{ /* it is old or not valid, rescan... */
				break;
			}

			{

				cJSON *root = cJSON_ParseWithLength (musicbrainz.cache[i].data, size);
				{
					cJSON *releases;
					if (!root)
					{
						return 0;
					}

					// we could search for error, but not finding "releases" is sufficient
					releases = cJSON_GetObjectItem (root, "releases");
					if (releases)
					{
						long releases_count = cJSON_GetArraySize (releases);
						long releases_iter;
						for (releases_iter = 0; (releases_iter < releases_count) && releases_iter == 0; releases_iter++)
						{ /* there should only be 1 release for a discid lookup */
							cJSON *release = cJSON_GetArrayItem (releases, releases_iter);
							if (cJSON_IsObject (release))
							{
								musicbrainz_parse_release (release, result);
							}
						}
					}
				}
				cJSON_Delete (root);
				return 0;
			}
		}
	}

	/* make the request */
	t = malloc (sizeof (*t));
	if (!t)
	{
		return 0;
	}
	snprintf (t->discid, sizeof (t->discid), "%s", discid);
	snprintf (t->toc, sizeof (t->toc), "%s", toc);

	/* are we busy */
	clock_gettime (CLOCK_MONOTONIC, &now);
	if (musicbrainz.active ||
	     (!(((musicbrainz.lastactive.tv_sec + 2) < now.tv_sec) ||
	        (((now.tv_sec - musicbrainz.lastactive.tv_sec) * 1000000000L + now.tv_nsec - musicbrainz.lastactive.tv_nsec) > 2000000000L))))
	{ /* two second requirement between previous request before we start the next, to avoid throtling */
		/* YEs, put in queue */
		t->next = musicbrainz.tail;
		musicbrainz.tail = t;
		if (!musicbrainz.head)
		{
			musicbrainz.head = t;
		}
		return t;
	}

	/* we are ready, get to work */

	if (musicbrainz_spawn (t))
	{
		/* spawn failed, fail */
		free (t);
		return 0;
	}
	musicbrainz.active = t;

	return t;
}

static uint32_t musicbrainz_parse_date (const char *src)
{
	uint32_t retval = 0;
	if ((!isdigit (src[0]))||
	    (!isdigit (src[1]))||
	    (!isdigit (src[2]))||
	    (!isdigit (src[3])))
	{
		return retval;
	}
	retval = atoi (src) << 16;
	if ((src[4] != '-') ||
	   ((!isdigit (src[5]))) ||
	   ((!isdigit (src[6]))))
	{
		return retval;
	}
	retval |= atoi (src + 5) << 8;
	if ((src[7] != '-') ||
	   ((!isdigit (src[8]))) ||
	   ((!isdigit (src[9]))))
	{
		return retval;
	}
	retval |= atoi (src + 8);

	return retval;
}

static void musicbrainz_parse_artists (cJSON *artists, char *target)
{
	char *dest = target;
	int len = MDB_ARTIST_LEN;

	long artists_count = cJSON_GetArraySize (artists);
	long artists_iter;
	for (artists_iter = 0; artists_iter < artists_count; artists_iter++)
	{
		cJSON *artist = cJSON_GetArrayItem (artists, artists_iter);
		if (artist && cJSON_IsObject (artist))
		{
			cJSON *name = cJSON_GetObjectItem (artist, "name");
			cJSON *joinphrase = cJSON_GetObjectItem (artist, "joinphrase");

			if (cJSON_IsString (name))
			{
				snprintf (dest, len, "%s", cJSON_GetStringValue (name));
				len -= strlen (dest);
				dest += strlen (dest);
			}
			if (cJSON_IsString (joinphrase))
			{
				snprintf (dest, len, "%s", cJSON_GetStringValue (joinphrase));
				len -= strlen (dest);
				dest += strlen (dest);
			}
		}
	}
}

static void musicbrainz_parse_tracks (cJSON *tracks, struct musicbrainz_database_h *result)
{
	long tracks_count = cJSON_GetArraySize (tracks);
	long tracks_iter;
	for (tracks_iter = 0; tracks_iter < tracks_count; tracks_iter++)
	{
		cJSON *track = cJSON_GetArrayItem (tracks, tracks_iter);
		if (cJSON_IsObject (track))
		{
			cJSON *number = cJSON_GetObjectItem (track, "number");
			cJSON *title2 = cJSON_GetObjectItem (track, "title");
			//cJSON *length = cJSON_GetObjectItem (track, "length");
			cJSON *recording = cJSON_GetObjectItem (track, "recording");
			cJSON *artists2 = cJSON_GetObjectItem (track, "artist-credit");
			long trackno = 0;

			if (cJSON_IsString (number))
			{
				trackno = atoi (cJSON_GetStringValue (number));
			}
			if ((trackno < 0) || (trackno > 99))
			{
				continue;
			}
			if (cJSON_IsString (title2))
			{
				snprintf (result->title[trackno], sizeof (result->title[trackno]), "%s", cJSON_GetStringValue (title2));
			}
#if 0
			if (cJSON_IsNumber (length))
			{
				(long)cJSON_GetNumberValue (length));
			}
#endif
			if (cJSON_IsObject (recording))
			{
				cJSON *date2 = cJSON_GetObjectItem (recording, "first-release-date");
				if (cJSON_IsString (date2))
				{
					result->date[trackno] = musicbrainz_parse_date (cJSON_GetStringValue (date2));
				}
			}
			if (cJSON_IsArray (artists2))
			{
				musicbrainz_parse_artists (artists2, result->artist[trackno]);
			}
		}
	}
}

static void musicbrainz_parse_release (cJSON *release, struct musicbrainz_database_h **result)
{
#if 0
	cJSON *id = cJSON_GetObjectItem (release, "id");
#endif
	cJSON *date = cJSON_GetObjectItem (release, "date");
	cJSON *artists1 = cJSON_GetObjectItem (release, "artist-credit");
	cJSON *title1 = cJSON_GetObjectItem (release, "title");
	cJSON *medias = cJSON_GetObjectItem (release, "media");

	*result = calloc (sizeof (**result), 1);
	if (!*result)
	{
		fprintf (stderr, "musicbrainz_parse_release(): calloc() failed\n");
		return;
	}

#warning TODO artwork
#if 0
	if (id && cJSON_IsString (title1)) // needed for artwork
	{
		fprintf (stderr, "ROOT.RELEASES[%ld].ID = %s\n", releases_iter, cJSON_GetStringValue (id));
	}
#endif
	if (cJSON_IsString (title1))
	{
		snprintf ((*result)->album, sizeof ((*result)->album), "%s", cJSON_GetStringValue (title1));
		snprintf ((*result)->title[0], sizeof ((*result)->title[0]), "%s", cJSON_GetStringValue (title1));
	}

	if (cJSON_IsString (date))
	{
		(*result)->date[0] = musicbrainz_parse_date (cJSON_GetStringValue (date));
	}
	if (cJSON_IsArray (artists1))
	{
		musicbrainz_parse_artists (artists1, (*result)->artist[0]);
	}
	if (cJSON_IsArray (medias))
	{
		long media_count = cJSON_GetArraySize (medias);
		long media_iter;
		for (media_iter = 0; (media_iter < media_count) && (media_iter==0); media_iter++)
		{ /* there should be only one media */
			cJSON *media = cJSON_GetArrayItem (medias, media_iter);
			if (cJSON_IsObject (media))
			{
				cJSON *tracks = cJSON_GetObjectItem (media, "tracks");
				if (cJSON_IsArray (tracks))
				{
					musicbrainz_parse_tracks (tracks, *result);
				}
			}
		}
	}
}

static void musicbrainz_commit_cache (const char *discid, const char *data, const uint32_t datalen, int valid)
{
	char *datac = 0;
	int i;
	if (datalen)
	{
		datac = malloc (datalen);
		if (!datac)
		{
			fprintf (stderr, "musicbrainz_commit_cache malloc failed\n");
		}
		memcpy (datac, data, datalen);
	}
	for (i=0; i < musicbrainz.cachecount; i++)
	{
		if (!strcmp (musicbrainz.cache[i].discid, discid))
		{
			if ((!valid) && (musicbrainz.cache[i].size & SIZE_VALID))
			{ /* keep the old record */
				free (datac);
			}
			break;
		}
	}
	if ((i == musicbrainz.cachecount) && (musicbrainz.cachecount >= musicbrainz.cachesize))
	{
		void *tmp = realloc (musicbrainz.cache, (musicbrainz.cachesize + 16) * sizeof (musicbrainz.cache[0]));
		if (!tmp)
		{
			fprintf (stderr, "musicbrainz_commit_cache realloc() failed\n");
			free (datac);
			return;
		}
		musicbrainz.cache = tmp;
		musicbrainz.cachesize += 16;
	}
	if (i < musicbrainz.cachecount)
	{
		free (musicbrainz.cache[i].data);
	} else {
		musicbrainz.cachecount++;
	}
	memcpy (musicbrainz.cache[i].discid, discid, 28);
	musicbrainz.cache[i].discid[28] = 0;
	musicbrainz.cache[i].data = datac;
	musicbrainz.cache[i].size = datalen | (valid?SIZE_VALID:0);
	musicbrainz.cache[i].lastscan = time (0);
	musicbrainz.cachedirty = 1;
	if (musicbrainz.cachedirtyfrom > i)
	{
		musicbrainz.cachedirtyfrom = i;
	}
}

static void musicbrainz_finalize (int retval, struct musicbrainz_database_h **result)
{
	*result = 0;

	if (retval)
	{
		/* execute curl failed, error code in RETVAL */
#if 0
		write (2, musicbrainz.err, musicbrainz.errs);
#endif
		musicbrainz_commit_cache (musicbrainz.active->discid, musicbrainz.active->toc, strlen (musicbrainz.active->toc), 0);

		return;
	}

	{
		cJSON *root = cJSON_ParseWithLength (musicbrainz.out, musicbrainz.outs);
		cJSON *releases;
		if (!root)
		{
			fprintf (stderr, "cJSON_ParseWithLength() failed to parse. Data not valid or truncated\n");
			return;
		}

		// we could search for error, but not finding "releases" is sufficient
		releases = cJSON_GetObjectItem (root, "releases");
		if (releases)
		{
			long releases_count = cJSON_GetArraySize (releases);
			long releases_iter;
			for (releases_iter = 0; (releases_iter < releases_count) && releases_iter == 0; releases_iter++)
			{ /* there should only be 1 release for a discid lookup */
				cJSON *release = cJSON_GetArrayItem (releases, releases_iter);
				if (cJSON_IsObject (release))
				{
					musicbrainz_parse_release (release, result);
				}
			}

			musicbrainz_commit_cache (musicbrainz.active->discid, musicbrainz.out, musicbrainz.outs, 1);
		} else {
			musicbrainz_commit_cache (musicbrainz.active->discid, musicbrainz.active->toc, strlen (musicbrainz.active->toc), 0);
		}

		cJSON_Delete (root);
	}
}

int musicbrainz_lookup_discid_iterate (void *token, struct musicbrainz_database_h **result)
{
	/* are we from the queue, if so, attempt to make us active */
	if (musicbrainz.active != token)
	{
		struct timespec now;
		struct musicbrainz_queue_t *t;

		if (musicbrainz.active)
		{
			return 1;
		}
		if (musicbrainz.head != token)
		{
			return 1;
		}

		/* are we busy */
		clock_gettime (CLOCK_MONOTONIC, &now);
		if ((!(((musicbrainz.lastactive.tv_sec + 2) < now.tv_sec) ||
	               (((now.tv_sec - musicbrainz.lastactive.tv_sec) * 1000000000L + now.tv_nsec - musicbrainz.lastactive.tv_nsec) > 2000000000L))))
		{
			return 1;
		}

		t = musicbrainz.head;
		musicbrainz.head = musicbrainz.head->next;
		if (!musicbrainz.head)
		{
			musicbrainz.tail = 0;
		}

		if (musicbrainz_spawn (t))
		{
			/* spawn failed, fail */
			free (t);
			return 0;
		}
		musicbrainz.active = t;
		return 1;
	}

	if (musicbrainz.pipehandle)
	{
		int a, b, retval;

		if (musicbrainz.outs != sizeof (musicbrainz.out))
		{
			a = ocpPipeProcess_read_stdout (musicbrainz.pipehandle, musicbrainz.out, sizeof (musicbrainz.out) - musicbrainz.outs);
			if (a > 0) musicbrainz.outs += a;
		} else {
			a = ocpPipeProcess_read_stdout (musicbrainz.pipehandle, musicbrainz.outo, sizeof (musicbrainz.outo));
		}

		if (musicbrainz.errs != sizeof (musicbrainz.err))
		{
			b = ocpPipeProcess_read_stderr (musicbrainz.pipehandle, musicbrainz.err, sizeof (musicbrainz.err) - musicbrainz.errs);
			if (b > 0) musicbrainz.errs += b;
		} else {
			b = ocpPipeProcess_read_stderr (musicbrainz.pipehandle, musicbrainz.erro, sizeof (musicbrainz.erro));
		}

		if ((a >= 0) || (b >= 0))
		{
			return 1;
		}

		retval = ocpPipeProcess_destroy (musicbrainz.pipehandle);
		musicbrainz.pipehandle = 0;

		clock_gettime (CLOCK_MONOTONIC, &musicbrainz.lastactive);

		musicbrainz_finalize (retval, result);

		free (musicbrainz.active);
		musicbrainz.active = 0;
		return 0;
	}
	fprintf (stderr, "musicbrainz_lookup_discid_iterate() called without a pipe active\n");
	return 0;
}

void musicbrainz_lookup_discid_cancel (void *token)
{
	struct musicbrainz_queue_t **head, *iter, *prev;

	if (!token)
	{
		return;
	}
	if (musicbrainz.active == token)
	{
		assert (musicbrainz.pipehandle);

		ocpPipeProcess_terminate (musicbrainz.pipehandle);

		do
		{
			int a, b;

			if (musicbrainz.outs != sizeof (musicbrainz.out))
			{
				a = ocpPipeProcess_read_stdout (musicbrainz.pipehandle, musicbrainz.out, sizeof (musicbrainz.out) - musicbrainz.outs);
				if (a > 0) musicbrainz.outs += a;
			} else {
				a = ocpPipeProcess_read_stdout (musicbrainz.pipehandle, musicbrainz.outo, sizeof (musicbrainz.outo));
			}

			if (musicbrainz.errs != sizeof (musicbrainz.err))
			{
				b = ocpPipeProcess_read_stderr (musicbrainz.pipehandle, musicbrainz.err, sizeof (musicbrainz.err) - musicbrainz.errs);
				if (b > 0) musicbrainz.errs += b;
			} else {
				b = ocpPipeProcess_read_stderr (musicbrainz.pipehandle, musicbrainz.erro, sizeof (musicbrainz.erro));
			}

			if ((a >= 0) || (b >= 0))
			{
				usleep(10000);
				continue;
			}
		} while (0);

		ocpPipeProcess_destroy (musicbrainz.pipehandle);
		musicbrainz.pipehandle = 0;

		clock_gettime (CLOCK_MONOTONIC, &musicbrainz.lastactive);

		free (musicbrainz.active);
		musicbrainz.active = 0;

		return;
	}

	head = &musicbrainz.head;
	iter = musicbrainz.head;
	prev = 0;
	while (iter)
	{
		if (iter == token)
		{
			if (musicbrainz.tail == token)
			{
				musicbrainz.tail = prev;
			}
			*head = iter->next;
			free (iter);
			return;
		}
		prev = iter;
		head = &iter->next;
		iter = iter->next;
	}
}

int musicbrainz_init (const struct configAPI_t *configAPI)
{
	char *path;
	char header[64];

#ifdef _WIN32
	if (musicbrainz.fdb != INVALID_HANDLE_VALUE)
#else
	if (musicbrainz.fddb >= 0)
#endif
	{
		fprintf (stderr, "musicbrainz already initialzied\n");
		return 0;
	}
	musicbrainz_setup_init ();

	path = malloc (strlen (configAPI->DataHomePath) + strlen ("CPMUSBRN.DAT") + 1);
	sprintf (path, "%sCPMUSBRN.DAT", configAPI->DataHomePath);
	fprintf (stderr, "Loading %s .. ", path);

#ifdef _WIN32
	musicbrainz.fdb = CreateFile (path,                         /* lpFileName */
	                              GENERIC_READ | GENERIC_WRITE, /* dwDesiredAccess */
	                              0,                            /* dwShareMode (exclusive access) */
	                              0,                            /* lpSecurityAttributes */
	                              OPEN_ALWAYS,                  /* dwCreationDisposition */
	                              FILE_ATTRIBUTE_NORMAL,        /* dwFlagsAndAttributes */
	                              0);                           /* hTemplateFile */
	if (musicbrainz.fdb == INVALID_HANDLE_VALUE)
	{
		char *lpMsgBuf = NULL;
		if (FormatMessage (
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM     |
			FORMAT_MESSAGE_IGNORE_INSERTS,             /* dwFlags */
			NULL,                                      /* lpSource */
			GetLastError(),                            /* dwMessageId */
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), /* dwLanguageId */
			(LPSTR) &lpMsgBuf,                         /* lpBuffer */
			0,                                         /* nSize */
			NULL                                       /* Arguments */
		))
		{
			fprintf (stderr, "CreateFile(%s): %s\n", path, lpMsgBuf);
			LocalFree (lpMsgBuf);
		}
		free (path);
		return 0;
	}
#else
	musicbrainz.fddb = open (path, O_RDWR | O_CREAT, S_IREAD|S_IWRITE);
	if (musicbrainz.fddb < 0)
	{
		fprintf (stderr, "open(%s): %s\n", path, strerror (errno));
		free (path);
		return 0;
	}
#endif
	free (path); path = 0;

#ifndef _WIN32
	if (flock (musicbrainz.fddb, LOCK_EX | LOCK_NB))
	{
		fprintf (stderr, "Failed to lock the file (more than one instance?)\n");
		return 0;
	}
#endif

#ifdef _WIN32
	{
		DWORD NumberOfBytesRead = 0;
		BOOL Result;
		Result = ReadFile (musicbrainz.fdb, &header, sizeof (header), &NumberOfBytesRead, 0);
		if ((!Result) || (NumberOfBytesRead != sizeof (header)))
		{
			fprintf (stderr, "Empty database\n");
			return 1;
		}
	}

#else
	if (read (musicbrainz.fddb, &header, sizeof (header)) != sizeof (header))
	{
		fprintf (stderr, "Empty database\n");
		return 1;
	}
#endif

	if (memcmp (header, musicbrainzsigv1, sizeof (musicbrainzsigv1)))
	{
		fprintf (stderr, "Old header - discard data\n");
		return 1;
	}

	while (1)
	{
		char temp[28+8+4];
#ifdef _WIN32
		DWORD NumberOfBytesRead = 0;
		BOOL Result;
		Result = ReadFile (musicbrainz.fdb, temp, 28 + 8 + 4, &NumberOfBytesRead, 0);
		if ((!Result) || (NumberOfBytesRead != (28 + 8 + 4)))
#else
		if (read (musicbrainz.fddb, temp, (28 + 8 + 4)) != (28 + 8 + 4))
#endif
		{
			break;
		}
		if (musicbrainz.cachecount >= musicbrainz.cachesize)
		{
			void *tmp = realloc (musicbrainz.cache, (musicbrainz.cachesize + 16) * sizeof (musicbrainz.cache[0]));
			if (!tmp)
			{
				fprintf (stderr, "musicbrainz_init: realloc() failed\n");
				break;
			}
			musicbrainz.cache = tmp;
			musicbrainz.cachesize += 16;
		}
		memcpy (musicbrainz.cache[musicbrainz.cachecount].discid, temp, 28);
		memcpy (&musicbrainz.cache[musicbrainz.cachecount].lastscan, temp + 28, 8);
		memcpy (&musicbrainz.cache[musicbrainz.cachecount].size, temp + 28 + 8, 4);
		musicbrainz.cache[musicbrainz.cachecount].discid[28] = 0;
		musicbrainz.cache[musicbrainz.cachecount].lastscan = uint64_little (musicbrainz.cache[musicbrainz.cachecount].lastscan);
		musicbrainz.cache[musicbrainz.cachecount].size = uint32_little (musicbrainz.cache[musicbrainz.cachecount].size);
		if (musicbrainz.cache[musicbrainz.cachecount].size)
		{
			musicbrainz.cache[musicbrainz.cachecount].data = malloc (musicbrainz.cache[musicbrainz.cachecount].size & SIZE_MASK);
			if (!musicbrainz.cache[musicbrainz.cachecount].data)
			{
				fprintf (stderr, "musicbrainz_init: malloc() failed\n");
				break;
			}
#ifdef _WIN32
			NumberOfBytesRead = 0;
			Result = ReadFile (musicbrainz.fdb, musicbrainz.cache[musicbrainz.cachecount].data, musicbrainz.cache[musicbrainz.cachecount].size & SIZE_MASK, &NumberOfBytesRead, 0);
			if ((!Result) || (NumberOfBytesRead != (musicbrainz.cache[musicbrainz.cachecount].size & SIZE_MASK)))
#else
			if (read (musicbrainz.fddb, musicbrainz.cache[musicbrainz.cachecount].data, (musicbrainz.cache[musicbrainz.cachecount].size & SIZE_MASK)) != (musicbrainz.cache[musicbrainz.cachecount].size & SIZE_MASK))
#endif
			{
				free (musicbrainz.cache[musicbrainz.cachecount].data);
				musicbrainz.cache[musicbrainz.cachecount].data = 0;
				fprintf (stderr, "Truncated entry\n");
				break;
			}
		}
		musicbrainz.cachecount++;
	}
	fprintf (stderr, "Done\n");
	return 1;
}

void musicbrainz_done (void)
{
	uint64_t pos;
	int i;
#ifdef _WIN32
	LARGE_INTEGER newpos;
	if (musicbrainz.fdb != INVALID_HANDLE_VALUE)
#else
	if (musicbrainz.fddb < 0)
#endif
	{
		goto done;
	}

	musicbrainz_setup_done ();

	if (!musicbrainz.cachedirty)
	{
		goto done;
	}
	if (!musicbrainz.cachedirtyfrom)
	{
#ifdef _WIN32
		DWORD NumberOfBytesWritten = 0;
		BOOL Result;

		newpos.QuadPart = 0;
		SetFilePointerEx (musicbrainz.fdb, newpos, 0, FILE_BEGIN);

		Result = WriteFile (musicbrainz.fdb, musicbrainzsigv1, 64, &NumberOfBytesWritten, 0);
		if ((!Result) || (NumberOfBytesWritten != 64))
		{
			fprintf (stderr, "musicbrainz_done: write #1 failed\n");
			goto done;
		}
#else
		lseek (musicbrainz.fddb, 0, SEEK_SET);

		while (1)
		{
			if (write (musicbrainz.fddb, musicbrainzsigv1, 64) != 64)
			{
				if ((errno != EAGAIN) && (errno != EINTR))
				{
					fprintf (stderr, "musicbrainz_done: write #1 failed\n");
					goto done;
				}
			} else {
				break;
			}
		}
#endif
	}
	pos = 64;
	for (i=0; i < musicbrainz.cachedirtyfrom; i++)
	{
		pos += 28 + 8 + 4;
		pos += musicbrainz.cache[i].size & SIZE_MASK;
	}
#ifdef _WIN32
	newpos.QuadPart = pos;
	SetFilePointerEx (musicbrainz.fdb, newpos, 0, FILE_BEGIN);
#else
	lseek (musicbrainz.fddb, pos, SEEK_SET);
#endif
	for (; i < musicbrainz.cachecount; i++)
	{
		char temp[28+8+4];
		uint64_t lastscan = uint64_little (musicbrainz.cache[i].lastscan);
		uint32_t size = uint32_little (musicbrainz.cache[i].size);
		memcpy (temp, musicbrainz.cache[i].discid, 28);
		memcpy (temp + 28, &lastscan, 8);
		memcpy (temp + 28 + 8, &size, 4);
		while (1)
		{
#ifdef _WIN32
			DWORD NumberOfBytesWritten = 0;
			BOOL Result;
			Result = WriteFile (musicbrainz.fdb, temp, 28 + 8 + 4, &NumberOfBytesWritten, 0);
			if ((!Result) || (NumberOfBytesWritten != (28 + 8 + 4)))
			{
				fprintf (stderr, "musicbrainz_done: write #2 failed\n");
				goto done;
			}
			break;
#else
			if (write (musicbrainz.fddb, temp, 28 + 8 + 4) != (28 + 8 + 4))
			{
				if ((errno != EAGAIN) && (errno != EINTR))
				{
					fprintf (stderr, "musicbrainz_done: write #2 failed\n");
					goto done;
				}
			} else {
				break;
			}
#endif
		}
		while (1)
		{
#ifdef _WIN32
			DWORD NumberOfBytesWritten = 0;
			BOOL Result;
			Result = WriteFile (musicbrainz.fdb, musicbrainz.cache[i].data, (musicbrainz.cache[i].size & SIZE_MASK), &NumberOfBytesWritten, 0);
			if ((!Result) || (NumberOfBytesWritten != (musicbrainz.cache[i].size & SIZE_MASK)))
			{
				fprintf (stderr, "musicbrainz_done: write 32 failed\n");
				goto done;
			}
			break;
#else
			if (write (musicbrainz.fddb, musicbrainz.cache[i].data, (musicbrainz.cache[i].size & SIZE_MASK)) != (musicbrainz.cache[i].size & SIZE_MASK))
			{
				if ((errno != EAGAIN) && (errno != EINTR))
				{
					fprintf (stderr, "musicbrainz_done: write #3 failed\n");
					goto done;
				}
			} else {
				break;
			}
#endif
		}
		pos += 28 + 8 + 4;
		pos += (musicbrainz.cache[i].size & SIZE_MASK);
	}
#ifdef _WIN32
	SetEndOfFile (musicbrainz.fdb);
#else
	if (ftruncate (musicbrainz.fddb, pos))
	{
		perror ("ftruncate() failed");
	}
#endif
done:
	for (i=0; i < musicbrainz.cachecount; i++)
	{
		free (musicbrainz.cache[i].data);
	}
	free (musicbrainz.cache);
#ifdef _WIN32
	CloseHandle (musicbrainz.fdb);
	musicbrainz.fdb = INVALID_HANDLE_VALUE;
#else
	close (musicbrainz.fddb);
	musicbrainz.fddb = -1;
#endif
	musicbrainz.cache = 0;
	musicbrainz.cachesize = 0;
	musicbrainz.cachecount = 0;
	musicbrainz.cachedirty = 0;
	musicbrainz.cachedirtyfrom = 0;
}

void musicbrainz_database_h_free (struct musicbrainz_database_h *e)
{
	free (e);
}

/****************************** setup/musicbrain.dev ******************************/

static struct ocpfile_t      *musicbrainzsetup; // needs to overlay an dialog above filebrowser, and after that the file is "finished"   Special case of DEVv

static void musicbrainzSetupRun  (void **token, const struct DevInterfaceAPI_t *API);

static void musicbrainz_setup_init (void)
{
	musicbrainzsetup = dev_file_create (
		dmSetup->basedir,
		"musicbrainz.dev",
		"MusicBrainz Cache DataBase",
		"",
		0, /* token */
		0, /* Init */
		musicbrainzSetupRun,
		0, /* Close */
		0  /* Destructor */
	);

	filesystem_setup_register_file (musicbrainzsetup);
}

static void musicbrainz_setup_done (void)
{
	if (musicbrainzsetup)
	{
		filesystem_setup_unregister_file (musicbrainzsetup);
		musicbrainzsetup->unref (musicbrainzsetup);
		musicbrainzsetup = 0;
	}
}

static void musicbrainzSetupDialogDraw (struct musicbrainz_cacheline_sort_t *entry, int epos)
{
	int mlWidth = 55;
	int mlHeight = 7;
	int mlTop = (plScrHeight - mlHeight) / 2;
	int mlLeft = (plScrWidth - mlWidth) / 2;

	display_nprintf (mlTop + 0, mlLeft, 0x17, mlWidth, "\xda%*C\xc4\xbf", mlWidth - 2); /* +---------+ */
	display_nprintf (mlTop + 1, mlLeft, 0x17, mlWidth, "\xb3%*C \xb3", mlWidth - 2);    /* |         | */

	displaystr  (mlTop + 2, mlLeft,               0x17, "\xb3  ", 3);
	if (musicbrainz.cache[entry->pointsat].size & SIZE_PRIVATE)
	{
		displaystr  (mlTop + 2, mlLeft +  3, (epos==0)?0x2e:0x17, "[Unmark as private]", 19);
	} else {
		displaystr  (mlTop + 2, mlLeft +  3, (epos==0)?0x2e:0x17, "[Mark as private]",   17);
		displaychr  (mlTop + 2, mlLeft + 20, 0x17, ' ',                   2);
	}
	displaychr  (mlTop + 2, mlLeft + 22,         0x17, ' ',                   7);
	if (musicbrainz.cache[entry->pointsat].size & SIZE_PRIVATE)
	{
		displaychr  (mlTop + 2, mlLeft + 29, 0x17, ' ',                   23);
	} else {//(musicbrainz.cache[entry->pointsat].size & SIZE_UNKNOWN)
		displaystr  (mlTop + 2, mlLeft + 29, (epos==1)?0x2e:0x17, "[Refresh]",  9);
		displaychr  (mlTop + 2, mlLeft + 38, 0x17, ' ',         14);
	}
	displaychr  (mlTop + 2, mlLeft + 52,          0x17, ' ',    mlWidth - 52 - 1);
	displaychr  (mlTop + 2, mlLeft + mlWidth - 1, 0x17, '\xb3', 1);

	display_nprintf (mlTop + 3, mlLeft, 0x17, mlWidth, "\xb3%*C \xb3", mlWidth - 2);    /* |         | */

	displaychr  (mlTop + 4, mlLeft,               0x17, '\xb3', 1);
	displaychr  (mlTop + 4, mlLeft + 1,           0x17, ' ',    2);
	displaystr  (mlTop + 4, mlLeft + 3,           (epos==2)?0x2e:0x17, "[Delete entry]", 14);
	displaychr  (mlTop + 4, mlLeft + 17,          0x17, ' ',    12);
	if ((!(musicbrainz.cache[entry->pointsat].size & SIZE_PRIVATE)) && (!(musicbrainz.cache[entry->pointsat].size & SIZE_VALID)))
	{
		displaystr  (mlTop + 4, mlLeft + 29,  (epos==3)?0x2e:0x17, "[Submit to MusicBrainz]", 23);
		displaychr  (mlTop + 4, mlLeft + 52,  0x17, ' ',    mlWidth - 1 - 52);
	} else {
		displaychr  (mlTop + 4, mlLeft + 29,  0x17, ' ',    mlWidth - 1 - 29);
	}
	displaychr  (mlTop + 4, mlLeft + mlWidth - 1, 0x17, '\xb3', 1);

	display_nprintf (mlTop + 5, mlLeft, 0x17, mlWidth, "\xb3%*C \xb3",     mlWidth - 2); /* |         | */
	display_nprintf (mlTop + 6, mlLeft, 0x17, mlWidth, "\xc0%*C\xc4\xd9", mlWidth - 2); /* +---------+ */
}

static void musicbrainzSetupDraw (const char *title, int dsel, struct musicbrainz_cacheline_sort_t *sorted)
{
	unsigned int mlHeight;
	unsigned int mlTop;
	unsigned int mlLeft;
	unsigned int mlWidth;

	unsigned int i, skip, half, dot;

	const unsigned int DEFAULT_HORIZONTAL_MARGIN = 5;
	const unsigned int MIN_HEIGHT = 20;
	const unsigned int MIN_WIDTH = 72;
	const unsigned int LINES_NOT_AVAILABLE = 8;

	int albumwidth;
	int artistwidth;

	/* SETUP the framesize */
	mlHeight = plScrHeight - MIN_HEIGHT;
	if (mlHeight < MIN_HEIGHT)
	{
		mlHeight = MIN_HEIGHT;
	}
	mlTop = (plScrHeight - mlHeight) / 2;

	mlLeft = DEFAULT_HORIZONTAL_MARGIN;
	mlWidth = plScrWidth - (DEFAULT_HORIZONTAL_MARGIN * 2);
	if (mlWidth < MIN_WIDTH)
	{
		mlWidth += (MIN_WIDTH - mlWidth + 1) & ~1;
		mlLeft -= (MIN_WIDTH - mlWidth + 1) >> 1;
	}
	half = (mlHeight - LINES_NOT_AVAILABLE) / 2;
	if (musicbrainz.cachecount <= mlHeight - LINES_NOT_AVAILABLE)
	{ /* all entries can fit */
		skip = 0;
		dot = -1;
	} else if (dsel < half)
	{ /* we are in the top part */
		skip = 0;
		dot = 0;
	} else if (dsel >= (musicbrainz.cachecount - half))
	{ /* we are at the bottom part */
		skip = musicbrainz.cachecount - (mlHeight - LINES_NOT_AVAILABLE);
		dot = mlHeight - LINES_NOT_AVAILABLE;
	} else {
		skip = dsel - half;
		dot = skip * (mlHeight - LINES_NOT_AVAILABLE) / (musicbrainz.cachecount - (mlHeight - LINES_NOT_AVAILABLE));
	}

	{
			int CacheLen = strlen (title);
			int Skip = (mlWidth - CacheLen - 2) / 2;
		display_nprintf (mlTop, mlLeft, 0x09, mlWidth, "\xda%*C\xc4 %s %*C\xc4\xbf", Skip - 1, title, mlWidth - Skip - 3 - CacheLen); /* +----- title ------+ */
	}

	display_nprintf (mlTop + 1, mlLeft, 0x09, mlWidth, "\xb3%0.7o Use arrow keys and %0.15o<ENTER>%0.7o to navigate. %0.15o<ESC>%0.7o to close.%*C %0.9o\xb3", mlWidth - 58);

	display_nprintf (mlTop + 2,  mlLeft, 0x09, mlWidth, "\xc3%*C\xc4\xb4", mlWidth - 2); /* |            | */

	if (mlWidth < (1 + 1 + 27 + 2 + 10 + 32 + 2 + 32 + 1 + 1)) /* borders + date + "private" + album32 + artist32 */
	{
		albumwidth = mlWidth - (1 + 1 + 27 + 2 /* + albumwidth */ + 1 + 1);
		artistwidth = 0;
	} else {
		albumwidth = ((mlWidth - (1 + 1 + 27 + 2 + 9 + 1 /* + albumwidth */+ 1 + 1)) / 2) + 10;
		if (albumwidth > (9 + 1 + 64))
		{
			albumwidth = 9 + 1 + 64;
		}
		artistwidth = mlWidth - (1 + 1 + 27 + 2 + albumwidth + 2 /*+ artistwidth*/ + 1 + 1);
	}

	for (i = 3; i < (mlHeight-5); i++)
	{
		int index = i - 3 + skip;

		displaychr  (mlTop + i, mlLeft, 0x09, '\xb3', 1);

		assert (index >= 0);

		if (index >= musicbrainz.cachecount)
		{
			if (index == 0)
			{
				displaystr (mlTop + i, mlLeft + 1, 0x03, " No entries in the database", mlWidth - 2);
			} else {
				displayvoid (mlTop + i, mlLeft + 1, mlWidth - 2);
			}
		} else {
			char timebuffer[28];
			struct tm thetime;
			time_t inputtime;

			inputtime = musicbrainz.cache[sorted[index].pointsat].lastscan;
			if (!localtime_r (&inputtime, &thetime))
			{
				memset (&thetime, 0, sizeof (thetime));
			}
			strftime(timebuffer, sizeof (timebuffer), "%d.%m.%Y %H:%M %z(%Z)", &thetime);

			displaychr (mlTop + i, mlLeft + 1, (dsel==index)?0x87:0x07, ' ', 1);
			displaystr (mlTop + i, mlLeft + 2, (dsel==index)?0x87:0x07, timebuffer, 29); /* includes two exrta space */
			if (musicbrainz.cache[sorted[index].pointsat].size & SIZE_VALID)
			{
				if (musicbrainz.cache[sorted[index].pointsat].size & SIZE_PRIVATE)
				{
					displaystr (mlTop + i, mlLeft + 31, (dsel==index)?0x84:0x04, "(private)", 9);
					displaychr (mlTop + i, mlLeft + 40, (dsel==index)?0x8f:0x07, ' ', 1);
					displaystr (mlTop + i, mlLeft + 41, (dsel==index)?0x8f:0x07, sorted[index].album, albumwidth - 10);
				} else {
					displaystr_utf8 (mlTop + i, mlLeft + 31, (dsel==index)?0x8f:0x07, sorted[index].album, albumwidth);
				}
			} else {
				if (musicbrainz.cache[sorted[index].pointsat].size & SIZE_PRIVATE)
				{
					displaystr (mlTop + i, mlLeft + 31, (dsel==index)?0x84:0x04, "(private)", 9);
					displaychr (mlTop + i, mlLeft + 40, (dsel==index)?0x8f:0x07, ' ', 1);
					displaystr (mlTop + i, mlLeft + 41, (dsel==index)?0x8a:0x0a, "Unknown disc", albumwidth - 10);
				} else {
					displaystr (mlTop + i, mlLeft + 31, (dsel==index)?0x8a:0x0a, "Unknown disc", albumwidth);
				}
			}
			if (artistwidth)
			{
				displaychr (mlTop + i, mlLeft + 31 + albumwidth, (dsel==index)?0x8f:0x0f, ' ', 2);
				displaystr_utf8 (mlTop + i, mlLeft + 33 + albumwidth, (dsel==index)?0x8f:0x07, sorted[index].artist, artistwidth);
			}
		}
		displaychr (mlTop + i, mlLeft + mlWidth - 2, (dsel==index)?0x8f:0x07, ' ', 1);
		displaychr (mlTop + i, mlLeft + mlWidth - 1, 0x09, ((i-3) == dot) ? '\xdd':'\xb3', 1);
	}

	display_nprintf (mlTop + mlHeight - 5,  mlLeft, 0x09, mlWidth, "\xc3%*C\xc4\xb4", mlWidth - 2); /* +---------------+ */

	display_nprintf (mlTop + mlHeight - 4, mlLeft, 0x09, mlWidth, "\xb3%0.7o %.*s%0.9o \xb3", mlWidth - 4, musicbrainz.cachecount ? musicbrainz.cache[sorted[dsel].pointsat].discid : "");
	display_nprintf (mlTop + mlHeight - 3, mlLeft, 0x09, mlWidth, "\xb3%0.7o %.*S%0.9o \xb3", mlWidth - 4, musicbrainz.cachecount ? sorted[dsel].album : "");
	display_nprintf (mlTop + mlHeight - 2, mlLeft, 0x09, mlWidth, "\xb3%0.7o %.*S%0.9o \xb3", mlWidth - 4, musicbrainz.cachecount ? sorted[dsel].artist : "");
	display_nprintf (mlTop + mlHeight - 1,  mlLeft, 0x09, mlWidth, "\xc0%*C\xc4\xd9", mlWidth - 2); /* +---------------+ */
}

static int sortedcompare (const void *a, const void *b)
{
	const struct musicbrainz_cacheline_sort_t *c = a;
	const struct musicbrainz_cacheline_sort_t *d = b;

	int valid_c = !!(musicbrainz.cache[c->pointsat].size & SIZE_VALID);
	int valid_d = !!(musicbrainz.cache[d->pointsat].size & SIZE_VALID);

	if (!valid_c)
	{
		if (valid_d) return 1;
	} else {
		int res;
		if (!valid_d) return -1;
		res = strcmp (c->album, d->album);
		if (res > 0)
		{
			return 1;
		} else if (res < 0)
		{
			return -1;
		}
		res = strcmp (c->artist, d->artist);
		if (res > 0)
		{
			return 1;
		} else if (res < 0)
		{
			return -1;
		}
	}
	return (musicbrainz.cache[c->pointsat].lastscan - musicbrainz.cache[d->pointsat].lastscan);

	return 1;
}

static struct musicbrainz_cacheline_sort_t *musicbrainz_create_sort(void)
{
	int i;
	struct musicbrainz_cacheline_sort_t *sorted = 0;
	if (musicbrainz.cachecount)
	{
		sorted = malloc (musicbrainz.cachecount * sizeof (sorted[0]));
		if (!sorted)
		{
			fprintf (stderr, "musicbrainzSetupRun: malloc failed\n");
			return 0;
		}
		for (i=0; i < musicbrainz.cachecount; i++)
		{
			sorted[i].pointsat = i;
			sorted[i].album[0] = 0;
			sorted[i].artist[0] = 0;

			if (musicbrainz.cache[i].size & SIZE_VALID)
			{
				cJSON *root = cJSON_ParseWithLength (musicbrainz.cache[i].data, musicbrainz.cache[i].size & SIZE_MASK);
				cJSON *releases;
				if (root)
				{
					struct musicbrainz_database_h *result = 0;
					// we could search for error, but not finding "releases" is sufficient
					releases = cJSON_GetObjectItem (root, "releases");
					if (releases)
					{
						long releases_count = cJSON_GetArraySize (releases);
						long releases_iter;
						for (releases_iter = 0; (releases_iter < releases_count) && releases_iter == 0; releases_iter++)
						{ /* there should only be 1 release for a discid lookup */
							cJSON *release = cJSON_GetArrayItem (releases, releases_iter);
							if (cJSON_IsObject (release))
							{
								musicbrainz_parse_release (release, &result);
							}
						}
					}
					cJSON_Delete (root);
					if (result)
					{
						snprintf (sorted[i].album, sizeof (sorted[i].album), "%s", result->album);
						snprintf (sorted[i].artist, sizeof (sorted[i].artist), "%s", result->artist[0]);
						musicbrainz_database_h_free (result);
					}
				}
			}
		}
		qsort (sorted, musicbrainz.cachecount, sizeof (sorted[0]), sortedcompare);
	}
	return sorted;
}

static void musicbrainzSetupRun (void **token, const struct DevInterfaceAPI_t *API)
{
	int dsel = 0, dialog = 0, epos = 0;
	struct musicbrainz_cacheline_sort_t *sorted = musicbrainz_create_sort ();
	if (!sorted && musicbrainz.cachecount)
	{ /* malloc failure... */
		return;
	}
	while (1)
	{
		API->fsDraw();
		musicbrainzSetupDraw("MusicBrain Cache DataBase", dsel, sorted);
		if (dialog)
		{
			musicbrainzSetupDialogDraw (sorted + dsel, epos);
			while (API->console->KeyboardHit())
			{
				int key = API->console->KeyboardGetChar();
				switch (key)
				{
					case KEY_DOWN:
						if (epos == 0)
						{
							epos = 2;
						} else if ((epos == 1) && (!(musicbrainz.cache[sorted[dsel].pointsat].size & SIZE_PRIVATE)) && (!(musicbrainz.cache[sorted[dsel].pointsat].size & SIZE_VALID)))
						{
							epos = 3;
						}
						break;
					case KEY_UP:
						if (epos == 2)
						{
							epos = 0;
						} else if (epos == 3)
						{
							epos = 1;
						}
						break;
					case KEY_LEFT:
						if (epos == 1)
						{
							epos = 0;
						} else if (epos == 3)
						{
							epos = 2;
						}
						break;
					case KEY_RIGHT:
						if ((epos == 0) && (!(musicbrainz.cache[sorted[dsel].pointsat].size & SIZE_PRIVATE)))
						{
							epos = 1;
						} else if ((epos == 2) && (!(musicbrainz.cache[sorted[dsel].pointsat].size & SIZE_PRIVATE)) && (!(musicbrainz.cache[sorted[dsel].pointsat].size & SIZE_VALID)))
						{
							epos = 3;
						}
						break;
					case KEY_EXIT:
					case KEY_ESC:
						if (musicbrainz.cachecount)
						{
							epos = 0;
							dialog = 0;
							goto superexit;
						}
						break;
					case _KEY_ENTER:
						if (epos == 0)
						{ /* toggle public/private */
							musicbrainz.cache[sorted[dsel].pointsat].size ^= SIZE_PRIVATE;
							if (musicbrainz.cachedirtyfrom > sorted[dsel].pointsat)
							{
								musicbrainz.cachedirtyfrom = sorted[dsel].pointsat;
							}
							musicbrainz.cachedirty = 1;
						} else if (epos == 1)
						{ /* Refresh */
							char discid[29];
							char toc[705+1];
							struct musicbrainz_database_h *result = 0;
							void *handle;

							snprintf (discid, sizeof (discid), "%s", musicbrainz.cache[sorted[dsel].pointsat].discid);
							if ( musicbrainz.cache[sorted[dsel].pointsat].size & SIZE_VALID)
							{
								toc[0] = 0; /* we do not overwrite a VALID entry with an invalid one, so no TOC will be stored if this fails */
							} else {
								uint32_t size = musicbrainz.cache[sorted[dsel].pointsat].size & SIZE_MASK;
								if ((size+1) > sizeof (toc))
								{
									size = 0;
								}
								memcpy (toc, musicbrainz.cache[sorted[dsel].pointsat].data, musicbrainz.cache[sorted[dsel].pointsat].size & SIZE_MASK);
								toc[size] = 0;
							}
							musicbrainz.cache[sorted[dsel].pointsat].size |= SIZE_FORCEREFRESH;
							handle = musicbrainz_lookup_discid_init (discid, toc, &result);
							if (handle)
							{
								while (musicbrainz_lookup_discid_iterate (handle, &result))
								{
									int superbreak = 0;
									int mlWidth = 55;
									int mlHeight = 7;
									int mlTop = (plScrHeight - mlHeight) / 2 ;
									int mlLeft = (plScrWidth - mlWidth) / 2;

									API->fsDraw();

									display_nprintf (mlTop + 0, mlLeft, 0x1e, mlWidth, "\xda%*C\xc4\xbf", mlWidth - 2);
									display_nprintf (mlTop + 1, mlLeft, 0x1e, mlWidth, "\xb3%*C \xb3", mlWidth - 2);
									display_nprintf (mlTop + 2, mlLeft, 0x1e, mlWidth, "\xb3%*C \xb3", mlWidth - 2);
									display_nprintf (mlTop + 3, mlLeft, 0x1e, mlWidth, "\xb3%.15o       Refreshing data from MusicBrainz Server%*C \xb3", mlWidth - 48);
									display_nprintf (mlTop + 4, mlLeft, 0x1e, mlWidth, "\xb3%*C \xb3", mlWidth - 2);
									display_nprintf (mlTop + 5, mlLeft, 0x1e, mlWidth, "\xb3%*C \xb3", mlWidth - 2);
									display_nprintf (mlTop + 6, mlLeft, 0x1e, mlWidth, "\xc0%*C\xc4\xd9", mlWidth - 2);

									API->console->FrameLock ();

									while (API->console->KeyboardHit())
									{
										int key = API->console->KeyboardGetChar();
										switch (key)
										{
											case KEY_EXIT:
											case KEY_ESC:
												musicbrainz_lookup_discid_cancel (handle);
												superbreak = 1;
										}
									}
									if (superbreak)
									{
										break;
									}
								}
								musicbrainz_database_h_free (result);
							}
							free (sorted);
							sorted = musicbrainz_create_sort ();
							if (!sorted && musicbrainz.cachecount)
							{ /* malloc failure... */
								return;
							}
							for (dsel = 0; dsel < musicbrainz.cachecount; dsel++)
							{
								if (!strcmp (musicbrainz.cache[sorted[dsel].pointsat].discid, discid))
								{
									musicbrainz.cache[sorted[dsel].pointsat].size &= ~SIZE_FORCEREFRESH; /* ensure that the flag does not stay, it should not */
									break;
								}
							}
							if (dsel == musicbrainz.cachecount)
							{ /* should never happen */
								dsel = 0;
							}
						} else if (epos == 2)
						{ /* delete */
							free (musicbrainz.cache[sorted[dsel].pointsat].data);
							memmove (&musicbrainz.cache[sorted[dsel].pointsat], &musicbrainz.cache[sorted[dsel].pointsat+1], sizeof (musicbrainz.cache[0]) * (musicbrainz.cachecount - sorted[dsel].pointsat) - 1);
							musicbrainz.cachecount--;
							if (musicbrainz.cachedirtyfrom > sorted[dsel].pointsat)
							{
								musicbrainz.cachedirtyfrom = sorted[dsel].pointsat;
							}
							musicbrainz.cachedirty = 1;
							if ((dsel >= musicbrainz.cachecount) && (musicbrainz.cachecount))
							{
								dsel--;
							}
							free (sorted);
							sorted = musicbrainz_create_sort ();
							if (!sorted && musicbrainz.cachecount)
							{ /* malloc failure... */
								return;
							}
						} else if (epos == 3)
						{ /* submit */
							char url[1024];
							int datalen = musicbrainz.cache[sorted[dsel].pointsat].size & SIZE_MASK;
							char *b = memchr (musicbrainz.cache[sorted[dsel].pointsat].data, ' ', datalen);
							int tracks = 0;
							int i;
#ifndef _WIN32
							pid_t pid;
#endif
							if (b)
							{
								char *c = memchr (b + 1, ' ', datalen - (b - musicbrainz.cache[sorted[dsel].pointsat].data) - 1);
								if (c)
								{
									tracks = atoi (b + 1);
								}
							}
							snprintf (url, sizeof (url), "https://musicbrainz.org/cdtoc/attach?id=%s&tracks=%d&toc=%.*s", musicbrainz.cache[sorted[dsel].pointsat].discid, tracks, datalen, musicbrainz.cache[sorted[dsel].pointsat].data);
							for (i=0; url[i]; i++)
							{
								if (url[i] == ' ')
								{
									url[i] = '+';
								}
							}
#ifdef _WIN32
							ShellExecute (0, 0, url, 0, 0, SW_SHOW);
#else
							pid = fork();
							if (pid == 0)
							{
								for (i=3; i < 1024; i++)
								{
									close (i);
								}
								execlp ("xdg-open", "xdg-open",
								        url,
								        NULL);
								execlp ("sensible-browser", "sensible-browser",
								        url,
								        NULL);
								exit(1);
							} else if (pid > 0)
							{
								int result = 0;
								while (waitpid (pid, &result, 0) != pid)
								{
								}
							}
#endif
						}
						epos = 0;
						dialog = 0;
						goto superexit;
				}
			}
		} else {
			while (API->console->KeyboardHit())
			{
				int key = API->console->KeyboardGetChar();
				switch (key)
				{
					case KEY_HOME:
						dsel = 0;
						break;
					case KEY_END:
						dsel = musicbrainz.cachecount ? musicbrainz.cachecount - 1 : 0;
						break;
					case KEY_UP:
						if (dsel)
						{
							dsel--;
						}
						break;
					case KEY_DOWN:
						if ((dsel + 1) < musicbrainz.cachecount)
						{
							dsel++;
						}
						break;
					case _KEY_ENTER:
						if (musicbrainz.cachecount)
						{
							dialog = 1;
							goto superexit;
						}
						break;
					case KEY_EXIT:
					case KEY_ESC:
						free (sorted);
						return;
					default:
						break;
				}
			}
		}
superexit:
		API->console->FrameLock();
	}
}
