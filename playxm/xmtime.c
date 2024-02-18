/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'24 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * XMPlay time routines for IMS
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
 *  -th980717   Tammo Hinrichs <opencp@gmx.net>
 *    -removed all references to gmd structures to make this more flexible
 */

#include "config.h"
#include "types.h"
#include "xmplay.h"

static uint8_t chPatLoopCount[256];
static uint8_t chPatLoopStart[256];

static uint8_t curtick;
static uint8_t curtempo;

static int looped;
static int currow;
static uint8_t (*patptr)[5];
static int patlen;
static int curord;

static int nord;
static int nchan;
static int loopord;
static uint8_t (**patterns)[5];
static uint16_t *orders;
static uint16_t *patlens;

static int jumptoord;
static int jumptorow;
static int patdelay;

static int timerval;
static int timerfrac;
static int speed;

static int (*calctimer)[2];
static int calcn;
static int sync;

static int xmpFindTick(void)
{
	int i;
	int p;

	curtick++;
	if (curtick>=curtempo)
		curtick=0;

	if (!curtick&&patdelay)
	{
		if (jumptoord!=-1)
		{
			if (jumptoord!=curord)
				for (i=0; i<nchan; i++)
				{
					chPatLoopCount[i]=0;
					chPatLoopStart[i]=0;
				}

			if (jumptoord>=nord)
				jumptoord=loopord;

			curord=jumptoord;
			currow=jumptorow;
			jumptoord=-1;
			patlen=patlens[orders[curord]];
			patptr=patterns[orders[curord]];
		}
	}

	if (!curtick&&patdelay)
	{
		patdelay--;
	} else if (!curtick)
	{
		currow++;
		if ((jumptoord==-1)&&(currow>=patlen))
		{
			jumptoord=curord+1;
			jumptorow=0;
		}
		if (jumptoord!=-1)
		{
			if (jumptoord!=curord)
				for (i=0; i<nchan; i++)
				{
					chPatLoopCount[i]=0;
					chPatLoopStart[i]=0;
				}

			if (jumptoord>=nord)
				jumptoord=loopord;
			if (jumptoord<curord)
				looped=1;

			curord=jumptoord;
			currow=jumptorow;
			jumptoord=-1;
			patlen=patlens[orders[curord]];
			patptr=patterns[orders[curord]];
		}

		for (i=0; i<nchan; i++)
		{
			int procdat=patptr[nchan*currow+i][4];

			switch (patptr[nchan*currow+i][3])
			{
				case xmpCmdSync1: case xmpCmdSync2: case xmpCmdSync3:
					sync=procdat;
					break;
				case xmpCmdJump:
					jumptoord=procdat;
					jumptorow=0;
					break;
				case xmpCmdBreak:
					if (jumptoord==-1)
						jumptoord=curord+1;
					jumptorow=(procdat&0xF)+(procdat>>4)*10;
					break;
				case xmpCmdSpeed:
					if (!procdat)
					{
						jumptoord=procdat;
						jumptorow=0;
						break;
					}
					if (procdat>=0x20)
						speed=procdat;
					else
						curtempo=procdat;
					break;
				case xmpCmdPatLoop:
					if (!procdat)
						chPatLoopStart[i]=currow;
					else {
						chPatLoopCount[i]++;
						if (chPatLoopCount[i]<=procdat)
						{
							jumptorow=chPatLoopStart[i];
							jumptoord=curord;
						} else {
							chPatLoopCount[i]=0;
							chPatLoopStart[i]=currow+1;
						}
					}
					break;
				case xmpCmdPatDelay:
					patdelay=procdat;
					break;
			}
		}
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

	timerfrac+=4096*163840/speed;
	timerval+=timerfrac>>12;
	timerfrac&=4095;

	for (i=0; i<calcn; i++)
		if (calctimer[i][1]<0)
			return 0;

	return 1;
}

OCP_INTERNAL int xmpPrecalcTime (struct xmodule *m, int startpos, int (*calc)[2], int n, int ite)
{
	int i;

	patdelay=0;
	sync=-1;
	looped=0;
	calcn=n;
	calctimer=calc;
	jumptorow=(startpos>>8)&0xFF;
	jumptoord=startpos&0xFF;
	curord=-1;
	currow=-1;
	nord=m->nord;
	patterns=m->patterns;
	orders=m->orders;
	patlens=m->patlens;
	nchan=m->nchan;
	loopord=m->loopord;

	curtempo=m->initempo;
	curtick=m->initempo-1;

	speed=m->inibpm;
	timerval=0;
	timerfrac=0;

	for (i=0; i<ite; i++)
		if (xmpFindTick())
			break;

	return 1;
}
