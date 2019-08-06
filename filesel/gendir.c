/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * Parsing a directory, and a patch.. aka   /root/Desktop + ../.xmms => /root/.xmms
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
 *
 * revision history: (please note changes here)
 *  -ss051231  Stian Skjelstad <stian@nixia.no>
 *    -first release
 */

#include "config.h"
#include "gendir.h"
#include "types.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* src="/foo/bar////  is stripped down to /foo/bar
   src="////////  is stripped down to /
 */
static void trim_off_leading_slashes(char *src)
{
	char *trim;
	while (1)
	{
		if (strlen(src))
		{
			trim=src+strlen(src)-1;
			if (*trim=='/')
				*trim=0;
			else
				break;
		} else {
			strcpy(src, "/");
			break;
		}
	}
}

static int trim_off_a_directory(char *src)
{
	char *last_slash=src, *next;

	if (!strcmp(src, "/"))
	{
		return -1; /* already at root, nothing more to strip away */
	}

	while ((next=strchr(last_slash+1, '/')))
	{
		if (!next[1]) /* but we accept the string to end with a / */
			break;
		last_slash=next;
	}
	if (last_slash!=src)
		*last_slash=0;
	else
		src[1]=0; /* let the / be alone */

	return 0;
}

struct stringbuilder
{
	char *str;
	unsigned int length;
	unsigned int size;
};

static int stringbuilder_init(struct stringbuilder *s)
{
	s->size=128;
	s->length=0;
	s->str=malloc(s->size);
	if (!s->str)
	{
		fprintf (stderr, "stringbuilder_init: malloc() failed\n");
		return -1;
	} else {
		s->str[0] = 0;
		return 0;
	}
}

static int stringbuilder_append(struct stringbuilder *s, const char *str)
{
	int cache = strlen(str);

	if (s->size <= s->length+1+cache)
	{
		char *temp;
		s->size = s->length + 1 + cache + 128;
		temp = realloc(s->str, s->size);
		if (!temp)
		{
			fprintf (stderr, "stringbuilder_append: realloc failed\n");
			return -1;
		}
		s->str = temp;
	}
	strcat (s->str, str);
	s->length += strlen(str);
	return 0;
}

static void stringbuilder_free(struct stringbuilder *s)
{
	free (s->str);
	s->str = 0;
}

static char *stringbuilder_take(struct stringbuilder *s)
{
	char *temp = strdup (s->str);
	if (!temp)
	{
		return s->str; /* if strdup() fails, we can always return the buffer */
	}
	free (s->str);
	s->str=0;
	return temp;
}

static char *next_dirpath(char *src)
{
	char *retval = index(src, '/');
	if (retval)
	{
		retval[0] = 0;
		if (!retval[1])
		{
			return 0;
		}
		return retval + 1;
	}
	return 0;
}


int gendir_malloc_internal (struct stringbuilder *buf, char *curr)
{
	char *next;

	next = curr[0] ? curr : 0;
	while (next)
	{
		curr = next;
		next = next_dirpath (curr);

		if (!strlen(curr))
		{ /* double slash, do nothing */
		} else if (!strcmp (curr, "."))
		{ /* Do nothing */
		} else if (!strcmp (curr, ".."))
		{ /* Bump up a level if possible */
			if (trim_off_a_directory(buf->str))
			{ /* no directories to trim off */
				return -1;
			}
			buf->length = strlen(buf->str);
		} else { /* append the shit.. prepend it with a / if needed */
			if (buf->length > 1)
			{
				if (stringbuilder_append (buf, "/"))
				{
					fprintf (stderr, "gendir_malloc(): stringbuilder_append failed #1\n");
					return -1;
				}
			}
			if (stringbuilder_append (buf, curr))
			{
				fprintf (stderr, "gendir_malloc(): stringbuilder_append failed #2\n");
				return -1;
			}
		}
	}
	return 0;
}


int gendir_malloc (const char *basepath, const char *relpath, char **resultpath)
{
	char *relpathclone = 0;
	char *tmp;
	int retval = -1;
	struct stringbuilder buf;

	/* assertion, and pre-clearing *relpath */
	if (!resultpath)
	{
		fprintf (stderr, "gendir_malloc(): resultpath==NULL\n");
		return -1;
	}
	*resultpath=0;
	if (!basepath)
	{
		fprintf (stderr, "gendir_malloc(): basepath==NULL\n");
		return -1;
	}
	if (!relpath)
	{
		fprintf (stderr, "gendir_malloc(): relpath==NULL\n");
		return -1;
	}
	if (basepath[0]!='/')
	{
		fprintf (stderr, "gendir_malloc(): basepath does not start with /\n");
		return -1;
	}

	relpathclone=strdup(relpath);
	if (!relpathclone)
	{
		fprintf (stderr, "gendir_malloc(): strdup() failed #1\n");
		return -1;
	}
	trim_off_leading_slashes (relpathclone);

	/* special-case if relpathclone starts with / */
	if (stringbuilder_init (&buf))
	{
		free (relpathclone);
		fprintf (stderr, "gendir_malloc(): stringbuilder_init failed\n");
		return -1;
	}

	if (relpathclone[0] == '/')
	{
		if (stringbuilder_append (&buf, "/"))
		{
			fprintf (stderr, "gendir_malloc(): stringbuilder_append failed #3\n");
			goto free_out;
		}
	} else {
		if (stringbuilder_append (&buf, basepath))
		{
			fprintf (stderr, "gendir_malloc(): stringbuilder_append failed #4\n");
			goto free_out;
		}
		/* Replace all instances of // with / */
		while ((tmp = strstr(buf.str, "//")))
		{
			memmove (tmp, tmp+1, strlen(tmp));
		}
		trim_off_leading_slashes (buf.str);
		buf.length = strlen (buf.str);
	}

	retval = gendir_malloc_internal (&buf, relpathclone[0]=='/' ? relpathclone+1 : relpathclone);
free_out:
	if (retval)
	{
		stringbuilder_free (&buf);
	} else {
		*resultpath = stringbuilder_take (&buf);
	}
	free (relpathclone);
	return retval;
}

static int genreldir_malloc_internal(char *basepathclone, char *targetpathclone, char **relpath)
{
	char *nextbasepath, *curbasepath;
	char *nexttargetpath, *curtargetpath;
	int firsttoken=1;
	char *tmp;

	/* Replace all instances of // with / */
	while ((tmp = strstr(basepathclone, "//")))
	{
		memmove (tmp, tmp+1, strlen(tmp));
	}
	while ((tmp = strstr(targetpathclone, "//")))
	{
		memmove (tmp, tmp+1, strlen(tmp));
	}

	/* intialize the loop - we are going to find the first point the two paths diverge, node by node.
	 *
	 * nextbasepath and nexttargetpath should always point to the next iteration
	 *
	 * curbasepath and curtargetpath points to the current iteration
	 */
	nextbasepath  = basepathclone[1]   ? basepathclone + 1 : 0;
	nexttargetpath= targetpathclone[1] ? targetpathclone + 1 : 0;

	while (1)
	{
		/* have we reach end of lines? */

		if (!nextbasepath) /* we append after old, no back-patch */
		{
			if (nexttargetpath)
			{
				/* orgpath: /foo/bar
				   newpath: /foo/bar/dir/file.mod
				   result:  dir/file.mod
				 */

				*relpath = strdup(nexttargetpath);
				if (!*relpath)
				{
					fprintf (stderr, "genreldir_malloc:() strdup() failed #3\n");
					return -1;
				}
				trim_off_leading_slashes (*relpath);
				return 0;
			} else {
				/* orgpath: /foo/bar
				   newpath: /foo/bar/
				   result:  .
				 */
				*relpath = strdup(".");
				if (!*relpath)
				{
					fprintf (stderr, "genreldir_malloc:() strdup() failed #4\n");
					return -1;
				}
				return 0;
			}
			/* this is unreachable */
		}

		if (!nexttargetpath) /* we back-patch all tokens after here */
		{
			/* orgpath: /foo/bar/test
			   newpath: /foo
			   result:  ../..
			 */

			struct stringbuilder buf;

			if (stringbuilder_init(&buf))
			{
				fprintf(stderr, "genreldir_malloc:() stringbuilder failed #1\n");
				return -1;
			}

			while (nextbasepath)
			{
				if (!buf.length) /* first iteration, set targetpath to .. */
				{
					if (stringbuilder_append(&buf, ".."))
					{
						stringbuilder_free(&buf);
						fprintf (stderr, "genreldir_malloc:() stringbuilder_append() failed #1\n");
						return -1;
					}
				} else { /* all other iterations, append /.. onto targetpath */
					if (stringbuilder_append(&buf, "/.."))
					{
						stringbuilder_free(&buf);
						fprintf (stderr, "genreldir_malloc:() stringbuilder_append() failed #2\n");
						return -1;
					}
				}
				curbasepath = nextbasepath;
				nextbasepath = next_dirpath (curbasepath);
			}
			*relpath = stringbuilder_take(&buf);
			return 0;
		}

		/* nope, so terminate current node at the next /, and setup the next pointers */
		curbasepath   = nextbasepath;
		curtargetpath = nexttargetpath;
		nextbasepath   = next_dirpath (curbasepath);
		nexttargetpath = next_dirpath (curtargetpath);

		if (strcmp(curbasepath, curtargetpath))
		{
			struct stringbuilder buf;

			if (stringbuilder_init(&buf))
			{
				fprintf(stderr, "genreldir_malloc:() stringbuilder failed #3\n");
				return -1;
			}

			if (!firsttoken) /* do not make all the .., we add a / later instead */
			{
				while (curbasepath) /* Add all the needed .. */
				{
					if (!buf.length) /* first iteration, set targetpath to .. */
					{
						if (stringbuilder_append(&buf, ".."))
						{
							stringbuilder_free(&buf);
							fprintf (stderr, "genreldir_malloc:() stringbuilder_append() failed #3\n");
							return -1;
						}
					} else { /* all other iterations, append /.. onto targetpath */
						if (stringbuilder_append(&buf, "/.."))
						{
							stringbuilder_free(&buf);
							fprintf (stderr, "genreldir_malloc:() stringbuilder_append() failed #4\n");
							return -1;
						}
					}

					curbasepath = nextbasepath;
					if (curbasepath)
					{
						nextbasepath = next_dirpath (curbasepath);
					}
				}
			}
			while (curtargetpath) /* Add the needed new-names */
			{
				if (buf.length || firsttoken)
				{
					if (stringbuilder_append(&buf, "/"))
					{
						stringbuilder_free(&buf);
						fprintf (stderr, "genreldir_malloc:() stringbuilder_append() failed #5\n");
						return -1;
					}
				}
				if (stringbuilder_append(&buf, curtargetpath))
				{
					stringbuilder_free(&buf);
					fprintf (stderr, "genreldir_malloc:() stringbuilder_append() failed #6\n");
					return -1;
				}

				curtargetpath = nexttargetpath;
				if (curtargetpath)
				{
					nexttargetpath = next_dirpath (curtargetpath);
				}
			}
			*relpath = stringbuilder_take (&buf);
			return 0;
		}
		firsttoken=0;
	}
}

int genreldir_malloc(const char *basepath, const char *targetpath, char **relpath)
{
	int retval = -1;
	char *basepathclone = 0;
	char *targetpathclone = 0;

	/* assertion, and pre-clearing *relpath */
	if (!relpath)
	{
		fprintf (stderr, "genreldir_malloc:() relpath==NULL\n");
		return -1;
	}
	*relpath=0;
	if (!basepath)
	{
		fprintf (stderr, "genreldir_malloc:() basepath==NULL\n");
		return -1;
	}
	if (!targetpath)
	{
		fprintf (stderr, "genreldir_malloc:() targetpath==NULL\n");
		return -1;
	}

	if (basepath[0]!='/')
	{
		fprintf (stderr, "genreldir_malloc:() basepath does not start with /\n");
		return -1;
	}

	if (targetpath[0]!='/')
	{
		fprintf (stderr, "genreldir_malloc:() targetpath does not start with /\n");
		return -1;
	}

	/* duplicate basepath and targetpath, since we going to replace / with \0 while parsing */
	basepathclone = strdup(basepath);
	if (!basepathclone)
	{
		fprintf (stderr, "genreldir_malloc:() strdup() failed #1\n");
		goto free_out;
	}
	targetpathclone = strdup(targetpath);
	if (!targetpathclone)
	{
		fprintf (stderr, "genreldir_malloc:() strdup() failed #2\n");
		goto free_out;
	}

	retval = genreldir_malloc_internal(basepathclone, targetpathclone, relpath);

free_out:
	if (retval)
	{
		if (*relpath)
		{
			/* this should not be reachable, but you can never be too sure */
			free (*relpath);
			*relpath = 0;
		}
	}

	free (basepathclone);
	free (targetpathclone);
	return retval;
}
