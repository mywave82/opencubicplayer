/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * GMDPlay time auxiliary routines
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
#include "gmdplay.h"

#if 0
static int timerval;
static int timerfrac;
static int gspeed;
static uint8_t currenttick;
static uint8_t tempo;
static uint16_t currentrow;
static uint16_t patternlen;
static uint16_t currentpattern;
static uint16_t patternnum;
static uint16_t looppat;
static uint16_t endpat;
static struct gmdtrack gtrack;
static const struct gmdpattern *patterns;
static const struct gmdtrack *tracks;
static const uint16_t *orders;
static uint16_t speed;
static int16_t brkpat;
static int16_t brkrow;
static uint8_t patlooprow[GMD_MAXLCHAN];
static uint8_t patloopcount[GMD_MAXLCHAN];
static uint8_t globchan;
static uint8_t patdelay;
static char looped;
static int (*calctimer)[2];
static int calcn;
static int sync;

static void trackmoveto(struct gmdtrack *t, uint8_t row)
{
	while (1)
	{
		if (t->ptr>=t->end)
			break;
		if (t->ptr[0]>=row)
			break;
		t->ptr+=t->ptr[1]+2;
	}
}

static void LoadPattern(uint16_t p, uint8_t r)
{
	const struct gmdpattern *pat=&patterns[orders[p]];
	patternlen=pat->patlen;
	if (r>=patternlen)
		r=0;
	currenttick=0;
	currentrow=r;
	currentpattern=p;

	gtrack=tracks[pat->gtrack];
	trackmoveto(&gtrack, r);
}

static void PlayGCommand(const uint8_t *cmd, uint8_t len)
{
	const uint8_t *cend=cmd+len;
	while (cmd<cend)
	{
		switch (*cmd++)
		{
			case cmdTempo:
				tempo=*cmd;
				break;
			case cmdSpeed:
				speed=*cmd;
				gspeed=speed*10;
				break;
			case cmdFineSpeed:
				gspeed=10*speed+*cmd;
				break;
			case cmdBreak:
				if (brkpat==-1)
				{
					brkpat=currentpattern+1;
					if (brkpat==endpat)
					{
						brkpat=looppat;
						looped=1;
					}
				}
				brkrow=*cmd;
				break;
			case cmdGoto:
				brkpat=*cmd;
				if (brkpat<=currentpattern)
					looped=1;
				break;
			case cmdPatLoop:
				if (*cmd)
					if (patloopcount[globchan]++<*cmd)
					{
						brkpat=currentpattern;
						brkrow=patlooprow[globchan];
					} else {
						patloopcount[globchan]=0;
						patlooprow[globchan]=currentrow+1;
					}
				else
					patlooprow[globchan]=currentrow;
				break;
			case cmdPatDelay:
				if (!patdelay&&*cmd)
					patdelay=*cmd+1;
				break;
			case cmdSetChan:
				globchan=*cmd;
				break;
		}
		cmd++;
	}
}

static int FindTick(void)
{
	int i, p;

	currenttick++;
	if (currenttick>=tempo)
		currenttick=0;

	if (!currenttick&&patdelay)
	{
		brkpat=currentpattern;
		brkrow=currentrow;
	}

	if (!currenttick/*&&!patdelay*/)
	{
		currenttick=0;

		currentrow++;

		if ((currentrow>=patternlen)&&(brkpat==-1))
		{
			brkpat=currentpattern+1;
			if (brkpat==endpat)
			{
				looped=1;
				brkpat=looppat;
			}
			brkrow=0;
		}
		if (brkpat!=-1)
		{
			if (currentpattern!=brkpat)
			{
				memset(patloopcount, 0, sizeof(patloopcount));
				memset(patlooprow, 0, sizeof(patlooprow));
			}
			currentpattern=brkpat;
			currentrow=brkrow;
			brkpat=-1;
			brkrow=0;
			while ((currentpattern<patternnum)&&(orders[currentpattern]==0xFFFF))
				currentpattern++;
			if ((currentpattern>=patternnum)||(currentpattern==endpat))
			{
				currentpattern=looppat;
				looped=1;
			}
			if (!currentpattern&&!currentrow&&!patdelay)
			{
				currentpattern=0;
				currentrow=0;
				tempo=6;
				speed=125;
				gspeed=1250;
			}
			LoadPattern(currentpattern, currentrow);
		}

		while (1)
		{
			if (gtrack.ptr>=gtrack.end)
				break;
			if (gtrack.ptr[0]!=currentrow)
				break;
			PlayGCommand(gtrack.ptr+2, gtrack.ptr[1]);
			gtrack.ptr+=gtrack.ptr[1]+2;
		}

		if (patdelay)
			patdelay--;
	}

	p=(currentpattern<<16)|(currentrow<<8)|currenttick;
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

	timerfrac+=1638400*1024/gspeed;
	timerval+=timerfrac>>10;
	timerfrac&=1023;

	for (i=0; i<calcn; i++)
		if (calctimer[i][1]<0)
			return 0;

	return 1;
}

int gmdPrecalcTime(struct gmdmodule *m, int ignore1, int (*calc)[2], int n, int ite)
{
	int i;

	if (m->orders[0]==0xFFFF)
		return 0;

	sync=-1;
	calcn=n;
	calctimer=calc;
	patterns=m->patterns;
	orders=m->orders;
	patternnum=m->ordnum;
	tracks=m->tracks;
	looppat=(m->loopord<m->ordnum)?m->loopord:0;
	while (m->orders[looppat]==0xFFFF)
		looppat--;

	endpat=m->endord;

	tempo=6;
	patdelay=0;
	patternlen=0;
	currenttick=tempo;
	currentrow=0;
	currentpattern=0;
	looped=0;
	brkpat=0;
	brkrow=0;
	speed=125;
	gspeed=1250;
	timerval=0;
	timerfrac=0;

	for (i=0; i<ite; i++)
		if (FindTick())
			return 1;

	return 0;
}
#endif
