/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'26 Stian Skjelstad <stian.skjelstad@gmail.com>
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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "types.h"
#include "boot/plinkman.h"
#include "boot/psetting.h"
#include "cpiface/cpiface.h"
#include "cpiface/cpiface-private.h"
#include "cpiface/cpipic.h"
#include "cpiface/cpiptype.h"
#include "cpiface/mcpedit.h"
#include "dev/mcp.h"
#include "dev/player.h"
#include "dev/ringbuffer.h"
#include "filesel/dirdb.h"
#include "filesel/filesystem.h"
#include "filesel/mdb.h"
#include "filesel/pfilesel.h"
#include "filesel/filesystem-unix.h"
#include "stuff/compat.h"
#include "stuff/err.h"
#include "stuff/imsrtns.h"
#include "stuff/framelock.h"
#include "stuff/latin1.h"
#include "stuff/piperun.h"
#include "stuff/poll.h"
#include "stuff/poutput.h"
#include "stuff/sets.h"
#include "stuff/utf-16.h"
#include <unistd.h>
#include <time.h>

OCP_INTERNAL struct cpifaceSessionPrivate_t cpifaceSessionAPI;

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(x, y) (((x) < (y)) ? (x) : (y))

static void mcpDrawGStringsFixedLengthStream (struct cpifaceSessionAPI_t *cpifaceSession,
                                              const uint64_t                 pos,
                                              const uint64_t                 size, /* can be smaller than the file-size due to meta-data */
                                              const char                     sizesuffix, /* 0 = "" (MIDI), 1 = KB */
                                              const char                    *opt25,
                                              const char                    *opt50,
                                              const int_fast16_t             kbs); /* kilo-bit-per-second */

static void mcpDrawGStringsSongXofY (struct cpifaceSessionAPI_t *cpifaceSession,
                                     const int                      songX,
                                     const int                      songY);

static void mcpDrawGStringsTracked (struct cpifaceSessionAPI_t *cpifaceSession,
                                    const int                      songX,
                                    const int                      songY,  /* 0 or smaller, disables this, else 2 digits.. */
                                    const uint8_t                  rowX,
                                    const uint8_t                  rowY,   /* displayed as 2 hex digits */
                                    const uint16_t                 orderX,
                                    const uint16_t                 orderY, /* displayed as 1,2,3 or 4 hex digits, depending on this size */
                                    const uint8_t                  speed,  /* displayed as %3 (with no space prefix) decimal digits */
                                    const uint8_t                  tempo,  /* displayed as %3 decimal digits */
                                    const int16_t                  gvol,   /* -1 for disable, else 0x00..0xff */
                                    const int                      gvol_slide_direction,
                                    const uint8_t                  chanX,
                                    const uint8_t                  chanY); /* set to zero to disable */

static struct drawHelperAPI_t drawHelperAPI =
{
	mcpDrawGStringsFixedLengthStream,
	mcpDrawGStringsSongXofY,
	mcpDrawGStringsTracked
};

extern struct cpimoderegstruct cpiModeText;

static const struct cpifaceplayerstruct *curplayer;

static signed char soloch=-1;

char plCompoMode;

static struct cpimoderegstruct *cpiModes;
static struct cpimoderegstruct *cpiDefModes;

time_t plEscTick;

static struct cpimoderegstruct *curmode;
static char curmodehandle[9];

static struct interfacestruct plOpenCP;

int cpiSetGraphMode(int big)
{
	if (plSetGraphMode(big) < 0)
	{
		return -1;
	}
	cpifaceSessionAPI.Public.SelectedChannelChanged = 1;
	return 0;
}

void cpiSetTextMode(int size)
{
	plSetTextMode(size);
	cpifaceSessionAPI.Public.SelectedChannelChanged = 1;
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

static void cpiDrawG1String (struct cpifaceSessionPrivate_t *f)
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

	int numspaces;
	int showfx;
	int showpan;

	if (plScrWidth >= 90)
	{
		endspace = headspace = (plScrWidth - 60) / 30;
	} else {
		endspace = headspace = 0;
	}

	/* increase the view-mode of each component until we can't grow them anymore */
#warning TODO, we can cache this as long a f->mcpset.viewfx and screenwidth has not changed!
	if ((f->mcpType & mcpNormalizeCanEcho) &&((
		(int)volumesizes[volumemode] + surroundsizes[surroundmode] + panningsizes[panningbalancemode] + balancesizes[panningbalancemode]
		                             + echosizes[echomode] + reverbsizes[reverbchorusmode] + chorussizes[reverbchorusmode]
		                             + speedpitchsizes[speedpitchmode] + headspace + endspace + 7
		) <= plScrWidth))
	{
		width = (int)volumesizes[volumemode] + surroundsizes[surroundmode] + panningsizes[panningbalancemode] + balancesizes[panningbalancemode]
			                             + echosizes[echomode] + reverbsizes[reverbchorusmode] + chorussizes[reverbchorusmode]
		                                     + speedpitchsizes[speedpitchmode] + headspace + endspace;
		numspaces = 7;
		showpan = 1;
		showfx = 1;
	} else if (f->mcpset.viewfx)
	{
		width = (int)volumesizes[volumemode] + echosizes[echomode] + reverbsizes[reverbchorusmode] + chorussizes[reverbchorusmode] + speedpitchsizes[speedpitchmode] + headspace + endspace;
		numspaces = 4;
		showpan = 0;
		showfx = 1;
	} else {
		width = (int)volumesizes[volumemode] + surroundsizes[surroundmode] + panningsizes[panningbalancemode] + balancesizes[panningbalancemode] + speedpitchsizes[speedpitchmode] + headspace + endspace;
		numspaces = 4;
		showpan = 1;
		showfx = 0;
	}

	if (showpan)
	{
		if (!surroundmode)
		{
			int n = width - surroundsizes[0] + surroundsizes[1];
			if ((n + numspaces) <= plScrWidth)
			{
				surroundmode = 1;
				width = n;
			}
		}
	}

	if (showfx)
	{
		if (!echomode)
		{
			int n = width - echosizes[0] + echosizes[1];
			if ((n + numspaces) <= plScrWidth)
			{
				echomode = 1;
				width = n;
			}
		}
	}

	if (!speedpitchmode)
	{
		int n = width - speedpitchsizes[0] + speedpitchsizes[1];
		if ((n + numspaces) <= plScrWidth)
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
			int n = width - volumesizes[volumemode] + volumesizes[volumemode+1];
			if ((n + numspaces) <= plScrWidth)
			{
				volumemode++;
				width = n;
				changed = 1;
			}
		}

		if (showpan)
		{
			if (panningbalancemode < 7)
			{
				int n = width - panningsizes[panningbalancemode] - balancesizes[panningbalancemode] + panningsizes[panningbalancemode+1] + balancesizes[panningbalancemode+1];
				if ((n + numspaces) <= plScrWidth)
				{
					panningbalancemode++;
					width = n;
					changed = 1;
				}
			}
		}

		if (showfx)
		{
			if (reverbchorusmode < 7)
			{
				int n = width - reverbsizes[reverbchorusmode] - chorussizes[reverbchorusmode] + reverbsizes[reverbchorusmode+1] + chorussizes[reverbchorusmode+1];
				if ((n + numspaces) <= plScrWidth)
				{
					reverbchorusmode++;
					width = n;
					changed = 1;
				}
			}
		}
	}

	interspace1 = (plScrWidth - width) / numspaces;
	interspace2 = (plScrWidth - width) % numspaces;

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
			case 0: va=(f->mcpset.vol+4)>>3; vi= 8-va; break;
			case 1: va=(f->mcpset.vol+2)>>2; vi=16-va; break;
			case 2: va=(f->mcpset.vol+1)>>1; vi=32-va; break;
			case 3: va= f->mcpset.vol      ; vi=64-va; break;
		}
		displaychr (1, x, 0x0f, '\xfe', va); x += va;
		displaychr (1, x, 0x09, '\xfa', vi); x += vi;
	}

	if (showpan)
	{
		displayvoid (1, x, interspace1 + (!!interspace2)); x += interspace1 + (!!interspace2);
		if (interspace2) interspace2--;

		if (surroundmode)
		{
			displaystr (1, x, 0x09, "surround: ", 10); x += 10;
		} else {
			displaystr (1, x, 0x09, "srnd: ",      5); x +=  5;
		}
		displaystr (1, x, 0x0f, f->mcpset.srnd?"x":"o", 1); x += 1;

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
				case 0: r = ((f->mcpset.pan+70)>>4); w =  8; temp = "l\xfa\xfa\xfam\xfa\xfa\xfar"; break;
				case 1: r = ((f->mcpset.pan+68)>>3); w = 16; temp = "l\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfar"; break;
				case 2: r = ((f->mcpset.pan+66)>>2); w = 32; temp = "l\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfar"; break;
				case 3: r = ((f->mcpset.pan+64)>>1); w = 64; temp = "l\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfar"; break;
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
				case 0: l = ((f->mcpset.bal+70)>>4); w =  8; temp = "l\xfa\xfa\xfam\xfa\xfa\xfar"; break;
				case 1: l = ((f->mcpset.bal+68)>>3); w = 16; temp = "l\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfar"; break;
				case 2: l = ((f->mcpset.bal+66)>>2); w = 32; temp = "l\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfar"; break;
				case 3: l = ((f->mcpset.bal+64)>>1); w = 64; temp = "l\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfar"; break;
			}
			displaystr (1, x, 0x07, temp, l); x += l;
			displaychr (1, x, 0x0f, 'I', 1); x += 1;
			displaystr (1, x, 0x07, temp + l + 1, w - l); x += w-l;
		}
	}

	if (showfx)
	{
		displayvoid (1, x, interspace1 + (!!interspace2)); x += interspace1 + (!!interspace2);
		if (interspace2) interspace2--;

		if (echomode)
		{
			displaystr (1, x, 0x09, "echoactive: ", 12); x += 12;
		} else {
			displaystr (1, x, 0x09, "echo: ",       5);  x +=  5;
		}
		displaystr (1, x, 0x0f, f->mcpset.useecho?"x":"o", 1); x += 1;

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
				case 0: l = ((f->mcpset.reverb+3)>>3); w =  8; temp = "-\xfa\xfa\xfam\xfa\xfa\xfa+"; break;
				case 1: l = ((f->mcpset.reverb+2)>>2); w = 16; temp = "-\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfa+"; break;
				case 2: l = ((f->mcpset.reverb+1)>>1); w = 32; temp = "-\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa+"; break;
				case 3: l = ((f->mcpset.reverb  )   ); w = 64; temp = "-\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa+"; break;
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
				case 0: l = ((f->mcpset.chorus+3)>>3); w =  8; temp = "-\xfa\xfa\xfam\xfa\xfa\xfa+"; break;
				case 1: l = ((f->mcpset.chorus+2)>>2); w = 16; temp = "-\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfa+"; break;
				case 2: l = ((f->mcpset.chorus+1)>>1); w = 32; temp = "-\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa+"; break;
				case 3: l = ((f->mcpset.chorus  )   ); w = 64; temp = "-\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfam\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa\xfa+"; break;
			}
			displaystr (1, x, 0x07, temp, l); x += l;
			displaychr (1, x, 0x0f, 'I', 1); x += 1;
			displaystr (1, x, 0x07, temp + l + 1, w - l); x += w - l;
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
		snprintf (temp, sizeof (temp), "%3d", saturate (f->mcpset.speed * 100 / 256, 0, 999));
		displaystr (1, x, 0x0f, temp, 3); x += 3;
		displaystr (1, x, 0x07, "% ", 2); x += 2;
		displaystr (1, x, 0x09, f->mcpset.splock?"\x1d ":"  ", 2); x+= 2;
		if (speedpitchmode)
		{
			displaystr (1, x, 0x09, "pitch: ", 7); x += 7;
		} else {
			displaystr (1, x, 0x09, "ptch: ", 6); x += 6;
		}
		snprintf (temp, sizeof (temp), "%3d", saturate (f->mcpset.pitch * 100 / 256, 0, 999));
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

	if ((*filesize)==0)
	{
		snprintf (b, 4, "NUL");
	} else {
		snprintf (b, 4, "%3d", (int)((*pos) * 100 / (*filesize)));
	}
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
	const uintptr_t len = (const uintptr_t)inputb;

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
	const uintptr_t len = (const uintptr_t)inputb;

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
	const uintptr_t len = (const uintptr_t)inputb;

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
	const uintptr_t len = (const uintptr_t)inputb;

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
	      uint_fast16_t  seconds = *(const uint_fast16_t *)inputb;
	char temp[7];

	switch (size)
	{
		case 1: displaystr (lineno, *x, (*inpause) ? 0x0c : 0x00, "paused ",           7); (*x) +=  7; break;
		case 2: displaystr (lineno, *x, (*inpause) ? 0x0c : 0x00, "playback paused ", 16); (*x) += 16; break;
	}
	displaystr (lineno, *x, 0x09, "time:", 5); (*x) += 5;

	if (seconds > 59999) seconds = 59999;
	snprintf(temp, 7, "%3d.%02d", (int)((seconds) / 60), (int)((seconds) % 60));
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

	memset (sizes, 0, sizeof (int) * count);

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

static uint_fast16_t getSeconds (struct cpifaceSessionAPI_t *cpifaceSession)
{
	struct cpifaceSessionPrivate_t *f = (struct cpifaceSessionPrivate_t *)cpifaceSession;
	uint64_t tail;
	uint32_t rate;

	cpifaceSession->plrDevAPI->GetStats (0, &tail);
	rate = cpifaceSession->plrDevAPI->GetRate();
	return (tail - f->SongStart) / rate;
}

static void cpiResetSongTimer (struct cpifaceSessionAPI_t *cpifaceSession)
{
	struct cpifaceSessionPrivate_t *f = (struct cpifaceSessionPrivate_t *)cpifaceSession;
	cpifaceSession->plrDevAPI->GetStats (0, &f->SongStart);
}

static void mcpDrawGStringsFixedLengthStream (struct cpifaceSessionAPI_t    *cpifaceSession,
                                              const uint64_t                 pos,
                                              const uint64_t                 size, /* can be smaller than the file-size due to meta-data */
                                              const char                     sizesuffix, /* 0 = "" (MIDI), 1 = KB */
                                              const char                    *opt25,
                                              const char                    *opt50,
                                              const int_fast16_t             kbs   /* kilo-bit-per-second */
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

	uint_fast16_t seconds = getSeconds (cpifaceSession);

	sizeinputa1[0] = &pos;
	sizeinputb1[0] = &size;
	sizeinputc1[0] = &sizesuffix;

	sizeinputa1[1] = &kbs;
	sizeinputb1[1] = 0;
	sizeinputc1[1] = 0;

	sizeinputa1[2] = cpifaceSession->mdbdata.title;
	sizeinputb1[2] = (void *)(uintptr_t)measurestr_utf8 (cpifaceSession->mdbdata.title, strlen (cpifaceSession->mdbdata.title));
	sizeinputc1[2] = 0;

	sizeinputa1[3] = cpifaceSession->mdbdata.comment;
	sizeinputb1[3] = (void *)(uintptr_t)measurestr_utf8 (cpifaceSession->mdbdata.comment, strlen (cpifaceSession->mdbdata.comment));
	sizeinputc1[3] = 0;

	sizeinputa1[4] = cpifaceSession->mdbdata.album;
	sizeinputb1[4] = (void *)(uintptr_t)measurestr_utf8 (cpifaceSession->mdbdata.album, strlen (cpifaceSession->mdbdata.album));
	sizeinputc1[4] = 0;

	sizeinputa1[5] = &cpifaceSession->mdbdata.date;
	sizeinputb1[5] = 0;
	sizeinputc1[5] = 0;

	sizeinputa1[6] = &cpifaceSession->mdbdata.playtime;
	sizeinputb1[6] = 0;
	sizeinputc1[6] = 0;

	sizeinputa2[0] = cpifaceSession->utf8_8_dot_3;
	sizeinputb2[0] = cpifaceSession->utf8_16_dot_3;
	sizeinputc2[0] = 0;

	sizeinputa2[1] = cpifaceSession->mdbdata.composer;
	sizeinputb2[1] = (void *)(uintptr_t)measurestr_utf8 (cpifaceSession->mdbdata.composer, strlen (cpifaceSession->mdbdata.composer));
	sizeinputc2[1] = 0;

	sizeinputa2[2] = cpifaceSession->mdbdata.artist;
	sizeinputb2[2] = (void *)(uintptr_t)measurestr_utf8 (cpifaceSession->mdbdata.artist, strlen (cpifaceSession->mdbdata.artist));
	sizeinputc2[2] = 0;

	sizeinputa2[3] = cpifaceSession->mdbdata.style;
	sizeinputb2[3] = (void *)(uintptr_t)measurestr_utf8 (cpifaceSession->mdbdata.style, strlen (cpifaceSession->mdbdata.style));
	sizeinputc2[3] = 0;

	sizeinputa2[4] = opt25;
	sizeinputb2[4] = opt50;
	sizeinputc2[4] = 0;

	sizeinputa2[5] = &cpifaceSession->InPause;
	sizeinputb2[5] = &seconds;
	sizeinputc2[5] = 0;

	GStrings_render (2, 7, Elements1, sizes1, sizeinputa1, sizeinputb1, sizeinputc1);
	GStrings_render (3, 6, Elements2, sizes2, sizeinputa2, sizeinputb2, sizeinputc2);
}

static void mcpDrawGStringsSongXofY (struct cpifaceSessionAPI_t    *cpifaceSession,
                                     const int                      songX,
                                     const int                      songY
)
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

	uint_fast16_t seconds = getSeconds (cpifaceSession);

	sizeinputa1[0] = &songX;
	sizeinputb1[0] = &songY;
	sizeinputc1[0] = 0;

	sizeinputa1[1] = cpifaceSession->mdbdata.title;
	sizeinputb1[1] = (void *)(uintptr_t)measurestr_utf8 (cpifaceSession->mdbdata.title, strlen (cpifaceSession->mdbdata.title));
	sizeinputc1[1] = 0;

	sizeinputa1[2] = cpifaceSession->mdbdata.comment;
	sizeinputb1[2] = (void *)(uintptr_t)measurestr_utf8 (cpifaceSession->mdbdata.comment, strlen (cpifaceSession->mdbdata.comment));
	sizeinputc1[2] = 0;

	sizeinputa1[3] = cpifaceSession->mdbdata.album;
	sizeinputb1[3] = (void *)(uintptr_t)measurestr_utf8 (cpifaceSession->mdbdata.album, strlen (cpifaceSession->mdbdata.album));
	sizeinputc1[3] = 0;

	sizeinputa1[4] = &cpifaceSession->mdbdata.date;
	sizeinputb1[4] = 0;
	sizeinputc1[4] = 0;

	sizeinputa1[5] = &cpifaceSession->mdbdata.playtime;
	sizeinputb1[5] = 0;
	sizeinputc1[5] = 0;

	sizeinputa2[0] = cpifaceSession->utf8_8_dot_3;
	sizeinputb2[0] = cpifaceSession->utf8_16_dot_3;
	sizeinputc2[0] = 0;

	sizeinputa2[1] = cpifaceSession->mdbdata.composer;
	sizeinputb2[1] = (void *)(uintptr_t)measurestr_utf8 (cpifaceSession->mdbdata.composer, strlen (cpifaceSession->mdbdata.composer));
	sizeinputc2[1] = 0;

	sizeinputa2[2] = cpifaceSession->mdbdata.artist;
	sizeinputb2[2] = (void *)(uintptr_t)measurestr_utf8 (cpifaceSession->mdbdata.artist, strlen (cpifaceSession->mdbdata.artist));
	sizeinputc2[2] = 0;

	sizeinputa2[3] = cpifaceSession->mdbdata.style;
	sizeinputb2[3] = (void *)(uintptr_t)measurestr_utf8 (cpifaceSession->mdbdata.style, strlen (cpifaceSession->mdbdata.style));
	sizeinputc2[3] = 0;

	sizeinputa2[4] = &cpifaceSession->InPause;
	sizeinputb2[4] = &seconds;
	sizeinputc2[4] = 0;

	GStrings_render (2, 6, Elements1, sizes1, sizeinputa1, sizeinputb1, sizeinputc1);
	GStrings_render (3, 5, Elements2, sizes2, sizeinputa2, sizeinputb2, sizeinputc2);
}

static void mcpDrawGStringsTracked (struct cpifaceSessionAPI_t    *cpifaceSession,
                                    const int                      songX,
                                    const int                      songY,  /* 0 or smaller, disables this, else 2 digits.. */
                                    const uint8_t                  rowX,
                                    const uint8_t                  rowY,   /* displayed as 2 hex digits */
                                    const uint16_t                 orderX,
                                    const uint16_t                 orderY, /* displayed as 1,2,3 or 4 hex digits, depending on this size */
                                    const uint8_t                  speed,  /* displayed as %3 (with no space prefix) decimal digits */
                                    const uint8_t                  tempo,  /* displayed as %3 decimal digits */
                                    const int16_t                  gvol,   /* -1 for disable, else 0x00..0xff */
                                    const int                      gvol_slide_direction,
                                    const uint8_t                  chanX,
                                    const uint8_t                  chanY   /* set to zero to disable */
)
{
	struct cpifaceSessionPrivate_t *f = (struct cpifaceSessionPrivate_t *)cpifaceSession;
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

	int amplification = -1;
	char *filter = 0;

	uint_fast16_t seconds = getSeconds (cpifaceSession);

	if (f->mcpType & mcpNormalizeCanAmplify)
	{
		amplification = f->mcpset.amp;
	}

	if (f->mcpType & mcpNormalizeFilterAOIFOI)
	{
		filter = (f->mcpset.filter==1) ? "AOI" : (f->mcpset.filter==2) ? "FOI" : "off";
	}

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

	sizeinputa1[7] = cpifaceSession->mdbdata.comment;
	sizeinputb1[7] = (void *)(uintptr_t)measurestr_utf8 (cpifaceSession->mdbdata.comment, strlen (cpifaceSession->mdbdata.comment));
	sizeinputc1[7] = 0;

	sizeinputa1[8] = &amplification;
	sizeinputb1[8] = 0;
	sizeinputc1[8] = 0;

	sizeinputa1[9] = filter;
	sizeinputb1[9] = 0;
	sizeinputc1[9] = 0;

	sizeinputa2[0] = cpifaceSession->utf8_8_dot_3;
	sizeinputb2[0] = cpifaceSession->utf8_16_dot_3;
	sizeinputc2[0] = 0;

	sizeinputa2[1] = cpifaceSession->mdbdata.title;
	sizeinputb2[1] = (void *)(uintptr_t)measurestr_utf8 (cpifaceSession->mdbdata.title, strlen (cpifaceSession->mdbdata.title));
	sizeinputc2[1] = 0;

	sizeinputa2[2] = cpifaceSession->mdbdata.composer;
	sizeinputb2[2] = (void *)(uintptr_t)measurestr_utf8 (cpifaceSession->mdbdata.composer, strlen (cpifaceSession->mdbdata.composer));
	sizeinputc2[2] = 0;

	sizeinputa2[3] = cpifaceSession->mdbdata.artist;
	sizeinputb2[3] = (void *)(uintptr_t)measurestr_utf8 (cpifaceSession->mdbdata.artist, strlen (cpifaceSession->mdbdata.artist));
	sizeinputc2[3] = 0;

	sizeinputa2[4] = cpifaceSession->mdbdata.style;
	sizeinputb2[4] = (void *)(uintptr_t)measurestr_utf8 (cpifaceSession->mdbdata.style, strlen (cpifaceSession->mdbdata.style));
	sizeinputc2[4] = 0;

	sizeinputa2[5] = &cpifaceSession->InPause;
	sizeinputb2[5] = &seconds;
	sizeinputc2[5] = 0;

	GStrings_render (2, 10, Elements1, sizes1, sizeinputa1, sizeinputb1, sizeinputc1);
	GStrings_render (3,  6, Elements2, sizes2, sizeinputa2, sizeinputb2, sizeinputc2);
}

void cpiDrawGStrings (struct cpifaceSessionAPI_t *cpifaceSession)
{
	struct cpifaceSessionPrivate_t *f = (struct cpifaceSessionPrivate_t *)cpifaceSession;

#if (CONSOLE_MIN_Y < 5)
# error cpiDrawGStrings() requires CONSOLE_MIN_Y >= 5
#endif

	make_title (curplayer ? curplayer->playername : "", plEscTick);

	cpiDrawG1String (f);

	if (f->Public.DrawGStrings)
	{
		f->Public.DrawGStrings (&f->Public);
	} else {
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

		chann = f->Public.LogicalChannelCount;
		if (chann>limit)
			chann=limit;
		chan0 = f->Public.SelectedChannel - (chann / 2);
		if ((chan0+chann) >= f->Public.LogicalChannelCount)
			chan0 = f->Public.LogicalChannelCount - chann;
		if (chan0<0)
			chan0 = 0;

		offset=plScrWidth/2-chann/2;

		for (i=0; i<chann; i++)
		{
			unsigned char chr;
			unsigned char col = 0;

			if (f->Public.MuteChannel[i+chan0]&&((i+chan0) != f->Public.SelectedChannel))
			{
				chr = 0xc4;
				col = 0x08;
			} else {
				chr = '0'+(i+chan0+1)%10;
				if (f->Public.MuteChannel[i+chan0])
				{
					col |= 0x80;
				} else {
					if ((i+chan0) != f->Public.SelectedChannel)
					{
						col |= 0x08;
					} else {
						col |= 0x07;
					}
				}
			}
			displaychr (4, offset + i + ((i + chan0) >= f->Public.SelectedChannel), col, chr, 1);
			if ((i + chan0) == f->Public.SelectedChannel)
			{
				displaychr (4, offset + i, col, '0' + (i + chan0 + 1)/10, 1);
			}
		}
		if (chann)
		{
			displaychr (4, offset - 1, 0x08, chan0 ? 0x1b : 0x04, 1);
			displaychr (4, offset + 1 + chann, 0x08, ((chan0+chann) != f->Public.LogicalChannelCount) ? 0x1a : 0x04, 1);
		}
	} else {
		if (f->Public.SelectedChannelChanged)
		{
			int chann;
			int chan0;
			int i;
			int limit=plScrWidth-(80-32);
			/* int offset; */

			if (limit<2)
				limit=2;

			chann = f->Public.LogicalChannelCount;;
			if (chann>limit)
				chann=limit;
			chan0 = f->Public.SelectedChannel - (chann / 2);
			if ((chan0+chann) >= f->Public.LogicalChannelCount)
				chan0 = f->Public.LogicalChannelCount - chann;
			if (chan0<0)
				chan0 = 0;

			/* offset=plScrWidth/2-chann/2; */

			for (i=0; i<chann; i++)
			{ /* needs tuning... TODO */
				Console.Driver->gDrawChar8x8 (384+i*8, 64, '0'+(i+chan0+1)/10, f->Public.MuteChannel[i+chan0]?8:7, 0);
				Console.Driver->gDrawChar8x8 (384+i*8, 72, '0'+(i+chan0+1)%10, f->Public.MuteChannel[i+chan0]?8:7, 0);
				Console.Driver->gDrawChar8x8 (384+i*8, 80, ((i+chan0)==f->Public.SelectedChannel)?0x18:((i==0)&&chan0)?0x1B:((i==(chann-1))&&((chan0+chann) != f->Public.LogicalChannelCount))?0x1A:' ', 15, 0);
			}
		}
	}
}

void cpiResetScreen(void)
{
	if (curmode)
		curmode->SetMode(&cpifaceSessionAPI.Public); /* we ignore errors here.... */
}

static void cpiChangeMode(struct cpimoderegstruct *m)
{
	if (curmode)
	{
		curmode->Event (&cpifaceSessionAPI.Public, cpievClose);
	}
	curmode = 0;

	if (m != &cpiModeText)
	{
		if (!m->Event (&cpifaceSessionAPI.Public, cpievOpen))
		{
			fprintf (stderr, "cpimode[%s]->Event(cpievOpen) failed\n", m->handle);
			goto text;
		}

		if (m->SetMode(&cpifaceSessionAPI.Public) < 0)
		{
			fprintf (stderr, "cpimode[%s]->SetMode() failed\n", m->handle);
			m->Event (&cpifaceSessionAPI.Public, cpievClose);
			goto text;
		}
		curmode = m;
		return;
	}

text:
	m = &cpiModeText;
	if (!m->Event (&cpifaceSessionAPI.Public, cpievOpen))
	{
		fprintf (stderr, "cpimode[%s]->Event(cpievOpen) failed\n", m->handle);
		return;
	}

	if (m->SetMode(&cpifaceSessionAPI.Public))
	{
		fprintf (stderr, "cpimode[%s]->SetMode() failed\n", m->handle);
		m->Event (&cpifaceSessionAPI.Public, cpievClose);

		return;
	}

	curmode = m;
}

void cpiGetMode(char *hand)
{
	if (curmode)
	{
		strcpy(hand, curmode->handle);
	} else {
		*hand = 0;
	}
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
	if (!m->Event (&cpifaceSessionAPI.Public, cpievInit))
	{
		return;
	}
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

static void cpiVerifyDefModes (void)
{
	struct cpimoderegstruct **iter, **next;

	for (iter = &cpiDefModes; iter && *iter; iter = next)
	{
		next = &(*iter)->nextdef;

		if (!(*iter)->Event (0, cpievInitAll)) /* no session is available yet */
		{
			*iter = *next; /* remove failed item from the linked list */
		}
	}
}

static void cpiInitAllModes(void)
{
	struct cpimoderegstruct *p;

	for (p=cpiModes; p; p=p->next)
	{
		p->Event (&cpifaceSessionAPI.Public, cpievInit);
	}
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

static int plmpInit (const struct configAPI_t *configAPI)
{
	fftInit ();
	cpiAnalInit ();
	cpiChanInit ();
	cpiGraphInit ();
	cpiInstInit ();
	cpiWurfel2Init (configAPI);
	cpiLinksInit ();
	cpiMVolInit ();
	cpiPhaseInit ();
	cpiScopeInit ();
	cpiTrackInit ();
	cpiVolCtrlInit ();

	return errOk;
}

static void plmpClose (void)
{
	cpiAnalDone ();
	cpiGraphDone ();
	cpiWurfel2Done ();
	cpiLinksDone ();
	cpiMVolDone ();
	cpiPhaseDone ();
	cpiScopeDone ();
	cpiVolCtrlDone ();
	plOpenCPPicDone ();
}

static int plmpInited = 0;
static int plmpLateInit(struct PluginInitAPI_t *API)
{
	plCompoMode=API->configAPI->GetProfileBool2(cfScreenSec, "screen", "compomode", 0, 0);
	strncpy(curmodehandle, API->configAPI->GetProfileString2(cfScreenSec, "screen", "startupmode", "text"), 8);
	curmodehandle[8]=0;

	mdbRegisterReadInfo(&cpiReadInfoReg);

	cpiRegisterDefMode(&cpiModeText);

	cpiVerifyDefModes ();

	cpiInitAllModes();

	plRegisterInterface (&plOpenCP);

	plmpInited = 1;

	return errOk;
}

static void plmpPreClose(struct PluginCloseAPI_t *API)
{
	if (plmpInited)
	{
		plUnregisterInterface (&plOpenCP);
		mdbUnregisterReadInfo(&cpiReadInfoReg);
		plmpInited = 0;
	}

	while (cpiDefModes)
	{
		cpiDefModes->Event (&cpifaceSessionAPI.Public, cpievDoneAll);
		cpiDefModes = cpiDefModes->nextdef;
	}

	if(plOpenCPPict)
	{
		free(plOpenCPPict);
		plOpenCPPict=NULL;
	}
}

static void cpifaceIdle (void)
{
	if (cpifaceSessionAPI.mcpPauseFadeDirection)
	{
		mcpDoPauseFade (&cpifaceSessionAPI.Public);
	}
	if (cpifaceSessionAPI.Public.IsEnd)
	{
		cpifaceSessionAPI.Public.IsEnd (&cpifaceSessionAPI.Public, fsLoopMods);
	}
}

static char NoteStr[134][4]=
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
	"C-9","C#9","D-9","D#9","E-9","F-9","F#9","G-9","G#9","A-9","A#9","B-9",
        "000","999"
};

static const char *plNoteStr(signed int note)
{
	if (note <   0) return NoteStr[132];
	if (note > 131) return NoteStr[133];
	return NoteStr[note];
};

static void cpiDebug (struct cpifaceSessionAPI_t *cpifaceSession, const char *fmt, ...)
{
	va_list va;
	struct cpifaceSessionPrivate_t *f = (struct cpifaceSessionPrivate_t *)cpifaceSession;

#warning we need to sort out ncurses, since stderr is sacred then later
	va_start (va, fmt);
	vfprintf (stderr, fmt, va);
	va_end (va);

	if (f->cpiDebug_buffill + 1 < sizeof (f->cpiDebug_bufbase))
	{
		va_start (va, fmt);
		vsnprintf (f->cpiDebug_bufbase + f->cpiDebug_buffill, sizeof (f->cpiDebug_bufbase) - 1 - f->cpiDebug_buffill, fmt, va);
		f->cpiDebug_buffill += strlen (f->cpiDebug_bufbase + f->cpiDebug_buffill);
		va_end (va);
	}
}

static int plmpOpenFile(struct moduleinfostruct *info, struct ocpfilehandle_t *fi, const struct cpifaceplayerstruct *cp)
{
	struct cpimoderegstruct *mod;
	const char *filename;

	memset (&cpifaceSessionAPI, 0, sizeof (cpifaceSessionAPI));
	cpifaceSessionAPI.Public.plrDevAPI = plrDevAPI;
	cpifaceSessionAPI.Public.ringbufferAPI = &ringbufferAPI;
	cpifaceSessionAPI.Public.mcpAPI = mcpAPI;
	cpifaceSessionAPI.Public.mcpDevAPI = mcpDevAPI;
	cpifaceSessionAPI.Public.drawHelperAPI = &drawHelperAPI;
	cpifaceSessionAPI.Public.configAPI = &configAPI;
	cpifaceSessionAPI.Public.console = &Console;
	cpifaceSessionAPI.Public.dirdb = &dirdbAPI;
	cpifaceSessionAPI.Public.PipeProcess = &PipeProcess;
#ifndef _WIN32
	cpifaceSessionAPI.Public.dmFile = dmFile;
#endif

	dirdbGetName_internalstr (fi->dirdb_ref, &filename);
	utf8_XdotY_name ( 8, 3, cpifaceSessionAPI.Public.utf8_8_dot_3 , filename);
	utf8_XdotY_name (16, 3, cpifaceSessionAPI.Public.utf8_16_dot_3, filename);
	cpifaceSessionAPI.Public.mdbdata = *info;

	cpifaceSessionAPI.Public.Normalize = mcpNormalize;
	cpifaceSessionAPI.Public.SetMasterPauseFadeParameters = mcpSetMasterPauseFadeParameters;
	cpifaceSessionAPI.Public.TogglePauseFade = mcpTogglePauseFade;
	cpifaceSessionAPI.Public.TogglePause = mcpTogglePause;
	cpifaceSessionAPI.Public.ResetSongTimer = cpiResetSongTimer;

	/*
	cpifaceSessionAPI.Public.GetRealMasterVolume = 0;
	cpifaceSessionAPI.Public.GetMasterSample = 0;
	cpifaceSessionAPI.Public.InPause = 0;
	cpifaceSessionAPI.Public.LogicalChannelCount = 0;
	cpifaceSessionAPI.Public.PhysicalChannelCount = 0;
	cpifaceSessionAPI.Public.DrawGStrings = 0;
	*/
	cpifaceSessionAPI.Public.UseChannels = plUseChannels;
	cpifaceSessionAPI.Public.UseDots = plUseDots;
	cpifaceSessionAPI.Public.UseInstruments = plUseInstruments;
	cpifaceSessionAPI.Public.UseMessage = plUseMessage;
	cpifaceSessionAPI.Public.TrackSetup = cpiTrkSetup;
	cpifaceSessionAPI.Public.TrackSetup2 = cpiTrkSetup2;

	cpifaceSessionAPI.Public.KeyHelp        = cpiKeyHelp;
	cpifaceSessionAPI.Public.KeyHelpClear   = cpiKeyHelpClear;
	cpifaceSessionAPI.Public.KeyHelpDisplay = cpiKeyHelpDisplay;

	/*
	cpifaceSessionAPI.Public.SetMuteChannel = 0;
	memset (cpifaceSessionAPI.Public.MuteChannel, 0, sizeof(cpifaceSessionAPI.Public.MuteChannel));

	cpifaceSessionAPI.Public.PanType=0;
	*/
	cpiModes=0;

	plEscTick = 0;

	/*
	cpifaceSessionAPI.Public.IsEnd = 0;
	cpifaceSessionAPI.Public.GetLChanSample = 0;
	cpifaceSessionAPI.Public.GetPChanSample = 0;
	cpifaceSessionAPI.Public.mcpGetChanSample = 0;
	cpifaceSessionAPI.Public.mcpMixChanSamples = 0;
	cpifaceSessionAPI.Public.mcpSet = 0;
	cpifaceSessionAPI.Public.mcpGet = 0;
	cpifaceSessionAPI.Public.mcpGetRealVolume = 0;
	*/
	cpifaceSessionAPI.Public.plNoteStr = plNoteStr;
	cpifaceSessionAPI.Public.cpiTextRegisterMode = cpiTextRegisterMode;
	cpifaceSessionAPI.Public.cpiTextUnregisterMode = cpiTextUnregisterMode;
	cpifaceSessionAPI.Public.cpiTextSetMode = cpiTextSetMode;
	cpifaceSessionAPI.Public.cpiTextRecalc = cpiTextRecalc;
	cpifaceSessionAPI.Public.latin1_f_to_utf8_z = latin1_f_to_utf8_z;
	cpifaceSessionAPI.Public.cpiDebug = cpiDebug;
#ifdef _WIN32
	cpifaceSessionAPI.Public.utf8_to_utf16_LFN = utf8_to_utf16_LFN;
	cpifaceSessionAPI.Public.utf16_to_utf8 = utf16_to_utf8;
#endif

	curplayer=cp;

	cpifaceSessionAPI.openStatus = curplayer->OpenFile (&cpifaceSessionAPI.Public, info, fi);
	if (cpifaceSessionAPI.openStatus)
	{
		cpifaceSessionAPI.Public.cpiDebug (&cpifaceSessionAPI.Public, "error: %s\n", errGetShortString(cpifaceSessionAPI.openStatus));
		if (cpifaceSessionAPI.openStatus == errPlay)
		{
			cpifaceSessionAPI.Public.cpiDebug (&cpifaceSessionAPI.Public, "Configuration of playback device driver is accessible in the setup: drive.\n");
		}
		curplayer->CloseFile (&cpifaceSessionAPI.Public);
		curplayer = 0;
		return 1;
	}

	pollInit (cpifaceIdle);

	for (mod=cpiDefModes; mod; mod=mod->nextdef)
		cpiRegisterMode(mod);
	for (mod=cpiModes; mod; mod=mod->next)
		if (!strcasecmp(mod->handle, curmodehandle))
			break;
	curmode=mod;

	soloch=-1;
	cpifaceSessionAPI.Public.SelectedChannel = 0;

	return 1;
}

static void plmpCloseFile (void)
{
	pollClose ();

	if (curplayer)
	{
		cpiGetMode (curmodehandle);
		curplayer->CloseFile (&cpifaceSessionAPI.Public);
		while (cpiModes)
		{
			cpiModes->Event (&cpifaceSessionAPI.Public, cpievDone);
			cpiModes = cpiModes->next;
		}
		curplayer = 0;
	}
}

static void plmpOpenScreen (void)
{
	cpifaceSessionAPI.Public.SelectedChannelChanged = 0; /* force redraw of selected channel */

	if (!curmode)
	{
		curmode=&cpiModeText;
	}
	if (!curmode->Event (&cpifaceSessionAPI.Public, cpievOpen))
	{
		curmode=&cpiModeText;
		curmode->Event (&cpifaceSessionAPI.Public, cpievOpen);
	}
	curmode->SetMode(&cpifaceSessionAPI.Public);
}


static void plmpCloseScreen (void)
{
	if (curmode)
	{
		curmode->Event (&cpifaceSessionAPI.Public, cpievClose);
	}
}

static int cpiChanProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
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
			if (cpifaceSession->SelectedChannel)
			{
				cpifaceSession->SelectedChannel--;
				cpifaceSession->SelectedChannelChanged = 1;
			}
			break;
		/*case 0x4800: //up*/
		case KEY_UP:
			cpifaceSession->SelectedChannel = (cpifaceSession->SelectedChannel - 1 + cpifaceSession->LogicalChannelCount) % cpifaceSession->LogicalChannelCount;
			cpifaceSession->SelectedChannelChanged = 1;
			break;
		/*case 0x4d00: //right*/
		case KEY_RIGHT:
			if ((cpifaceSession->SelectedChannel + 1) < cpifaceSession->LogicalChannelCount)
			{
				cpifaceSession->SelectedChannel++;
				cpifaceSession->SelectedChannelChanged = 1;
			}
			break;
		/*case 0x5000: //down*/
		case KEY_DOWN:
			cpifaceSession->SelectedChannel = (cpifaceSession->SelectedChannel + 1) % cpifaceSession->LogicalChannelCount;
			cpifaceSession->SelectedChannelChanged = 1;
			break;

		case '1': case '2': case '3': case '4': case '5':
		case '6': case '7': case '8': case '9': case '0':
			if (key=='0')
			{
				key=9;
			} else {
				key-='1';
			}
			if (key >= cpifaceSession->LogicalChannelCount)
				break;
			cpifaceSession->SelectedChannel = key;

		case 'q': case 'Q':
			cpifaceSession->SetMuteChannel (cpifaceSession, cpifaceSession->SelectedChannel, !cpifaceSession->MuteChannel[cpifaceSession->SelectedChannel]);
			cpifaceSession->SelectedChannelChanged = 1;
			break;

		case 's': case 'S':
			if (cpifaceSession->SelectedChannel == soloch)
			{
				for (i=0; i < cpifaceSession->LogicalChannelCount; i++)
				{
					cpifaceSession->SetMuteChannel (cpifaceSession, i, 0);
				}
				soloch=-1;
			} else {
				for (i=0; i < cpifaceSession->LogicalChannelCount; i++)
				{
					cpifaceSession->SetMuteChannel (cpifaceSession, i, i != cpifaceSession->SelectedChannel);
				}
				soloch = cpifaceSession->SelectedChannel;
			}
			cpifaceSession->SelectedChannelChanged = 1;
			break;

		case KEY_CTRL_Q: case KEY_CTRL_S: /* TODO-keys*/
			for (i=0; i < cpifaceSession->LogicalChannelCount; i++)
			{
				cpifaceSession->SetMuteChannel (cpifaceSession, i, 0);
			}
			soloch=-1;
			cpifaceSession->SelectedChannelChanged = 1;
			break;
		default:
			return 0;
	}
	return 1;
}

void cpiForwardIProcessKey (struct cpifaceSessionAPI_t *cpifaceSession, uint16_t key)
{
	struct cpimoderegstruct *mod;

	for (mod=cpiModes; mod; mod=mod->next)
	{
#ifdef KEYBOARD_DEBUG
		fprintf (stderr, "cpiForwardIProcessKey: mod[%s]->IProcessKey()\n", mod->handle);
#endif
		mod->IProcessKey (cpifaceSession, key);
	}
}

static interfaceReturnEnum plmpDrawScreen(void)
{
	struct cpimoderegstruct *mod;
	static int plInKeyboardHelp = 0;

	if (cpifaceSessionAPI.Public.IsEnd)
	{
		if (cpifaceSessionAPI.Public.IsEnd(&cpifaceSessionAPI.Public, fsLoopMods))
		{
			plInKeyboardHelp = 0;
			return interfaceReturnNextAuto;
		}
	}

	for (mod=cpiModes; mod; mod=mod->next)
	{
		mod->Event (&cpifaceSessionAPI.Public, cpievKeepalive);
	}

	if (plEscTick && (clock_ms() > (time_t)(plEscTick+2000) ) ) /* 2000 ms */
		plEscTick = 0;

	if (plInKeyboardHelp)
	{
		if (curmode)
		{
			curmode->Draw (&cpifaceSessionAPI.Public);
		}
		plInKeyboardHelp = cpiKeyHelpDisplay();
		if (!plInKeyboardHelp)
		{
			if (curmode)
			{
				curmode->SetMode(&cpifaceSessionAPI.Public); /* force complete redraw */
			}
		} else {
			framelock();
		}
		return interfaceReturnContinue;
	}

	while (Console.KeyboardHit())
	{
		uint16_t key = Console.KeyboardGetChar();

		if (plEscTick)
		{
			plEscTick=0;
			if ((key==KEY_ESC) || (key==KEY_EXIT))
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

		if (curmode && curmode->AProcessKey (&cpifaceSessionAPI.Public, key))
		{
#ifdef KEYBOARD_DEBUG
			fprintf (stderr, "plmpDrawScreen: curmode[%s]->AProcessKey() swallowed the key\n", curmode->handle);
#endif
			continue;
		}

		switch (key)
		{
			struct cpimoderegstruct *mod;
			case KEY_EXIT:
				return interfaceReturnQuit;
			case KEY_ESC:
				plEscTick = clock_ms();
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
				if (curmode)
				{
					curmode->SetMode(&cpifaceSessionAPI.Public);
				}
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
					if (mod->IProcessKey (&cpifaceSessionAPI.Public, key))
					{
#ifdef KEYBOARD_DEBUG
						fprintf (stderr, "plmpDrawScreen:   key was swallowed\n");
#endif
						break;
					}
				}
				if (mod) break; /* forward the break */
				if (cpifaceSessionAPI.Public.LogicalChannelCount)
				{
#ifdef KEYBOARD_DEBUG
					fprintf (stderr, "plmpDrawScreen: cpifaceSessionAPI.LogicalChannelCount!=0   =>   cpiChanProcessKey()\n");
#endif
					if (cpiChanProcessKey (&cpifaceSessionAPI.Public, key))
					{
#ifdef KEYBOARD_DEBUG
						fprintf (stderr, "plmpDrawScreen:   key was swallowed\n");
#endif
						break;
					}
				}
				if (cpifaceSessionAPI.Public.ProcessKey)
				{
#ifdef KEYBOARD_DEBUG
					fprintf (stderr, "plmpDrawScreen: cpifaceSessionAPI.Public.ProcessKey()\n");
#endif
					if (cpifaceSessionAPI.Public.ProcessKey (&cpifaceSessionAPI.Public, key))
					{
#ifdef KEYBOARD_DEBUG
						fprintf (stderr, "plmpDrawScreen:   key was swallowed\n");
#endif
						break;
					}
				}

#ifdef KEYBOARD_DEBUG
				fprintf (stderr, "plmpDrawScreen: mcpSetProcessKey()\n");
#endif
				if (mcpSetProcessKey (&cpifaceSessionAPI, key))
				{
#ifdef KEYBOARD_DEBUG
					fprintf (stderr, "plmpDrawScreen:   key was swallowed\n");
#endif
					break;
				}

				if (key == KEY_ALT_K)
				{
					plInKeyboardHelp = 1;
					goto superbreak;
				}
				break;
		}
	}

superbreak:
	if (curmode)
	{
		curmode->Draw(&cpifaceSessionAPI.Public);
	}
	framelock();

	cpifaceSessionAPI.Public.SelectedChannelChanged = 0;

	return interfaceReturnContinue;
}

static void cpiDebugRecalcLines (const int TargetWidth)
{
	char *iter = cpifaceSessionAPI.cpiDebug_bufbase;
	char *next = 0;
	int length;
	int linebreak = 0, linebreakold=0;
	cpifaceSessionAPI.cpiDebugLastWidth = TargetWidth - 2;
	cpifaceSessionAPI.cpiDebug_lines = 0;
	while ((*iter) && (cpifaceSessionAPI.cpiDebug_lines < (sizeof (cpifaceSessionAPI.cpiDebug_line) / sizeof (cpifaceSessionAPI.cpiDebug_line[0]))))
	{
		next = strchr (iter, '\n');
		if (next)
		{
			length = next - iter;
			if (length > cpifaceSessionAPI.cpiDebugLastWidth)
			{
				linebreak    = 1;
				linebreakold = 1;
				next = iter + cpifaceSessionAPI.cpiDebugLastWidth;
				length = cpifaceSessionAPI.cpiDebugLastWidth;
			} else {
				linebreak    = linebreakold;
				linebreakold = 0;
				next++;
			}
		} else {
			length = strlen (iter);
			next = iter + length;
		}

		cpifaceSessionAPI.cpiDebug_line[cpifaceSessionAPI.cpiDebug_lines].linebreak = linebreak;
		cpifaceSessionAPI.cpiDebug_line[cpifaceSessionAPI.cpiDebug_lines].offset    = iter - cpifaceSessionAPI.cpiDebug_bufbase;
		cpifaceSessionAPI.cpiDebug_line[cpifaceSessionAPI.cpiDebug_lines].length    = length;
		cpifaceSessionAPI.cpiDebug_lines++;
		iter = next;
	}
}

static void cpiDebugRun (void)
{
	int noexit = 1;
	int mlScroll = 0;

	plSetTextMode(plScrType);

	while (noexit)
	{
		int i;
		int mlWidth, mlHeight, mlLeft, mlTop;

		mlWidth = MIN (cpifaceSessionAPI.Public.console->TextWidth - 2, 122);

		/* We need to know the number of lines before we calculate mlHeight */
		if (cpifaceSessionAPI.cpiDebugLastWidth != (mlWidth - 2))
		{
			cpiDebugRecalcLines(mlWidth - 2);
		}

#if (CONSOLE_MIN_Y < 10)
# error cpiDebugRun() requires CONSOLE_MIN_Y >= 10
#endif

		mlHeight = MAX ( MIN ( cpifaceSessionAPI.Public.console->TextHeight - 2,
				       cpifaceSessionAPI.cpiDebug_lines + 2 ), 10);

		while (((mlScroll + mlHeight - 2) > cpifaceSessionAPI.cpiDebug_lines) && mlScroll)
		{
			mlScroll--;
		}

		mlLeft = (cpifaceSessionAPI.Public.console->TextWidth - mlWidth) / 2;

		mlTop = (cpifaceSessionAPI.Public.console->TextHeight - mlHeight) / 2;

		for (i=0; i < cpifaceSessionAPI.Public.console->TextHeight; i++)
		{
			if ((i < mlTop) || (i >= mlTop + mlHeight))
			{
				cpifaceSessionAPI.Public.console->Driver->DisplayVoid (i, 0, cpifaceSessionAPI.Public.console->TextWidth);
			} else {
				if (i == mlTop)
				{
					cpifaceSessionAPI.Public.console->DisplayPrintf (i, 0, 0x01, cpifaceSessionAPI.Public.console->TextWidth, "%*C \xc9%*C\xcd\xbb", mlLeft, mlWidth - 2);
				} else if (i == (mlTop + mlHeight - 1))
				{
					cpifaceSessionAPI.Public.console->DisplayPrintf (i, 0, 0x01, cpifaceSessionAPI.Public.console->TextWidth, "%*C \xc8%*C\xcd\xbc", mlLeft, mlWidth - 2);
				} else if ((i - mlTop + mlScroll - 1) >= cpifaceSessionAPI.cpiDebug_lines)
				{
					cpifaceSessionAPI.Public.console->DisplayPrintf (i, 0, 0x01, cpifaceSessionAPI.Public.console->TextWidth, "%*C \xba%*C \xba", mlLeft, mlWidth - 2);
				} else {
					cpifaceSessionAPI.Public.console->DisplayPrintf (i, 0, 0x01, cpifaceSessionAPI.Public.console->TextWidth, "%*C \xba%0.*o%*.*s%0.1o\xba",
							/* how many spaces to insert */
							mlLeft,
							/* color to use on the text */
							(((cpifaceSessionAPI.cpiDebug_bufbase + cpifaceSessionAPI.cpiDebug_line[i - mlTop + mlScroll - 1].offset)[0] == '[') ||
							 cpifaceSessionAPI.cpiDebug_line[i - mlTop + mlScroll - 1].linebreak ) ? 3 : 12,
							/* target text width */
							mlWidth - 2,
							/* source text width */
							cpifaceSessionAPI.cpiDebug_line[i - mlTop + mlScroll - 1].length,
							/* source text data */
							cpifaceSessionAPI.cpiDebug_bufbase + cpifaceSessionAPI.cpiDebug_line[i - mlTop + mlScroll - 1].offset
					);
				}
			}
		}
		framelock();
		while (Console.KeyboardHit())
		{
			uint16_t key = Console.KeyboardGetChar();
			if (key == KEY_UP)
			{
				if (mlScroll)
				{
					mlScroll--;
				}
			} else if (key == KEY_DOWN)
			{
				if (((mlScroll + 1 + mlHeight - 2) <= cpifaceSessionAPI.cpiDebug_lines))
				{
					mlScroll++;
				}
			} else if (
			    ((key >= 'A') && (key <= 'Z')) ||
			    ((key >= 'a') && (key <= 'z')) ||
			    ((key >= '0') && (key <= '9')) ||
			    (key == _KEY_ENTER)            ||
			    (key == ' ')                   ||
			    (key == KEY_ESC) )
			{
				noexit = 0;
			} else if (key == KEY_EXIT)
			{
				noexit = 0;
				break; /* KEY_EXIT is flooded into Console.KeyboardHit(), so we need to break the while() */
			}
		}
	}
}

static interfaceReturnEnum plmpCallBack(void)
{
	if (curplayer)
	{
		interfaceReturnEnum stop;
		plmpOpenScreen ();
		stop = interfaceReturnContinue;
		while (!stop)
		{
			stop = plmpDrawScreen ();
		}
		plmpCloseScreen ();
		return stop;
	} else {
		cpiDebugRun ();
		return interfaceReturnNextAuto;
	}
}

static struct interfacestruct plOpenCP = {plmpOpenFile, plmpCallBack, plmpCloseFile, "plOpenCP", NULL};

DLLEXTINFO_CORE_PREFIX struct linkinfostruct dllextinfo = {.name = "cpiface", .desc = "OpenCP Interface (c) 1994-'26 Niklas Beisert, Tammo Hinrichs, Stian Skjelstad", .ver = DLLVERSION, .sortindex = 35, .Init = plmpInit, .LateInit = plmpLateInit, .PreClose = plmpPreClose, .Close = plmpClose};
/* OpenCP Module Player */
