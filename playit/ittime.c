/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'25 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * ITPlay timing/sync handlers for IMS
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
#include <signal.h>
#include <string.h>
#include <stdio.h> /* FILE * in itplay.h */
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include "types.h"
#include "itplay.h"

OCP_INTERNAL int it_precalctime (struct it_module *this, int startpos, int (*calctimer)[2], int calcn, int ite)
{
	uint8_t *patptr=0;

	int patdelaytick=0;
	int patdelayrow=0;
	int sync=-1;
	int looped=0;
	int gotorow=(startpos>>8)&0xFF;
	int gotoord=startpos&0xFF;
	int curord=-1;
	int currow=-1;

	int curspeed=this->inispeed;
	int curtick=this->inispeed-1;

	/* int loopord=0;   unused */
	int tempo=this->initempo;
	int timerval=0;
	int timerfrac=0;

	uint8_t tempos[64];
	uint8_t cmds[64];
	uint8_t specials[64];
	uint8_t patloopcount[64];
	uint8_t patloopstart[64];

	int it;

	memset(tempos, 0, this->nchan);
	memset(specials, 0, this->nchan);
	memset(cmds, 0, this->nchan);


	for (it=0; it<ite; it++)
	{
		int i;
		int p;

		curtick++;
		if ((curtick==(curspeed+patdelaytick))&&patdelayrow)
		{
			curtick=0;
			patdelayrow--;
		}
		if (curtick==(curspeed+patdelaytick))
		{
			patdelaytick=0;
			curtick=0;
			currow++;
			if ((gotoord==-1)&&(currow==this->patlens[this->orders[curord]]))
			{
				gotoord=curord+1;
				gotorow=0;
			}
			if (gotoord!=-1)
			{
				if (gotoord!=curord)
				{
					memset(patloopcount, 0, this->nchan);
					memset(patloopstart, 0, this->nchan);
				}

				if (gotoord>=this->endord)
					gotoord=0;
				while (this->orders[gotoord]==0xFFFF)
					gotoord++;
				if (gotoord==this->endord)
					gotoord=0;
				if (gotorow>=this->patlens[this->orders[gotoord]])
				{
					gotoord++;
					gotorow=0;
					while (this->orders[gotoord]==0xFFFF)
						gotoord++;
					if (gotoord==this->endord)
						gotoord=0;
				}
				if (gotoord<curord)
					looped=1;
				curord=gotoord;
				patptr=this->patterns[this->orders[curord]];
				for (currow=0; currow<gotorow; currow++)
				{
					while (*patptr)
						patptr+=6;
					patptr++;
				}
				gotoord=-1;
			}

			for (i=0; i<this->nchan; i++)
				cmds[i]=0;
			if (!patptr)
			{
				fprintf(stderr, "playit: ittime.c: patptr not set\n");
				abort();
			}
			while (*patptr)
			{
				int ch=*patptr++-1;

				int data=patptr[4];

				cmds[ch]=patptr[3];
				switch (cmds[ch])
				{
					case cmdSpeed:
						if (data)
							curspeed=data;
						break;
					case cmdJump:
						gotorow=0;
						gotoord=data;
						break;
					case cmdBreak:
						if (gotoord==-1)
							gotoord=curord+1;
						gotorow=data;
						break;
					case cmdSpecial:
						if (data)
							specials[ch]=data;
						switch (specials[ch]>>4)
						{
							case cmdSPatDelayTick:
								patdelaytick=specials[ch]&0xF;
								break;
							case cmdSPatLoop:
								if (!(specials[ch]&0xF))
									patloopstart[ch]=currow;
								else {
									patloopcount[ch]++;
									if (patloopcount[ch]<=(specials[ch]&0xF))
									{
										gotorow=patloopstart[ch];
										gotoord=curord;
									} else {
										patloopcount[ch]=0;
										patloopstart[ch]=currow+1;
									}
								}
								break;
							case cmdSPatDelayRow:
								patdelayrow=specials[ch]&0xF;
								break;
						}
						break;
					case cmdTempo:
						if (data)
							tempos[ch]=data;
						if (tempos[ch]>=0x20)
							tempo=tempos[ch];
						break;
#if 0
					TODO this upcode is invalid
					case cmdSync:
						sync=data;
						break;
#endif
				}
				patptr+=5;
			}
			patptr++;
		} else
			for (i=0; i<this->nchan; i++)
				if ((cmds[i]==cmdTempo)&&(tempos[i]<0x20))
				{
					tempo+=(tempos[i]<0x10)?-tempos[i]:(tempos[i]&0xF);
					tempo=(tempo<0x20)?0x20:(tempo>0xFF)?0xFF:tempo;
				}



		p=(curord<<16)|(currow<<8)|curtick;
		for (i=0; i<calcn; i++)
			if ((p==calctimer[i][0])&&(calctimer[i][1]<0))
				if (!++calctimer[i][1])
					calctimer[i][1]=timerval;

		if (sync!=-1)
			for (i=0; i<calcn; i++)
				if ((calctimer[i][0]==(-256-sync))&&(calctimer[i][1]<0))
					if (!++calctimer[i][1])
						calctimer[i][1]=timerval;

		sync=-1;

		if (looped)
			for (i=0; i<calcn; i++)
				if ((calctimer[i][0]==-1)&&(calctimer[i][1]<0))
					if (!++calctimer[i][1])
						calctimer[i][1]=timerval;

		looped=0;

		timerfrac+=4096*163840/tempo;
		timerval+=timerfrac>>12;
		timerfrac&=4095;

		for (i=0; i<calcn; i++)
			if (calctimer[i][1]<0)
				break;

		if (i==calcn)
			break;
	}

	return 1;
}
