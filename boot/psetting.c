/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'23 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * OCP.INI file and environment reading functions
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
 *  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *    -first release
 *  -fd981014   Felix Domke <tmbinc@gmx.net>
 *    -Bugfix at cfReadINIFile (first if-block, skips the filename in
 *     the commandline, without these, funny errors occurred.)
 *  -fd981106   Felix Domke    <tmbinc@gmx.net>
 *    -edited for new binfile
 *  -ss040613   Stian Skjelstad <stian@nixia.no>
 *    - rewritten for unix
 *    - use argc and argv semantics
 *  -ss040825   Stian Skjelstad <stian@nixia.no>
 *    - added back the commandline stuff
 */

#include "config.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "types.h"
#include "psetting.h"
#include "stuff/compat.h"

char *cfProgramDir;
char *cfProgramDirAutoload;

#define KEYBUF_LEN 105
#define STRBUF_LEN 405
#define COMMENTBUF_LEN 256
#define COMMENT_INDENT 26

#ifndef MAX
# define MAX(x,y) ((x)>(y)?(x):(y))
#endif

struct profilekey
{
  char *key;
  char *str;
  char *comment;
  int linenum;
};

struct profileapp
{
  char *app;
  char *comment;
  struct profilekey *keys;
  int nkeys;
  int linenum;
};

static struct profileapp *cfINIApps=NULL;
static int cfINInApps=0;

static int readiniline(char *key, char *str, char *comment, const char *line)
{
	const char *sol;
	const char *eol;
	const char *chk;

	comment[0]=0;

  /* read until we get a line*/
	while (isspace(*line))
		line++;
  /* this line is a comment ?*/
	if (((*line)==';')||((*line)=='#')||(!*line))
	{
		snprintf (comment, COMMENTBUF_LEN, "%.*s", COMMENTBUF_LEN-1, line);
		return 0;
	}

	sol=line;
	eol=sol;

	while ((*eol!='#')&&(*eol!=';')&&(*eol))
		eol++;
	if ((eol[0]==';')||(eol[0]=='#'))
	{
		strncpy(comment, eol, COMMENTBUF_LEN);
		comment[COMMENTBUF_LEN-1]=0;
	}
	while (isspace(eol[-1]))
	{
		if (sol==eol)
			return 0;
		eol--;
	}
	if ((*sol=='[')&&(eol[-1]==']'))
	{
		strcpy(key, "[]");
		if ((eol-sol)>400)
			return 0;
		memcpy(str, sol+1, eol-sol-2);
		str[eol-sol-2]=0;
		return 1;
	}
	if (!(chk=strchr(sol, '=')))
		return 0;
	while (isspace(chk[-1]))
	{
		if (chk==sol)
			return 0;
		chk--;
	}

	if ((chk-sol)>=(KEYBUF_LEN-1))
		return 0;
	memcpy(key, sol, chk-sol);
	key[chk-sol]=0;

	while (chk[-1]!='=')
		chk++;

	while ((chk<eol)&&isspace(*chk))
		chk++;

	if ((eol-chk)>=(STRBUF_LEN-1))
		return 0;
	memcpy(str, chk, eol-chk);
	str[eol-chk]=0;

	return 2;
}

static int cfReadINIFile(int argc, char *argv[])
{
	void *memtmp;
	char *path;
	FILE *f;
	int linenum=0;
	int cfINIApps_index=-1;

	char keybuf[KEYBUF_LEN];
	char strbuf[STRBUF_LEN];
	char commentbuf[COMMENTBUF_LEN];

	char linebuffer[1024];
	/*  int curapp=-1;*/

	makepath_malloc (&path, 0, cfConfigHomeDir, "ocp.ini", 0);

	strcpy(keybuf, "");

	cfINIApps=0;
	cfINInApps=0;

	if (!(f=fopen(path, "r")))
	{
		fprintf (stderr, "fopen(\"%s\", \"r\"): %s\n", path, strerror (errno));
		free (path);
		return 1;
	}
	free (path); path=0;

	while (fgets(linebuffer, sizeof(linebuffer), f))
	{
		linenum++;
		{
			char *tmp;
			if ((tmp=strchr(linebuffer, '\n')))
				*tmp=0;
			if ((tmp=strchr(linebuffer, '\r')))
				*tmp=0;
		}

		commentbuf[0]=0;
		switch (readiniline(keybuf, strbuf, commentbuf, linebuffer))
		{
			case 0:
				if (commentbuf[0]&&(cfINIApps_index>=0))
				{
					int index=cfINIApps[cfINIApps_index].nkeys;
					cfINIApps[cfINIApps_index].nkeys++;
					memtmp = realloc(cfINIApps[cfINIApps_index].keys, sizeof(cfINIApps[cfINIApps_index].keys[0])*(index+1));
					if (!memtmp) { fprintf (stderr, "cfReadINIFile() realloc failed #1 (%lu)\n", (unsigned long)(sizeof(cfINIApps[cfINIApps_index].keys[0])*(index+1))); _exit(1); }
					cfINIApps[cfINIApps_index].keys = memtmp;
					cfINIApps[cfINIApps_index].keys[index].key=NULL;
					cfINIApps[cfINIApps_index].keys[index].str=NULL;
					cfINIApps[cfINIApps_index].keys[index].comment=strdup(commentbuf);
					cfINIApps[cfINIApps_index].keys[index].linenum=linenum;
				}
				break;
			case 1:
				cfINIApps_index=-1;
				{
					int n;
					for (n=0;n<cfINInApps;n++)
						if (!strcmp(cfINIApps[n].app, strbuf))
						{
							cfINIApps_index=n;
							break;
						}
				}
				if (cfINIApps_index<0)
				{
					cfINIApps_index=cfINInApps;
					cfINInApps++;
					memtmp = realloc(cfINIApps, sizeof(cfINIApps[cfINInApps])*cfINInApps);
					if (!memtmp) { fprintf (stderr, "cfReadINIFile() realloc failed #2 (%lu)\n", (unsigned long)(sizeof(cfINIApps[cfINInApps])*cfINInApps)); _exit(1); }
					cfINIApps = memtmp;
					cfINIApps[cfINIApps_index].app=strdup(strbuf);
					cfINIApps[cfINIApps_index].keys=NULL;
					cfINIApps[cfINIApps_index].nkeys=0;
					cfINIApps[cfINIApps_index].comment=(commentbuf[0]?strdup(commentbuf):NULL);
					cfINIApps[cfINIApps_index].linenum=linenum;
				}
				continue;
			case 2:
				if (cfINIApps_index>=0) /* Don't append keys if we don't have a section yet */
				{
					int index=cfINIApps[cfINIApps_index].nkeys;
					cfINIApps[cfINIApps_index].nkeys++;
					memtmp = realloc(cfINIApps[cfINIApps_index].keys, sizeof(cfINIApps[cfINIApps_index].keys[0])*(index+1));
					if (!memtmp) { fprintf (stderr, "cfReadINIFile() realloc failed #3 (%lu)\n", (unsigned long)(sizeof(cfINIApps[cfINIApps_index].keys[0])*(index+1))); _exit(1); }
					cfINIApps[cfINIApps_index].keys = memtmp;
					cfINIApps[cfINIApps_index].keys[index].key=strdup(keybuf);
					cfINIApps[cfINIApps_index].keys[index].str=strdup(strbuf);
					cfINIApps[cfINIApps_index].keys[index].comment=(commentbuf[0]?strdup(commentbuf):NULL);
					cfINIApps[cfINIApps_index].keys[index].linenum=linenum;
				}
				continue;
		}
	}
	{
		char *argvstat=0;
		int c;

		for (c=1;c<argc;c++)
			if ((argv[c][0]=='-')&&argv[c][1])
			{
				if ((argv[c][1]=='-')&&(!argv[c][2])) /* Unix legacy: stop reading parameters if ran like       ./ocp -dcurses -- -filename.xm */
					break;
				if (argv[c][1]=='-') /* Ignore parameters that start with double dash like  ./ocp --help */
					continue;

				/* Generate a new section as ini file contained [commandline_x] and create keypairs for supporting v100,p80,c10,dcurses => v=100 p=80 c=10 d=curses */
				cfINInApps++;
				memtmp = realloc(cfINIApps, sizeof(cfINIApps[cfINInApps])*cfINInApps);
				if (!memtmp) { fprintf (stderr, "cfReadINIFile() realloc failed #4 (%lu)\n", (unsigned long)(sizeof(cfINIApps[cfINInApps])*cfINInApps)); _exit(1); }
				cfINIApps = memtmp;
				cfINIApps[cfINInApps-1].app=strdup("commandline__");
				cfINIApps[cfINInApps-1].app[12]=argv[c][1];
				cfINIApps[cfINInApps-1].keys=NULL;
				cfINIApps[cfINInApps-1].nkeys=0;
				cfINIApps[cfINInApps-1].linenum=-1;
				cfINIApps[cfINInApps-1].comment=NULL;

				argvstat=argv[c]+2;
				while (*argvstat)
				{
					char *temp=index(argvstat, ',');
					int index=cfINIApps[cfINInApps-1].nkeys;

					if (!temp)
						temp=argvstat+strlen(argvstat);

					cfINIApps[cfINInApps-1].nkeys++;
					memtmp = realloc(cfINIApps[cfINInApps-1].keys, sizeof(cfINIApps[cfINInApps-1].keys[0])*(index+1));
					if (!memtmp) { fprintf (stderr, "cfReadINIFile() realloc failed #5 (%lu)\n", (unsigned long)(sizeof(cfINIApps[cfINInApps-1].keys[0])*(index+1))); _exit(1); }
					cfINIApps[cfINInApps-1].keys = memtmp;
					cfINIApps[cfINInApps-1].keys[index].key=strdup("_");
					cfINIApps[cfINInApps-1].keys[index].key[0]=*argvstat;
					argvstat++;
					cfINIApps[cfINInApps-1].keys[index].str=malloc(temp-argvstat+1);
					strncpy(cfINIApps[cfINInApps-1].keys[index].str, argvstat, temp-argvstat);
					cfINIApps[cfINInApps-1].keys[index].str[temp-argvstat]=0;
					cfINIApps[cfINInApps-1].keys[index].linenum=-1;
					cfINIApps[cfINInApps-1].keys[index].comment=NULL;
					argvstat=temp;
					if (*argvstat)
						argvstat++;
				}
			}

		/* Generate a new section as ini file contained [CommandLine] and create keypairs for all arguments as-is:   v100,p10,c10,dcurses => v=100,p10,c10,dcurses */
		cfINInApps++;
		memtmp = realloc(cfINIApps, sizeof(cfINIApps[cfINInApps])*cfINInApps);
		if (!memtmp) { fprintf (stderr, "cfReadINIFile() realloc failed #6 (%lu)\n", (unsigned long)(sizeof(cfINIApps[cfINInApps])*cfINInApps)); _exit(1); }
		cfINIApps = memtmp;
		cfINIApps[cfINInApps-1].app=strdup("CommandLine");
		cfINIApps[cfINInApps-1].keys=NULL;
		cfINIApps[cfINInApps-1].nkeys=0;
		cfINIApps[cfINInApps-1].linenum=-1;
		cfINIApps[cfINInApps-1].comment=NULL;

		for (c=1;c<argc;c++)
			if ((argv[c][0]=='-')&&argv[c][1])
			{
				int index=cfINIApps[cfINInApps-1].nkeys;

				if ((argv[c][1]=='-')&&(!argv[c][2])) /* Unix legacy: stop reading parameters if ran like       ./ocp -dcurses -- -filename.xm */
					break;

				cfINIApps[cfINInApps-1].nkeys++;
				memtmp = realloc(cfINIApps[cfINInApps-1].keys, sizeof(cfINIApps[cfINInApps-1].keys[0])*(index+1));
				if (!memtmp) { fprintf (stderr, "cfReadINIFile() realloc failed #7 (%lu)\n", (unsigned long)(sizeof(cfINIApps[cfINInApps-1].keys[0])*(index+1))); _exit(1); }
				cfINIApps[cfINInApps-1].keys = memtmp;
				cfINIApps[cfINInApps-1].keys[index].key=strdup("_");
				cfINIApps[cfINInApps-1].keys[index].key[0]=argv[c][1];
				cfINIApps[cfINInApps-1].keys[index].str=strdup(argv[c]+2);
				cfINIApps[cfINInApps-1].keys[index].linenum=-1;
				cfINIApps[cfINInApps-1].keys[index].comment=NULL;
			}

		/* Generate a new section as ini file contained [CommandLine--] and create keypairs for all --arguments as-is --help  => help=1 */
		cfINInApps++;
		memtmp = realloc(cfINIApps, sizeof(cfINIApps[cfINInApps])*cfINInApps);
		if (!memtmp) { fprintf (stderr, "cfReadINIFile() realloc failed #8 (%lu)\n", (unsigned long)(sizeof(cfINIApps[cfINInApps])*cfINInApps)); _exit(1); }
		cfINIApps = memtmp;
		cfINIApps[cfINInApps-1].app=strdup("CommandLine--");
		cfINIApps[cfINInApps-1].keys=NULL;
		cfINIApps[cfINInApps-1].nkeys=0;
		cfINIApps[cfINInApps-1].linenum=-1;
		cfINIApps[cfINInApps-1].comment=NULL;

		for (c=1;c<argc;c++)
			if ((argv[c][0]=='-')&&(argv[c][1]=='-'))
			{
				int index=cfINIApps[cfINInApps-1].nkeys;

				if (!argv[c][2]) /* Unix legacy: stop reading parameters if ran like       ./ocp -dcurses -- -filename.xm */
					break;

				cfINIApps[cfINInApps-1].nkeys++;
				memtmp = realloc(cfINIApps[cfINInApps-1].keys, sizeof(cfINIApps[cfINInApps-1].keys[0])*(index+1));
				if (!memtmp) { fprintf (stderr, "cfReadINIFile() realloc failed #9 (%lu)\n", (unsigned long)(sizeof(cfINIApps[cfINInApps-1].keys[0])*(index+1))); _exit(1); }
				cfINIApps[cfINInApps-1].keys = memtmp;
				cfINIApps[cfINInApps-1].keys[index].key=strdup(argv[c]+2);
				cfINIApps[cfINInApps-1].keys[index].str=strdup("1");
				cfINIApps[cfINInApps-1].keys[index].linenum=-1;
				cfINIApps[cfINInApps-1].keys[index].comment=NULL;
			}

		cfINInApps++;
		memtmp = realloc(cfINIApps, sizeof(cfINIApps[cfINInApps])*cfINInApps);
		if (!memtmp) { fprintf (stderr, "cfReadINIFile() realloc failed #10 (%lu)\n", (unsigned long)(sizeof(cfINIApps[cfINInApps])*cfINInApps)); _exit(1); }
		cfINIApps = memtmp;
		cfINIApps[cfINInApps-1].app=strdup("CommandLine_Files");
		cfINIApps[cfINInApps-1].keys=NULL;
		cfINIApps[cfINInApps-1].nkeys=0;
		cfINIApps[cfINInApps-1].linenum=-1;
		cfINIApps[cfINInApps-1].comment=NULL;

		{
			int countin=0;
			int files = 0;
			int playlists = 0;

			for (c=1;c<argc;c++)
			{
				if (!countin)
				{
					if (argv[c][0]=='-')
					{
						if (argv[c][1]=='-')
						{
							if (!argv[c][2])
								countin=1;
						}
						continue;
					}
				}

				{
					int index=cfINIApps[cfINInApps-1].nkeys;
					char buffer[32];
					if (argv[c][0]!='@')
						sprintf(buffer, "file%d", files++);
					else
						sprintf(buffer, "playlist%d", playlists++);
					cfINIApps[cfINInApps-1].nkeys++;
					memtmp = realloc(cfINIApps[cfINInApps-1].keys, sizeof(cfINIApps[cfINInApps-1].keys[0])*(index+1));
					if (!memtmp) { fprintf (stderr, "cfReadINIFile() realloc failed #11 (%lu)\n", (unsigned long)(sizeof(cfINIApps[cfINInApps-1].keys[0])*(index+1))); _exit(1); }
					cfINIApps[cfINInApps-1].keys = memtmp;
					cfINIApps[cfINInApps-1].keys[index].key=strdup(buffer);
					cfINIApps[cfINInApps-1].keys[index].str=strdup(argv[c]+(argv[c][0]=='@'?1:0));
					cfINIApps[cfINInApps-1].keys[index].linenum=-1;
					cfINIApps[cfINInApps-1].keys[index].comment=NULL;
				}
			}
		}
	}
	fclose(f);
	return 0;
}

static void cfFreeINI(void)
{
	int i, j;
	for (i=0;i<cfINInApps;i++)
	{
		for (j=0;j<cfINIApps[i].nkeys;j++)
		{
			if (cfINIApps[i].keys[j].key)
				free(cfINIApps[i].keys[j].key);
			if (cfINIApps[i].keys[j].str)
				free(cfINIApps[i].keys[j].str);
			if (cfINIApps[i].keys[j].comment)
				free(cfINIApps[i].keys[j].comment);
		}
		free(cfINIApps[i].app);
		if (cfINIApps[i].comment)
			free(cfINIApps[i].comment);
		if (cfINIApps[i].keys)
			free(cfINIApps[i].keys);
	}
	if (cfINIApps)
		free(cfINIApps);
}

void cfCloseConfig()
{
	cfFreeINI();
}

static const char *_cfGetProfileString(const char *app, const char *key, const char *def)
{
	int i,j;
	for (i=0; i<cfINInApps; i++)
		if (!strcasecmp(cfINIApps[i].app, app))
			for (j=0; j<cfINIApps[i].nkeys; j++)
				if (cfINIApps[i].keys[j].key)
					if (!strcasecmp(cfINIApps[i].keys[j].key, key))
						return cfINIApps[i].keys[j].str;
	return def;
}

static const char *_cfGetProfileString2(const char *app, const char *app2, const char *key, const char *def)
{
	return cfGetProfileString(app, key, cfGetProfileString(app2, key, def));
}

static void _cfSetProfileString(const char *app, const char *key, const char *str)
{
	int i, j;
	void *memtmp;
	for (i=0; i<cfINInApps; i++)
		if (!strcasecmp(cfINIApps[i].app, app))
		{
			for (j=0; j<cfINIApps[i].nkeys; j++)
				if (cfINIApps[i].keys[j].key)
					if (!strcasecmp(cfINIApps[i].keys[j].key, key))
					{
						if (cfINIApps[i].keys[j].str == str) return;
						free(cfINIApps[i].keys[j].str);
						cfINIApps[i].keys[j].str = strdup (str);
						return;
					}
doappend:
			j=cfINIApps[i].nkeys;
			cfINIApps[i].nkeys++;
			memtmp = realloc(cfINIApps[i].keys, sizeof(cfINIApps[i].keys[0])*(j+1));
			if (!memtmp) { fprintf (stderr, "cfSetProfileString() realloc failed #1 (%lu)\n", (unsigned long)(sizeof(cfINIApps[i].keys[0])*(j+1))); _exit(1); }
			cfINIApps[i].keys = memtmp;
			cfINIApps[i].keys[j].key=strdup(key);
			cfINIApps[i].keys[j].str=strdup(str);
			cfINIApps[i].keys[j].comment=NULL;
			cfINIApps[i].keys[j].linenum=9999;
			return;
		}
	cfINInApps++;
	memtmp = realloc(cfINIApps, sizeof(cfINIApps[cfINInApps])*cfINInApps);
	if (!memtmp) { fprintf (stderr, "cfSetProfileString() realloc failed #2 (%lu)\n", (unsigned long)(sizeof(cfINIApps[cfINInApps])*cfINInApps)); _exit(1); }
	cfINIApps = memtmp;
	cfINIApps[i].app=strdup(app);
	cfINIApps[i].keys=NULL;
	cfINIApps[i].nkeys=0;
	cfINIApps[i].comment=NULL;
	cfINIApps[i].linenum=9999;
	goto doappend;
}

static int _cfGetProfileInt(const char *app, const char *key, int def, int radix)
{
	const char *s=_cfGetProfileString(app, key, "");
	if (!*s)
		return def;
	return strtol(s, 0, radix);
}

static int _cfGetProfileInt2(const char *app, const char *app2, const char *key, int def, int radix)
{
	return _cfGetProfileInt(app, key, _cfGetProfileInt(app2, key, def, radix), radix);
}

static void _cfSetProfileInt(const char *app, const char *key, int str, int radix)
{
	char buffer[64];
	if (radix==16)
		snprintf(buffer, sizeof(buffer), "0x%x", str);
	else
		snprintf(buffer, sizeof(buffer), "%d", str);
	_cfSetProfileString(app, key, buffer);
}

static int _cfGetProfileBool(const char *app, const char *key, int def, int err)
{
	const char *s=_cfGetProfileString(app, key, 0);
	if (!s)
		return def;
	if (!*s)
		return err;
	if (!strcasecmp(s, "on")||!strcasecmp(s, "yes")||!strcasecmp(s, "+")||!strcasecmp(s, "true")||!strcasecmp(s, "1"))
		return 1;
	if (!strcasecmp(s, "off")||!strcasecmp(s, "no")||!strcasecmp(s, "-")||!strcasecmp(s, "false")||!strcasecmp(s, "0"))
		return 0;
	return err;
}

static int _cfGetProfileBool2(const char *app, const char *app2, const char *key, int def, int err)
{
	return _cfGetProfileBool(app, key, _cfGetProfileBool(app2, key, def, err), err);
}

static void _cfSetProfileBool(const char *app, const char *key, const int str)
{
	_cfSetProfileString(app, key, (str?"on":"off"));
}

static const char *_cfGetProfileComment(const char *app, const char *key, const char *def)
{
	int i,j;
	for (i=0; i<cfINInApps; i++)
		if (!strcasecmp(cfINIApps[i].app, app))
			for (j=0; j<cfINIApps[i].nkeys; j++)
				if (cfINIApps[i].keys[j].key)
					if (!strcasecmp(cfINIApps[i].keys[j].key, key))
						return cfINIApps[i].keys[j].comment ? cfINIApps[i].keys[j].comment : def;
	return def;
}

static void _cfSetProfileComment(const char *app, const char *key, const char *comment)
{
	int i, j;
	for (i=0; i<cfINInApps; i++)
	{
		if (!strcasecmp(cfINIApps[i].app, app))
		{
			for (j=0; j<cfINIApps[i].nkeys; j++)
			{
				if (cfINIApps[i].keys[j].key)
				{
					if (!strcasecmp(cfINIApps[i].keys[j].key, key))
					{
						if (cfINIApps[i].keys[j].comment == comment) return;
						free(cfINIApps[i].keys[j].comment);
						cfINIApps[i].keys[j].comment = strdup (comment);
						return;
					}
				}
			}
		}
	}
}

static void _cfRemoveEntry(const char *app, const char *key)
{
	int i, j;
	for (i=0; i<cfINInApps; i++)
		if (!strcasecmp(cfINIApps[i].app, app))
		{
			for (j=0; j<cfINIApps[i].nkeys; j++)
				if (cfINIApps[i].keys[j].key)
					if (!strcasecmp(cfINIApps[i].keys[j].key, key))
					{
						if (cfINIApps[i].keys[j].str)
							free(cfINIApps[i].keys[j].str);
						if (cfINIApps[i].keys[j].key)
							free(cfINIApps[i].keys[j].key);
						if (cfINIApps[i].keys[j].comment)
							free(cfINIApps[i].keys[j].comment);
						memmove(cfINIApps[i].keys + j, cfINIApps[i].keys + j + 1, (cfINIApps[i].nkeys - j  - 1) * sizeof(cfINIApps[i].keys[0]));
						cfINIApps[i].nkeys--;
						if (cfINIApps[i].nkeys)
						{
							void *temp = realloc(cfINIApps[i].keys, cfINIApps[i].nkeys*sizeof(cfINIApps[i].keys[0]));
							if (temp)
								cfINIApps[i].keys = temp;
							else
								fprintf(stderr, __FILE__ ": warning, realloc() failed #1\n");
						}
					}
		}
}

static void _cfRemoveProfile(const char *app)
{
	int i, j;
	for (i=0; i<cfINInApps; i++)
	{
		if (!strcasecmp(cfINIApps[i].app, app))
		{
			for (j=0; j<cfINIApps[i].nkeys; j++)
			{
				if (cfINIApps[i].keys[j].str)
					free(cfINIApps[i].keys[j].str);
				if (cfINIApps[i].keys[j].key)
					free(cfINIApps[i].keys[j].key);
				if (cfINIApps[i].keys[j].comment)
					free(cfINIApps[i].keys[j].comment);
			}
			if (cfINIApps[i].nkeys)
			{
				free (cfINIApps[i].keys);
			}

			memmove (cfINIApps + i, cfINIApps + i + 1, sizeof (cfINIApps[0]) * (cfINInApps - i - 1));
			cfINInApps--;
			i--;
		}
	}
}

static int _cfCountSpaceList(const char *str, int maxlen)
{
	int i=0;
	while (1)
	{
		const char *fb;

		while (isspace(*str))
			str++;
		if (!*str)
			return i;
		fb=str;
		while (!isspace(*str)&&*str)
			str++;
		if ((str-fb)<=maxlen)
			i++;
	}
}

static int _cfGetSpaceListEntry(char *buf, const char **str, int maxlen)
{
	while (1)
	{
		const char *fb;

		while (isspace(**str))
			(*str)++;
		if (!**str)
			return 0;
		fb=*str;
		while (!isspace(**str)&&**str)
			(*str)++;
		if (((*str)-fb)>maxlen)
			continue;
		memcpy(buf, fb, (*str)-fb);
		buf[(*str)-fb]=0;
		return 1;
	}
}

int cfGetConfig(int argc, char *argv[])
{
	const char *t;

	if (!argc)
		return -1; /* no config at all pigs! */
	if (cfReadINIFile(argc, argv))
	{
		fprintf(stderr, "Failed to read ocp.ini\nPlease put it in ~/.ocp/ or $XDG_CONFIG_HOME/ocp/\n");
		return -1;
	}

	t=_cfGetProfileString("general", "datadir", NULL);
	if (t)
	{
		free (cfDataDir);
		cfDataDir = strdup (t);
	}

	if ((t=_cfGetProfileString("general", "tempdir", t)))
	{
		cfTempDir = strdup (t);
	} else if ((t=getenv("TEMP")))
	{
		cfTempDir = strdup (t);
	} else if ((t=getenv("TMP")))
	{
		cfTempDir = strdup (t);
	} else {
		cfTempDir = strdup ("/tmp/");
	}

#ifdef PSETTING_DEBUG
	{
		int i, j;
		char buffer[2+KEYBUF_LEN+1+STRBUF_LEN+COMMENTBUF_LEN+32+1+1];
		for (i=0;i<cfINInApps;i++)
		{
			strcpy(buffer, "[");
			strcat(buffer, cfINIApps[i].app);
			strcat(buffer, "]");
			if (cfINIApps[i].comment)
			{
				while (strlen(buffer)<32)
					strcat(buffer, " ");
				strcat(buffer, cfINIApps[i].comment);
			}
			strcat(buffer, "\n");

			fprintf(stderr, "% 4d:%s", cfINIApps[i].linenum, buffer);
			for (j=0;j<cfINIApps[i].nkeys;j++)
			{
				if (cfINIApps[i].keys[j].key)
				{
					strcpy(buffer, "  ");
					strcat(buffer, cfINIApps[i].keys[j].key);
					strcat(buffer, "=");
					strcat(buffer, cfINIApps[i].keys[j].str);
					if (cfINIApps[i].keys[j].comment)
					{
						while (strlen(buffer)<32)
							strcat(buffer, " ");
						strcat(buffer, cfINIApps[i].keys[j].comment);
					}
				} else
					strcpy(buffer, cfINIApps[i].keys[j].comment);
				strcat(buffer, "\n");
				fprintf(stderr, "% 4d:%s", cfINIApps[i].keys[j].linenum, buffer);
			}
		}
	}
#endif
	return 0;
}

static int _cfStoreConfig(void)
{
	char *path;
	FILE *f;
	int i, j;

	makepath_malloc (&path, 0, cfConfigHomeDir, "ocp.ini", 0);

	if (!(f=fopen(path, "w")))
	{
		fprintf (stderr, "fopen(\"%s\", \"w\"): %s\n", path, strerror (errno));
		free (path);
		return 1;
	}
	free (path); path=0;

	for (i=0;i<cfINInApps;i++)
	{
		if (cfINIApps[i].linenum>=0)
		{
			if (i)
			{
				fprintf (f, "\n");
			}
			fprintf (f, "[%.*s]", KEYBUF_LEN, cfINIApps[i].app);
			if (cfINIApps[i].comment)
			{
				fprintf (f, "%*s%.*s",
			                MAX (0, COMMENT_INDENT - 2 - (int)strlen(cfINIApps[i].app)), "",
					COMMENTBUF_LEN, cfINIApps[i].comment);
			}
			fprintf (f, "\n");

			for (j=0;j<cfINIApps[i].nkeys;j++)
			{
				if (cfINIApps[i].keys[j].linenum>=0)
				{
					if (cfINIApps[i].keys[j].key)
					{
						fprintf (f, "  %.*s=%.*s",
							KEYBUF_LEN, cfINIApps[i].keys[j].key,
							STRBUF_LEN, cfINIApps[i].keys[j].str);
						if (cfINIApps[i].keys[j].comment)
						{
							fprintf (f, "%*s%.*s",
								MAX (0, COMMENT_INDENT - 3 - (int)strlen(cfINIApps[i].keys[j].key) - (int)strlen(cfINIApps[i].keys[j].str)), "",
								COMMENTBUF_LEN, cfINIApps[i].keys[j].comment);
						}
						fprintf (f, "\n");
					} else if (cfINIApps[i].keys[j].comment)
					{
						fprintf (f, "%.*s\n", COMMENTBUF_LEN, cfINIApps[i].keys[j].comment);
					}
				}
			}
		}
	}
	fclose(f);
	return 0;
}

struct configAPI_t configAPI =
{
	_cfStoreConfig,
	_cfGetProfileString,
	_cfGetProfileString2,
	_cfSetProfileString,
	_cfGetProfileBool,
	_cfGetProfileBool2,
	_cfSetProfileBool,
	_cfGetProfileInt,
	_cfGetProfileInt2,
	_cfSetProfileInt,
	_cfGetProfileComment,
	_cfSetProfileComment,
	_cfRemoveEntry,
	_cfRemoveProfile,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	_cfCountSpaceList,
	_cfGetSpaceListEntry
};
