/* OpenCP Module Player
 * copyright (c) 2005-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * This file is based on AdPlug project, released under GPLv2
 * with permission from Simon Peter.
 *
 * AdPlug - Replayer for many OPL2/OPL3 audio file formats.
 * Copyright (C) 1999 - 2002 Simon Peter <dn.tlp@gmx.net>, et al.
 * emuopl.cpp - Emulated OPL, by Simon Peter <dn.tlp@gmx.net>
 *
 * File is changed in letting FM_OPL be public
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

 */

#include "ocpemu.h"
#include <string.h>
#include <math.h>

static const int slot_array[32]=
{
	0, 2, 4, 1, 3, 5,-1,-1,
        6, 8,10, 7, 9,11,-1,-1,
        12,14,16,13,15,17,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1
};


Cocpopl::Cocpopl(int rate)
{
  opl = OPLCreate(OPL_TYPE_YM3812, 3579545, rate);
}

Cocpopl::~Cocpopl()
{
  OPLDestroy(opl);
}

void Cocpopl::update(short *buf, int samples)
{
	int i;

	YM3812UpdateOne(opl,buf,samples);

	for(i=samples-1;i>=0;i--) {
		buf[i*2] = buf[i];
		buf[i*2+1] = buf[i];
	}
}

void Cocpopl::write(int reg, int val)
{
	int slot = slot_array[reg&0x1f];

	switch(reg&0xe0)
	{
		case 0xe0:
			if (slot==-1)
				goto done;
			wavesel[slot]=val&3;
			break;
		case 0x40:
			if (slot==-1)
				goto done;
	                hardvols[slot][0] = val;
			if (mute[slot])
				return;
			break;
		case 0xc0:
			if (slot==-1)
				goto done;
			if (reg<=0xc8)
			{
		                hardvols[reg-0xc0][1] = val;
				if (mute[reg-0xc0]&&mute[reg-0xc0+9])
					return;
			}
			break;
	}

done:
	OPLWrite(opl,0,reg);
	OPLWrite(opl,1,val);
}

/* envelope counter lower bits */
#define ENV_BITS 16
/* envelope output entries */
#define EG_ENT   4096
static INT32 ENV_CURVE[2*EG_ENT+1];
#define EG_OFF   ((2*EG_ENT)<<ENV_BITS)  /* OFF          */
#define EG_DED   EG_OFF
#define EG_DST   (EG_ENT<<ENV_BITS)      /* DECAY  START */
#define EG_AED   EG_DST
#define EG_AST   0                       /* ATTACK START */

int Cocpopl::vol(int i)
{
	OPL_CH *CH = &opl->P_CH[i/2];
	OPL_SLOT *SLOT = &CH->SLOT[i&1];
	unsigned int ofs;
	ofs=SLOT->evc>>ENV_BITS;
	if (ofs>=sizeof(2*EG_ENT))
		ofs=2*EG_ENT;
	return SLOT->TLL+ENV_CURVE[SLOT->evc>>ENV_BITS];
}

void Cocpopl::init()
{
	OPLResetChip(opl);
	memset(wavesel, 0, sizeof(wavesel));
	memset(hardvols, 0, sizeof(hardvols));
	memset(mute, 0, sizeof(mute));

        /* envelope counter -> envelope output table */
	for (int i=0; i<EG_ENT; i++)
	{
		/* ATTACK curve */
		double pom = pow( ((double)(EG_ENT-1-i)/EG_ENT) , 8 ) * EG_ENT;
		/* if( pom >= EG_ENT ) pom = EG_ENT-1; */
		ENV_CURVE[i] = (int)pom;
		/* DECAY ,RELEASE curve */
		ENV_CURVE[(EG_DST>>ENV_BITS)+i]= i;
	}
	/* off */
	ENV_CURVE[EG_OFF>>ENV_BITS]= EG_ENT-1;
}

void Cocpopl::setmute(int chan, int val)
{
	int i;
	mute[chan]=val;
	for (i=0;i<32;i++)
	{
		int slot = slot_array[i];
		if (slot<0)
			continue;
		OPLWrite(opl, 0, i+0x40);
		if ((mute[slot]))
			OPLWrite(opl, 1, 63);
		else
			OPLWrite(opl, 1, hardvols[slot][0]);
	}
	for (i=0;i<9;i++)
	{
		OPLWrite(opl, 0, i+0xc0);
		if ((mute[i]&&mute[i+9]))
			OPLWrite(opl, 1, 0);
		else
			OPLWrite(opl, 1, hardvols[i][1]);
	}
}
