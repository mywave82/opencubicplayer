/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * GMIPlay MID/RMI file loader
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
 *
 * revision history: (please note changes here)
 *  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *    -first release
 *  -kbwhenever Tammo Hinrichs <opencp@gmx.net>
 *    -changed path searching for ULTRASND.INI and patch files
 *  -ryg_xmas   Fabian Giesen  <fabian@jdcs.su.nw.schule.de>
 *    -the .FFF hack, part I - untested, not integrated. wish you a nice
 *     time debugging it...
 *  -fd990122   Felix Domke    <tmbinc@gmx.net>
 *    -integrated the .FFF-hack, improved it, removed some silly bugs,
 *     but it's still not a loader... (and it's far away from that.)
 *     (some hours later: ok, some things are really loaded now.
 *      envelopes are ignored completely, maybe THIS is the error...
 *      you won't hear anything yet, sorry.)
 *     anyway: i am wondering how much changes we need to support all
 *     features of that fff :(
 *  -fd990124   Felix Domke    <tmbinc@gmx.net>
 *    -continued on the work. hmm, some sound will come out of your speakers
 *     right now, but it sounds just TERRIBLE. :)
 *  -ryg990125  Fabian Giesen  <fabian@jdcs.su.nw.schule.de>
 *    -corrected tmbs formulas by combining beisert's ones with some from
 *     GIPC (as Curtis Patzer, programmer of GIPC, would put it: a glorious
 *     hack). and, believe me, the songs start to sound like they should :)
 *    -added mu-law decoding table (nicer than your code, tmb)
 *    -now displays also instrument names (if program number in GM range
 *     this means <128)
 *  -kb990208  Tammo Hinrichs <kb@vmo.nettrade.de>
 *    -fixed some too obvious bugs
 *     ( for (i=...;..;..)
 *       {
 *         ...
 *         for (i=...;..;..) ... ;
 *         ...
 *       }; et al.)
 *  -sss050411 Stian Skjelstad <stian@nixia.no>
 *    -splitet up sourcecode to it's logical pieces
 */

/*
        do THIS in your ocp.ini...

[midi]
  usefff=yes                ; ...or just "no" if you still want to hear midis
  dir=d:\utoplite\          ; where are your DATs located? :)
                            ; note: if you use fffs converted by gipc,
                            ; just write "dir=", because gipc includes
                            ; the path to the .dat-file.
  fff=d:\utoplite\utopi_li.fff

*/

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include "types.h"
#include "boot/psetting.h"
#include "dev/mcp.h"
#include "gmiplay.h"
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/imsrtns.h"

#define TRACK_BUFFER_OVERFLOW_WINDOW 4 /* 4 bytes of zero should be "plenty" to avoid buffer overflows on correupt files */

int __attribute__ ((visibility ("internal")))
	(*loadpatch)( struct minstrument *ins,
	              uint8_t             program,
	              uint8_t            *sampused,
	              struct sampleinfo **smps,
	              uint16_t           *samplenum) = 0;

int __attribute__ ((visibility ("internal")))
	(*addpatch)( struct minstrument *ins,
	             uint8_t             program,
	             uint8_t             sn,
	             uint8_t             sampnum,
	             struct sampleinfo  *sip,
	             uint16_t           *samplenum) = 0;

__attribute__ ((visibility ("internal"))) char midInstrumentNames[256][NAME_MAX+1];

static inline unsigned long readvlnum(unsigned char **ptr, unsigned char *endptr, int *eof)
{
	unsigned long num=0;
	while (1)
	{
#ifdef MIDI_LOAD_DEBUG
		fprintf(stderr, "0x%02x ", **ptr);
#endif
		if(*ptr>=endptr)
		{
			*eof=1;
			break; /* END OF DATA */
		}
		num=(num<<7)|((**ptr)&0x7F);
		if (!(*((*ptr)++)&0x80))
			break;
	}
#ifdef MIDI_LOAD_DEBUG
	fprintf(stderr, "=> 0x%08lx", num);
#endif
	return num;
}

char __attribute__ ((visibility ("internal"))) midLoadMidi( struct midifile *m,
                  FILE            *file,
                  uint32_t         opt)
{
	uint8_t drumch2;
	uint32_t len;

	uint16_t trknum;
	uint16_t mtype;

	uint32_t dummy;
	int i, j;
	int samplenum;

	uint8_t (*sampused)[16];
	uint8_t instused[0x81];
	uint8_t chaninst[16];

	struct sampleinfo **smps;
	uint16_t inst;

	mid_free(m);

	m->opt=opt;

	drumch2=(m->opt&MID_DRUMCH16)?15:16;

	while (1)
	{
		uint8_t type[4];

#ifdef MIDI_LOAD_DEBUG
		fprintf(stderr, "[midi-load]: attempting to read a chunk\n");
#endif
		if (fread(type, sizeof(type), 1, file) != 1)
		{
			fprintf(stderr, __FILE__ ": warning, read failed #1\n");
			return errFormStruc;
		}
#ifdef MIDI_LOAD_DEBUG
		fprintf(stderr, "[midi-load]: Checking chunk \"%c%c%c%c\" against \"RIFF\"\n", type[0], type[1], type[2], type[3]);
#endif
		if (!memcmp(type, "RIFF", 4))
		{
			uint8_t subtype[4];
#ifdef MIDI_LOAD_DEBUG
			fprintf(stderr, "[midi-load]: Reading chunk headers\n");
#endif
			if (fread(&dummy, sizeof(uint32_t), 1, file) != 1)
				fprintf(stderr, __FILE__ ": warning, read failed #2\n");
			if (fread(subtype, sizeof(subtype), 1, file) != 1)
				fprintf(stderr, __FILE__ ": warning, read failed #3\n");
#ifdef MIDI_LOAD_DEBUG
			fprintf(stderr, "[midi-load]: checking type \"%c%c%c%c\" against \"RMID\"\n", subtype[0], subtype[1], subtype[2], subtype[3]);
#endif
			if (memcpy(subtype, "RMID", 4))
				return errFormStruc;
#ifdef MIDI_LOAD_DEBUG
			fprintf(stderr, "[midi-load] yes, we might have RMID data embedded into RIFF, searching sub-chucks\n");
#endif
			while (1)
			{
				uint32_t size;
#ifdef MIDI_LOAD_DEBUG
				fprintf(stderr, "[midi-load] reading next sub-chunk\n");
#endif
				if (fread(subtype, sizeof(subtype), 1, file) != 1)
					fprintf(stderr, __FILE__ ": warning, read failed #4\n");
#ifdef MIDI_LOAD_DEBUG
				fprintf(stderr, "[midi-load]: checking subtype \"%c%c%c%c\" against \"data\"\n", subtype[0], subtype[1], subtype[2], subtype[3]);
#endif
				if (!memcmp(subtype, "data",4))
				{
#ifdef MIDI_LOAD_DEBUG
					fprintf(stderr, "[midi-load] Yes, it matches, breaking out of inner-loop.. outer loop will detect upcomming MThd header, dirty, but it works\n");
#endif
					break;
				}
#ifdef MIDI_LOAD_DEBUG
				fprintf(stderr, "[midi-load] Going to read subchunk size\n");
#endif
				if (fread(&size, sizeof(uint32_t), 1, file) != 1)
					fprintf(stderr, __FILE__ ": warning, read failed #5\n");
				size = uint32_little (size);
#ifdef MIDI_LOAD_DEBUG
				fprintf(stderr, "[midi-load] size is %d, skipping it\n", (int)size);
#endif
				fseek(file, size, SEEK_CUR);
			}
#ifdef MIDI_LOAD_DEBUG
			fprintf(stderr, "[midi-load] reading a dummy dword\n");
#endif
			if (fread(&dummy, sizeof(uint32_t), 1, file) != 1)
				fprintf(stderr, __FILE__ ": warning, read failed #6\n");
			continue;
		}
#ifdef MIDI_LOAD_DEBUG
		fprintf(stderr, "[midi-load]: Checking chunk \"%c%c%c%c\" against \"MThd\"\n", type[0], type[1], type[2], type[3]);
#endif
		if (!memcmp(type, "MThd", 4))
		{
#ifdef MIDI_LOAD_DEBUG
			fprintf(stderr, "[midi-load]: matches, bail out to the midi data parser\n");
#endif
			break;
		}
#ifdef MIDI_LOAD_DEBUG
		fprintf(stderr, "[midi-load] Going to read chunk size\n");
#endif
		if (fread(&dummy, sizeof(uint32_t), 1, file) != 1)
		{
			fprintf(stderr, __FILE__ ": warning, read failed #7\n");
			return errFormStruc;
		}
#ifdef MIDI_LOAD_DEBUG
		fprintf(stderr, "[midi-load] size is %d, skipping it\n", (int)uint32_big(dummy));
#endif

		fseek(file, uint32_big(dummy), SEEK_CUR);
	}

	if (fread(&dummy, sizeof(uint32_t), 1, file) != 1)
		fprintf(stderr, __FILE__ ": warning, read failed #8\n");
	len=uint32_big(dummy);
	if (len<6)
		return errFormStruc;

	if (fread(&mtype, sizeof(uint16_t), 1, file) != 1)
		fprintf(stderr, __FILE__ ": warning, read failed #9\n");
	mtype=uint16_big(mtype);
	if (fread(&trknum, sizeof(uint16_t), 1, file) != 1)
		fprintf(stderr, __FILE__ ": warning, read failed #10\n");
	trknum=uint16_big(trknum);
	if (fread(&m->tempo, sizeof(uint16_t), 1, file) != 1)
		fprintf(stderr, __FILE__ ": warning, read failed #11\n");
	m->tempo=uint16_big(m->tempo);
	fseek(file, len-6, SEEK_CUR);

	if (mtype>=3)
		return errFormSupp;
	if ((mtype==1)&&(trknum>64))
		return errFormSupp;

	m->tracknum=(mtype==1)?trknum:1;

	if (!(m->tracks=calloc(sizeof(struct miditrack), m->tracknum)))
		return errAllocMem;

	for (i=0; i<m->tracknum; i++)
	{
		m->tracks[i].trk=0;
		m->tracks[i].trkend=0;
	}

	for (i=0; i<trknum; i++)
	{
		while (1)
		{
			uint8_t type[4];

			if (fread(type, sizeof(type), 1, file) != 1)
			{
				fprintf(stderr, __FILE__ ": error, read failed #1\n");
				return errFormStruc;
			}
			if (fread(&len, sizeof(uint32_t), 1, file) != 1)
				fprintf(stderr, __FILE__ ": warning, read failed #12\n");
			len=uint32_big(len);
			if (!memcmp(type, "MTrk", 4))
				break;
			fseek(file, len, SEEK_CUR);
		}

		if (mtype!=2)
		{
			if (!(m->tracks[i].trk=calloc(len + TRACK_BUFFER_OVERFLOW_WINDOW, 1)))
				return errAllocMem;
			m->tracks[i].trkend=m->tracks[i].trk+len;
			if (fread(m->tracks[i].trk, len, 1, file) != 1)
				fprintf(stderr, __FILE__ ": warning, read failed #13\n");
		} else {
			uint32_t oldlen=m->tracks[0].trkend-m->tracks[0].trk;
			uint8_t *n=(uint8_t *)realloc(m->tracks[0].trk, oldlen + len + TRACK_BUFFER_OVERFLOW_WINDOW);
			if (!n)
				return errAllocMem;
			m->tracks[0].trk=n;
			m->tracks[0].trkend=n+oldlen+len;
			if (fread(m->tracks[0].trk+oldlen, len, 1, file) != 1)
				fprintf(stderr, __FILE__ ": warning, read failed #14\n");
			memset(m->tracks[0].trkend, 0, TRACK_BUFFER_OVERFLOW_WINDOW);
		}
	}

	if (!(sampused=calloc(0x81, 16)))
		return errAllocMem;

	memset(instused, 0, 0x81);
	memset(chaninst, 0, 16);
	chaninst[9]=0x80;
	if (drumch2<16)
		chaninst[drumch2]=0x80;

	m->ticknum=0;
	for (i=0; i<m->tracknum; i++)
	{
		uint8_t *trkptr=m->tracks[i].trk;
		uint8_t status=0;
		uint32_t trackticks=0;
		int eof=0;
		while (trkptr<m->tracks[i].trkend)
		{
#ifdef MIDI_LOAD_DEBUG
			fprintf(stderr, "[midi-load] Adding new event\n    ");
#endif
			trackticks+=readvlnum(&trkptr, m->tracks[i].trkend, &eof);
			if (eof)
			{
				m->ticknum=0; /* abort the song */
				fprintf(stderr, "[midi-load] #1 premature end-of-data in track %d\n", i);
				break;
			}
#ifdef MIDI_LOAD_DEBUG
			fprintf(stderr, " timeslice\n");
			//fprintf(stderr, "[midi-load] New trackticks is now: %d\n", (int)trackticks);
			if (*trkptr&0x80)
				fprintf(stderr, "    0x%02x event type and channel\n", *trkptr);
			else
				fprintf(stderr, "           event type is cached\n");
#endif
			if (*trkptr&0x80)
			{
				status=*trkptr++;
				if (trkptr>m->tracks[i].trkend)
				{
					m->ticknum=0; /* abort the song */
					fprintf(stderr, "[midi-load] #2 premature end-of-data in track %d\n", i);
					break;
				}
			}
			else if ((status&0xf0) == 0xf0)
			{
				fprintf(stderr, "[midi-load] cached status 0xFn is not supposed to happen\n");
			}
			if ((status==0xFF)||(status==0xF0)||(status==0xF7))
			{
				unsigned len;
				if (status==0xFF)
				{
#ifdef MIDI_LOAD_DEBUG
					fprintf(stderr, "    0x%02x META EVENT type\n", status);
#endif
					trkptr++;
					if (trkptr>=m->tracks[i].trkend)
					{
						m->ticknum=0; /* abort the song */
						fprintf(stderr, "[midi-load] #3 premature end-of-data in track %d\n", i);
						break;
					}
				}
#ifdef MIDI_LOAD_DEBUG
				fprintf(stderr, "    ");
#endif
				len=readvlnum(&trkptr, m->tracks[i].trkend, &eof);
				if (eof)
				{
					m->ticknum=0; /* abort the song */
					fprintf(stderr, "[midi-load] #4 premature end-of-data in track %d\n", i);
					break;
				}

				trkptr+=len;

				if (trkptr>m->tracks[i].trkend)
				{
					m->ticknum=0; /* abort the song */
					fprintf(stderr, "[midi-load] #5 premature end-of-data in track %d\n", i);
					break;
				}

#ifdef MIDI_LOAD_DEBUG
				fprintf(stderr, " length.. skipping it\n");
#endif
			} else
				switch (status&0xF0)
				{
					case 0x90: /* note on */
						if ((trkptr+1)>=m->tracks[i].trkend)
						{
							m->ticknum=0; /* abort the song */
							fprintf(stderr, "[midi-load] #6 premature end-of-data in track %d\n", i);
							break;
						}
						if (trkptr[1])
						{
							sampused[chaninst[status&0xF]][trkptr[0]>>3]|=1<<(trkptr[0]&7);
							instused[chaninst[status&0xF]]=1;
						}
						trkptr+=2;
						break;
					case 0x80: /* note off */
					case 0xA0: /* aftertouch */
					case 0xB0: /* control-change */
					case 0xE0: /* pitch wheel */
						if ((trkptr+1)>=m->tracks[i].trkend)
						{
							m->ticknum=0; /* abort the song */
							fprintf(stderr, "[midi-load] #7 premature end-of-data in track %d\n", i);
							break;
						}
						trkptr+=2;
						break;
					case 0xD0: /* Channel pressure */
						trkptr++;
						break;
					case 0xC0: /* Program change */
						if (((status&0xF)!=9)&&((status&0xF)!=drumch2))
						{
							chaninst[status&0xF]=trkptr[0];
							/* shit! */
							memset(sampused[trkptr[0]], 0xFF, 16);
							instused[trkptr[0]]=1;
						}
						trkptr++;
						break;
					default:
						fprintf(stderr, "[midi-load] got a status I don't know how to handle (0xFn probably): 0x%02x\n", status);
						break;
				}
		}
#ifdef MIDI_LOAD_DEBUG
		fprintf(stderr, "[midi-load] track %d is %d/0x%08x ticks\n", i, (int)trackticks, (int)trackticks);
#endif
		if (m->ticknum<trackticks)
			m->ticknum=trackticks;
	}
	if (!m->ticknum)
	{
		free(sampused);
		return errFormStruc;
	}

	m->instnum=0;
	for (i=0; i<0x81; i++)
		if (instused[i])
			m->instnum++;

	if (!m->instnum)
	{
		instused[0]=1;
		memset(sampused[0], 0xFF, 16);
		m->instnum++;
	}

	if (!(smps=calloc(sizeof(struct sampleinfo *), m->instnum)))
	{
		free(sampused);
		return errAllocMem;
	}
	if (!(m->instruments=calloc(sizeof(struct minstrument), m->instnum)))
	{
		free(sampused);
		free(smps);
		return errAllocMem;
	}

	for (i=0; i<m->instnum; i++)
	{
		m->instruments[i].sampnum=0;
		m->instruments[i].samples=0;
		smps[i]=0;
	}

	if (!midInit())
	{
		free(sampused);
		free(smps);
		midClose();
		return errFileMiss;
	}

	m->sampnum=0;
	memset(m->instmap, 0, 0x81);
	inst=0;
	for (i=0; i<0x80; i++)
		if (instused[i])
		{
			int stat;
			stat=loadpatch(&m->instruments[inst], i, sampused[i], &smps[inst], &m->sampnum);
			if (stat)
			{
				free(sampused);
				midClose();
				free(smps);
				return stat;
			}
			m->instruments[inst].prognum=i;
			m->instmap[i]=inst;
			inst++;
		}

	if (instused[0x80] && addpatch) /* fff-todo, blah... */
	{
		struct minstrument *ins;
		uint16_t drums=0;
		uint8_t sn;
		for (i=0; i<0x80; i++)
			if (sampused[0x80][i>>3]&(1<<(i&7)))
				if (midInstrumentNames[i+0x80][0])
					drums++;
		m->instmap[0x80]=inst;
		ins=&m->instruments[inst];
		ins->prognum=0x80;
		ins->sampnum=drums;
		smps[inst]=calloc(sizeof(struct sampleinfo), drums);
		if (!(ins->samples=calloc(sizeof(struct msample), drums)))
		{
			free(sampused);
			midClose();
			free(smps);
			return errAllocMem;
		}
		memset(ins->note, 0xFF, 0x80);
		strcpy(ins->name, "drums");
		sn=0;
		for (i=0; i<0x80; i++)
			if (sampused[0x80][i>>3]&(1<<(i&7)))
				if (midInstrumentNames[i+0x80][0])
				{
					int stat;
					ins->note[i]=sn;
					stat=addpatch(ins, i+0x80, sn, i, &smps[inst][sn], &m->sampnum);
					if (stat)
					{
						free(sampused);
						midClose();
						return stat;
					}
					sn++;
				}
		inst++;
	}
	free(sampused);

	m->samples=calloc(sizeof(struct sampleinfo), m->sampnum);

	samplenum=0;
	for (i=0; i<inst; i++)
	{
		for (j=0; j<m->instruments[i].sampnum; j++)
			m->samples[samplenum++]=smps[i][j];
		free(smps[i]);
	}
	free(smps);

	midClose();
	return errOk;
}

void __attribute__ ((visibility ("internal"))) mid_reset(struct midifile *mf)
{
	mf->tracks=0;
	mf->instruments=0;
	mf->samples=0;
}

void __attribute__ ((visibility ("internal"))) mid_free(struct midifile *mf)
{
	int i;
	if (mf->tracks)
	{
		for (i=0; i<mf->tracknum; i++)
			if (mf->tracks[i].trk)
				free(mf->tracks[i].trk);
		free(mf->tracks);
	}
	if (mf->instruments)
	{
		for (i=0; i<mf->instnum; i++)
			if (mf->instruments[i].samples)
				free(mf->instruments[i].samples);
		free(mf->instruments);
	}
	if (mf->samples)
	{
		for (i=0; i<mf->sampnum; i++)
			free(mf->samples[i].ptr);
		free(mf->samples);
	}
	mid_reset(mf);
}
