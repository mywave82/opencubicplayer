/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2011-'20 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * CPIface text track/pattern display
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
 *  -fd981119   Felix Domke <tmbinc@gmx.net>
 *    -added the really important 'NO_CPIFACE_IMPORT'
 *  -ss04????   Stian Skjelstad <stian@nixia.no>
 *    -many changes to make it resize to any-sizes more pretty
 *  -ss040915   Stian Skjelstad <stian@nixia.no>
 *    -Only print the right row-numbers if it fits without it makes us drop more
 *     rows
 *  -ss040918   Stian Skjelstad <stian@nixia.no>
 *    -Fixed a bug when scrolling channels, when they are zoomed wider than the screen
 */

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "stuff/poutput.h"
#include "boot/psetting.h"
#include "cpiface.h"

#define COLPTNOTE 0x0A
#define COLNOTE 0x0F
#define COLPITCH 0x02
#define COLSPEED 0x02
#define COLPAN 0x05
#define COLVOL 0x09
#define COLACT 0x04
#define COLINS 0x07
#define COLLNUM 0x07
#define COLHLNUM 0x04
#define COLBACK 0x08
#define COLMUTE 0x08
#define COLTITLE1 0x01
#define COLTITLE1H 0x09
#define COLTITLE2 0x07
#define COLTITLE2M 0x08
#define COLTITLE2H 0x0F
#define COLMARK 0x0F
#define COLHLAND 0xFF
#define COLHLOR 0x88

static int plTrackActive;

static int (*getcurpos)();
static int (*getpatlen)(int n);
static const char *(*getpatname)(int n);
static void (*seektrack)(int n, int c);
static int (*startrow)();
static int (*getnote)(uint16_t *bp, int small);
static int (*getins)(uint16_t *bp);
static int (*getvol)(uint16_t *bp);
static int (*getpan)(uint16_t *bp);
static void (*getfx)(uint16_t *bp, int n);
static void (*getgcmd)(uint16_t *bp, int n);

static int plPatternNum;
static int plPrepdPat;
static signed int plPatType = -1;
static int plPatFirstLine;
static int plPatHeight;
static int plPatWidth;
static int plPatManualPat;
static int plPatManualRow;
#define plPatBufH (256+256)
static uint16_t (*plPatBuf)[CONSOLE_MAX_X];
static uint16_t pathighlight[CONSOLE_MAX_X];
static char pattitle1[CONSOLE_MAX_X+1];
static uint16_t pattitle2[CONSOLE_MAX_X];
static int patwidth, patpad;

static int overrideplNLChan;

enum
{
	cpiTrkFXIns=1,cpiTrkFXNote=2,cpiTrkFXVol=4,cpiTrkFXNoPan=8
};

static void getfx2(uint16_t *bp, int n, int o)
{
	int p=0;
	if (o&cpiTrkFXIns)
		if (getins(bp+1))
		{
			writestring(bp, 0, COLINS, "i", 1);
			p++;
			bp+=3;
		}
	if (p==n)
		return;
	if (o&cpiTrkFXNote)
		if (getnote(bp, 0))
		{
			p++;
			bp+=3;
		}
	if (p==n)
		return;
	if (o&cpiTrkFXVol)
		if (getvol(bp+1))
		{
			writestring(bp, 0, COLVOL, "v", 1);
			p++;
			bp+=3;
		}
	if (p==n)
		return;
	if (!(o&cpiTrkFXNoPan))
		if (getpan(bp+1))
		{
			writestring(bp, 0, COLPAN, "p", 1);
			p++;
			bp+=3;
		}
	if (p==n)
		return;
	getfx(bp, n-p);
}

static void getscrollpos(int tr, int *firstchan, int *chnn)
{
	if (overrideplNLChan>tr)
	{
		if (plSelCh<(tr>>1))
			*firstchan=0;
		else
			if (plSelCh>=(overrideplNLChan-(tr>>1)))
				*firstchan=overrideplNLChan-tr;
			else
				*firstchan=plSelCh-((tr>>1));
		*chnn=tr;
	} else {
		*firstchan=0;
		*chnn=overrideplNLChan;
	}
}


static void setattrgrey(uint16_t *buf, int len)
{
	int i;
	for (i=0; i<len; i++)
	{
		((char*)buf)[1]=COLMUTE;
		buf++;
	}
}

static void preparetrack1(uint16_t *bp)
{
	getnote(bp, 2);
}

static void preparetrack2(uint16_t *bp)
{
	getnote(bp, 1);
}

static void preparetrack3(uint16_t *bp)
{
	getnote(bp, 0);
}

static void preparetrack3f(uint16_t *bp)
{
	if (!getnote(bp, 0))
		getfx2(bp, 1, cpiTrkFXVol);
}


static void preparetrack6nf(uint16_t *bp)
{
	getnote(bp, 0);
	getfx2(bp+3, 1, cpiTrkFXVol);
}

static void preparetrack17invff(uint16_t *bp)
{
	getins(bp);
	getnote(bp+3, 0);
	getvol(bp+7);
	getfx2(bp+10, 2, 0);
}

static void preparetrack14invff(uint16_t *bp)
{
	getins(bp);
	getnote(bp+2, 0);
	getvol(bp+5);
	getfx2(bp+7, 2, 0);
}

static void preparetrack14nvff(uint16_t *bp)
{
	getnote(bp, 0);
	getvol(bp+4);
	getfx2(bp+7, 2, 0);
}

static void preparetrack26invpffff(uint16_t *bp)
{
	getins(bp);
	getnote(bp+3, 0);
	getvol(bp+7);
	getpan(bp+10);
	getfx2(bp+13, 4, cpiTrkFXNoPan);
}

static void preparetrack16fffff(uint16_t *bp)
{
	getfx2(bp, 5, cpiTrkFXIns|cpiTrkFXNote|cpiTrkFXVol);
}

static void preparetrack8nvf(uint16_t *bp)
{
	getnote(bp, 0);
	getvol(bp+3);
	getfx2(bp+5, 1, 0);
}

static void preparetrack8inf(uint16_t *bp)
{
	getins(bp);
	getnote(bp+2, 0);
	getfx2(bp+5, 1, cpiTrkFXVol);
}

struct patviewtype
{
/*
	uint8_t maxch;   These are now calculated real-time */
	uint8_t gcmd;
	uint8_t width;
	const char *title;
	const char *paused;
	const char *normal;
	const char *selected;
	void (*putcmd)(uint16_t *bp);
};

static void preparepatgen(int pat, const struct patviewtype *pt)
{
	int i;
	int firstchan,chnn;
	const char *pname;
	int p0, n0;
	uint16_t patmask[CONSOLE_MAX_X];
	int firstrow=20;
	int firstprow=0;
	int firstpat;

	int maxch=(plPatWidth-pt->gcmd*4-3)/pt->width;

	if (maxch>overrideplNLChan)
		maxch=overrideplNLChan;

	patpad=(plPatWidth-maxch*pt->width-pt->gcmd*4-3)>=4;

	plPrepdPat=pat;

	getscrollpos(maxch, &firstchan, &chnn);

	strcpy(pattitle1, "   pattern view:  order ");
	convnum(pat, pattitle1+strlen(pattitle1), 16, 3, 0);
	strcat(pattitle1, ", ");
	convnum(maxch, pattitle1+strlen(pattitle1), 10, 2, 1);
	strcat(pattitle1, " channels,  ");
	strcat(pattitle1, pt->title);
	pname=getpatname(pat);
	if (pname&&*pname)
	{
		strcat(pattitle1, ": ");
		strcat(pattitle1, pname);
	}

	p0=4+4*pt->gcmd;
	patwidth=p0+pt->width*chnn+4;
	writestring(pattitle2, 0, COLTITLE2, "row", CONSOLE_MAX_X);
	if (patpad)
		writestring(pattitle2, patwidth-4, COLTITLE2, "row", 3);
	switch (pt->gcmd)
	{
		case 0:
			break;
		case 1:
			writestring(pattitle2, 4, plPause?COLTITLE2M:COLTITLE2, "gbl", 3);
			break;
		case 2:
			writestring(pattitle2, 5, plPause?COLTITLE2M:COLTITLE2, "global", 6);
			break;
		case 3:
			writestring(pattitle2, 5, plPause?COLTITLE2M:COLTITLE2, "global cmd", 10);
			break;
		default:
			writestring(pattitle2, pt->gcmd*2-4, plPause?COLTITLE2M:COLTITLE2, "global commands", 15);
			break;
	}


	writestring(patmask, 0, COLLNUM, "00 ", CONSOLE_MAX_X);
	if (patpad)
		writestring(patmask, patwidth-3, COLLNUM, "00", 2);
	writestring(patmask, 3, COLBACK, "\xba", 1);
	writestring(patmask, 3+pt->gcmd*4, COLBACK, "\xba", 1);
	if (!plPause)
		for (i=0; i<pt->gcmd; i++)
			writestring(patmask, 4+4*i, COLBACK, "\xfa\xfa\xfa", 3);

	n0=p0+(pt->width+1)/2-1;
	if (pt->width==4)
		n0--;
	for (i=0; i<chnn; i++)
	{
		char chpaus=plMuteCh[i+firstchan];
		char sel=((i+firstchan)==plSelCh);
		writenum(pattitle2, n0+pt->width*i, sel?COLTITLE2H:chpaus?COLTITLE2M:COLLNUM, i+firstchan+1, 10, (pt->width==1)?1:2, pt->width>2);
		writestring(patmask, p0+pt->width*i, COLBACK, chpaus?pt->paused:sel?pt->selected:pt->normal, pt->width);
	}

	firstpat=pat;

	/* attempt to rewind 20 (default firsrow value) places back */
	while (firstpat)
	{
		firstpat--;
		firstrow-=getpatlen(firstpat);
		if (firstrow<=0)
		{
			firstprow-=firstrow;
			firstrow=0;
			break;
		}
	}
	/* if we were at the start of the song, this wasn't possible, fill in with blank lines */
	for (i=0; i<firstrow; i++)
		writestring(plPatBuf[i], 0, COLBACK, "", CONSOLE_MAX_X);

	/* firstpat = first pattern to render from
	 * firstprow = first row to render from
	 * firstrow = first line to render to (inside buffer)
	 */

	/* do not render past end of song */
	while (firstpat<plPatternNum)
	{
		int curlen;
		int lastprow;

		if (!(curlen=getpatlen(firstpat)))
		{
			/* skip zerolength patterns */
			firstpat++;
			continue;
		}
		lastprow=curlen;
		if ((firstrow+curlen-firstprow)>=plPatBufH)
			lastprow=plPatBufH-firstrow-firstprow-1;

		for (i=firstprow; i<lastprow; i++)
		{
			writestringattr(plPatBuf[i+firstrow-firstprow], 0, patmask, CONSOLE_MAX_X);
			writenum(plPatBuf[i+firstrow-firstprow], 0, i?COLLNUM:COLHLNUM, i, 16, 2, 0);
			if (patpad)
				writenum(plPatBuf[i+firstrow-firstprow], patwidth-3, i?COLLNUM:COLHLNUM, i, 16, 2, 0);
		}

		if (pt->gcmd)
		{
			seektrack(firstpat, -1);
			while (1)
			{
				int currow=startrow();
				if (currow==-1)
					break;
				if ((currow>=firstprow)&&(currow<lastprow))
				{
					uint16_t *bp=plPatBuf[currow+firstrow-firstprow]+4;
					getgcmd(bp, pt->gcmd);
					if (plPause)
						setattrgrey(bp, pt->gcmd*4);
				}
			}
		}

		for (i=0; i<chnn; i++)
		{
			int chpaus;
			seektrack(firstpat, i+firstchan);
			chpaus=plMuteCh[i+firstchan];
			while (1)
			{
				int currow=startrow();
				if (currow==-1)
					break;
				if ((currow>=firstprow)&&(currow<lastprow))
				{
					uint16_t *bp=plPatBuf[currow+firstrow-firstprow]+p0+i*pt->width;
					pt->putcmd(bp);
					if (chpaus)
						setattrgrey(bp, pt->width);
				}
			}
		}

		firstrow+=lastprow-firstprow;
		firstprow=0;
		firstpat++;

		if (firstrow>=plPatBufH)
			break;
	}
	for (i=firstrow; i<plPatBufH; i++)
		writestring(plPatBuf[i], 0, COLBACK, "", CONSOLE_MAX_X);
}

static struct patviewtype pat6480   = {/*64,*/  3,  1, "(notes only)",                          " ",
                                                                                                "\xfa",
                                                                                                "\xf9", preparetrack1};
static struct patviewtype pat64132  = {/*64,*/ 15,  1, "(notes only)",                          " ",
                                                                                                "\xfa",
                                                                                                "\xf9", preparetrack1};
static struct patviewtype pat64132m = {/*64,*/  0,  2, "(notes only)",                          "  ",
                                                                                                "\xfa\xfa",
                                                                                                "\xf9\xf9", preparetrack2};
static struct patviewtype pat4880   = {/*48,*/  6,  1, "(notes only)",                          " ",
                                                                                                "\xfa",
                                                                                                "\xf9", preparetrack1};
static struct patviewtype pat48132  = {/*48,*/  7,  2, "(notes only)",                          "  ",
                                                                                                "\xfa\xfa",
                                                                                                "\xf9\xf9", preparetrack2};
static struct patviewtype pat3280   = {/*32,*/  3,  2, "(notes only)",                          "  ",
                                                                                                "\xfa\xfa",
                                                                                                "\xf9\xf9", preparetrack2};
static struct patviewtype pat32132  = {/*32,*/  7,  3, "(notes only)",                          "   ",
                                                                                                "\xfa\xfa\xfa",
                                                                                                "\xf9\xf9\xf9", preparetrack3};
static struct patviewtype pat32132f = {/*32,*/  7,  3, "(notes & fx)",                          "   ",
                                                                                                "\xfa\xfa\xfa",
                                                                                                "\xf9\xf9\xf9", preparetrack3f};
static struct patviewtype pat2480   = {/*24,*/  1,  3, "(notes only)",                          "   ",
                                                                                                "\xfa\xfa\xfa",
                                                                                                "\xf9\xf9\xf9", preparetrack3};
static struct patviewtype pat2480f  = {/*24,*/  1,  3, "(notes & fx)",                          "   ",
                                                                                                "\xfa\xfa\xfa",
                                                                                                "\xf9\xf9\xf9", preparetrack3f};
static struct patviewtype pat24132  = {/*24,*/  7,  4, "(notes only)",                          "   \xb3",
                                                                                                "\xfa\xfa\xfa\xb3",
                                                                                                "\xf9\xf9\xf9\xb3", preparetrack3};
static struct patviewtype pat24132f = {/*24,*/  7,  4, "(notes & fx)",                          "   \xb3",
                                                                                                "\xfa\xfa\xfa\xb3",
                                                                                                "\xf9\xf9\xf9\xb3", preparetrack3f};
static struct patviewtype pat1680   = {/*16,*/  3,  4, "(notes only)",                          "   \xb3",
                                                                                                "\xfa\xfa\xfa\xb3",
                                                                                                "\xf9\xf9\xf9\xb3", preparetrack3};
static struct patviewtype pat1680f  = {/*16,*/  3,  4, "(notes & fx)",                          "   \xb3",
                                                                                                "\xfa\xfa\xfa\xb3",
                                                                                                "\xf9\xf9\xf9\xb3", preparetrack3f};
static struct patviewtype pat16132  = {/*16,*/  3,  7, "(note, fx)",                            "      \xb3",
                                                                                                "\xfa\xfa\xfa   \xb3",
                                                                                                "\xf9\xf9\xf9\xfa\xfa\xfa\xb3", preparetrack6nf};
static struct patviewtype pat880    = {/* 8,*/  1,  9, "(ins, note, fx)",                       "        \xb3",
                                                                                                "  \xfa\xfa\xfa   \xb3",
                                                                                                "\xfa\xfa\xf9\xf9\xf9\xfa\xfa\xfa\xb3", preparetrack8inf};
static struct patviewtype pat880f   = {/* 8,*/  1,  9, "(note, vol, fx)",                       "        \xb3",
                                                                                                "\xfa\xfa\xfa  \xfa\xfa\xfa\xb3",
                                                                                                "\xf9\xf9\xf9\xfa\xfa\xf9\xf9\xf9\xb3", preparetrack8nvf};
static struct patviewtype pat8132   = {/* 8,*/  3, 14, "(note, vol, fx, fx)",                   "             \xb3",
                                                                                                "\xfa\xfa\xfa \xfa\xfa \xfa\xfa\xfa\xfa\xfa\xfa\xb3",
                                                                                                "\xf9\xf9\xf9 \xf9\xf9 \xf9\xf9\xf9\xf9\xf9\xf9\xb3", preparetrack14nvff};
static struct patviewtype pat8132f  = {/* 8,*/  3, 14, "(ins, note, vol, fx, fx)",              "             \xb3",
                                                                                                "  \xfa\xfa\xfa  \xfa\xfa\xfa\xfa\xfa\xfa\xb3",
                                                                                                "\xfa\xfa\xf9\xf9\xf9\xfa\xfa\xf9\xf9\xf9\xf9\xf9\xf9\xb3", preparetrack14invff};
static struct patviewtype pat480    = {/* 4,*/  2, 17, "(ins, note, vol, fx, fx)",              "                \xb3",
                                                                                                "\xfa\xfa \xfa\xfa\xfa \xfa\xfa \xfa\xfa\xfa\xfa\xfa\xfa\xb3",
                                                                                                "\xf9\xf9 \xf9\xf9\xf9 \xf9\xf9 \xf9\xf9\xf9\xf9\xf9\xf9\xb3", preparetrack17invff};
static struct patviewtype pat480f   = {/* 4,*/  3, 16, "(fx, fx, fx, fx, fx)",                  "               \xb3",
                                                                                                "\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xb3",
                                                                                                "\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xb3", preparetrack16fffff};
static struct patviewtype pat4132   = {/* 4,*/  5, 26, "(ins, note, vol, pan, fx, fx, fx, fx)", "                         \xb3",
	                                                                                        "\xfa\xfa \xfa\xfa\xfa \xfa\xfa \xfa\xfa \xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xb3",
                                                                                                "\xf9\xf9 \xf9\xf9\xf9 \xf9\xf9 \xf9\xf9 \xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xf9\xb3", preparetrack26invpffff};

static int min(int a, int b)
{
	if (a<b)
		return a;
	else
		return b;
}

struct calcPatTypeProbe
{
	int PatType;
	struct patviewtype *ref;
};
static const struct calcPatTypeProbe ProbeNarrow[] = /* less than 128 characters wide screens */
{
	{13, &pat480},
	{11, &pat880f},
	{ 9, &pat1680f},
	{ 7, &pat2480f},
	{ 5, &pat3280},
	{ 3, &pat4880},
	{ 1, &pat6480} /* default */
};
static const struct calcPatTypeProbe ProbeWide[] = /* larger or equal than 128 characters wide screens */
{
	{13, &pat4132},
	{11, &pat8132},
	{ 9, &pat16132},
	{ 7, &pat24132f},
	{ 5, &pat32132f},
	{ 3, &pat48132},
	{ 1, &pat64132}
};
/*
	calcPatType will try to detect the best zoom that can fit all channels on the screen. maxch is the same formula as gmdDrawPattern uses, and the ProbeNarrow/ProbeWide uses the same references aswell
*/
static void calcPatType(void)
{
	const struct calcPatTypeProbe *Probes = (plPatWidth<128)?ProbeNarrow:ProbeWide;
	int i;

	for (i=0; i<6 /* the sixth entry we do not care to probe, since it will be the default */; i++)
	{
		int maxch=(plPatWidth-Probes[i].ref->gcmd*4-3)/Probes[i].ref->width;
		if (maxch >= overrideplNLChan) /* all channels can fit in this mode */
		{
			break;
		}
	}

	plPatType=Probes[i].PatType;
}


static void gmdDrawPattern(int sel)
{
	int pos=getcurpos();
	int pat=pos>>8;
	int currow=pos&0xFF;
	int curpat=pat;
	signed int crow=currow;
	signed int i,j,row;
	/* int plen; */

	if (plPatType < 0)
	{
		calcPatType();
	}

	if (plPatManualPat!=-1)
	{
		pat=plPatManualPat;
		crow=plPatManualRow;
	}
	while (!getpatlen(pat))
	{
		pat++;
		crow=0;
		if (pat>=plPatternNum)
			pat=0;
	}

	if ((plPrepdPat!=pat)||plChanChanged)
	{
		if (plPatWidth<128)
			switch (plPatType)
			{
				case 0: preparepatgen(pat, &pat6480); break;
				case 1: preparepatgen(pat, &pat6480); break;
				case 2: preparepatgen(pat, &pat4880); break;
				case 3: preparepatgen(pat, &pat4880); break;
				case 4: preparepatgen(pat, &pat3280); break;
				case 5: preparepatgen(pat, &pat3280); break;
				case 6: preparepatgen(pat, &pat2480); break;
				case 7: preparepatgen(pat, &pat2480f); break;
				case 8: preparepatgen(pat, &pat1680); break;
				case 9: preparepatgen(pat, &pat1680f); break;
				case 10: preparepatgen(pat, &pat880); break;
				case 11: preparepatgen(pat, &pat880f); break;
				case 12: preparepatgen(pat, &pat480f); break;
				case 13: preparepatgen(pat, &pat480); break;
			} else switch (plPatType)
			{
				case 0: preparepatgen(pat, &pat64132m); break;
				case 1: preparepatgen(pat, &pat64132); break;
				case 2: preparepatgen(pat, &pat48132); break;
				case 3: preparepatgen(pat, &pat48132); break;
				case 4: preparepatgen(pat, &pat32132); break;
				case 5: preparepatgen(pat, &pat32132f); break;
				case 6: preparepatgen(pat, &pat24132); break;
				case 7: preparepatgen(pat, &pat24132f); break;
				case 8: preparepatgen(pat, &pat16132); break;
				case 9: preparepatgen(pat, &pat16132); break;
				case 10: preparepatgen(pat, &pat8132f); break;
				case 11: preparepatgen(pat, &pat8132); break;
				case 12: preparepatgen(pat, &pat4132); break;
				case 13: preparepatgen(pat, &pat4132); break;
			}
	}

	/* show the two header lines*/
	displaystr(plPatFirstLine-2, 0, sel?COLTITLE1H:COLTITLE1, pattitle1, plPatWidth);
	displaystrattr(plPatFirstLine-1, 0, pattitle2, plPatWidth);
	/* done */

	/* plen=getpatlen(pat); */
	for (i=0, row=crow-min(plPatHeight/3, 20); i<plPatHeight; i++, row++)
	{
		/* if no manual position, and we are not at the current position, display the buffer normal */
		/* crow will be negativ for the lines that hit before the marker and that are from the previous pattern */
		if ((row!=crow)&&((plPatManualPat==-1)||(row!=currow)||(pat!=curpat)))
		{
			/* +20, since the first 20 lines in the buffer are the lines before the marker/pattern, and that we have a bias in the row counter */
			displaystrattr(plPatFirstLine+i, 0, plPatBuf[20+row], plPatWidth);
			continue;
		}
		/* copy in the line, since it is special */
		writestringattr(pathighlight, 0, plPatBuf[20+row], plPatWidth);
		/* is this the current position, if so, it needs arrows */
		if ((row==currow)&&(pat==curpat))
		{
			writestring(pathighlight, 2, COLMARK, "\x10", 1);
			if (patwidth>=132)
				writestring(pathighlight, patwidth-4, COLMARK, "\x11", 1);
		}
		/* if this is the target line (manual browse overrides this position, highlight it */
		if (row==crow)
			for (j=0; j<patwidth; j++)
				pathighlight[j]=(pathighlight[j]|(COLHLOR<<8))&((COLHLAND<<8)|0xFF);
		/* and print to screen */
		displaystrattr(plPatFirstLine+i, 0, pathighlight, plPatWidth);
	}
}

static int gmdTrkProcessKey(uint16_t key)
{
	if (plPatType < 0)
	{ /* more an assertion... should not be able to get so far without this beeing calculated. Usually we would draw atleast one frame before processing keyboard presses */
		calcPatType();
	}

	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp(' ', "Release the track viewer (enable manual scrolling)");
			cpiKeyHelp(KEY_TAB, "Rotate track viewer modes");
			cpiKeyHelp(KEY_SHIFT_TAB, "Rotate track viewer modes (reverse)");
			cpiKeyHelp(KEY_HOME, "Reset track viewer settings");
			cpiKeyHelp(KEY_NPAGE, "Zoom track viewer / scroll track viewer");
			cpiKeyHelp(KEY_PPAGE, "Zoom track viewer / scroll track viewer");
			return 0;
		case KEY_PPAGE:
			if (plPatManualPat==-1)
			{
				if (plPatType>1)
				{
					plPatType-=2;
					plPrepdPat=-1;
				}
			} else {
				plPatManualRow-=8;
				if (plPatManualRow<0)
				{
					plPatManualPat--;
					if (plPatManualPat<0)
						plPatManualPat=plPatternNum-1;
					while (!getpatlen(plPatManualPat))
						plPatManualPat--;
					plPatManualRow=getpatlen(plPatManualPat)-1;
				}
			}
			break;
		case KEY_NPAGE:
			if (plPatManualPat==-1)
			{
				if (plPatType<12)
				{
					plPatType+=2;
					plPrepdPat=-1;
				}
			} else {
				plPatManualRow+=8;
				if (plPatManualRow>=getpatlen(plPatManualPat))
				{
					plPatManualPat++;
					while ((plPatManualPat<plPatternNum)&&!getpatlen(plPatManualPat))
						plPatManualPat++;
					if (plPatManualPat>=plPatternNum)
						plPatManualPat=0;
					plPatManualRow=0;
				}
			}
			break;
/* ALREADY COMMENTED
  case 0x8400: //ctrl-pgup
    if (plInstType)
      plInstScroll--;
    break;
  case 0x7600: //ctrl-pgdn
    if (plInstType)
      plInstScroll++;
    break;
*/
		case KEY_HOME:
			calcPatType();
/* TODO   winrecalc(); */
			break;
		case KEY_TAB: /* tab */
			if (plPatManualPat==-1)
			{
				plPatType^=1;
				plPrepdPat=-1;
			} else {
				if (plPatType<13)
				{
					plPatType++;
					plPrepdPat=-1;
				}
			}
			break;
		case KEY_SHIFT_TAB:
/* TODO-keys
		case 0xA500:*/
			if (plPatManualPat==-1)
			{
				plPatType^=1;
				plPrepdPat=-1;
			} else {
				if (plPatType)
				{
					plPatType--;
					plPrepdPat=-1;
				}
			}
			break;
		case ' ':
			if (plPatManualPat==-1)
			{
				int p=getcurpos();
				plPatManualPat=p>>8;
				plPatManualRow=p&0xFF;
			} else
				plPatManualPat=-1;
			break;
		default:
			return 0;
	}
	return 1;
}

static void TrakSetWin(int _ignore, int wid, int ypos, int hgt)
{
	plPatFirstLine=ypos+2;
	plPatHeight=hgt-2;
	plPatWidth=wid;
}

static int TrakGetWin(struct cpitextmodequerystruct *q)
{
	if (!plTrackActive)
		return 0;

	q->hgtmin=3;
	q->hgtmax=100;
	q->xmode=1;
	q->size=2;
	q->top=0;
	q->killprio=64;
	q->viewprio=160;
	return 1;
}

static void TrakDraw(int focus)
{
	gmdDrawPattern(focus);
}

static int TrakIProcessKey(uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('t', "Enable track viewer");
			cpiKeyHelp('T', "Enable track viewer");
			break;
		case 't': case 'T':
			plTrackActive=1;
			cpiTextSetMode("trak");
			calcPatType();
			return 1;
		case 'x': case 'X':
			plTrackActive=1;
			calcPatType();
			break;
		case KEY_ALT_X:
			plTrackActive=0;
			break;
	}
	return 0;
}

static int TrakAProcessKey(uint16_t key)
{
	switch (key)
	{
		case 't': case 'T':
			plTrackActive=!plTrackActive;
			cpiTextRecalc();
			break;
		default:
			return gmdTrkProcessKey(key);
	}
	return 1;
}

static int trkEvent(int ev)
{
	switch (ev)
	{
		case cpievInitAll:
			plTrackActive=cfGetProfileBool2(cfScreenSec, "screen", "pattern", 1, 1);
			return 0;
		case cpievInit:
			plPatBuf=calloc(sizeof(short), (plPatBufH/*don't ask where we get these from */)*CONSOLE_MAX_X);
			if (!plPatBuf)
				return 0;
			break;
		case cpievDone:
			free(plPatBuf);
			plPatBuf=0;
			break;
	}
	return 1;
}

static struct cpitextmoderegstruct cpiTModeTrack = {"trak", TrakGetWin, TrakSetWin, TrakDraw, TrakIProcessKey, TrakAProcessKey, trkEvent CPITEXTMODEREGSTRUCT_TAIL};

void cpiTrkSetup(const struct cpitrakdisplaystruct *c, int npat)
{
	overrideplNLChan=plNLChan;
	plPatternNum=npat;
	plPatManualPat=-1;
	plPrepdPat=-1;
	plPatType=-1;
	getcurpos=c->getcurpos;
	getpatlen=c->getpatlen;
	getpatname=c->getpatname;
	seektrack=c->seektrack;
	startrow=c->startrow;
	getnote=c->getnote;
	getins=c->getins;
	getvol=c->getvol;
	getpan=c->getpan;
	getfx=c->getfx;
	getgcmd=c->getgcmd;
	cpiTextRegisterMode(&cpiTModeTrack);
}

void cpiTrkSetup2(const struct cpitrakdisplaystruct *c, int npat, int tracks)
{
	overrideplNLChan=tracks;
	plPatternNum=npat;
	plPatManualPat=-1;
	plPrepdPat=-1;
	plPatType=-1;
	getcurpos=c->getcurpos;
	getpatlen=c->getpatlen;
	getpatname=c->getpatname;
	seektrack=c->seektrack;
	startrow=c->startrow;
	getnote=c->getnote;
	getins=c->getins;
	getvol=c->getvol;
	getpan=c->getpan;
	getfx=c->getfx;
	getgcmd=c->getgcmd;
	cpiTextRegisterMode(&cpiTModeTrack);
}

static void __attribute__((constructor))init(void)
{
	cpiTextRegisterDefMode(&cpiTModeTrack);
}

static void __attribute__((destructor))done(void)
{
	cpiTextUnregisterDefMode(&cpiTModeTrack);
}
