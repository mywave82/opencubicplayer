/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * CPIface main interface code
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
 *  -kb980517   Tammo Hinrichs <kb@nwn.de>
 *    -fixed one small bug in Ctrl-Q/Ctrl-S key handler
 *    -various minor changes
 *  -doj980928  Dirk Jagdmann <doj@cubic.org>
 *    -added cpipic.h to the #include list
 *  -kb981118   Tammo Hinrichs <opencp@gmx.net>
 *    -restructured key handler to let actual modes override otherwise
 *     important keys
 *  -fd981119   Felix Domke <tmbinc@gmx.net>
 *    -added the really important 'NO_CPIFACE_IMPORT', along with some
 *     other portability-related changes
 *  -doj990328  Dirk Jagdmann <doj@cubic.org>
 *    -fixed bug in delete plOpenCPPict
 *    -changed note strings
 *    -made title string Y2K compliant
 *  -doj20020410 Dirk Jagdmann <doj@cubic.org>
 *    -added screenshot
 *  -doj20020901 Dirk Jagdmann <doj@cubic.org>
 *    -added plLoopPatterns to enable/disable pattern looping
 *  -ss20040709 Stian Skjelstad <stian@nixia.no>
 *    -use compatible timing, and not cputime/clock()
 *  -ss20040918 Stian Skjelstad <stian@nixia.no>
 *    -We printed the resolution wrong when it was 100 and 1000 characters wide
 */

#include "config.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "types.h"
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "cpiface.h"
#include "cpipic.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/imsrtns.h"
#include "stuff/framelock.h"
#include "stuff/poutput.h"
#include "stuff/sets.h"
#include "stuff/timer.h"
#include <unistd.h>
#include <time.h>

#define MAXLCHAN 64

extern struct mdbreadinforegstruct cpiReadInfoReg;

void (*plGetRealMasterVolume)(int *l, int *r);

extern struct cpimoderegstruct cpiModeText;

static struct cpifaceplayerstruct *curplayer;
void (*plSetMute)(int i, int m);
void (*plDrawGStrings)(void);
int (*plProcessKey)(uint16_t key);
int (*plIsEnd)(void);
void (*plIdle)(void);

char plMuteCh[MAXLCHAN];

void (*plGetMasterSample)(int16_t *, unsigned int len, uint32_t rate, int mode);
int (*plGetLChanSample)(unsigned int ch, int16_t *, unsigned int len, uint32_t rate, int opt);
int (*plGetPChanSample)(unsigned int ch, int16_t *, unsigned int len, uint32_t rate, int opt);

unsigned short plNLChan;
unsigned short plNPChan;
unsigned char plSelCh;
unsigned char plChanChanged;
static signed char soloch=-1;

char plPause;

char plCompoMode;
char plPanType;

static struct cpimoderegstruct *cpiModes;
static struct cpimoderegstruct *cpiDefModes;

time_t plEscTick;

static struct cpimoderegstruct *curmode;
static char curmodehandle[9];

static struct interfacestruct plOpenCP;

void cpiSetGraphMode(int big)
{
	plSetGraphMode(big);
	plChanChanged=1;
}

void cpiSetTextMode(int size)
{
	plSetTextMode(size);
	plChanChanged=1;
}

/*
         1         2         3         4         5         6         7         8         0         0         1         2         3
123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012
 vol: 12345678  echo: 1  rev: -234n678+  chr: -234n678+  spd: 123% = ptch: 123% |
 vol: 12345678  srnd: 1  pan: l234m678r  bal: l234m678r  spd: 123% = ptch: 123% |
     volume: 1234567890123456   echoactive: 1   reverb: -2345678m0123456+   chorus: -2345678m0123456+   speed: ---% = pitch: ---%   |
     volume: 1234567890123456   surround: 1   panning: l2345678m0123456r   balance: l2345678m0123456r   speed: ---% = pitch: ---%   |

volume:    5+ 8=13 "vol: 12345678"
           8+ 8=16 "volume: 12345678"
	   5+16=21 "vol: 1234567890123456"
	   8+16=24 "volume: 1234567890123456"
	   5+32=37 "vol: 12345678901234567890123456789012"
	   8+32=40 "volume: 12345678901234567890123456789012"
           5+64=69 "vol: 1234567890123456789012345678901234567890123456789012345678901234"
           8+64=72 "volume: 1234567890123456789012345678901234567890123456789012345678901234"

echo:      6+ 1= 7 "echo: 1"
	  12+ 1=13 "echoactive: 1"

reverb:    5+ 9=14 "rev: -234m678+"
           8+ 9=17 "reverb: -234m678+"
           5+17=22 "rev: -2345678m0123456+"
           8+17=25 "reverb: -2345678m0123456+"
           5+33=38 "rev: -234567890123456m890123456789012+"
           8+33=41 "reverb: -234567890123456m890123456789012+"
           5+65=70 "rev: -2345678901234567890123456789012m4567890123456789012345678901234+"
           8+65=73 "reverb: -2345678901234567890123456789012m4567890123456789012345678901234+"

chorus:    5+ 9=14 "chr: -234m678+"
           8+ 9=17 "chorus: -234m678+"
           5+17=22 "chr: -2345678m0123456+"
           8+17=25 "chorus: -2345678m0123456+"
           5+33=38 "chr: -234567890123456m890123456789012+"
           8+33=41 "chorus: -234567890123456m890123456789012+"
           5+65=70 "chr: -2345678901234567890123456789012m4567890123456789012345678901234+"
           8+65=73 "chorus: -2345678901234567890123456789012m4567890123456789012345678901234+"

surround:  6+ 1= 7 "srnd: 1"
          10+ 1=11 "surround: 1"

panning:   5+ 9=14 "pan: l234m678r"
           9+ 9=18 "panning: l234m678r"
           5+17=22 "pan: l2345678m0123456r"
           9+17=26 "panning: l2345678m0123456r"
           5+33=38 "pan: l234567890123456m890123456789012r"
           9+33=42 "panning: l234567890123456m890123456789012r"
           5+65=70 "pan: l2345678901234567890123456789012m4567890123456789012345678901234r"
           9+65=74 "panning: l2345678901234567890123456789012m4567890123456789012345678901234r"

balance:   5+ 9=14 "bal: l234m678r"
           9+ 9=18 "balance: l234m678r"
           5+17=22 "bal: l2345678m0123456r"
           9+17=26 "balance: l2345678m0123456r"
           5+33=38 "bal: l234567890123456m890123456789012r"
           9+33=42 "balance: l234567890123456m890123456789012r"
           5+65=70 "bal: l2345678901234567890123456789012m4567890123456789012345678901234r"
           9+65=74 "balance: l2345678901234567890123456789012m4567890123456789012345678901234r"

spd/ptch       =22 "spd: 123% = ptch: 123%"
               =25 "speed: 123% = pitch: 123%"
*/

void cpiDrawG1String (struct settings *g1)
{
	int volumemode = 0;
	int echomode = 0;
	int reverbchorusmode = 0;
	int surroundmode = 0;
	int panningbalancemode = 0;
	int speedpitchmode = 0;
	const uint8_t volumesizes[8] = {13, 16, 21, 24, 37, 40, 69, 72};
	const uint8_t echosizes[2] = {7, 13};
	const uint8_t reverbsizes[8] = {14, 17, 22, 25, 38, 41, 70, 73};
	const uint8_t chorussizes[8] = {14, 17, 22, 25, 38, 41, 70, 73};
	const uint8_t surroundsizes[2] = {7, 11};
	const uint8_t panningsizes[8] = {14, 18, 22, 26, 38, 42, 70, 74};
	const uint8_t balancesizes[8] = {14, 18, 22, 26, 38, 42, 70, 74};
	const uint8_t speedpitchsizes[2] = {22, 25};

	int interspace1;
	int interspace2;
	int headspace;
	int endspace;
	int changed = 1;
	int width;
	int x;

	if (plScrWidth >= 90)
	{
		endspace = headspace = (plScrWidth - 60) / 30;
	} else {
		endspace = headspace = 0;
	}

	/* increase the view-mode of each component until we can't grow them anymore */
#warning TODO, we can cache this as long a g1->viewfx and screenwidth has not changed!
	if (g1->viewfx)
	{
		width = (int)volumesizes[volumemode] + echosizes[echomode] + reverbsizes[reverbchorusmode] + chorussizes[reverbchorusmode] + speedpitchsizes[speedpitchmode] + headspace + endspace;

		if (!echomode)
		{
			int n = volumesizes[volumemode] + echosizes[1] + reverbsizes[reverbchorusmode] + chorussizes[reverbchorusmode] + speedpitchsizes[speedpitchmode] + headspace + endspace;
			if ((n + 4) <= plScrWidth)
			{
				echomode = 1;
				width = n;
			}
		}
		if (!speedpitchmode)
		{
			int n = (int)volumesizes[volumemode] + echosizes[echomode] + reverbsizes[reverbchorusmode] + chorussizes[reverbchorusmode] + speedpitchsizes[1] + headspace + endspace;
			if ((n + 4) <= plScrWidth)
			{
				speedpitchmode = 1;
				width = n;
			}
		}
		while (changed)
		{
			changed = 0;
			if (volumemode < 7)
			{
				int n = (int)volumesizes[volumemode+1] + echosizes[echomode] + reverbsizes[reverbchorusmode] + chorussizes[reverbchorusmode] + speedpitchsizes[speedpitchmode] + headspace + endspace;
				if ((n + 4) <= plScrWidth)
				{
					volumemode++;
					width = n;
					changed = 1;
				}
			}
			if (reverbchorusmode < 7)
			{
				int n = (int)volumesizes[volumemode] + echosizes[echomode] + reverbsizes[reverbchorusmode+1] + chorussizes[reverbchorusmode+1] + speedpitchsizes[speedpitchmode] + headspace + endspace;
				if ((n + 4) <= plScrWidth)
				{
					reverbchorusmode++;
					width = n;
					changed = 1;
				}
			}
		}
	} else {
		width = (int)volumesizes[volumemode] + surroundsizes[surroundmode] + panningsizes[panningbalancemode] + balancesizes[panningbalancemode] + speedpitchsizes[speedpitchmode] + headspace + endspace;

		if (!surroundmode)
		{
			int n = (int)volumesizes[volumemode] + surroundsizes[1] + panningsizes[panningbalancemode] + balancesizes[panningbalancemode] + speedpitchsizes[speedpitchmode] + headspace + endspace;
			if ((n + 4) <= plScrWidth)
			{
				surroundmode = 1;
				width = n;
			}
		}
		if (!speedpitchmode)
		{
			int n = (int)volumesizes[volumemode] + surroundsizes[surroundmode] + panningsizes[panningbalancemode] + balancesizes[panningbalancemode] + speedpitchsizes[1] + headspace + endspace;
			if ((n + 4) <= plScrWidth)
			{
				speedpitchmode = 1;
				width = n;
			}
		}
		while (changed)
		{
			changed = 0;
			if (volumemode < 7)
			{
				int n = (int)volumesizes[volumemode+1] + surroundsizes[surroundmode] + panningsizes[panningbalancemode] + balancesizes[panningbalancemode] + speedpitchsizes[speedpitchmode] + headspace + endspace;
				if ((n + 4) <= plScrWidth)
				{
					volumemode++;
					width = n;
					changed = 1;
				}
			}
			if (panningbalancemode < 7)
			{
				int n = (int)volumesizes[volumemode] + surroundsizes[surroundmode] + panningsizes[panningbalancemode+1] + balancesizes[panningbalancemode+1] + speedpitchsizes[speedpitchmode] + headspace + endspace;
				if ((n + 4) <= plScrWidth)
				{
					panningbalancemode++;
					width = n;
					changed = 1;
				}
			}
		}
	}

	interspace1 = (plScrWidth - width) / 4;
	interspace2 = (plScrWidth - width) % 4;

	displayvoid (1, 0, headspace);
	x=headspace;

	{
		int va, vi; /* volume active / inactive */

		if (volumemode & 1)
		{
			displaystr (1, x, 0x09, "volume: ", 8); x += 8;
		} else {
			displaystr (1, x, 0x09, "vol: ",    5); x += 5;
		}
		switch (volumemode>>1)
		{
			default:
			case 0: va=(g1->vol+4)>>3; vi= 8-va; break;
			case 1: va=(g1->vol+2)>>2; vi=16-va; break;
			case 2: va=(g1->vol+1)>>1; vi=32-va; break;
			case 3: va= g1->vol      ; vi=64-va; break;
		}
		displaychr (1, x, 0x0f, '\xfe', va); x += va;
		displaychr (1, x, 0x09, '\xfa', vi); x += vi;
	}

	displayvoid (1, x, interspace1 + (!!interspace2)); x += interspace1 + (!!interspace2);
	if (interspace2) interspace2--;

	if (g1->viewfx)
	{
		if (echomode)
		{
			displaystr (1, x, 0x09, "echoactive: ", 12); x += 12;
		} else {
			displaystr (1, x, 0x09, "echo: ",       5);  x +=  5;
		}
		displaystr (1, x, 0x0f, g1->useecho?"x":"o", 1); x += 1;

		displayvoid (1, x, interspace1 + (!!interspace2)); x += interspace1 + (!!interspace2);
		if (interspace2) interspace2--;

		{
			int l, w;
			const char *temp;

			if (reverbchorusmode & 1)
			{
				displaystr (1, x, 0x09, "reverb: ", 8); x += 8;
			} else {
				displaystr (1, x, 0x09, "rev: ",    5); x += 5;
			}

			switch (reverbchorusmode >> 1)
			{
				default:
				case 0: l = ((g1->reverb+70)>>4); w =  8; temp = "-\xfa\xfa\xfam\xfa\xfa\xfa+"; break;
				case 1: l = ((g1->reverb+68)>>3); w = 16; temp = "-\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfa+"; break;
				case 2: l = ((g1->reverb+66)>>2); w = 32; temp = "-\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa+"; break;
				case 3: l = ((g1->reverb+64)>>1); w = 64; temp = "-\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa+"; break;
			}
			displaystr (1, x, 0x07, temp, l); x += l;
			displaychr (1, x, 0x0f, 'I', 1); x += 1;
			displaystr (1, x, 0x07, temp + l + 1, w - l); x += w - l;
		}

		displayvoid (1, x, interspace1 + (!!interspace2)); x += interspace1 + (!!interspace2);
		if (interspace2) interspace2--;

		{
			int l, w;
			const char *temp;

			if (reverbchorusmode & 1)
			{
				displaystr (1, x, 0x09, "chorus: ", 8); x += 8;
			} else {
				displaystr (1, x, 0x09, "chr: ",    5); x += 5;
			}

			switch (reverbchorusmode >> 1)
			{
				default:
				case 0: l = ((g1->chorus+70)>>4); w =  8; temp = "-\xfa\xfa\xfam\xfa\xfa\xfa+"; break;
				case 1: l = ((g1->chorus+68)>>3); w = 16; temp = "-\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfa+"; break;
				case 2: l = ((g1->chorus+66)>>2); w = 32; temp = "-\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa+"; break;
				case 3: l = ((g1->chorus+64)>>1); w = 64; temp = "-\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa+"; break;
			}
			displaystr (1, x, 0x07, temp, l); x += l;
			displaychr (1, x, 0x0f, 'I', 1); x += 1;
			displaystr (1, x, 0x07, temp + l + 1, w - l); x += w - l;
		}
	} else {
		if (surroundmode)
		{
			displaystr (1, x, 0x09, "surround: ", 10); x += 10;
		} else {
			displaystr (1, x, 0x09, "srnd: ",      5); x +=  5;
		}
		displaystr (1, x, 0x0f, g1->srnd?"x":"o", 1); x += 1;

		displayvoid (1, x, interspace1 + (!!interspace2)); x += interspace1 + (!!interspace2);
		if (interspace2) interspace2--;

		{
			int l, r, w;
			char _l, _r;
			const char *temp;

			if (panningbalancemode & 1)
			{
				displaystr (1, x, 0x09, "panning: ", 9); x += 9;
			} else {
				displaystr (1, x, 0x09, "pan: ",     5); x += 5;
			}

			switch (panningbalancemode >> 1)
			{
				default:
				case 0: r = ((g1->pan+70)>>4); w =  8; temp = "l\xfa\xfa\xfam\xfa\xfa\xfar"; break;
				case 1: r = ((g1->pan+68)>>3); w = 16; temp = "l\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfar"; break;
				case 2: r = ((g1->pan+66)>>2); w = 32; temp = "l\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfar"; break;
				case 3: r = ((g1->pan+64)>>1); w = 64; temp = "l\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfar"; break;
			}
			l = w - r;
			if (r < l)
			{ /* swap the positions and text, so they are in ascending left right order */
				_l = 'r';
				_r = 'l';
				l = r;
				r = w - l;
			} else {
				_l = 'l';
				_r = 'r';
			}
			displaystr (1, x, 0x07, temp, l); x += l;
			if (l==r)
			{
				displaychr (1, x, 0x0f, 'm', 1); x += 1;
				displaystr (1, x, 0x07, temp + l + 1, w - l); x += w - l;
			} else {
				displaychr (1, x, 0x0f, _l, 1); x += 1;
				displaystr (1, x, 0x07, temp + l + 1, r - l - 1); x += r - l -1;
				displaychr (1, x, 0x0f, _r, 1); x += 1;
				displaystr (1, x, 0x07, temp + r + 1, w - r); x += w - r;
			}
		}

		displayvoid (1, x, interspace1 + (!!interspace2)); x += interspace1 + (!!interspace2);
		if (interspace2) interspace2--;

		{
			int l, w;
			const char *temp;

			if (panningbalancemode & 1)
			{
				displaystr (1, x, 0x09, "balance: ", 9); x += 9;
			} else {
				displaystr (1, x, 0x09, "bal: ",     5); x += 5;
			}

			switch (panningbalancemode >> 1)
			{
				default:
				case 0: l = ((g1->bal+70)>>4); w =  8; temp = "l\xfa\xfa\xfam\xfa\xfa\xfar"; break;
				case 1: l = ((g1->bal+68)>>3); w = 16; temp = "l\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfar"; break;
				case 2: l = ((g1->bal+66)>>2); w = 32; temp = "l\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfar"; break;
				case 3: l = ((g1->bal+64)>>1); w = 64; temp = "l\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfar"; break;
			}
			displaystr (1, x, 0x07, temp, l); x += l;
			displaychr (1, x, 0x0f, 'I', 1); x += 1;
			displaystr (1, x, 0x07, temp + l + 1, w - l); x += w-l;
		}
	}

	displayvoid (1, x, interspace1 + (!!interspace2)); x += interspace1 + (!!interspace2);
	if (interspace2) interspace2--;

	{
		char temp[4];
		if (speedpitchmode)
		{
			displaystr (1, x, 0x09, "speed: ", 7); x += 7;
		} else {
			displaystr (1, x, 0x09, "spd: ", 5); x += 5;
		}
		snprintf (temp, sizeof (temp), "%3d", saturate (g1->speed * 100 / 256, 0, 999));
		displaystr (1, x, 0x0f, temp, 3); x += 3;
		displaystr (1, x, 0x07, "% ", 2); x += 2;
		displaystr (1, x, 0x09, g1->splock?"\x1d ":"  ", 2); x+= 2;
		if (speedpitchmode)
		{
			displaystr (1, x, 0x09, "pitch: ", 7); x += 7;
		} else {
			displaystr (1, x, 0x09, "ptch: ", 6); x += 6;
		}
		snprintf (temp, sizeof (temp), "%3d", saturate (g1->pitch * 100 / 256, 0, 999));
		displaystr (1, x, 0x0f, temp, 3); x += 3;
		displaychr (1, x, 0x07, '%', 1); x += 1;
	}

	displayvoid (1, x, endspace); x += endspace;
}

/*
pos:123%
position:123%
pos:123% 12345678/12345678 KB
position:123% 12345678/12345678 KB

opt: 1234567890123456789012345
option: 1234567890123456789012345
opt: 1234567890123456789123456789012345678901234567890
option: 1234567890123456789123456789012345678901234567890

rate: 1234kbs
bitrate: 1234kbs

paused time:123:12
playback paused time:123:12

file: 12345678.123
filename: 12345678.123
file: 1234567890123456.123
filename: 1234567890123456.123

len:123.12      (MDB playtime)
length:123.12   (MDB playtime)

date: 12.12.1234      (MDB date)

title: 1234567890123456                                                   16 (MDB title)
title: 123456789012345678901234                                           24
title: 12345678901234567890123456789012                                   32
title: 1234567890123456789012345678901234567890                           40
title: 123456789012345678901234567890123456789012345678                   48
title: 12345678901234567890123456789012345678901234567890123456           56
title: 1234567890123456789012345678901234567890123456789012345678901234   64

composer: 1234567890123456                                                   16 (MDB composer)
composer: 123456789012345678901234                                           24
composer: 12345678901234567890123456789012                                   32
composer: 1234567890123456789012345678901234567890                           40
composer: 123456789012345678901234567890123456789012345678                   48
composer: 12345678901234567890123456789012345678901234567890123456           56
composer: 1234567890123456789012345678901234567890123456789012345678901234   64

artist: 1234567890123456                                                   16 (MDB artist)
artist: 123456789012345678901234                                           24
artist: 12345678901234567890123456789012                                   32
artist: 1234567890123456789012345678901234567890                           40
artist: 123456789012345678901234567890123456789012345678                   48
artist: 12345678901234567890123456789012345678901234567890123456           56
artist: 1234567890123456789012345678901234567890123456789012345678901234   64

style: 1234567890123456                                                   16 (MDB style)
style: 123456789012345678901234                                           24
style: 12345678901234567890123456789012                                   32
style: 1234567890123456789012345678901234567890                           40
style: 123456789012345678901234567890123456789012345678                   48
style: 12345678901234567890123456789012345678901234567890123456           56
style: 1234567890123456789012345678901234567890123456789012345678901234   64

comment: 1234567890123456                                                   16 (MDB commment)
comment: 123456789012345678901234                                           24
comment: 12345678901234567890123456789012                                   32
comment: 1234567890123456789012345678901234567890                           40
comment: 123456789012345678901234567890123456789012345678                   48
comment: 12345678901234567890123456789012345678901234567890123456           56
comment: 1234567890123456789012345678901234567890123456789012345678901234   64

album: 1234567890123456                                                   16 (MDB album)
album: 123456789012345678901234                                           24
album: 12345678901234567890123456789012                                   32
album: 1234567890123456789012345678901234567890                           40
album: 123456789012345678901234567890123456789012345678                   48
album: 12345678901234567890123456789012345678901234567890123456           56
album: 1234567890123456789012345678901234567890123456789012345678901234   64

pos:123%  12345678/12345678 KB  rate: 12345kbs title: 1234567890123456  comment: 1234567890123456  album: 1234567890123456  date: 12.12.1234  length: 123:12
filename: 12345678.123  composer: 1234567890123456  artist: 12345678990123456  style: 1234567890123456 opt: 1234567890123456789012345  paused time:123:12
*/

struct GStringElement;
struct GStringElement
{
	int (*allowgrow)(const void *inputa, const void *inputb, const void *inputc, int nextsize); /* returns the grow size, if it wants to grow */
	void (*render)(const void *inputa, const void *inputb, const void *inputc, const int size, int *x, const int lineno);
	int priority;
	int sizescount;
};

static int GString_pos_allowgrow (const void *inputa, const void *inputb, const void *inputc, int nextsize)
{
	const char *sizesuffix = inputc;

	if (!(*sizesuffix))
	{
		switch (nextsize)
		{
			case 1: return 8;
			case 2: return 13-8;
			case 3: return 26-13;
			case 4: return 31-26;
			default: return 0;
		}
	} else {
		switch (nextsize)
		{
			case 1: return 8;
			case 2: return 13-8;
			case 3: return 29-13;
			case 4: return 34-29;
			default: return 0;
		}
	}
}

static void GString_pos_render (const void *inputa, const void *inputb, const void *inputc, const int size, int *x, const int lineno)
{
	const uint64_t *pos = inputa;
	const uint64_t *filesize = inputb;
	const char *sizesuffix = inputc;
	char b[10];

	switch (size)
	{
		case 1:
		case 3: displaystr (lineno, *x, 0x09, "pos:",      4); (*x) += 4; break;
		case 2:
		case 4: displaystr (lineno, *x, 0x09, "position:", 9); (*x) += 9; break;
	}

	snprintf (b, 4, "%3d", (int)((*pos) * 100 / (*filesize)));
	displaystr (lineno, *x, 0x0f,   b, 3); (*x)+=3;
	displaychr (lineno, *x, 0x07, '%', 1); (*x)+=1;

	if (size > 2)
	{
		if (!(*sizesuffix))
		{
			snprintf (b, 10, " %8" PRIu64, (*pos));
			displaystr (lineno, *x, 0x0f,     b, 9); (*x) += 9;
			displaychr (lineno, *x, 0x07,   '/', 1); (*x) += 1;
			snprintf (b, 9, "%8" PRIu64, (*filesize));
			displaystr (lineno, *x, 0x0f,     b, 8); (*x) += 8;
		} else {
			snprintf (b, 10, " %8" PRIu64, saturate((*pos)>>10, 0, 99999999));
			displaystr (lineno, *x, 0x0f,     b, 9); (*x) += 9;
			displaychr (lineno, *x, 0x07,   '/', 1); (*x) += 1;
			snprintf (b, 9, "%8" PRIu64, saturate((*filesize)>>10, 0, 99999999));
			displaystr (lineno, *x, 0x0f,     b, 8); (*x) += 8;
			displaystr (lineno, *x, 0x07, " KB", 3); (*x) += 3;
		}
	}
}

static struct GStringElement GString_pos =
{
	GString_pos_allowgrow,
	GString_pos_render,
	1,
	4
};

static int GString_bitrate_allowgrow (const void *inputa, const void *inputb, const void *inputc, int nextsize)
{
	const int_fast16_t *kbs = inputa;

	if (*kbs < 0)
		return 0;

	switch (nextsize)
	{
		case 1: return 13;
		case 2: return 16-13;
		default: return 0;
	}
}

static void GString_bitrate_render (const void *inputa, const void *inputb, const void *inputc, const int size, int *x, const int lineno)
{
	const int_fast16_t *kbs = inputa;
	char b[6];

	if (size == 1)
	{
		displaystr (lineno, *x, 0x09, "rate:", 5); (*x) += 5;
	} else {
		displaystr (lineno, *x, 0x09, "bitrate:", 8); (*x) += 8;
	}
	snprintf (b, 6, "%5d", (int)*kbs);
	displaystr (lineno, *x, 0x0f,     b, 5); (*x) += 5;
	displaystr (lineno, *x, 0x07, "kbs", 3); (*x) += 3;
}

static struct GStringElement GString_bitrate =
{
	GString_bitrate_allowgrow,
	GString_bitrate_render,
	0,
	2
};

static int GString_head5_allowgrow (const void *inputa, const void *inputb, const void *inputc, int nextsize)
{
	const long len = (const long)inputb;

	if (!len) return 0;
	if (plCompoMode) return 0;

	switch (nextsize)
	{
		case 1: return 23;
		case 2: if (len <= 16) return 0; return 8;
		case 3: if (len <= 24) return 0; return 8;
		case 4: if (len <= 32) return 0; return 8;
		case 5: if (len <= 40) return 0; return 8;
		case 6: if (len <= 48) return 0; return 8;
		case 7: if (len <= 56) return 0; return 8;
		default: return 0;
	}
}

static int GString_head6_allowgrow (const void *inputa, const void *inputb, const void *inputc, int nextsize)
{
	const long len = (const long)inputb;

	if (!len) return 0;
	if (plCompoMode) return 0;

	switch (nextsize)
	{
		case 1: return 24;
		case 2: if (len <= 16) return 0; return 8;
		case 3: if (len <= 24) return 0; return 8;
		case 4: if (len <= 32) return 0; return 8;
		case 5: if (len <= 40) return 0; return 8;
		case 6: if (len <= 48) return 0; return 8;
		case 7: if (len <= 56) return 0; return 8;
		default: return 0;
	}
}

static int GString_head7_allowgrow (const void *inputa, const void *inputb, const void *inputc, int nextsize)
{
	const long len = (const long)inputb;

	if (!len) return 0;
	if (plCompoMode) return 0;

	switch (nextsize)
	{
		case 1: return 25;
		case 2: if (len <= 16) return 0; return 8;
		case 3: if (len <= 24) return 0; return 8;
		case 4: if (len <= 32) return 0; return 8;
		case 5: if (len <= 40) return 0; return 8;
		case 6: if (len <= 48) return 0; return 8;
		case 7: if (len <= 56) return 0; return 8;
		default: return 0;
	}
}

static int GString_head8_allowgrow (const void *inputa, const void *inputb, const void *inputc, int nextsize)
{
	const long len = (const long)inputb;

	if (!len) return 0;
	if (plCompoMode) return 0;

	switch (nextsize)
	{
		case 1: return 26;
		case 2: if (len <= 16) return 0; return 8;
		case 3: if (len <= 24) return 0; return 8;
		case 4: if (len <= 32) return 0; return 8;
		case 5: if (len <= 40) return 0; return 8;
		case 6: if (len <= 48) return 0; return 8;
		case 7: if (len <= 56) return 0; return 8;
		default: return 0;
	}
}

static void GString_title_render (const void *inputa, const void *inputb, const void *inputc, const int size, int *x, const int lineno)
{
	const char *title = inputa;
	displaystr (lineno, *x, 0x09, "title: ", 7); (*x) += 7;
	displaystr_utf8 (lineno, *x, 0x0f, title, 8 + size * 8); (*x) += 8 + size * 8;
}

static void GString_album_render (const void *inputa, const void *inputb, const void *inputc, const int size, int *x, const int lineno)
{
	const char *album = inputa;
	displaystr (lineno, *x, 0x09, "album: ", 7); (*x) += 7;
	displaystr_utf8 (lineno, *x, 0x0f, album, 8 + size * 8); (*x) += 8 + size * 8;
}

static void GString_style_render (const void *inputa, const void *inputb, const void *inputc, const int size, int *x, const int lineno)
{
	const char *style = inputa;
	displaystr (lineno, *x, 0x09, "style: ", 7); (*x) += 7;
	displaystr_utf8 (lineno, *x, 0x0f, style, 8 + size * 8); (*x) += 8 + size * 8;
}

static void GString_artist_render (const void *inputa, const void *inputb, const void *inputc, const int size, int *x, const int lineno)
{
	const char *artist = inputa;
	displaystr (lineno, *x, 0x09, "artist: ", 8); (*x) += 8;
	displaystr_utf8 (lineno, *x, 0x0f, artist, 8 + size * 8); (*x) += 8 + size * 8;
}

static void GString_comment_render (const void *inputa, const void *inputb, const void *inputc, const int size, int *x, const int lineno)
{
	const char *comment = inputa;
	displaystr (lineno, *x, 0x09, "comment: ", 9); (*x) += 9;
	displaystr_utf8 (lineno, *x, 0x0f, comment, 8 + size * 8); (*x) += 8 + size * 8;
}

static void GString_composer_render (const void *inputa, const void *inputb, const void *inputc, const int size, int *x, const int lineno)
{
	const char *comment = inputa;
	displaystr (lineno, *x, 0x09, "composer: ", 10); (*x) += 10;
	displaystr_utf8 (lineno, *x, 0x0f, comment, 8 + size * 8); (*x) += 8 + size * 8;
}

static struct GStringElement GString_title =
{
	GString_head5_allowgrow,
	GString_title_render,
	0,
	7
};

static struct GStringElement GString_album =
{
	GString_head5_allowgrow,
	GString_album_render,
	0,
	7
};

static struct GStringElement GString_style =
{
	GString_head5_allowgrow,
	GString_style_render,
	0,
	7
};

static struct GStringElement GString_artist =
{
	GString_head6_allowgrow,
	GString_artist_render,
	0,
	7
};

static struct GStringElement GString_comment =
{
	GString_head7_allowgrow,
	GString_comment_render,
	0,
	7
};

static struct GStringElement GString_composer =
{
	GString_head8_allowgrow,
	GString_composer_render,
	0,
	7
};

static int GString_date_allowgrow (const void *inputa, const void *inputb, const void *inputc, int nextsize)
{
	const uint32_t *date = inputa;

	if (!*date)
	{
		return 0;
	}

	switch (nextsize)
	{
		case 1: return 16;
		default: return 0;
	}
}

static void GString_date_render (const void *inputa, const void *inputb, const void *inputc, const int size, int *x, const int lineno)
{
	const uint32_t *date = inputa;
	char temp[11];

	displaystr (lineno, *x, 0x09, "date: ", 6); (*x) += 6;

	if ((*date)&0xFF)
	{
		snprintf (temp, sizeof (temp), "%02d.", saturate((*date) & 0xff, 0, 99));
	} else {
		snprintf (temp, sizeof (temp), "   ");
	}
	if ((*date)&0xFFFF)
	{
		snprintf (temp + 3, sizeof (temp) - 3, "%02d.", saturate(((*date) >> 8)&0xff, 0, 99));
	} else {
		snprintf (temp + 3, sizeof (temp) - 3, "   ");
	}
	if ((*date)>>16)
	{
		snprintf (temp + 6, sizeof (temp) - 6, "%4d", saturate(((*date) >> 16), 0, 9999));
		if (!(((*date)>>16)/100))
		{
			temp[6] = '\'';
		}
	}
	displaystr (lineno, *x, 0x0f, temp, 10); (*x) += 10;
}

static struct GStringElement GString_date =
{
	GString_date_allowgrow,
	GString_date_render,
	0,
	1
};

static int GString_playtime_allowgrow (const void *inputa, const void *inputb, const void *inputc, int nextsize)
{
	const uint16_t *playtime = inputa;

	if (!*playtime)
	{
		return 0;
	}
	switch (nextsize)
	{
		case 1: return 10;
		case 2: return 13-10;
		default: return 0;
	}
}

static void GString_playtime_render (const void *inputa, const void *inputb, const void *inputc, const int size, int *x, const int lineno)
{
	const uint16_t *playtime = inputa;
	char temp[7];

	if (size == 1)
	{
		displaystr (lineno, *x, 0x09, "len:", 4); (*x) += 4;
	} else {
		displaystr (lineno, *x, 0x09, "length:", 7); (*x) += 7;
	}
	snprintf(temp, 7, "%3d.%02d", saturate((*playtime) / 60, 0, 999), (*playtime) % 60);
	displaystr (lineno, *x, 0x0f, temp, 6); (*x) += 6;
}

static struct GStringElement GString_playtime =
{
	GString_playtime_allowgrow,
	GString_playtime_render,
	0,
	2
};

static int GString_filename_allowgrow (const void *inputa, const void *inputb, const void *inputc, int nextsize)
{
	switch (nextsize)
	{
		case 1: return 18;
		case 2: return 22-18;
		case 3: return 26-22;
		case 4: return 30-26;
		default: return 0;
	}
}

static void GString_filename_render (const void *inputa, const void *inputb, const void *inputc, const int size, int *x, const int lineno)
{
	const char *filename8_3 = inputa;
	const char *filename16_3 = inputb;

	switch (size)
	{
		case 1:
		case 3: displaystr (lineno, *x, 0x09, "file: ",      6); (*x) +=  6; break;
		case 2:
		case 4: displaystr (lineno, *x, 0x09, "filename: ", 10); (*x) += 10; break;
	}
	switch (size)
	{
		case 1:
		case 2: displaystr_utf8 (lineno, *x, 0x0f, filename8_3,  12); (*x) += 12; break;
		case 3:
		case 4: displaystr_utf8 (lineno, *x, 0x0f, filename16_3, 20); (*x) += 20; break;
	}
}

static struct GStringElement GString_filename =
{
	GString_filename_allowgrow,
	GString_filename_render,
	0,
	4
};

static int GString_option_allowgrow (const void *inputa, const void *inputb, const void *inputc, int nextsize)
{
	const char *opt25 = inputa;
	const char *opt50 = inputb;

	if (!opt25) return 0;
	if (!opt25[0]) return 0;

	switch (nextsize)
	{
		case 1: return 30;
		case 2: return 33-30;
		case 3: if (!strcmp(opt25, opt50)) return 0; return 55-33;
		case 4: return 58 - 55;
		default: return 0;
	}
}

static void GString_option_render (const void *inputa, const void *inputb, const void *inputc, const int size, int *x, const int lineno)
{
	const char *opt25 = inputa;
	const char *opt50 = inputb;

	switch (size)
	{
		case 1:
		case 3: displaystr (lineno, *x, 0x09, "opt: ",    5); (*x) += 5; break;
		case 2:
		case 4: displaystr (lineno, *x, 0x09, "option: ", 8); (*x) += 8; break;
	}
	switch (size)
	{
		case 1:
		case 2: displaystr_utf8 (lineno, *x, 0x0f, opt25, 25); (*x) += 25; break;
		case 3:
		case 4: displaystr_utf8 (lineno, *x, 0x0f, opt50, 50); (*x) += 50; break;
	}
}

static struct GStringElement GString_option =
{
	GString_option_allowgrow,
	GString_option_render,
	0,
	4
};

static int GString_pausetime_allowgrow (const void *inputa, const void *inputb, const void *inputc, int nextsize)
{
	switch (nextsize)
	{
		case 1: return 18;
		case 2: return 27-18;
		default: return 0;
	}
}

static void GString_pausetime_render (const void *inputa, const void *inputb, const void *inputc, const int size, int *x, const int lineno)
{
	const uint_fast8_t  *inpause = inputa;
	const uint_fast16_t *seconds = inputb;
	char temp[7];

	switch (size)
	{
		case 1: displaystr (lineno, *x, (*inpause) ? 0x0c : 0x00, "paused ",           7); (*x) +=  7; break;
		case 2: displaystr (lineno, *x, (*inpause) ? 0x0c : 0x00, "playback paused ", 16); (*x) += 16; break;
	}
	displaystr (lineno, *x, 0x09, "time:", 5); (*x) += 5;

	snprintf(temp, 7, "%3d.%02d", (int)((*seconds) / 60), (int)((*seconds) % 60));
	displaystr (lineno, *x, 0x0f, temp, 6); (*x) += 6;
}

static struct GStringElement GString_pausetime =
{
	GString_pausetime_allowgrow,
	GString_pausetime_render,
	1,
	2
};

static int GString_song_x_y_allowgrow (const void *inputa, const void *inputb, const void *inputc, int nextsize)
{
	const int *songx = inputa;
	const int *songy = inputb;

	if (((*songx) <= 0) && ((*songy) <= 0))
	{
		return 0;
	}
	switch (nextsize)
	{
		case 1:
		{
			int L;

			if (*songy < 10)
			{
				L = 2;
			} else if (*songy < 100)
			{
				L = 4;
			} else {
				L = 6;
			}

			return 9+L;
		}
		case 2:
			return 3;
		default:
			return 0;
	}
}

static void GString_song_x_y_render (const void *inputa, const void *inputb, const void *inputc, const int size, int *x, const int lineno)
{
	const int *songx = inputa;
	const int *songy = inputb;
	char temp[4];

	displaystr (lineno, *x, 0x09, "song:", 5); (*x) += 6;
	if (*songy < 10)
	{
		snprintf (temp, sizeof (temp), "%01d", saturate(*songx, 0, 9));
		displaystr (lineno, *x, 0x0f, temp, 2); (*x) += 1;
	} else if (*songy < 100)
	{
		snprintf (temp, sizeof (temp), "%02d", saturate(*songx, 0, 99));
		displaystr (lineno, *x, 0x0f, temp, 2); (*x) += 2;
	} else {
		snprintf (temp, sizeof (temp), "%03d", saturate(*songx, 0, 999));
		displaystr (lineno, *x, 0x0f, temp, 3); (*x) += 3;
	}
	if (size==1)
	{
		displaystr (lineno, *x, 0x07, "/",  1); (*x) += 1;
	} else {
		displaystr (lineno, *x, 0x07, " of ",  4); (*x) += 4;
	}
	if (*songy < 10)
	{
		snprintf (temp, sizeof (temp), "%01d", saturate(*songy, 0, 9));
		displaystr (lineno, *x, 0x0f, temp, 2); (*x) += 1;
	} else if (*songy < 100)
	{
		snprintf (temp, sizeof (temp), "%02d", saturate(*songy, 0, 99));
		displaystr (lineno, *x, 0x0f, temp, 2); (*x) += 2;
	} else {
		snprintf (temp, sizeof (temp), "%03d", saturate(*songy, 0, 999));
		displaystr (lineno, *x, 0x0f, temp, 3); (*x) += 3;
	}
}

static struct GStringElement GString_song_x_y =
{
	GString_song_x_y_allowgrow,
	GString_song_x_y_render,
	1,
	2
};

static int GString_row_x_y_allowgrow (const void *inputa, const void *inputb, const void *inputc, int nextsize)
{
	switch (nextsize)
	{
		case 1: return 10;
		default: return 0;
	}
}

static void GString_row_x_y_render (const void *inputa, const void *inputb, const void *inputc, const int size, int *x, const int lineno)
{
	const uint8_t *rowx = inputa;
	const uint8_t *rowy = inputb;
	char temp[3];

	displaystr (lineno, *x, 0x09, "row: ", 5); (*x) += 5;
	snprintf (temp, sizeof (temp), "%02X", *rowx);
	displaystr (lineno, *x, 0x0f, temp,    2); (*x) += 2;
	displaystr (lineno, *x, 0x07, "/",     1); (*x) += 1;
	snprintf (temp, sizeof (temp), "%02X", *rowy);
	displaystr (lineno, *x, 0x0f, temp,    2); (*x) += 2;
}

static struct GStringElement GString_row_x_y =
{
	GString_row_x_y_allowgrow,
	GString_row_x_y_render,
	1,
	1
};

static int GString_channels_x_y_allowgrow (const void *inputa, const void *inputb, const void *inputc, int nextsize)
{
	const uint8_t *chany = inputb;

	if ((*chany) <= 0)
	{
		return 0;
	}

	switch (nextsize)
	{
		case 1: return 11;
		case 2: return 15-11;
		default: return 0;
	}
}

static void GString_channels_x_y_render (const void *inputa, const void *inputb, const void *inputc, const int size, int *x, const int lineno)
{
	const uint8_t *chanx = inputa;
	const uint8_t *chany = inputb;
	char temp[3];

	if (size == 1)
	{
		displaystr (lineno, *x, 0x09, "chan: ", 6); (*x) += 6;
	} else {
		displaystr (lineno, *x, 0x09, "channels: ", 10); (*x) += 10;
	}
	snprintf (temp, sizeof (temp), "%02d", saturate(*chanx, 0, 99));
	displaystr (lineno, *x, 0x0f, temp,    2); (*x) += 2;
	displaystr (lineno, *x, 0x07, "/",     1); (*x) += 1;
	snprintf (temp, sizeof (temp), "%02d", saturate(*chany, 0, 99));
	displaystr (lineno, *x, 0x0f, temp,    2); (*x) += 2;
}

static struct GStringElement GString_channels_x_y =
{
	GString_channels_x_y_allowgrow,
	GString_channels_x_y_render,
	1,
	2
};

static int GString_order_x_y_allowgrow (const void *inputa, const void *inputb, const void *inputc, int nextsize)
{
	const uint16_t *ordery = inputb;

	switch (nextsize)
	{
		case 1:
		{
			if ((*ordery) < 0x10  ) return 8;
			if ((*ordery) < 0x100 ) return 10;
			if ((*ordery) < 0x1000) return 12;
			return 14;
		}
		case 2: return 2;
		default: return 0;
	}
}

static void GString_order_x_y_render (const void *inputa, const void *inputb, const void *inputc, const int size, int *x, const int lineno)
{
	const uint16_t *orderx = inputa;
	const uint16_t *ordery = inputb;
	char temp[5];

	if (size == 1)
	{
		displaystr (lineno, *x, 0x09, "ord: ",   5); (*x) += 5;
	} else {
		displaystr (lineno, *x, 0x09, "order: ", 7); (*x) += 7;
	}

	       if ((*ordery) < 0x10  )
	{
		snprintf (temp, sizeof (temp), "%01X", *orderx);
		displaystr (lineno, *x, 0x0f, temp,    1); (*x) += 1;
		displaystr (lineno, *x, 0x07, "/",     1); (*x) += 1;
		snprintf (temp, sizeof (temp), "%01X", *ordery);
		displaystr (lineno, *x, 0x0f, temp,    1); (*x) += 1;
	} else if ((*ordery) < 0x100 )
	{
		snprintf (temp, sizeof (temp), "%02X", *orderx);
		displaystr (lineno, *x, 0x0f, temp,    2); (*x) += 2;
		displaystr (lineno, *x, 0x07, "/",     1); (*x) += 1;
		snprintf (temp, sizeof (temp), "%02X", *ordery);
		displaystr (lineno, *x, 0x0f, temp,    2); (*x) += 2;
	} else if ((*ordery) < 0x1000)
	{
		snprintf (temp, sizeof (temp), "%03X", *orderx);
		displaystr (lineno, *x, 0x0f, temp,    3); (*x) += 3;
		displaystr (lineno, *x, 0x07, "/",     1); (*x) += 1;
		snprintf (temp, sizeof (temp), "%03X", *ordery);
		displaystr (lineno, *x, 0x0f, temp,    3); (*x) += 3;
	} else {
		snprintf (temp, sizeof (temp), "%04X", *orderx);
		displaystr (lineno, *x, 0x0f, temp,    4); (*x) += 4;
		displaystr (lineno, *x, 0x07, "/",     1); (*x) += 1;
		snprintf (temp, sizeof (temp), "%04X", *ordery);
		displaystr (lineno, *x, 0x0f, temp,    4); (*x) += 4;
	}
}

static struct GStringElement GString_order_x_y =
{
	GString_order_x_y_allowgrow,
	GString_order_x_y_render,
	1,
	2
};

static int GString_speed_allowgrow (const void *inputa, const void *inputb, const void *inputc, int nextsize)
{
	switch (nextsize)
	{
		case 1: return 7;
		case 2: return 9-7;
		default: return 0;
	}
}

static void GString_speed_render (const void *inputa, const void *inputb, const void *inputc, const int size, int *x, const int lineno)
{
	const uint8_t *speed = inputa;
	char temp[4];

	if (size == 1)
	{
		displaystr (lineno, *x, 0x09, "spd:", 4); (*x) += 4;
	} else {
		displaystr (lineno, *x, 0x09, "speed:", 6); (*x) += 6;
	}

	snprintf (temp, sizeof (temp), "%3d", *speed);
	displaystr (lineno, *x, 0x0f, temp, 3); (*x) += 3;
}

static struct GStringElement GString_speed =
{
	GString_speed_allowgrow,
	GString_speed_render,
	1,
	2
};

static int GString_tempo_allowgrow (const void *inputa, const void *inputb, const void *inputc, int nextsize)
{
	switch (nextsize)
	{
		case 1: return 8;
		case 2: return 10-8;
		case 3: return 14-10;
		default: return 0;
	}
}

static void GString_tempo_render (const void *inputa, const void *inputb, const void *inputc, const int size, int *x, const int lineno)
{
	const uint8_t *tempo = inputa;
	char temp[4];

	switch (size)
	{
		case 1: displaystr (lineno, *x, 0x09, "bpm: ",        5); (*x) +=  5; break;
		case 2: displaystr (lineno, *x, 0x09, "tempo: ",      7); (*x) +=  7; break;
		case 3: displaystr (lineno, *x, 0x09, "tempo/bpm: ", 11); (*x) += 11; break;
	}

	snprintf (temp, sizeof (temp), "%3d", *tempo);
	displaystr (lineno, *x, 0x0f, temp, 3); (*x) += 3;
}

static struct GStringElement GString_tempo =
{
	GString_tempo_allowgrow,
	GString_tempo_render,
	1,
	3
};

static int GString_gvol_allowgrow (const void *inputa, const void *inputb, const void *inputc, int nextsize)
{
	const int16_t *gvol = inputa;

	if ((*gvol) < 0) return 0;

	switch (nextsize)
	{
		case 1: return 9;
		case 2: return 18-9;
		default: return 0;
	}
}

static void GString_gvol_render (const void *inputa, const void *inputb, const void *inputc, const int size, int *x, const int lineno)
{
	const int16_t *gvol = inputa;
	const int      *direction = inputb;
	char temp[3];

	switch (size)
	{
		case 1: displaystr (lineno, *x, 0x09, "gvol: ",           6); (*x) +=   6; break;
		case 2: displaystr (lineno, *x, 0x09, "global volume: ", 15); (*x) +=  15; break;
	}

	snprintf (temp, sizeof (temp), "%02X", *gvol);
	displaystr (lineno, *x, 0x0f, temp, 2); (*x) += 2;
	displaystr (lineno, *x, 0x0f, (*direction)>0?"\x18":(*direction)<0?"\x19":" ", 1); (*x) += 1;
}

static struct GStringElement GString_gvol =
{
	GString_gvol_allowgrow,
	GString_gvol_render,
	1,
	2
};

static int GString_amplification_allowgrow (const void *inputa, const void *inputb, const void *inputc, int nextsize)
{
	const int *amp = inputa;

	if ((*amp) < 0) return 0;

	switch (nextsize)
	{
		case 1: return 9;
		case 2: return 17-9;
		default: return 0;
	}
}

static void GString_amplification_render (const void *inputa, const void *inputb, const void *inputc, const int size, int *x, const int lineno)
{
	const int *amp = inputa;
	char temp[4];

	switch (size)
	{
		case 1: displaystr (lineno, *x, 0x09, "amp: ",          5); (*x) +=   5; break;
		case 2: displaystr (lineno, *x, 0x09, "amplication: ", 13); (*x) +=  13; break;
	}

	snprintf (temp, sizeof (temp), "%3d", saturate((*amp) * 100 / 64, 0, 999));
	displaystr (lineno, *x, 0x0f, temp, 3); (*x) += 3;
	displaystr (lineno, *x, 0x07, "%", 5); (*x) += 1;
}

static struct GStringElement GString_amplification =
{
	GString_amplification_allowgrow,
	GString_amplification_render,
	0,
	2
};

static int GString_filter_allowgrow (const void *inputa, const void *inputb, const void *inputc, int nextsize)
{
	const char *filter = inputa;

	if (!filter) return 0;

	switch (nextsize)
	{
		case 1: return 11;
		default: return 0;
	}
}

static void GString_filter_render (const void *inputa, const void *inputb, const void *inputc, const int size, int *x, const int lineno)
{
	const char *filter= inputa;

	displaystr (lineno, *x, 0x09, "filter: ", 8); (*x) += 8;
	displaystr (lineno, *x, 0x0f, filter, 3); (*x) += 3;
}

static struct GStringElement GString_filter =
{
	GString_filter_allowgrow,
	GString_filter_render,
	1,
	1
};

void GStrings_render (int lineno, int count, const struct GStringElement **Elements, int *sizes, const void **inputa, const void **inputb, const void **inputc)
{
	int headspace, endspace, interspace1, interspace2;
	int first = 1;
	int width = 0;
	int fields = 0;
	int changed;
	int x;
	int i;

	if (plScrWidth >= 90)
	{
		headspace = (plScrWidth - 60) / 30;
	} else {
		headspace = 0;
	}

	bzero (sizes, sizeof (int) * count);

	width = headspace * 2;

	do
	{
		changed = 0;

		for (i=0; i < count; i++)
		{
			if (((!first) || Elements[i]->priority) && sizes[i] < Elements[i]->sizescount)
			{
				int grow = Elements[i]->allowgrow (inputa[i], inputb[i], inputc[i], sizes[i] + 1);
				if (grow && (grow + !sizes[i] + width <= plScrWidth))
				{
					width += grow + ( fields ? (!sizes[i]) : 0 );
					fields += !sizes[i];
					sizes[i] += 1;
					changed = 1;
				}
			}
		}

		if (first)
		{
			first = 0;
			changed = 1;
		}
	} while (changed);

	x = 0;
	first = 1;
	width -= headspace * 2;
	width -= (fields > 1) ? fields - 1 : 0;
	if (fields >= 2)
	{
		endspace = headspace;
		interspace1  = (plScrWidth - width - headspace - endspace) / (fields - 1);
		interspace2 = (plScrWidth - width - headspace - endspace) % (fields - 1);
	} else {
		endspace = plScrWidth - width - headspace;
		interspace1 = interspace2 = 0;
	}

	displayvoid (lineno, x, headspace); x += headspace;
	for (i=0; i < count; i++)
	{
		if (sizes[i])
		{
			if (first) { first = 0; } else { displayvoid (3, x, interspace1 + (!!interspace2)); x += interspace1 + (!!interspace2); if (interspace2) interspace2--; }
			Elements[i]->render (inputa[i], inputb[i], inputc[i], sizes[i], &x, lineno);
		}
	}

	displayvoid (lineno, x, endspace);
}

void mcpDrawGStringsFixedLengthStream (const char                    *filename8_3,
                                       const char                    *filename16_3,
                                       const uint64_t                 pos,
                                       const uint64_t                 size, /* can be smaller than the file-size due to meta-data */
                                       const char                     sizesuffix, /* 0 = "" (MIDI), 1 = KB */
                                       const char                    *opt25,
                                       const char                    *opt50,
                                       const int_fast16_t             kbs,  /* kilo-bit-per-second */
                                       const uint_fast8_t             inpause,
                                       const uint_fast16_t            seconds,
                                       const struct moduleinfostruct *mdbdata
)
{
	const struct GStringElement *Elements1[7] = {&GString_pos, &GString_bitrate, &GString_title, &GString_comment, &GString_album, &GString_date, &GString_playtime};
	const struct GStringElement *Elements2[6] = {&GString_filename, &GString_composer, &GString_artist, &GString_style, &GString_option, &GString_pausetime};

	int sizes1[7];
	int sizes2[6];

	const void *sizeinputa1[7];
	const void *sizeinputb1[7];
	const void *sizeinputc1[7];
	const void *sizeinputa2[6];
	const void *sizeinputb2[6];
	const void *sizeinputc2[6];

	sizeinputa1[0] = &pos;
	sizeinputb1[0] = &size;
	sizeinputc1[0] = &sizesuffix;

	sizeinputa1[1] = &kbs;
	sizeinputb1[1] = 0;
	sizeinputc1[1] = 0;

	sizeinputa1[2] = mdbdata->title;
	sizeinputb1[2] = (void *)(long)measurestr_utf8 (mdbdata->title, strlen (mdbdata->title));
	sizeinputc1[2] = 0;

	sizeinputa1[3] = mdbdata->comment;
	sizeinputb1[3] = (void *)(long)measurestr_utf8 (mdbdata->comment, strlen (mdbdata->comment));
	sizeinputc1[3] = 0;

	sizeinputa1[4] = mdbdata->album;
	sizeinputb1[4] = (void *)(long)measurestr_utf8 (mdbdata->album, strlen (mdbdata->album));
	sizeinputc1[4] = 0;

	sizeinputa1[5] = &mdbdata->date;
	sizeinputb1[5] = 0;
	sizeinputc1[5] = 0;

	sizeinputa1[6] = &mdbdata->playtime;
	sizeinputb1[6] = 0;
	sizeinputc1[6] = 0;

	sizeinputa2[0] = filename8_3;
	sizeinputb2[0] = filename16_3;
	sizeinputc2[0] = 0;

	sizeinputa2[1] = mdbdata->composer;
	sizeinputb2[1] = (void *)(long)measurestr_utf8 (mdbdata->composer, strlen (mdbdata->composer));
	sizeinputc2[1] = 0;

	sizeinputa2[2] = mdbdata->artist;
	sizeinputb2[2] = (void *)(long)measurestr_utf8 (mdbdata->artist, strlen (mdbdata->artist));
	sizeinputc2[2] = 0;

	sizeinputa2[3] = mdbdata->style;
	sizeinputb2[3] = (void *)(long)measurestr_utf8 (mdbdata->style, strlen (mdbdata->style));
	sizeinputc2[3] = 0;

	sizeinputa2[4] = opt25;
	sizeinputb2[4] = opt50;
	sizeinputc2[4] = 0;

	sizeinputa2[5] = &inpause;
	sizeinputb2[5] = &seconds;
	sizeinputc2[5] = 0;

	GStrings_render (2, 7, Elements1, sizes1, sizeinputa1, sizeinputb1, sizeinputc1);
	GStrings_render (3, 6, Elements2, sizes2, sizeinputa2, sizeinputb2, sizeinputc2);
}

void mcpDrawGStringsSongXofY (const char                    *filename8_3,
                              const char                    *filename16_3,
                              const int                      songX,
                              const int                      songY,
                              const uint_fast8_t             inpause,
                              const uint_fast16_t            seconds,
                              const struct moduleinfostruct *mdbdata)
{
	const struct GStringElement *Elements1[6] = {&GString_song_x_y, &GString_title, &GString_comment, &GString_album, &GString_date, &GString_playtime};
	const struct GStringElement *Elements2[5] = {&GString_filename, &GString_composer, &GString_artist, &GString_style, &GString_pausetime};

	int sizes1[6];
	int sizes2[5];

	const void *sizeinputa1[6];
	const void *sizeinputb1[6];
	const void *sizeinputc1[6];
	const void *sizeinputa2[5];
	const void *sizeinputb2[5];
	const void *sizeinputc2[5];

	sizeinputa1[0] = &songX;
	sizeinputb1[0] = &songY;
	sizeinputc1[0] = 0;

	sizeinputa1[1] = mdbdata->title;
	sizeinputb1[1] = (void *)(long)measurestr_utf8 (mdbdata->title, strlen (mdbdata->title));
	sizeinputc1[1] = 0;

	sizeinputa1[2] = mdbdata->comment;
	sizeinputb1[2] = (void *)(long)measurestr_utf8 (mdbdata->comment, strlen (mdbdata->comment));
	sizeinputc1[2] = 0;

	sizeinputa1[3] = mdbdata->album;
	sizeinputb1[3] = (void *)(long)measurestr_utf8 (mdbdata->album, strlen (mdbdata->album));
	sizeinputc1[3] = 0;

	sizeinputa1[4] = &mdbdata->date;
	sizeinputb1[4] = 0;
	sizeinputc1[4] = 0;

	sizeinputa1[5] = &mdbdata->playtime;
	sizeinputb1[5] = 0;
	sizeinputc1[5] = 0;

	sizeinputa2[0] = filename8_3;
	sizeinputb2[0] = filename16_3;
	sizeinputc2[0] = 0;

	sizeinputa2[1] = mdbdata->composer;
	sizeinputb2[1] = (void *)(long)measurestr_utf8 (mdbdata->composer, strlen (mdbdata->composer));
	sizeinputc2[1] = 0;

	sizeinputa2[2] = mdbdata->artist;
	sizeinputb2[2] = (void *)(long)measurestr_utf8 (mdbdata->artist, strlen (mdbdata->artist));
	sizeinputc2[2] = 0;

	sizeinputa2[3] = mdbdata->style;
	sizeinputb2[3] = (void *)(long)measurestr_utf8 (mdbdata->style, strlen (mdbdata->style));
	sizeinputc2[3] = 0;

	sizeinputa2[4] = &inpause;
	sizeinputb2[4] = &seconds;
	sizeinputc2[4] = 0;

	GStrings_render (2, 6, Elements1, sizes1, sizeinputa1, sizeinputb1, sizeinputc1);
	GStrings_render (3, 5, Elements2, sizes2, sizeinputa2, sizeinputb2, sizeinputc2);
}

void mcpDrawGStringsTracked (const char                    *filename8_3,
                             const char                    *filename16_3,
                             const int                      songX,
                             const int                      songY, /* 0 or smaller, disables this, else 2 digits.. */
                             const uint8_t                  rowX,
                             const uint8_t                  rowY, /* displayed as 2 hex digits */
                             const uint16_t                 orderX,
                             const uint16_t                 orderY, /* displayed as 1,2,3 or 4 hex digits, depending on this size */
                             const uint8_t                  speed, /* displayed as %3 (with no space prefix) decimal digits */
                             const uint8_t                  tempo, /* displayed as %3 decimal digits */
                             const int16_t                  gvol, /* -1 for disable, else 0x00..0xff */
                             const int                      gvol_slide_direction,
                             const uint8_t                  chanX,
                             const uint8_t                  chanY, /* set to zero to disable */
                             const int                      amplification, /* -1 for disable */
                             const char                    *filter, /* 3 character string if non-null */
                             const uint_fast8_t             inpause,
                             const uint_fast16_t            seconds,
                             const struct moduleinfostruct *mdbdata)
{
	const struct GStringElement *Elements1[10] = {&GString_song_x_y, &GString_row_x_y, &GString_order_x_y, &GString_speed, &GString_tempo, &GString_gvol, &GString_channels_x_y, &GString_comment, &GString_amplification, &GString_filter};
	const struct GStringElement *Elements2[6] = {&GString_filename, &GString_title, &GString_composer, &GString_artist, &GString_style, &GString_pausetime};

	int sizes1[10];
	int sizes2[6];

	const void *sizeinputa1[10];
	const void *sizeinputb1[10];
	const void *sizeinputc1[10];
	const void *sizeinputa2[6];
	const void *sizeinputb2[6];
	const void *sizeinputc2[6];

	sizeinputa1[0] = &songX;
	sizeinputb1[0] = &songY;
	sizeinputc1[0] = 0;

	sizeinputa1[1] = &rowX;
	sizeinputb1[1] = &rowY;
	sizeinputc1[1] = 0;

	sizeinputa1[2] = &orderX;
	sizeinputb1[2] = &orderY;
	sizeinputc1[2] = 0;

	sizeinputa1[3] = &speed;
	sizeinputb1[3] = 0;
	sizeinputc1[3] = 0;

	sizeinputa1[4] = &tempo;
	sizeinputb1[4] = 0;
	sizeinputc1[4] = 0;

	sizeinputa1[5] = &gvol;
	sizeinputb1[5] = &gvol_slide_direction;
	sizeinputc1[5] = 0;

	sizeinputa1[6] = &chanX;
	sizeinputb1[6] = &chanY;
	sizeinputc1[6] = 0;

	sizeinputa1[7] = mdbdata->comment;
	sizeinputb1[7] = (void *)(long)measurestr_utf8 (mdbdata->comment, strlen (mdbdata->comment));
	sizeinputc1[7] = 0;

	sizeinputa1[8] = &amplification;
	sizeinputb1[8] = 0;
	sizeinputc1[8] = 0;

	sizeinputa1[9] = filter;
	sizeinputb1[9] = 0;
	sizeinputc1[9] = 0;

	sizeinputa2[0] = filename8_3;
	sizeinputb2[0] = filename16_3;
	sizeinputc2[0] = 0;

	sizeinputa2[1] = mdbdata->title;
	sizeinputb2[1] = (void *)(long)measurestr_utf8 (mdbdata->title, strlen (mdbdata->title));
	sizeinputc2[1] = 0;

	sizeinputa2[2] = mdbdata->composer;
	sizeinputb2[2] = (void *)(long)measurestr_utf8 (mdbdata->composer, strlen (mdbdata->composer));
	sizeinputc2[2] = 0;

	sizeinputa2[3] = mdbdata->artist;
	sizeinputb2[3] = (void *)(long)measurestr_utf8 (mdbdata->artist, strlen (mdbdata->artist));
	sizeinputc2[3] = 0;

	sizeinputa2[4] = mdbdata->style;
	sizeinputb2[4] = (void *)(long)measurestr_utf8 (mdbdata->style, strlen (mdbdata->style));
	sizeinputc2[4] = 0;

	sizeinputa2[5] = &inpause;
	sizeinputb2[5] = &seconds;
	sizeinputc2[5] = 0;

	GStrings_render (2, 10, Elements1, sizes1, sizeinputa1, sizeinputb1, sizeinputc1);
	GStrings_render (3,  6, Elements2, sizes2, sizeinputa2, sizeinputb2, sizeinputc2);
}

void cpiDrawGStrings (void)
{
	make_title (curplayer ? curplayer->playername : "", plEscTick);

	if (plDrawGStrings)
		plDrawGStrings();
	else {
		displayvoid (1, 0, plScrWidth);
		displayvoid (2, 0, plScrWidth);
		displayvoid (3, 0, plScrWidth);
	}

	if (plScrMode<100)
	{
		int chann;
		int chan0;
		int i;
		int limit=plScrWidth-(80-32);
		int offset;
		char temp[16];

		displaystr (4, 0, 0x08, " \xc4 \xc4\xc4 \xc4\xc4\xc4 \xc4\xc4\xc4\xc4\xc4\xc4\xc4  x  ", 22);
		displaychr (4, 22, 0x08, '\xc4', plScrWidth - 22 - 10);
		displaystr (4, plScrWidth - 10, 0x08, " \xc4\xc4\xc4 \xc4\xc4 \xc4 ", 10);

		snprintf (temp, sizeof (temp), " %d", plScrWidth);
		displaystr (4, 19 - strlen (temp), 0x08, temp, strlen (temp));

		snprintf (temp, sizeof (temp), "%d ", plScrHeight);
		displaystr (4, 20, 0x08, temp, strlen (temp));

		if (limit<2)
			limit=2;

		chann=plNLChan;
		if (chann>limit)
			chann=limit;
		chan0=plSelCh-(chann/2);
		if ((chan0+chann)>=plNLChan)
			chan0=plNLChan-chann;
		if (chan0<0)
			chan0=0;

		offset=plScrWidth/2-chann/2;

		for (i=0; i<chann; i++)
		{
			unsigned char chr;
			unsigned char col = 0;

			if (plMuteCh[i+chan0]&&((i+chan0)!=plSelCh))
			{
				chr = 0xc4;
				col = 0x08;
			} else {
				chr = '0'+(i+chan0+1)%10;
				if (plMuteCh[i+chan0])
				{
					col |= 0x80;
				} else {
					if ((i+chan0)!=plSelCh)
					{
						col |= 0x08;
					} else {
						col |= 0x07;
					}
				}
			}
			displaychr (4, offset + i + ((i + chan0) >= plSelCh), col, chr, 1);
			if ((i+chan0)==plSelCh)
			{
				displaychr (4, offset + i, col, '0' + (i + chan0 + 1)/10, 1);
			}
		}
		if (chann)
		{
			displaychr (4, offset - 1, 0x08, chan0 ? 0x1b : 0x04, 1);
			displaychr (4, offset + 1 + chann, 0x08, ((chan0+chann)!=plNLChan) ? 0x1a : 0x04, 1);
		}
	} else {
		if (plChanChanged)
		{
			int chann;
			int chan0;
			int i;
			int limit=plScrWidth-(80-32);
			/* int offset; */

			if (limit<2)
				limit=2;

			chann=plNLChan;
			if (chann>limit)
				chann=limit;
			chan0=plSelCh-(chann/2);
			if ((chan0+chann)>=plNLChan)
				chan0=plNLChan-chann;
			if (chan0<0)
				chan0=0;

			/* offset=plScrWidth/2-chann/2; */

			for (i=0; i<chann; i++)
			{ /* needs tuning... TODO */
				gdrawchar8(384+i*8, 64, '0'+(i+chan0+1)/10, plMuteCh[i+chan0]?8:7, 0);
				gdrawchar8(384+i*8, 72, '0'+(i+chan0+1)%10, plMuteCh[i+chan0]?8:7, 0);
				gdrawchar8(384+i*8, 80, ((i+chan0)==plSelCh)?0x18:((i==0)&&chan0)?0x1B:((i==(chann-1))&&((chan0+chann)!=plNLChan))?0x1A:' ', 15, 0);
			}
		}
	}
}

void cpiResetScreen(void)
{
	if (curmode)
		curmode->SetMode();
}

static void cpiChangeMode(struct cpimoderegstruct *m)
{
	if (curmode)
		if (curmode->Event)
			curmode->Event(cpievClose);
	if (!m)
		m=&cpiModeText;
	curmode=m;
	if (m->Event) /* do not relay on parseing from left in if's  - Stian*/
		if (!m->Event(cpievOpen))
		curmode=&cpiModeText;
	curmode->SetMode();
}

void cpiGetMode(char *hand)
{
	strcpy(hand, curmode->handle);
}

void cpiSetMode(const char *hand)
{
	struct cpimoderegstruct *mod;
	for (mod=cpiModes; mod; mod=mod->next)
		if (!strcasecmp(mod->handle, hand))
			break;
	cpiChangeMode(mod);
}

void cpiRegisterMode(struct cpimoderegstruct *m)
{
	if (m->Event)
		if (!m->Event(cpievInit))
			return;
	m->next=cpiModes;
	cpiModes=m;
}

void cpiUnregisterMode(struct cpimoderegstruct *m)
{
	if (cpiModes==m)
	{
		cpiModes=m->next;
		return;
	} else {
		struct cpimoderegstruct *p = cpiModes;
		while (p)
		{
			if (p->next==m)
			{
				p->next=m->next;
				return;
			}
			p=p->next;
		}
	}
}

void cpiRegisterDefMode(struct cpimoderegstruct *m)
{
	m->nextdef=cpiDefModes;
	cpiDefModes=m;
}

static void cpiVerifyDefModes(void)
{
	struct cpimoderegstruct *p;

	while (cpiDefModes)
	{
		if (cpiDefModes->Event&&!cpiDefModes->Event(cpievInitAll))
			cpiDefModes=cpiDefModes->nextdef;
		else
			break;
	}
	p = cpiDefModes;
	while (p)
	{
		if (p->nextdef)
		{
			if (p->nextdef->Event&&!p->nextdef->Event(cpievInitAll))
				p->nextdef=p->nextdef->nextdef;
			else
				p=p->nextdef;
		} else
			break;
	}
}

static void cpiInitAllModes(void)
{
	struct cpimoderegstruct *p;

	for (p=cpiModes;p;p=p->next)
		if (p->Event)
			p->Event(cpievInit);
}

void cpiUnregisterDefMode(struct cpimoderegstruct *m)
{
	if (cpiDefModes==m)
	{
		cpiDefModes=m->next;
		return;
	} else {
		struct cpimoderegstruct *p = cpiDefModes;
		while (p)
		{
			if (p->nextdef==m)
			{
				p->nextdef=m->nextdef;
				return;
			}
			p=p->nextdef;
		}
	}
}

static int plmpInited = 0;
static int plmpInit(void)
{
	plCompoMode=cfGetProfileBool2(cfScreenSec, "screen", "compomode", 0, 0);
	strncpy(curmodehandle, cfGetProfileString2(cfScreenSec, "screen", "startupmode", "text"), 8);
	curmodehandle[8]=0;

	mdbRegisterReadInfo(&cpiReadInfoReg);

	cpiRegisterDefMode(&cpiModeText);

	cpiVerifyDefModes();

	cpiInitAllModes();

	plRegisterInterface (&plOpenCP);

	plmpInited = 1;

	return errOk;
}

static void plmpClose(void)
{
	if (plmpInited)
	{
		plUnregisterInterface (&plOpenCP);
		mdbUnregisterReadInfo(&cpiReadInfoReg);
		plmpInited = 0;
	}

	while (cpiDefModes)
	{
		if (cpiDefModes->Event)
			cpiDefModes->Event(cpievDoneAll);
		cpiDefModes=cpiDefModes->nextdef;
	}

	if(plOpenCPPict)
	{
		free(plOpenCPPict);
		plOpenCPPict=NULL;
	}
}

static int linkhandle;

static int plmpOpenFile(struct moduleinfostruct *info, struct ocpfilehandle_t *fi, const struct interfaceparameters *ip)
{
	struct cpimoderegstruct *mod;
	void *fp;
	int retval;

	cpiModes=0;

	plEscTick=0;
	plPause=0;

	plNLChan=0;
	plNPChan=0;
	plSetMute=0;
	plIsEnd=0;
	plIdle=0;
	plGetMasterSample=0;
	plGetRealMasterVolume=0;
	plGetLChanSample=0;
	plGetPChanSample=0;

	linkhandle=lnkLink(ip->pllink);
	if (linkhandle<0)
	{
		fprintf(stderr, "Error finding plugin (pllink) %s\n", ip->pllink);
		return 0;
	}

	fp=lnkGetSymbol(linkhandle, ip->player);
	if (!fp)
	{
		lnkFree(linkhandle);
		fprintf(stderr, "Error finding symbol (player) %s from plugin %s\n", ip->player, ip->pllink);
		fprintf(stderr, "link error\n");
		sleep(1);
		return 0;
	}

	curplayer=(struct cpifaceplayerstruct*)fp;

	retval=curplayer->OpenFile(info, fi, ip->ldlink, ip->loader);

	if (retval)
	{
		lnkFree(linkhandle);
		fprintf(stderr, "error: %s\n", errGetShortString(retval));
		sleep(1);
		return 0;
	}

	for (mod=cpiDefModes; mod; mod=mod->nextdef)
		cpiRegisterMode(mod);
	for (mod=cpiModes; mod; mod=mod->next)
		if (!strcasecmp(mod->handle, curmodehandle))
			break;
	curmode=mod;

	soloch=-1;
	memset(plMuteCh, 0, sizeof(plMuteCh));
	plSelCh=0;

	return 1;
}

static void plmpCloseFile()
{
	cpiGetMode(curmodehandle);
	curplayer->CloseFile();
	while (cpiModes)
	{
		if (cpiModes->Event)
			cpiModes->Event(cpievDone);
		cpiModes=cpiModes->next;
	}
	lnkFree(linkhandle);
}

static void plmpOpenScreen()
{
	if (!curmode)
		curmode=&cpiModeText;
	if (curmode->Event&&!curmode->Event(cpievOpen))
		curmode=&cpiModeText;
	curmode->SetMode();
}


static void plmpCloseScreen()
{
	if (curmode->Event)
		curmode->Event(cpievClose);
/*
  cpimoderegstruct *mod;
  for (mod=cpiModes; mod; mod=mod->next)
    if (mod->Event)
      mod->Event(cpievClose);
*/
}

static int cpiChanProcessKey(uint16_t key)
{
	int i;
	switch (key)
	{
		/*case 0x4b00: //left*/
		case KEY_ALT_K:
			cpiKeyHelp(KEY_LEFT, "Select previous channel");
			cpiKeyHelp(KEY_UP, "Select next channel (and wrap)");
			cpiKeyHelp(KEY_RIGHT, "Select next channel");
			cpiKeyHelp(KEY_DOWN, "Select previous channel (and wrap)");
			cpiKeyHelp('1', "Select and toggle channel 1 on/off");
			cpiKeyHelp('2', "Select and toggle channel 2 on/off");
			cpiKeyHelp('3', "Select and toggle channel 3 on/off");
			cpiKeyHelp('4', "Select and toggle channel 4 on/off");
			cpiKeyHelp('5', "Select and toggle channel 5 on/off");
			cpiKeyHelp('6', "Select and toggle channel 6 on/off");
			cpiKeyHelp('7', "Select and toggle channel 7 on/off");
			cpiKeyHelp('8', "Select and toggle channel 8 on/off");
			cpiKeyHelp('9', "Select and toggle channel 9 on/off");
			cpiKeyHelp('0', "Select and toggle channel 10 on/off");
			cpiKeyHelp('Q', "Toggle selected channel on/off");
			cpiKeyHelp('q', "Toggle selected channel on/off");
			cpiKeyHelp('s', "Toggle solo on selected channel on/off");
			cpiKeyHelp('S', "Toggle solo on selected channel on/off");
			cpiKeyHelp(KEY_CTRL_S, "Enable all channels");
			cpiKeyHelp(KEY_CTRL_Q, "Enable all channels");
			return 0;
		case KEY_LEFT:
			if (plSelCh)
			{
				plSelCh--;
				plChanChanged=1;
			}
			break;
		/*case 0x4800: //up*/
		case KEY_UP:
			plSelCh=(plSelCh-1+plNLChan)%plNLChan;
			plChanChanged=1;
			break;
		/*case 0x4d00: //right*/
		case KEY_RIGHT:
			if ((plSelCh+1)<plNLChan)
			{
				plSelCh++;
				plChanChanged=1;
			}
			break;
		/*case 0x5000: //down*/
		case KEY_DOWN:
			plSelCh=(plSelCh+1)%plNLChan;
			plChanChanged=1;
			break;


		case '1': case '2': case '3': case '4': case '5':
		case '6': case '7': case '8': case '9': case '0':
/*TODO-keys
	  case 0x7800: case 0x7900: case 0x7A00: case 0x7B00: case 0x7C00:
	  case 0x7D00: case 0x7E00: case 0x7F00: case 0x8000: case 0x8100:*/
			if (key=='0')
				key=9;
			else
/*
				if (key<='9')*/
					key-='1';
/*
				else
					key=(key>>8)-0x78+10;*/
			if (key>=plNLChan)
				break;
			plSelCh=key;

		case 'q': case 'Q':
			plMuteCh[plSelCh]=!plMuteCh[plSelCh];
			plSetMute(plSelCh, plMuteCh[plSelCh]);
			plChanChanged=1;
			break;

		case 's': case 'S':
			if (plSelCh==soloch)
			{
				for (i=0; i<plNLChan; i++)
				{
					plMuteCh[i]=0;
					plSetMute(i, plMuteCh[i]);
				}
				soloch=-1;
			} else {
				for (i=0; i<plNLChan; i++)
				{
					plMuteCh[i]=i!=plSelCh;
					plSetMute(i, plMuteCh[i]);
				}
				soloch=plSelCh;
			}
			plChanChanged=1;
			break;

		case KEY_CTRL_Q: case KEY_CTRL_S: /* TODO-keys*/
			for (i=0; i<plNLChan; i++)
			{
				plMuteCh[i]=0;
				plSetMute(i, plMuteCh[i]);
			}
			soloch=-1;
			plChanChanged=1;
			break;
		default:
			return 0;
	}
	return 1;
}

void cpiForwardIProcessKey(uint16_t key)
{
	struct cpimoderegstruct *mod;

	for (mod=cpiModes; mod; mod=mod->next)
	{
#ifdef KEYBOARD_DEBUG
		fprintf (stderr, "cpiForwardIProcessKey: mod[%s]->IProcessKey()\n", mod->handle);
#endif
		mod->IProcessKey(key);
	}
}

static interfaceReturnEnum plmpDrawScreen(void)
{
	int needdraw = 1;
	struct cpimoderegstruct *mod;
	static int plInKeyboardHelp = 0;

	plChanChanged=0;

	if (plIsEnd)
	{
		if (plIsEnd())
		{
			plInKeyboardHelp = 0;
			return interfaceReturnNextAuto;
		}
	}

	if (plIdle)
	{
		plIdle();
	}

	for (mod=cpiModes; mod; mod=mod->next)
		mod->Event(cpievKeepalive);

	if (plEscTick&&(dos_clock()>(time_t)(plEscTick+2*DOS_CLK_TCK)))
		plEscTick=0;

	if (plInKeyboardHelp)
	{
		curmode->Draw();
		plInKeyboardHelp = cpiKeyHelpDisplay();
		if (!plInKeyboardHelp)
		{
			curmode->SetMode(); /* force complete redraw */
		} else {
			framelock();
		}
		return interfaceReturnContinue;
	}

	while (ekbhit())
	{
		uint16_t key=egetch();

		if (plEscTick)
		{
			plEscTick=0;
			if (key==KEY_ESC)
			{
				return interfaceReturnQuit;
			}
		}

#ifdef DEBUG
		DEBUGINT(key);
#endif

		if (key == KEY_ALT_K)
		{
			cpiKeyHelpClear();
		}

		if (curmode->AProcessKey(key))
		{
#ifdef KEYBOARD_DEBUG
			fprintf (stderr, "plmpDrawScreen: curmode[%s]->AProcessKey() swallowed the key\n", curmode->handle);
#endif
			needdraw = 1;
			continue;
		}

		switch (key)
		{
			struct cpimoderegstruct *mod;
			case KEY_ESC:
				plEscTick=dos_clock();
				break;
			case _KEY_ENTER:
				return interfaceReturnNextManuel;
			case 'f': case 'F':
			case KEY_INSERT:
			/* case 0x5200: //insert */
				return interfaceReturnCallFs;
			case 'd': case 'D': case KEY_CTRL_D:
			/* case 0x6f00: // alt-f8 TODO-keys*/
				return interfaceReturnDosShell;
			case KEY_CTRL_J:
				return interfaceReturnPrevManuel;
			case KEY_CTRL_K:
				return interfaceReturnNextManuel;
			case KEY_CTRL_L:
				fsLoopMods=!fsLoopMods;
				break;
			case KEY_ALT_C:
				fsSetup();
				plSetTextMode(fsScrType);
				fsScrType=plScrType;
				curmode->SetMode();
				break;
			#if 0
			TODO plLoopPatterns
			case KEY_ALT_L:
				plLoopPatterns=!plLoopPatterns;
			#ifdef DEBUG
				if(plLoopPatterns)
					DEBUGSTR("pattern loop enabled");
				else
					DEBUGSTR("pattern loop disabled");
			#endif
			#endif
			case KEY_ALT_K:
				cpiKeyHelp(KEY_ESC, "Exit");
				cpiKeyHelp(_KEY_ENTER, "Next song");
				cpiKeyHelp(KEY_INSERT, "Open file selected");
				cpiKeyHelp('f', "Open file selector");
				cpiKeyHelp('F', "Open file selector");
				cpiKeyHelp('d', "Open a shell");
				cpiKeyHelp('D', "Open a shell");
				cpiKeyHelp(KEY_CTRL_D, "Open a shell");
				cpiKeyHelp(KEY_CTRL_J, "Prev song (forced)");
				cpiKeyHelp(KEY_CTRL_K, "Next song (forced)");
				cpiKeyHelp(KEY_CTRL_L, "Toggle song looping (ALT-C setting)");
				cpiKeyHelp(KEY_ALT_C, "Open setup dialog");
				/* cpiKeyHelp(KEY_ALT_L, "Toggle plLoopPatterns"); */
				/*return 0;*/
#ifdef KEYBOARD_DEBUG
			fprintf (stderr, "plmpDrawScreen: ALT-K, dropping to the default path\n");
#endif
			default:
				for (mod=cpiModes; mod; mod=mod->next)
				{
#ifdef KEYBOARD_DEBUG
					fprintf (stderr, "plmpDrawScreen: mod[%s]->IProcessKey()\n", mod->handle);
#endif
					if (mod->IProcessKey(key))
					{
#ifdef KEYBOARD_DEBUG
						fprintf (stderr, "plmpDrawScreen:   key was swallowed\n");
#endif
						break;
					}
				}
				if (mod) break; /* forward the break */
				if (plNLChan)
				{
#ifdef KEYBOARD_DEBUG
					fprintf (stderr, "plmpDrawScreen: plNLChan!=0   =>   cpiChanProcessKey()\n");
#endif
					if (cpiChanProcessKey(key))
					{
#ifdef KEYBOARD_DEBUG
						fprintf (stderr, "plmpDrawScreen:   key was swallowed\n");
#endif
						break;
					}
				}
				if (plProcessKey)
				{
#ifdef KEYBOARD_DEBUG
					fprintf (stderr, "plmpDrawScreen: plProcessKey()\n");
#endif
					plProcessKey(key);
				}
				if (needdraw)
				{
					needdraw = 0;
					curmode->Draw();
				}
				if (key == KEY_ALT_K)
				{
					plInKeyboardHelp = 1;
					goto superbreak;
				}
				break;
		}

		needdraw=1;
	}

superbreak:
	if (needdraw)
	{
		curmode->Draw();
	}
	framelock();

	return interfaceReturnContinue;
}

static interfaceReturnEnum plmpCallBack(void)
{
	interfaceReturnEnum stop;

	plmpOpenScreen();
	stop=interfaceReturnContinue;
	while (!stop)
		stop=plmpDrawScreen();
	plmpCloseScreen();
	return stop;
}

char plNoteStr[132][4]=
{
	"c-1","c#1","d-1","d#1","e-1","f-1","f#1","g-1","g#1","a-1","a#1","b-1",
	"C-0","C#0","D-0","D#0","E-0","F-0","F#0","G-0","G#0","A-0","A#0","B-0",
	"C-1","C#1","D-1","D#1","E-1","F-1","F#1","G-1","G#1","A-1","A#1","B-1",
	"C-2","C#2","D-2","D#2","E-2","F-2","F#2","G-2","G#2","A-2","A#2","B-2",
	"C-3","C#3","D-3","D#3","E-3","F-3","F#3","G-3","G#3","A-3","A#3","B-3",
	"C-4","C#4","D-4","D#4","E-4","F-4","F#4","G-4","G#4","A-4","A#4","B-4",
	"C-5","C#5","D-5","D#5","E-5","F-5","F#5","G-5","G#5","A-5","A#5","B-5",
	"C-6","C#6","D-6","D#6","E-6","F-6","F#6","G-6","G#6","A-6","A#6","B-6",
	"C-7","C#7","D-7","D#7","E-7","F-7","F#7","G-7","G#7","A-7","A#7","B-7",
	"C-8","C#8","D-8","D#8","E-8","F-8","F#8","G-8","G#8","A-8","A#8","B-8",
	"C-9","C#9","D-9","D#9","E-9","F-9","F#9","G-9","G#9","A-9","A#9","B-9"
};

static struct interfacestruct plOpenCP = {plmpOpenFile, plmpCallBack, plmpCloseFile, "plOpenCP", NULL};

#ifndef SUPPORT_STATIC_PLUGINS
char *dllinfo = "";
#endif
DLLEXTINFO_PREFIX struct linkinfostruct dllextinfo = {.name = "cpiface", .desc = "OpenCP Interface (c) 1994-'22 Niklas Beisert, Tammo Hinrichs, Stian Skjelstad", .ver = DLLVERSION, .size = 0, .LateInit = plmpInit, .PreClose = plmpClose};
/* OpenCP Module Player */
