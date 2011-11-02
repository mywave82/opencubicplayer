/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
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
#include "stuff/imsrtns.h"
#include "filesel/mdb.h"
#include "charset.h"

#ifdef ID3_DEBUG
# define PRINT(fmt, args...) fprintf(stderr, fmt, ##args)
#else
# define PRINT(a, ...) do {} while(0)
#endif


#define VBRFRAMES      15

static char *mpstyles[]=
{
	"Blues", "Classic Rock", "Country", "Dance", "Disco", "Funk", "Grunge",
	"Hip-Hop", "Jazz", "Metal", "New Age", "Oldies", "Other", "Pop", "R&B",
	"Rap", "Reggae", "Rock", "Techno", "Industrial", "Alternative", "Ska",
	"Death Metal", "Pranks", "SoundTrack", "Euro-Techno", "Ambient", "Trip-Hop",
	"Vocal", "Jazz Funk", "Fusion", "Trance", "Classical", "Instrumental",
	"Acid", "House", "Game", "Sound Clip", "Gospel", "Noise", "Altern Rock",
	"Bass", "Soul"
};

#ifdef ID3_DEBUG
static const char *hex = "0123456789abcdef";
#endif

static void handle_T__(const uint8_t *p, uint_fast32_t size, char *target, int targetsize)
{
#ifdef ID3_DEBUG
	char *temp_p = malloc((size-1)*4+1);
	int i, j=0;
	for (i=1;i<size;i++)
	{
		if ((p[i]>=32)||(p[i]<=127))
			temp_p[j++]=p[i];
		else {
			temp_p[j++]='\\';
			temp_p[j++]='x';
			temp_p[j++]=hex[p[i]>>4];
			temp_p[j++]=hex[p[i]&15];
		}
	}
	temp_p[j++]=0;
	PRINT ("handle_T__(*p=(charset=%d;data=\"%s\"), size=%d, target=%p, targetsize=%d\n", p[0], temp_p, size, target, targetsize);
	free(temp_p);
#endif
	target[0]=0;
	if (size<1)
		return;
	if (p[0]>=MAX_CHARSET_N)
		return; /* invalid / unknown charset */
	id3v2_charsets[p[0]].readstring(p+1, size-1, target, targetsize);
}

static void handle_T___(const uint8_t *p, uint_fast32_t size, char *target, int targetsize)
{
#ifdef ID3_DEBUG
	char *temp_p = malloc((size-1)*4+1);
	int i, j=0;
	for (i=1;i<size;i++)
	{
		if ((p[i]>=32)||(p[i]<=127))
			temp_p[j++]=p[i];
		else {
			temp_p[j++]='\\';
			temp_p[j++]='x';
			temp_p[j++]=hex[p[i]>>4];
			temp_p[j++]=hex[p[i]&15];
		}
	}
	temp_p[j++]=0;
	PRINT("handle_T___(*p=(charset=%d;data=\"%s\"), size=%d, target=%p, targetsize=%d) ", p[0], temp_p, size, target, targetsize);
	PRINT("forwarding to handle_T__\n");
	free(temp_p);
#endif
	handle_T__(p, size, target, targetsize);
}
static void parseid3v1(struct moduleinfostruct *m, const unsigned char *tag)
{
#ifdef ID3_DEBUG
	char temp_tag[128*4+1];
	int i, j=0;
	for (i=1;i<128;i++)
	{
		if ((tag[i]>=32)||(tag[i]<=127))
			temp_tag[j++]=tag[i];
		else {
			temp_tag[j++]='\\';
			temp_tag[j++]='x';
			temp_tag[j++]=hex[tag[i]>>4];
			temp_tag[j++]=hex[tag[i]&15];
		}
	}
	temp_tag[j++]=0;
	PRINT("parseid3v1(m=%p, tag=%s) ", m, temp_tag);
#endif
	if (!memcmp(tag, "TAG", 3))
	{
		if (memcmp(tag+3, "                              ", 30))
		{
			/* charset 0 is iso8859-1 in id3v2 */
			id3v2_charsets[0].readstring(tag+3, 30, m->modname, sizeof(m->modname));
			/*memcpy(m->modname, tag+3, 30);
			m->modname[30]=0;
			while (strlen(m->modname)&&(m->modname[strlen(m->modname)-1]==' '))
				m->modname[strlen(m->modname)-1]=0;*/
			PRINT("m->modname=\"%s\"\n", m->modname);
		}
		if (memcmp(tag+33, "                              ", 30))
		{
			/* charset 0 is iso8859-1 in id3v2 */
			id3v2_charsets[0].readstring(tag+33, 30, m->composer, sizeof(m->composer));
			/*memcpy(m->composer, tag+33, 30);
			m->composer[30]=0;
			while (strlen(m->composer)&&(m->composer[strlen(m->composer)-1]==' '))
				m->composer[strlen(m->composer)-1]=0;*/
			PRINT("m->composer=\"%s\"\n", m->composer);
		}
		if (memcmp(tag+63, "                              ", 30)||memcmp(tag+97, "                              ", 30))
		{
			/* TODO, charset */
			memcpy(m->comment, tag+63, 30);
			m->comment[30]=' ';
			m->comment[31]=' ';
			memcpy(m->comment+32, tag+97, 30);
			m->comment[62]=0;
			while (strlen(m->comment)&&(m->comment[strlen(m->comment)-1]==' '))
				m->comment[strlen(m->comment)-1]=0;
			PRINT("m->comment=\"%s\"\n", m->comment);
		}
		if (tag[127]<43)
		{
			strcpy(m->style, mpstyles[tag[127]]);
			PRINT("m->style=\"%s\"\n", m->style);
		}
		if (memcmp(tag+93, "    ", 4))
		{
			char i[5];
			memcpy(i, tag+93, 4);
			i[4]=0;
			m->date=atoi(i)<<16;
			PRINT("m->date=%d\n", m->date);
		}
	}
}

static inline uint_fast32_t unsync(uint8_t *src, uint_fast32_t orgsize)
{
	uint_fast32_t retval = 0;
	uint8_t *dst, *end = src+orgsize-1;
	PRINT("unsync(src=%p, orgsize=%d) = ", src, orgsize);
	if (!orgsize)
	{
		PRINT("0\n");
		return 0;
	}

	for (dst=src;(src+1)<end;src++)
	{
		if ( (*src==0xff) && (*(src+1)==0x00) )
		{
			*(dst++)=0xff;
			src++;
		} else
			*(dst++)=*src;
		retval++;
	}
	*(dst++)=*src;
	retval++;

	PRINT("%d\n", retval);
	return retval;
}

static int parseid3v20(struct moduleinfostruct *m, const uint8_t *id3v2header, uint8_t *id3v2data, uint_fast32_t len)
{
	PRINT("parseid3v20(m, id3v2header = %p (not used), id3v3data = %p, len = %d\n", id3v2header, id3v2data, len);
	while (1)
	{
		uint_fast32_t size;
		PRINT("// parseid3v20, iterate, len = %d\n", len);
		if (len<1)
		{
			PRINT("// parseid3v20 len<1, return\n");
			return 1; /* out of data */
		}
		if (id3v2data[0]==0)
		{
			PRINT("// parseid3v20 padding reached, return\n");
			return 0; /* padding reached */
		}
		if (len<6)
		{
			PRINT("// parseid3v20 len<6, return\n");
			return 1; /* out of data */
		}
		size = (id3v2data[3]<<16)|(id3v2data[4]<<8)|(id3v2data[5]);
		PRINT("// parseid3v20 size=%06x (computed from id3v2data[3]=%02x id3v2data[4]=%02x id3v2data[5]=%02x)\n", size, id3v2data[3], id3v2data[4], id3v2data[5]);
		if (len<(size+6))
		{
			PRINT("// parseid3v20 len<(size+6), return\n");
			return 1; /* out of data */
		}
		PRINT("// parseid3v20 tag=\"%c%c%c\"\n", id3v2data[0], id3v2data[1], id3v2data[2]);
		if (!memcmp(id3v2data, "TP1", 3))
		{
			PRINT("// parseid3v20 store into m->composer (size=%d)\n", sizeof(m->composer));
			handle_T__(id3v2data+6, size, m->composer, sizeof(m->composer));
		} else if (!memcmp(id3v2data, "TT2", 3))
		{
			PRINT("// parseid3v20 store into m->modname (size=%d)\n", sizeof(m->modname));
			handle_T__(id3v2data+6, size, m->modname, sizeof(m->modname));
		}
		PRINT("// parseid3v20 skipping 6+size into id3v2data/len\n");
		len-=(size+6);
		id3v2data+=(size+6);
	}
}

static int parseid3v23(struct moduleinfostruct *m, const uint8_t *id3v2header, uint8_t *id3v2data, uint_fast32_t len)
{
	PRINT("parseid3v23(m, id3v2header = %p (not used), id3v3data = %p, len = %d\n", id3v2header, id3v2data, len);
	while (1)
	{
		uint_fast32_t size;

		PRINT("// parseid3v23, iterate, len = %d\n", len);
		if (len<1)
		{
			PRINT("// parseid3v23 len<1, return\n");
			return 1; /* out of data */
		}
		if (id3v2data[0]==0)
		{
			PRINT("// parseid3v23 padding reached, return\n");
			return 0; /* padding reached */
		}
		if (len<10)
		{
			PRINT("// parseid3v23 len<10, return\n");
			return 1; /* out of data */
		}

		size = (id3v2data[4]<<24)|(id3v2data[5]<<16)|(id3v2data[6]<<8)|(id3v2data[7]);
		PRINT("// parseid3v23 size=%08x (computed from id3v2data[4]=%02x id3v2data[5]=%02x id3v2data[6]=%02x id3v2data[7]=%02x)\n", size, id3v2data[4], id3v2data[5], id3v2data[6], id3v2data[7]);
		if (len<(size+10))
		{
			PRINT("// parseid3v23 len<(size+10), return\n");
			return 1; /* out of data */
		}
		PRINT("// parseid3v23 flags: id3v2data[8]=%02x id3v2data[9]=%02x\n", id3v2data[8], id3v2data[9]);
		if ((id3v2data[8]&0x8f)||(id3v2data[9]&0xfc))
		{ /* TODO, we now silently ignore entries with flags, so zlib compressed tag-pages will fail */
			PRINT("// parseid3v23 unsupported flag detected (like zlib compression etc.)\n");
		} else {
			int offset=10;
			uint_fast32_t _size = size;

			if (id3v2data[9]&0x02)
			{
				_size = unsync(id3v2data+offset, size);
				PRINT("// parseid3v23 unsync bit set, size updated to %d\n", size);
			}
			if (id3v2data[9]&0x01)
			{
				PRINT("// parseid3v23 size-validation check, ignoring, skipping 4 bytes\n");
				offset+=4; /* we ignore the size-validation check here */
				if (_size < 4)
					_size = 0;
				else
					_size -= 4;
			}
			PRINT("// parseid3v23 tag=\"%c%c%c%c\"\n", id3v2data[0], id3v2data[1], id3v2data[2], id3v2data[3]);

			if (!memcmp(id3v2data, "TPE1", 4))
			{
				PRINT("// parseid3v23 storing into m->composer\n");
				handle_T___(id3v2data+offset, _size, m->composer, sizeof(m->composer));
			} else if (!memcmp(id3v2data, "TIT2", 4))
			{
				PRINT("// parseid3v23 storing into m->modname\n");
				handle_T___(id3v2data+offset, _size, m->modname, sizeof(m->modname));
			}
		}
		PRINT("// parseid3v23 skipping 10+size into id3v2data/len\n");
		len-=(size+10);
		id3v2data+=(size+10);
	}
}

/* returns 0 on out of data, parseerror (invalid size) */
/* returns 1 on ok */
static int parseid3v2(struct moduleinfostruct *m, const uint8_t *id3v2header, uint8_t *id3v2data, uint_fast32_t len)
{
	PRINT("parseid3v2(m, id3v2header = %p, id3v3data = %p, len = %d\n", id3v2header, id3v2data, len);
	PRINT("// parseid3v2 tag version 2.%d\n", id3v2header[3]);

	if ((id3v2header[5]&0x80))
	{
		len = unsync(id3v2data, len);
		PRINT("// parseid3v2 global unsync bit set, new len = %d\n", len);
	}
	/*fprintf(stderr, "Version 2.%d\n", id3v2header[3]);*/
	if (id3v2header[3]<4)
	{
		if (id3v2header[5]&0x40)
		{
			PRINT("// parseid3v2 TAG version lower than 2.4, and extended tag flaged, trying to skip it (len is fixed to 10 bytes)\n");
			if (len<10)
			{
				PRINT("// parseid3v2 to little data to skip, return \n");
				return 1; /* failed to parse */
			}
			len-=10;
			id3v2data+=10;
		}
	} else {
		if (id3v2header[5]&0x40)
		{
			uint_fast32_t esize;
			PRINT("// parseid3v2 TAG version greater or equal to 2.4, and extended tag flaged, trying  to skip it\n");
			/*fprintf(stderr, "Removing extended header\n");*/
			if (len<6)
			{
				PRINT("// parseid3v2 to little data available to calculate extended header length (len<6), return\n");
				return 1; /* failed to parse */
			}
			esize = (id3v2data[0]<<21)|(id3v2data[1]<<14)|(id3v2data[2]<<7)|(id3v2data[3]);
			PRINT("// parseid3v2 extended header size=%08x, calculated from id3v2data[0]=%02x, id3v2data[1]=%02x, id3v2data[2]=%02x, id3v2data=%02x\n", esize, id3v2data[0], id3v2data[1], id3v2data[2], id3v2data[3]);
			if (len<esize)
			{
				PRINT("// parseid3v2 skipping extended header failed, to little data available (len<size), return\n");
				return 1; /* failed to parse */
			}
			len-=esize;
			id3v2data+=esize;
		}
	}

	PRINT("// parseid3v2, calling sub-parser based on version\n");
	if (id3v2header[3]<=2)
		return parseid3v20(m, id3v2header, id3v2data, len);
	else
		return parseid3v23(m, id3v2header, id3v2data, len);
}

static int freqtab[3][3]=
{
	{44100, 48000, 32000}, {22050, 24000, 16000}, {11025, 12000, 8000}
};

static uint16_t fetch16(const char *buf)
{
	return ((unsigned char)buf[0])|(((unsigned char)buf[1])<<8);
}

static uint32_t fetch32(const char *buf)
{
	return ((unsigned char)buf[0])|(((unsigned char)buf[1])<<8)|(((unsigned char)buf[2])<<16)|(((unsigned char)buf[3])<<24);
}

static int ampegpReadMemInfo(struct moduleinfostruct *m, const char *buf, size_t len)
{
	char const *bufend=buf+len;
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

	if ((toupper(m->name[9])!='M')||(toupper(m->name[10])!='P'))
		return 0;

	/* first, try to detect if we have an mpeg stream embedded into a riff/wave container. Often used on layer II files */
	                   /* RIFF */                    /* WAVE */                     /* fmt  */
	if ((fetch32(buf)==0x46464952)&&(fetch32(buf+8)==0x45564157)&&(fetch32(buf+12)==0x20746D66)&&(fetch16(buf+20)==0x0055))
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
	}

	while ((buf+3)<bufend)
	{
		if ((buf[0]=='T')&&(buf[1]=='A')&&(buf[2]=='G'))
		{
			m->modtype=mtMPx; /* force this to be a mp3.. even though id3 tag can be bigger than our buffer */
			buf+=128; /* TOOD, if we have enough buffer, just parse it and return 1 */
			continue;
		}
		if ((buf[0]=='I')&&(buf[1]=='D')&&(buf[2]=='3'))
		{
			uint_fast32_t size;
			m->modtype=mtMPx; /* force this to be a mp3.. even though id3 tag can be bigger than our buffer */
			if ((buf+10)>=bufend)
				return 0;
			size=(buf[6]<<21)|(buf[7]<<14)|(buf[8]<<7)|(buf[9]);
			buf+=(size+10); /* TOOD, try to parse it, and if padding was reached, return 1 */
			continue;
		}
		break;
	}

	if ((buf+sizeof(uint32_t))>=bufend)
		return 0;
	while ((fetch16(buf)&0xE0FF)!=0xE0FF)
	{
		buf+=1;
		if ((buf+sizeof(uint32_t))>=bufend)
			return 0;
	}

	hdr=fetch32(buf);
	layer=4-((hdr>>9)&3);
	if (layer>=4)
		return 0;
	ver=((hdr>>11)&1)?0:1;
	if (!((hdr>>12)&1))
	{
		if (ver)
			ver=2;
		else
			return 0;
	}
	if ((ver==2)&&(layer!=3))
		return 0;
	rateidx=(hdr>>20)&15;
	frqidx=(hdr>>18)&3;
	padding=(hdr>>17)&1;
	stereo="\x01\x01\x02\x00"[(hdr>>30)&3];
	if (frqidx==3)
		return 0;
	if (!ver)
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
				return 0;
		} else switch (layer)
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
				return 0;
		}

	if (!rate)
		return 0;

	m->modname[0]=0;
	switch (layer)
	{
		case 1:
			strcat(m->modname, "Layer   I, ");
			break;
		case 2:
			strcat(m->modname, "Layer  II, ");
			break;
		case 3:
			strcat(m->modname, "Layer III, ");
			break;
	}
	switch (ver)
	{
		case 0:
			switch (frqidx)
			{
				case 0:
					strcat(m->modname, "44100 Hz, ");
					break;
				case 1:
					strcat(m->modname, "48000 Hz, ");
					break;
				case 2:
					strcat(m->modname, "32000 Hz, ");
					break;
			}
			break;
		case 1:
			switch (frqidx)
			{
				case 0:
					strcat(m->modname, "22050 Hz, ");
					break;
				case 1:
					strcat(m->modname, "24000 Hz, ");
					break;
				case 2:
					strcat(m->modname, "16000 Hz, ");
					break;
			}
			break;
		case 2:
			switch (frqidx)
			{
				case 0:
					strcat(m->modname, "11025 Hz, ");
					break;
				case 1:
					strcat(m->modname, "12000 Hz, ");
					break;
				case 2:
					strcat(m->modname, " 8000 Hz, ");
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
			strcat(m->modname, " ");
		if (rate<10)
			strcat(m->modname, " ");

		sprintf(m->modname+strlen(m->modname), "%d", rate);

		strcat(m->modname, " kbps");
		m->playtime=m->size/(rate*125);
	} else {
		strcat(m->modname, "VBR");
		m->playtime=0; /* unknown */
	}

	m->channels=stereo?2:1;
	m->modtype=mtMPx;
	return 0;
}

static int ampegpReadInfo(struct moduleinfostruct *m, FILE *f, const char *buf, size_t len)
{
	off_t relstart = 0; /* offset */
	char const *bufend=buf+len;
	uint32_t hdr;
	int layer;
	int ver;
	int rateidx;
	int frqidx;
	/* int stereo; */
	int rate;
	/*int gottag = 0;*/

	if ((toupper(m->name[9])!='M')||(toupper(m->name[10])!='P'))
		return 0;

	/* First, try to detect if we have an mpeg stream embedded into a riff/wave container. Often used on layer II files.
	 * This should fit inside the provided buf/len data
	 * */
	                   /* RIFF */                    /* WAVE */                     /* fmt  */
	if ((fetch32(buf)==0x46464952)&&(fetch32(buf+8)==0x45564157)&&(fetch32(buf+12)==0x20746D66)&&(fetch16(buf+20)==0x0055))
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
		if ((buf[0]=='I')&&(buf[1]=='D')&&(buf[2]=='3'))
		{
			uint_fast32_t realsize;
			uint8_t tagv2header[10];
			uint8_t *buffer = 0;
			uint8_t *tagv2data;
			m->modtype=mtMPx;
			if ((buf+10)>=bufend)
			{
				if (fseeko(f, relstart, SEEK_SET)==-1)
					break;
				if (fread(tagv2header, 10, 1, f)!=1)
					break;
			} else {
				memcpy(tagv2header, buf, 10);
			}
			realsize = (tagv2header[6]<<21)|(tagv2header[7]<<14)|(tagv2header[8]<<7)|(tagv2header[9]);
			if ((buf+10+realsize)>bufend)
			{
				tagv2data = buffer = malloc(realsize);
				if (!buffer)
					break;
				if (fseeko(f, relstart+10, SEEK_SET)==-1)
					break;
				if (fread(buffer, realsize, 1, f)!=1)
					break;
			} else
					tagv2data = (uint8_t *)buf + 10;
			parseid3v2(m, tagv2header, tagv2data, realsize);
			if (buffer)
				free(buffer);
			buf+=10+realsize;
			relstart+=10+realsize;
			return 1;
			/*gottag++;
			 *continue;*/
		} else if ((buf[0]=='T')&&(buf[1]=='A')&&(buf[2]=='G'))
		{
			uint8_t id3v1[128];
			m->modtype=mtMPx;
			if ((buf+128)>bufend)
			{
				if (fseeko(f, relstart, SEEK_SET)==-1)
					break;
				if (fread(id3v1, 128, 1, f)!=1)
					break;
			} else {
				memcpy(id3v1, buf, 128);
			}
			parseid3v1(m, id3v1);
			buf+=128;
			relstart+=128;
			return 1;
			/*gottag++;
			continue;*/
		} else
			break;
	}

	fseeko(f, 0, SEEK_END);
	while (1)
	{
		{
			uint8_t id3v1[128];
			fseeko(f, -128, SEEK_CUR);
			if (fread(id3v1, 128, 1, f)!=1)
			{
				fseeko(f, 0, SEEK_SET);
				return 0;
			}
			if ((id3v1[0]=='T')&&(id3v1[1]=='A')&&(id3v1[2]=='G'))
			{
				m->modtype=mtMPx;
				fseeko(f, -128, SEEK_CUR);
				parseid3v1(m, id3v1);
				return 1;
				/*gottag++;
				 *continue;*/
			}
		}
		{
			uint8_t id3v2header[10];
			fseeko(f, -10, SEEK_CUR);
			if (fread(id3v2header, 10, 1, f)!=1)
			{
				fseeko(f, 0, SEEK_SET);
				return 0;
			}
			if ((id3v2header[0]=='I')&&(id3v2header[1]=='D')&&(id3v2header[2]=='3')&&(id3v2header[3]!=0xff)&&(id3v2header[4]!=0xff))
			{
				uint8_t *id3v2data;
				uint_fast32_t size = (id3v2header[6]<<21)|(id3v2header[7]<<14)|(id3v2header[8]<<7)|(id3v2header[9]);

				m->modtype=mtMPx;

				fseeko(f, -(size), SEEK_CUR);

				id3v2data = malloc(size);
				if (fread(id3v2data, 10, 1, f)==1)
					parseid3v2(m, id3v2header, id3v2data, size);
				free(id3v2data);

				fseeko(f, -(size+10), SEEK_CUR);

				return 1;
				/*gottag++;
				 *continue;*/
			}
		}
		break;
	}
	/*fseeko(f, relstart, SEEK_SET);*/

	if ((buf+sizeof(uint32_t))>=bufend)
		return 0;
	while ((fetch16(buf)&0xE0FF)!=0xE0FF)
	{
		buf+=1;
		if ((buf+sizeof(uint32_t))>=bufend)
			return 0;
	}

	hdr=fetch32(buf);
	layer=4-((hdr>>9)&3);
	if (layer==4)
		return 0;
	ver=((hdr>>11)&1)?0:1;
	if (!((hdr>>12)&1))
	{
		if (ver)
			ver=2;
		else
			return 0;
	}
	if ((ver==2)&&(layer!=3))
		return 0;
	rateidx=(hdr>>20)&15;
	frqidx=(hdr>>18)&3;
	/* stereo="\x01\x01\x02\x00"[(hdr>>30)&3]; */
	if (frqidx==3)
		return 0;
	if (!ver)
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
				return 0;
		} else switch (layer)
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
				return 0;
		}

	if (!rate)
		return 0;

	m->modtype=mtMPx;

	return 1;
}

struct mdbreadinforegstruct ampegpReadInfoReg = {ampegpReadMemInfo, ampegpReadInfo, 0 MDBREADINFOREGSTRUCT_TAIL};
