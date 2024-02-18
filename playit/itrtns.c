/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * ITPlay auxiliary routines
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
#include <stdio.h> /* FILE in itplay.h */
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "itplay.h"

OCP_INTERNAL void it_optimizepatlens (struct it_module *this)
{
	uint8_t *lastrows;
	int i;

	if (!(lastrows=malloc(sizeof(uint8_t)*this->npat)))
		return;
	memset(lastrows, 0, this->npat);
	for (i=0; i<this->nord; i++)
	{
		uint8_t *t, first;
		int row;
		int newpat;
		int newrow=0;

		if (this->orders[i]==0xFFFF)
			continue;
		t=this->patterns[this->orders[i]];
		first=0;
		row=0;
		newpat=-1;
		while (row<this->patlens[this->orders[i]])
		{
			if (!*t++)
			{
				if (newpat!=-1)
				{
					while ((newpat<this->nord)&&(this->orders[newpat]==0xFFFF))
						newpat++;
					if (newpat>=this->nord)
					{
						newpat=0;
						newrow=0;
					}
					if ((newrow>=this->patlens[this->orders[newpat]]))
					{
						newpat++;
						newrow=0;
					}
					if (newpat>=this->nord)
						newpat=0;
					if (newrow)
						lastrows[this->orders[newpat]]=this->patlens[this->orders[newpat]]-1;
					if (!first)
					{
						first=1;
						if (!lastrows[this->orders[i]])
							lastrows[this->orders[i]]=row;
					}
				}
				row++;
				newpat=-1;
			} else {
				switch (t[3])
				{
					case cmdJump:
						newpat=t[4];
						newrow=0;
						break;
					case cmdBreak:
						if (newpat==-1)
							newpat=i+1;
						newrow=t[4];
						break;
				}
				t+=5;
			}
		}
		if (!first)
			lastrows[this->orders[i]]=this->patlens[this->orders[i]]-1;
	}

	for (i=0; i<this->npat; i++)
		this->patlens[i]=lastrows[i]+1;
	free(lastrows);
}
