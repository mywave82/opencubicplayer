/* OpenCP Module Player
 * copyright (c) '94-'21 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
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
#include "filesel/filesystem.h"
#include "filesel/mdb.h"

static unsigned char gmdGetModuleType(const char *buf, const size_t len)
{
	if (len>=0x30)
	{ /* STM check */
		int i;

		/* first 20 bytes should be ASCII */
		for (i=0; i < 20; i++)
		{
			if (buf[i] & 0x80) goto nostm;
		}
		/* next bytes, is the tracker / converter software */
		for (i=0; i < 8; i++)
		{
			if (buf[i+0x14] & 0x80) goto nostm;
		}

		if ((buf[0x1c] != 0x1a) && (buf[i+0x1c] != 0x02)) goto nostm; /* signature */

		if (buf[0x1d] != 0x02) goto nostm; /* type, we only support modules */

		if (buf[0x1e] != 0x02) goto nostm; /* major */

		if ((buf[0x1f] != 10) && (buf[0x1f] != 20) && (buf[0x1f] != 21)) goto nostm; /* minor */

		if (memcmp (buf+0x14, "!Scream!", 4) && memcmp (buf+0x14, "BMOD2STM", 4) && memcmp (buf+0x14, "WUZAMOD!", 4)) goto nostm;

		return mtSTM;
	}
nostm:
	if (len>=0x60)
	{
		/* TODO, endian */
		if (!memcmp(buf+44, "SCRM", 4))
		{
			int chan_opl=0;  /* channels, OPL3 */
			int chan_nopl=0; /* channels, PCM */
			int ins_opl = 0; /* instruments, OPL3 */
			int ins_nopl = 0; /* instruments, PCM */
			uint16_t orders = uint16_little (((uint16_t *)(buf+0x20))[0]);
			uint16_t instruments = uint16_little (((uint16_t *)(buf+0x22))[0]);
			int i;
			for (i=0;i<0x20;i++)
				if (((unsigned char)buf[0x40+i]>=0x10)&&((unsigned char)buf[0x40+i]<0x20))
					chan_opl++;
				else
					if ((unsigned char)buf[0x40+i]!=0xff)
						chan_nopl++;
			for (i=0; (i<instruments) && (len >= 0x60+orders+i*2); i++)
			{
				uint16_t paraptr = uint16_little (((uint16_t *)(buf+0x60+orders))[i]);
				uint32_t offset = paraptr * 16;
				if (len > (offset+1))
				{
					switch (buf[offset])
					{
						case 0: break;
						case 1: ins_nopl++; break;
						case 2:
						case 3:
						case 4:
						case 5:
						case 6:
						case 7: ins_opl++; break;
					}
				}
			}

			if (chan_opl && ins_opl)
				return mtOPL; /* adlib sample, adplug handles these */
			if (chan_nopl && ins_nopl)
				return mtS3M;
			if (chan_nopl)
				return mtS3M;
			if (chan_opl)
				return mtOPL;
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
		case mtSTM:
			if (len > 0x1f)
			{
				memcpy (m->modname, buf, 20);
				m->modname[20] = 0;
				m->channels = 4;
				if (!memcmp (buf+0x14, "!Scream!", 4))
				{
					if (buf[0x1f] == 21)
					{
						snprintf (m->comment, sizeof (m->comment), "ScreamTracker 2.21 or later");
					} else {
						snprintf (m->comment, sizeof (m->comment), "ScreamTracker 2.%d", (unsigned char)buf[0x1f]);
					}
				} else if (!memcmp (buf+0x14, "BMOD2STM", 4))
				{
					snprintf (m->comment, sizeof (m->comment), "BMOD2STM (STM 2.%d)", (unsigned char)buf[0x1f]);
				} else if (!memcmp (buf+0x14, "WUZAMOD!", 4))
				{
					snprintf (m->comment, sizeof (m->comment), "Wuzamod (STM 2.%d)", (unsigned char)buf[0x1f]);
				}
				return 1;
			}
			break;
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

static int gmdReadInfo(struct moduleinfostruct *m, struct ocpfilehandle_t *fp, const char *buf, size_t len)
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
				uint8_t t1, t2;
				if ((fp->seek_set (fp, 48 + buf[47] * 32) == 0) &&
				    (fp->read (fp, &t1, 1) == 1) &&
				    (fp->seek_set (fp, 256 + t1 * ( (buf[14]>='4') ? 66:64)) == 0) &&
				    (fp->read (fp, &t2, 1) == 1))
				{
					m->channels = t2 + 1;
					fp->seek_set (fp, 0);
					return 1;
				}
			}
			break;

		case mtDMF:
			if (fp->seek_set (fp, 66) == 0)
			{
				m->channels=32;
				while (1)
				{
					uint32_t sig=0;
					uint32_t len=0;
					if (ocpfilehandle_read_uint32_le (fp, &sig))
					{
						break;
					}
					if (ocpfilehandle_read_uint32_le (fp, &len))
					{
						break;
					}
					if (sig == 0x54544150)
					{
						m->channels = 0;
						if (fp->seek_cur (fp, 1024) == 0)
						{
							uint8_t t;
							if (fp->read (fp, &t, 1) == 1)
							{
								m->channels = t;
							}
						}
						break;
					}
					if (fp->seek_cur (fp, len) < 0)
					{
						break;
					}
				}
				fp->seek_set (fp, 0);
				return 1;
			}
	}
	fp->seek_set (fp, 0);
	return 0;
}

struct mdbreadinforegstruct gmdReadInfoReg = {gmdReadMemInfo, gmdReadInfo, 0 MDBREADINFOREGSTRUCT_TAIL};
