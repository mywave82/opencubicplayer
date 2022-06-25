/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * CPIface note dots mode
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
 *  -doj980928  Dirk Jagdmann  <doj@cubic.org>
 *    -added cpipic.h and isqrt.cpp to the #include list
 *  -fd981119   Felix Domke    <tmbinc@gmx.net>
 *    -added the really important 'NO_CPIFACE_IMPORT'
 *  -fd981220   Felix Domke    <tmbinc@gmx.net>
 *    -changes for LFB (and other faked banked-modes)
 */


#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "types.h"
#include "stuff/poutput.h"
#include "cpiface.h"
#include "cpipic.h"

#define MAXPCHAN 64
#define MAXVIEWCHAN 32

static int (*plGetDots)(struct notedotsdata *, int);

static int plDotsMiddle = 72*256;
static int plDotsScale = 32;
static unsigned char plDotsType;

static struct notedotsdata dotdata[MAXPCHAN];
static unsigned char dotchan[MAXPCHAN];
static uint16_t dotpos[MAXPCHAN];
static int dotvoll[MAXPCHAN];
static int dotvolr[MAXPCHAN];
static unsigned char dotcol[MAXPCHAN];
static unsigned char dothgt;
static unsigned char dotwid;
static unsigned char dotwid2;
static unsigned char dotbuf[32][96];

static unsigned char dotuse[MAXVIEWCHAN][640/32];

static unsigned char dotsqrttab[65];
static unsigned char dotcirctab[17][16];

static void drawbox(uint16_t y, int16_t x)
{
	uint8_t *scrpos=plVidMem+96*640+y*dothgt*640+x*32;
	int j;

	for (j=0; j<dothgt; j++)
	{
		memcpy(scrpos, dotbuf[j]+32, 32);
		scrpos+=640;
	}
}

static void resetbox(uint16_t y, int16_t x)
{
	int i;
	if (plOpenCPPict)
	{
		unsigned char *p=plOpenCPPict+y*dothgt*640+x*32;
		for (i=0; i<dothgt; i++)
		{
			memcpy(dotbuf[i]+32, p, 32);
			p+=640;
		}
	} else
		for (i=0; i<dothgt; i++)
			memset(dotbuf[i]+32, 0, 32);
}

static void putbar(uint16_t k, uint16_t j)
{
	int v=dotvoll[k]+dotvolr[k];
	unsigned char len;
	unsigned char first;
	int l;

	if (v>64)
		v=64;
	len=(v+3)>>2;
	first=32+dotpos[k]-(len>>1)-j*32;
	for (l=0; l<dothgt; l++)
		memset(dotbuf[l]+first, dotcol[k], len);
}

static void putstcone(uint16_t k, uint16_t j)
{
	int l;
	unsigned char pos=32+dotpos[k]-j*32;
	unsigned char lenl=(dotsqrttab[dotvoll[k]]+3)>>2;
	unsigned char lenr=(dotsqrttab[dotvolr[k]]+3)>>2;
	for (l=0; l<(dothgt>>1); l++)
	{
		if (l<lenl)
		{
			memset(dotbuf[(dothgt>>1)-1-l]+pos-lenl, dotcol[k], lenl-l);
			memset(dotbuf[(dothgt>>1)+l]+pos-lenl, dotcol[k], lenl-l);
		}
		if (l<lenr)
		{
			memset(dotbuf[(dothgt>>1)-1-l]+pos+l, dotcol[k], lenr-l);
			memset(dotbuf[(dothgt>>1)+l]+pos+l, dotcol[k], lenr-l);
		}
	}
}

static void putdot(uint16_t k, uint16_t j)
{
	int l;
	unsigned char pos=32+dotpos[k]-j*32;
	int v=dotvoll[k]+dotvolr[k];
	unsigned char len;
	if (v>64)
		v=64;
	len=(dotsqrttab[v]+3)>>2;
	for (l=0; l<(dothgt>>1); l++)
	{
		unsigned char ln=dotcirctab[len][l];
		memset(dotbuf[(dothgt>>1)-1-l]+pos-ln, dotcol[k], 2*ln);
		memset(dotbuf[(dothgt>>1)+l]+pos-ln, dotcol[k], 2*ln);
	}
}

static void putstdot(uint16_t k, uint16_t j)
{
	int l;
	unsigned char pos=32+dotpos[k]-j*32;
	unsigned char lenl=(dotsqrttab[dotvoll[k]]+3)>>2;
	unsigned char lenr=(dotsqrttab[dotvolr[k]]+3)>>2;
	for (l=0; l<(dothgt>>1); l++)
	{
		unsigned char lnl=dotcirctab[lenl][l];
		unsigned char lnr=dotcirctab[lenr][l];
		memset(dotbuf[(dothgt>>1)-1-l]+pos-lnl, dotcol[k], lnl);
		memset(dotbuf[(dothgt>>1)+l]+pos-lnl, dotcol[k], lnl);
		memset(dotbuf[(dothgt>>1)-1-l]+pos, dotcol[k], lnr);
		memset(dotbuf[(dothgt>>1)+l]+pos, dotcol[k], lnr);
	}
}

static void plDrawDots()
{
	int i,j,k,n,m;
	int chan0;
	int chann = cpifaceSessionAPI.LogicalChannelCount;
	int pos;

	if (chann>MAXVIEWCHAN)
		chann=MAXVIEWCHAN;
	chan0=plSelCh-(chann/2);
	if ((chan0+chann) >= cpifaceSessionAPI.LogicalChannelCount)
		chan0 = cpifaceSessionAPI.LogicalChannelCount - chann;
	if (chan0<0)
		chan0=0;

	if (plChanChanged)
		for (i=0; i<chann; i++)
		{
			if (dothgt>=16)
			{
				gdrawcharp(8, 96+(dothgt-16)/2+i*dothgt, '0'+(i+1+chan0)/10, ((i+chan0)==plSelCh)?15:plMuteCh[i+chan0]?8:7, plOpenCPPict?(plOpenCPPict-96*640):0);
				gdrawcharp(16, 96+(dothgt-16)/2+i*dothgt, '0'+(i+1+chan0)%10, ((i+chan0)==plSelCh)?15:plMuteCh[i+chan0]?8:7, plOpenCPPict?(plOpenCPPict-96*640):0);
				gdrawcharp(616, 96+(dothgt-16)/2+i*dothgt, '0'+(i+1+chan0)/10, ((i+chan0)==plSelCh)?15:plMuteCh[i+chan0]?8:7, plOpenCPPict?(plOpenCPPict-96*640):0);
				gdrawcharp(624, 96+(dothgt-16)/2+i*dothgt, '0'+(i+1+chan0)%10, ((i+chan0)==plSelCh)?15:plMuteCh[i+chan0]?8:7, plOpenCPPict?(plOpenCPPict-96*640):0);
			} else {
				gdrawchar8p(8, 96+(dothgt-8)/2+i*dothgt, '0'+(i+1+chan0)/10, ((i+chan0)==plSelCh)?15:plMuteCh[i+chan0]?8:7, plOpenCPPict?(plOpenCPPict-96*640):0);
				gdrawchar8p(16, 96+(dothgt-8)/2+i*dothgt, '0'+(i+1+chan0)%10, ((i+chan0)==plSelCh)?15:plMuteCh[i+chan0]?8:7, plOpenCPPict?(plOpenCPPict-96*640):0);
				gdrawchar8p(616, 96+(dothgt-8)/2+i*dothgt, '0'+(i+1+chan0)/10, ((i+chan0)==plSelCh)?15:plMuteCh[i+chan0]?8:7, plOpenCPPict?(plOpenCPPict-96*640):0);
				gdrawchar8p(624, 96+(dothgt-8)/2+i*dothgt, '0'+(i+1+chan0)%10, ((i+chan0)==plSelCh)?15:plMuteCh[i+chan0]?8:7, plOpenCPPict?(plOpenCPPict-96*640):0);
			}
		}

	n=plGetDots(dotdata, MAXPCHAN);

	k=0;
	for (i=0; i<n; i++)
	{
		signed long xp;

		if (dotdata[i].voll>64)
			dotdata[i].voll=64;
		if (dotdata[i].volr>64)
			dotdata[i].volr=64;
		if (!dotdata[i].voll&&!dotdata[i].volr)
		{
			dotdata[i].voll=1;
			dotdata[i].volr=1;
		}
		xp=(dotdata[i].note-plDotsMiddle)*plDotsScale/ /*1024*/ 3072 +320;
		if ((xp<16)||(xp>614))
			continue;
		dotdata[i].note=xp;
		if (plMuteCh[dotdata[i].chan])
			dotdata[i].col=8;

		dotchan[k]=dotdata[i].chan;
		dotpos[k]=dotdata[i].note;
		dotvoll[k]=(dotdata[i].voll+1);
		dotvolr[k]=(dotdata[i].volr+1);
		dotcol[k]=dotdata[i].col;
		k++;
	}
	n=k;

	for (pos=0; pos<n; pos++)
		if (dotchan[pos]>=chan0)
			break;

	for (i=0; i<chann; i++)
	{
		unsigned char use[20];
		memcpy(use, dotuse[i], 20);

		for (m=pos; m<n; m++)
			if (dotchan[m]!=(i+chan0))
				break;

		for (j=1; j<19; j++)
		{
			unsigned char inited=dotuse[i][j];
			dotuse[i][j]=0;
			if (inited)
				resetbox(i, j);
			for (k=pos; k<m; k++)
			{
				if ((((dotpos[k]-dotwid2)>>5)==j)||(((dotpos[k]+dotwid2-1)>>5)==j))
				{
					dotuse[i][j]=1;
					if (!inited)
						resetbox(i, j);
					inited=1;

					switch (plDotsType)
					{
						case 0:
							putdot(k,j);
							break;
						case 1:
							putbar(k,j);
							break;
						case 2:
							putstcone(k,j);
							break;
						case 3:
							putstdot(k,j);
							break;
					}
				}
			}
			if (inited)
				drawbox(i, j);
		}
		pos=m;
	}
}

static void plPrepareDotsScr()
{
	char str[49];
	switch (plDotsType)
	{
		case 0: strcpy(str, "   note dots"); break;
		case 1: strcpy(str, "   note bars"); break;
		case 2: strcpy(str, "   stereo note cones"); break;
		case 3: strcpy(str, "   stereo note dots"); break;
	}
	gdrawstr(4, 0, 0x09, str, 48);
}

static void plPrepareDots()
{
	int i,j;
	int chann;

	for (i=0; i<16; i++)
	{
		unsigned char colt=rand()%6;
		unsigned char coll=rand()%63;
		unsigned char colw=8+rand()%32;
		unsigned char r,g,b;
		switch (colt)
		{
			default: /* removes warning -ss040902 */
/*
			case 0:*/
				r=63;
				g=coll;
				b=0;
				break;
			case 1:
				r=63-coll;
				g=63;
				b=0;
				break;
			case 2:
				r=0;
				g=63;
				b=coll;
				break;
			case 3:
				r=0;
				g=63-coll;
				b=63;
				break;
			case 4:
				r=coll;
				g=0;
				b=63;
				break;
			case 5:
				r=63;
				g=0;
				b=63-coll;
				break;
		}
		r=63-((63-r)*(64-colw)/64);
		g=63-((63-g)*(64-colw)/64);
		b=63-((63-b)*(64-colw)/64);

		plOpenCPPal[3*i+48]=r>>1;
		plOpenCPPal[3*i+49]=g>>1;
		plOpenCPPal[3*i+50]=b>>1;
		plOpenCPPal[3*i+96]=r;
		plOpenCPPal[3*i+97]=g;
		plOpenCPPal[3*i+98]=b;
	}

	memset(dotuse, 0, sizeof (dotuse));

	chann = cpifaceSessionAPI.LogicalChannelCount;
	if (chann>MAXVIEWCHAN)
		chann=MAXVIEWCHAN;

	dothgt=(chann>24)?12:(chann>16)?16:(chann>12)?24:32;
	dotwid=32;
	dotwid2=16;

	for (i=16; i<256; i++)
		gupdatepal(i, plOpenCPPal[i*3], plOpenCPPal[i*3+1], plOpenCPPal[i*3+2]);
	gflushpal();

	if (plOpenCPPict)
	{
		memcpy(plVidMem+96*640, plOpenCPPict, (480-96)*640 );
	}

	for (i=0; i<65; i++)
		dotsqrttab[i]=(int)(sqrt(256*i)+1)>>1;

	for (i=0; i<17; i++)
		for (j=0; j<16; j++)
			dotcirctab[i][j]=(j<i)?((int)(sqrt(4*(i*i-j*(j+1))-1)+1)>>1):0;
}

static void plSetDotsType(int t)
{
	plDotsType=(t+4)%4;
}

static int plDotsKey(uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('n', "Change note dots type");
			cpiKeyHelp('N', "Change note dots type");
			cpiKeyHelp(KEY_PPAGE, "Change note dots range down");
			cpiKeyHelp(KEY_NPAGE, "Change note dots range up");
			cpiKeyHelp(KEY_CTRL_PGUP, "Adjust scale up");
			cpiKeyHelp(KEY_CTRL_PGDN, "Adjust scale down");
			cpiKeyHelp(KEY_HOME, "Reset note dots range");
			return 0;
		case KEY_PPAGE:
			plDotsMiddle-=128;
			if (plDotsMiddle<48*256)
				plDotsMiddle=48*256;
			break;
		case KEY_NPAGE:
			plDotsMiddle+=128;
			if (plDotsMiddle>96*256)
				plDotsMiddle=96*256;
			break;
		case KEY_CTRL_PGUP:
		/* case 0x7600: //ctrl-pgdn */
			plDotsScale=(plDotsScale+1)*32/31;
				if (plDotsScale>256)
			plDotsScale=256;
			break;
		case KEY_CTRL_PGDN:
		/* case 0x8400: //ctrl-pgup */
			plDotsScale=plDotsScale*31/32;
			if (plDotsScale<16)
				plDotsScale=16;
			break;
		/*case 0x4700: //home*/
		case KEY_HOME:
			plDotsMiddle=72*256;
			plDotsScale=32;
			break;
		case 'n': case 'N':
			plSetDotsType(plDotsType+1);
			break;
		default:
			return 0;
	}
	plPrepareDotsScr();
	return 1;
}

static void dotDraw(void)
{
	cpiDrawGStrings();
	plDrawDots();
}

static void dotSetMode(void)
{
	plReadOpenCPPic();
	cpiSetGraphMode(0);
	plPrepareDots();
	plPrepareDotsScr();
}

static int dotIProcessKey(uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('n', "Enable note dots mode");
			cpiKeyHelp('N', "Enable note dots mode");
			return 0;
		case 'n': case 'N':
			cpiSetMode("dots");
			break;
		default:
			return 0;
	}
	return 1;
}

static int plDotsEvent(int ignore)
{
	return 1;
}

static struct cpimoderegstruct plDotsMode = {"dots", dotSetMode, dotDraw, dotIProcessKey, plDotsKey, plDotsEvent CPIMODEREGSTRUCT_TAIL};

void plUseDots(int (*get)(struct notedotsdata *, int))
{
	if (plVidType==vidNorm)
		return;
	plGetDots=get;
	cpiRegisterMode(&plDotsMode);
}
