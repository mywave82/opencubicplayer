/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * GMDPlay file type detection routines for the fileselector
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
 */

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "filesel/mdb.h"

/*

#define _MAX_EXT 5
static void getext(char *ext, const char *name)
{
	int i;
	name+=8;
	for (i=0; i<(_MAX_EXT-1); i++)
		if (*name==' ')
			break;
		else
			*ext++=*name++;
	*ext=0;
}*/

static unsigned char gmdGetModuleType(const char *buf, const size_t len)
{
	if (len>=0x60)
	{
		/* TODO, endian */
		if (!memcmp(buf+44, "SCRM", 4))
		{
			int opl=0;
			int nopl=0;
			int i;
			for (i=0;i<0x20;i++)
				if (((unsigned char)buf[0x40+i]>=0x10)&&((unsigned char)buf[0x40+i]<0x20))
					opl++;
				else
					if ((unsigned char)buf[0x40+i]!=0xff)
						nopl++;
			if (opl)
				return mtOPL; /* adlib sample, adplug handles these */
			if (nopl)
				return mtS3M;
		}
	}
	if (len>=48)
		if (!memcmp(buf+44, "PTMF", 4))
			return mtPTM;

	if (len>=7)
		if (!memcmp(buf, "AMShdr\x1A", 7))
			return mtAMS;

	if (len>=14)
		if (!memcmp(buf, "MAS_UTrack_V00", 14))
			return mtULT;

	if (len>=8)
		if (!memcmp(buf, "OKTASONG", 8))
			return mtOKT;

	if (len>=4)
	{
		if (!memcmp(buf, "DMDL", 4))
			return mtMDL;

		if (!memcmp(buf, "MTM\x10", 4))
			return mtMTM;

		if (!memcmp(buf, "DDMF", 4))
			return mtDMF;
	}
	if (len>=2)
		if ((!memcmp(buf, "if" /* 0x6669 */, 2))||(!memcmp(buf, "JN", 2)))
			return mt669;

	return mtUnRead;
}


static int gmdReadMemInfo(struct moduleinfostruct *m, const char *buf, size_t len)
{
	/* char ext[_MAX_EXT];*/
	int type;
	int i;

	if (!memcmp(buf, "ziRCONia", 8))
	{
		strcpy(m->modname, "MMCMPed module");
		return 0;
	}

	/* getext(ext, m.name);*/

	if ((type=gmdGetModuleType(buf, len))==mtUnRead)
		return 0;

	m->modtype=type;
	switch (type)
	{
		case mtS3M:
			if (len>=(64+32))
			{
				memcpy(m->modname, buf, 28);
				m->modname[28]=0;
				m->channels=0;
				for (i=0; i<32; i++)
					if (buf[64+i]!=(char)0xFF)
						m->channels++;
				memset(&m->composer, 0, sizeof(m->composer));
				return 1;
			}
			break;
		case mtMDL:
			if (len>=(70+32))
			{
				if (buf[4]<0x10)
				{
					m->modtype=0xFF;
					strcpy(m->modname,"MDL: too old version");
					return 0;
				}
				memcpy(m->modname, buf+11, 32);
				for (i=32; i>0; i--)
					if (m->modname[i-1]!=' ')
						break;
				if (i!=32)
					m->modname[i]=0;
				memcpy(m->composer, buf+43, 20);
				for (i=20; i>0; i--)
					if (m->composer[i-1]!=' ')
						break;
				if (i!=20)
					m->composer[i]=0;
				m->channels=0;
				for (i=0; i<32; i++)
					if (!(buf[i+70]&0x80))
						m->channels++;
				return 1;
			}
			break;
		case mtPTM:
			if (len>=39)
			{
				memcpy(m->modname, buf, 28);
				m->modname[28]=0;
				m->channels=buf[38];
				memset(&m->composer, 0, sizeof(m->composer));
				return 1;
			}
			break;
		case mtAMS:
			if (len>=9)
				if (len>=((unsigned char)buf[7])+(unsigned)8)
				{
					memcpy(m->modname, buf+8, (unsigned char)buf[7]);
					m->modname[(unsigned char)buf[7]]=0;
					memset(&m->composer, 0, sizeof(m->composer));
					return 1;
				}
			break;
		case mtMTM:
			if (len>=24)
			{
				memcpy(m->modname, buf+4, 20);
				m->modname[20]=0;
				m->channels=buf[33];
				memset(&m->composer, 0, sizeof(m->composer));
				return 1;
			}
			break;
		case mt669:
			if (len>=(2+32))
			{
				memcpy(m->modname, buf+2, 32);
				m->channels=8;
				memset(&m->composer, 0, sizeof(m->composer));
				return 1;
			}
			break;
		case mtOKT:
			if (len>=24)
			{
				m->channels=4+(buf[17]&1)+(buf[19]&1)+(buf[21]&1)+(buf[23]&1);
				memset(&m->modname, 0, sizeof(m->modname));
				memset(&m->composer, 0, sizeof(m->composer));
				return 1;
			}
			break;
		case mtULT:
			/* these seems like.. very  broken */
			if (len>=(15+32))
			{
				m->modtype=0xFF;
				memcpy(m->modname, buf+15, 32);
				memset(&m->composer, 0, sizeof(m->composer));
				return 0;
			}
			break;
		case mtDMF:
			if (len>=(43+20))
			{
				m->modtype=0xFF;
				memcpy(m->modname, buf+13, 30);
				m->modname[30]=0;
				memcpy(m->composer, buf+43, 20);
				m->composer[20]=0;
				m->date=uint32_little(*(uint32_t *)(buf+63))&0xFFFFFF;
				return 0;
			}
			break;
	}
	/* if we reach this point, the file is broken in length... */
	return 0;
}

static int gmdReadInfo(struct moduleinfostruct *m, FILE *fp, const char *buf, size_t len)
{
	/* char ext[_MAX_EXT];*/
	int type;
	/* getext(ext, m->gen.name);*/

	if ((type=gmdGetModuleType(buf, len))==mtUnRead)
		return 0;

	m->modtype=type;
	switch (type)
	{
		case mtULT:
			if (len>=(48))
			{
				fseek(fp, 48+buf[47]*32, SEEK_SET);
				fseek(fp, 256+fgetc(fp)*((buf[14]>='4')?66:64), SEEK_CUR);
				m->channels=fgetc(fp)+1;
				return 1;
			}
			break;
		case mtDMF:
			fseek(fp, 66, SEEK_SET);
			m->channels=32;
			while (1)
			{
				uint32_t sig=0;
				uint32_t len=0;
				if (!fread(&sig, 4, 1, fp))
					break;
				if (!fread(&len, 4, 1, fp))
					break;
				len = uint32_little (len);
				if (sig==uint32_little(0x54544150))
				{
					char buffer[1024];
					m->channels = 0;
					if (fgets(buffer, 1024, fp))
					{
						int res=fgetc(fp);
						if (res!=EOF)
							m->channels=res;
					}
					break;
				}
				fseek(fp, len, SEEK_CUR);
			}
			return 1;
	}
	return 0;
}

struct mdbreadinforegstruct gmdReadInfoReg = {gmdReadMemInfo, gmdReadInfo, 0 MDBREADINFOREGSTRUCT_TAIL};
