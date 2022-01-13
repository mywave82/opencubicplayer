#include "config.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <cJSON.h>
#include "types.h"
#include "boot/psetting.h"
#include "musicbrainz.h"
#include "stuff/compat.h"

const char musicbrainzsigv1[64] = "Cubic Player MusicBrainz Data Base\x1B\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";

struct musicbrainz_queue_t;
struct musicbrainz_queue_t
{
	char discid[28+1];
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
	int fddb;
	struct timespec lastactive;
	struct musicbrainz_cacheline_t *cache;
	int                             cachecount;
	int                             cachesize;
	int                             cachedirty;
	int                             cachedirtyfrom;
	struct musicbrainz_queue_t *active;
	struct musicbrainz_queue_t *head, *tail;
	int fdout;
	int fderr;
	pid_t pid;
	char out[65536];
	char err[65536];
	int outs;
	int errs;
};

struct musicbrainz_t musicbrainz = {-1, {0, 0}, 0, 0, 0, 0, 0, NULL, NULL, NULL, -1, -1, 0};

static void musicbrainz_parse_release (cJSON *release, struct musicbrainz_database_h **result);

static int musicbrainz_spawn (struct musicbrainz_queue_t *t)
{
	int fdout[2];
	int fderr[2];
	char temp[4096];

	if (pipe (fdout) < 0)
	{
		return -1;
	}
	if (pipe (fderr) < 0)
	{
		close (fdout[0]);
		close (fdout[1]);
		return -1;
	}

	musicbrainz.pid = fork ();
	if (musicbrainz.pid < 0)
	{
		close (fdout[0]);
		close (fdout[1]);
		close (fderr[0]);
		close (fderr[1]);
		return -1;
	}

	if (musicbrainz.pid > 0)
	{
		close (fdout[1]);
		close (fderr[1]);
		fcntl (fdout[0], F_SETFD, FD_CLOEXEC);
		fcntl (fderr[0], F_SETFD, FD_CLOEXEC);
		musicbrainz.fdout = fdout[0];
		musicbrainz.fderr = fderr[0];
		musicbrainz.outs=0;
		musicbrainz.errs=0;
		return 0;
	}

	close (0); /* stdin */
	open ("/dev/null", O_RDONLY);

	close (1); /* stdout */
	dup (fdout[1]);

	close (2); /* stderr */
	dup (fderr[1]);

	close (fdout[0]);
	close (fdout[1]);
	close (fderr[0]);
	close (fderr[1]);

	snprintf (temp, sizeof (temp), "https://musicbrainz.org/ws/2/discid/%s?inc=recordings+artist-credits&cdstubs=no", t->discid);

	execlp ("curl", "curl",
		"--max-redirs", "10",
		"--user-agent", "opencubicplayer/0.2.93 ( stian.skjelstad@gmail.com )",
		"--header", "Accept: application/json",
		"--max-time", "10",
		"-L", temp,
		NULL);

	perror ("execve(curl");

	_exit (1);
}


void *musicbrainz_lookup_discid_init (const char *discid, struct musicbrainz_database_h **result)
{
	struct musicbrainz_queue_t *t;
	int i;
	struct timespec now;

	*result = 0;

	if (strlen (discid) > 28)
	{
		return 0;
	}

	/* check cache first */
	for (i=0; i < musicbrainz.cachecount; i++)
	{
		if (!strcmp (musicbrainz.cache[i].discid, discid))
		{
			if (!musicbrainz.cache[i].lastscan)
			{
				break;
			}
			if ((musicbrainz.cache[i].lastscan + 60 * 60 * 24 * 7 * 26) < time(0)) /* is record over 26 weeks old */
			{
				break;
			}
			cJSON *root = cJSON_ParseWithLength (musicbrainz.cache[i].data, musicbrainz.cache[i].size);
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

	/* make the request */
	t = malloc (sizeof (*t));
	if (!t)
	{
		return 0;
	}
	snprintf (t->discid, sizeof (t->discid), "%s", discid);

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

static void musicbrainz_handlestdout (void)
{
	if (musicbrainz.outs == sizeof (musicbrainz.out))
	{
		char temp[256];
		int res;
		res = read (musicbrainz.fdout, temp, 256);
		if (res < 0)
		{
			return;
		}
		if (res == 0)
		{
			close (musicbrainz.fdout);
			musicbrainz.fdout = -1;
		}
	} else {
		int res;
		res = read (musicbrainz.fdout, musicbrainz.out + musicbrainz.outs, sizeof (musicbrainz.out) - musicbrainz.outs);
		if (res < 0)
		{
			return;
		}
		if (res == 0)
		{
			close (musicbrainz.fdout);
			musicbrainz.fdout = -1;
		} else {
			musicbrainz.outs += res;
		}
	}
}

static void musicbrainz_handlestderr (void)
{
	if (musicbrainz.errs == sizeof (musicbrainz.err))
	{
		char temp[256];
		int res;
		res = read (musicbrainz.fderr, temp, 256);
		if (res < 0)
		{
			return;
		}
		if (res == 0)
		{
			close (musicbrainz.fderr);
			musicbrainz.fderr = -1;
		}
	} else {
		int res;
		res = read (musicbrainz.fderr, musicbrainz.err + musicbrainz.errs, sizeof (musicbrainz.err) - musicbrainz.errs);
		if (res < 0)
		{
			return;
		}
		if (res == 0)
		{
			close (musicbrainz.fderr);
			musicbrainz.fderr = -1;
		} else {
			musicbrainz.errs += res;
		}
	}
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

static void musicbrainz_commit_cache (const char *discid, const char *data, const uint32_t datalen)
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
	musicbrainz.cache[i].size = datalen;
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
		fprintf (stderr, "Curl gave error-code: %d\n", retval);
#if 0
		write (2, musicbrainz.err, musicbrainz.errs);
#endif
		return;
	}
#if 0
	write (1, musicbrainz.out, musicbrainz.outs);
#endif

	{
		cJSON *root = cJSON_ParseWithLength (musicbrainz.out, musicbrainz.outs);
		cJSON *releases;
		if (!root)
		{
			return;
		}

		// we could search for error, but not finding "releases" is sufficient
		releases = cJSON_GetObjectItem (root, "releases");
		if (releases)
		{
			long releases_count = cJSON_GetArraySize (releases);
			long releases_iter;
#if 0
			for (iter = releases->child; iter; iter = iter->next)
			{
				fprintf (stderr, " + %s\n", iter->string);
			}
#endif
			for (releases_iter = 0; (releases_iter < releases_count) && releases_iter == 0; releases_iter++)
			{ /* there should only be 1 release for a discid lookup */
				cJSON *release = cJSON_GetArrayItem (releases, releases_iter);
#if 0
				for (iter = release->child; iter; iter = iter->next)
				{
					fprintf (stderr, " + %s\n", iter->string);
				}
#endif
				if (cJSON_IsObject (release))
				{
					musicbrainz_parse_release (release, result);
				}
			}

			musicbrainz_commit_cache (musicbrainz.active->discid, musicbrainz.out, musicbrainz.outs);
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

	if ((musicbrainz.fdout >= 0) || (musicbrainz.fderr >= 0))
	{
		int count = 0;
		struct pollfd fds[2];
		if ((musicbrainz.fdout >= 0))
		{
			fds[count].fd = musicbrainz.fdout;
			fds[count].events = POLLIN | POLLHUP;
			fds[count].revents = 0;
			count++;
		}
		if ((musicbrainz.fderr >= 0))
		{
			fds[count].fd = musicbrainz.fderr;
			fds[count].events = POLLIN | POLLHUP;
			fds[count].revents = 0;
			count++;
		}
		if (poll (fds, count, 0) >= 0)
		{
			if ((musicbrainz.fdout >= 0) && (fds[0].revents))
			{
				musicbrainz_handlestdout ();
			}
			if ((musicbrainz.fderr >= 0) && (fds[count-1].revents))
			{
				musicbrainz_handlestderr ();
			}
		}
	}

	if ((musicbrainz.fdout < 0) && (musicbrainz.fderr < 0) && (musicbrainz.pid > 0))
	{
		int retval = 0;
		if (waitpid (musicbrainz.pid, &retval, WNOHANG) != musicbrainz.pid)
		{
			return 1;
		}
		clock_gettime (CLOCK_MONOTONIC, &musicbrainz.lastactive);
		musicbrainz.pid = -1;
		musicbrainz_finalize (retval, result);
		free (musicbrainz.active);
		musicbrainz.active = 0;
		return 0;
	} else {
		return 1;
	}
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
		char temp[256];
		kill (musicbrainz.pid, SIGQUIT);
		while ((musicbrainz.fdout >= 0) || (musicbrainz.fderr >= 0))
		{
			if (musicbrainz.fdout >= 0)
			{
				int res;
				res = read (musicbrainz.fdout, temp, 256);
				if (res == 0)
				{
					close (musicbrainz.fdout);
					musicbrainz.fdout = -1;
				}
			}
			if (musicbrainz.fderr >= 0)
			{
				int res;
				res = read (musicbrainz.fderr, temp, 256);
				if (res == 0)
				{
					close (musicbrainz.fderr);
					musicbrainz.fderr = -1;
				}
			}
		}
		while (musicbrainz.pid >= 0)
		{
			int retval = 0;
			if (waitpid (musicbrainz.pid, &retval, WNOHANG) != musicbrainz.pid)
			{
				usleep(10000);
				continue;
			}
			clock_gettime (CLOCK_MONOTONIC, &musicbrainz.lastactive);
			musicbrainz.pid = -1;
		}

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

int musicbrainz_init (void)
{
	char *path;
	char header[64];

	if (musicbrainz.fddb >= 0)
	{
		return 0;
	}

	makepath_malloc (&path, 0, cfConfigDir, "CPMUSBRN.DAT", 0);
	fprintf (stderr, "Loading %s .. ", path);
	musicbrainz.fddb = open (path, O_RDWR | O_CREAT, S_IREAD|S_IWRITE);
	if (musicbrainz.fddb < 0)
	{
		fprintf (stderr, "open(%s): %s\n", path, strerror (errno));
		return 0;
	}
	free (path); path = 0;

	if (flock (musicbrainz.fddb, LOCK_EX | LOCK_NB))
	{
		fprintf (stderr, "Failed to lock the file (more than one instance?)\n");
		return 0;
	}

	if (read (musicbrainz.fddb, &header, sizeof (header)) != sizeof (header))
	{
		fprintf (stderr, "Empty database\n");
		return 1;
	}

	if (memcmp (header, musicbrainzsigv1, sizeof (musicbrainzsigv1)))
	{
		fprintf (stderr, "Old header - discard data\n");
		return 1;
	}

	while (1)
	{
		char temp[28+8+4];
		if (read (musicbrainz.fddb, temp, (28 + 8 + 4)) != (28 + 8 + 4))
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
			musicbrainz.cache[musicbrainz.cachecount].data = malloc (musicbrainz.cache[musicbrainz.cachecount].size);
			if (!musicbrainz.cache[musicbrainz.cachecount].data)
			{
				fprintf (stderr, "musicbrainz_init: malloc() failed\n");
				break;
			}
			if (read (musicbrainz.fddb, musicbrainz.cache[musicbrainz.cachecount].data, musicbrainz.cache[musicbrainz.cachecount].size) != musicbrainz.cache[musicbrainz.cachecount].size)
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
	if (musicbrainz.fddb < 0)
	{
		goto done;
	}
	if (!musicbrainz.cachedirty)
	{
		goto done;
	}
	if (!musicbrainz.cachedirtyfrom)
	{
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
	}
	pos = 64;
	for (i=0; i < musicbrainz.cachedirtyfrom; i++)
	{
		pos += 28 + 8 + 4;
		pos += musicbrainz.cache[i].size;
	}
	lseek (musicbrainz.fddb, pos, SEEK_SET);
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
			if (write (musicbrainz.fddb, temp, 28 + 8 + 4) !=(28 + 8 +4))
			{
				if ((errno != EAGAIN) && (errno != EINTR))
				{
					fprintf (stderr, "musicbrainz_done: write #2 failed\n");
					goto done;
				}
			} else {
				break;
			}
		}
		while (1)
		{
			if (write (musicbrainz.fddb, musicbrainz.cache[i].data, musicbrainz.cache[i].size) != musicbrainz.cache[i].size)
			{
				if ((errno != EAGAIN) && (errno != EINTR))
				{
					fprintf (stderr, "musicbrainz_done: write #3 failed\n");
					goto done;
				}
			} else {
				break;
			}
		}
		pos += 28 + 8 + 4;
		pos += musicbrainz.cache[i].size;
	}
	ftruncate (musicbrainz.fddb, pos);
done:
	for (i=0; i < musicbrainz.cachecount; i++)
	{
		free (musicbrainz.cache[i].data);
	}
	free (musicbrainz.cache);
	close (musicbrainz.fddb);
	musicbrainz.fddb = -1;
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

#if 0
char *cfConfigDir = "/home/stian/.ocp/";
int main (int argc, char *argv[])
{
	void *handle1;
	void *handle2;
	struct musicbrainz_database_h *result1 = 0;
	struct musicbrainz_database_h *result2 = 0;

	musicbrainz_init ();

	handle1 = musicbrainz_lookup_discid_init (".ye78cxEoC.U3EqVZrpvrJkqPUk-", &result1);
	handle2 = musicbrainz_lookup_discid_init ("dDeHI5tMISTDAT89ELxkMi1fTNc-", &result2);

	while (handle1 || handle2)
	{
		usleep (10000);
		if (handle1)
		{
			if (!musicbrainz_lookup_discid_iterate (handle1, &result1))
			{
				handle1 = 0;
			}
		}
		if (handle2)
		{
			if (!musicbrainz_lookup_discid_iterate (handle2, &result2))
			{
				handle2 = 0;
			}
		}
		fputc('.', stderr);
	}
	fputc('\n', stderr);

	musicbrainz_database_h_free (result1);
	musicbrainz_database_h_free (result2);

	result1 = 0;
	result2 = 0;

	musicbrainz_done ();
}
#endif
