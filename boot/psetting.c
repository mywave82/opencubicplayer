/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
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
 *     the commandline, without these, funny errors occured.)
 *  -fd981106   Felix Domke    <tmbinc@gmx.net>
 *    -edited for new binfile
 *  -ss040613   Stian Skjelstad <stian@nixia.no>
 *    - rewritten for unix
 *    - use argc and argv semantics
 *  -ss040825   Stian Skjelstad <stian@nixia.no>
 *    - added back the commandline stuff
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "psetting.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

char cfConfigDir[PATH_MAX+1];
char cfDataDir[PATH_MAX+1]="";
char cfProgramDir[PATH_MAX+1]; /* we get this from argv[0] */
char cfTempDir[PATH_MAX+1]="/tmp";

#define KEYBUF_LEN 105
#define STRBUF_LEN 405
#define COMMENTBUF_LEN 256

const char *cfConfigSec;
const char *cfSoundSec;
const char *cfScreenSec;

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
		strncpy(comment, line, COMMENTBUF_LEN);
		comment[COMMENTBUF_LEN-1]=0;
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
	char path[PATH_MAX+1];
	FILE *f;
	int linenum=0;
	int cfINIApps_index=-1;

	char keybuf[KEYBUF_LEN];
	char strbuf[STRBUF_LEN];
	char commentbuf[COMMENTBUF_LEN];

	char linebuffer[1024];
	/*  int curapp=-1;*/

	strcpy(path, cfConfigDir);
	strcat(path, "ocp.ini");

	strcpy(keybuf, "");

	cfINIApps=0;
	cfINInApps=0;

	if (!(f=fopen(path, "r")))
		return 1;

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
					cfINIApps[cfINIApps_index].keys=realloc(cfINIApps[cfINIApps_index].keys, sizeof(cfINIApps[cfINIApps_index].keys[0])*(index+1));
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
					cfINIApps=realloc(cfINIApps, sizeof(cfINIApps[cfINInApps])*cfINInApps);
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
					cfINIApps[cfINIApps_index].keys=realloc(cfINIApps[cfINIApps_index].keys, sizeof(cfINIApps[cfINIApps_index].keys[0])*(index+1));
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
				if ((argv[c][1]=='-')&&(!argv[c][2])) /* Unix legacy */
					break;
				if (argv[c][1]=='-')
					continue;

				cfINInApps++;
				cfINIApps=realloc(cfINIApps, sizeof(cfINIApps[cfINInApps])*cfINInApps);
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
					cfINIApps[cfINInApps-1].keys=realloc(cfINIApps[cfINInApps-1].keys, sizeof(cfINIApps[cfINInApps-1].keys[0])*(index+1));
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

		cfINInApps++;
		cfINIApps=realloc(cfINIApps, sizeof(cfINIApps[cfINInApps])*cfINInApps);
		cfINIApps[cfINInApps-1].app=strdup("CommandLine");
		cfINIApps[cfINInApps-1].keys=NULL;
		cfINIApps[cfINInApps-1].nkeys=0;
		cfINIApps[cfINInApps-1].linenum=-1;
		cfINIApps[cfINInApps-1].comment=NULL;

		for (c=1;c<argc;c++)
			if ((argv[c][0]=='-')&&argv[c][1])
			{
				int index=cfINIApps[cfINInApps-1].nkeys;

				if ((argv[c][1]=='-')&&(!argv[c][2])) /* Unix legacy */
					break;

				cfINIApps[cfINInApps-1].nkeys++;
				cfINIApps[cfINInApps-1].keys=realloc(cfINIApps[cfINInApps-1].keys, sizeof(cfINIApps[cfINInApps-1].keys[0])*(index+1));
				cfINIApps[cfINInApps-1].keys[index].key=strdup("_");
				cfINIApps[cfINInApps-1].keys[index].key[0]=argv[c][1];
				cfINIApps[cfINInApps-1].keys[index].str=strdup(argv[c]+2);
				cfINIApps[cfINInApps-1].keys[index].linenum=-1;
				cfINIApps[cfINInApps-1].keys[index].comment=NULL;
			}

		cfINInApps++;
		cfINIApps=realloc(cfINIApps, sizeof(cfINIApps[cfINInApps])*cfINInApps);
		cfINIApps[cfINInApps-1].app=strdup("CommandLine--");
		cfINIApps[cfINInApps-1].keys=NULL;
		cfINIApps[cfINInApps-1].nkeys=0;
		cfINIApps[cfINInApps-1].linenum=-1;
		cfINIApps[cfINInApps-1].comment=NULL;

		for (c=1;c<argc;c++)
			if ((argv[c][0]=='-')&&(argv[c][1]=='-'))
			{
				int index=cfINIApps[cfINInApps-1].nkeys;

				if (!argv[c][2]) /* Unix legacy */
					break;

				cfINIApps[cfINInApps-1].nkeys++;
				cfINIApps[cfINInApps-1].keys=realloc(cfINIApps[cfINInApps-1].keys, sizeof(cfINIApps[cfINInApps-1].keys[0])*(index+1));
				cfINIApps[cfINInApps-1].keys[index].key=strdup(argv[c]+2);
				cfINIApps[cfINInApps-1].keys[index].str=strdup("1");
				cfINIApps[cfINInApps-1].keys[index].linenum=-1;
				cfINIApps[cfINInApps-1].keys[index].comment=NULL;
			}

		cfINInApps++;
		cfINIApps=realloc(cfINIApps, sizeof(cfINIApps[cfINInApps])*cfINInApps);
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
					cfINIApps[cfINInApps-1].keys=realloc(cfINIApps[cfINInApps-1].keys, sizeof(cfINIApps[cfINInApps-1].keys[0])*(index+1));
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

const char *cfGetProfileString(const char *app, const char *key, const char *def)
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

const char *cfGetProfileString2(const char *app, const char *app2, const char *key, const char *def)
{
	return cfGetProfileString(app, key, cfGetProfileString(app2, key, def));
}

void cfSetProfileString(const char *app, const char *key, const char *str)
{
	int i, j;
	for (i=0; i<cfINInApps; i++)
		if (!strcasecmp(cfINIApps[i].app, app))
		{
			for (j=0; j<cfINIApps[i].nkeys; j++)
				if (cfINIApps[i].keys[j].key)
					if (!strcasecmp(cfINIApps[i].keys[j].key, key))
					{
						free(cfINIApps[i].keys[j].str);
						cfINIApps[i].keys[j].str = strdup (str);
						return;
					}
doappend:
			j=cfINIApps[i].nkeys;
			cfINIApps[i].nkeys++;
			cfINIApps[i].keys=realloc(cfINIApps[i].keys, sizeof(cfINIApps[i].keys[0])*(j+1));
			cfINIApps[i].keys[j].key=strdup(key);
			cfINIApps[i].keys[j].str=strdup(str);
			cfINIApps[i].keys[j].comment=NULL;
			cfINIApps[i].keys[j].linenum=9999;
			return;
		}
	cfINInApps++;
	cfINIApps=realloc(cfINIApps, sizeof(cfINIApps[cfINInApps])*cfINInApps);
	cfINIApps[i].app=strdup(app);
	cfINIApps[i].keys=NULL;
	cfINIApps[i].nkeys=0;
	cfINIApps[i].comment=NULL;
	cfINIApps[i].linenum=9999;
	goto doappend;
}

int cfGetProfileInt(const char *app, const char *key, int def, int radix)
{
	const char *s=cfGetProfileString(app, key, "");
	if (!*s)
		return def;
	return strtol(s, 0, radix);
}

int cfGetProfileInt2(const char *app, const char *app2, const char *key, int def, int radix)
{
	return cfGetProfileInt(app, key, cfGetProfileInt(app2, key, def, radix), radix);
}

void cfSetProfileInt(const char *app, const char *key, int str, int radix)
{
	char buffer[64];
	if (radix==16)
		snprintf(buffer, sizeof(buffer), "0x%x", str);
	else
		snprintf(buffer, sizeof(buffer), "%d", str);
	cfSetProfileString(app, key, buffer);
}

int cfGetProfileBool(const char *app, const char *key, int def, int err)
{
	const char *s=cfGetProfileString(app, key, 0);
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

int cfGetProfileBool2(const char *app, const char *app2, const char *key, int def, int err)
{
	return cfGetProfileBool(app, key, cfGetProfileBool(app2, key, def, err), err);
}

void cfSetProfileBool(const char *app, const char *key, const int str)
{
	cfSetProfileString(app, key, (str?"on":"off"));
}

void cfRemoveEntry(const char *app, const char *key)
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

int cfCountSpaceList(const char *str, int maxlen)
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

int cfGetSpaceListEntry(char *buf, const char **str, int maxlen)
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
		fprintf(stderr, "Failed to read ocp.ini\nPlease put it in ~/.ocp/\n");
		return -1;
	}

	t=cfGetProfileString("general", "datadir", NULL);
	if (t)
	{
		if (strlen(t)>=(sizeof(cfDataDir)-1))
		{
			fprintf(stderr, "datadir in ~/.ocp/ocp.ini is too long\n");
			return -1;
		}
		strcpy(cfDataDir, t);
	}
	if (!cfDataDir[0])
		strcpy(cfDataDir, cfProgramDir);
	if (cfDataDir[strlen(cfDataDir)-1]!='/')
	{
		if (strlen(cfDataDir)>=(sizeof(cfDataDir)-1))
		{
			fprintf(stderr, "datadir is too long, can't append / to it\n");
			return -1;
		}
		strcat(cfDataDir, "/");
	}
	t=getenv("TEMP");
	if (!t)
		t=getenv("TMP");
	if (t)
		strncpy(cfTempDir, t, sizeof(cfTempDir));
	if ((t=cfGetProfileString("general", "tempdir", t)))
		strncpy(cfTempDir, t, sizeof(cfTempDir));
	cfTempDir[sizeof(cfTempDir)-1]=0;
	if (cfTempDir[strlen(cfTempDir)-1]!='/')
	{
		if (strlen(cfTempDir)>=(sizeof(cfTempDir)-1))
		{
			fprintf(stderr, "tempdir too long\n");
			return -1;
		}
		strcat(cfTempDir, "/");
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

int cfStoreConfig(void)
{
	char path[PATH_MAX+1];
	FILE *f;
	int i, j;
	char buffer[2+KEYBUF_LEN+1+STRBUF_LEN+COMMENTBUF_LEN+32+1+1];

	strcpy(path, cfConfigDir);
	strcat(path, "ocp.ini");

	if (!(f=fopen(path, "w")))
		return 1;

	for (i=0;i<cfINInApps;i++)
		if (cfINIApps[i].linenum>=0)
		{
			strcpy(buffer, "[");
			strcat(buffer, cfINIApps[i].app);
			strcat(buffer, "]");
			if (cfINIApps[i].comment)
			{
				int n=strlen(buffer)-32;
				if (n>0)
					strncat(buffer, "                                ", n);
				strcat(buffer, cfINIApps[i].comment);
			}
			strcat(buffer, "\n");

			fprintf(f, "%s", buffer);
			for (j=0;j<cfINIApps[i].nkeys;j++)
				if (cfINIApps[i].keys[j].linenum>=0)
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
					fprintf(f, "%s", buffer);
				}
		}
	fclose(f);
	return 0;
}
