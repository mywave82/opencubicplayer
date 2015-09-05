/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * GMIPlay timidity PAT format handler (reads GUS patches etc)
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
 *  -sss050415 Stian Skjelstad <stian@nixia.no>
 *    -first release
 */

#include "config.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "types.h"
#include "gmipat.h"
#include "gmiplay.h"
#include "stuff/err.h"

#define DirStackSize 5
#define maxlevel 10

static char DirectoryStack[DirStackSize][PATH_MAX+1];
static int DirectoryStackIndex;

static void parse_config(FILE *input, int level)
{
	const char *home=getenv("HOME");
	char line[1024], *pos, *base;
	int type=0;
	while (fgets(line, sizeof(line), input))
	{
		if ((pos=strchr(line, '\r')))
			*pos=0;
		if ((pos=strchr(line, '\n')))
			*pos=0;
		if ((pos=strchr(line, '#')))
			*pos=0;
		base=line;
		while ((*base)&&isspace(*base))
			base++;
		if (!base)
			continue;
		if (!strncmp(base, "dir ", 4))
		{
			int pos;
			if (DirectoryStackIndex==DirStackSize)
			{
				fprintf(stderr, "[timidity] Too many dir sections in config\n");
				continue;
			}
			pos=0;
			base+=4;
			while (*base)
			{
				if ((*base)=='~')
				{
					if ((pos+strlen(home))>=PATH_MAX)
					{
						fprintf(stderr, "[timidity] a dir entry is too long\n");
						goto no_add_dir;
					}
					strcpy(DirectoryStack[DirectoryStackIndex]+pos, home);
					pos+=strlen(home);
				} else {
					if (pos>=PATH_MAX)
					{
						fprintf(stderr, "[timidity] a dir entry is too long\n");
						goto no_add_dir;
					}
					DirectoryStack[DirectoryStackIndex][pos++]=*base;
				}
				base++;
			}
			DirectoryStack[DirectoryStackIndex++][pos]=0;
no_add_dir:{}
		} else if (!strncmp(base, "source ", 7))
		{
			char path[PATH_MAX+1];
			int i;
			if (level==maxlevel)
			{
				fprintf(stderr, "[timidity] Too high nesting level of config-files");
				continue;
			}
			base+=7;
			for (i=DirectoryStackIndex-1;i>=0;i--)
			{
				fprintf(stderr, "[timidity]: Directorystack %d is %s\n", i, DirectoryStack[i]);
				if ((strlen(base)+strlen(DirectoryStack[i]+1))<=PATH_MAX)
				{
					FILE *file;
					if (base[0] != '/')
					{
						strcpy(path, DirectoryStack[i]);
						strcat(path, "/");
					}
					strcat(path, base);
					if ((file=fopen(path, "r")))
					{
						fprintf(stderr, "[timidity] parsing %s\n", path);
						parse_config(file, level+1);
						fclose(file);
						break;
					}
				}
			}
			if (i<0)
				fprintf(stderr, "[timidity] Failed to find file for source '%s'\n", base);
		} else if (!strncmp(base, "drumset ", 8))
		{
			type=0;
			base+=8;
			while ((*base)&&isspace(*base))
				base++;
			if (*base)
				if (isdigit(*base)&&!atoi(base))
					type=2;
		} else if (!strncmp(base, "bank ", 5))
		{
			type=0;
			base+=5;
			while ((*base)&&isspace(*base))
				base++;
			if (*base)
				if (isdigit(*base)&&!atoi(base))
					type=1;
		} else if (isdigit(*base)&&type)
		{
			unsigned long insnum=strtoul(base, 0, 10)+((type==2)?128:0);
			if (insnum<256)
			{
				while ((*base)&&isdigit(*base))
					base++;
				while ((*base)&&isspace(*base))
					base++;
				if (*base)
				{
					while ((*base)&&isspace(*base))
						base++;
					for(pos=base;!isspace(*pos);pos++)
					{
						if (!*pos)
						{
							pos=0;
							break;
						}
					}
					if (pos)
						*(pos++)=0;
					snprintf(midInstrumentNames[insnum], sizeof(midInstrumentNames[insnum]), "%s", base); /* we ignore parameters for now TODO */
				}
			}
		}
	}
}

static int loadpatchTimidity( struct minstrument *ins,
                              uint8_t             program,
                              uint8_t            *sampused,
                              struct sampleinfo **smps,
                              uint16_t           *samplenum)
{
	int i;
	FILE *file=NULL;
	int retval;
	char path[PATH_MAX+NAME_MAX+1];
	int needpat = 1;
	int len;

	ins->sampnum=0;
	*ins->name=0;

	if (!*midInstrumentNames[program])
	{
		fprintf(stderr, "[timidity] no entry configured for program %d\n", program);
		return errFileMiss;
	}

	len = strlen(midInstrumentNames[program]);
	if (len >= 4)
		needpat = strcasecmp(midInstrumentNames[program]+len-4, ".pat");

	if (midInstrumentNames[program][0] == '/') /* Is the path absolute? */
	{
		snprintf(path, sizeof(path), "%s%s", midInstrumentNames[program], needpat?".pat":"");

		if ((file=fopen(path, "r"))==NULL)
		{
			fprintf(stderr, "[timidity] '%s': failed to open file: %s\n", path, strerror (errno));
		}
	} else {
		for (i=DirectoryStackIndex-1;i>=0;i--)
		{
			snprintf(path, sizeof(path), "%s/%s%s", DirectoryStack[i], midInstrumentNames[program], needpat?".pat":"");
			if ((file=fopen(path, "r"))!=NULL)
				break;
			fprintf(stderr, "[timidity] '%s': failed to open file (we might try other directory prefixes before we give up)\n", path);
		}
	}
	if (!file)
	{
		fprintf(stderr, "[timidity] failed to open instrument %s\n", midInstrumentNames[program]);
		return errFileMiss;
	}
	fprintf(stderr, "[timidity] loading file %s\n", path);
	retval=loadpatchPAT(file, ins, program, sampused, smps, samplenum);
	fclose(file);
	if (retval)
		fprintf(stderr, "Invalid PAT file\n");
	return retval;
}

static int addpatchTimidity( struct minstrument *ins,
                             uint8_t             program,
                             uint8_t             sn,
                             uint8_t             sampnum,
                             struct sampleinfo  *sip,
                             uint16_t           *samplenum)
{
	FILE *file=NULL;
	int retval;
	char path[PATH_MAX+NAME_MAX];
	int i;
	int needpat = 1;
	int len;

	if (!*midInstrumentNames[program])
	{
		fprintf(stderr, "[timidity] not entry configured for program %d\n", program);
		return errFileMiss;
	}

	len = strlen(midInstrumentNames[program]);
	if (len >= 4)
		needpat = strcasecmp(midInstrumentNames[program]+len-4, ".pat");

	if (midInstrumentNames[program][0] == '/') /* Is the path absolute? */
	{
		snprintf(path, sizeof(path), "%s%s", midInstrumentNames[program], needpat?".pat":"");

		if ((file=fopen(path, "r"))==NULL)
		{
			fprintf(stderr, "[timidity] '%s': failed to open file: %s\n", path, strerror (errno));
		}
	} else {
		for (i=DirectoryStackIndex-1;i>=0;i--)
		{
			snprintf(path, sizeof(path), "%s/%s%s", DirectoryStack[i], midInstrumentNames[program], needpat?".pat":"");
			if ((file=fopen(path, "r"))!=NULL)
				break;
			fprintf(stderr, "[timidity] '%s': failed to open file (we might try other directory prefixes before we give up)\n", path);
		}
	}

	if (!file)
	{
		fprintf(stderr, "[timidity] '%s': failed to open file\n", midInstrumentNames[program]);
		return errFileMiss;
	}

	fprintf(stderr, "[timidity] loading file %s\n", path);
	retval=addpatchPAT(file, ins, program, sn, sampnum, sip, samplenum);
	fclose(file);
	if (retval)
		fprintf(stderr, "Invalid PAT file\n");
	return retval;
}

int __attribute__ ((visibility ("internal"))) midInitTimidity(void)
{
	FILE *inifile;
	int i;

	_midClose=0;

	for (i=0; i<256; i++)
		midInstrumentNames[i][0]=0;

	DirectoryStackIndex=0;

	if ((inifile=fopen("/etc/timidity.cfg", "r")))
	{
		fprintf(stderr, "[timidity] parsing /etc/timitidy.cfg\n");
		strcpy(DirectoryStack[DirectoryStackIndex++], "/etc/");
	} else if ((inifile=fopen("/etc/timidity/timidity.cfg", "r")))
	{
		fprintf(stderr, "[timidity] parsing /etc/timidity/timitidy.cfg\n");
		strcpy(DirectoryStack[DirectoryStackIndex++], "/etc/timidity/");
	} else if ((inifile=fopen("/usr/local/etc/timidity.cfg", "r")))
	{
		fprintf(stderr, "[timidity] parsing /usr/local/etc/timitidy.cfg\n");
		strcpy(DirectoryStack[DirectoryStackIndex++], "/usr/local/etc/");
	} else if ((inifile=fopen("/usr/share/timidity/timidity.cfg", "r")))
	{
		fprintf(stderr, "[timidity] /usr/share/timidity/timidity.cfg\n");
		strcpy(DirectoryStack[DirectoryStackIndex++], "/usr/share/timidity/");
	} else if ((inifile=fopen("/usr/local/share/timidity/timidity.cfg", "r")))
	{
		fprintf(stderr, "[timidity] /usr/local/share/timidity/timidity.cfg\n");
		strcpy(DirectoryStack[DirectoryStackIndex++], "/usr/local/share/timidity");
	} else {
		fprintf(stderr, "[timididy] failed to open /etc/timidity.cfg\n");
		return 0;
	}
	parse_config(inifile, 0);
	fclose(inifile);
	loadpatch = loadpatchTimidity;
	addpatch = addpatchTimidity;
	return 1;
}
