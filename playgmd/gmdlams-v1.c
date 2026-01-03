/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * GMDPlay loader for Extreme Tracker modules.
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

static int _mpLoadAMS_v1_Instruments (struct cpifaceSessionAPI_t *cpifaceSession, struct gmdmodule *m, struct ocpfilehandle_t *file, struct sampleinfo **smps, struct gmdsample **msmps)
{
	int i;
	/* one sample per instrument */
	for (i = 0; i < m->instnum; i++)
	{
		struct __attribute__ ((packed))
		{
			uint32_t length;
			uint32_t loopstart;
			uint32_t loopend;
			uint8_t finetune_and_pan;
			uint16_t samplerate;        // C-2 = 8363
			uint8_t volume;            // 0-127
			uint8_t infobyte;
		} samplehdr; //AMSSAMPLEHEADER;

		struct gmdinstrument *ip=&m->instruments[i];
		struct gmdsample *sp=&msmps[i][0];
		struct sampleinfo *sip=&smps[i][0];
		int k;

		if (file->read (file, &samplehdr, sizeof (samplehdr)) != sizeof (samplehdr))
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[GMD/AMS v1] read sample %d header\n", i + 1);
			return errFileRead;
		}
		samplehdr.length = uint32_little (samplehdr.length);
		samplehdr.loopstart = uint32_little (samplehdr.loopstart);
		samplehdr.loopend = uint32_little (samplehdr.loopend);
		samplehdr.samplerate = uint16_little (samplehdr.samplerate);
		DEBUG_PRINTF ("[GMD/AMS v1] sample[%d].length=0x%04"PRIx32"\n", i, samplehdr.length);
		DEBUG_PRINTF ("[GMD/AMS v1] sample[%d].loopstart=0x%04"PRIx32"\n", i, samplehdr.loopstart);
		DEBUG_PRINTF ("[GMD/AMS v1] sample[%d].loopend=0x%04"PRIx32"\n", i, samplehdr.loopend);
		DEBUG_PRINTF ("[GMD/AMS v1] sample[%d].finetune_and_pan=0x%02"PRIx8"\n", i, samplehdr.finetune_and_pan);
		DEBUG_PRINTF ("[GMD/AMS v1] sample[%d].samplerate=0x%04"PRIx16"\n", i, samplehdr.samplerate);
		DEBUG_PRINTF ("[GMD/AMS v1] sample[%d].volume=0x%02"PRIx8"\n", i, samplehdr.volume);
		DEBUG_PRINTF ("[GMD/AMS v1] sample[%d].infobyte=0x%02"PRIx8"\n", i, samplehdr.infobyte);

		for (k=0; k<116; k++)
		{
			ip->samples[k+12]=i;
		}

		sip->length = samplehdr.length;
		sp->stdpan=(samplehdr.finetune_and_pan&0xF0)?((samplehdr.finetune_and_pan>>4)*0x10):-1;
		sp->stdvol=samplehdr.volume * 2;
		sp->normnote=-(int)((signed char)(samplehdr.finetune_and_pan<<4))*32;
		sp->opt=(samplehdr.infobyte & 0x80)?MP_OFFSETDIV2:0;
		//sp->volfade=volfade;
		//sp->pchint=pchint;
		//sp->volenv=m->envelopes[3*i+0].env?(3*(signed)i+0):-1;
		//sp->panenv=m->envelopes[3*i+1].env?(3*(signed)i+1):-1;
		//sp->pchenv=m->envelopes[3*i+2].env?(3*(signed)i+2):-1;
		sip->loopstart=samplehdr.loopstart;
		sip->loopend=samplehdr.loopend;
		sip->samprate=samplehdr.samplerate;
		sip->type=((samplehdr.infobyte & 0x80) ?mcpSamp16Bit : 0);
		if ((sip->loopend <= sip->length) && ((sip->loopstart+4) <= sip->loopend))
		{
			sip->type |= mcpSampLoop;
		}
		if ((samplehdr.infobyte & 0x03) && 1)
		{
			sip->type |= mcpSampRedStereo; /* we borrow this flag, in order to remember to decompress.... */
		}
	}

	return errOk;
}

static int _mpLoadAMS_v12_v13_LoadPattern (struct cpifaceSessionAPI_t *cpifaceSession, struct gmdmodule *m, struct ocpfilehandle_t *file, struct AMSPattern *pattern, uint8_t **buffer, uint32_t *buflen, int patternindex, int nopatterns, int nteoffset, int nte1keyoff)
{
	uint32_t patlen;
	uint32_t patpos;
	unsigned int rowpos;

	if (ocpfilehandle_read_uint32_le (file, &patlen))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/AMS v1] read pattern %d/%d length failed\n", patternindex + 1, nopatterns);
		patlen = 0;
		return errFormStruc;
	}

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
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/AMS v1] read pattern %d/%d data (patlen=%d) failed\n", patternindex + 1, nopatterns, patlen);
		return errFileRead;
	}

	/* first we unpack the pattern */
	memset (pattern, 0, sizeof (*pattern));
	patpos = 0;
	rowpos = 0;
	pattern->rowcount = 64;
	while (((patpos+2) < patlen) && (rowpos < pattern->rowcount))
	{
/*
fpmiiiii eppppppp ssssssss [rgcccccc bbbbbbbb...]

Legend:
f       1=Last data chunk on the row.
p       0=Only Read Period+SampleNr, 1=Only Read Command
m       0=Sample Channel, 1=MIDI channel
i       Channel (Samp 0-31, MIDI 0-15)
e       1=Read one command
p       Period. Note 12-108 (C-0 to B-7)
s       SampleNr (0-255)
r       1=Read one more command
g       1=Low 6 bits are volume/2
c       Command-nr
b       Command-byte

If g is set, the command only consist of that byte and
the low 6 bits are assumed to be a volume command. You
have to multiply the volume by 2 to get the proper value
(Volume will only be stored this way if it's dividible with 2)
*/
		uint8_t b0, b1, b2, ch;

		b0 = (*buffer)[patpos++];

		DEBUG_PRINTF ("[GMD/AMS v1] pattern %02x rowpos %02x  %02x", patternindex, rowpos, b0);

		if (b0 == 0xff)
		{
			DEBUG_PRINTF ("     no data on row\n");
			rowpos++;
			continue;
		}

		ch = b0 & 0x1f;
		b1 = (*buffer)[patpos++];
		DEBUG_PRINTF (" %02x", b1);

		if (!(b0 & 0x40)) /* 0=Only Read Period+SampleNr, 1=Only Read Command */
		{
			DEBUG_PRINTF (" ins+note");
			if (patpos >= patlen) break;
			b2 = (*buffer)[patpos++];
			if (!(b0 & 0x20)) /* 0=Sample Channel, 1=MIDI channel */
			{
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
				pattern->channel[ch].row[rowpos].instrument = b2;
				pattern->channel[ch].row[rowpos].fill |= FILL_INSTRUMENT;
			}
			if (b1 & 0x80) /* 1=Read one command */
			{
				if (patpos >= patlen) break;
				b0 |= 0x40;
				b1 = (*buffer)[patpos++];
				DEBUG_PRINTF (" %02x", b1);
			}
		}

		if (b0 & 0x40)
		{
anothercommand:
			if (b1 & 0x40) /* 1=Low 6 bits are volume/2 */
			{
				DEBUG_PRINTF (" vol");
				if (!(b0 & 0x20)) /* 0=Sample Channel, 1=MIDI channel */
				{
					pattern->channel[ch].row[rowpos].volume = (b1 & 0x3f) << 1;
					pattern->channel[ch].row[rowpos].fill |= FILL_VOLUME;
				}
			} else {
				if (patpos >= patlen) break;
				b2 = (*buffer)[patpos++];
				DEBUG_PRINTF (" %02x cmd", b2);

				if (!(b0 & 0x20)) /* 0=Sample Channel, 1=MIDI channel */
				{
					uint8_t cmd = b1 & 0x3f;

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
			if (b1 & 0x80)
			{
				if (patpos >= patlen) break;
				b1 = (*buffer)[patpos++];
				DEBUG_PRINTF (" %02x", b1);
				goto anothercommand;
			}
		}
		if (b0 & 0x80)
		{
			rowpos++;
		}
		DEBUG_PRINTF ("\n", b1);
	}

	return errOk;
}


/* Extreme Tracker */
/* file should be 7 bytes in */
static int _mpLoadAMS_v1 (struct cpifaceSessionAPI_t *cpifaceSession, struct gmdmodule *m, struct ocpfilehandle_t *file)
{
	struct __attribute__ ((packed))
	{
		/* char szHeader[7]; "Extreme" */
		uint8_t verlo, verhi; // 0x??,0x01
		uint8_t chncfg;
		uint8_t samples;
		uint16_t patterns;
		uint16_t orders;
		uint8_t vmidi;
		uint16_t extra;
	} hdr;

	uint16_t *ordlist=0;
	struct sampleinfo **smps=0;
	struct gmdsample **msmps=0;
	unsigned int *instsampnum=0;

	uint32_t buflen=0;
	uint8_t *buffer=0;

	int sampnum;
	int i, j, t;
	int retval;

	mpReset(m);

	if (file->read (file, &hdr, sizeof (hdr)) != sizeof (hdr))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/AMS v1] read header failed\n");
		return errFileRead;
	}

	hdr.patterns = uint16_little (hdr.patterns);
	hdr.orders = uint16_little (hdr.orders);
	hdr.extra = uint16_little (hdr.extra);
	m->channum = (hdr.chncfg & 0x1f) + 1;

	DEBUG_PRINTF ("[GMD/AMS v1] header.verlo=0x%02"PRIx8"\n", hdr.verlo);
	DEBUG_PRINTF ("[GMD/AMS v1] header.verhi=0x%02"PRIx8"\n", hdr.verhi);
	DEBUG_PRINTF ("[GMD/AMS v1] header.chncfg=0x%02"PRIx8"\n", hdr.chncfg);
	DEBUG_PRINTF ("[GMD/AMS v1] header.samples=0x%02"PRIx8"\n", hdr.samples);
	DEBUG_PRINTF ("[GMD/AMS v1] header.patterns=0x%04"PRIx16"\n", hdr.patterns);
	DEBUG_PRINTF ("[GMD/AMS v1] header.orders=0x%04"PRIx16"\n", hdr.orders);
	DEBUG_PRINTF ("[GMD/AMS v1] header.vmidi=0x%02"PRIx8"\n", hdr.vmidi);
	DEBUG_PRINTF ("[GMD/AMS v1] header.extra=0x%04"PRIx16"\n", hdr.extra);

	if ((hdr.verhi != 0x01) || ((hdr.verlo != 0x20) && hdr.verlo != 0x30))
	{
		/* v01.00 has a uniqe pattern file-format - need test data
		   v01.10 has a uniqe pattern file-format - need test data

		   v01.20 and v01.30 files we can load
		 */
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/AMS v1] we can only load v1.2 and v1.3 files at the moement - please submit file at https://github.com/mywave82/opencubicplayer/issues\n");
		return errFormOldVer;
	}

	file->seek_set (file, file->getpos(file) + hdr.extra);

	if ((!hdr.patterns) || (!hdr.orders))
	{
		retval=errFormMiss;
		goto safeout;
	}

	m->instnum=hdr.samples;
	m->modsampnum=hdr.samples;
	m->sampnum=hdr.samples;
	m->patnum=hdr.patterns + 1; /* +1: temporary make space for dummy pattern */
	m->ordnum=hdr.orders;
	m->endord=hdr.orders;
	m->tracknum=(m->channum + 1)*hdr.patterns + 1; /* channels + 1 global track  * patterns + termination */
	m->loopord=0;

	if (!(ordlist=malloc(sizeof(uint16_t)*hdr.orders)))
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
        for (i = 0; i < m->instnum; i++)
        {
		msmps[i]=calloc(1, sizeof(struct gmdsample)*m->instnum);
		smps[i]=calloc(1, sizeof(struct sampleinfo)*m->instnum);
		if (!smps[i]||!msmps[i])
		{
			retval=errAllocMem;
			goto safeout;
		}
		msmps[i][0].handle=0xFFFF;
		msmps[i][0].volenv=0xFFFF;
		msmps[i][0].panenv=0xFFFF;
		msmps[i][0].pchenv=0xFFFF;
		msmps[i][0].volfade=0xFFFF;
		instsampnum[i]=1;
	}

	if (!mpAllocInstruments(m, m->instnum)||!mpAllocPatterns(m, m->patnum)||!mpAllocTracks(m, m->tracknum)/*||!mpAllocEnvelopes(m, m->envnum)*/||!mpAllocOrders(m, m->ordnum))
	{
		retval=errAllocMem;
		goto safeout;
	}

	retval = _mpLoadAMS_v1_Instruments (cpifaceSession, m, file, smps, msmps);
	if (retval) goto safeout;

	// Read Song Name
	if (readPascalString (cpifaceSession, file, m->name, sizeof (m->name), "module.name"))
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/AMS v1] read module name failed\n");
		retval=errFormStruc;
		goto safeout;
	}
	DEBUG_PRINTF ("[GMD/AMS v1] songname=\"%s\"\n", m->name);

	for (i = 0; i < m->instnum; i++)
	{
		struct gmdsample *sp=&msmps[i][0];
		//struct sampleinfo *sip=&smps[i][0];
		if (readPascalString (cpifaceSession, file, sp->name, sizeof (sp->name), "sample.name"))
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[GMD/AMS v1] read sample %d/%d name failed\n", i + 1, m->instnum);
			retval=errFormStruc;
			goto safeout;
		}
		DEBUG_PRINTF ("[GMD/AMS v1] sample[%d].name=\"%s\"\n", i, sp->name);
	}
	// Skip Channel names
	for (i = 0; i < m->channum; i++)
	{
		char channelname[33];
		if (readPascalString (cpifaceSession, file, channelname, sizeof (channelname), ""))
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[GMD/AMS v1] read channel %d/%d name failed\n", i + 1, m->channum);
			retval=errFormStruc;
			goto safeout;
		}
		DEBUG_PRINTF ("[GMD/AMS v1] channel[%d].name=\"%s\"\n", i, channelname);
	}
	// Read Pattern names
	for (i = 0; i < hdr.patterns; i++)
	{
		struct gmdpattern *pp=&m->patterns[i];
		if (readPascalString (cpifaceSession, file, pp->name, sizeof (pp->name), ""))
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[GMD/AMS v1] read pattern %d/%d name failed\n", i + 1, hdr.patterns);
			retval=errFormStruc;
			goto safeout;
		}
		DEBUG_PRINTF ("[GMD/AMS v1] pattern[%d].name=\"%s\"\n", i, pp->name);
	}

	{ /* read song message */
		uint16_t length;
		if (ocpfilehandle_read_uint16_le (file, &length))
		{
			cpifaceSession->cpiDebug (cpifaceSession, "[GMD/AMS v1] read message length failed\n");
			retval=errFormStruc;
			goto safeout;
		}
		DEBUG_PRINTF ("[GMD/AMS v1] message.length=%d\n", length);
		if (length)
		{
			uint8_t *tmp = malloc (length + 1);
			uint8_t *ptr1, *ptr2;
			if (!tmp)
			{
				retval = errAllocMem;
				goto safeout;
			}
			if (file->read (file, tmp, length) != length)
			{
				cpifaceSession->cpiDebug (cpifaceSession, "[GMD/AMS v1] read message data failed\n");
				retval = errFileRead;
				goto safeout;
			}
			tmp[length]=0;
			for (i=0, j=1; i < length; i++)
			{
				if (tmp[i] == 0x81) tmp[i]=' ';
				if (tmp[i] == 0x82) { tmp[i]='\n'; j++;}
			}

			m->message = calloc (j+1, sizeof (m->message[0]));
			if (!m->message)
			{
				free (tmp);
				retval=errAllocMem;
				goto safeout;
			}

			for (ptr1=tmp,j=0,ptr2=(uint8_t *)strchr((char *)tmp, '\n');;ptr2=(uint8_t *)strchr ((char *)ptr1, '\n'))
			{
				if (ptr2) *(ptr2++)=0;
				m->message[j++] = strdup((char *)ptr1);
				DEBUG_PRINTF ("[GMD/AMS v1] message[%d]=\"%s\"\n", j-1, m->message[j-1]);
				if (!m->message[j-1])
				{
					free (tmp);
					retval=errAllocMem;
					goto safeout;
				}
				if (ptr2)
				{
					ptr1=ptr2;
				} else {
					break;
				}
			}
		}
	}

	if (file->read (file, ordlist, 2 * hdr.orders) != 2 * hdr.orders)
	{
		cpifaceSession->cpiDebug (cpifaceSession, "[GMD/AMS v1] read order data failed\n");
		retval = errFileRead;
		goto safeout;
	}

	/* Read Order List */
	for (i=0; i<m->ordnum; i++)
	{
		m->orders[i] = uint16_little (ordlist[i]);
		if (m->orders[i] > hdr.patterns)
		{
			m->orders[i] = hdr.patterns;
		}
	}

	/* initialize the dummy pattern */
	for (i=0; i < 32; i++)
	{
		m->patterns[hdr.patterns].tracks[i]=m->tracknum-1; /* termination */
	}
	m->patterns[hdr.patterns].gtrack=m->tracknum-1;
	m->patterns[hdr.patterns].patlen=64;

	for (t=0; t < hdr.patterns; t++)
	{
		struct AMSPattern pattern;

		retval = _mpLoadAMS_v12_v13_LoadPattern (cpifaceSession, m, file, &pattern, &buffer, &buflen, t, hdr.patterns, 24, 0);
		if (retval) goto safeout;

		retval = _mpLoadAMS_ConvertPattern (m, file, &pattern, t);
		if (retval) goto safeout;
	}
	free(ordlist);
	free(buffer);
	ordlist=0;
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

	/* There should be a 1:1 relationship between instruments and samples for v1 files */
	for (i=0; i<m->instnum; i++)
	{
		DEBUG_PRINTF ("[GMD/AMS v1] Instrument=%d/%d\n", i + 1, m->instnum);
		retval = _mpLoadAMS_InstrumentSample (cpifaceSession, m, file, i, instsampnum, &sampnum);
		if (retval) goto safeout;
	}

	free(instsampnum);
	instsampnum=0;

	return errOk;

safeout:
	if (instsampnum)
		free(instsampnum);
	if (ordlist)
		free(ordlist);
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
