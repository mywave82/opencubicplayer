/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2007-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * MPPlay file type detection routines for fileselector
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
 *  -ryg990615  Fabian Giesen  <fabian@jdcs.su.nw.schule.de>
 *    -99% faked VBR detection.. and I still don't really know why it
 *     works, but... who cares?
 */

#include "config.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include "types.h"
#include "boot/plinkman.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "id3.h"
#include "stuff/imsrtns.h"
#include "stuff/utf-8.h"
#include "stuff/err.h"
#include "mptype.h"

#define VBRFRAMES      15

static void apply_ID3(struct moduleinfostruct *m, struct ID3_t *tag)
{
	if (tag->TIT2) snprintf (m->title, sizeof (m->title), "%s", (char *)tag->TIT2);
	if (tag->TPE2)
	{
		snprintf (m->artist, sizeof (m->artist), "%s", (char *)tag->TPE2);
	} else if (tag->TPE1)
	{
		 snprintf (m->artist, sizeof (m->artist), "%s", (char *)tag->TPE1);
	}
	if (tag->TALB) snprintf (m->album, sizeof (m->album), "%s", (char *)tag->TALB);
	if (tag->TCOM) snprintf (m->composer, sizeof (m->composer), "%s", (char *)tag->TCOM);

	if (tag->COMM) snprintf (m->comment, sizeof (m->comment), "%s", (char *)tag->COMM);

	m->date = 0;
	if (tag->TYER) m->date =atoi((char *)tag->TYER) << 16;
	if (tag->TDAT) m->date|=atoi((char *)tag->TDAT);
}

static void parseid3v1(struct moduleinfostruct *m, const uint8_t *tag)
{
	struct ID3v1data_t data;
	struct ID3_t dest = {0};

	if (parse_ID3v1x (&data, tag, 128)) return;
	if (finalize_ID3v1 (&dest, &data)) return;

	apply_ID3(m, &dest);

	ID3_clear(&dest);
}

static void parseid3v12(struct moduleinfostruct *m, const unsigned char *tag)
{
	struct ID3v1data_t data;
	struct ID3_t dest = {0};

	if (parse_ID3v1x (&data, tag+128, 128)) return;
	if (parse_ID3v12 (&data, tag    , 128)) return;
	if (finalize_ID3v1 (&dest, &data)) return;

	apply_ID3(m, &dest);

	ID3_clear(&dest);
}

static int parseid3v2(struct moduleinfostruct *m, uint8_t *tag, uint32_t len)
{
	struct ID3_t dest = {0};

	if (parse_ID3v2x (&dest, tag, len) < 0) return -1;

	apply_ID3(m, &dest);

	ID3_clear(&dest);

	return 0;
}

static int freqtab[3][3]=
{
	{44100, 48000, 32000}, {22050, 24000, 16000}, {11025, 12000, 8000}
};

static uint16_t fetch16(const uint8_t *buf)
{
	return buf[0]|(buf[1]<<8);
}

static uint32_t fetch32(const uint8_t *buf)
{
	return buf[0]|(buf[1]<<8)|(buf[2]<<16)|(buf[3]<<24);
}

static int ampegpReadInfo(struct moduleinfostruct *m, struct ocpfilehandle_t *f, const char *_buf, size_t len, const struct mdbReadInfoAPI_t *API)
{
	int64_t relstart = 0; /* offset */
	const uint8_t *buf = (const uint8_t *)_buf;
	const uint8_t *bufend=buf+len;
	uint32_t hdr;
	int layer;
	int ver;
	int rateidx;
	int frqidx;
	int padding;
	int stereo;
	int rate;
	int br, lastbr;
	int temp;
	const char *filename = 0;
	int filenamelen;

	API->dirdb->GetName_internalstr(f->dirdb_ref, &filename);
	if (!filename)
	{
		return 0;
	}
	filenamelen = strlen (filename);
	if (!((filenamelen >= 4) && ((!strcasecmp (filename + filenamelen - 4, ".mp1")) ||
	                             (!strcasecmp (filename + filenamelen - 4, ".mp2")) ||
	                             (!strcasecmp (filename + filenamelen - 4, ".mp3")) ||
	                             (!strcasecmp (filename + filenamelen - 4, ".mpg")))))
	{ /* only test files that can be MP2/MP3 files since the routine often "steals" adplug, DMF files etc. */
		return 0;
	}

	/* First, try to detect if we have an mpeg stream embedded into a riff/wave container. Often used on layer II files.
	 * This should fit inside the provided buf/len data
	 * */
	                               /* RIFF */                    /* WAVE */                     /* fmt  */
	if ((len > 22)&&(fetch32(buf)==0x46464952)&&(fetch32(buf+8)==0x45564157)&&(fetch32(buf+12)==0x20746D66)&&(fetch16(buf+20)==0x0055))
	{
		int i;
		i=20;
		while (i<800)
		{
			/* avoid buffer overflow */
			if ((buf+i)>=bufend)
				return 0;
			                      /* data */
			if (fetch32(buf+i-8)==0x61746164)
				break;
			i+=8+uint32_little(*(uint32_t*)(buf+i-4));
		}
		if (i>=800) /* nothing found */
			return 0;
		buf+=i;
		relstart=i;
	}

	while ((buf+3)<bufend)
	{
		/* most common is ID3v2 tag at the start of the file, and ID3v1 at the end - we want the ID3v2 tag */
		if ((buf[0]=='I')&&(buf[1]=='D')&&(buf[2]=='3'))
		{
			uint_fast32_t realsize;
			uint8_t tagv2header[10];
			uint8_t *buffer = 0;
			uint8_t *tagv2data;
			m->modtype.integer.i=MODULETYPE("MPx");
			if ((buf+10)>=bufend)
			{
				if (f->seek_set (f, relstart) < 0)
				{
					break;
				}
				if (f->read (f, tagv2header, 10) != 10)
				{
					break;
				}
			} else {
				memcpy(tagv2header, buf, 10);
			}
			if ((tagv2header[6] & 0x80) ||
			    (tagv2header[7] & 0x80) ||
			    (tagv2header[8] & 0x80) ||
			    (tagv2header[9] & 0x80))
			{
				break;
			}
			realsize = (tagv2header[6]<<21)|(tagv2header[7]<<14)|(tagv2header[8]<<7)|(tagv2header[9]);
			if (realsize > 32*1024*1024)
			{
				f->seek_set (f, 0);
				return 1;
			}
			if ((buf+10+realsize)>bufend)
			{
				tagv2data = buffer = malloc(realsize+10);
				if (!buffer)
					break;
				if (f->seek_set (f, relstart) < 0)
				{
					break;
				}
				if (f->read (f, buffer, realsize + 10) != (realsize + 10))
				{
					break;
				}
			} else {
					tagv2data = (uint8_t *)buf;
			}
			parseid3v2(m, tagv2data, realsize + 10);
			if (buffer)
				free(buffer);
			/*
			buf+=10+realsize;
			relstart+=10+realsize;
			continue;
			*/
			f->seek_set (f, 0);
			return 1;
		/* ID3v1 tag at the start of the file is bit uncommon */
		} else if ((buf[0]=='T')&&(buf[1]=='A')&&(buf[2]=='G'))
		{
			uint8_t id3v1[128];
			m->modtype.integer.i=MODULETYPE("MPx");

			if ((buf+128)>bufend)
			{
				if (f->seek_set (f, relstart) < 0)
				{
					break;
				}
				if (f->read (f, id3v1, 128) != 128)
				{
					break;
				}
			} else {
				memcpy(id3v1, buf, 128);
			}
			parseid3v1(m, id3v1);
			/*
			buf+=128;
			relstart+=128;
			continue;
			*/
			f->seek_set (f, 0);
			return 1;
		} else
			break;
	}

	f->seek_end (f, 0);
	m->title[0] = 0;
	while (1)
	{
		/* test for ID3v1.0/ID3v1.1 and ID3v1.2 */
		{
			uint8_t id3v1[256];
			f->seek_cur (f, -256);
			if (f->read (f, id3v1, 256) != 256)
			{
				f->seek_set (f, 0);
				return 0;
			}
			if ((id3v1[128]=='T')&&(id3v1[129]=='A')&&(id3v1[130]=='G'))
			{
				m->modtype.integer.i=MODULETYPE("MPx");
				if ((id3v1[0]=='E')&&(id3v1[1]=='X')&&(id3v1[2]=='T'))
				{
					f->seek_cur (f, -256);
					parseid3v12(m, id3v1);
				} else {
					f->seek_cur (f, -128);
					parseid3v1(m, id3v1+128);
				}
				/*gottag++;*/
				continue;
			}
		}
		/* test for ID3v2.x */
		{
			uint8_t id3v2header[10];
			f->seek_cur (f, -10);
			if (f->read (f, id3v2header, 10) != 10)
			{
				f->seek_set (f, 0);
				return 0;
			}
			/* test for ID3v2.4 footer first */
			if ((id3v2header[0]=='3')&&(id3v2header[1]=='D')&&(id3v2header[2]=='I')&&(id3v2header[3]!=0xff)&&(id3v2header[4]!=0xff))
			{
				uint8_t *id3v2data;
				uint_fast32_t size = (id3v2header[6]<<21)|(id3v2header[7]<<14)|(id3v2header[8]<<7)|(id3v2header[9]);

				m->modtype.integer.i=MODULETYPE("MPx");

				f->seek_cur (f, -(size+20));

				id3v2data = malloc(size+10);
				if (f->read (f, id3v2data, size+10) == (size + 10))
				{
					parseid3v2(m, id3v2data, size+10);
				}
				free(id3v2data);

				/*fseeko(f, -(size+20), SEEK_CUR);*/
				/*continue;*/

				f->seek_set (f, 0);
				return 1;
			}
			/* search for ID3v2.x */
			{
				uint8_t *buffer = calloc(65536+4096, 1); /* 4k for for miss-designed tag paddings */
				f->seek_cur (f, -65536);
				if (f->read (f, buffer, 65536) == 65536)
				{
					uint8_t *curr = buffer;
					uint8_t *next;
					while ((next = memmem (curr, 65536 - (curr - buffer), "ID3", 3)))
					{
						curr = next + 1;
						if ((next[3] != 0xff) &&
						    (next[4] != 0xff) &&
						    (next[5] != 0xff) &&
						  (!(next[6] & 0x80)) &&
						  (!(next[7] & 0x80)) &&
						  (!(next[8] & 0x80)) &&
						  (!(next[9] & 0x80)))
						{
							uint32_t size = (next[6] << 21) |
							                (next[7] << 14) |
							                (next[8] <<  7) |
							                 next[9];
							if (size < 65536 + 4096 - (next - buffer))
							{
								if (!parseid3v2(m, next, size + 10))
								{
									free (buffer);
									f->seek_set (f, 0);
									return 1;
								}
							}
						}
					}
				}
				free (buffer);
			}
		}
		break;
	}
	if (m->title[0])
	{
		f->seek_set (f, 0);
		return 0;
	}
	/*fseeko(f, relstart, SEEK_SET);*/

	/* no meta-data, so make up some */

	if ((buf+sizeof(uint32_t))>=bufend)
	{
		f->seek_set (f, 0);
		return 0;
	}
	while ((fetch16(buf)&0xE0FF)!=0xE0FF)
	{
		buf+=1;
		if ((buf+sizeof(uint32_t))>=bufend)
		{
			f->seek_set (f, 0);
			return 0;
		}
	}

	hdr=fetch32(buf);
	layer=4-((hdr>>9)&3);
	if (layer>=4)
	{
		f->seek_set (f, 0);
		return 0;
	}
	ver=((hdr>>11)&1)?0:1;
	if (!((hdr>>12)&1))
	{
		if (ver)
		{
			ver=2;
		} else {
			f->seek_set (f, 0);
			return 0;
		}
	}
	if ((ver==2)&&(layer!=3))
	{
		f->seek_set (f, 0);
		return 0;
	}
	rateidx=(hdr>>20)&15;
	frqidx=(hdr>>18)&3;
	padding=(hdr>>17)&1;
	stereo="\x01\x01\x02\x00"[(hdr>>30)&3];
	if (frqidx==3)
	{
		f->seek_set (f, 0);
		return 0;
	}
	if (!ver)
	{
		switch (layer)
		{
			case 1:
				rate="\x00\x04\x08\x0C\x10\x14\x18\x1C\x20\x24\x28\x2C\x30\x34\x38\x00"[rateidx]*8;
				break;
			case 2:
				rate="\x00\x04\x06\x07\x08\x0A\x0C\x0E\x10\x14\x18\x1C\x20\x28\x30\x00"[rateidx]*8;
				break;
			case 3:
				rate="\x00\x04\x05\x06\x07\x08\x0A\x0C\x0E\x10\x14\x18\x1C\x20\x28\x00"[rateidx]*8;
				break;
			default:
				f->seek_set (f, 0);
				return 0;
		}
	} else {
		switch (layer)
		{
			case 1:
				rate="\x00\x04\x06\x07\x08\x0A\x0C\x0E\x10\x12\x14\x16\x18\x1C\x20\x00"[rateidx]*8;
				break;
			case 2:
				rate="\x00\x01\x02\x03\x04\x05\x06\x07\x08\x0A\x0C\x0E\x10\x12\x14\x00"[rateidx]*8;
				break;
			case 3:
				rate="\x00\x01\x02\x03\x04\x05\x06\x07\x08\x0A\x0C\x0E\x10\x12\x14\x00"[rateidx]*8;
				break;
			default:
				f->seek_set (f, 0);
				return 0;
		}
	}

	if (!rate)
	{
		f->seek_set (f, 0);
		return 0;
	}

	m->title[0]=0;
	switch (layer)
	{
		case 1:
			strcat(m->title, "Layer   I, ");
			break;
		case 2:
			strcat(m->title, "Layer  II, ");
			break;
		case 3:
			strcat(m->title, "Layer III, ");
			break;
	}
	switch (ver)
	{
		case 0:
			switch (frqidx)
			{
				case 0:
					strcat(m->title, "44100 Hz, ");
					break;
				case 1:
					strcat(m->title, "48000 Hz, ");
					break;
				case 2:
					strcat(m->title, "32000 Hz, ");
					break;
			}
			break;
		case 1:
			switch (frqidx)
			{
				case 0:
					strcat(m->title, "22050 Hz, ");
					break;
				case 1:
					strcat(m->title, "24000 Hz, ");
					break;
				case 2:
					strcat(m->title, "16000 Hz, ");
					break;
			}
			break;
		case 2:
			switch (frqidx)
			{
				case 0:
					strcat(m->title, "11025 Hz, ");
					break;
				case 1:
					strcat(m->title, "12000 Hz, ");
					break;
				case 2:
					strcat(m->title, " 8000 Hz, ");
					break;
			}
			break;
	}

	br=rate;
	lastbr=rate;

	for (temp=0; temp<VBRFRAMES; temp++)
	{
		int skip;
		uint32_t hdr;

		switch (layer)
		{
			case 1:
				skip=umuldiv(br, 12000, freqtab[ver][frqidx])+(padding<<2);
			case 2:
				skip=umuldiv(br, 144000, freqtab[ver][frqidx])+padding;
			default:
			case 3:
				skip=umuldiv(br, 144000, freqtab[ver][frqidx])+padding;
		}
		buf+=skip;

		if ((buf+sizeof(uint32_t))>=bufend)
			break;

		while ((fetch16(buf)&0xE0FF)!=0xE0FF)
		{
			buf+=1;
			if ((buf+sizeof(uint32_t))>=bufend)
				goto outofframes; /* we can't break two levels */
		}

		hdr=fetch32(buf);
		layer=4-((hdr>>9)&3);
		if (layer==4)
			break;
		ver=((hdr>>11)&1)?0:1;
		if (!((hdr>>12)&1))
		{
			if (ver)
				ver=2;
			else
				break;
		}
		if ((ver==2)&&(layer!=3))
			break;
		frqidx=(hdr>>18)&3;
		padding=(hdr>>17)&1;
		stereo="\x01\x01\x02\x00"[(hdr>>30)&3];
		if (frqidx==3)
			break;

		lastbr=br;
		br=(hdr>>20)&15;

		if (!ver)
			switch (layer)
			{
				case 1:
					br="\x00\x04\x08\x0C\x10\x14\x18\x1C\x20\x24\x28\x2C\x30\x34\x38\x00"[br]*8;
					break;
				case 2:
					br="\x00\x04\x06\x07\x08\x0A\x0C\x0E\x10\x14\x18\x1C\x20\x28\x30\x00"[br]*8;
					break;
				case 3:
					br="\x00\x04\x05\x06\x07\x08\x0A\x0C\x0E\x10\x14\x18\x1C\x20\x28\x00"[br]*8;
					break;
			} else switch (layer)
			{
				case 1:
					br="\x00\x04\x06\x07\x08\x0A\x0C\x0E\x10\x12\x14\x16\x18\x1C\x20\x00"[br]*8;
					break;
				case 2:
					br="\x00\x01\x02\x03\x04\x05\x06\x07\x08\x0A\x0C\x0E\x10\x12\x14\x00"[br]*8;
					break;
				case 3:
					br="\x00\x01\x02\x03\x04\x05\x06\x07\x08\x0A\x0C\x0E\x10\x12\x14\x00"[br]*8;
					break;
			}

		if ((lastbr!=br) && temp) /* first frame might be TAG */
			break;
	}
outofframes:

	if (lastbr==br)
	{
		if (rate<100)
			strcat(m->title, " ");
		if (rate<10)
			strcat(m->title, " ");

		sprintf(m->title+strlen(m->title), "%d", rate);

		strcat(m->title, " kbps");
		m->playtime=m->size/(rate*125);
	} else {
		strcat(m->title, "VBR");
		m->playtime=0; /* unknown */
	}

	m->channels=stereo?2:1;
	m->modtype.integer.i=MODULETYPE("MPx");
	f->seek_set (f, 0);
	return 0;
}

static const char *MPx_description[] =
{
	//                                                                          |
	"MP2/MP3 files are mpeg II Layer 2/3 audio files. This is a lossy, audio",
	"compressed file. Open Cubic Player uses libmad for decoding of these.",
	NULL
};

static struct mdbreadinforegstruct ampegpReadInfoReg = {"MPx", ampegpReadInfo MDBREADINFOREGSTRUCT_TAIL};

int __attribute__ ((visibility ("internal"))) ampeg_type_init (struct PluginInitAPI_t *API)
{
	struct moduletype mt;

	API->fsRegisterExt ("MP1");
	API->fsRegisterExt ("MP2");
	API->fsRegisterExt ("MP3");

	mt.integer.i = MODULETYPE("MPx");
	API->fsTypeRegister (mt, MPx_description, "plOpenCP", &mpegPlayer);

	API->mdbRegisterReadInfo(&ampegpReadInfoReg);

	return errOk;
}

void __attribute__ ((visibility ("internal"))) ampeg_type_done (struct PluginCloseAPI_t *API)
{
	struct moduletype mt;

	mt.integer.i = MODULETYPE("MPx");
	API->fsTypeUnregister (mt);

	API->mdbUnregisterReadInfo(&ampegpReadInfoReg);
}
