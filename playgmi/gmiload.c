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
	(*loadpatch)( struct minstrument *ins, /* bank MSB, LSB and program is pre-filled into instrument */
	              uint8_t            *notesused,
	              struct sampleinfo **samples,
	              uint16_t           *samplenum);

#warning Remove midInstrumentNames (used internally by some font-loaders)
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

struct instused_t
{
	uint8_t bankmsb;
	uint8_t banklsb;
	uint8_t program;
	uint8_t notesused[16];
};

char __attribute__ ((visibility ("internal"))) midLoadMidi( struct midifile *m,
                  FILE            *file,
                  uint32_t         opt)
{
	char retval;
	int inited=0;

	uint8_t drumch2;
	uint32_t len;

	uint16_t trknum;
	uint16_t mtype;

	uint32_t dummy;
	int i;

	struct instused_t **instused = 0;
	int instused_n = 0;

	struct instused_t *lookup_instused(uint8_t bankmsb, uint8_t banklsb, uint8_t program)
	{
		int k;
		void *tmp;

		for (k=0; k < instused_n; k++)
		{
			if ((instused[k]->banklsb == banklsb) && (instused[k]->bankmsb == bankmsb) && (instused[k]->program==program))
			{
				return instused[k];
			}
		}
		tmp = realloc (instused, (instused_n+1) * sizeof (*instused));
		if (!tmp)
		{
			fprintf (stderr, "[midi-load]: fatal error, realloc() failed\n");
			free (instused);
			instused = 0;
			return 0;
		}
		instused = tmp;
		instused_n++;

		instused[k] = calloc (sizeof (**instused), 1);
		instused[k]->bankmsb = bankmsb;
		instused[k]->banklsb = banklsb;
		instused[k]->program = program;
		return instused[k];
	}

	struct instused_t *chaninst[16];
	uint8_t bank_msb[16];
	uint8_t bank_lsb[16];
	uint8_t program[16];

//	struct sampleinfo **smps = 0;

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

	for (i=0; i<m->tracknum; i++)
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

	memset(chaninst, 0, sizeof(chaninst));
	
	for (i=0; i < 16; i++)
	{ /* drumch2 will be outside the 0-15 range if not in use */
		bank_msb[i] = ( (i==9) || (i==drumch2) ) ? 120 : 121;
		bank_lsb[i] = 0;
		program[i] = 0;
	}

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
				fprintf(stderr, "[midi-load] #1 premature end-of-data in track %d\n", i);
				retval = errFormStruc;
				goto failed;
			}
#ifdef MIDI_LOAD_DEBUG
			fprintf(stderr, " timeslice\n");
			//fprintf(stderr, "[midi-load] New trackticks is now: %d\n", (int)trackticks);
			if (*trkptr&0x80)
				fprintf(stderr, "    0x%02x event type and channel\n", *trkptr);
			else
				fprintf(stderr, "         event type is cached\n");
#endif
			if (*trkptr&0x80)
			{
				status=*trkptr++;
				if (trkptr>m->tracks[i].trkend)
				{
					fprintf(stderr, "[midi-load] #2 premature end-of-data in track %d\n", i);
					retval = errFormStruc;
					goto failed;
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
						fprintf(stderr, "[midi-load] #3 premature end-of-data in track %d\n", i);
						retval = errFormStruc;
						goto failed;
					}
				}
#ifdef MIDI_LOAD_DEBUG
				fprintf(stderr, "    ");
#endif
				len=readvlnum(&trkptr, m->tracks[i].trkend, &eof);
				if (eof)
				{
					fprintf(stderr, "[midi-load] #4 premature end-of-data in track %d\n", i);
					retval = errFormStruc;
					goto failed;
				}

				trkptr+=len;

				if (trkptr>m->tracks[i].trkend)
				{
					fprintf(stderr, "[midi-load] #5 premature end-of-data in track %d\n", i);
					retval = errFormStruc;
					goto failed;
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
							fprintf(stderr, "[midi-load] #6 premature end-of-data in track %d\n", i);
							retval = errFormStruc;
							goto failed;
						}
						if (trkptr[1])
						{
							uint8_t ch=status&0x0F;
							if (!chaninst[ch])
							{
								chaninst[ch]=lookup_instused(bank_msb[ch], bank_lsb[ch], program[ch]);
							}
							if (!chaninst[ch])
							{
								retval = errAllocMem;
								goto failed;
							}
							chaninst[ch]->notesused[trkptr[0]>>3]|=1<<(trkptr[0]&7);
						}
						trkptr+=2;
						break;
					case 0xB0: /* control-change */
						if ((trkptr+1)>=m->tracks[i].trkend)
						{
							fprintf(stderr, "[midi-load] #7 premature end-of-data in track %d\n", i);
							retval = errFormStruc;
							goto failed;
						}
						if (trkptr[0] == 0x00) /* BANK MSB */
						{
							if (trkptr[1] & 0x80)
							{
								fprintf (stderr, "[midi-load] bank MSB out of range\n");
								retval = errFormStruc;
								goto failed;
							}

							bank_msb[status&0x0f] = trkptr[1];
						}
						if (trkptr[0] == 0x20) /* BANK LSB */
						{
							if (trkptr[1] & 0x80)
							{
								fprintf (stderr, "[midi-load] bank LSB out of range\n");
								retval = errFormStruc;
								goto failed;
							}

							bank_lsb[status&0x0f] = (trkptr[1] & 0x7f);
						}
						trkptr+=2;
						break;
					case 0x80: /* note off */
					case 0xA0: /* aftertouch */
					case 0xE0: /* pitch wheel */
						if ((trkptr+1)>=m->tracks[i].trkend)
						{
							fprintf(stderr, "[midi-load] #7 premature end-of-data in track %d\n", i);
							retval = errFormStruc;
							goto failed;
						}
						trkptr+=2;
						break;
					case 0xD0: /* Channel pressure */
						trkptr++;
						break;
					case 0xC0: /* Program change */
						{
							uint_fast8_t ch = status & 0x0f;

							fprintf (stderr, "INST CHANNEL=%d BANK=%d 0x%2x 0x%02x PROGRAM=%d\n", ch, ((int)bank_msb[ch]<<7) | bank_lsb[ch], bank_msb[ch], bank_lsb[ch], trkptr[0] & 0x7f);

							if (trkptr[0] & 0x80)
							{
								fprintf (stderr, "[midi-load] program out of range\n");
								retval = errFormStruc;
								goto failed;
							}

							program[status&0xF]=trkptr[0];
							chaninst[status&0x0F]=0;
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
		fprintf(stderr, "[midi-load] no time-slices\n");
		retval = errFormStruc;
		goto failed;
	}

	if (!instused_n)
	{
		fprintf(stderr, "[midi-load] no instruments used\n");
		retval = errFormStruc;
		goto failed;
	}

	m->instnum=instused_n;

/*
	if (!(smps=calloc(sizeof(struct sampleinfo *), m->instnum)))
	{
		retval = errAllocMem;
		goto failed;
	}
*/
	if (!(m->instruments=calloc(sizeof(struct minstrument), m->instnum)))
	{
		retval = errAllocMem;
		goto failed;
	}

	inited=1;
	if (!midInit())
	{
		retval = errFileMiss;
		goto failed;
	}

	m->sampnum=0;

	for (i=0; i<m->instnum; i++)
	{
		int stat;

		m->instruments[i].bankmsb = instused[i]->bankmsb;
		m->instruments[i].banklsb = instused[i]->banklsb;
		m->instruments[i].prognum = instused[i]->program;

		stat=loadpatch(m->instruments + i, instused[i]->notesused, &m->samples, &m->sampnum);

		if (stat)
		{
			retval = stat;
			goto failed;
		}
	}

#if 0
	for (i=0; i<0x80; i++)
		if (instused[i])
		{
			int stat;
			stat=loadpatch(&m->instruments[inst], i, sampused[i], &smps[inst], &m->sampnum);
			if (stat)
			{
				retval = stat;
				goto failed;
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
		smps[inst]=calloc(sizeof(struct sampleinfo), drums);        /* OCP sample */
		if (!(ins->samples=calloc(sizeof(struct msample), drums)))  /* midi-sample */
		{
			retval = errAllocMem;
			goto failed;
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
						retval = stat;
						goto failed;
					}
					sn++;
				}
		inst++;
	}
	free(instused);
	instused=0;
#endif

//	m->samples=calloc(sizeof(struct sampleinfo), m->sampnum);

#if 0
loadpatch should append to global sample-list using realloc() etc.
	samplenum=0;
	for (i=0; i<m->instnum; i++)
	{
		for (j=0; j<m->instruments[i].sampnum; j++)
			m->samples[samplenum++]=m->instruments[i].samples[j];
//#warning what is the free-logic here, vs non-drums?
//		free(smps[i]);
	}
//	free(smps);
#endif

	midClose();
	return errOk;

failed:
//	if (smps)
//	{
/*		for (i=0; i<inst; i++)
		{
			if (smps[i])
			{
				for (j=0; j<m->instruments[i].sampnum; j++)
				{
					if (smps[i][j])
					{
						free (smps[i][j]->ptr);
					}
					free(smps[i][j];
			free(smps[i]);
		}
*/
//		free(smps);
//		smps=0;
//	}

	for (i=0; i < instused_n; i++)
	{
		free (instused[i]);
	}
	free(instused);
	instused=0;

	if (inited)
	{
		midClose();
	}

	return retval;
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
