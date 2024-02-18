/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
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

OCP_INTERNAL void xmpFreeModule (struct xmodule *m)
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
	memset (m, 0, sizeof (*m));
}

OCP_INTERNAL void xmpOptimizePatLens (struct xmodule *m)
{
	unsigned int i;
	uint8_t **detection=calloc(sizeof(uint8_t *), sizeof(uint8_t)*m->nord);

	if (!detection)
	{
		return;
	}

	/* cycle throught the song, with each order as the start-point (to simulate the user jumping into the song) */
	for (i=0; i<m->nord; i++)
	{
		unsigned int curord = i;
		unsigned int currow = 0;
		uint16_t curpat;    /* the pattern that the order points to */
		unsigned int curpatlen = 0; /* the original length */
		int orderchanged = 1;
		unsigned int k;
		unsigned int nextpatternrow = 0;

		while (1)
		{
			int_fast32_t neword = -1;
			unsigned int newrow = 0;

			if (orderchanged)
			{
				do
				{
					if (curord >= m->nord)
					{
						goto eof;
					}

					curpat = m->orders[curord];
					/* it this order a no-op ? */
					if (curpat == 0xFFFF)
					{
						curord++;
						continue;
					}

					curpatlen = m->patlens[curpat];
					if (curpatlen && (!detection[curord]))
					{
						detection[curord] = calloc (1, curpatlen);
						if (!detection[curord])
						{
							goto memoryabort;
						}
					}
				} while (0);
				orderchanged = 0;
			}

			if (currow >= curpatlen)
			{
				/* next order please */
				curord++;
				currow = nextpatternrow;
				nextpatternrow = 0;
				orderchanged = 1;
				continue;
			}

			if (detection[curord][currow])
			{
				break; /* we have been here before */
			}
			detection[curord][currow] = 1;

			/* search for Jump and Break commands */

			for (k=0; k<m->nchan; k++)
			{
				switch (m->patterns[curpat][m->nchan*currow+k][3])
				{
					case xmpCmdPatLoop:
						if (m->ft2_e60bug && (m->patterns[curpat][m->nchan*currow+k][4] == 0x00))
						{
							nextpatternrow = currow;
						}
						break;
					case xmpCmdJump:
						neword = m->patterns[curpat][m->nchan*currow+k][4];
						newrow = 0;
						break;
					case xmpCmdBreak:
						if (neword == -1)
						{
							neword = curord + 1;
						}
						newrow =  (m->patterns[curpat][m->nchan*currow+k][4] & 0x0f) +
						         ((m->patterns[curpat][m->nchan*currow+k][4] >> 4) * 10);

						break;
				}
			}

			if (neword!=-1)
			{
				/* Jump/Break found */
				while ((neword < m->nord) && (m->orders[neword] == 0xFFFF))
				{
					neword++;
				}

				if (neword >= m->nord)
				{
					/* jumps past EOF - stop the song */
					goto eof;
				}
				curord = neword;
				currow = newrow;
				orderchanged = 1;
			} else {
				/* next row please */
				currow++;
			}
		}
eof:{}
	}

	/* cycle all the patterns */
	for (i=0; i<m->npat; i++)
	{
		signed int j, k;
		signed int l = 0;
		for (k = 0; k < m->nord; k++)
		{
			/* find all orders that references this pattern */
			if (m->orders[k] == i)
			{

				/* find the biggest row that are in use */
				for (j=m->patlens[i] - 1; j >= 0; j--)
				{
					if (detection[k][j])
					{
						if ((j+1) > l)
						{
							l = j + 1;
						}
						break;
					}
				}
			}
		}
		/* our new lenght should be */
		m->patlens[i] = l;
	}
memoryabort:
	for (i=0; i<m->nord; i++)
	{
		free (detection[i]);
	}
	free(detection);
}
