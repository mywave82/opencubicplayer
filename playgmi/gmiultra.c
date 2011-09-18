/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * GMIPlay ULTRASND.INI PAT format handler (reads GUS patches etc)
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
#include "stuff/compat.h"
#include "stuff/err.h"

static char midInstrumentPath[PATH_MAX+1];

static int loadpatchUltra(struct minstrument *ins,
                          uint8_t             program,
                          uint8_t            *sampused,
                          struct sampleinfo **smps,
                          uint16_t           *samplenum)
{
	int retval;
	ins->sampnum=0;
	*ins->name=0;
	{
		FILE *file;
		char path[PATH_MAX+NAME_MAX];
		snprintf(path, sizeof(path), "%s%s", midInstrumentPath, midInstrumentNames[program]);
		if ((file=fopen(path, "r"))==NULL)
		{
			fprintf(stderr, "[ultradir] '%s': %s\n", path, strerror(errno));
			return errFileMiss;
		}
		fprintf(stderr, "[ultradir] loading %s\n", path);
		retval=loadpatchPAT(file, ins, program, sampused, smps, samplenum);
		fclose(file);
	}
	if (retval)
		fprintf(stderr, "[ultradir] Invalid PAT file\n");
	return retval;
}

static int addpatchUltra( struct minstrument *ins,
                          uint8_t             program,
                          uint8_t             sn,
                          uint8_t             sampnum,
                          struct sampleinfo  *sip,
                          uint16_t           *samplenum)
{
	int retval;
	{
		FILE *file;
		char path[PATH_MAX+NAME_MAX];

		snprintf(path, sizeof(path), "%s%s", midInstrumentPath, midInstrumentNames[program]);
		if ((file=fopen(path, "r"))==NULL)
		{
			fprintf(stderr, "[ultradir] '%s': %s\n", path, strerror(errno));
			return errFileMiss;
		}
		fprintf(stderr, "[ultradir] loading %s\n", path);
		retval=addpatchPAT(file, ins, program, sn, sampnum, sip, samplenum);
		fclose(file);
	}
	if (retval)
		fprintf(stderr, "[ultradir] Invalid PAT file\n");
	return retval;
}

int __attribute__ ((visibility ("internal"))) midInitUltra(void)
{
	FILE *inifile;
	int i;
	char path[PATH_MAX+1];
	char *buf;
	char *bp;
	char *bp2;
	char type;
	long len;
	const char *ipath;

	_midClose=0;

	if (!(ipath=getenv("ULTRADIR")))
		ipath=cfGetProfileString("midi", "ultradir", "");

	for (i=0; i<256; i++)
		midInstrumentNames[i][0]=0;

	snprintf(midInstrumentPath, sizeof(midInstrumentPath), "%s%s", ipath, strlen(ipath)?(ipath[strlen(ipath)-1]=='/'?"":"/"):"/");

	snprintf(path, sizeof(path), "%s%s", midInstrumentPath, "ULTRASND.INI");
	if ((inifile=fopen(path, "r")))
	{
		fprintf(stderr, "[ultradir] parsing %s\n", path);
		goto got_ultrasound_ini;
	}

	fprintf(stderr, "[ultradir] failed to locate ULTRASND.INI\n");
	return 0;

got_ultrasound_ini:

	fseek(inifile, 0, SEEK_END);
	len=ftell(inifile);
	fseek(inifile, 0, SEEK_SET);

	if (!(buf=calloc(len+1, 1)))
	{
		fprintf(stderr, "[ultradir] calloc() failed\n");
		return 0;
	}
	if (fread(buf, len, 1, inifile) != 1)
	{
		fprintf(stderr, "[ultradir] fread() failed\n");
		free(buf);
		return 0;
	}
	buf[len]=0;
	fclose(inifile);

	*path=0;

	bp=buf;
	type=0;

	while (1)
	{
		int insnum;

		while (isspace(*bp))
			bp++;
		if (!*bp)
			break;

		if (*bp=='[')
		{
			if (!memicmp(bp, "[Melodic Bank 0]", 16))
				type=1;
			else
				if (!memicmp(bp, "[Drum Bank 0]", 13))
					type=2;
				else
					type=0;
		}

		if (!memicmp(bp, "PatchDir", 8))
		{
			while ((*bp!='=')&&*bp)
				bp++;
			if (*bp)
				bp++;
			while ((*bp==' ')||(*bp=='\t'))
				bp++;
			bp2=bp;
			while (!isspace(*bp2)&&*bp2)
				bp2++;
			memcpy(path, bp, bp2-bp);
			path[bp2-bp]=0;
			if (path[strlen(path)-1]!='/')
				strcat(path, "/");
		}

		if (!isdigit(*bp)||!type)
		{
			while (*bp&&(*bp!='\r')&&(*bp!='\n'))
				bp++;
			continue;
		}

		insnum=strtoul(bp, 0, 10)+((type==2)?128:0);
		while ((*bp!='=')&&*bp)
			bp++;
		if (*bp)
			bp++;
		while ((*bp==' ')||(*bp=='\t'))
			bp++;
		bp2=bp;
		while (!isspace(*bp2)&&*bp2)
			bp2++;

		if (insnum<256)
		{
			char *x=midInstrumentNames[insnum];
			strcpy(x+(bp2-bp), ".PAT");
			memcpy(x, bp, bp2-bp);
		}

		while (*bp&&(*bp!='\r')&&(*bp!='\n'))
			bp++;

	}
	free(buf);

	loadpatch = loadpatchUltra;
	addpatch = addpatchUltra;
	return 1;
}
