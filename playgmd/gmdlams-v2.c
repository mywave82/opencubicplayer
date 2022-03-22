/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * GMDPlay loader for Velvet Studio modules.
 * PatternLoader algorithm is from https://github.com/Konstanty/libmodplug
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
 * ENVELOPES & SUSTAIN!!!
 */

static int _mpLoadAMS_v2_Instruments(struct gmdmodule *m, struct ocpfilehandle_t *file, const uint16_t filever, uint8_t shadowedby[256], unsigned int *instsampnum, struct sampleinfo **smps, struct gmdsample **msmps)
{
	int i, j;

	m->sampnum=0;
	m->modsampnum=0;
	for (i=0; i<m->instnum; i++)
	{
		struct gmdinstrument *ip=&m->instruments[i];
		uint8_t smpnum;

		uint8_t samptab[120];
		struct __attribute__((packed))
		{
			uint8_t speed;
			uint8_t sustain;
			uint8_t loopstart;
			uint8_t loopend;
			uint8_t points;
			uint8_t data[64][3];
		} envs[3];
		uint16_t envflags;

		/* uint8_t vibsweep; */
		uint8_t shadowinst;
		uint16_t volfade;

		uint8_t pchint;

		shadowedby[i]=0;

		if (readPascalString (file, ip->name, sizeof (ip->name), "instrument.name"))
		{
			fprintf (stderr, "AMSv2 loader: read instrument %d named failed\n", i + 1);
			return errFormStruc;
		}
		DEBUG_PRINTF (stderr, "instrument[%d].name=\"%s\"\n", i, ip->name);

		if (ocpfilehandle_read_uint8 (file, &smpnum))
		{
			fprintf (stderr, "AMSv2 loader: read instrument %d sample count failed\n", i + 1);
			return errFormStruc;
		}
		instsampnum[i]=smpnum;
		DEBUG_PRINTF (stderr, "instrument[%d].samples=0x%02"PRIx8"\n", i, smpnum);

		if (!smpnum)
			continue;

		msmps[i]=calloc(1, sizeof(struct gmdsample)*smpnum);
		smps[i]=calloc(1, sizeof(struct sampleinfo)*smpnum);
		if (!smps[i]||!msmps[i])
		{
			return errAllocMem;
		}

		if (filever==0x200)
		{
			bzero (samptab, sizeof (samptab));
			if (file->read (file, samptab+12, 96) != 96)
			{
				fprintf (stderr, "AMSv2 loader: read instrument %d (old style) sample tab failed\n", i + 1);
				return errFormStruc;
			}
		} else {
			if (file->read (file, samptab, sizeof (samptab)) != sizeof (samptab))
			{
				fprintf (stderr, "AMSv2 loader: read instrument %d (new style) sample tab failed\n", i + 1);
				return errFormStruc;
			}
		}
		for (j=0; j<3; j++)
		{
			/* first iteration is volume
			 * second iteration is panning
			 * third iteration is vibrato
			 */
			if (file->read (file, &envs[j], 5) != 5)
			{
				fprintf (stderr, "AMSv2 loader: read instrument %d envelope %d main data failed\n", i + 1, j + 1);
				return errFormStruc;
			}
			DEBUG_PRINTF (stderr, "instrument[%d].env[%d].speed=0x%02x\n", i, j, envs[j].speed);
			DEBUG_PRINTF (stderr, "instrument[%d].env[%d].sustain=0x%02x\n", i, j, envs[j].sustain);
			DEBUG_PRINTF (stderr, "instrument[%d].env[%d].loopstart=0x%02x\n", i, j, envs[j].loopstart);
			DEBUG_PRINTF (stderr, "instrument[%d].env[%d].loopend=0x%02x\n", i, j, envs[j].loopend);
			DEBUG_PRINTF (stderr, "instrument[%d].env[%d].points=0x%02x\n", i, j, envs[j].points);

			if (envs[j].points>64)
			{
				fprintf (stderr, "AMSv2 loader: instrument %d envelope %d has too many points\n", i + 1, j + 1);
				return errFormStruc;
			}
			if (file->read (file, envs[j].data, envs[j].points*3) != envs[j].points*3)
			{
				fprintf (stderr, "AMSv2 loader: read instrument %d envelope %d points failed\n", i + 1, j + 1);
				return errFormStruc;
			}
		}

		/* vibsweep=0; */

		if (ocpfilehandle_read_uint8 (file, &shadowinst))
		{
			fprintf (stderr, "AMSv2 loader: read instrument %d shadow instrument / (vibration sweep) failed\n", i + 1);
			return errFormStruc;
		}
		DEBUG_PRINTF (stderr, "instrument[%d].shadowinst=0x%02"PRIx8"\n", i, shadowinst);

		if (filever==0x201)
		{
			/* vibsweep seems to never been used */
			shadowinst=0;
		} else {
			shadowedby[i]=shadowinst;
		}

		if (ocpfilehandle_read_uint16_le (file, &volfade))
		{
			fprintf (stderr, "AMSv2 loader: read instrument %d volume fade failed\n", i + 1);
			return errFormStruc;
		}
		DEBUG_PRINTF (stderr, "instrument[%d].volfade=0x%04"PRIx16"\n", i, volfade);

		if (ocpfilehandle_read_uint16_le (file, &envflags))
		{
			fprintf (stderr, "AMSv2 loader: read instrument %d envelope flags failed\n", i + 1);
			return errFormStruc;
		}
		DEBUG_PRINTF (stderr, "instrument[%d].envflags=0x%04"PRIx16"\n", i, envflags);

		pchint=(volfade>>12)&3;
		volfade&=0xFFF;

		for (j=0; j<envs[0].points; j++)
		{
			envs[0].data[j][2]<<=1;
		}

		for (j=0; j<3; j++)
		{ /* Volume :Envelope on   <- first iteration
		   * Panning:Envelope on   <- second iteration
		   * Vibrato:Envelope on   <- third iteration
		   */
			if (envflags&(4<<(3*j)))
			{
				uint32_t envlen=0;
				uint8_t *env;
				uint32_t k, p, h;
				int32_t sus;
				int32_t lst;
				int32_t lend;

				int t;

				DEBUG_PRINTF (stderr, "instrument[%d].envflags %d => engage\n", i, j);

				for (t=1; t<envs[j].points; t++)
					envlen+=((envs[j].data[t][1]&1)<<8)|envs[j].data[t][0];

				env=malloc(sizeof(uint8_t)*(envlen+1));
				if (!env)
				{
					return errAllocMem;
				}

				p=0;
				h=envs[j].data[0][2];
				for (t=1; t<envs[j].points; t++)
				{
					uint32_t l=((envs[j].data[t][1]&1)<<8)|envs[j].data[t][0];
					uint32_t dh=envs[j].data[t][2]-h;
					switch (envs[j].data[t][1]&6)
					{
						case 0:
							for (k=0; k<l; k++)
								env[p++]=h+dh*k/l;
							break;
						case 2:
							for (k=0; k<l; k++)
								env[p++]=h+dh*envsin[512*k/l]/256;
							break;
						case 4:
							for (k=0; k<l; k++)
								env[p++]=h+dh*(255-envsin[512-512*k/l])/256;
							break;
					}
					h+=dh;
				}
				env[p]=h;

				sus=-1;
				lst=-1;
				lend=-1;

				if (envflags&(2<<(3*j)))
				{
					sus=0;
					for (t=1; t<envs[j].sustain; t++)
						sus+=((envs[j].data[t][1]&1)<<8)|envs[j].data[t][0];
				}
				if (envflags&(1<<(3*j)))
				{
					lst=0;
					lend=0;
					for (t=1; t<envs[j].loopstart; t++)
						lst+=((envs[j].data[t][1]&1)<<8)|envs[j].data[t][0];
					for (t=1; t<envs[j].loopend; t++)
						lend+=((envs[j].data[t][1]&1)<<8)|envs[j].data[t][0];
				}

				m->envelopes[i*3+j].env=env;
				m->envelopes[i*3+j].len=envlen;
				m->envelopes[i*3+j].type=0;
				m->envelopes[i*3+j].speed=envs[j].speed;

				if (sus!=-1)
				{
					m->envelopes[i*3+j].sloops=sus;
					m->envelopes[i*3+j].sloope=sus+1;
					m->envelopes[i*3+j].type=mpEnvSLoop;
				}
				if (lst!=-1)
				{
					if (envflags&(0x200<<j))
					{
						if (lend<sus)
						{
							m->envelopes[i*3+j].sloops=lst;
							m->envelopes[i*3+j].sloope=lend;
							m->envelopes[i*3+j].type=mpEnvSLoop;
						}
					} else {
						m->envelopes[i*3+j].loops=lst;
						m->envelopes[i*3+j].loope=lend;
						m->envelopes[i*3+j].type=mpEnvLoop;
					}
				}
			}
		}
		memset(ip->samples, -1, 128*2);

		for (j=0; j<smpnum; j++, m->sampnum++, m->modsampnum++)
		{
			struct gmdsample *sp=&msmps[i][j];
			struct sampleinfo *sip=&smps[i][j];

			struct __attribute__((packed))
			{
				uint32_t loopstart;
				uint32_t loopend;
				uint16_t samprate;
				uint8_t panfine;
				uint16_t rate;
				int8_t relnote;
				uint8_t vol;
				uint8_t flags; /* bit 6: direction */
			} amssmp;


			int k;
			for (k=0; k<116; k++)
				if (samptab[k]==j)
					ip->samples[k+12]=m->modsampnum;

			sp->handle=0xFFFF;
			sp->volenv=0xFFFF;
			sp->panenv=0xFFFF;
			sp->pchenv=0xFFFF;
			sp->volfade=0xFFFF;

			if (readPascalString (file, sp->name, sizeof (sp->name), "sample.name"))
			{
				fprintf (stderr, "AMSv2 loader: read instrument %d sample %d name failed\n", i + 1, j + 1);
				return errFormStruc;

			}
			DEBUG_PRINTF (stderr, "instrument[%d].sample[%d].name=\"%s\"\n", i, j, sp->name);

			if (ocpfilehandle_read_uint32_le (file, &sip->length))
			{
				sip->length = 0;
				fprintf (stderr, "AMSv2 loader: read instrument %d sample %d length failed\n", i + 1, j + 1);
				return errFormStruc;

			}
			DEBUG_PRINTF (stderr, "instrument[%d].sample[%d].length=0x%08"PRIx32"\n", i, j, sip->length);

			if (!sip->length)
				continue;

			if (file->read (file, &amssmp, sizeof(amssmp)) != sizeof(amssmp))
			{
				fprintf (stderr, "AMSv2 loader: read instrument %d sample %d header failed\n", i + 1, j + 1);
				return errFormStruc;
			}
			amssmp.loopstart = uint32_little (amssmp.loopstart);
			amssmp.loopend   = uint32_little (amssmp.loopend);
			amssmp.samprate  = uint16_little (amssmp.samprate);
			amssmp.rate      = uint16_little (amssmp.rate);

			DEBUG_PRINTF (stderr, "instrument[%d].sample[%d].loopstart=0x%08"PRIx32"\n", i, j, amssmp.loopstart);
			DEBUG_PRINTF (stderr, "instrument[%d].sample[%d].loopend=0x%08"PRIx32"\n", i, j, amssmp.loopend);
			DEBUG_PRINTF (stderr, "instrument[%d].sample[%d].samprate=0x%04"PRIx16"\n", i, j, amssmp.samprate);
			DEBUG_PRINTF (stderr, "instrument[%d].sample[%d].panfine=0x%02"PRIx8"\n", i, j, amssmp.panfine);
			DEBUG_PRINTF (stderr, "instrument[%d].sample[%d].rate=0x%04"PRIx16"\n", i, j, amssmp.rate);
			DEBUG_PRINTF (stderr, "instrument[%d].sample[%d].relnote=0x%02"PRIx8"\n", i, j, (uint8_t)amssmp.relnote);
			DEBUG_PRINTF (stderr, "instrument[%d].sample[%d].vol=0x%02"PRIx8"\n", i, j, amssmp.vol);
			DEBUG_PRINTF (stderr, "instrument[%d].sample[%d].flags=0x%02"PRIx8"\n", i, j, amssmp.flags);

			sp->stdpan=(amssmp.panfine&0xF0)?((amssmp.panfine>>4)*0x11):-1;
			sp->stdvol=amssmp.vol*2;
			sp->normnote=-amssmp.relnote*256-(int)((signed char)(amssmp.panfine<<4))*2;
			sp->opt=(amssmp.flags&0x04)?MP_OFFSETDIV2:0;

			sp->volfade=volfade;
			sp->pchint=pchint;
			sp->volenv=m->envelopes[3*i+0].env?(3*(signed)i+0):-1;
			sp->panenv=m->envelopes[3*i+1].env?(3*(signed)i+1):-1;
			sp->pchenv=m->envelopes[3*i+2].env?(3*(signed)i+2):-1;

			sip->loopstart=amssmp.loopstart;
			sip->loopend=amssmp.loopend;
			sip->samprate=amssmp.rate;
			sip->type=((amssmp.flags&0x04)?mcpSamp16Bit:0)|((amssmp.flags&0x08)?mcpSampLoop:0)|((amssmp.flags&0x10)?mcpSampBiDi:0);
			if ((amssmp.flags & 0x03) == 1)
			{
				sip->type |= mcpSampRedStereo; /* we borrow this flag, in order to remember to decompress.... */
			}
		}
	}
	return errOk;
}

static int _mpLoadAMS_v2_LoadPattern (struct gmdmodule *m, struct ocpfilehandle_t *file, struct AMSPattern *pattern, uint8_t **buffer, uint32_t *buflen, int patternindex, int nopatterns, int nteoffset, int nte1keyoff)
{
	uint32_t patlen;
	uint32_t patpos;
	unsigned int rowpos;

	if (ocpfilehandle_read_uint32_le (file, &patlen))
	{
		fprintf (stderr, "AMS loader: read pattern %d/%d length failed\n", patternindex + 1, nopatterns);
		patlen = 0;
		return errFormStruc;
	}

	bzero (pattern, sizeof (*pattern));
	patpos = 0;
	rowpos = 0;

	/* first we read the header and pattern-name */
	do {
		uint8_t rowcount;
		uint8_t cmdschns;
		uint64_t oldpos;
		struct gmdpattern *pp=&m->patterns[patternindex];
		oldpos = file->getpos (file);

		if (file->read (file, &rowcount, 1) != 1)
		{
			fprintf (stderr, "AMSv2 loader: read pattern %d/%d row count failed\n", patternindex + 1, nopatterns);
			return errFormStruc;
		}
#if 0
		if (rowcount > 127)
		{
			fprintf (stderr, "AMSv2 loader: read pattern %d/%d rowcount %d  > 128\n", patternindex + 1, nopatterns, (int)rowcount + 1);
				return errFormStruc;
		}
#endif
		pattern->rowcount = (unsigned int)rowcount+1;

		if (file->read (file, &cmdschns, 1) != 1)
		{
			fprintf (stderr, "AMSv2 loader: read pattern %d/%d reserved (cmds/chns) byte failed\n", patternindex + 1, nopatterns);
			return errFormStruc;
		}

		if (((cmdschns & 0x1f) >= m->channum))
		{
			m->channum = (cmdschns & 0x1f) + 1;
		}


		if (readPascalString (file, pp->name, sizeof (pp->name), ""))
		{
			fprintf (stderr, "AMSv2 loader: read pattern %d/%d name failed\n", patternindex + 1, nopatterns);
			return errFormStruc;
		}
		DEBUG_PRINTF (stderr, "pattern[%d].name=\"%s\"\n", patternindex, pp->name);

		oldpos = file->getpos (file) - oldpos;
		if (oldpos > patlen)
		{
			fprintf (stderr, "AMSv2 loader: pattern %d/%d header crossed pattern buffer boundary\n", patternindex + 1, nopatterns);
			return errFormStruc;
		}
		patlen -= oldpos;
	} while (0);

	if (patlen>*buflen)
	{
		*buflen=patlen;
		free(*buffer);
		*buffer=malloc(sizeof(unsigned char)*(*buflen));
		if (!*buffer)
		{
			return errAllocMem;
		}
	}
	if (file->read (file, *buffer, patlen) != patlen)
	{
		fprintf (stderr, "AMS loader: read pattern %d/%d data (patlen=%d) failed\n", patternindex + 1, nopatterns, patlen);
		return errFormStruc;
	}

	while ((patpos < patlen) && (rowpos < pattern->rowcount))
	{
/*
fp0aaaaa ennnnnnn iiiiiiii [rgcccccc bbbbbbbb...]

Legend:
f       1=Last data chunk on the row.
p       0=Read Note+InstNr 1=Don't Read Note+Instnr
a       Channel (Samp 0-31)
e       1=Read one command
n       Period. Note 2-121 (C-0 to B-9), 1 = Key off note
i       InstrumentNr (0-255)
r       1=Read one more command
g       1=Low 6 bits are volume/2
c       Command-nr
b       Command-byte

If g is set, the command only consist of that byte and
the low 6 bits are assumed to be a volume command. You
have to multiply the volume by 2 to get the proper value
(Volume will only be stored this way if it's dividible with 2)


head:
 11111111 empty row, must be at the introduction of the row

 fp_aaaaa f=0 more entries on this row will follow
          f=1 last entry on this row
          p=0 instrument/note will follow (and optional command)
          p=1 command will follow

instrument/note:
 ennnnnnn iiiiiiii e=0 no command will follow
                   e=1 command will follow

command:
 r0cccccc r=0 no command will follow
          r=1 command will follow
          c=volume/2
 r1cccccc bbbbbbbb r=0 no command will follow
                   r=1 command will follow
                   c=command-nr   (command)
                   b=command-byte (parameter)


*/
		uint8_t b0 = (*buffer)[patpos++];
		uint8_t b1;
		uint8_t ch = b0 & 0x1f;

		DEBUG_PRINTF (stderr, "pattern %d row %02x  %02x", patternindex, rowpos, b0);

		if (b0 == 0xff) /* This is not in libmodplug per 20th March 2022 */
		{
			DEBUG_PRINTF (stderr, "     no data on row\n");
			rowpos++;
			continue;
		}

		if (!(b0 & 0x40))
		{
			if (patpos >= patlen)
			{
				fprintf (stderr, "PATv2 warning buffer overflow attempted #1\n");
				break;
			}
			b1 = (*buffer)[patpos++];

			DEBUG_PRINTF (stderr, " %02x NOTE", b1);

			/* note */
			if (b1 & 0x7f)
			{
				if (nte1keyoff && ((b1 & 0x7f) == 1))
				{
					pattern->channel[ch].row[rowpos].fill |= FILL_KEYOFF;
				} else {
					pattern->channel[ch].row[rowpos].note = (b1 & 0x7f) + nteoffset; /* 25 */
					pattern->channel[ch].row[rowpos].fill |= FILL_NOTE;
				}
			}

#if 0
			pattern->channel[ch].row[rowpos].fill |= FILL_INSTRUMENT;
			pattern->channel[ch].row[rowpos].instrument = (*buffer)[patpos++];
#else
			pattern->channel[ch].row[rowpos].instrument = (*buffer)[patpos++];
			DEBUG_PRINTF (stderr, " %02x INSTRUMENT", pattern->channel[ch].row[rowpos].instrument);
			if (pattern->channel[ch].row[rowpos].instrument)
			{
				pattern->channel[ch].row[rowpos].instrument--;
				pattern->channel[ch].row[rowpos].fill |= FILL_INSTRUMENT;
			}
#endif
		} else {
			b1 = 0x80;
		}

		// Read Effect
		while (b1 & 0x80) /* did previous command flag more commands? */
		{
			if (patpos >= patlen)
			{
				fprintf (stderr, "PATv2 warning buffer overflow attempted #2\n");
				break;
			}
			b1 = (*buffer)[patpos++];
			DEBUG_PRINTF (stderr, " %02x", b1);

			if (b1 & 0x40) /* 1=Low 6 bits are volume/2 */
			{
				DEBUG_PRINTF (stderr, " VOL/2");
				pattern->channel[ch].row[rowpos].volume = (b1 & 0x3f) << 1;
				pattern->channel[ch].row[rowpos].fill |= FILL_VOLUME;
			} else {
				uint8_t cmd, b2;

				if (patpos >= patlen)
				{
					fprintf (stderr, "PATv2 warning buffer overflow attempted #3\n");
					break;
				}
				b2 = (*buffer)[patpos++];
				DEBUG_PRINTF (stderr, " %02x CMD+PARAM", b2);

				cmd = b1 & 0x3f;

				if ((cmd == 0x08) && ((b2 & 0xf0) == 0x00))
				{
					pattern->channel[ch].row[rowpos].pan = b2;
					pattern->channel[ch].row[rowpos].fill |= FILL_PAN;
				} else if (cmd == 0x0c)
				{
					pattern->channel[ch].row[rowpos].volume = b2;
					pattern->channel[ch].row[rowpos].fill |= FILL_VOLUME;
				} else if ((cmd == 0x0e) && ((b2 & 0xf0) == 0xd0))
				{
					pattern->channel[ch].row[rowpos].delaynote = b2 & 0x0f;
					pattern->channel[ch].row[rowpos].fill |= FILL_DELAYNOTE;
				} else {
					if (pattern->channel[ch].row[rowpos].effects < MAX_EFFECTS)
					{
						pattern->channel[ch].row[rowpos].effect[pattern->channel[ch].row[rowpos].effects] = cmd;
						pattern->channel[ch].row[rowpos].parameter[pattern->channel[ch].row[rowpos].effects] = b2;
						pattern->channel[ch].row[rowpos].effects++;
					}
				}
				if ((cmd == 0x03) || (cmd == 0x05) || (cmd == 0x15))
				{
					pattern->channel[ch].row[rowpos].note |= 0x80; /* portamento */
				}
			}
		}
		if (b0 & 0x80)
		{
			rowpos++;
		}
		DEBUG_PRINTF (stderr, "\n");
	}

	return errOk;
}

static int _mpLoadAMS_v2(struct gmdmodule *m, struct ocpfilehandle_t *file)
{
	struct __attribute__((packed))
	{
		uint8_t ins;
		uint16_t pat;
		uint16_t pos;
		uint16_t bpm;
		uint8_t speed;
		uint8_t defchn;
		uint8_t defcmd;
		uint8_t defrow;
		uint16_t flags;
	} hdr;

	uint16_t filever;

	uint16_t *ordlist=0;
	struct sampleinfo **smps=0;
	struct gmdsample **msmps=0;
	unsigned int *instsampnum=0;

	uint8_t shadowedby[256];
	uint32_t packlen;

#if 0
	unsigned char *temptrack=0;
#endif
	unsigned int buflen=0;
	unsigned char *buffer=0;

	int sampnum;
	unsigned int i,j,t;
	int retval;


	mpReset(m);

	if (readPascalString (file, m->name, sizeof (m->name), "module.name"))
	{
		fprintf (stderr, "AMSv2 loader: read module name failed\n");
		return errFormSig;
	}

	if (ocpfilehandle_read_uint16_le (file, &filever))
	{
		fprintf (stderr, "AMSv2 loader: read module version failed\n");
		return errFormSig;
	}

	if ((filever!=0x200)&&(filever!=0x201)&&(filever!=0x202))
	{
		return errFormOldVer;
	}

	DEBUG_PRINTF (stderr, "filever=0x%04x\n", filever);

	if (filever<=0x0201)
	{
		struct __attribute__((packed))
		{
			uint8_t ins;
			uint16_t pat;
			uint16_t pos;
			uint8_t bpm;
			uint8_t speed;
			uint8_t flags;
			/*  1         Flags:mfsspphh
			 *  ||||||\\- Pack byte header
			 *  ||||\\--- Pack byte patterns
			 *  ||\\----- Pack byte samples
			 *  \\------- MIDI channels are used in tune. */
		} oldhdr;

		if (file->read (file, &oldhdr, sizeof (oldhdr)) != sizeof (oldhdr))
		{
			fprintf (stderr, "AMSv2 loader: read old style header failed\n");
			return errFormSig;
		}
		hdr.ins = oldhdr.ins;
		hdr.pat = uint16_little (oldhdr.pat);
		hdr.pos = uint16_little (oldhdr.pos);
		hdr.bpm = oldhdr.bpm<<8;
		hdr.speed = oldhdr.speed;
		hdr.flags = (oldhdr.flags&0xC0)|0x20;

		DEBUG_PRINTF (stderr, "header.ins=0x%02x\n", oldhdr.ins);
		DEBUG_PRINTF (stderr, "header.pat=0x%04x\n", hdr.pat);
		DEBUG_PRINTF (stderr, "header.pos=0x%04x\n", hdr.pos);
		DEBUG_PRINTF (stderr, "header.bpm=0x%02x\n", oldhdr.bpm);
		DEBUG_PRINTF (stderr, "header.speed=0x%02x\n", oldhdr.speed);
		DEBUG_PRINTF (stderr, "header.flags=0x%02x\n", oldhdr.flags);
	} else {
		if (file->read (file, &hdr, sizeof (hdr)) != sizeof (hdr))
		{
			fprintf (stderr, "AMSv2 loader: read new style header failed\n");
			return errFormSig;
		}

		hdr.pat = uint16_little (hdr.pat);
		hdr.pos = uint16_little (hdr.pos);
		hdr.bpm = uint16_little (hdr.bpm);
		hdr.flags = uint16_little (hdr.flags);

		DEBUG_PRINTF (stderr, "header.ins=0x%02x\n", hdr.ins);
		DEBUG_PRINTF (stderr, "header.pat=0x%04x\n", hdr.pat);
		DEBUG_PRINTF (stderr, "header.pos=0x%04x\n", hdr.pos);
		DEBUG_PRINTF (stderr, "header.bpm=0x%02x.%02x\n", hdr.bpm >> 8, hdr.bpm & 0xff);
		DEBUG_PRINTF (stderr, "header.speed=0x%02x\n", hdr.speed);
		DEBUG_PRINTF (stderr, "header.flags=0x%04x\n", hdr.flags);
	}

	m->options=((hdr.flags&0x40)?MOD_EXPOFREQ:0)|MOD_EXPOPITCHENV;

	m->channum=32;
	m->instnum=hdr.ins;
	m->envnum=hdr.ins*3;
	m->patnum=hdr.pat+1;
	m->ordnum=hdr.pos;
	m->endord=hdr.pos;
	m->tracknum=33*hdr.pat+1;
	m->loopord=0;

	if (!(ordlist=malloc(sizeof(uint16_t)*hdr.pos)))
	{
		retval=errAllocMem;
		goto safeout;
	}
	if (!(smps=calloc(1, sizeof(struct sampleinfo *)*m->instnum)))
	{
		retval=errAllocMem;
		goto safeout;
	}
	if (!(msmps=calloc(1, sizeof(struct gmdsample *)*m->instnum)))
	{
		retval=errAllocMem;
		goto safeout;
	}
	if (!(instsampnum=malloc(sizeof(int)*m->instnum)))
	{
		retval=errAllocMem;
		goto safeout;
	}
	if (!mpAllocInstruments(m, m->instnum)||!mpAllocPatterns(m, m->patnum)||!mpAllocTracks(m, m->tracknum)||!mpAllocEnvelopes(m, m->envnum)||!mpAllocOrders(m, m->ordnum))
	{
		retval=errAllocMem;
		goto safeout;
	}

	retval = _mpLoadAMS_v2_Instruments (m, file, filever, shadowedby, instsampnum, smps, msmps);
	if (retval) goto safeout;

	if (readPascalString (file, m->composer, sizeof (m->composer), "composer"))
	{
		retval=errFormStruc;
		goto safeout;
	}
	DEBUG_PRINTF (stderr, "composer=\"%s\"\n", m->composer);

	for (i=0; i<32; i++)
	{
		char channelname[12];
		if (readPascalString (file, channelname, sizeof (channelname), "channelname"))
		{
			retval=errFormStruc;
			goto safeout;
		}
		DEBUG_PRINTF (stderr, "channel[%d].name=\"%s\"\n", i, channelname);
	}

	if (ocpfilehandle_read_uint32_le (file, &packlen))
	{
		packlen = 0;
		fprintf (stderr, "AMSv2 loader: read description total length failed\n");
		retval=errFormStruc;
		goto safeout;
	}
	DEBUG_PRINTF (stderr, "description.length(packed)=0x%04"PRIx32"\n", packlen);
#warning Implement description... same format as v1 files after decompression?

	file->seek_cur (file, packlen - 4); /* we currently ignore description */

	if (file->read (file, ordlist, 2 * hdr.pos) != 2 * hdr.pos)
	{
		fprintf (stderr, "AMSv2 loader: read order list failed\n");
		goto safeout;
	}

	for (i=0; i<m->ordnum; i++)
	{
		m->orders[i] = uint16_little (ordlist[i]);
		if (m->orders[i] > hdr.pat)
		{
			m->orders[i] = hdr.pat;
		}
	}

	for (i=0; i<32; i++)
		m->patterns[hdr.pat].tracks[i]=m->tracknum-1;
	m->patterns[hdr.pat].gtrack=m->tracknum-1;
	m->patterns[hdr.pat].patlen=64;

#if 0
	temptrack=malloc(sizeof(unsigned char)*4000);
	if (!temptrack)
	{
		retval=errAllocMem;
		goto safeout;
	}
#endif

	m->channum=1; /* files are "fixed" at 32 channels, so we must rely on LoadPattern to detect this */

	for (t=0; t<hdr.pat; t++)
	{
#if  1
		struct AMSPattern pattern;

		retval = _mpLoadAMS_v2_LoadPattern (m, file, &pattern, &buffer, &buflen, t, hdr.pat, (filever == 0x200)?24:10, 1);
		if (retval) goto safeout;

		retval = _mpLoadAMS_ConvertPattern (m, file, &pattern, t);
		if (retval) goto safeout;
#else
		uint32_t patlen;
		uint8_t maxrow;
		uint8_t chan;
		/* uint8_t maxcmd; */
		char patname[11];
		struct gmdpattern *pp;

		uint8_t *tp;
		uint8_t *buf;
		uint8_t *cp;
		uint16_t row=0;
		uint8_t another=1;

		struct gmdtrack *trk;
		uint16_t len;

		DEBUG_PRINTF (stderr, "Pattern=%d/%d\n", t + 1, hdr.pat);

		if (ocpfilehandle_read_uint32_le (file, &patlen))
		{
			patlen = 0;
			fprintf (stderr, "AMSv2 loader: read pattern %d/%d length failed #24\n", t + 1, hdr.pat);
			retval=errFormStruc;
			goto safeout;
		}
		if (ocpfilehandle_read_uint8 (file, &maxrow))
		{
			maxrow = 0;
			fprintf (stderr, "AMSv2 loader: read pattern %d/%d data failed\n", t + 1, hdr.pat);
			retval=errFormStruc;
			goto safeout;
		}
		if (ocpfilehandle_read_uint8 (file, &chan))
		{
			chan = 0;
			fprintf (stderr, "AMSv2 loader: read pattern %d/%d PatternLength (#channels) failed\n", t + 1, hdr.pat);
			retval=errFormStruc;
			goto safeout;
		}
		if (readPascalString (file, patname, sizeof (patname), "pattern"))
		{
			fprintf (stderr, "AMSv2 loader: read pattern %d/%d name failed\n", t + 1, hdr.pat);
			retval=errFormStruc;
			goto safeout;
		}
		patlen-=3+strlen (patname);
		DEBUG_PRINTF (stderr, "Pattern[%d].name=\"%s\"\n", t, patname);
		/* maxcmd=chan>>5; */
		chan&=0x1F;
		chan++;
		if (chan>m->channum)
			m->channum=chan;

		pp=&m->patterns[t];
		for (i=0; i<32; i++)
			pp->tracks[i]=t*33+i;
		pp->gtrack=t*33+32;
		pp->patlen=maxrow+1;
		strcpy(pp->name, patname);

		if (patlen>buflen)
		{
			buflen=patlen;
			free(buffer);
			buffer=malloc(sizeof(unsigned char)*buflen);
			if (!buffer)
			{
				retval=errAllocMem;
				goto safeout;
			}
		}
		if (file->read (file, buffer, patlen) != patlen)
		{
			fprintf (stderr, "AMSv2 loader: read pattern %d/%d data failed\n", t + 1, hdr.pat);
			retval=errFormStruc;
			goto safeout;
		}

		for (i=0; i<chan; i++)
		{
			uint8_t *tp=temptrack;
			uint8_t *buf=buffer;

			unsigned int row=0;
			uint8_t another=1;

			uint8_t curchan;
			uint8_t anothercmd;
			uint8_t nte;
			uint8_t ins;
			uint8_t noteporta;
			int16_t delaynote;
			uint8_t cmds[7][2];
			uint8_t cmdnum;
			int16_t vol;
			int16_t pan;
			struct gmdtrack *trk;
			uint16_t len;

			while (row<=maxrow)
			{
				unsigned char *cp;

				if (!another||(*buf==0xFF))
				{
					if (another)
						buf++;
					row++;
					another=1;
					continue;
				}
				another=!(*buf&0x80);
				curchan=*buf&0x1F;
				anothercmd=1;
				nte=0;
				ins=0;
				noteporta=0;
				delaynote=-1;
				cmdnum=0;
				vol=-1;
				pan=-1;
/*
				if ((ordlist[0]==t)&&!row)
					pan=(i&1)?0xC0:0x40;
*/
				if (!(*buf++&0x40))
				{
					anothercmd=*buf&0x80;
					nte=*buf++&0x7F;
					ins=*buf++;

					/* taken from Velvet Studio source code, "AMS_Load.asm", Construct20: */
					if ((nte >= 1) && (filever == 0x200))
					{
						nte += 14;
					}
				}
				while (anothercmd)
				{
					anothercmd=*buf&0x80;
					cmds[cmdnum][0]=*buf++&0x7F;
					if (!(cmds[cmdnum][0]&0x40))
						switch (cmds[cmdnum][0])
						{
							case 0x08:
								pan=(*buf++&0x0F)*0x11;
								break;
							case 0x0C:
								vol=*buf++*2;
								break;
							case 0x03: case 0x05: case 0x15:
								noteporta=1;
								cmds[cmdnum++][1]=*buf++;
								break;
							case 0x0E:
								if ((*buf&0xF0)==0xD0)
									delaynote=*buf&0x0F;
								cmds[cmdnum++][1]=*buf++;
								break;
							default:
								cmds[cmdnum++][1]=*buf++;
						} else
							vol=(cmds[cmdnum][0]&0x3f)*4;
				}
				if (curchan!=i)
					continue;

				cp=tp+2;

				if ((ordlist[0]==t)&&!row)
					putcmd(&cp, cmdPlayNote|cmdPlayPan, (i&1)?0xC0:0x40);

				if (ins||nte||(vol!=-1)||(pan!=-1))
				{
					uint8_t *act=cp;

					*cp++=cmdPlayNote;
					if (ins)
					{
						*act|=cmdPlayIns;
						*cp++=ins-1;
					}
					if (nte>=2)
					{
						*act|=cmdPlayNte;
						*cp++=(nte+10)|(noteporta?0x80:0);
					}
					if (vol!=-1)
					{
						*act|=cmdPlayVol;
						*cp++=vol;
					}
					if (pan!=-1)
					{
						*act|=cmdPlayPan;
						*cp++=pan;
					}
					if (delaynote!=-1)
					{
						*act|=cmdPlayDelay;
						*cp++=delaynote;
					}
					if (nte==1)
						putcmd(&cp, cmdKeyOff, 0);
				}

				for (j=0; j<cmdnum; j++)
				{
					uint8_t data=cmds[j][1];
					switch (cmds[j][0])
					{
						case 0x0:
							if (data)
								putcmd(&cp, cmdArpeggio, data);
							break;
						case 0x1:
							putcmd(&cp, cmdPitchSlideUp, data);
							break;
						case 0x2:
							putcmd(&cp, cmdPitchSlideDown, data);
							break;
						case 0x3:
							putcmd(&cp, cmdPitchSlideToNote, data);
							break;
						case 0x4:
							putcmd(&cp, cmdPitchVibrato, data);
							break;
						case 0x5:
							putcmd(&cp, cmdPitchSlideToNote, 0);
							if (!data)
								putcmd(&cp, cmdSpecial, cmdContVolSlide);
							else if (data&0xF0)
								putcmd(&cp, cmdVolSlideUp, (data>>4)<<2);
							else
								putcmd(&cp, cmdVolSlideDown, (data&0xF)<<2);
							break;
						case 0x6:
							putcmd(&cp, cmdPitchVibrato, 0);
							if (!data)
								putcmd(&cp, cmdSpecial, cmdContVolSlide);
							else if (data&0xF0)
								putcmd(&cp, cmdVolSlideUp, (data>>4)<<2);
							else
								putcmd(&cp, cmdVolSlideDown, (data&0xF)<<2);
							break;
						case 0x7:
							putcmd(&cp, cmdVolVibrato, data);
							break;
						case 0x9:
							putcmd(&cp, cmdOffset, data);
							break;
						case 0xA:
							if (!data)
								putcmd(&cp, cmdSpecial, cmdContVolSlide);
							else if (data&0xF0)
								putcmd(&cp, cmdVolSlideUp, (data>>4)<<2);
							else
								putcmd(&cp, cmdVolSlideDown, (data&0xF)<<2);
							break;
						case 0xE:
							cmds[j][0]=data>>4;
							data&=0x0F;
							switch (cmds[j][0])
							{
								case 0x1:
									putcmd(&cp, cmdRowPitchSlideUp, data<<4);
									break;
								case 0x2:
									putcmd(&cp, cmdRowPitchSlideDown, data<<4);
									break;
								case 0x3:
									putcmd(&cp, cmdSpecial, data?cmdGlissOn:cmdGlissOff);
									break;
								case 0x4:
									if (data<4)
										putcmd(&cp, cmdPitchVibratoSetWave, data);
									break;
								case 0x7:
									if (data<4)
										putcmd(&cp, cmdVolVibratoSetWave, data);
									break;
								case 0x8:
									if (!(data&0x0F))
										putcmd(&cp, cmdSetLoop, 0);
									break;
								case 0x9:
									if (data)
										putcmd(&cp, cmdRetrig, data);
									break;
								case 0xA:
									putcmd(&cp, cmdRowVolSlideUp, data<<2);
									break;
								case 0xB:
									putcmd(&cp, cmdRowVolSlideDown, data<<2);
									break;
								case 0xC:
									putcmd(&cp, cmdNoteCut, data);
									break;
							}
							break;

						case 0x11:
							putcmd(&cp, cmdRowPitchSlideUp, data<<2);
							break;
						case 0x12:
							putcmd(&cp, cmdRowPitchSlideDown, data<<2);
							break;
						case 0x13:
							putcmd(&cp, cmdRetrig, data);
							break;
						case 0x15:
							putcmd(&cp, cmdPitchSlideToNote, 0);
							if (!data)
								putcmd(&cp, cmdSpecial, cmdContVolSlide);
							else if (data&0xF0)
								putcmd(&cp, cmdVolSlideUp, (data>>4)<<1);
							else
								putcmd(&cp, cmdVolSlideDown, (data&0xF)<<1);
							break;
						case 0x16:
							putcmd(&cp, cmdPitchVibrato, 0);
							if (!data)
								putcmd(&cp, cmdSpecial, cmdContVolSlide);
							else if (data&0xF0)
								putcmd(&cp, cmdVolSlideUp, (data>>4)<<1);
							else
								putcmd(&cp, cmdVolSlideDown, (data&0xF)<<1);
							break;
						case 0x18:
							if (!data)
								putcmd(&cp, cmdPanSlide, 0);
							else if (data&0xF0)
								putcmd(&cp, cmdPanSlide, data>>4);
							else
								putcmd(&cp, cmdPanSlide, -(data&0xF));
							/*
							if ((data&0x0F)&&(data&0xF0))
								break;
							if (data&0xF0)
								data=-(data>>4)*4;
							else
								data=data*4;
							putcmd(cp, cmdPanSlide, data);
							*/
							break;
						case 0x1A:
							if (!data)
								putcmd(&cp, cmdSpecial, cmdContVolSlide);
							else
								if (data&0xF0)
									putcmd(&cp, cmdVolSlideUp, (data>>4)<<1);
								else
									putcmd(&cp, cmdVolSlideDown, (data&0xF)<<1);
							break;
						case 0x1E:
							cmds[j][0]=data>>4;
							data&=0x0F;
							switch (cmds[j][0])
							{
								case 0x1:
									putcmd(&cp, cmdRowPitchSlideUp, data<<4);
									break;
								case 0x2:
									putcmd(&cp, cmdRowPitchSlideDown, data<<4);
									break;
								case 0xA:
									putcmd(&cp, cmdRowVolSlideUp, data<<1);
									break;
								case 0xB:
									putcmd(&cp, cmdRowVolSlideDown, data<<1);
									break;
							}
							break;
						case 0x1C:
							putcmd(&cp, cmdChannelVol, (data<=0x7F)?(data<<1):0xFF);
							break;
						case 0x20:
							putcmd(&cp, cmdKeyOff, data);
							break;
					}
				}

				if (cp!=(tp+2))
				{
					tp[0]=row;
					tp[1]=cp-tp-2;
					tp=cp;
				}
			}

			trk=&m->tracks[t*33+i];
			len=tp-temptrack;

			if (!len)
				trk->ptr=trk->end=0;
			else {
				trk->ptr=malloc(sizeof(unsigned char)*len);
				trk->end=trk->ptr+len;
				if (!trk->ptr)
				{
					retval=errAllocMem;
					goto safeout;
				}
				memcpy(trk->ptr, temptrack, len);
			}
		}

		tp=temptrack;
		buf=buffer;

		cp=tp+2;

		if (ordlist[0]==t)
		{
			if (hdr.bpm&0xFF00)
				putcmd(&cp, cmdTempo, hdr.bpm>>8);
			if (hdr.bpm&0xFF)
				putcmd(&cp, cmdFineSpeed, hdr.bpm);
			putcmd(&cp, cmdTempo, hdr.speed);
		}

		row=0;
		another=1;
		while (row<=maxrow)
		{
			uint8_t curchan;
			uint8_t anothercmd;
			uint8_t cmds[7][2];
			uint8_t cmdnum;

			if (!another||(*buf==0xFF))
			{
				if (another)
					buf++;
				if (cp!=(tp+2))
				{
					tp[0]=row;
					tp[1]=cp-tp-2;
					tp=cp;
					cp=tp+2;
				}
				row++;
				another=1;
				continue;
			}
			another=!(*buf&0x80);

			curchan=*buf&0x1F;
			anothercmd=1;
			cmdnum=0;
			if (!(*buf++&0x40))
			{
				anothercmd=*buf&0x80;
				buf+=2;
			}
			while (anothercmd)
			{
				anothercmd=*buf&0x80;
				cmds[cmdnum][0]=*buf++&0x7F;
				if (!(cmds[cmdnum][0]&0x40))
					cmds[cmdnum++][1]=*buf++;
			}

			if (curchan>=chan)
				continue;

			for (j=0; j<cmdnum; j++)
			{
				uint8_t data=cmds[j][1];
				switch (cmds[j][0])
				{
					case 0xB:
						putcmd(&cp, cmdGoto, data);
						break;
					case 0xD:
						putcmd(&cp, cmdBreak, (data&0x0F)+(data>>4)*10);
						break;
					case 0x1D:
						putcmd(&cp, cmdBreak, data);
						break;
					case 0xE:
						switch (data>>4)
						{
							case 0x6:
								putcmd(&cp, cmdSetChan, curchan);
								putcmd(&cp, cmdPatLoop, data&0xF);
								break;
							case 0xE:
								putcmd(&cp, cmdPatDelay, data&0xF);
								break;
						}
						break;
					case 0xF:
						if (data)
						{
							if (data<0x20)
								putcmd(&cp, cmdTempo, data);
							else
								putcmd(&cp, cmdSpeed, data);
						}
						break;
					case 0x1F:
						if (data<10)
							putcmd(&cp, cmdFineSpeed, data);
						break;
					case 0x2A:
						if ((data&0x0F)&&(data&0xF0))
							break;
						putcmd(&cp, cmdSetChan, curchan);
						if (data&0xF0)
							putcmd(&cp, cmdGlobVolSlide, (data>>4)<<2);
						else
							putcmd(&cp, cmdGlobVolSlide, -(data<<2));
					case 0x2C:
						putcmd(&cp, cmdGlobVol, data*2);
						break;
				}
			}
		}

		trk=&m->tracks[t*33+32];
		len=tp-temptrack;

		if (!len)
			trk->ptr=trk->end=0;
		else {
			trk->ptr=malloc(sizeof(unsigned char)*len);
			trk->end=trk->ptr+len;
			if (!trk->ptr)
			{
				retval=errAllocMem;
				goto safeout;
			}
			memcpy(trk->ptr, temptrack, len);
		}
#endif
	}
	free(ordlist);
#if 0
	free(temptrack);
#endif
	free(buffer);
	ordlist=0;
#if 0
	temptrack=0;
#endif
	buffer=0;

	if (!mpAllocSamples(m, m->sampnum)||!mpAllocModSamples(m, m->modsampnum))
	{
		retval=errAllocMem;
		goto safeout;
	}

	m->sampnum=0;
	m->modsampnum=0;

	for (i=0; i<m->instnum; i++)
	{
/*
		struct gmdinstrument *ip=&m->instruments[i];     NOT USED */
		for (j=0; j<instsampnum[i]; j++)
		{
			m->modsamples[m->modsampnum++]=msmps[i][j];
			m->samples[m->sampnum++]=smps[i][j];
		}
		free(msmps[i]);
		free(smps[i]);
	}

	free(smps);
	smps=0;
	free(msmps);
	msmps=0;

	sampnum=0;

	for (i=0; i<m->instnum; i++)
	{
		DEBUG_PRINTF (stderr, "Instrument=%d/%d (shadowdby=%d)\n", i + 1, m->instnum, shadowedby[i]);
		if (shadowedby[i])
		{
			sampnum+=instsampnum[i];
			continue;
		}
		retval = _mpLoadAMS_InstrumentSample (m, file, i, instsampnum, &sampnum);
		if (retval) goto safeout;
	}

	for (i=0; i<m->instnum; i++)
	{
		if (shadowedby[i])
		{
			for (j=0; j<instsampnum[i]; j++)
			{
				memcpy(m->instruments[i].samples, m->instruments[shadowedby[i]-1].samples, sizeof (m->instruments[i].samples));
			}
		}
	}

	free(instsampnum);
	instsampnum=0;

	return errOk;

safeout:
	if (instsampnum)
		free(instsampnum);
	if (ordlist)
		free(ordlist);
#if 0
	if (temptrack)
		free(temptrack);
#endif
	if (buffer)
		free(buffer);

	for (i=0; i<m->instnum; i++)
	{
		if (msmps && msmps[i])
			free(msmps[i]);
		if (smps && smps[i])
			free(smps[i]);
	}

	if (smps)
		free(smps);
	if (msmps)
		free(msmps);

	mpFree(m);

	return retval;
}
