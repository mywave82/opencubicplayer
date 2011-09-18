/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * XMPlay auxiliary routines
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
 *  -kb980717   Tammo Hinrichs <opencp@gmx.net>
 *    -removed all references to gmd structures to make this more flexible
 */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "dev/mcp.h"
#include "xmplay.h"
#include "stuff/err.h"

void __attribute__ ((visibility ("internal"))) xmpFreeModule(struct xmodule *m)
{
	unsigned int i;
	if (m->sampleinfos)
		for (i=0; i<m->nsampi; i++)
			free(m->sampleinfos[i].ptr);
	free(m->sampleinfos);
	free(m->samples);
	if (m->envelopes)
		for (i=0; i<m->nenv; i++)
			free(m->envelopes[i].env);
	free(m->envelopes);
	free(m->instruments);
	if (m->patterns)
		for (i=0; i<m->npat; i++)
			free(m->patterns[i]);
	free(m->patterns);
	free(m->patlens);
	free(m->orders);
}

void __attribute__ ((visibility ("internal"))) xmpOptimizePatLens(struct xmodule *m)
{
	uint8_t *lastrows=malloc(sizeof(uint8_t)*m->npat);
	unsigned int i,j,k;

	if (!lastrows)
		return;
	memset(lastrows, 0, m->npat);
	for (i=0; i<m->nord; i++)
	{
		int first;

		if (m->orders[i]==0xFFFF)
			continue;
		first=0;
		for (j=0; j<m->patlens[m->orders[i]]; j++)
		{
			unsigned int neword=(unsigned)-1;
			unsigned int newrow=newrow; /* supress warning.. neword needs to be set first, and that will set newrow aswell */
			for (k=0; k<m->nchan; k++)
				switch (m->patterns[m->orders[i]][m->nchan*j+k][3])
				{
					case xmpCmdJump:
						neword=m->patterns[m->orders[i]][m->nchan*j+k][4];
						newrow=0;
						break;
					case xmpCmdBreak:
						if (neword==(unsigned)-1)
							neword=i+1;
						newrow=m->patterns[m->orders[i]][m->nchan*j+k][4];
						break;
				}
			if (neword!=(unsigned)-1)
			{
				while ((neword<m->nord)&&(m->orders[neword]==0xFFFF))
					neword++;
				if (neword>=m->nord)
				{
					neword=0;
					newrow=0;
				}
				if ((newrow>=m->patlens[m->orders[neword]]))
				{
					neword++;
					newrow=0;
				}
				if (neword>=m->nord)
					neword=0;
				if (newrow)
					lastrows[m->orders[neword]]=m->patlens[m->orders[neword]]-1;
				if (!first)
				{
					first=1;
					if (!lastrows[m->orders[i]])
						lastrows[m->orders[i]]=j;
				}
			}
		}
		if (!first)
			lastrows[m->orders[i]]=m->patlens[m->orders[i]]-1;
	}

	for (i=0; i<m->npat; i++)
		m->patlens[i]=lastrows[i]+1;
	free(lastrows);
}
