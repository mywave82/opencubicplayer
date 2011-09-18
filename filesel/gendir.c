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

static void trim_off_a_directory(char *src)
{
	char *last_slash=src, *next;
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
}

void gendir(const char *orgdir, const char *fixdir, char *_retval)
{
	char fixdir_clone[PATH_MAX+1];
	char *next_token;
	char retval[PATH_MAX+1];
	if (strlen(orgdir)>(PATH_MAX))
	{
		fprintf(stderr, "gendir.c: strlen(orgdir)>PATH_MAX\n");
		exit(1);
		/*strcpy(_retval, "/");
		return;*/
	}
	if (strlen(fixdir)>(PATH_MAX))
	{
		fprintf(stderr, "gendir.c: strlen(fixdir)>PATH_MAX\n");
		exit(1);
		/*strcpy(_retval, "/");
		return;*/
	}
	/* we are safe.. buffers should never overflow now! */

	strcpy(retval, orgdir);
	strcpy(fixdir_clone, fixdir);

	trim_off_leading_slashes(retval);
	trim_off_leading_slashes(fixdir_clone);

	next_token=fixdir_clone;
	while (*fixdir_clone)
	{
		if ((*fixdir_clone=='/'))
		{
			strcpy(retval, "/");
			memmove(fixdir_clone, fixdir_clone+1, strlen(fixdir_clone)/*+0*/);
			continue;
		}

		if ((next_token=strchr(fixdir_clone+1, '/')))
		{
			*next_token=0;
			next_token++;
		} else {
			next_token=fixdir_clone+strlen(fixdir_clone);
		}

		if (!strcmp(fixdir_clone, "."))
		{ /* Do nothing */
		} else if (!strcmp(fixdir_clone, ".."))
		{ /* Bump up a level if possible */
			trim_off_a_directory(retval);
		} else { /* append the shit.. prepend it with a / if needed */
			if (retval[1])
			{
				if (strlen(retval)<=(PATH_MAX))
					strcat(retval, "/");
			}
			if ((strlen(retval)+strlen(fixdir_clone))<=PATH_MAX)
				strcat(retval, fixdir_clone);
		}
		memmove(fixdir_clone, next_token, strlen(next_token)+1);
	}
	trim_off_leading_slashes(retval);
	strcpy(_retval, retval);
	return;
}

void genreldir(const char *orgdir, const char *fixdir, char *targetdir)
{
	char orgdirclone[PATH_MAX+1];
	char fixdirclone[PATH_MAX+1];

	char *nextorgdir, *curorgdir;
	char *nextfixdir, *curfixdir;

	int firsttoken=1;

	if ((orgdir[0]!='/')||(fixdir[0]!='/'))
	{
		strcpy(targetdir, fixdir);
		return;
	}

	targetdir[0]=0;
	strcpy(orgdirclone, orgdir);
	strcpy(fixdirclone, fixdir);

	nextorgdir=orgdirclone+1;
	nextfixdir=fixdirclone+1;

	while (1)
	{
		curorgdir=nextorgdir;
		curfixdir=nextfixdir;
		if (curorgdir)
			if (!*curorgdir)
				curorgdir=NULL;
		if (curfixdir)
			if (!*curfixdir)
				curfixdir=NULL;

		if (!curorgdir) /* we append after old, no back-patch */
		{
			if (curfixdir)
			{
				strcpy(targetdir, curfixdir);
				return;
			}
			strcpy(targetdir, ".");
			trim_off_leading_slashes(targetdir);
			return;
		}

		if (!curfixdir) /* we back-patch all tokens after here */
		{
			while (curorgdir)
			{
				if (*targetdir)
					if (strlen(targetdir)<PATH_MAX)
						strcat(targetdir, "/");
				if ((strlen(targetdir)+2)<PATH_MAX)
					strcat(targetdir, "..");
				if ((curorgdir=index(curorgdir, '/')))
					curorgdir++;
				if (curorgdir)
					if (!*curorgdir)
						curorgdir=NULL;
			}
			return;
		}
		if ((nextorgdir=index(curorgdir, '/')))
			*(nextorgdir++)=0;
		if ((nextfixdir=index(curfixdir, '/')))
			*(nextfixdir++)=0;
		if (strcmp(curorgdir, curfixdir))
		{
			if (firsttoken) /* Total difference, then we use full path */
			{
				strcpy(targetdir, fixdir);
				return;
			}
			while (curorgdir) /* Add all the needed .. */
			{
				if (*targetdir)
					if (strlen(targetdir)<PATH_MAX)
						strcat(targetdir, "/");
				if ((strlen(targetdir)+2)<PATH_MAX)
					strcat(targetdir, "..");
				if ((curorgdir=index(curorgdir, '/')))
					curorgdir++;
				if (curorgdir)
					if (!*curorgdir)
						curorgdir=NULL;
			}
			while (curfixdir) /* Add the needed new-names */
			{
				if (*targetdir)
					if (strlen(targetdir)<PATH_MAX)
						strcat(targetdir, "/");
				if ((strlen(targetdir)+strlen(curfixdir))<PATH_MAX)
					strcat(targetdir, curfixdir);

				if ((curfixdir=nextfixdir))
				{
					if ((nextfixdir=index(curfixdir, '/')))
						*(nextfixdir++)=0;
					if (curfixdir)
						if (!*curfixdir)
							curfixdir=NULL;
				}
			}
			return;
		}
		firsttoken=0;
	}
}

/*
#include <stdio.h>
int main(int argc, char *argv[])
{
	char temp[PATH_MAX+1];

	gendir("/", "/", temp);
	printf("/ / -> %s\n", temp);

	gendir("/home/stian", "/", temp);
	printf("/home/stian / -> %s\n", temp);

	gendir("/home/stian", "../", temp);
	printf("/home/stian/ ../ -> %s\n", temp);

	gendir("/home/stian", "../.", temp);
	printf("/home/stian/ ../. -> %s\n", temp);

	gendir("/home/stian", "./", temp);
	printf("/home/stian/ ./ -> %s\n", temp);

	gendir("/home/stian", "..//", temp);
	printf("/home/stian/ ..// -> %s\n", temp);

	gendir("/home/stian", "../../tmp/test/./", temp);
	printf("/home/stian/ ../../tmp/test/./ -> %s\n", temp);

	printf("\n");

	genreldir("/home/stian", "/home/stian/disk/load.mp3", temp);
	printf("/home/stian /home/stian/disk/load.mp3 -> %s\n", temp);

	genreldir("/home/stian/", "/home/stian/disk/load.mp3", temp);
	printf("/home/stian/ /home/stian/disk/load.mp3 -> %s\n", temp);

	genreldir("/home/stian", "/home/stian/disk/load.mp3/", temp);
	printf("/home/stian /home/stian/disk/load.mp3/ -> %s\n", temp);

	genreldir("/home/stian/", "/home/stian/disk/load.mp3/", temp);
	printf("/home/stian/ /home/stian/disk/load.mp3/ -> %s\n", temp);

	genreldir("/home/stian", "/home/alfa/disk99/song.s3m", temp);
	printf("/home/stian /home/alfa/disk99/song.s3m -> %s\n", temp);

	genreldir("/home/stian", "/root/diska/test.ogg", temp);
	printf("/home/stian /root/diska/test.ogg -> %s\n", temp);

	return 0;
}*/
