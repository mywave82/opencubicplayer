/* OpenCP Module Player
 * copyright (c) 1994-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 * copyright (c) 2004-'22 Stian Skjelstad <stian.skjelstad@gmail.com>
 *
 * CPIFace output routines / key handlers for the MCP system
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
 */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "dev/deviplay.h"
#include "dev/deviwave.h"
#include "dev/mcp.h"
#include "boot/psetting.h"
#include "stuff/poutput.h"
#include "stuff/sets.h"
#include "cpiface.h"

struct settings mcpset;

static int finespeed=8;

static enum mcpNormalizeType mcpType;

static int MasterPauseFadeParameter = 64;

void mcpNormalize(enum mcpNormalizeType Type)
{
	mcpType = Type;
	mcpset = set;
	MasterPauseFadeParameter = 64;

	if (!(mcpType & mcpNormalizeCanSpeedPitchUnlock))
	{
		mcpset.speed = mcpset.pitch;
		mcpset.splock = 1;
	}
	if (!(mcpType & mcpNormalizeCanEcho))
	{
		mcpset.viewfx = 0;
		//mcpset.useecho = 0;
	}

	mcpSet(-1, mcpMasterAmplify, 256*mcpset.amp);
	mcpSet(-1, mcpMasterVolume, mcpset.vol);
	mcpSet(-1, mcpMasterBalance, mcpset.bal);
	mcpSet(-1, mcpMasterPanning, mcpset.pan);
	mcpSet(-1, mcpMasterSurround, mcpset.srnd);
	mcpSet(-1, mcpMasterPitch, mcpset.pitch);
	mcpSet(-1, mcpMasterSpeed, mcpset.speed);
	mcpSet(-1, mcpMasterReverb, mcpset.reverb);
	mcpSet(-1, mcpMasterChorus, mcpset.chorus);
	if (mcpType & mcpNormalizeCanEcho)
	{
		mcpSet(-1, mcpMasterFilter, set.filter);
	} else {
		mcpSet(-1, mcpMasterFilter, 0);
	}
}

void mcpDrawGStrings (struct cpifaceSessionAPI_t *cpifaceSession)
{
	cpiDrawG1String (cpifaceSession, &mcpset);
}

int mcpSetProcessKey(uint16_t key)
{
	switch (key)
	{
		case KEY_ALT_K:
			cpiKeyHelp('-', "Decrease volume");
			cpiKeyHelp('+', "Increase volume");
			cpiKeyHelp('/', "Fade balance left");
			cpiKeyHelp('*', "Fade balance right");
			cpiKeyHelp(',', "Fade panning against normal");
			cpiKeyHelp('.', "Fade panning against reverse");
			cpiKeyHelp(KEY_F(2), "Decrease volume (faster)");
			cpiKeyHelp(KEY_F(3), "Increase volume (faster)");
			cpiKeyHelp(KEY_F(4), "Toggle surround on/off");
			cpiKeyHelp(KEY_F(5), "Fade balance left (faster)");
			cpiKeyHelp(KEY_F(6), "Fade balance right (faster)");
			cpiKeyHelp(KEY_F(7), "Fade panning against normal (faster)");
			cpiKeyHelp(KEY_F(8), "Fade panning against reverse (faster)");
			cpiKeyHelp(KEY_F(9), "Decrease speed (fine)");
			cpiKeyHelp(KEY_F(10), "Increase speed (fine)");
			cpiKeyHelp(KEY_F(11), "Decrease pitch (fine)");
			cpiKeyHelp(KEY_F(12), "Increase pitch (fine)");
			if (mcpType & mcpNormalizeCanSpeedPitchUnlock)
			{
				cpiKeyHelp(KEY_CTRL_F(12), "Toggle lock between pitch/speed");
				cpiKeyHelp('\\', "Toggle lock between pitch/speed");
			}
			cpiKeyHelp(KEY_CTRL_F(11), "Toggle between fine/course speed/pitch control");
			if (mcpType & mcpNormalizeCanAmplify)
			{
				cpiKeyHelp (KEY_SHIFT_F(2), "Decrease amplification");
				cpiKeyHelp (KEY_SHIFT_F(3), "Increase amplification");
			}
			cpiKeyHelp(KEY_CTRL_SHIFT_F(2), "`Save` the current configuration");
			cpiKeyHelp(KEY_CTRL_SHIFT_F(3), "`Load` configuration");
			cpiKeyHelp(KEY_CTRL_SHIFT_F(4), "`Reset` configuration");
			cpiKeyHelp(KEY_BACKSPACE, "Cycle mixer-filters");
			if (plrProcessKey)
				plrProcessKey(key);
			if (mcpProcessKey)
				mcpProcessKey(key);
			return 0;
		case '-':
			if (mcpset.vol>=2)
				mcpset.vol-=2;
			mcpSet(-1, mcpMasterVolume, mcpset.vol * MasterPauseFadeParameter / 64);
			break;
		case '+':
			if (mcpset.vol<=62)
				mcpset.vol+=2;
			mcpSet(-1, mcpMasterVolume, mcpset.vol * MasterPauseFadeParameter / 64);
			break;
		case '/':
			if ((mcpset.bal-=4)<-64)
				mcpset.bal=-64;
			mcpSet(-1, mcpMasterBalance, mcpset.bal);
			break;
		case '*':
			if ((mcpset.bal+=4)>64)
				mcpset.bal=64;
			mcpSet(-1, mcpMasterBalance, mcpset.bal);
			break;
		case ',':
			if ((mcpset.pan-=4)<-64)
				mcpset.pan=-64;
			mcpSet(-1, mcpMasterPanning, mcpset.pan);
			break;
		case '.':
			if ((mcpset.pan+=4)>64)
				mcpset.pan=64;
			mcpSet(-1, mcpMasterPanning, mcpset.pan);
			break;
		/*case 0x3c00: //f2*/
		case KEY_F(2):
			if ((mcpset.vol-=8)<0)
				mcpset.vol=0;
			mcpSet(-1, mcpMasterVolume, mcpset.vol * MasterPauseFadeParameter / 64);
			break;
		/*case 0x3d00: //f3*/
		case KEY_F(3):
			if ((mcpset.vol+=8)>64)
				mcpset.vol=64;
			mcpSet(-1, mcpMasterVolume, mcpset.vol * MasterPauseFadeParameter / 64);
			break;
		/*case 0x3e00: //f4*/
		case KEY_F(4):
			mcpSet(-1, mcpMasterSurround, mcpset.srnd=!mcpset.srnd);
			break;
		/*case 0x3f00: //f5*/
		case KEY_F(5):
			if ((mcpset.pan-=16)<-64)
				mcpset.pan=-64;
			mcpSet(-1, mcpMasterPanning, mcpset.pan);
			break;
		/*case 0x4000: //f6*/
		case KEY_F(6):
			if ((mcpset.pan+=16)>64)
				mcpset.pan=64;
			mcpSet(-1, mcpMasterPanning, mcpset.pan);
			break;
		/*case 0x4100: //f7*/
		case KEY_F(7):
			if ((mcpset.bal-=16)<-64)
				mcpset.bal=-64;
			mcpSet(-1, mcpMasterBalance, mcpset.bal);
			break;
		/*case 0x4200: //f8*/
		case KEY_F(8):
			if ((mcpset.bal+=16)>64)
				mcpset.bal=64;
			mcpSet(-1, mcpMasterBalance, mcpset.bal);
			break;
		/*case 0x4300: //f9*/
		case KEY_F(9):
			if ((mcpset.speed-=finespeed)<16)
				mcpset.speed=16;
			mcpSet(-1, mcpMasterSpeed, mcpset.speed * MasterPauseFadeParameter / 64);
			if (mcpset.splock)
				mcpSet(-1, mcpMasterPitch, (mcpset.pitch=mcpset.speed) * MasterPauseFadeParameter / 64);
			break;
		/*case 0x4400: //f10*/
		case KEY_F(10):
			if ((mcpset.speed+=finespeed)>2048)
				mcpset.speed=2048;
			mcpSet(-1, mcpMasterSpeed, mcpset.speed * MasterPauseFadeParameter / 64);
			if (mcpset.splock)
				mcpSet(-1, mcpMasterPitch, (mcpset.pitch=mcpset.speed) * MasterPauseFadeParameter / 64);
			break;
		/*case 0x8500: //f11*/
		case KEY_F(11):
			if ((mcpset.pitch-=finespeed)<16)
				mcpset.pitch=16;
			mcpSet(-1, mcpMasterPitch, (mcpset.pitch) * MasterPauseFadeParameter / 64);
			if (mcpset.splock)
				mcpSet(-1, mcpMasterSpeed, (mcpset.speed=mcpset.pitch) * MasterPauseFadeParameter  / 64);
			break;
		/*case 0x8600: //f12*/
		case KEY_F(12):
			if ((mcpset.pitch+=finespeed)>2048)
				mcpset.pitch=2048;
			mcpSet(-1, mcpMasterPitch, mcpset.pitch * MasterPauseFadeParameter / 64);
			if (mcpset.splock)
				mcpSet(-1, mcpMasterSpeed, (mcpset.speed=mcpset.pitch) * MasterPauseFadeParameter / 64);
			break;

		case KEY_SHIFT_F(2):
			if (mcpType & mcpNormalizeCanAmplify)
			{
				if ((mcpset.amp-=4)<4)
					mcpset.amp=4;
				mcpSet(-1, mcpMasterAmplify, 256*mcpset.amp);
			}
			break;
		case KEY_SHIFT_F(3):
			if (mcpType & mcpNormalizeCanAmplify)
			{
				if ((mcpset.amp+=4)>508)
					mcpset.amp=508;
				mcpSet(-1, mcpMasterAmplify, 256*mcpset.amp);
			}
			break;

#if 0 /* none of the software wavetables implements this - AWE32 hardware mixer in DOS probably was the main user */
		case KEY_SHIFT_F(4):
			if (mcpType & mcpNormalizeCanEcho)
			{
				mcpset.viewfx^=1;
			}
			break;
		case KEY_SHIFT_F(5):
			if (mcpType & mcpNormalizeCanEcho)
			{
				if ((mcpset.reverb-=8)<-64)
					mcpset.reverb=-64;
				mcpSet(-1, mcpMasterReverb, mcpset.reverb);
			}
			break;
		case KEY_SHIFT_F(6):
			if (mcpType & mcpNormalizeCanEcho)
			{
				if ((mcpset.reverb+=8)>64)
					mcpset.reverb=64;
				mcpSet(-1, mcpMasterReverb, mcpset.reverb);
			}
			break;
		case KEY_SHIFT_F(7):
			if (mcpType & mcpNormalizeCanEcho)
			{
				if ((mcpset.chorus-=8)<-64)
					mcpset.chorus=-64;
				mcpSet(-1, mcpMasterChorus, mcpset.chorus);
			}
			break;
		case KEY_SHIFT_F(8):
			if (mcpType & mcpNormalizeCanEcho)
			{
				if ((mcpset.chorus+=8)>64)
					mcpset.chorus=64;
				mcpSet(-1, mcpMasterChorus, mcpset.chorus);
			}
			break;
#endif

		case KEY_CTRL_F(11):
			finespeed=(finespeed==8)?1:8;
			break;

		case KEY_CTRL_F(12):
		case '\\':
			if (mcpType & mcpNormalizeCanSpeedPitchUnlock)
			{
				mcpset.splock^=1;
			}
			break;
		case KEY_BACKSPACE:
			if (mcpType & mcpNormalizeFilterAOIFOI)
			{
				mcpSet(-1, mcpMasterFilter, set.filter=(set.filter==1)?2:(set.filter==2)?0:1);
				mcpset.filter=set.filter;
			}
			break;

		case KEY_CTRL_SHIFT_F(2):
			set.pan=mcpset.pan;
			set.bal=mcpset.bal;
			set.vol=mcpset.vol;
			set.speed=mcpset.speed;
			set.pitch=mcpset.pitch;
			set.amp=mcpset.amp;
			set.reverb=mcpset.reverb;
			set.chorus=mcpset.chorus;
			set.srnd=mcpset.srnd;
			break;

		case KEY_CTRL_SHIFT_F(3):
			mcpNormalize(mcpType);
			break;

		case KEY_CTRL_SHIFT_F(4):
			mcpset.pan=64;
			mcpset.bal=0;
			mcpset.vol=64;
			mcpset.speed=256;
			mcpset.pitch=256;
			mcpset.chorus=0;
			mcpset.reverb=0;
			mcpset.amp=64;
			mcpSet(-1, mcpMasterAmplify, 256*mcpset.amp);
			mcpSet(-1, mcpMasterVolume, mcpset.vol * MasterPauseFadeParameter / 64);
			mcpSet(-1, mcpMasterBalance, mcpset.bal);
			mcpSet(-1, mcpMasterPanning, mcpset.pan);
			mcpSet(-1, mcpMasterSurround, mcpset.srnd);
			mcpSet(-1, mcpMasterPitch, mcpset.pitch);
			mcpSet(-1, mcpMasterSpeed, mcpset.speed);
			mcpSet(-1, mcpMasterReverb, mcpset.reverb);
			mcpSet(-1, mcpMasterChorus, mcpset.chorus);
			break;

		default:
			if (plrProcessKey)
			{
				int ret=plrProcessKey(key);
				if (ret==2)
					cpiResetScreen();
				if (ret)
					return 1;
			}
			if (mcpProcessKey)
			{ /* can be echo stuff, not used yet */
				int ret=mcpProcessKey(key);
				if (ret==2)
				cpiResetScreen();
				if (ret)
					return 1;
			}

			return 0;
	}
	return 1;
}

void mcpSetMasterPauseFadeParameters (int i)
{
	MasterPauseFadeParameter = i;
	mcpSet(-1, mcpMasterPitch, mcpset.pitch*i/64);
	mcpSet(-1, mcpMasterSpeed, mcpset.speed*i/64);
	mcpSet(-1, mcpMasterVolume, mcpset.vol*i/64);
}
