/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * GMDPlay auxiliary routines
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "dev/mcp.h"
#include "gmdplay.h"

void __attribute__ ((visibility ("internal"))) mpOptimizePatLens(struct gmdmodule *m)
{
	uint8_t *lastrows=calloc(sizeof(uint8_t), m->patnum);
	unsigned int i;

	if (!lastrows)
		return;
	for (i=0; i<m->ordnum; i++)
	{
		struct gmdtrack t;
		uint8_t first;
		if (m->orders[i]==0xFFFF)
			continue;
		memcpy(&t, &m->tracks[m->patterns[m->orders[i]].gtrack], sizeof(struct gmdtrack));
		first=0;
		while (t.ptr<t.end)
		{
			uint8_t row=*t.ptr;
			uint16_t newpat=0xffff;
			uint8_t newrow=0;
			uint8_t *end;
			t.ptr+=2;

			for (end=t.ptr+t.ptr[-1]; t.ptr<end; t.ptr+=2)
				switch (t.ptr[0])
				{
					case cmdGoto:
						newpat=t.ptr[1];
						newrow=0;
						break;
					case cmdBreak:
						if (newpat==0xffff)
							newpat=i+1;
						newrow=t.ptr[1];
						break;
				}
			if (newpat!=0xffff)
			{
				while ((newpat<m->ordnum)&&(m->orders[newpat]==0xFFFF))
					newpat++;
				if (newpat>=m->ordnum)
				{
					newpat=0;
					newrow=0;
				}
				if ((newrow>=m->patterns[m->orders[newpat]].patlen))
				{
					newpat++;
					newrow=0;
				}
				if (newpat>=m->ordnum)
					newpat=0;
				if (newrow)
					lastrows[m->orders[newpat]]=m->patterns[m->orders[newpat]].patlen-1;
				if (!first)
				{
					first=1;
					if (!lastrows[m->orders[i]])
						lastrows[m->orders[i]]=row;
				}
			}
		}
		if (!first)
			lastrows[m->orders[i]]=m->patterns[m->orders[i]].patlen-1;
	}

	for (i=0; i<m->patnum; i++)
		m->patterns[i].patlen=lastrows[i]+1;
	free(lastrows);
}

void __attribute__ ((visibility ("internal"))) mpReduceInstruments(struct gmdmodule *m)
{
	unsigned int i,j;
	char *inptr;
	for (i=0; i<m->modsampnum; i++)
	{
		struct gmdsample *smp=&m->modsamples[i];
		for (inptr=smp->name; *inptr==' '; inptr++);
		if (!*inptr)
			*smp->name=0;
	}
	for (i=0; i<m->instnum; i++)
	{
		struct gmdinstrument *ins=&m->instruments[i];
		for (inptr=ins->name; *inptr==' '; inptr++);
		if (!*inptr)
			*ins->name=0;
		for (j=0; j<128; j++)
			if ((ins->samples[j]<m->modsampnum)&&(m->modsamples[ins->samples[j]].handle>=m->sampnum))
				ins->samples[j]=0xFFFF;
	}

	for (i=m->instnum-1; (signed)i>=0; i--)
	{
		struct gmdinstrument *ins=&m->instruments[i];
		for (j=0; j<128; j++)
			if ((ins->samples[j]<m->modsampnum)&&(m->modsamples[ins->samples[j]].handle<m->sampnum))
				break;
		if ((j!=128)||*m->instruments[i].name)
			break;
		m->instnum--;
	}
}

void __attribute__ ((visibility ("internal"))) mpReduceMessage(struct gmdmodule *m)
{
	char *mptr;
	int len,i;

	for (mptr=m->name; *mptr==' '; mptr++);
	if (!*mptr)
		*m->name=0;
	for (mptr=m->composer; *mptr==' '; mptr++);
	if (!*mptr)
		*m->composer=0;

	if (!m->message)
		return;
	for (len=0; m->message[len]; len++)
	{
		for (mptr=m->message[len]; *mptr==' '; mptr++);
		if (!*mptr)
			*m->message[len]=0;
	}
	for (i=len-1; i>=0; i--)
	{
		if (*m->message[i])
			break;
		if (i)
			m->message[i]=0;
		else {
			free(*m->message);
			free(m->message);
			m->message=0;
		}
	}
}

int __attribute__ ((visibility ("internal"))) mpReduceSamples(struct gmdmodule *m)
{
	uint16_t *rellist=malloc(sizeof(uint16_t)*m->sampnum);
	unsigned int i,n;

	if (!rellist)
		return 0;

	n=0;
	for (i=0; i<m->sampnum; i++)
	{
		if (!m->samples[i].ptr)
		{
			rellist[i]=0xFFFF;
			continue;
		}
		m->samples[n]=m->samples[i];
		rellist[i]=n++;
	}

	for (i=0; i<m->modsampnum; i++)
		if (m->modsamples[i].handle<m->sampnum)
			m->modsamples[i].handle=rellist[m->modsamples[i].handle];

	m->sampnum=n;
	free(rellist);
	return 1;
}

void mpReset(struct gmdmodule *m)
{
	m->instruments=0;
	m->tracks=0;
	m->patterns=0;
	m->message=0;
	m->samples=0;
	m->modsamples=0;
	m->envelopes=0;
	m->orders=0;
	*m->composer=0;
	*m->name=0;
}

void mpFree(struct gmdmodule *m)
{
	unsigned int i;

	if (m->envelopes)
		for (i=0; i<m->envnum; i++)
			free(m->envelopes[i].env);
	if (m->tracks)
		for (i=0; i<m->tracknum; i++)
				free(m->tracks[i].ptr);
	if (m->message)
		free(*m->message);
	if (m->samples)
		for (i=0; i<m->sampnum; i++)
			free(m->samples[i].ptr);

	free(m->tracks);
	free(m->patterns);
	free(m->message);
	free(m->samples);
	free(m->envelopes);
	free(m->instruments);
	free(m->modsamples);
	free(m->orders);

	mpReset(m);
}

int mpAllocInstruments(struct gmdmodule *m, int n)
{
	unsigned int i;

	m->instnum=n;
	m->instruments=calloc(sizeof(struct gmdinstrument), m->instnum);
	if (!m->instruments)
		return 0;
	for (i=0; i<m->instnum; i++)
		memset(m->instruments[i].samples, -1, 2*128);
	return 1;
}

int mpAllocTracks(struct gmdmodule *m, int n)
{
	m->tracknum=n;
	m->tracks=calloc(sizeof(struct gmdtrack), m->tracknum);
	if (!m->tracks)
		return 0;
	return 1;
}

int mpAllocPatterns(struct gmdmodule *m, int n)
{
	m->patnum=n;
	m->patterns=calloc(sizeof(struct gmdpattern), m->patnum);
	if (!m->patterns)
		return 0;
	return 1;
}

int mpAllocSamples(struct gmdmodule *m, int n)
{
	m->sampnum=n;
	m->samples=calloc(sizeof(struct sampleinfo), m->sampnum);
	if (!m->samples)
		return 0;
	return 1;
}

int mpAllocEnvelopes(struct gmdmodule *m, int n)
{
	m->envnum=n;
	m->envelopes=calloc(sizeof(struct gmdenvelope), m->envnum);
	if (!m->envelopes)
		return 0;
	return 1;
}

int mpAllocOrders(struct gmdmodule *m, int n)
{
	m->ordnum=n;
	m->orders=calloc(sizeof(uint16_t), m->ordnum);
	if (!m->orders)
		return 0;
	return 1;
}

int mpAllocModSamples(struct gmdmodule *m, int n)
{
	unsigned int i;

	m->modsampnum=n;
	m->modsamples=calloc(sizeof(struct gmdsample), m->modsampnum);
	if (!m->modsamples)
		return 0;
	for (i=0; i<m->modsampnum; i++)
	{
		m->modsamples[i].volfade=0xFFFF;
		m->modsamples[i].volenv=0xFFFF;
		m->modsamples[i].panenv=0xFFFF;
		m->modsamples[i].pchenv=0xFFFF;
		m->modsamples[i].handle=0xFFFF;
	}
	return 1;
}
