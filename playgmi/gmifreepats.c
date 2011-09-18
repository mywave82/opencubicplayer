/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * GMIPlay freepats PAT format handler (reads GUS patches etc)
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
 *  -sss050411 Stian Skjelstad <stian@nixia.no>
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
#include "boot/psetting.h"
#include "stuff/err.h"

static char fpdir[PATH_MAX+1];

static void parse_config(FILE *input)
{
	char line[1024], *pos, *base;
	int type=0;
	while (fgets(line, sizeof(line), input))
	{
/*
		if ((pos=strchr(line, '\r')))
			*pos=0;
		if ((pos=strchr(line, '\n')))
			*pos=0;
*/
		if ((pos=strchr(line, '#')))
			*pos=0;
		base=line;
		while ((*base)&&(*base==' '))
			base++;
		if (!base)
			continue;
		if (!strncmp(base, "drumset ", 8))
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

static int loadpatchFreePats( struct minstrument *ins,
                              uint8_t             program,
                              uint8_t            *sampused,
                              struct sampleinfo **smps,
                              uint16_t           *samplenum)
{
	FILE *file;
	int retval;
	char path[PATH_MAX+NAME_MAX+1];

	ins->sampnum=0;
	*ins->name=0;

	if (!*midInstrumentNames[program])
	{
		fprintf(stderr, "[freepats] not entry configured for program %d\n", program);
		return errFileMiss;
	}

	snprintf(path, sizeof(path), "%s%s", fpdir, midInstrumentNames[program]);
	if ((file=fopen(path, "r"))==NULL)
	{
		fprintf(stderr, "[freepats] '%s': %s\n", path, strerror(errno));
		return errFileMiss;
	}

	fprintf(stderr, "[freepats] loading file %s\n", path);
	retval=loadpatchPAT(file, ins, program, sampused, smps, samplenum);
	fclose(file);
	if (retval)
		fprintf(stderr, "Invalid PAT file\n");
	return retval;
}

static int addpatchFreePats( struct minstrument *ins,
                             uint8_t             program,
                             uint8_t             sn,
                             uint8_t             sampnum,
                             struct sampleinfo  *sip,
                             uint16_t           *samplenum)
{
	FILE *file;
	int retval;
	char path[PATH_MAX+NAME_MAX];

	if (!*midInstrumentNames[program])
	{
		fprintf(stderr, "[freepats] not entry configured for program %d\n", program);
		return errFileMiss;
	}
	snprintf(path, sizeof(path), "%s%s", fpdir, midInstrumentNames[program]);
	if ((file=fopen(path, "r"))==NULL)
	{
		fprintf(stderr, "[freepats] '%s': %s\n", path, strerror(errno));
		return errFileMiss;
	}

	fprintf(stderr, "[freepats] loading file %s\n", path);
	retval=addpatchPAT(file, ins, program, sn, sampnum, sip, samplenum);
	fclose(file);
	if (retval)
		fprintf(stderr, "Invalid PAT file\n");
	return retval;
}

int __attribute__ ((visibility ("internal"))) midInitFreePats(void)
{
	FILE *inifile0, *inifile1;
	char path[PATH_MAX+1];
	const char *_fpdir;
	int i;

	_midClose=0;

	for (i=0; i<256; i++)
		midInstrumentNames[i][0]=0;

	if (!(_fpdir=cfGetProfileString("midi", "freepats", 0)))
		return 0;
	if (!strlen(_fpdir))
		return 0;
	snprintf(fpdir, sizeof(fpdir), "%s%s", _fpdir, fpdir[strlen(fpdir)-1]=='/'?"":"/");
	snprintf(path, sizeof(path), "%s%s", fpdir, "freepats.cfg");
	if ((inifile1=fopen(path, "r")))
	{
		fprintf(stderr, "[freepats] Loading %s\n", path);
		snprintf(path, sizeof(path), "%s%s", fpdir, "crude.cfg");
		if ((inifile0=(fopen(path, "r"))))
		{
			fprintf(stderr, "[freepats] Loading %s\n", path);
			parse_config(inifile0);
			fclose(inifile0);
		}
		parse_config(inifile1);
		fclose(inifile1);
	} else {
		fprintf(stderr, "[freepats] '%s': %s\n", path, strerror(errno));
		return 0;
	}
	loadpatch = loadpatchFreePats;
	addpatch = addpatchFreePats;
	return 1;
}
